// WWE '12 PC: Discord Rich Presence ("Playing WWE '12 PC - In a match"), opt-in, Windows only. 2026-11-07.
//
// Our own small client of Discord's local IPC, no library, no DLL: the desktop app listens on the named pipe
// \\.\pipe\discord-ipc-N (N = 0..9). Each message is a frame [opcode u32 LE][length u32 LE][JSON]:
//   0 HANDSHAKE {"v":1,"client_id":"<application id>"} -> Discord answers a FRAME (READY), or CLOSE (bad id);
//   1 FRAME     {"cmd":"SET_ACTIVITY","args":{"pid":..,"activity":{..}},"nonce":".."};
//   2 CLOSE, 3 PING (answered by 4 PONG with the same body).
// Everything runs on a thread of its own, started once the game image is loaded: nothing waits on Discord in the
// game. Discord closed or not installed = it tries again every 15 s, silently (one log line per connection).
//
// Settings: wwe12_discord (false by default, the launcher's checkbox) and wwe12_discord_app_id (the Discord
// application's id, public; empty or not a number = off). Both are read every second, so a change applies at once.
//
// What is shown (details, state, time in that state):
//   "In the menus" / "In a match", with "Universe mode" under it in Universe.
//   - In a match: a loading screen is up (Wwe12LoadingScreenUp) or the first match actor (0x8317B770, placed at
//     match start by sub_82386168) exists with a role (+3428) other than 10 (all actors go to 10 at the match end;
//     wwe12_kind_probe.cpp, 2026-10-22). A state must last 2 s before it is shown (no flicker).
//   - Universe: *0x8324E51C == 1 (the Universe flag wwe12_universe.cpp uses).
//   To check in game: the "[discord] now ..." log lines (one per change).

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_discord, false, "WWE12",
                    "Discord Rich Presence: show in Discord that you play WWE '12 PC (menus / match / Universe).");
REXCVAR_DEFINE_STRING(wwe12_discord_app_id, "", "WWE12",
                      "Discord Rich Presence: the Discord application's id (empty = off).");

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>

#include "core/logging.h"
#include "core/memory_helpers.h"

bool Wwe12LoadingScreenUp();  // wwe12_load_turbo.cpp

namespace {

constexpr u32 kOpHandshake = 0, kOpFrame = 1, kOpClose = 2, kOpPing = 3, kOpPong = 4;
constexpr u32 kMatchActors = 0x8317B770;   // 9 actor pointers (6 wrestlers, referee, 2 more)
constexpr u32 kActorRole = 3428;           // 10 once the match is over
constexpr u32 kInUniverseFlag = 0x8324E51C;
constexpr u32 kMaxBody = 64 * 1024;

int64_t NowS() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// A Discord application id is a number (a "snowflake", 17-20 digits). Anything else = off; it also keeps the JSON
// below free of quotes.
bool ValidAppId(const std::string &id) {
  if (id.size() < 5 || id.size() > 24)
    return false;
  for (char c : id)
    if (c < '0' || c > '9')
      return false;
  return true;
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
  const char *details = "";
  bool universe = false;
  bool operator==(const Presence &o) const {
    return std::strcmp(details, o.details) == 0 && universe == o.universe;
  }
};

Presence Read() {
  Presence p;
  bool match = Wwe12LoadingScreenUp();
  if (!match) {
    const u32 actor = bd::mem::try_load<u32>(kMatchActors);
    match = actor && bd::mem::try_field<u32>(actor, kActorRole, 10) != 10;
  }
  p.details = match ? "In a match" : "In the menus";
  p.universe = bd::mem::try_load<u32>(kInUniverseFlag) == 1;
  return p;
}

std::string ActivityJson(const Presence &p, int64_t since, u32 nonce) {
  const std::string state = p.universe ? R"(,"state":"Universe mode")" : "";
  return fmt::format(R"({{"cmd":"SET_ACTIVITY","args":{{"pid":{},"activity":{{"details":"{}"{},)"
                     R"("timestamps":{{"start":{}}}}}}},"nonce":"{}"}})",
                     GetCurrentProcessId(), p.details, state, since, nonce);
}

void Run() {
  using namespace std::chrono_literals;
  HANDLE pipe = INVALID_HANDLE_VALUE;
  std::string pipe_app_id, sent;
  int64_t next_try = 0, last_send = 0;
  Presence shown, seen;
  int64_t shown_since = NowS(), seen_since = NowS();
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
    if (!REXCVAR_GET(wwe12_discord) || !ValidAppId(app_id)) {
      close();
      continue;
    }

    // The state, shown once it has lasted 2 s.
    const Presence p = Read();
    if (!(p == seen)) {
      seen = p;
      seen_since = now;
    }
    if (!(seen == shown) && now - seen_since >= 2) {
      shown = seen;
      shown_since = now;
      BD_INFO("[discord] now {}{}", shown.details, shown.universe ? " (Universe mode)" : "");
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
    const std::string json = ActivityJson(shown, shown_since, 0);
    if (!*shown.details || json == sent || now - last_send < 4)  // (nothing known yet in the first 2 s)
      continue;
    if (!WriteFrame(pipe, kOpFrame, ActivityJson(shown, shown_since, ++nonce))) {
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

#endif
