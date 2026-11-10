// WWE '12 PC: Discord Rich Presence, opt-in, Windows only. 2026-11-07.
//
// Our own small client of Discord's local IPC, no library, no DLL: the desktop app listens on the named pipe
// \\.\pipe\discord-ipc-N (N = 0..9). Each message is a frame [opcode u32 LE][length u32 LE][JSON]:
//   0 HANDSHAKE {"v":1,"client_id":"<application id>"} -> Discord answers a FRAME (READY), or CLOSE (bad id);
//   1 FRAME     {"cmd":"SET_ACTIVITY","args":{"pid":..,"activity":{..}},"nonce":".."};
//   2 CLOSE, 3 PING (answered by 4 PONG with the same body).
// The pipe has a thread of its own, started once the game image is loaded: nothing waits on Discord in the game.
// Discord closed or not installed = it tries again every 15 s, silently (one log line per connection).
//
// Settings: wwe12_discord (false by default, the launcher's checkbox) and wwe12_discord_app_id (the Discord
// application's id, public; empty or not a number = off). Both are read every second, so a change applies at once.
//
// What is shown (details / state, and the time since the details changed):
//   menus:    "Exhibition" / "One on One · Normal Match", "WWE Universe" / "In the menus", "Create a Superstar"...
//   a match:  "Exhibition" / "In a match · One on One · Hell in a Cell" (no superstars' names: the user's choice)
// Where it comes from (game thread, kept under a lock for the pipe's thread):
//   - The menus' decide handler (wwe12_quit.cpp calls Wwe12DiscordDecide): the chosen entry's record, +0 its id,
//     +4 its parent's id (0 = a main-menu entry), +24 its label (a text id). The main menu's label gives the mode, a
//     creation's label ("SUPERSTAR", "STORY"...) the editor; deeper labels the menu path. Any choice = no longer in
//     a match. Test of 2026-11-07: PLAY 41001 > ONE ON ONE 41009 > NORMAL MATCH 41041, CREATIONS 41005 >
//     SUPERSTAR 41024 > NEW 41109, UNIVERSE 41169.
//   - A loading screen opening (wwe12_load_turbo.cpp calls Wwe12DiscordLoadScreen): the match type, from the match
//     record (0x8317C850): +0 the type, named by text 27400 + type. (Its places at +16 are empty at that time and
//     hold something else later: not the superstars. Loading screens also come between menus.)
//   - The match's characters (sub_822D23A0 hooked below): two superstars or more = a match, with their names
//     (the save's roster, as the jukebox reads it).
//   - Universe: *0x8324E51C == 1 (set on entering the Universe menu, cleared on leaving it; test of 2026-11-07); the
//     Universe menu built again (wwe12_universe.cpp calls Wwe12DiscordUniverseMenu) = its match is over.
// The texts are the game's own (the player's language); the WWE logo character (U+E018) becomes "WWE".
// To check in game: the "[discord] ..." log lines.

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_discord, false, "WWE12",
                    "Discord Rich Presence: show in Discord what you play in WWE '12 PC (mode, kind of match).");
REXCVAR_DEFINE_STRING(wwe12_discord_app_id, "", "WWE12",
                      "Discord Rich Presence: the Discord application's id (empty = off).");

#include "svr_hook.h"

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>

#include "core/logging.h"
#include "core/memory_helpers.h"

REX_EXTERN(__imp__sub_8253A028);  // the menu's selected entry record
REX_EXTERN(__imp__sub_821A4400);  // the game's text of an id

namespace {

constexpr u32 kOpHandshake = 0, kOpFrame = 1, kOpClose = 2, kOpPing = 3, kOpPong = 4;
constexpr u32 kMaxBody = 64 * 1024;
constexpr u32 kInUniverseFlag = 0x8324E51C;
constexpr u32 kStringManager = 0x8317B86C;
constexpr u32 kMatchRecord = 0x8317C850;
constexpr u32 kMatchTypeText = 27400;
constexpr u32 kRoster = 0x8317FA08u;       // the save's superstar records (286 bytes, name at +178)
constexpr u32 kRosterIndex = 0x8307B5E0u;  // u16 [373]: number -> record index
constexpr size_t kMaxField = 120;          // Discord takes 128 bytes per field

int64_t NowS() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// ---- what the player does (written on the game thread) ---------------------------------------

struct Shared {
  std::mutex mu;
  std::string mode;              // "Exhibition", "WWE Universe", "Create a Superstar"... (empty: title screen)
  std::vector<std::string> sub;  // the menu path under the mode
  bool in_match = false;
  std::string match_type;        // "Hell in a Cell"
  std::map<u32, std::string> wrestlers;  // slot -> superstar's name, this match
};
Shared g;

bool On() { return REXCVAR_GET(wwe12_discord); }

// "ONE ON ONE" -> "One on One"; the WWE logo character -> "WWE"; other private characters dropped.
std::string Pretty(const char *s) {
  std::string raw;
  for (const char *p = s; *p; ++p) {
    const auto c = static_cast<unsigned char>(*p);
    if (c == 0xEE && p[1] && p[2]) {  // U+E000-U+EFFF: the game's button and logo characters
      if (static_cast<unsigned char>(p[1]) == 0x80 && static_cast<unsigned char>(p[2]) == 0x98)
        raw += "WWE";
      p += 2;
      continue;
    }
    raw += char(c);
  }
  std::string out, word;
  auto flush = [&] {
    if (word.empty())
      return;
    bool upper = true;
    for (char c : word)
      upper &= !std::islower(static_cast<unsigned char>(c));
    static const char *kKeep[] = {"WWE", "TLC", "HUD", "A.I.", "II", "III", "MITB"};
    bool keep = !upper;
    for (const char *k : kKeep)
      keep |= word == k;
    if (!keep) {
      static const char *kSmall[] = {"ON", "OF", "IN", "A", "AN", "THE", "TO", "VS", "AND"};
      bool small = !out.empty();
      if (small) {
        small = false;
        for (const char *k : kSmall)
          small |= word == k;
      }
      for (size_t i = 0; i < word.size(); ++i)
        word[i] = char((i == 0 && !small) ? std::toupper(static_cast<unsigned char>(word[i]))
                                          : std::tolower(static_cast<unsigned char>(word[i])));
      // "6-MAN" -> "6-Man", "4-WAY" -> "4-Way"
      for (size_t i = 1; i < word.size(); ++i)
        if (word[i - 1] == '-' || word[i - 1] == '/')
          word[i] = char(std::toupper(static_cast<unsigned char>(word[i])));
    }
    out += word;
    word.clear();
  };
  for (char c : raw) {
    if (c == ' ') {
      flush();
      if (!out.empty() && out.back() != ' ')
        out += ' ';
    } else {
      word += c;
    }
  }
  flush();
  while (!out.empty() && out.back() == ' ')
    out.pop_back();
  return out;
}

std::string Text(const PPCContext &in, u8 *base, u32 id) {
  PPCContext x = in;
  x.r3.u64 = bd::mem::load<u32>(kStringManager);
  x.r4.u64 = id;
  __imp__sub_821A4400(x, base);
  const u32 t = x.r3.u32;
  const char *s = t ? bd::mem::try_at<const char>(t) : nullptr;
  if (!s)
    return {};
  std::string copy;
  for (u32 i = 0; i < 200 && s[i]; ++i)
    copy += s[i];
  return Pretty(copy.c_str());
}

std::string SuperstarName(u32 number) {
  const u32 roster = bd::mem::try_load<u32>(kRoster);
  const u32 index = number < 373 ? bd::mem::try_load<u16>(kRosterIndex + number * 2) : 0xFFFF;
  if (!roster || index >= 373)
    return {};
  std::string name;
  for (u32 i = 0; i < 40; ++i) {
    const char c = char(bd::mem::try_load<u8>(roster + index * 286 + 178 + i));
    if (!c)
      break;
    name += c;
  }
  return name;  // stored as written ("CM Punk"), not in capitals
}

// The mode a main-menu label starts, or a creation's editor; empty = not one of them.
const char *ModeOf(u32 label) {
  switch (label) {
  case 41001: return "Exhibition";
  case 41002: return "My WWE";
  case 41003: return "Road to WrestleMania";
  case 41005: return "WWE Creations";
  case 41006:
  case 41108:
  case 41113: return "Online";
  case 41007: return "Title Management";
  case 41008: return "Options";
  case 41018: return "WWE Highlight Reel";
  case 41169: return "WWE Universe";
  default: return "";
  }
}
const char *EditorOf(u32 label) {
  switch (label) {
  case 41024: return "Create a Superstar";
  case 41025:
  case 41114: return "Create a Finisher";
  case 41026: return "Create an Entrance";
  case 41027: return "Create a Move-set";
  case 41021: return "Create a Story";
  case 41111: return "Superstar Threads";
  case 41112: return "Paint Tool";
  case 41036:
  case 41171: return "Match Creator";
  case 41166: return "Create an Entrance Movie";
  default: return "";
  }
}

// The chosen menu entries (game thread): record +0 id, +4 parent id (0 = the main menu's entries), +24 label.
// Test of 2026-11-07: PLAY id 1 parent 0, ONE ON ONE id 8 parent 1, NORMAL MATCH id 36 parent 8; +8 is not the depth.
struct Choice {
  u32 parent, label;
};
std::map<u32, Choice> g_labels;
std::vector<u32> g_path;                  // labels of the current path, main menu first
constexpr u32 kPressStart = 99999999;     // the title screen's "Press Start button" entry

}  // namespace

// The menus' decide handler (wwe12_quit.cpp), before the game acts on the choice.
void Wwe12DiscordDecide(const PPCContext &ctx, u8 *base) {
  if (!On())
    return;
  PPCContext probe = ctx;
  __imp__sub_8253A028(probe, base);
  const u32 entry = probe.r3.u32;
  if (!entry || entry == 0xFFFFFFFFu)
    return;
  const u32 id = bd::mem::load<u32>(entry), parent = bd::mem::load<u32>(entry + 4),
            label = bd::mem::load<u32>(entry + 24);
  BD_INFO("[discord] menu {:08X} choice: id {} parent {} label {}", ctx.r3.u32, id, parent, label);
  if (label == kPressStart)
    return;
  // The path: this entry, then its parents (each chosen before, so known), main menu first.
  g_labels[id] = {parent, label};
  g_path.clear();
  for (u32 at = id, steps = 0; steps < 8; ++steps) {
    const auto it = g_labels.find(at);
    if (it == g_labels.end())
      break;
    g_path.insert(g_path.begin(), it->second.label);
    if (it->second.parent == 0 || it->second.parent == 0xFFFFFFFFu)
      break;
    at = it->second.parent;
  }
  std::string mode = ModeOf(g_path[0]);
  std::vector<std::string> sub;
  for (size_t i = 1; i < g_path.size(); ++i) {
    if (const char *editor = EditorOf(g_path[i]); *editor) {
      mode = editor;
      sub.clear();
      continue;
    }
    sub.push_back(Text(ctx, base, g_path[i]));
  }
  if (mode.empty())
    mode = Text(ctx, base, g_path[0]);
  std::lock_guard lock(g.mu);
  g.mode = mode;
  g.sub = sub;
  g.in_match = false;
  g.wrestlers.clear();
}

// A loading screen opens (wwe12_load_turbo.cpp; screen = the loading screen object). Loading screens also come
// between menus (test of 2026-11-07: at start-up, after the title screen), so it is not a match by itself; it gives
// the match type while the match record is filled (+0 the type, its name text 27400 + type: "One on One").
// The game calls the opening several frames in a row: once per 3 s here.
void Wwe12DiscordLoadScreen(const PPCContext &ctx, u8 *base, u32 screen) {
  if (!On())
    return;
  static int64_t s_last = 0;
  const int64_t now = NowS();
  if (now - s_last < 3)
    return;
  s_last = now;
  const u32 type = bd::mem::load<u8>(kMatchRecord);
  const std::string type_name = Text(ctx, base, kMatchTypeText + type);
  BD_INFO("[discord] loading screen {:08X} (vtable {:08X}): match type {} '{}'", screen,
          bd::mem::try_load<u32>(screen), type, type_name);
  std::lock_guard lock(g.mu);
  if (!g.in_match) {
    g.wrestlers.clear();  // a new load: the superstars it creates are the next match's
    if (!type_name.empty())
      g.match_type = type_name;
  }
}

// The Universe menu is built again (wwe12_universe.cpp, sub_82545A70: on entering the Universe and after each of
// its matches): no longer in a match. (The Universe's own screens don't go through the decide handler.)
void Wwe12DiscordUniverseMenu() {
  std::lock_guard lock(g.mu);
  if (g.in_match)
    BD_INFO("[discord] Universe menu: match over");
  g.in_match = false;
  g.wrestlers.clear();
}

// A match character is asked for (sub_822D23A0(loader, slot, desc, ..): it queues the actor of slot "slot"; the
// desc's u32 at +0 goes into the loader's per-slot id table, +6 is the role sub_822CDF78 gives the actor). Slots 0-5
// are the superstars (the match actor list 0x8317B770: 6 superstars, the referee, 2 more). Two superstars or more =
// a match, unless the menus said an editor (Create a Superstar...) or nothing was chosen yet (the title screen's
// demo match). The superstar's number at desc+0 is a reading of the code, logged to be checked.
REX_EXTERN(__imp__sub_822D23A0);
void Wwe12BenchProbeCharacter(u32 loader, u32 slot, u32 desc, u8 *base);  // wwe12_bench_probe.cpp (dev, off)
extern "C" REX_FUNC(sub_822D23A0) {
  Wwe12BenchProbeCharacter(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, base);
  if (On()) {
    const u32 slot = ctx.r4.u32, desc = ctx.r5.u32;
    std::string bytes;
    for (u32 b = 0; b < 24 && desc; ++b)
      bytes += fmt::format("{:02X}", bd::mem::try_load<u8>(desc + b));
    // desc +0 u32 = superstar number x 100 + attire (10202 = superstar 102, attire 2), +5 the slot again, +6 the role:
    // 0 the match's superstars, 1 the referee, 3 superstars that come in during the match (a Universe run-in, 9 s
    // after the start), 4 two more (test of 2026-11-07).
    const u32 id = desc ? bd::mem::try_load<u32>(desc) : 0xFFFFFFFFu;
    const u32 role = desc ? bd::mem::try_load<u8>(desc + 6) : 0xFF;
    const std::string name = role == 0 || role == 2 ? SuperstarName(id / 100) : std::string();
    BD_INFO("[discord] character slot {} id {} role {} '{}' desc {}", slot, id, role, name, bytes);
    // (the name only tells a real superstar: Discord shows the kind of match, not who is in it)
    const std::string type_name =
        name.empty() ? std::string() : Text(ctx, base, kMatchTypeText + bd::mem::load<u8>(kMatchRecord));
    std::lock_guard lock(g.mu);
    const bool editor = [&] {
      for (u32 id : {41024u, 41025u, 41114u, 41026u, 41027u, 41021u, 41111u, 41112u, 41036u, 41171u, 41166u})
        if (g.mode == EditorOf(id))
          return true;
      return false;
    }();
    if (slot < 6 && !name.empty() && !g.mode.empty() && !editor) {
      g.wrestlers[slot] = name;
      if (!g.in_match && g.wrestlers.size() >= 2) {
        g.in_match = true;
        if (!type_name.empty())
          g.match_type = type_name;
        BD_INFO("[discord] match: {} superstars", g.wrestlers.size());
      }
    }
  }
  __imp__sub_822D23A0(ctx, base);
}

namespace {

// ---- the pipe (its own thread) ----------------------------------------------------------------

// A Discord application id is a number (a "snowflake", 17-20 digits). Anything else = off.
bool ValidAppId(const std::string &id) {
  if (id.size() < 5 || id.size() > 24)
    return false;
  for (char c : id)
    if (c < '0' || c > '9')
      return false;
  return true;
}

// A JSON string's content, cut to Discord's limit without splitting a character.
std::string Json(std::string s) {
  if (s.size() > kMaxField) {
    size_t cut = kMaxField;
    while (cut && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80)
      --cut;
    s.resize(cut);
  }
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      out += ' ';
    } else {
      out += c;
    }
  }
  return out;
}

bool WriteAll(HANDLE h, const void *data, DWORD size) {
  const auto *p = static_cast<const char *>(data);
  while (size) {
    DWORD done = 0;
    if (!WriteFile(h, p, size, &done, nullptr) || done == 0)
      return false;
    p += done;
    size -= done;
  }
  return true;
}

bool ReadAll(HANDLE h, void *data, DWORD size) {
  auto *p = static_cast<char *>(data);
  while (size) {
    DWORD done = 0;
    if (!ReadFile(h, p, size, &done, nullptr) || done == 0)
      return false;
    p += done;
    size -= done;
  }
  return true;
}

bool WriteFrame(HANDLE h, u32 op, const std::string &json) {
  std::vector<char> buf(8 + json.size());
  const u32 len = u32(json.size());
  std::memcpy(buf.data(), &op, 4);  // little-endian on every Windows PC
  std::memcpy(buf.data() + 4, &len, 4);
  std::memcpy(buf.data() + 8, json.data(), json.size());
  return WriteAll(h, buf.data(), DWORD(buf.size()));
}

// Reads what Discord sent, without ever waiting: answers PING, false on CLOSE or a broken pipe.
// *got_frame is set when a FRAME came (the handshake's READY).
bool Drain(HANDLE h, bool *got_frame = nullptr) {
  for (;;) {
    DWORD avail = 0;
    if (!PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr))
      return false;
    if (avail < 8)
      return true;
    u32 head[2];
    if (!ReadAll(h, head, 8) || head[1] > kMaxBody)
      return false;
    std::string body(head[1], '\0');
    if (head[1] && !ReadAll(h, body.data(), head[1]))
      return false;
    switch (head[0]) {
    case kOpPing:
      if (!WriteFrame(h, kOpPong, body))
        return false;
      break;
    case kOpClose:
      BD_INFO("[discord] Discord closed the connection: {}", body.substr(0, 200));
      return false;
    case kOpFrame:
      if (got_frame)
        *got_frame = true;
      break;
    default:
      break;
    }
  }
}

// The first pipe that answers the handshake, or INVALID_HANDLE_VALUE.
HANDLE Connect(const std::string &app_id) {
  for (int i = 0; i < 10; ++i) {
    const std::wstring name = L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring(i);
    HANDLE h = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
      continue;
    bool ready = false;
    if (WriteFrame(h, kOpHandshake, fmt::format(R"({{"v":1,"client_id":"{}"}})", app_id))) {
      for (int wait = 0; wait < 60 && !ready; ++wait) {  // up to 3 s
        if (!Drain(h, &ready))
          break;
        if (!ready)
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }
    if (ready) {
      BD_INFO("[discord] connected (discord-ipc-{})", i);
      return h;
    }
    CloseHandle(h);
  }
  return INVALID_HANDLE_VALUE;
}

struct Presence {
  std::string details, state;
  bool operator==(const Presence &o) const { return details == o.details && state == o.state; }
};

Presence Read() {
  Presence p;
  std::lock_guard lock(g.mu);
  std::string mode = g.mode;
  const bool universe = bd::mem::try_load<u32>(kInUniverseFlag) == 1;
  if (universe)
    mode = "WWE Universe";
  if (g.in_match) {
    // The kind of match, no superstars' names (the user's choice, 2026-11-07): the menu path when the match was
    // chosen in the menus ("One on One · Normal Match"), else the match type's name ("One on One").
    p.details = mode.empty() ? "In a match" : mode;
    std::string kind;
    if (!universe)
      for (const std::string &s : g.sub)
        kind += (kind.empty() ? "" : " \xC2\xB7 ") + s;
    if (kind.empty())
      kind = g.match_type;
    p.state = kind.empty() ? "In a match" : "In a match \xC2\xB7 " + kind;
    return p;
  }
  if (mode.empty()) {
    p.details = "In the menus";
    return p;
  }
  p.details = mode;
  if (universe || g.sub.empty()) {
    p.state = "In the menus";
  } else {
    for (const std::string &s : g.sub)
      p.state += (p.state.empty() ? "" : " \xC2\xB7 ") + s;  // " · "
  }
  return p;
}

std::string ActivityJson(const Presence &p, int64_t since, u32 nonce) {
  const std::string state = p.state.empty() ? "" : fmt::format(R"(,"state":"{}")", Json(p.state));
  return fmt::format(R"({{"cmd":"SET_ACTIVITY","args":{{"pid":{},"activity":{{"details":"{}"{},)"
                     R"("timestamps":{{"start":{}}}}}}},"nonce":"{}"}})",
                     GetCurrentProcessId(), Json(p.details), state, since, nonce);
}

void Run() {
  using namespace std::chrono_literals;
  HANDLE pipe = INVALID_HANDLE_VALUE;
  std::string pipe_app_id, sent;
  int64_t next_try = 0, last_send = 0;
  Presence shown, seen;
  int64_t since = NowS(), seen_since = NowS();
  u32 nonce = 0;
  bool logged_waiting = false;

  auto close = [&] {
    if (pipe != INVALID_HANDLE_VALUE)
      CloseHandle(pipe);
    pipe = INVALID_HANDLE_VALUE;
    sent.clear();
  };

  for (;;) {
    std::this_thread::sleep_for(1s);
    const int64_t now = NowS();
    const std::string app_id = REXCVAR_GET(wwe12_discord_app_id);
    if (!On() || !ValidAppId(app_id)) {
      close();
      continue;
    }

    // What to show, once it has lasted 2 s (no flicker); the clock restarts when the details change.
    const Presence p = Read();
    if (!(p == seen)) {
      seen = p;
      seen_since = now;
    }
    if (!(seen == shown) && now - seen_since >= 2) {
      if (seen.details != shown.details)
        since = now;
      shown = seen;
      BD_INFO("[discord] now \"{}\" / \"{}\"", shown.details, shown.state);
    }

    if (pipe != INVALID_HANDLE_VALUE && app_id != pipe_app_id)
      close();  // the id changed: hand-shake again
    if (pipe == INVALID_HANDLE_VALUE) {
      if (now < next_try)
        continue;
      pipe = Connect(app_id);
      if (pipe == INVALID_HANDLE_VALUE) {
        if (!logged_waiting)
          BD_INFO("[discord] Discord not found, trying again every 15 s");
        logged_waiting = true;
        next_try = now + 15;
        continue;
      }
      logged_waiting = false;
      pipe_app_id = app_id;
    }
    if (!Drain(pipe)) {
      close();
      next_try = now + 5;
      continue;
    }
    // Discord takes about 5 updates per 20 s: one every 4 s at most, only when something changed.
    const std::string json = ActivityJson(shown, since, 0);
    if (shown.details.empty() || json == sent || now - last_send < 4)
      continue;
    if (!WriteFrame(pipe, kOpFrame, ActivityJson(shown, since, ++nonce))) {
      close();
      next_try = now + 5;
      continue;
    }
    sent = json;
    last_send = now;
  }
}

}  // namespace

// wwe12_app.h, once the game image is loaded: the thread waits until the setting is on.
void Wwe12DiscordStart() {
  static std::atomic<bool> s_started{false};
  if (!s_started.exchange(true))
    std::thread(Run).detach();
}

#else  // Android and the rest: nothing.

void Wwe12DiscordStart() {}
void Wwe12DiscordDecide(const PPCContext &, u8 *) {}
void Wwe12DiscordLoadScreen(const PPCContext &, u8 *, u32) {}
void Wwe12DiscordUniverseMenu() {}

#endif
