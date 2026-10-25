// MODS page: install, switch on/off and remove .svrmod mods (docs/mod_maker.md).
//
// A .svrmod is a zip of one mod folder with manifest.txt at its root (key=value lines), the format of
// Kaiko's SvR 2011 port, credited in docs/mod_maker.md. WWE '12 mods carry game=wwe12; a mod without it
// (an SvR 2011 one) is refused. The files a mod gives the game sit under files/ in the zip, at the
// disc's paths (files/movies/titantron/100.bik).
//
// The game reads the mods folder beside the exe as an overlay of the disc (whole files only). So:
//   mods\.svrmods\<Kind>\<id>\          an installed mod, unpacked (manifest.txt, files\...);
//                                       a "disabled" file in it switches it off
//   mods\<disc path>                    the files of the mods that are on, hard links to the copies
//                                       above (a copy when a link can't be made)
//   mods\.svrmods\deployed.txt          which of those files the launcher put there (path, mod, size)
// Files the player put in mods by hand are never touched: a mod file that would replace one is
// left out and reported. Of two mods giving the same file, the first one (by folder name) wins.
// Texture packs (type=textures, ours: Kaiko keeps them out of .svrmod) give their pictures under
// textures/ in the zip (<anything>_<hash16>.png or .dds, see port/src/native/wwe12_texture_packs.h);
// they go to textures\<id>\ beside the exe, where the game looks for them (deployed.txt: "textures:").
// Added arenas (type=arena with slot=new, base=NN) give arena/arena.pac and arena/banner.dds: they stay in their
// folder, where the game reads them at startup (src/native/wwe12_arena_pages.cpp); nothing is put in mods\.
// Added superstars (type=superstar, base=NNN) give superstar/ch.pac (and later its pictures): they stay in their folder
// too; the game makes its own DLC pack of them at startup (src/native/wwe12_superstar_mods.cpp).
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "game_process.h"
#include "imgui.h"
#include "inflate.h"

namespace mods {

namespace fs = std::filesystem;

struct Texts {
  const char *installed, *install, *none, *col_name, *col_version, *col_author, *col_type, *col_on, *remove,
      *confirm_remove, *yes, *cancel, *game_running, *done_installed, *done_removed, *not_wwe12, *no_manifest,
      *bad_zip, *bad_type, *no_files, *failed, *conflict_player, *conflict_mod, *files_count;
};

const Texts kEnglish = {
    "Installed mods (.svrmod)", "Install a mod...", "No mod installed yet.", "Name", "Version", "Author",
    "Type", "On", "Remove", "Remove this mod for good?", "Yes", "Cancel",
    "The game is running: close it to install or change mods.", "Installed:", "Mod removed.",
    "This mod is not for WWE '12 (its manifest has no game=wwe12; SvR 2011 mods don't work here).",
    "Not a mod: manifest.txt is missing at the root of the zip.", "Not a mod or a damaged zip:",
    "This version can't install mods of type", "This mod has no files for the game (no files\\ or textures\\ folder).",
    "Failed:", "%s: left out, a file you put in mods yourself is already there.",
    "%s: left out, the mod %s already gives it.", "file(s)",
};

const Texts kFrench = {
    "Mods installés (.svrmod)", "Installer un mod...", "Aucun mod installé.", "Nom", "Version", "Auteur",
    "Type", "Actif", "Supprimer", "Supprimer ce mod pour de bon ?", "Oui", "Annuler",
    "Le jeu est lancé : fermez-le pour installer ou modifier des mods.", "Installé :", "Mod supprimé.",
    "Ce mod n'est pas pour WWE '12 (son manifest n'a pas game=wwe12 ; les mods de SvR 2011 ne marchent "
    "pas ici).",
    "Ce n'est pas un mod : manifest.txt manque à la racine du zip.", "Ce n'est pas un mod, ou zip abîmé :",
    "Cette version ne sait pas installer les mods de type",
    "Ce mod n'a pas de fichiers pour le jeu (pas de dossier files\\ ni textures\\).", "Échec :",
    "%s : laissé de côté, un fichier que vous avez mis vous-même dans mods est déjà là.",
    "%s : laissé de côté, le mod %s le fournit déjà.", "fichier(s)",
};

struct Mod {
  fs::path dir;
  std::string kind, id, name, author, version, type;
  bool on = true;
  size_t files = 0;
};

class Manager {
 public:
  void Init(const fs::path &mods_dir) {
    mods_ = mods_dir;
    root_ = mods_ / ".svrmods";
    textures_ = mods_.parent_path() / "textures";
    Refresh();
    if (!game::IsRunning())
      Deploy();  // a mod folder removed or changed by hand is caught up here
  }

  // True when the launcher put this file of the mods folder there (path relative to mods, '/').
  bool IsDeployed(const std::string &rel) const { return deployed_.count(Key(rel)) != 0; }

  // Installs a .svrmod made by the launcher itself (the entrance movie maker); the result shows in the list.
  void InstallFile(bool french, const fs::path &zip) { Install(french ? kFrench : kEnglish, zip); }
  const std::string &Message() const { return message_; }
  const std::vector<Mod> &List() const { return list_; }

  // Writes a zip (entries stored, not compressed: the movies are compressed already).
  static bool WriteZip(const fs::path &file, const std::vector<std::pair<std::string, std::vector<uint8_t>>> &entries) {
    std::vector<uint8_t> z, cd;
    auto u16 = [](std::vector<uint8_t> &v, uint32_t x) { v.push_back(uint8_t(x)), v.push_back(uint8_t(x >> 8)); };
    auto u32 = [&](std::vector<uint8_t> &v, uint32_t x) { u16(v, x), u16(v, x >> 16); };
    for (const auto &[name, bytes] : entries) {
      const uint32_t crc = Crc32(bytes.data(), bytes.size()), size = uint32_t(bytes.size()), at = uint32_t(z.size());
      u32(z, 0x04034b50), u16(z, 10), u16(z, 0x800), u16(z, 0), u16(z, 0), u16(z, 0x21);  // UTF-8 names
      u32(z, crc), u32(z, size), u32(z, size), u16(z, uint32_t(name.size())), u16(z, 0);
      z.insert(z.end(), name.begin(), name.end());
      z.insert(z.end(), bytes.begin(), bytes.end());
      u32(cd, 0x02014b50), u16(cd, 20), u16(cd, 10), u16(cd, 0x800), u16(cd, 0), u16(cd, 0), u16(cd, 0x21);
      u32(cd, crc), u32(cd, size), u32(cd, size), u16(cd, uint32_t(name.size())), u16(cd, 0), u16(cd, 0);
      u16(cd, 0), u16(cd, 0), u32(cd, 0), u32(cd, at);
      cd.insert(cd.end(), name.begin(), name.end());
    }
    const uint32_t cd_at = uint32_t(z.size());
    z.insert(z.end(), cd.begin(), cd.end());
    u32(z, 0x06054b50), u16(z, 0), u16(z, 0), u16(z, uint32_t(entries.size())), u16(z, uint32_t(entries.size()));
    u32(z, uint32_t(cd.size())), u32(z, cd_at), u16(z, 0);
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char *>(z.data()), std::streamsize(z.size()));
    return bool(f);
  }

  void Draw(bool french) {
    const Texts &t = french ? kFrench : kEnglish;
    const bool running = game::IsRunning();
    ImGui::Text("%s", t.installed);
    ImGui::BeginDisabled(running);
    if (ImGui::Button(t.install)) {
      const fs::path zip = PickModFile();
      if (!zip.empty())
        Install(t, zip);
    }
    ImGui::EndDisabled();
    if (running)
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", t.game_running);
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? ImVec4(0.55f, 0.85f, 0.45f, 1.0f) : ImVec4(1.0f, 0.45f, 0.35f, 1.0f),
                         "%s", message_.c_str());
    for (const auto &c : conflicts_) {
      char line[1024];
      if (c.other.empty())
        snprintf(line, sizeof(line), t.conflict_player, c.path.c_str());
      else
        snprintf(line, sizeof(line), t.conflict_mod, c.path.c_str(), c.other.c_str());
      ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s", line);
    }

    if (list_.empty()) {
      ImGui::TextDisabled("%s", t.none);
    } else if (ImGui::BeginTable("svrmods", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn(t.col_on, ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn(t.col_name);
      ImGui::TableSetupColumn(t.col_version, ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn(t.col_author);
      ImGui::TableSetupColumn(t.col_type, ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableHeadersRow();
      for (size_t i = 0; i < list_.size(); ++i) {
        Mod &m = list_[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(running);
        bool on = m.on;
        if (ImGui::Checkbox("##on", &on)) {
          toggle_ = m.dir;  // applied after the table: it rebuilds the list
          toggle_on_ = on;
        }
        ImGui::EndDisabled();
        ImGui::TableNextColumn();
        ImGui::Text("%s  (%zu %s)", m.name.c_str(), m.files, t.files_count);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(m.version.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(m.author.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(m.type.c_str());
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(running);
        if (ImGui::Button(t.remove)) {
          remove_ = m.dir;
          ask_remove_ = true;
        }
        ImGui::EndDisabled();
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    if (!toggle_.empty()) {
      SetOn(toggle_, toggle_on_);
      toggle_.clear();
    }

    if (ask_remove_) {
      ImGui::OpenPopup("##remove_mod");
      ask_remove_ = false;
    }
    if (ImGui::BeginPopupModal("##remove_mod", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted(t.confirm_remove);
      ImGui::Dummy(ImVec2(0, 6));
      if (ImGui::Button(t.yes)) {
        Remove(t, remove_);
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button(t.cancel))
        ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
  }

 private:
  struct Conflict {
    std::string path, other;  // other: the mod that won ("" = a file the player put there)
  };
  struct Deployed {
    std::string rel, mod;
    uintmax_t size = 0;
  };

  // ---- small helpers

  static std::string Lower(std::string s) {
    for (auto &c : s)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  }
  static std::string Key(const std::string &rel) { return Lower(rel); }  // Windows: case doesn't matter
  static fs::path FromUtf8(const std::string &s) { return fs::path(std::u8string(s.begin(), s.end())); }
  static std::string ToUtf8(const fs::path &p) {
    const std::u8string u = p.generic_u8string();
    return std::string(u.begin(), u.end());
  }

  // key=value lines; the key matches from the first character (as in Kaiko's format).
  static std::string Value(const std::string &text, const char *key) {
    const std::string k = std::string(key) + "=";
    size_t pos = 0;
    std::string found;
    while (pos < text.size()) {
      size_t end = text.find('\n', pos);
      if (end == std::string::npos)
        end = text.size();
      std::string line = text.substr(pos, end - pos);
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.compare(0, k.size(), k) == 0)
        found = line.substr(k.size());  // the last one wins
      pos = end + 1;
    }
    return found;
  }

  // Kaiko's folder per type (mod_check.cpp KindOfType); "" = a type this version doesn't know.
  static std::string KindOf(const std::string &type) {
    static const std::map<std::string, std::string> kinds = {
        {"superstar", "Superstars"}, {"attire", "Attires"},   {"arena", "Arenas"},
        {"moves", "Moves"},          {"backstage", "Backstage"}, {"signs", "Signs"},
        {"media", "Media"},          {"background", "Backgrounds"}, {"matchtype", "MatchTypes"},
        {"textures", "Textures"}};
    const auto it = kinds.find(Lower(type));
    return it == kinds.end() ? std::string() : it->second;
  }

  // The folder name from id=: letters, digits, '_', '-' and '.'; anything else becomes '_'.
  static std::string SafeId(const std::string &id) {
    std::string s;
    for (unsigned char c : id)
      s += (std::isalnum(c) || c == '_' || c == '-' || c == '.') ? static_cast<char>(c) : '_';
    if (s.empty() || s == "." || s == "..")
      s = "mod";
    return s;
  }

  static uint32_t Crc32(const uint8_t *data, size_t size) {
    static const auto table = [] {
      std::vector<uint32_t> t(256);
      for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k)
          c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        t[i] = c;
      }
      return t;
    }();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i)
      crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
  }
  static uint32_t U32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
  static uint16_t U16(const uint8_t *p) { return uint16_t(p[0] | (p[1] << 8)); }

  struct Entry {
    std::string name;  // '/'-separated, relative
    std::vector<uint8_t> bytes;
  };

  // The whole zip in memory: stored and deflate entries, sizes and CRCs checked, only plain relative
  // paths (no "..", ':', absolute paths), no zip64, no encryption.
  static bool ReadZip(const fs::path &file, std::vector<Entry> &out, std::string &err) {
    std::ifstream f(file, std::ios::binary);
    std::vector<uint8_t> zip((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (zip.size() < 22) {
      err = "too small";
      return false;
    }
    size_t eocd = zip.size() - 22;
    const size_t stop = zip.size() > 22 + 65535 ? zip.size() - 22 - 65535 : 0;
    while (eocd > stop && U32(&zip[eocd]) != 0x06054b50)
      --eocd;
    if (U32(&zip[eocd]) != 0x06054b50) {
      err = "zip directory not found";
      return false;
    }
    const uint16_t entries = U16(&zip[eocd + 10]);
    size_t cd = U32(&zip[eocd + 16]);
    for (uint16_t i = 0; i < entries; ++i) {
      if (cd + 46 > zip.size() || U32(&zip[cd]) != 0x02014b50) {
        err = "zip directory damaged";
        return false;
      }
      const uint16_t flags = U16(&zip[cd + 8]), method = U16(&zip[cd + 10]);
      const uint32_t crc = U32(&zip[cd + 16]), csize = U32(&zip[cd + 20]), usize = U32(&zip[cd + 24]);
      const uint16_t nlen = U16(&zip[cd + 28]), xlen = U16(&zip[cd + 30]), clen = U16(&zip[cd + 32]);
      const uint32_t local = U32(&zip[cd + 42]);
      if (cd + 46 + nlen > zip.size()) {
        err = "zip directory damaged";
        return false;
      }
      std::string name(reinterpret_cast<const char *>(&zip[cd + 46]), nlen);
      cd += 46 + nlen + xlen + clen;
      for (auto &c : name)
        if (c == '\\')
          c = '/';
      if (name.empty() || name.back() == '/')
        continue;  // a folder
      bool unsafe = name[0] == '/' || name.find(':') != std::string::npos;
      for (size_t p = 0; !unsafe && p <= name.size();) {
        const size_t q = std::min(name.find('/', p), name.size());
        unsafe = name.compare(p, q - p, "..") == 0;
        p = q + 1;
      }
      if (unsafe) {
        err = "unsafe path: " + name;
        return false;
      }
      if (flags & 1) {
        err = "encrypted: " + name;
        return false;
      }
      if (csize == 0xFFFFFFFFu || usize == 0xFFFFFFFFu || local + 30 > zip.size()) {
        err = "not supported: " + name;
        return false;
      }
      const size_t data = local + 30 + U16(&zip[local + 26]) + U16(&zip[local + 28]);
      if (data + csize > zip.size()) {
        err = "truncated: " + name;
        return false;
      }
      Entry e;
      e.name = name;
      if (method == 0) {
        e.bytes.assign(zip.begin() + data, zip.begin() + data + csize);
      } else if (method == 8) {
        e.bytes.reserve(usize);
        if (!Inflater::Inflate(&zip[data], csize, e.bytes)) {
          err = "cannot unpack " + name;
          return false;
        }
      } else {
        err = "method not supported: " + name;
        return false;
      }
      if (e.bytes.size() != usize || Crc32(e.bytes.data(), e.bytes.size()) != crc) {
        err = "damaged (checksum): " + name;
        return false;
      }
      out.push_back(std::move(e));
    }
    return true;
  }

  static fs::path PickModFile() {
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
      return {};
    const COMDLG_FILTERSPEC filter[] = {{L"Mods (*.svrmod)", L"*.svrmod"}, {L"*.*", L"*.*"}};
    dlg->SetFileTypes(2, filter);
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    fs::path picked;
    if (SUCCEEDED(dlg->Show(GetActiveWindow()))) {
      IShellItem *item = nullptr;
      if (SUCCEEDED(dlg->GetResult(&item))) {
        PWSTR p = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
          picked = p;
          CoTaskMemFree(p);
        }
        item->Release();
      }
    }
    dlg->Release();
    return picked;
  }

  // ---- the list of installed mods

  void Refresh() {
    list_.clear();
    std::error_code ec;
    std::vector<fs::path> dirs;
    for (fs::directory_iterator k(root_, ec), end; !ec && k != end; k.increment(ec)) {
      if (!k->is_directory(ec) || k->path().filename().string()[0] == '.')
        continue;
      for (fs::directory_iterator m(k->path(), ec), e2; !ec && m != e2; m.increment(ec))
        if (m->is_directory(ec))
          dirs.push_back(m->path());
      ec.clear();
    }
    // Kind then folder name: the order "first one wins" follows.
    std::sort(dirs.begin(), dirs.end());
    for (const auto &d : dirs) {
      std::ifstream f(d / "manifest.txt", std::ios::binary);
      const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
      Mod m;
      m.dir = d;
      m.kind = d.parent_path().filename().string();
      m.id = d.filename().string();
      m.name = Value(text, "name");
      if (m.name.empty())
        m.name = m.id;
      m.author = Value(text, "author");
      m.version = Value(text, "version");
      m.type = Value(text, "type");
      m.on = !fs::exists(d / "disabled", ec);
      for (const char *sub : {"files", "textures", "arena", "superstar"}) {
        for (fs::recursive_directory_iterator it(d / sub, ec), end; !ec && it != end; it.increment(ec))
          if (it->is_regular_file(ec))
            ++m.files;
        ec.clear();
      }
      list_.push_back(std::move(m));
    }
  }

  // ---- putting the files of the mods that are on into the mods folder

  void ReadDeployed() {
    deployed_.clear();
    std::ifstream f(root_ / "deployed.txt", std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      const size_t a = line.find('\t'), b = line.find('\t', a + 1);
      if (a == std::string::npos || b == std::string::npos)
        continue;
      Deployed d{line.substr(0, a), line.substr(a + 1, b - a - 1),
                 std::strtoull(line.c_str() + b + 1, nullptr, 10)};
      deployed_[Key(d.rel)] = d;
    }
  }

  void WriteDeployed() {
    std::error_code ec;
    fs::create_directories(root_, ec);
    std::ofstream f(root_ / "deployed.txt", std::ios::binary | std::ios::trunc);
    for (const auto &[key, d] : deployed_)
      f << d.rel << '\t' << d.mod << '\t' << d.size << '\n';
  }

  void Deploy() {
    ReadDeployed();
    conflicts_.clear();
    std::error_code ec;
    // What should be there: path -> (source file, mod). First mod (Kind, then folder name) wins.
    struct Want {
      std::string rel, mod;
      fs::path source;
    };
    std::map<std::string, Want> want;
    for (const auto &m : list_) {
      if (!m.on)
        continue;
      // files\ -> mods\ (disc paths); textures\ -> textures\<id>\ (texture packs).
      const std::pair<fs::path, std::string> sources[] = {{m.dir / "files", ""},
                                                          {m.dir / "textures", "textures:" + m.id + "/"}};
      for (const auto &[from, prefix] : sources) {
        for (fs::recursive_directory_iterator it(from, ec), end; !ec && it != end; it.increment(ec)) {
          if (!it->is_regular_file(ec))
            continue;
          const std::string rel = prefix + ToUtf8(fs::relative(it->path(), from, ec));
          if (rel.size() == prefix.size() || rel.rfind(".svrmods", 0) == 0)
            continue;
          const auto found = want.find(Key(rel));
          if (found != want.end())
            conflicts_.push_back({rel, found->second.mod});
          else
            want[Key(rel)] = {rel, m.id, it->path()};
        }
        ec.clear();
      }
    }
    // Take away what the launcher put there and is no longer wanted (only if still the same file).
    for (auto it = deployed_.begin(); it != deployed_.end();) {
      const auto w = want.find(it->first);
      if (w != want.end() && w->second.mod == it->second.mod) {
        ++it;
        continue;
      }
      const fs::path target = Target(it->second.rel);
      if (fs::exists(target, ec) && fs::file_size(target, ec) == it->second.size)
        fs::remove(target, ec);
      it = deployed_.erase(it);
    }
    // Put in what is missing.
    for (const auto &[key, w] : want) {
      const fs::path target = Target(w.rel);
      const uintmax_t size = fs::file_size(w.source, ec);
      const auto d = deployed_.find(key);
      if (d != deployed_.end() && fs::exists(target, ec) && fs::file_size(target, ec) == size)
        continue;  // already there
      if (d == deployed_.end() && fs::exists(target, ec)) {
        conflicts_.push_back({w.rel, ""});  // the player's own file: never replaced
        continue;
      }
      fs::create_directories(target.parent_path(), ec);
      fs::remove(target, ec);
      ec.clear();
      fs::create_hard_link(w.source, target, ec);
      if (ec) {
        ec.clear();
        fs::copy_file(w.source, target, fs::copy_options::overwrite_existing, ec);
      }
      if (!ec)
        deployed_[key] = {w.rel, w.mod, size};
    }
    WriteDeployed();
  }

  // ---- actions

  void Install(const Texts &t, const fs::path &zip_file) {
    message_ok_ = false;
    std::vector<Entry> entries;
    std::string err;
    if (!ReadZip(zip_file, entries, err)) {
      message_ = std::string(t.bad_zip) + " " + err;
      return;
    }
    const auto manifest = std::find_if(entries.begin(), entries.end(),
                                       [](const Entry &e) { return Lower(e.name) == "manifest.txt"; });
    if (manifest == entries.end()) {
      message_ = t.no_manifest;
      return;
    }
    const std::string text(manifest->bytes.begin(), manifest->bytes.end());
    if (Lower(Value(text, "game")) != "wwe12") {
      message_ = t.not_wwe12;
      return;
    }
    const std::string type = Value(text, "type"), kind = KindOf(type);
    if (kind.empty()) {
      message_ = std::string(t.bad_type) + " \"" + type + "\".";
      return;
    }
    if (std::none_of(entries.begin(), entries.end(), [](const Entry &e) {
          const std::string n = Lower(e.name);
          return n.rfind("files/", 0) == 0 || n.rfind("textures/", 0) == 0 || n.rfind("arena/", 0) == 0 ||
                 n.rfind("superstar/", 0) == 0;
        })) {
      message_ = t.no_files;
      return;
    }
    std::string id = Value(text, "id");
    if (id.empty())
      id = zip_file.stem().string();
    id = SafeId(id);

    // Unpack beside, then swap in (a mod with the same id is replaced; its "disabled" is kept).
    std::error_code ec;
    const fs::path incoming = root_ / ".incoming", dest = root_ / kind / FromUtf8(id);
    fs::remove_all(incoming, ec);
    for (const auto &e : entries) {
      const fs::path out = incoming / FromUtf8(e.name);
      fs::create_directories(out.parent_path(), ec);
      std::ofstream f(out, std::ios::binary | std::ios::trunc);
      f.write(reinterpret_cast<const char *>(e.bytes.data()), static_cast<std::streamsize>(e.bytes.size()));
      if (!f) {
        message_ = std::string(t.failed) + " " + ToUtf8(out);
        fs::remove_all(incoming, ec);
        return;
      }
    }
    const bool was_off = fs::exists(dest / "disabled", ec);
    fs::remove_all(dest, ec);
    fs::create_directories(dest.parent_path(), ec);
    fs::rename(incoming, dest, ec);
    if (ec) {
      message_ = std::string(t.failed) + " " + ec.message();
      fs::remove_all(incoming, ec);
      Refresh();
      Deploy();
      return;
    }
    if (was_off)
      std::ofstream(dest / "disabled").put('\n');
    Refresh();
    Deploy();
    const std::string name = Value(text, "name");
    message_ = std::string(t.done_installed) + " " + (name.empty() ? id : name);
    message_ok_ = true;
  }

  void SetOn(const fs::path &dir, bool on) {
    std::error_code ec;
    if (on)
      fs::remove(dir / "disabled", ec);
    else
      std::ofstream(dir / "disabled").put('\n');
    message_.clear();
    Refresh();
    Deploy();
  }

  void Remove(const Texts &t, const fs::path &dir) {
    std::error_code ec;
    fs::remove_all(dir, ec);
    Refresh();
    Deploy();
    message_ok_ = !ec;
    message_ = ec ? std::string(t.failed) + " " + ec.message() : t.done_removed;
  }

  // Where a deployed file goes: "textures:<id>/<path>" under textures\, anything else under mods\.
  fs::path Target(const std::string &rel) const {
    static const std::string kTex = "textures:";
    if (rel.compare(0, kTex.size(), kTex) == 0)
      return textures_ / FromUtf8(rel.substr(kTex.size()));
    return mods_ / FromUtf8(rel);
  }

  fs::path mods_, root_, textures_, remove_, toggle_;
  bool toggle_on_ = false;
  std::vector<Mod> list_;
  std::map<std::string, Deployed> deployed_;
  std::vector<Conflict> conflicts_;
  std::string message_;
  bool message_ok_ = false, ask_remove_ = false;
};

}  // namespace mods
