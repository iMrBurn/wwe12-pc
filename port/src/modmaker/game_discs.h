// Mod Maker, "My game discs" page (docs/plan_page_arenes.md, step 1): the player's other WWE games, set once (a disc
// image or a game folder each) and kept in modmaker_discs.txt next to the Mod Maker ("title id|path" lines). The
// ports read them from there; nothing is copied. A game is told by its default.xex's title id (execution info,
// optional header 0x40006): SvR 2011 5451085D, WWE '12 54510890, WWE '13 545108B4, 2K14 545408B2, 2K15 545408B6,
// 2K16 315A07D2, 2K17 545408BA (read from the user's own discs).
#pragma once

#include <windows.h>
#include <shobjidl.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "game_disc.h"
#include "imgui.h"

namespace game_discs {

namespace fs = std::filesystem;

// What the Mod Maker can bring from a game today.
enum class Can { kNo, kSoon, kYes };

struct Known {
  uint32_t title;
  const char *name;
  Can arenas, superstars;
};

inline constexpr Known kKnown[] = {
    {0x5451085D, "WWE SmackDown vs. Raw 2011", Can::kYes, Can::kYes},
    {0x54510890, "WWE '12", Can::kNo, Can::kNo},  // (this game)
    {0x545108B4, "WWE '13", Can::kYes, Can::kYes},
    {0x545408B2, "WWE 2K14", Can::kYes, Can::kSoon},
    {0x545408B6, "WWE 2K15", Can::kYes, Can::kSoon},
    {0x315A07D2, "WWE 2K16", Can::kYes, Can::kSoon},
    {0x545408BA, "WWE 2K17", Can::kYes, Can::kSoon},
};

inline const Known *Find(uint32_t title) {
  for (const auto &k : kKnown)
    if (k.title == title)
      return &k;
  return nullptr;
}

inline std::string U8(const fs::path &p) {
  const auto s = p.u8string();
  return std::string(s.begin(), s.end());
}

// The title id of the game in an ISO or a folder; 0 when it has no readable default.xex.
inline uint32_t TitleOf(const fs::path &p) {
  disc::GameDisc d;
  std::vector<uint8_t> x;
  if (!d.OpenPath(p) || !d.Read("default.xex", x) || x.size() < 24 || memcmp(x.data(), "XEX2", 4) != 0)
    return 0;
  const auto be = [&](size_t o) {
    return o + 4 <= x.size() ? uint32_t(x[o]) << 24 | x[o + 1] << 16 | x[o + 2] << 8 | x[o + 3] : 0u;
  };
  const uint32_t n = be(20);
  for (uint32_t k = 0; k < n && 24 + 8 * size_t(k) + 8 <= x.size(); ++k)
    if (be(24 + 8 * size_t(k)) == 0x40006)
      return be(size_t(be(28 + 8 * size_t(k))) + 12);
  return 0;
}

struct Disc {
  uint32_t title = 0;
  fs::path path;
};

class Discs {
 public:
  void Load(const fs::path &file) {
    file_ = file;
    list_.clear();
    std::ifstream f(file);
    std::string line;
    while (std::getline(f, line)) {
      const size_t bar = line.find('|');
      if (bar == std::string::npos)
        continue;
      Disc d;
      d.title = uint32_t(strtoul(line.substr(0, bar).c_str(), nullptr, 16));
      const std::string u = line.substr(bar + 1);
      d.path = fs::path(std::u8string(u.begin(), u.end()));
      if (d.title)
        list_.push_back(d);
    }
  }

  // The disc of a game (its title id), else an empty path.
  fs::path PathOf(uint32_t title) const {
    for (const auto &d : list_)
      if (d.title == title)
        return d.path;
    return {};
  }

  // A disc image or a game folder: added (or put in place of the same game's), "" or why not.
  std::string Add(bool fr, const fs::path &p) {
    const uint32_t title = TitleOf(p);
    if (!title)
      return fr ? "Pas un jeu Xbox 360 lisible (pas de default.xex)." : "Not a readable Xbox 360 game (no default.xex).";
    if (title == 0x54510890)
      return fr ? "C'est WWE '12 : le Mod Maker le lit déjà à côté du jeu." : "That's WWE '12: the Mod Maker reads it "
                                                                              "next to the game already.";
    list_.erase(std::remove_if(list_.begin(), list_.end(), [&](const Disc &d) { return d.title == title; }),
                list_.end());
    list_.push_back({title, p});
    std::sort(list_.begin(), list_.end(), [](const Disc &a, const Disc &b) { return Order(a.title) < Order(b.title); });
    Save();
    return {};
  }

  // Every ISO, and every folder holding a default.xex, under `root` (three levels down): the WWE games among them.
  int Scan(bool fr, const fs::path &root) {
    int added = 0;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
      if (it.depth() > 2) {
        it.disable_recursion_pending();
        continue;
      }
      const fs::path p = it->path();
      std::string ext = p.extension().string();
      for (auto &c : ext)
        c = char(tolower(static_cast<unsigned char>(c)));
      const bool iso = it->is_regular_file(ec) && ext == ".iso";
      const bool folder = it->is_directory(ec) && fs::exists(p / "default.xex", ec);
      if (!iso && !folder)
        continue;
      const uint32_t title = TitleOf(p);
      if (Find(title) && title != 0x54510890 && PathOf(title).empty() && Add(fr, p).empty())
        ++added;
      if (folder)
        it.disable_recursion_pending();
    }
    return added;
  }

  void Draw(bool fr) {
    ImGui::TextWrapped("%s", fr ? "Vos autres jeux WWE, ajoutés une fois : l'image du disque (.iso) ou le dossier du "
                                  "jeu. Les pages ARÈNES et NOUVEAU s'en servent pour porter leurs arènes et leurs "
                                  "catcheurs. Rien n'est copié : le Mod Maker lit vos fichiers au moment du portage."
                                : "Your other WWE games, added once: the disc image (.iso) or the game's folder. The "
                                  "ARENAS and NEW SUPERSTAR pages port their arenas and superstars from them. Nothing "
                                  "is copied: the Mod Maker reads your files when it ports.");
    ImGui::Dummy(ImVec2(0, 6));
    if (ImGui::Button(fr ? "Ajouter un ISO..." : "Add an ISO...")) {
      const fs::path p = Pick(fr, false);
      if (!p.empty())
        note_ = Add(fr, p), note_ok_ = note_.empty(), note_ = note_ok_ ? (fr ? "Ajouté." : "Added.") : note_;
    }
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Ajouter un dossier de jeu..." : "Add a game folder...")) {
      const fs::path p = Pick(fr, true);
      if (!p.empty())
        note_ = Add(fr, p), note_ok_ = note_.empty(), note_ = note_ok_ ? (fr ? "Ajouté." : "Added.") : note_;
    }
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Chercher mes disques dans un dossier..." : "Find my discs in a folder...")) {
      const fs::path p = Pick(fr, true);
      if (!p.empty()) {
        const int n = Scan(fr, p);
        char b[160];
        snprintf(b, sizeof(b), fr ? "%d jeu(x) trouvé(s) et ajouté(s)." : "%d game(s) found and added.", n);
        note_ = b;
        note_ok_ = n > 0;
      }
    }
    if (!note_.empty())
      ImGui::TextColored(note_ok_ ? ImVec4(0.55f, 0.85f, 0.45f, 1.0f) : ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s",
                         note_.c_str());
    ImGui::Dummy(ImVec2(0, 8));
    if (ImGui::BeginTable("discs", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
      ImGui::TableSetupColumn(fr ? "Jeu" : "Game", ImGuiTableColumnFlags_WidthFixed, 230);
      ImGui::TableSetupColumn(fr ? "Arènes" : "Arenas", ImGuiTableColumnFlags_WidthFixed, 90);
      ImGui::TableSetupColumn(fr ? "Catcheurs" : "Superstars", ImGuiTableColumnFlags_WidthFixed, 90);
      ImGui::TableSetupColumn(fr ? "Où" : "Where");
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 80);
      ImGui::TableHeadersRow();
      int remove = -1;
      for (const auto &k : kKnown) {
        if (k.title == 0x54510890)
          continue;
        const fs::path p = PathOf(k.title);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(k.name);
        for (Can c : {k.arenas, k.superstars}) {
          ImGui::TableNextColumn();
          if (c == Can::kYes)
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f), "%s", fr ? "oui" : "yes");
          else
            ImGui::TextDisabled("%s", c == Can::kSoon ? (fr ? "bientôt" : "coming") : "-");
        }
        ImGui::TableNextColumn();
        std::error_code ec;
        if (p.empty())
          ImGui::TextDisabled("%s", fr ? "pas ajouté" : "not added");
        else if (!fs::exists(p, ec))
          ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s %s", fr ? "introuvable :" : "missing:",
                             U8(p).c_str());
        else
          ImGui::TextUnformatted(U8(p).c_str());
        ImGui::TableNextColumn();
        if (!p.empty()) {
          ImGui::PushID(int(k.title));
          if (ImGui::SmallButton(fr ? "Retirer" : "Remove"))
            remove = int(&k - kKnown);
          ImGui::PopID();
        }
      }
      ImGui::EndTable();
      if (remove >= 0) {
        const uint32_t t = kKnown[remove].title;
        list_.erase(std::remove_if(list_.begin(), list_.end(), [&](const Disc &d) { return d.title == t; }),
                    list_.end());
        Save();
      }
    }
  }

 private:
  static int Order(uint32_t title) {
    for (int i = 0; i < int(std::size(kKnown)); ++i)
      if (kKnown[i].title == title)
        return i;
    return 99;
  }

  void Save() const {
    std::ofstream f(file_, std::ios::trunc);
    for (const auto &d : list_) {
      char t[16];
      snprintf(t, sizeof(t), "%08X", d.title);
      f << t << '|' << U8(d.path) << '\n';
    }
  }

  static fs::path Pick(bool fr, bool folder) {
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
      return {};
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    if (folder) {
      dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    } else {
      const COMDLG_FILTERSPEC filter[] = {{fr ? L"Image du jeu (*.iso)" : L"Game image (*.iso)", L"*.iso"},
                                          {L"*.*", L"*.*"}};
      dlg->SetFileTypes(2, filter);
      dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    }
    fs::path picked;
    if (SUCCEEDED(dlg->Show(GetActiveWindow()))) {
      IShellItem *item = nullptr;
      if (SUCCEEDED(dlg->GetResult(&item))) {
        PWSTR q = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &q))) {
          picked = q;
          CoTaskMemFree(q);
        }
        item->Release();
      }
    }
    dlg->Release();
    return picked;
  }

  fs::path file_;
  std::vector<Disc> list_;
  std::string note_;
  bool note_ok_ = false;
};

}  // namespace game_discs
