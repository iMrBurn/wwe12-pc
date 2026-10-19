// Dev-only: compiled only with the WWE12_DEV_PROBES environment variable at configure time.
// F6 in game scans guest memory for the text typed by the user on the game's own on-screen
// keyboard (Create a Story text, CAW names...) to find the name buffer, and for the character
// rows of that keyboard (as UTF-16LE: "1234567890-="...) to locate the screen state. The
// first line of Documents\wwe12\kbd_marker.txt is the text to find (typed with the pad grid
// just before pressing F6). Logs every hit with a 64-byte hexdump, and dumps 64 KB around the
// non-ASCII hits to Documents\wwe12\logs\kbdscan_<run>_<k>_<va>.bin (first 4 bytes: guest
// address). Everything is also appended to Documents\wwe12\logs\kbdscan_summary.txt.
// F5 = radar PAGE_GUARD sur les pages des buffers : note le RIP de chaque ecriture
// made sur ces pages (le handler desarme puis relache), pour identifier la fonction
// d'insertion de lettre du jeu. Journal : logs\kbdtrap_log.txt.
#if defined(WWE12_DEV_PROBES)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "core/logging.h"
#include "core/memory_helpers.h"

namespace {

namespace fs = std::filesystem;

// Les VAs (adresses invite) des hits ASCII du texte tape, remplis par F6, pour
// servir le radar F5 (piege les pages de ces buffers).
std::vector<u32> g_marker_hits;
u8 *g_base = nullptr;

fs::path LogsDir() {
  PWSTR docs = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs))) {
    return fs::path(L".");
  }
  const auto path = fs::path(docs) / L"wwe12" / L"logs";
  CoTaskMemFree(docs);
  fs::create_directories(path);
  return path;
}

// The text to search for: first non-empty line of Documents\wwe12\kbd_marker.txt (utf-8).
std::u16string ReadMarker() {
  PWSTR docs = nullptr;
  std::u16string marker;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs))) {
    return marker;
  }
  const auto path = (fs::path(docs) / L"wwe12" / L"kbd_marker.txt").wstring();
  CoTaskMemFree(docs);
  if (FILE *f = _wfopen(path.c_str(), L"rb")) {
    std::string line;
    int c;
    while ((c = std::fgetc(f)) != EOF && c != '\r' && c != '\n') line.push_back(char(c));
    std::fclose(f);
    if (line.size() >= 2 && u8(line[0]) == 0xEF && u8(line[1]) == 0xBB) line.erase(0, 2);  // BOM
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
    if (!line.empty()) marker = rex::string::to_utf16(line);
    if (marker.empty())
      BD_INFO("[kbdscan] marker file lu mais vide");
  } else {
    BD_INFO("[kbdscan] marker file introuvable (Documents\\wwe12\\kbd_marker.txt)");
  }
  return marker;
}

std::vector<u8> Utf16Le(const std::u16string &text) {
  std::vector<u8> b;
  b.reserve(text.size() * 2);
  for (const char16_t c : text) {
    b.push_back(u8(c & 0xFF));
    b.push_back(u8(c >> 8));
  }
  return b;
}

// Rangees du clavier du jeu (captures d'ecran Create a Story), en UTF-16LE.
std::vector<std::vector<u8>> KeyboardRows() {
  return {Utf16Le(u"1234567890-="), Utf16Le(u"qwertyuiop[]"), Utf16Le(u"asdfghjkl;'"),
          Utf16Le(u"zxcvbnm,./")};
}

// Variantes d'encodings possibles d'un meme texte:
//  - ASCII (un octet par caractere)
//  - UTF-16LE / UTF-16BE (deux octets)
//  - ASCII par u32 little-endian ('x', 0, 0, 0) : beaucoup de moteurs stockent
//    les caracteres d'un buffer texte ainsi.
std::vector<std::pair<std::vector<u8>, const char *>> MarkerForms(const std::u16string &marker) {
  std::vector<std::pair<std::vector<u8>, const char *>> forms;
  std::vector<u8> ascii;
  ascii.reserve(marker.size());
  for (const char16_t c : marker) {
    ascii.push_back(u8(c & 0xFF));
  }
  if (!ascii.empty() &&
      std::all_of(ascii.begin(), ascii.end(), [](u8 c) { return c >= 0x20 && c < 0x7F; })) {
    forms.push_back({ascii, "ascii"});
    if (marker.size() >= 3) {  // "xxxx" avec un caractere sur 4 : trop bruite en deca.
      std::vector<u8> stride4;
      stride4.reserve(marker.size() * 4);
      for (const u8 c : ascii) {
        stride4.push_back(c);
        stride4.push_back(0);
        stride4.push_back(0);
        stride4.push_back(0);
      }
      forms.push_back({stride4, "ascii-u32le"});
    }
  }
  forms.push_back({Utf16Le(marker), "utf16le"});
  std::vector<u8> be;
  be.reserve(marker.size() * 2);
  for (const char16_t c : marker) {
    be.push_back(u8(c >> 8));
    be.push_back(u8(c & 0xFF));
  }
  forms.push_back({be, "utf16be"});
  return forms;
}

// Lecture avec garde : evite de toucher une page non lisible (les hits peuvent etre proches
// d'une fin de region allouee).
class GuardedReader {
 public:
  explicit GuardedReader(u8 *base) : base_(base) {}

  u8 Peek(u32 va) {
    const u8 *page = reinterpret_cast<const u8 *>(va & ~0xFFFu);
    if (page != cached_page_) {
      MEMORY_BASIC_INFORMATION mbi;
      cached_ok_ = VirtualQuery(base_ + (va & ~0xFFFu), &mbi, sizeof(mbi)) &&
                   mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
      cached_page_ = page;
    }
    return cached_ok_ ? base_[va] : u8(0);
  }

  bool readable(u32 va) {
    Peek(va);
    return cached_ok_;
  }

 private:
  u8 *base_;
  const u8 *cached_page_ = nullptr;
  bool cached_ok_ = false;
};

void Hexdump(std::string &out, GuardedReader &mem, u32 va) {
  out.reserve(out.size() + 64 * 3);
  for (int i = 0; i < 64; ++i) {
    const u32 a = va + u32(i);
    out += mem.readable(a) ? fmt::format(" {:02X}", mem.Peek(a)) : " --";
  }
}

void DumpAround(u8 *base, u32 va, const fs::path &logs, int run, int slot) {
  const u32 start = (va & ~0xFFFu) - 0x2000u;
  const auto path = logs / fmt::format("kbdscan_{}_{}_{}.bin", run, slot, start);
  if (FILE *f = _wfopen(path.c_str(), L"wb")) {
    std::fwrite(&start, 4, 1, f);
    GuardedReader mem(base);
    for (u32 off = 0; off < 0x10000; ++off) {
      const u32 a = start + off;
      const u8 v = mem.readable(a) ? mem.Peek(a) : u8(0);
      std::fwrite(&v, 1, 1, f);
    }
    std::fclose(f);
  }
}

void Scan(int run) {
  const fs::path logs = LogsDir();
  u8 *const base = bd::mem::at<u8>(0x40000000) - 0x40000000;
  g_base = base;
  const std::u16string marker = ReadMarker();

  // Toutes les formes de recherche : le texte tape (marker) sous tous ses
  // encodings possibles + les rangees du clavier du jeu (UTF-16LE, ASCII,
  // ASCII-u32) pour attraper la table de lettres quelle que soit sa forme.
  struct Pattern {
    std::vector<u8> bytes;
    std::string kind;
  };
  std::vector<Pattern> pats;
  for (const auto &[bytes, kind] : MarkerForms(marker)) {
    pats.push_back({bytes, kind});
  }
  const std::vector<std::vector<u8>> row_utf16 = KeyboardRows();
  for (size_t r = 0; r < row_utf16.size(); ++r) {
    pats.push_back({row_utf16[r], fmt::format("row{}-utf16le", r)});
  }
  // Formes ASCII des rangees (la table peut etre stockee sur un octet).
  for (const auto *row : {u"1234567890-=", u"qwertyuiop[]", u"asdfghjkl;'", u"zxcvbnm,./"}) {
    std::vector<u8> a;
    for (const char16_t *c = row; *c; ++c) a.push_back(u8(*c & 0xFF));
    pats.push_back({a, fmt::format("row-ascii{}", a[0] & 0xF)});
  }

  struct Hit {
    u32 va;
    std::string kind;
  };
  std::vector<Hit> hits;
  std::vector<u32> x_positions;
  u64 scanned = 0;

  for (const auto [lo, hi] : {std::pair<u32, u32>{0x00010000u, 0x9FFF0000u},
                              std::pair<u32, u32>{0xA0000000u, 0xC0000000u}}) {
    u8 *p = base + lo;
    u8 *const end = base + hi;
    while (p < end) {
      MEMORY_BASIC_INFORMATION mbi;
      if (!VirtualQuery(p, &mbi, sizeof(mbi))) break;
      u8 *rs = static_cast<u8 *>(mbi.BaseAddress);
      u8 *re = std::min(rs + mbi.RegionSize, end);
      const bool readable =
          mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
      if (readable) {
        scanned += u64(re - std::max(p, rs));
        for (u8 *q = std::max(p, rs); q < re; ++q) {
          bool found = false;
          for (const auto &pat : pats) {
            if (pat.bytes.empty() || pat.bytes.size() < 2) continue;
            if (q + pat.bytes.size() <= re &&
                std::memcmp(q, pat.bytes.data(), pat.bytes.size()) == 0) {
              hits.push_back({u32(q - base), pat.kind});
              found = true;
              break;
            }
          }
          if (!found && *q == 'x' && q + 32 <= re) {
            // Position d'un 'x' : on note pour le balayage espace ensuite.
            x_positions.push_back(u32(q - base));
          }
        }
      }
      p = re;
    }
  }

  GuardedReader mem(base);

  // Scan "epingles" : chaque 'x' vu en memoire, teste si x-x-q-z existe a
  // ecartement regulier (1, 2, 3, 4 ou 8 octets) - attrape les buffers qui
  // stockent les caracteres avec des donnees entre eux.
  for (const u32 xa : x_positions) {
    for (const u32 stride : {1u, 2u, 3u, 4u, 8u}) {
      const u32 need = xa + stride * 3 + 1;
      if (need < xa || !mem.readable(need - 1)) continue;
      if (mem.Peek(xa) == u8('x') && mem.Peek(xa + stride) == u8('x') &&
          mem.Peek(xa + stride * 2) == u8('q') && mem.Peek(xa + stride * 3) == u8('z')) {
        hits.push_back({xa, fmt::format("sparse-stride{}", stride)});
        break;
      }
    }
  }

  int dumped = 0;
  g_marker_hits.clear();
  for (size_t i = 0; i < hits.size() && i < 200; ++i) {
    // Pour les hits du texte tape : montre aussi les 0x40 octets AVANT (le
    // compteur/curseur du buffer vivent la, juste avant le texte).
    std::string pre;
    Hexdump(pre, mem, hits[i].va >= 0x40 ? hits[i].va - 0x40 : 0);
    BD_INFO("[kbdscan] run {}: {:08X} {} AVANT {}", run, hits[i].va, hits[i].kind, pre);
    BD_INFO("[kbdscan] run {}: {:08X} {}", run, hits[i].va, hits[i].kind);
    if (hits[i].kind == "ascii" && g_marker_hits.size() < 6) {
      g_marker_hits.push_back(hits[i].va);
    }
    if (dumped < 10) {
      DumpAround(base, hits[i].va, logs, run, dumped++);
    }
  }
  BD_INFO("[kbdscan] run {}: {} hits, {} dumps, {} MB scannes, {} 'x'", run, hits.size(), dumped,
          scanned >> 20, x_positions.size());

  const auto path = logs / L"kbdscan_summary.txt";
  if (FILE *f = _wfopen(path.c_str(), L"ab")) {
    std::string s = fmt::format(
        "run {}: {} hits, {} dumps, {} MB scannes; marker \"{}\" ({} chars), {} 'x'\r\n", run,
        hits.size(), dumped, scanned >> 20, rex::string::to_utf8(marker), marker.size(),
        x_positions.size());
    for (size_t i = 0; i < hits.size() && i < 200; ++i) {
      std::string line;
      Hexdump(line, mem, hits[i].va);
      s += fmt::format("  {:08X} {} {}\r\n", hits[i].va, hits[i].kind, line);
      if (hits[i].va >= 0x40) {
        std::string pre;
        Hexdump(pre, mem, hits[i].va - 0x40);
        s += fmt::format("        PRE {}\r\n", pre);
      }
    }
    std::fwrite(s.data(), 1, s.size(), f);
    std::fclose(f);
  }
}

// Test manuel d'ecriture : ecrit au debut de chaque buffer de texte trouve par
// F6 une lettre distincte repetee ('Z', 'Y', 'X'...) + un zero final. La barre
// de texte du jeu doit alors montrer cette lettre : preuve que le buffer
// alimente l'ecran. (Servi a valider le buffer ; F5 est aujourd'hui le radar.)
[[maybe_unused]] void OverwriteMarker() {
  if (g_marker_hits.empty()) {
    BD_INFO("[kbdscan] F5: aucun buffer connu (faire F6 d'abord)");
    return;
  }
  u8 *const base = g_base;
  int k = 0;
  for (const u32 va : g_marker_hits) {
    const u8 c = u8('Z' - (u8)k);
    for (int i = 0; i < 6; ++i) {
      base[va + i] = c;
    }
    base[va + 6] = 0;
    BD_INFO("[kbdscan] F5: {:08X} ecris avec '{}'...", va, char(c));
    ++k;
  }
}

// Le radar F5 : met un PAGE_GUARD sur les pages des buffers trouves par F6.
// Tout acces sur ces pages leve une exception "page de garde" : le handler
// note l'adresse de l'instruction fautive (RIP) puis desarme la page, et le
// fil radar la re-piege toutes les ~40 ms tant que le radar tourne. Le jeu
// continue normalement; on arrete a 400 ecritures, apres 90 s, ou sur F5.
// Les LECTURES ne comptent pas dans le budget (le jeu relit le texte chaque
// frame, ca noierait les ecritures) : elles sont juste journalisees.
// Nombre max d'ecritures journalisees avant arret automatique.
static constexpr int kMaxTrapEvents = 400;

struct Trap {
  struct Page {
    void *base;
    DWORD old_protect;
    bool guarded = false;
  };

  std::atomic<int> faults{0};
  std::atomic<int> writes{0};
  std::atomic<bool> armed{false};
  std::vector<Page> pages;
  std::thread worker;
  FILE *log = nullptr;
  LARGE_INTEGER epoch{};
  u8 *base = nullptr;

  static Trap &Self() {
    static Trap self;
    return self;
  }

  void Toggle() {
    if (armed.load()) {
      Disarm();
      return;
    }
    Arm();
  }

  void Arm() {
    // Termine proprement un cycle precedent eventuel.
    armed = false;
    if (worker.joinable()) worker.join();
    DisarmPages();

    base = g_base;
    if (!base || g_marker_hits.empty()) {
      BD_INFO("[kbdscan] radar: rien a pieger (lance F6 sur l'ecran de texte d'abord)");
      return;
    }
    if (log) {
      std::fclose(log);
      log = nullptr;
    }
    const auto path = LogsDir() / L"kbdtrap_log.txt";
    log = _wfopen(path.c_str(), L"wb");
    if (!log) {
      BD_INFO("[kbdscan] radar: log inaccessible");
      return;
    }
    faults = 0;
    writes = 0;
    QueryPerformanceCounter(&epoch);
    std::vector<void *> seen;
    pages.clear();
    pages.reserve(8);
    for (const u32 va : g_marker_hits) {
      void *page = base + (va & ~0xFFFu);
      if (std::find(seen.begin(), seen.end(), page) != seen.end()) continue;
      seen.push_back(page);
      DWORD old = 0;
      if (!VirtualProtect(page, 0x1000, PAGE_READWRITE | PAGE_GUARD, &old)) {
        BD_INFO("[kbdscan] radar: VirtualProtect {:08X} refuse", u32(va & ~0xFFFu));
        continue;
      }
      Page &p = pages.emplace_back();
      p.base = page;
      p.old_protect = old;
      p.guarded = true;
      BD_INFO("[kbdscan] radar: page {:08X} piegee", u32(va & ~0xFFFu));
    }
    if (pages.empty()) {
      BD_INFO("[kbdscan] radar: impossible de pieger les pages");
      std::fclose(log);
      log = nullptr;
      return;
    }
    armed = true;
    worker = std::thread([this] {
      // Re-piege periodiquement les pages desarmees par le handler, pour
      // attraper plusieurs ecritures successives. S'arrete seul apres 90 s
      // (avec nettoyage, pour ne jamais laisser une page piegee derriere soi).
      const auto t0 = std::chrono::steady_clock::now();
      bool self_stopped = false;
      while (armed.load()) {
        // Re-piège vite après chaque capture : une lettre peut arriver dès
        // la milliseconde suivante.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(90)) {
          self_stopped = true;
          break;
        }
        for (auto &p : pages) {
          // Vérifie l'état réel de la page : le handler désarme à la volée
          // sans nous prévenir, et VirtualProtect à deux fils qui se
          // chevauchent reste bénin (au pire un événement en double).
          MEMORY_BASIC_INFORMATION mbi;
          if (VirtualQuery(p.base, &mbi, sizeof(mbi)) && !(mbi.Protect & PAGE_GUARD)) {
            DWORD old = 0;
            VirtualProtect(p.base, 0x1000, PAGE_READWRITE | PAGE_GUARD, &old);
          }
        }
      }
      if (self_stopped) {
        // Le handler ne doit jamais voir armed=false avec une page encore
        // piegee (sinon l'exception passe les autres handlers = crash).
        DisarmPages();
        if (log) {
          std::fclose(log);
          log = nullptr;
        }
        armed = false;
        BD_INFO("[kbdscan] radar: auto-arret apres 90 s ({} evenements, {} ecritures)",
                faults.load(), writes.load());
      }
    });
    BD_INFO("[kbdscan] radar: arme ({} pages) - tape une lettre avec la grille", pages.size());
  }

  void Disarm() {
    armed = false;
    if (worker.joinable()) worker.join();
    DisarmPages();
    if (log) {
      std::fclose(log);
      log = nullptr;
    }
    BD_INFO("[kbdscan] radar: coupe ({} evenements, {} ecritures)", faults.load(),
            writes.load());
  }

  void DisarmPages() {
    for (auto &p : pages) {
      if (p.guarded) {
        DWORD ignore = 0;
        VirtualProtect(p.base, 0x1000, p.old_protect, &ignore);
        p.guarded = false;
      }
    }
    pages.clear();
  }

  // Handler d'exception : premiere chance de voir chaque acces a une page
  // piegee. Il note RIP + adresse puis libere la page pour que l'acces se
  // rejoue normalement. Ne doit jamais allouer ni se bloquer lui-meme.
  static LONG CALLBACK Handler(EXCEPTION_POINTERS *ep) {
    if (!ep || ep->ExceptionRecord->ExceptionCode != 0x80000001u) {
      return EXCEPTION_CONTINUE_SEARCH;
    }
    auto &self = Self();
    if (!self.armed.load()) return EXCEPTION_CONTINUE_SEARCH;
    if (ep->ExceptionRecord->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
    const unsigned long long rip = ep->ContextRecord->Rip;
    const unsigned long long addr =
        (unsigned long long)ep->ExceptionRecord->ExceptionInformation[1];
    const unsigned long long type =
        (unsigned long long)ep->ExceptionRecord->ExceptionInformation[0];
    unsigned long long guest = addr;
    if (self.base && addr >= (unsigned long long)self.base &&
        addr < (unsigned long long)self.base + 0x100000000ull) {
      guest = addr - (unsigned long long)self.base;
    }
    const int n = self.faults.fetch_add(1) + 1;
    // type: 0 = lecture, 1 = ecriture, 8 = execution (comme une access violation).
    const bool is_write = type == 1;
    int w = 0;
    if (is_write) w = self.writes.fetch_add(1) + 1;
    if (self.log) {
      LARGE_INTEGER now{}, freq{};
      QueryPerformanceCounter(&now);
      QueryPerformanceFrequency(&freq);
      const double ms =
          double(now.QuadPart - self.epoch.QuadPart) * 1000.0 / double(freq.QuadPart);
      std::fprintf(self.log, "%10.1fms #%-5d w=%-4d rip=%016llx type=%llu guest=%08llx\n", ms, n,
                   w, rip, type, guest);
      std::fflush(self.log);
    }
    // Libere la page fautive ; le fil radar la re-piege ~40 ms plus tard.
    DWORD old = 0;
    VirtualProtect((void *)(addr & ~0xFFFull), 0x1000, PAGE_READWRITE, &old);
    if (is_write && w >= kMaxTrapEvents) self.armed = false;
    return EXCEPTION_CONTINUE_EXECUTION;
  }
};

struct Starter {
  Starter() {
    // Le handler du radar doit etre en premiere position : il attrape les
    // exceptions de page de garde avant tout autre handler, puis laisse le
    // jeu continuer.
    AddVectoredExceptionHandler(1, &Trap::Handler);
    std::thread([] {
      int run = 0;
      bool f6_was = false, f5_was = false;
      for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        const bool f6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
        const bool f5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        if (f6 && !f6_was) {
          if (Trap::Self().armed.load()) Trap::Self().Disarm();
          BD_INFO("[kbdscan] F6: scanning guest memory");
          Scan(++run);
          // Arme le radar automatiquement apres le scan : les buffers viennent
          // d'etre localises, l'utilisateur a juste a taper des lettres.
          Trap::Self().Arm();
        }
        if (f5 && !f5_was) {
          Trap::Self().Toggle();
        }
        f6_was = f6;
        f5_was = f5;
      }
    }).detach();
  }
} g_starter;

}  // namespace

#endif  // WWE12_DEV_PROBES
