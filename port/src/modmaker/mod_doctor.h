// CHECK MODS page: the mod doctor, after the one in Kaiko's SvR 2011 Mod Maker (mod_check.cpp; credit in
// docs/mod_maker.md). It reads every installed mod (mods\.svrmods\<Kind>\<id>) and says in plain words what will go
// wrong in the game and what to do, with a button when the Mod Maker can do it. Each rule is something the game or
// the launcher really does (the file it comes from in brackets):
//  - added superstars (src/native/wwe12_superstar_mods.cpp, superstar_pack.h): a base the game has, a model that
//    reads, at most 20 bones a mesh (the game's palette), no broken numbers (NaN: the game crashes), not much bigger
//    than a game attire (the room it loads into), an entrance video in Bink 1 (WWE '12 doesn't play Bink 2), portraits
//    of the game's sizes; the numbers kept for mods no longer installed (Documents\wwe12\superstar_slots.txt);
//  - added arenas (src/native/wwe12_arena_pages.cpp): a base 0-22, an arena file (its STG record at 0x800) that fits
//    the room an added arena gets (12.5 MB), a 256 x 128 banner, VS screen / entrance graphics as packs, a Bink 1
//    transition; at most 35 (the banner pack's free places);
//  - replacing files (mods.h): a file the disc has (else the game never reads it), an arena no bigger than the one it
//    replaces, Bink 1 videos; two mods giving the same file (the first one wins).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "game_disc.h"
#include "imgui.h"
#include "mods.h"
#include "svrfmt/archive.h"
#include "svrfmt/jboy.h"
#include "svrfmt/texture.h"

namespace mod_doctor {

namespace fs = std::filesystem;
using svrfmt::Bytes;

struct Problem {
  int level = 1;  // 2: it will go wrong in the game, 1: maybe, 0: good to know
  std::string mod, text;
  std::string fix_label;          // "" = nothing the Mod Maker can do
  std::function<bool()> fix;      // true when done
};

class Page {
 public:
  void Draw(bool fr, const disc::GameDisc &disc, mods::Manager &mods, const fs::path &mods_dir,
            const fs::path &user_root) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), yellow(0.94f, 0.75f, 0.24f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f),
        grey(0.65f, 0.65f, 0.7f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Le docteur des mods lit chaque mod installé et dit en clair ce qui ira mal dans le jeu, "
                                  "avec un bouton quand le Mod Maker peut le réparer."
                                : "The mod doctor reads every installed mod and says in plain words what will go wrong "
                                  "in the game, with a button when the Mod Maker can fix it.");
    if (ImGui::Button(fr ? "Vérifier les mods" : "Check the mods", ImVec2(200, 0)) || !checked_) {
      Check(fr, disc, mods_dir, user_root);
      checked_ = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled(fr ? "%d mod(s) lu(s)" : "%d mod(s) read", read_);
    if (!message_.empty())
      ImGui::TextColored(green, "%s", message_.c_str());
    ImGui::Separator();
    if (problems_.empty()) {
      ImGui::TextColored(green, "%s", fr ? "Rien à signaler : vos mods sont bons." : "Nothing to report: your mods are fine.");
      return;
    }
    ImGui::BeginChild("doctor_list", ImVec2(0, 0));
    std::string last;
    for (size_t i = 0; i < problems_.size(); ++i) {
      Problem &p = problems_[i];
      if (p.mod != last) {
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextUnformatted(p.mod.c_str());
        last = p.mod;
      }
      ImGui::PushID(int(i));
      ImGui::Bullet();
      ImGui::SameLine();
      ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - (p.fix ? 200 : 0) + ImGui::GetCursorPosX());
      ImGui::TextColored(p.level == 2 ? red : p.level == 1 ? yellow : grey, "%s", p.text.c_str());
      ImGui::PopTextWrapPos();
      if (p.fix) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 190 + ImGui::GetCursorPosX());
        ImGui::BeginDisabled(game::IsRunning());
        if (ImGui::SmallButton(p.fix_label.c_str())) {
          const bool ok = p.fix();
          message_ = ok ? (fr ? "Fait : " : "Done: ") + p.fix_label : (fr ? "Échec : " : "Failed: ") + p.fix_label;
          mods.Init(mods_dir);  // (the launcher's view, files put in place again)
          Check(fr, disc, mods_dir, user_root);
          ImGui::EndDisabled();
          ImGui::PopID();
          break;
        }
        ImGui::EndDisabled();
      }
      ImGui::PopID();
    }
    ImGui::EndChild();
  }

  // Every rule, over every installed mod.
  void Check(bool fr, const disc::GameDisc &disc, const fs::path &mods_dir, const fs::path &user_root) {
    problems_.clear();
    read_ = 0;
    std::error_code ec;
    const fs::path root = mods_dir / ".svrmods";
    std::map<std::string, std::string> owner;  // a file given to the game -> the first mod (by folder) giving it
    std::set<std::string> superstar_ids;
    int added_arenas = 0;
    std::vector<fs::path> dirs;
    for (fs::directory_iterator k(root, ec), end; !ec && k != end; k.increment(ec))
      if (k->is_directory(ec) && k->path().filename() != "." )
        for (fs::directory_iterator m(k->path(), ec), e2; !ec && m != e2; m.increment(ec))
          if (m->is_directory(ec))
            dirs.push_back(m->path());
    ec.clear();
    std::sort(dirs.begin(), dirs.end());
    for (const auto &dir : dirs) {
      const std::string text = ReadText(dir / "manifest.txt");
      const std::string id = dir.filename().string();
      const std::string label = Value(text, "name").empty() ? id : Value(text, "name") + "  (" + id + ")";
      ++read_;
      auto add = [&](int level, const std::string &t, const std::string &fix_label = "", std::function<bool()> fix = {}) {
        problems_.push_back({level, label, t, fix_label, std::move(fix)});
      };
      auto disable = [&](void) -> std::function<bool()> {
        return [dir] {
          std::ofstream(dir / "disabled").put('\n');
          return true;
        };
      };
      const char *off = fr ? "Désactiver ce mod" : "Turn this mod off";
      if (text.empty()) {
        add(2, fr ? "Pas de manifest.txt : ce n'est pas un mod (le launcher ne le voit pas)."
                  : "No manifest.txt: not a mod (the launcher doesn't see it).");
        continue;
      }
      if (Value(text, "game") != "wwe12")
        add(2, fr ? "Mod d'un autre jeu (game=wwe12 manque) : WWE '12 ne le lira pas comme il faut."
                  : "A mod for another game (no game=wwe12): WWE '12 won't read it right.", off, disable());
      const bool on = !fs::exists(dir / "disabled", ec);
      if (!on) {
        add(0, fr ? "Désactivé : le jeu ne le voit pas." : "Turned off: the game doesn't see it.",
            fr ? "Le réactiver" : "Turn it on", [dir] {
              std::error_code e;
              return fs::remove(dir / "disabled", e);
            });
        continue;
      }
      const std::string type = Value(text, "type");
      if (type == "superstar") {
        superstar_ids.insert(Value(text, "id").empty() ? id : Value(text, "id"));
        CheckSuperstar(fr, disc, dir, text, add, off, disable());
      } else if (type == "arena" && Value(text, "slot") == "new") {
        ++added_arenas;
        CheckAddedArena(fr, dir, text, add, off, disable());
      }
      // Files given to the game (any type).
      const fs::path files = dir / "files";
      for (fs::recursive_directory_iterator it(files, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec))
          continue;
        std::string rel = fs::relative(it->path(), files, ec).generic_string();
        std::string key = rel;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (auto o = owner.find(key); o != owner.end()) {
          add(1, (fr ? "Donne " : "Gives ") + rel + (fr ? ", que le mod " : ", which the mod ") + o->second +
                     (fr ? " donne déjà : le sien est utilisé." : " already gives: that one is used."));
          continue;
        }
        owner.emplace(key, label);
        Bytes orig;
        const bool on_disc = disc.Found() && disc.Read(rel, orig);
        if (disc.Found() && !on_disc && rel.rfind("movies/titantron/", 0) != 0)
          add(1, (fr ? "Le jeu n'a pas de fichier " : "The game has no file ") + rel +
                     (fr ? " : il ne le lira jamais." : ": it will never read it."));
        const uintmax_t size = it->file_size(ec);
        if (rel.size() > 4 && rel.substr(rel.size() - 4) == ".bik" && !IsBink1(it->path()))
          add(2, rel + (fr ? " n'est pas une vidéo Bink 1 : WWE '12 ne la joue pas (écran noir ou plantage). Refaites-la "
                             "avec la page VIDÉOS D'ENTRÉE."
                           : " isn't a Bink 1 video: WWE '12 doesn't play it (black screen or crash). Make it again "
                             "on the ENTRANCE VIDEOS page."),
              off, disable());
        if (on_disc && rel.rfind("pac/bg/bg", 0) == 0 && size > orig.size())
          add(2, rel + (fr ? " est plus gros que l'arène qu'il remplace : le jeu manque de place et plante au chargement "
                             "(réduisez ses textures sur la page ARÈNES)."
                           : " is bigger than the arena it replaces: the game runs out of room and crashes when it "
                             "loads (shrink its textures on the ARENAS page)."),
              off, disable());
      }
      ec.clear();
    }
    if (added_arenas > 35)
      problems_.push_back({1, fr ? "Arènes ajoutées" : "Added arenas",
                           fr ? "Plus de 35 arènes ajoutées : les suivantes n'ont pas de bannière."
                              : "Over 35 added arenas: the next ones have no banner.",
                           "", {}});
    CheckSlots(fr, user_root, superstar_ids);
    std::stable_sort(problems_.begin(), problems_.end(),
                     [](const Problem &a, const Problem &b) { return a.mod < b.mod; });
  }

  const std::vector<Problem> &Problems() const { return problems_; }

 private:
  using Add = std::function<void(int, const std::string &, const std::string &, std::function<bool()>)>;

  static std::string ReadText(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  }
  static Bytes ReadBytes(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  }
  static std::string Value(const std::string &text, const char *key) {
    const std::string k = std::string(key) + "=";
    size_t pos = 0;
    while (pos < text.size()) {
      size_t end = text.find('\n', pos);
      if (end == std::string::npos)
        end = text.size();
      std::string line = text.substr(pos, end - pos);
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.rfind(k, 0) == 0)
        return line.substr(k.size());
      pos = end + 1;
    }
    return "";
  }
  // Bink 1 ("BIK" + a version letter); WWE '13 and the 2K games' are Bink 2 ("KB2").
  static bool IsBink1(const fs::path &p) {
    char sig[3] = {};
    std::ifstream(p, std::ios::binary).read(sig, 3);
    return !memcmp(sig, "BIK", 3);
  }

  // An added superstar: its base, its model, its video and portraits.
  void CheckSuperstar(bool fr, const disc::GameDisc &disc, const fs::path &dir, const std::string &text, const Add &add,
                      const char *off, std::function<bool()> disable) {
    const int base = atoi(Value(text, "base").c_str());
    char base_file[32];
    snprintf(base_file, sizeof(base_file), "pac/ch/ch%03d.pac", base);
    Bytes base_pac;
    if (Value(text, "name").empty())
      add(1, fr ? "Pas de nom : il s'appellera comme son dossier." : "No name: it will be called as its folder.", "", {});
    if (disc.Found() && (base <= 0 || !disc.Read(base_file, base_pac)))
      add(2, (fr ? "Sa base (" : "Its base (") + std::to_string(base) +
                 (fr ? ") n'est pas un catcheur du jeu : il ne sera pas ajouté." : ") isn't one of the game's superstars: it won't be added."),
          off, disable);
    const fs::path s = dir / "superstar";
    std::error_code ec;
    if (!fs::exists(s / "ch.pac", ec) && !fs::exists(s / "look.txt", ec)) {
      add(2, fr ? "Pas de modèle (superstar\\ch.pac) : il ne sera pas ajouté." : "No model (superstar\\ch.pac): it won't be added.",
          off, disable);
      return;
    }
    if (fs::exists(s / "ch.pac", ec)) {
      const Bytes pac = ReadBytes(s / "ch.pac");
      svrfmt::Archive a;
      std::string why;
      if (!svrfmt::ArchiveOpen(pac, a, &why) || a.root.kids.empty()) {
        add(2, (fr ? "Son modèle ne se lit pas (" : "Its model can't be read (") + why +
                   (fr ? ") : écran noir ou plantage au match." : "): black screen or crash at the match."),
            off, disable);
      } else {
        svrfmt::Archive b;
        const bool have_base = !base_pac.empty() && svrfmt::ArchiveOpen(base_pac, b, nullptr) && !b.root.kids.empty();
        for (auto &att : a.root.kids) {
          if (att.group != "EMD ")
            continue;
          if (have_base && att.stored.size() > b.root.kids[0].stored.size() * 3 / 2)
            add(1, (fr ? "Tenue " : "Attire ") + att.name +
                       (fr ? " : bien plus grosse qu'une tenue du jeu, le jeu peut manquer de place en match à 4 ou 6."
                           : ": much bigger than a game attire, the game may run out of room in 4- or 6-man matches."),
                "", {});
          if (!svrfmt::ArchiveExpand(att))
            continue;
          for (auto &k : att.kids) {
            if (k.id != 0 && k.id != 10000 && k.id != 20000)
              continue;
            svrfmt::Model m;
            if (!svrfmt::JboyRead(svrfmt::ArchiveData(k), m, nullptr)) {
              add(2, (fr ? "Tenue " : "Attire ") + att.name + (fr ? " : un modèle ne se lit pas (plantage au match)."
                                                              : ": a model can't be read (crash at the match)."),
                  off, disable);
              continue;
            }
            size_t verts = 0, nan = 0, palette = 0;
            for (const auto &mesh : m.meshes) {
              verts += mesh.verts.size();
              palette = std::max(palette, mesh.palette.size());
              for (const auto &v : mesh.verts)
                for (float f : v.pos)
                  nan += !std::isfinite(f);
            }
            if (nan)
              add(2, (fr ? "Tenue " : "Attire ") + att.name + (fr ? " : des sommets abîmés (NaN), le jeu plante."
                                                              : ": broken vertices (NaN), the game crashes."),
                  off, disable);
            if (palette > 20)
              add(2, (fr ? "Tenue " : "Attire ") + att.name + (fr ? " : plus de 20 os sur un maillage, le jeu ne peut pas l'animer."
                                                              : ": over 20 bones on a mesh, the game can't animate it."),
                  off, disable);
            if (k.id == 0 && verts > 30000)
              add(1, (fr ? "Tenue " : "Attire ") + att.name + ": " + std::to_string(verts) +
                         (fr ? " sommets (un catcheur du jeu : ~9 000), lent ou trop gros en match à plusieurs."
                             : " vertices (a game superstar: ~9,000), slow or too big in multi-man matches."),
                  "", {});
          }
        }
      }
    }
    if (fs::exists(s / "titantron.bik", ec) && !IsBink1(s / "titantron.bik"))
      add(2, fr ? "Sa vidéo d'entrée n'est pas en Bink 1 : écran noir pendant l'entrée."
                : "Its entrance video isn't Bink 1: a black screen during the entrance.",
          fr ? "Retirer la vidéo" : "Remove the video", [s] {
            std::error_code e;
            return fs::remove(s / "titantron.bik", e);
          });
    // Portraits: the game's sizes (HD 512 x 512, 512 x 512, 256 x 256; SD 512 x 256, 512 x 256, 256 x 256).
    const int want[2][3][2] = {{{512, 256}, {512, 256}, {256, 256}}, {{512, 512}, {512, 512}, {256, 256}}};
    for (int hd = 0; hd < 2; ++hd)
      for (int k = 0; k < 3; ++k) {
        const fs::path p = s / (std::string("SSF") + char('A' + k) + (hd ? "_HD.dds" : "_SD.dds"));
        if (!fs::exists(p, ec))
          continue;
        svrfmt::DdsInfo i;
        if (!svrfmt::DdsInfoOf(ReadBytes(p), i) || i.w != want[hd][k][0] || i.h != want[hd][k][1])
          add(1, p.filename().string() + (fr ? " : pas à la taille du jeu, le portrait de la base est pris."
                                             : ": not the game's size, the base's portrait is used."),
              fr ? "Retirer ce portrait" : "Remove this portrait", [p] {
                std::error_code e;
                return fs::remove(p, e);
              });
      }
  }

  // An added arena: its base, its file, its pictures and videos.
  void CheckAddedArena(bool fr, const fs::path &dir, const std::string &text, const Add &add, const char *off,
                       std::function<bool()> disable) {
    const std::string base_s = Value(text, "base");
    const int base = atoi(base_s.c_str());
    if (base_s.empty() || base < 0 || base > 22)
      add(2, fr ? "Sa base n'est pas une arène du jeu (0 à 22) : elle ne sera pas ajoutée."
                : "Its base isn't one of the game's arenas (0 to 22): it won't be added.",
          off, disable);
    const fs::path a = dir / "arena";
    std::error_code ec;
    const Bytes pac = ReadBytes(a / "arena.pac");
    if (pac.size() < 0x818 || memcmp(&pac[0x800], "STG ", 4) != 0) {
      add(2, fr ? "Pas de fichier d'arène lisible (arena\\arena.pac) : elle ne sera pas ajoutée."
                : "No readable arena file (arena\\arena.pac): it won't be added.",
          off, disable);
      return;
    }
    if (pac.size() > size_t(12.5 * 1024 * 1024))
      add(2, fr ? "Plus de 12,5 Mo : le jeu manque de place et plante au chargement (refaites-la sur la page ARÈNES, "
                  "elle réduit les textures)."
                : "Over 12.5 MB: the game runs out of room and crashes when it loads (make it again on the ARENAS "
                  "page, it shrinks the textures).",
          off, disable);
    const uintmax_t banner = fs::exists(a / "banner.dds", ec) ? fs::file_size(a / "banner.dds", ec) : 0;
    if (banner != 32896)
      add(1, fr ? "Pas de bannière 256 x 128 : sa case montre une image vide." : "No 256 x 128 banner: its tile shows a blank picture.",
          "", {});
    for (const char *f : {"card_mb.pac", "card_mf.pac", "etex_hd.pac", "load_hd.pac"}) {
      if (!fs::exists(a / f, ec))
        continue;
      const Bytes b = ReadBytes(a / f);
      if (b.size() < 8 || memcmp(b.data(), "PACH", 4) != 0)
        add(1, std::string(f) + (fr ? " n'est pas un pack : celui de la base est pris." : " isn't a pack: the base's is used."),
            "", {});
    }
    if (fs::exists(a / "transition.bik", ec) && !IsBink1(a / "transition.bik"))
      add(1, fr ? "Sa vidéo de transition n'est pas en Bink 1 : celle de la base est jouée."
                : "Its transition video isn't Bink 1: the base's is played.",
          "", {});
  }

  // Documents\wwe12\superstar_slots.txt: numbers kept for mods no longer installed; one number given twice.
  void CheckSlots(bool fr, const fs::path &user_root, const std::set<std::string> &installed) {
    const fs::path f = user_root / "superstar_slots.txt";
    const std::string text = ReadText(f);
    if (text.empty())
      return;
    std::vector<std::pair<std::string, int>> lines;
    size_t pos = 0;
    while (pos < text.size()) {
      size_t end = text.find('\n', pos);
      if (end == std::string::npos)
        end = text.size();
      std::string l = text.substr(pos, end - pos);
      if (!l.empty() && l.back() == '\r')
        l.pop_back();
      if (const size_t eq = l.find('='); eq != std::string::npos)
        lines.push_back({l.substr(0, eq), atoi(l.c_str() + eq + 1)});
      pos = end + 1;
    }
    std::vector<std::string> stale;
    std::map<int, int> used;
    for (const auto &[k, n] : lines) {
      ++used[n];
      if (!installed.count(k))
        stale.push_back(k);
    }
    if (stale.empty())
      return;
    std::string list;
    for (const auto &s : stale)
      list += (list.empty() ? "" : ", ") + s;
    bool twice = false;
    for (const auto &[n, c] : used)
      twice |= c > 1;
    problems_.push_back(
        {twice ? 1 : 0, fr ? "Numéros des catcheurs ajoutés" : "Added superstars' numbers",
         (fr ? "Numéros gardés pour des mods qui ne sont plus installés (" : "Numbers kept for mods no longer installed (") +
             list +
             (fr ? ")" + std::string(twice ? " ; certains servent aussi à un autre mod : s'ils reviennent, ils changeront de place." : ".")
                 : ")" + std::string(twice ? "; some are another mod's too: if they come back, they'll move." : ".")),
         fr ? "Les oublier" : "Forget them", [f, lines, installed] {
           std::string out;
           for (const auto &[k, n] : lines)
             if (installed.count(k))
               out += k + "=" + std::to_string(n) + "\n";
           std::ofstream(f, std::ios::binary).write(out.data(), std::streamsize(out.size()));
           return true;
         }});
  }

  std::vector<Problem> problems_;
  int read_ = 0;
  bool checked_ = false;
  std::string message_;
};

}  // namespace mod_doctor
