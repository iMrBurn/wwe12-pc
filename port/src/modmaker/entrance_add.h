// Mod Maker, MOVES & ENTRANCES, tab "Add an entrance" (the user, 2026-11-12: "like the titantrons, add them to Create
// an Entrance", SvR's too): one entrance of another game (My game discs), no superstar, made WWE '12's. The mod
// (.svrmod, type=entrance, kind folder Entrances) holds manifest entrance_own=<number> and name=, its parts as a
// superstar mod's (superstar/entrance/, superstar/objects/); at startup the game serves it under that number and puts
// it, under its name, in CREATE AN ENTRANCE's entrance lists (wwe12_arena_pages.cpp, MountEntrances and the
// sub_829F9F90 hook), where any superstar may take it, as his entrance or his champion's.
//  - WWE '13 / 2K14-2K17: every entrance of the disc (star_port::EntranceList: its script's name), made as a
//    champion's entrance is (star_port::PortChamp / FinishChamp: effects, objects, people, face);
//  - SvR 2007-2011: the entrances of the scene pacs (Nyujyo*.pac, SvR 2007 evt1 / evt2.pac), named after the
//    superstars whose entrance it is (svr_old_entrance::EntranceOf), made WWE '12's as for a ported superstar
//    (svr_scenes::Entrance; their props are another format: hidden).
// Served under a number neither WWE '12 nor an installed mod has (499 down to 100).
#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <future>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "arena_page.h"
#include "entrance_preview.h"
#include "game_icons.h"
#include "imgui.h"
#include "media.h"
#include "mod_version.h"
#include "mods.h"
#include "star_port.h"

namespace entrance_add {

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

struct Item {
  int number = 0;     // its number in its game
  std::string name;   // its script's name (WWE '13 / 2K) or the superstars it is the entrance of (SvR)
};

// The entrances of `game` (its disc open in src): by number.
inline std::vector<Item> ListOf(const game_discs::Discs &discs, const disc::GameDisc &w12, uint32_t game,
                                const disc::GameDisc &src) {
  std::vector<Item> out;
  if (star_port::Scenes(game)) {
    for (const auto &[n, nm] : star_port::EntranceList(src))
      out.push_back({n, nm});
    return out;
  }
  if (!star_port::Svr(game))
    return out;
  // (SvR: whose entrance each number is, from the superstars' profiles)
  std::map<int, std::string> whose;
  {
    star_port::GameList g;
    star_port::ListGame(&discs, &w12, game, g);
    for (const auto &[id, nm] : g.list)
      if (const int e = svr_old_entrance::EntranceOf(src, game, id); e > 0) {
        std::string &w = whose[e];
        w += (w.empty() ? "" : ", ") + nm;
      }
  }
  std::set<int> seen;
  const std::string stem = game == svr_old_entrance::kSvr07 ? "evt" : "nyujyo";
  for (const auto &f : src.List("pac/evt")) {
    std::string l = f;
    for (auto &c : l)
      c = char(std::tolower(uint8_t(c)));
    // (Nyujyo0-9,A.pac, not the tests TNyujyo; SvR 2007: evt1 / evt2.pac)
    if (l.size() != stem.size() + 5 || l.compare(0, stem.size(), stem) || l.compare(stem.size() + 1, 4, ".pac") ||
        !std::isalnum(uint8_t(l[stem.size()])) ||
        (game == svr_old_entrance::kSvr07 && l[3] != '1' && l[3] != '2'))
      continue;
    svr_old_entrance::detail::Entries(src, "pac/evt/" + f, [&](const std::string &grp, int n, bool, const Bytes & = {}) {
      // (an entrance's script: under 1000 - SvR 2010 / 2011 - or 4000-4999 - SvR 2007-2009; 1xNNN: its variants)
      if (!grp.rfind("EVP", 0) && ((n >= 1 && n < 1000) || (n >= 4000 && n < 5000)))
        seen.insert(n);
      return false;
    });
  }
  for (int n : seen) {
    const auto w = whose.find(n);
    out.push_back({n, w != whose.end() ? w->second : std::string()});
  }
  return out;
}

// What a ported entrance is.
struct Ported {
  std::vector<entrance13::Part> parts;    // its script, tracks, game event script, effects
  std::vector<entrance13::Part> objects;  // the objects WWE '12 hasn't (WWE '13 / 2K)
  int number = 0;                          // the number it is served under
  std::string why;                         // why it couldn't be, else ""
};

// Entrance `from` of `game` as WWE '12's, under a free number.
inline Ported Port(const disc::GameDisc &w12, const game_discs::Discs &discs, uint32_t game, int from,
                   star_port::Music12 &music) {
  Ported r;
  disc::GameDisc src;
  if (discs.PathOf(game).empty() || !src.OpenPath(discs.PathOf(game))) {
    r.why = "its game isn't in My game discs";
    return r;
  }
  if (star_port::Scenes(game)) {  // (as a champion's entrance picked on a page)
    star_port::Parts p;
    if (!star_port::PortChamp(w12, discs, src, game, from, 0, -1, music, p)) {
      r.why = "it could not be made WWE '12's (or no free number)";
      return r;
    }
    star_port::FinishChamp(w12, src, p);
    r.parts = std::move(p.champ);
    r.objects = std::move(p.objects);
    r.number = p.champ_number;
    return r;
  }
  // SvR: a free number first (SvR 2007-2009's are made under it; 2010 / 2011's renumbered to it)
  const std::set<int> taken = star_port::TakenEntrances();
  int to = 0;
  for (int n = 499; n >= 100 && !to; --n)
    if (n != from && !taken.count(n) && !entrance13::Group12(n).empty() && !entrance13::Exists12(w12, n) &&
        !music.Has(w12, n))
      to = n;
  if (!to) {
    r.why = "no free entrance number in WWE '12";
    return r;
  }
  disc::GameDisc disc11;
  const disc::GameDisc *with11 = star_port::SvrOld(game) && !discs.PathOf(svr_old_entrance::kSvr11).empty() &&
                                         disc11.OpenPath(discs.PathOf(svr_old_entrance::kSvr11))
                                     ? &disc11
                                     : nullptr;
  if (!svr_scenes::Entrance(src, w12, from, r.parts, &r.why, game, with11, to) || r.parts.empty()) {
    if (r.why.empty())
      r.why = "it could not be made WWE '12's";
    r.parts.clear();
    return r;
  }
  const int at = r.parts.front().number % 1000;
  if (at != to % 1000)
    entrance13::Renumber(r.parts, at, to);
  r.number = to;
  entrance13::HideMissing(r.parts, w12, r.objects);
  entrance13::Announcer12(r.parts, w12);
  entrance13::Waiting12(r.parts, w12);
  entrance13::WaitingFirst12(r.parts);
  entrance13::People12(r.parts, w12);
  face12::AddFaces(r.parts, w12, 139);
  r.why.clear();
  return r;
}

class Page {
 public:
  void Draw(bool fr, ID3D11Device *dev, const disc::GameDisc &w12, const game_discs::Discs *discs, mods::Manager &mods,
            const std::vector<entrance_preview::Star> &stars, bool &want_discs) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    device_ = dev;
    Poll();
    // (test aid: WWE12_MM_ADDENT=<game title hex>,<its entrance number>,<report file>: that game chosen, that entrance
    // made WWE '12's, what came of it written to the file)
    if (const char *t = getenv("WWE12_MM_ADDENT"); t && discs && test_step_ < 3) {
      unsigned title = 0;
      int number = 0;
      char out[260] = {};
      sscanf(t, "%x,%d,%259[^\n]", &title, &number, out);
      if (test_step_ == 0) {
        Choose(w12, discs, title);
        test_step_ = 1;
      } else if (test_step_ == 1 && !listing_.valid()) {
        for (const auto &it : list_)
          if (it.number == number)
            Pick(w12, *discs, it);
        test_step_ = 2;
      } else if (test_step_ == 2 && !porting_.valid()) {
        test_step_ = 3;
        if (FILE *f = fopen(out, "w")) {
          fprintf(f, "game %08X: %zu entrances\n", title, list_.size());
          for (const auto &it : list_)
            fprintf(f, "  %d %s\n", it.number, it.name.c_str());
          fprintf(f, "picked %d, name \"%s\": served as %d, why \"%s\", %zu parts, %zu objects\n", pick_, name_,
                  done_.number, done_.why.c_str(), done_.parts.size(), done_.objects.size());
          for (const auto &q : done_.parts)
            fprintf(f, "  %s %d (%zu bytes)\n", entrance13::ModPath(q).c_str(), q.number, q.data.size());
          fclose(f);
          // (and the parts themselves beside it, <report>.parts\<kind>_<number>.bin)
          std::error_code ec;
          const fs::path dir = fs::path(out).concat(".parts");
          fs::create_directories(dir, ec);
          for (const auto &q : done_.parts)
            if (FILE *g = _wfopen((dir / (q.kind + "_" + std::to_string(q.number) + ".bin")).c_str(), L"wb")) {
              fwrite(q.data.data(), 1, q.data.size(), g);
              fclose(g);
            }
        }
      }
    }
    // (test aid: WWE12_MM_KIDS=<game title hex, 0 WWE '12>,<pac path>,<report file>: each attire's children - ids,
    // models' names, layout, nodes -, to see a superstar file's extra models)
    if (const char *t = getenv("WWE12_MM_KIDS"); t && discs && !kids_done_) {
      kids_done_ = true;
      unsigned title = 0;
      char path[200] = {}, out[260] = {};
      sscanf(t, "%x,%199[^,],%259[^\n]", &title, path, out);
      disc::GameDisc src;
      const disc::GameDisc *g = title ? (src.OpenPath(discs->PathOf(title)) ? &src : nullptr) : &w12;
      svrfmt::Archive a;
      if (FILE *f = fopen(out, "w")) {
        if (!g || !entrance13::detail::OpenPac(*g, path, a))
          fprintf(f, "can't open %s\n", path);
        else
          for (auto &k : a.root.kids) {
            fprintf(f, "entry %s/%s id %u\n", k.group.c_str(), k.name.c_str(), k.id);
            if (!svrfmt::ArchiveExpand(k))
              continue;
            for (auto &c : k.kids) {
              const Bytes d = svrfmt::Unpack(svrfmt::ArchiveData(c));
              fprintf(f, "  kid %u (0x%x) %zu bytes", c.id, c.id, d.size());
              svrfmt::Model m;
              std::string e;
              if (svrfmt::IsJboy(d)) {
                if (svrfmt::JboyRead(d, m, &e)) {
                  fprintf(f, " JBOY %s w12 %d meshes %zu nodes %zu:", m.name.c_str(), int(m.w12), m.meshes.size(),
                          m.nodes.size());
                  const bool all = c.id >= 0x10 && c.id <= 0xFF;
                  for (size_t i = 0; i < m.nodes.size() && (all || i < 40); ++i)
                    fprintf(f, " %s:%d(%.1f %.1f %.1f)", m.nodes[i].name.c_str(), m.nodes[i].parent, m.nodes[i].t[0],
                            m.nodes[i].t[1], m.nodes[i].t[2]);
                  for (const auto &me : m.meshes) {
                    fprintf(f, "\n    palette");
                    for (int p : me.palette)
                      fprintf(f, " %d", p);
                    if (!me.verts.empty())
                      fprintf(f, " first vert %.1f %.1f %.1f", me.verts[0].pos[0], me.verts[0].pos[1], me.verts[0].pos[2]);
                    if (!all)
                      break;
                  }
                } else {
                  fprintf(f, " JBOY unreadable: %s", e.c_str());
                }
              }
              fprintf(f, "\n");
            }
          }
        fclose(f);
      }
    }
    ImGui::TextWrapped("%s", fr ? "Ajoutez une entrée de vos autres jeux, sans catcheur : elle apparaît dans CRÉER UNE "
                                  "ENTRÉE (Create an Entrance) sous le nom que vous lui donnez, pour n'importe quel "
                                  "catcheur, comme son entrée ou son entrée de champion. Les entrées avec ceinture : "
                                  "cherchez « champ »."
                                : "Add an entrance of your other games, without a superstar: it shows in CREATE AN "
                                  "ENTRANCE under the name you give it, for any superstar, as his entrance or his "
                                  "champion's. The belt entrances: search \"champ\".");
    ImGui::Dummy(ImVec2(0, 4));
    const float h = std::max(300.0f, ImGui::GetContentRegionAvail().y - 150);
    // 1. the game
    ImGui::BeginChild("ea_games", ImVec2(330, h), ImGuiChildFlags_Borders);
    ImGui::TextDisabled("%s", fr ? "1. Le jeu" : "1. The game");
    game_icons::DrawList(
        fr, device_, discs, icons_, game_,
        [](const game_discs::Known &k) {
          return k.title != 0x54510890 && (star_port::Scenes(k.title) || star_port::Svr(k.title));
        },
        [](uint32_t t) { return star_port::GameName(t); }, [fr](uint32_t) { return fr ? "ses entrées" : "its entrances"; },
        [&, discs](uint32_t t) { Choose(w12, discs, t); }, [&want_discs]() { want_discs = true; });
    ImGui::EndChild();
    ImGui::SameLine();
    // 2. the entrance
    ImGui::BeginChild("ea_list", ImVec2(0, h), ImGuiChildFlags_Borders);
    ImGui::TextDisabled("%s", fr ? "2. L'entrée" : "2. The entrance");
    if (!game_) {
      ImGui::TextWrapped("%s", fr ? "Choisissez d'abord un jeu." : "Choose a game first.");
    } else if (listing_.valid()) {
      Dots(fr, fr ? "Lecture des entrées de %s" : "Reading the entrances of %s", star_port::GameName(game_));
    } else if (list_.empty()) {
      ImGui::TextColored(red, "%s", fr ? "Aucune entrée trouvée sur ce disque." : "No entrance found on that disc.");
    } else {
      ImGui::SetNextItemWidth(320);
      ImGui::InputTextWithHint("##ea_filter", fr ? "Rechercher (champ, miz, 4012...)" : "Search (champ, miz, 4012...)",
                               filter_, sizeof(filter_));
      ImGui::SameLine();
      ImGui::TextDisabled(fr ? "%zu entrées" : "%zu entrances", list_.size());
      ImGui::BeginChild("ea_items", ImVec2(0, 0));
      const std::string want = Lower(filter_);
      for (const auto &it : list_) {
        char label[160];
        snprintf(label, sizeof(label), "%4d  %s", it.number,
                 it.name.empty() ? (fr ? "(sans nom)" : "(no name)") : it.name.c_str());
        if (!want.empty() && Lower(label).find(want) == std::string::npos)
          continue;
        if (ImGui::Selectable(label, it.number == pick_, porting_.valid() ? ImGuiSelectableFlags_Disabled : 0) &&
            it.number != pick_)
          Pick(w12, *discs, it);
      }
      ImGui::EndChild();
    }
    ImGui::EndChild();
    // 3. its name, the preview, the mod
    if (porting_.valid()) {
      Dots(fr, fr ? "Conversion de l'entrée %s" : "Making the entrance %s WWE '12's", pick_label_.c_str());
    } else if (pick_ > 0 && !done_.why.empty()) {
      ImGui::TextColored(red, fr ? "Cette entrée ne peut pas être ajoutée : %s." : "That entrance can't be added: %s.",
                         done_.why.c_str());
    } else if (pick_ > 0 && done_.number > 0) {
      ImGui::Text(fr ? "3. Son nom dans Create an Entrance (servie sous le n° %d ; essai)"
                     : "3. Its name in Create an Entrance (served as no. %d; a test)",
                  done_.number);
      ImGui::SetNextItemWidth(320);
      ImGui::InputText("##ea_name", name_, sizeof(name_));
      ImGui::SameLine();
      if (ImGui::Button(fr ? "Regarder l'entrée" : "Watch the entrance")) {
        int on = 139;
        if (std::none_of(stars.begin(), stars.end(), [](const auto &s) { return s.id == 139; }) && !stars.empty())
          on = stars.front().id;
        watch_.Open(device_, w12, done_.parts, done_.number,
                    std::string(name_[0] ? name_ : "?") + " (" + star_port::GameName(game_) + ")", on, stars);
      }
      ImGui::SameLine();
      ImGui::BeginDisabled(!name_[0] || game::IsRunning());
      if (ImGui::Button(fr ? "Créer le mod" : "Make the mod", ImVec2(160, 0)))
        Make(fr, mods);
      ImGui::EndDisabled();
      if (message_ok_) {
        ImGui::SameLine();
        ImGui::BeginDisabled(game::IsRunning());
        if (ImGui::Button(fr ? "Tester en jeu" : "Test in game"))
          arena_page::LaunchGame();
        ImGui::EndDisabled();
      }
      if (!done_.objects.empty())
        ImGui::TextDisabled(fr ? "(avec %zu objet(s) que WWE '12 n'a pas)" : "(with %zu object(s) WWE '12 hasn't)",
                            done_.objects.size());
      if (star_port::Svr(game_))
        ImGui::TextDisabled("%s", fr ? "(les accessoires des entrées SvR ne sont pas encore repris : cachés)"
                                     : "(SvR entrances' props aren't brought yet: hidden)");
      ImGui::TextDisabled("%s", fr ? "(la musique et la vidéo se choisissent dans Create an Entrance)"
                                   : "(the theme and the video are chosen in Create an Entrance)");
      if (game::IsRunning())
        ImGui::TextColored(red, "%s", fr ? "Le jeu est lancé : fermez-le pour installer le mod."
                                         : "The game is running: close it to install the mod.");
    } else {
      ImGui::TextDisabled("%s", fr ? "3. Choisissez une entrée dans la liste." : "3. Choose an entrance in the list.");
    }
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
    watch_.Draw(fr);
  }

 private:
  static std::string Lower(std::string s) {
    for (auto &c : s)
      c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
  }
  static void Dots(bool fr, const char *fmt, const char *what) {
    (void)fr;
    static const char *kDots[] = {"", ".", "..", "..."};
    char line[200];
    snprintf(line, sizeof(line), fmt, what);
    ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s%s", line, kDots[int(ImGui::GetTime() * 3) % 4]);
  }

  void Choose(const disc::GameDisc &w12, const game_discs::Discs *discs, uint32_t title) {
    if (title == game_ || !discs || listing_.valid() || porting_.valid())
      return;
    game_ = title;
    list_.clear();
    pick_ = 0;
    done_ = {};
    filter_[0] = 0;
    message_.clear();
    message_ok_ = false;
    const disc::GameDisc *w = &w12;
    listing_ = std::async(std::launch::async, [w, discs, title]() {
      disc::GameDisc src;
      if (discs->PathOf(title).empty() || !src.OpenPath(discs->PathOf(title)))
        return std::vector<Item>{};
      return ListOf(*discs, *w, title, src);
    });
  }

  void Pick(const disc::GameDisc &w12, const game_discs::Discs &discs, const Item &it) {
    pick_ = it.number;
    pick_label_ = std::to_string(it.number) + (it.name.empty() ? "" : " " + it.name);
    done_ = {};
    message_.clear();
    message_ok_ = false;
    // (its name in the lists: the script's - "The_Miz_Champ" -, the superstar's for SvR, else its number)
    std::string nm = it.name.substr(0, it.name.find(','));
    for (auto &c : nm)
      if (c == '_')
        c = ' ';
    while (!nm.empty() && nm.back() == ' ')
      nm.pop_back();
    while (!nm.empty() && nm.front() == ' ')
      nm.erase(nm.begin());
    // (with its game: "Sandman (SvR 2008)" - the player picks which game's entrance a superstar comes with, the user
    // 2026-11-12)
    if (nm.empty())
      nm = std::string(star_port::GameName(game_)) + " " + std::to_string(it.number);
    else
      nm += std::string(" (") + star_port::GameName(game_) + ")";
    snprintf(name_, sizeof(name_), "%s", nm.c_str());
    const disc::GameDisc *w = &w12;
    const game_discs::Discs *d = &discs;
    const uint32_t game = game_;
    const int from = it.number;
    star_port::Music12 *music = &music_;
    porting_ = std::async(std::launch::async, [w, d, game, from, music]() { return Port(*w, *d, game, from, *music); });
  }

  void Poll() {
    if (listing_.valid() && listing_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
      list_ = listing_.get();
    if (porting_.valid() && porting_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
      done_ = porting_.get();
  }

  // The mod: chosen where to save, written (manifest.txt, superstar/entrance/, superstar/objects/), installed.
  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    message_.clear();
    std::wstring file;
    std::string id = "entrance_";
    for (unsigned char c : std::string(name_)) {
      id += std::isalnum(c) ? char(std::tolower(c)) : '_';
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    }
    const fs::path save = media::PickSave(fr, (file.empty() ? L"entrance" : file) + L"_entrance.svrmod");
    if (save.empty())
      return;
    const std::string from = std::string(star_port::GameName(game_)) + "'s entrance " + std::to_string(pick_);
    std::string manifest = "game=wwe12\ntype=entrance\nid=" + id + "\nname=" + std::string(name_) +
                           "\nversion=" + mod_version::Of("") + "\nentrance_own=" + std::to_string(done_.number) +
                           "\ndescription=" + std::string(name_) + ": " + from +
                           " (the player's own copy), for Create an Entrance, made with the WWE '12 Mod Maker.\n";
    std::vector<std::pair<std::string, Bytes>> entries = {{"manifest.txt", Bytes(manifest.begin(), manifest.end())}};
    for (const auto &part : done_.parts)
      entries.push_back({entrance13::ModPath(part), part.data});
    for (const auto &part : done_.objects)
      entries.push_back({entrance13::ModPath(part), part.data});
    if (!mods::Manager::WriteZip(save, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    message_ok_ = true;
    message_ = (fr ? "Mod créé et installé : " : "Mod made and installed: ") + media::Utf8(save.wstring()) +
               (fr ? " (au prochain lancement du jeu, dans Create an Entrance)"
                   : " (next time the game starts, in Create an Entrance)");
  }

  ID3D11Device *device_ = nullptr;
  int test_step_ = 0;  // (WWE12_MM_ADDENT)
  bool kids_done_ = false;  // (WWE12_MM_KIDS)
  game_icons::Icons icons_;
  uint32_t game_ = 0;
  std::vector<Item> list_;
  std::future<std::vector<Item>> listing_;
  char filter_[64] = {};
  int pick_ = 0;
  std::string pick_label_;
  std::future<Ported> porting_;
  Ported done_;
  star_port::Music12 music_;
  char name_[40] = {};
  std::string message_;
  bool message_ok_ = false;
  entrance_preview::Window watch_;
};

}  // namespace entrance_add
