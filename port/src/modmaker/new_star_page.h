// Mod Maker, "New superstar" page: a superstar added to the game, next to the others (docs/mod_maker.md, "Ajouter
// des catcheurs"). The mod (.svrmod, type=superstar) holds only its name, its base superstar (whose record, moves
// and portraits it starts from) and, if it has its own look, superstar/ch.pac; the game makes a DLC pack of the
// superstar mods at startup (src/native/wwe12_superstar_mods.cpp, modmaker/superstar_pack.h). Its look: the base's,
// another superstar file (a chNNN.pac), or an attire of SvR 2011 or WWE '13 ported over the base's first attire
// (port_svr.h, port13_star.h).
// Its entrance video (titantron and minitron, superstar/titantron.bik) can be its own, made from the base's: the
// game plays it during its entrance in place of the base's.
// The base is optional: without one, an average superstar of the disc gives the moves, ratings, entrance and
// portraits (kDefaultBases), and the look must then be the mod's own.
#pragma once

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <iterator>
#include <string>
#include <vector>

#include "arena_edit.h"
#include "arena_page.h"
#include "arena_view.h"
#include "game_disc.h"
#include "game_discs.h"
#include "game_process.h"
#include "imgui.h"
#include "media.h"
#include "mods.h"
#include "port13_star.h"
#include "port_svr.h"

namespace new_star_page {

namespace fs = std::filesystem;
using svrfmt::Bytes;

// The superstar a new one starts from when no base is chosen: average ratings, an ordinary entrance (the first of
// these the disc has).
constexpr int kDefaultBases[] = {261 /* Ted DiBiase */, 327 /* Justin Gabriel */, 175 /* John Morrison */};

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &disc, mods::Manager &mods) {
    disc12_ = &disc;
    device_ = device;
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f), yellow(0.94f, 0.75f, 0.24f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Ajoutez un catcheur au jeu, à côté des autres (il apparaît dans la case DLC de "
                                  "l'écran de choix). Il peut partir d'un catcheur de base (ses prises, ses stats, ses "
                                  "portraits), sinon d'un catcheur moyen ; son apparence peut être celle de la base, un "
                                  "autre fichier de catcheur, ou une tenue portée de SvR 2011, de WWE '13 ou d'un 2K."
                                : "Add a superstar to the game, next to the others (shown in the select screen's DLC "
                                  "tile). It can start from a base superstar (moves, ratings and portraits), else from an "
                                  "average one; its look can be the base's, another superstar file, or an attire ported "
                                  "from SvR 2011, WWE '13 or a 2K game.");
    if (movie_.Finished())
      Finished(fr, mods);
    if (!disc.Found()) {
      ImGui::TextColored(red, "%s", fr ? "Fichiers du jeu introuvables." : "Game files not found.");
      return;
    }
    if (!listed_) {
      List(disc);
      if (default_ >= 0) {
        base_ = default_;
        // (its look from another game when My game discs has one: WWE '13 and 2K first, else SvR 2011)
        look_ = discs_ && !discs_->PathOf(0x545108B4).empty()   ? 3
                : discs_ && !discs_->PathOf(0x5451085D).empty() ? 2
                                                                : 1;
        OpenBase(fr, disc);
      }
    }

    // The assistant: where its look comes from, its base, its name, its entrance video, a look at it, the mod.
    const char *steps_fr[] = {"1. Apparence", "2. Catcheur de base", "3. Nom", "4. Vidéo d'entrée", "5. Aperçu",
                              "6. Terminer"};
    const char *steps_en[] = {"1. Look", "2. Base superstar", "3. Name", "4. Entrance video", "5. Look at it",
                              "6. Finish"};
    const bool look_ready = !model_.empty() && (look_ == 0 ? chosen_ : model_ != base_file_);
    const int reach = base_ < 0 ? 0 : look_ready ? 5 : 1;
    step_ = std::min(step_, reach);
    const float h = std::max(320.0f, ImGui::GetContentRegionAvail().y - 72);
    ImGui::BeginChild("ns_steps", ImVec2(190, h), ImGuiChildFlags_Borders);
    for (int i = 0; i < 6; ++i) {
      ImGui::BeginDisabled(i > reach);
      if (ImGui::Selectable(fr ? steps_fr[i] : steps_en[i], step_ == i, 0, ImVec2(0, 26)))
        step_ = i;
      ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("ns_body", ImVec2(0, h), ImGuiChildFlags_Borders);
    ImGui::PushTextWrapPos(0);
    switch (step_) {
    case 0: StepLook(fr); break;
    case 1: StepBase(fr, disc); break;
    case 2:
      ImGui::SetNextItemWidth(320);
      ImGui::InputTextWithHint(fr ? "Nom" : "Name", fr ? "(comme en jeu)" : "(as in the game)", name_, sizeof(name_));
      ImGui::SetNextItemWidth(320);
      ImGui::InputTextWithHint(fr ? "Auteur" : "Author", fr ? "(facultatif)" : "(optional)", author_, sizeof(author_));
      break;
    case 3: StepVideo(fr); break;
    case 4: {
      ImGui::BeginChild("ns_view", ImVec2(0, 0), ImGuiChildFlags_Borders);
      Draw3D(fr);
      ImGui::EndChild();
      break;
    }
    default: StepFinish(fr, mods); break;
    }
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::BeginDisabled(step_ == 0);
    if (ImGui::Button(fr ? "Retour" : "Back", ImVec2(110, 0)))
      --step_;
    ImGui::EndDisabled();
    ImGui::SameLine();
    char prog[32];
    snprintf(prog, sizeof(prog), fr ? "Étape %d sur 6" : "Step %d of 6", step_ + 1);
    ImGui::ProgressBar((step_ + 1) / 6.0f, ImVec2(ImGui::GetContentRegionAvail().x - 120, 0), prog);
    ImGui::SameLine();
    ImGui::BeginDisabled(step_ >= 5 || step_ + 1 > reach);
    if (ImGui::Button(fr ? "Suivant" : "Next", ImVec2(110, 0)))
      ++step_;
    ImGui::EndDisabled();
    // What keeps it from being done.
    if (!look_ready)
      ImGui::TextColored(red, "%s", fr ? "Choisissez son apparence (étape 1)." : "Choose its look (step 1).");
    else if (!name_[0])
      ImGui::TextColored(red, "%s", fr ? "Donnez-lui un nom (étape 3)." : "Give it a name (step 3).");
    else if (own_movie_ && !movie_.Ready())
      ImGui::TextColored(yellow, "%s", fr ? "Sa vidéo d'entrée n'est pas prête (étape 4)." : "Its entrance video isn't ready (step 4).");
    else
      ImGui::TextColored(green, "%s", fr ? "Tout est prêt." : "All set.");
  }

  // Step 1: its look.
  void StepLook(bool fr) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "D'où vient son apparence : un catcheur de vos autres jeux (MES DISQUES), un fichier "
                                  "de catcheur, ou la base elle-même."
                                : "Where its look comes from: a superstar of your other games (MY GAME DISCS), a "
                                  "superstar file, or the base itself.");
    int look = look_;
    ImGui::BeginDisabled(!chosen_);  // (without a base, the base's look would be the average superstar's)
    ImGui::RadioButton(fr ? "La base" : "The base", &look, 0);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::RadioButton(fr ? "Un fichier (.pac)" : "A file (.pac)", &look, 1);
    ImGui::SameLine();
    ImGui::RadioButton("SvR 2011", &look, 2);
    ImGui::SameLine();
    ImGui::RadioButton(fr ? "WWE '13 ou 2K" : "WWE '13 or 2K", &look, 3);
    if (look != look_) {
      if ((look >= 2) != (look_ >= 2) || look >= 2) {  // (another game: its file chosen again)
        svr_file_.clear();
        svr_path_.clear();
        svr_attires_.clear();
        ported_.clear();
      }
      look_ = look;
      note_.clear();
      if (look_ == 0)
        SetModel(base_file_);
      else
        SetModel(look_ == 1 ? other_ : ported_);
    }
    if (look_ == 1) {
      if (ImGui::Button(fr ? "Choisir le fichier (chNNN.pac)..." : "Choose the file (chNNN.pac)...")) {
        const fs::path p = arena_page::PickOpen(fr, true);
        if (!p.empty())
          LoadOther(fr, p);
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", other_path_.empty() ? "" : media::Utf8(other_path_.wstring()).c_str());
    } else if (look_ >= 2) {
      const bool w13 = look_ == 3;
      // WWE '13 and the 2K games (2K14 to 2K17: the same superstar files, port13_star.h): which one.
      if (w13) {
        static constexpr std::pair<uint32_t, const char *> kGames[] = {{0x545108B4, "WWE '13"}, {0x545408B2, "WWE 2K14"},
                                                                       {0x545408B6, "WWE 2K15"}, {0x315A07D2, "WWE 2K16"},
                                                                       {0x545408BA, "WWE 2K17"}};
        for (int i = 0; i < 5; ++i) {
          const bool have = discs_ && !discs_->PathOf(kGames[i].first).empty();
          if (i)
            ImGui::SameLine();
          ImGui::BeginDisabled(!have && i);
          if (ImGui::RadioButton(kGames[i].second, star_game_ == kGames[i].first) && star_game_ != kGames[i].first) {
            star_game_ = kGames[i].first;
            svr_file_.clear();
            svr_path_.clear();
            svr_attires_.clear();
            ported_.clear();
            SetModel(ported_);
          }
          ImGui::EndDisabled();
        }
      }
      // From My game discs: the game's superstars by name.
      const uint32_t title = w13 ? star_game_ : 0x5451085D;
      if (discs_ && game_title_ != title)
        ListDisc(title);
      if (!disc_list_.empty()) {
        std::string current = fr ? "Choisir..." : "Choose...";
        for (const auto &[n, nm] : disc_list_)
          if (n == disc_pick_)
            current = nm;
        ImGui::SetNextItemWidth(320);
        if (ImGui::BeginCombo(w13 ? (fr ? "Catcheur##other" : "Superstar##other")
                                  : (fr ? "Catcheur de SvR 2011" : "SvR 2011 superstar"),
                              current.c_str(), ImGuiComboFlags_HeightLarge)) {
          for (const auto &[n, nm] : disc_list_) {
            char label[96];
            if (nm.size() == 5 && nm.compare(0, 2, "ch") == 0)
              snprintf(label, sizeof(label), "%s", nm.c_str());
            else
              snprintf(label, sizeof(label), "%s  (ch%03d)", nm.c_str(), n);
            if (ImGui::Selectable(label, n == disc_pick_)) {
              LoadFromDisc(fr, n);
              if (!svr_attires_.empty())
                PortSvr(fr);
              const bool guessed = nm.size() > 2 && nm.compare(nm.size() - 2, 2, " ?") == 0;
              if (!name_[0] && !(nm.size() == 5 && nm.compare(0, 2, "ch") == 0))  // (its name, to start with)
                snprintf(name_, sizeof(name_), "%s", (guessed ? nm.substr(0, nm.size() - 2) : nm).c_str());
            }
          }
          ImGui::EndCombo();
        }
        ImGui::TextDisabled("%s", fr ? "(depuis MES DISQUES ; ou un fichier :)" : "(from MY GAME DISCS; or one file:)");
        ImGui::SameLine();
      }
      if (ImGui::Button(w13 ? (fr ? "Fichier de WWE '13 (chNNN.pac)..." : "WWE '13 file (chNNN.pac)...")
                            : (fr ? "Fichier de SvR 2011 (chNNN.pac)..." : "SvR 2011 file (chNNN.pac)..."))) {
        const fs::path p = arena_page::PickOpen(fr, true);
        if (!p.empty())
          LoadSvr(fr, p);
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", !svr_path_.empty() ? media::Utf8(svr_path_.wstring()).c_str()
                                : w13             ? (fr ? "(vos fichiers de WWE '13)" : "(your WWE '13 files)")
                                                  : (fr ? "(vos fichiers de SvR 2011)" : "(your SvR 2011 files)"));
      if (!svr_attires_.empty()) {
        char label[64];
        snprintf(label, sizeof(label), fr ? "Tenue %d  (%s)" : "Attire %d  (%s)", svr_attire_ + 1,
                 svr_attires_[svr_attire_].c_str());
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo(fr ? "Tenue à porter" : "Attire to port", label)) {
          for (int i = 0; i < int(svr_attires_.size()); ++i) {
            snprintf(label, sizeof(label), fr ? "Tenue %d  (%s)" : "Attire %d  (%s)", i + 1, svr_attires_[i].c_str());
            if (ImGui::Selectable(label, i == svr_attire_) && i != svr_attire_) {
              svr_attire_ = i;
              PortSvr(fr);
            }
          }
          ImGui::EndCombo();
        }
      }
    }
    if (!note_.empty()) {
      ImGui::PushTextWrapPos(0);
      ImGui::TextColored(note_ok_ ? green : red, "%s", note_.c_str());
      ImGui::PopTextWrapPos();
    }

  }

  // Step 2: the base superstar (the look ported again over the new base).
  void StepBase(bool fr, const disc::GameDisc &disc) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Le catcheur de base donne ses prises, ses stats, son entrée et ses portraits au "
                                  "nouveau. Sans base, ce sont ceux d'un catcheur moyen."
                                : "The base superstar gives the new one its moves, ratings, entrance and portraits. "
                                  "Without one, an average superstar's.");
    ImGui::SetNextItemWidth(300);
    const char *none = fr ? "Aucune (catcheur moyen)" : "None (an average superstar)";
    if (ImGui::BeginCombo(fr ? "Base (facultatif)" : "Base (optional)",
                          base_ < 0 ? (fr ? "Choisir..." : "Choose...") : chosen_ ? stars_[base_].name.c_str() : none,
                          ImGuiComboFlags_HeightLarge)) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
      ImGui::InputTextWithHint("##ns_filter", fr ? "Rechercher" : "Search", filter_, sizeof(filter_));
      if (default_ >= 0 && !filter_[0] && ImGui::Selectable(none, !chosen_)) {
        chosen_ = false;
        base_ = default_;
        if (look_ == 0)
          look_ = 1;
        ChooseBase(fr, disc);
      }
      for (int i = 0; i < int(stars_.size()); ++i) {
        std::string a = stars_[i].name, b = filter_;
        for (auto *x : {&a, &b})
          std::transform(x->begin(), x->end(), x->begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (!b.empty() && a.find(b) == std::string::npos)
          continue;
        if (ImGui::Selectable(stars_[i].name.c_str(), chosen_ && i == base_)) {
          base_ = i;
          chosen_ = true;
          ChooseBase(fr, disc);
        }
      }
      ImGui::EndCombo();
    }
    if (!chosen_ && base_ >= 0)
      ImGui::TextDisabled("%s", (std::string(fr ? "Sans base, ses prises, ses stats, son entrée et ses portraits sont "
                                                  "ceux de "
                                                : "Without a base, its moves, ratings, entrance and portraits are ") +
                                 stars_[base_].name + (fr ? " ; son apparence est la vôtre." : "; its look is yours."))
                                    .c_str());
    if (!error_.empty())
      ImGui::TextColored(red, "%s", error_.c_str());
  }

  void ChooseBase(bool fr, const disc::GameDisc &disc) {
    OpenBase(fr, disc);
    if (look_ >= 2 && !svr_attires_.empty())
      PortSvr(fr);
  }

  // Step 4: its entrance video.
  void StepVideo(bool fr) {
    ImGui::TextWrapped("%s", fr ? "Le titantron et le minitron pendant son entrée : ceux de la base, ou les siens."
                                : "The titantron and minitron during its entrance: the base's, or its own.");
    ImGui::BeginDisabled(movie_.Busy());
    if (ImGui::RadioButton(fr ? "Celle de la base" : "The base's", !own_movie_ && !svr_movie_))
      own_movie_ = svr_movie_ = false;
    // (SvR 2011's entrance movies are WWE '12's kind, Bink 1 at 320 x 320: taken as they are; WWE '13's and the 2K
    // games' are Bink 2, not read yet)
    if (look_ != 2)
      svr_movie_ = false;  // (only with a look from SvR 2011)
    if (look_ == 2 && !svr_movie_file_.empty()) {
      ImGui::SameLine();
      std::string file = media::Utf8(svr_movie_file_.filename().wstring());
      if (file.rfind("game_", 0) == 0)  // (a copy out of the disc)
        file.erase(0, 5);
      char label[64];
      snprintf(label, sizeof(label), fr ? "La sienne dans SvR 2011 (%s)" : "Its own in SvR 2011 (%s)", file.c_str());
      if (ImGui::RadioButton(label, svr_movie_)) {
        svr_movie_ = true;
        own_movie_ = false;
      }
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(fr ? "Une vidéo à vous" : "One of yours", own_movie_)) {
      own_movie_ = true;
      svr_movie_ = false;
    }
    ImGui::EndDisabled();
    if (look_ == 2 && svr_movie_file_.empty() && !svr_attires_.empty())
      ImGui::TextDisabled("%s", fr ? "(ce catcheur n'a pas de vidéo à lui dans SvR 2011)"
                                   : "(this superstar has no movie of its own in SvR 2011)");
    if (svr_movie_) {
      ImGui::Dummy(ImVec2(0, 6));
      ImGui::TextDisabled("%s", fr ? "Sa vidéo de SvR 2011, titantron et minitron, reprise telle quelle :"
                                   : "Its SvR 2011 movie, titantron and minitron, taken as it is:");
      svr_view_.DrawGame(200);
    }
    if (own_movie_)
      movie_.Draw(fr, "ns_movie");

  }

  // Step 6: the mod.
  void StepFinish(bool fr, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f), yellow(0.94f, 0.75f, 0.24f, 1.0f);
    ImGui::TextWrapped(fr ? "« %s » apparaîtra dans la case DLC de l'écran de choix. Le mod (.svrmod) est enregistré "
                            "où vous voulez, pour le partager, puis installé."
                          : "\"%s\" will show in the select screen's DLC tile. The mod (.svrmod) is saved where you "
                            "want, to share it, then installed.",
                       name_);
    ImGui::Dummy(ImVec2(0, 6));
    const bool ready = name_[0] && !model_.empty() && (look_ == 0 || model_ != base_file_) &&
                       (!own_movie_ || movie_.Ready());
    ImGui::BeginDisabled(!ready || game::IsRunning() || movie_.Busy());
    if (ImGui::Button(fr ? "Créer le mod et l'installer..." : "Make the mod and install it...", ImVec2(260, 36)))
      Make(fr, mods);
    ImGui::EndDisabled();
    movie_.DrawProgress(fr);
    if (!name_[0]) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", fr ? "(donnez-lui un nom)" : "(give it a name)");
    }
    if (game::IsRunning())
      ImGui::TextColored(red, "%s", fr ? "Le jeu est lancé : fermez-le pour installer le mod."
                                       : "The game is running: close it to install the mod.");
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
    if (!reduced_.empty())
      ImGui::TextColored(yellow, "%s", reduced_.c_str());
  }

 private:
  struct Star {
    int id = 0;
    std::string name, file;
  };

  void List(const disc::GameDisc &disc) {
    listed_ = true;
    const auto names = disc::SuperstarNames(disc);
    for (const auto &f : disc.List("pac/ch")) {
      if (f.size() != 9 || _strnicmp(f.c_str(), "ch", 2) || _stricmp(f.c_str() + 5, ".pac") || !isdigit(f[2]))
        continue;
      Star s;
      s.id = atoi(f.c_str() + 2);
      s.file = "pac/ch/" + f;
      const auto it = names.find(s.id);
      if (it == names.end())
        continue;
      s.name = it->second;
      stars_.push_back(s);
    }
    std::sort(stars_.begin(), stars_.end(), [](const Star &a, const Star &b) { return a.name < b.name; });
    for (int id : kDefaultBases)
      for (int i = 0; i < int(stars_.size()) && default_ < 0; ++i)
        if (stars_[size_t(i)].id == id)
          default_ = i;
  }

  void OpenBase(bool fr, const disc::GameDisc &disc) {
    error_.clear();
    message_.clear();
    reduced_.clear();
    base_file_.clear();
    ported_.clear();
    if (!disc.Read(stars_[base_].file, base_file_)) {
      error_ = (fr ? "Lecture impossible : " : "Could not read: ") + stars_[base_].file;
      return;
    }
    char movie[16];
    snprintf(movie, sizeof(movie), "%03d.bik", stars_[base_].id);
    movie_.SetGame(device_, media::CopyGameMovie(disc, movie));  // (its entrance video starts from the base's)
    if (look_ >= 2 && !svr_attires_.empty())
      PortSvr(fr);  // (ported over the new base)
    SetModel(look_ == 0 ? base_file_ : look_ == 1 ? other_ : ported_);
  }

  // Its SvR 2011 entrance movie, kept only if it is a Bink 1 movie (WWE '12's kind).
  void SetSvrMovie(const fs::path &m) {
    char sig[3] = {};
    if (!m.empty())
      std::ifstream(m, std::ios::binary).read(sig, 3);
    svr_movie_file_ = memcmp(sig, "BIK", 3) == 0 ? m : fs::path();
    if (svr_movie_file_.empty())
      svr_movie_ = false;
    svr_view_.SetGame(device_, svr_movie_file_);
  }

  void SetModel(const Bytes &m) {
    model_ = m;
    view_stale_ = true;
  }

  static Bytes ReadAll(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  }

  // A superstar file: an EPK8 of "EMD " entries named 000NNNAA (another game's: any names, WWE 2K16's all "0001").
  static bool IsSuperstarFile(const Bytes &b, std::vector<std::string> *attires, std::string *why,
                              bool any_name = false) {
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(b, a, why))
      return false;
    for (const auto &k : a.root.kids) {
      if (k.group != "EMD " || (k.name.size() != 8 && !any_name)) {
        if (why)
          *why = "not a superstar file (chNNN.pac)";
        return false;
      }
      if (attires)
        attires->push_back(k.name);
    }
    return !a.root.kids.empty();
  }

  void LoadOther(bool fr, const fs::path &p) {
    other_path_ = p;
    other_ = ReadAll(p);
    std::string why;
    if (!IsSuperstarFile(other_, nullptr, &why)) {
      other_.clear();
      note_ok_ = false;
      note_ = (fr ? "Fichier refusé : " : "File refused: ") + why;
    } else {
      note_ok_ = true;
      note_ = fr ? "Fichier chargé." : "File loaded.";
    }
    SetModel(other_);
  }

  void LoadSvr(bool fr, const fs::path &p) {
    const std::string stem = p.stem().string();
    const int number = stem.size() > 2 && _strnicmp(stem.c_str(), "ch", 2) == 0 && isdigit(stem[2]) ? atoi(stem.c_str() + 2)
                                                                                                    : -1;
    LoadSvrBytes(fr, p, ReadAll(p), number,
                 [&](const char *rel) { return ReadAll(p.parent_path().parent_path() / rel); });  // (pac\ch\..)
    if (look_ == 2 && number >= 0) {
      char movie[16];
      snprintf(movie, sizeof(movie), "%03d.bik", number);
      std::error_code ec;
      const fs::path m = p.parent_path().parent_path().parent_path() / "movies" / "titantron" / movie;
      SetSvrMovie(fs::exists(m, ec) ? m : fs::path());
    }
  }

  // A superstar of the game chosen (SvR 2011 or WWE '13) from My game discs.
  void LoadFromDisc(bool fr, int number) {
    char rel[32];
    snprintf(rel, sizeof(rel), "pac/ch/ch%03d.pac", number);
    Bytes file;
    if (!game_disc_.Read(rel, file)) {
      note_ok_ = false;
      note_ = (fr ? "Lecture impossible : " : "Could not read: ") + std::string(rel);
      return;
    }
    disc_pick_ = number;
    LoadSvrBytes(fr, game_path_ / rel, std::move(file), number, [&](const char *in_pac) {
      Bytes b;
      game_disc_.Read(std::string("pac/") + in_pac, b);
      return b;
    });
    if (look_ == 2) {
      char movie[16];
      snprintf(movie, sizeof(movie), "%03d.bik", number);
      SetSvrMovie(media::CopyGameMovie(game_disc_, movie));
    }
  }

  // The superstars of the game chosen: its pac/ch/chNNN.pac files, named from its pac/ch/chEtc.pac (the CHAR / DAT
  // records: SvR 2011 264 bytes, id at +36 and name at +38; WWE '12 290 bytes, +44 / +46; WWE '13 324 bytes, +56 /
  // +58), else "chNNN".
  void ListDisc(uint32_t title) {
    disc_list_.clear();
    disc_pick_ = -1;
    game_title_ = title;
    game_path_ = discs_ ? discs_->PathOf(title) : fs::path();
    if (game_path_.empty() || !game_disc_.OpenPath(game_path_))
      return;
    std::map<int, std::string> names = NamesOf(game_disc_), guess;
    // The 2K games' numbers without a name in their strings (Names2K): the superstar numbers are the same as in WWE '13
    // and WWE '12, whose names are shown with a "?" (most of the time right).
    if (title != 0x5451085D && title != 0x545108B4) {  // (the 2K games' unnamed numbers)
      disc::GameDisc w13;
      if (discs_ && w13.OpenPath(discs_->PathOf(0x545108B4)))
        guess = NamesOf(w13);
      if (disc12_)
        for (const auto &[n, nm] : NamesOf(*disc12_))
          guess.emplace(n, nm);
    }
    for (const auto &f : game_disc_.List("pac/ch"))
      if (f.size() == 9 && _strnicmp(f.c_str(), "ch", 2) == 0 && _stricmp(f.c_str() + 5, ".pac") == 0 && isdigit(f[2]) &&
          isdigit(f[3]) && isdigit(f[4])) {
        const int n = atoi(f.c_str() + 2);
        const auto it = names.find(n), g = guess.find(n);
        const bool named = it != names.end() && it->second.size() > 1;  // (some records hold "0": no name)
        disc_list_.push_back({n, named ? it->second
                                 : g != guess.end() && g->second.size() > 1 ? g->second + " ?"
                                                                            : f.substr(0, 5)});
      }
    std::sort(disc_list_.begin(), disc_list_.end(), [](const auto &x, const auto &y) { return x.second < y.second; });
  }

  // A game's superstar names by number, from its pac/ch/chEtc.pac (see ListDisc).
  static std::map<int, std::string> NamesOf(const disc::GameDisc &g) {
    std::map<int, std::string> names;
    std::map<int, std::vector<int>> ids;  // the 2K games: number -> its "number + k" string ids' k
    Bytes etc;
    svrfmt::Archive a;
    if (g.Read("pac/ch/chEtc.pac", etc) && svrfmt::ArchiveOpen(etc, a, nullptr)) {
      const auto walk = [&](auto &self, svrfmt::ArcNode &n, int depth) -> void {
        const Bytes d = depth ? svrfmt::ArchiveData(n) : Bytes();
        const size_t id_at = d.size() == 264 ? 36 : d.size() == 290 ? 44 : d.size() == 324 ? 56 : 0;
        if (d.size() == 208 || d.size() == 216 || d.size() == 220) {  // (the 2K games: names in their strings)
          const int number = d[4] | d[5] << 8;
          for (size_t k = 6; k + 1 < 32; k += 2)
            if (const int v = d[k] | d[k + 1] << 8; number > 0 && v > number)
              ids[number].push_back(v - number);
          return;
        }
        if (id_at) {
          std::string name;
          for (size_t k = id_at + 2; k < id_at + 34 && k < d.size() && d[k]; ++k)
            name += d[k] < 0x80 ? std::string(1, char(d[k]))
                                : std::string{char(0xC0 | (d[k] >> 6)), char(0x80 | (d[k] & 0x3F))};
          names[d[id_at] | d[id_at + 1] << 8] = name;
          return;
        }
        if (depth < 3 && svrfmt::ArchiveExpand(n))
          for (auto &c : n.kids)
            self(self, c, depth + 1);
      };
      walk(walk, a.root, 0);
    }
    if (names.empty() && !ids.empty())  // (SvR 2011, WWE '12, WWE '13: names in the records)
      Names2K(g, ids, names);
    return names;
  }

  // The 2K games' names: their CHAR / DAT records (208, 216 or 220 bytes, number at +4) hold string ids made of the
  // number plus a constant: the full name is number + 64000 in WWE 2K14, number + 6000 in 2K15 to 2K17 (+1000 / +3000
  // are shorter names, +9000 the Twitter name). The strings: pac/string.pac (2K17: string_360.pac), SDB tables of
  // u32 0, u32 count, count x {u32 offset, u32 length, u32 id}, English first ("SDB /XX00"; 2K17's names are hashed).
  static void Names2K(const disc::GameDisc &g, const std::map<int, std::vector<int>> &ids,
                      std::map<int, std::string> &names) {
    Bytes str;
    svrfmt::Archive s;
    if ((!g.Read("pac/string.pac", str) && !g.Read("pac/string_360.pac", str)) || !svrfmt::ArchiveOpen(str, s, nullptr))
      return;
    std::map<uint32_t, std::string> strings;
    const auto walk = [&](auto &self, svrfmt::ArcNode &n, int depth) -> void {
      if (depth == 1 && n.group == "SDB " && n.name.size() == 4 && n.name.compare(2, 2, "00") != 0)
        return;  // (another language)
      const Bytes d = depth ? svrfmt::ArchiveData(n) : Bytes();
      uint32_t zero = 1, count = 0;
      if (d.size() >= 8) {
        memcpy(&zero, d.data(), 4);
        memcpy(&count, d.data() + 4, 4);
      }
      if (depth && zero == 0 && count && 8 + size_t(count) * 12 <= d.size()) {
        for (uint32_t i = 0; i < count; ++i) {
          uint32_t e[3];
          memcpy(e, d.data() + 8 + i * 12, 12);
          if (e[0] < d.size() && e[1] <= d.size() - e[0] && e[2] >= 6000)
            strings.emplace(e[2], std::string(reinterpret_cast<const char *>(d.data() + e[0]),
                                              strnlen(reinterpret_cast<const char *>(d.data() + e[0]), e[1])));
        }
        return;
      }
      if (depth < 3 && svrfmt::ArchiveExpand(n))
        for (auto &c : n.kids)
          self(self, c, depth + 1);
    };
    walk(walk, s.root, 0);
    for (const auto &[number, ks] : ids)
      for (int k : {64000, 6000})
        if (std::find(ks.begin(), ks.end(), k) != ks.end())
          if (const auto it = strings.find(uint32_t(number + k)); it != strings.end() && !it->second.empty()) {
            std::string name = it->second;
            for (const char *q : {"\xE2\x80\x9C", "\xE2\x80\x9D"})  // (curly quotes: plain ones for the font)
              for (size_t at; (at = name.find(q)) != std::string::npos;)
                name.replace(at, 3, "\"");
            names[number] = name;
            break;
          }
  }

  void LoadSvrBytes(bool fr, const fs::path &p, Bytes file, int number,
                    const std::function<Bytes(const char *)> &read_pac) {  // (a file of its game's pac folder)
    svr_path_ = p;
    svr_file_ = std::move(file);
    SetSvrMovie({});
    svr_attires_.clear();
    svr_attire_ = 0;
    std::string why;
    if (!IsSuperstarFile(svr_file_, &svr_attires_, &why, true)) {
      svr_attires_.clear();
      note_ok_ = false;
      note_ = (fr ? "Fichier refusé : " : "File refused: ") + why;
    } else {
      note_.clear();
    }
    // Its portraits, SSFA/SSFB/SSFC "NNNN" as WWE '12's (DXT5, no mips; HD 512 x 512, 512 x 512, 256 x 256).
    for (auto &hd : portraits_)
      for (auto &b : hd)
        b.clear();
    if (svr_attires_.empty())
      return;  // (the refusal stays shown)
    if (look_ == 2 && number >= 0)
      SvrPortraits(fr, number, read_pac);
    // WWE '13: its game's pac\menu\menuHD.pac / menuSD.pac (the same pictures as WWE '12's).
    if (look_ == 3 && number >= 0) {
      char name[8];
      snprintf(name, sizeof(name), "%04d", number);
      int found = 0;
      for (int hd = 0; hd < 2; ++hd) {
        svrfmt::Archive m;
        if (!svrfmt::ArchiveOpen(read_pac(hd ? "menu/menuHD.pac" : "menu/menuSD.pac"), m, nullptr))
          continue;
        for (const auto &k : m.root.kids)
          if (k.name == name && k.group.size() == 4 && k.group.compare(0, 3, "SSF") == 0 && k.group[3] >= 'A' &&
              k.group[3] <= 'C') {
            portraits_[hd][k.group[3] - 'A'] = svrfmt::ArchiveData(k);
            ++found;
          }
      }
      if (!found)
        found = Portraits2K(number, read_pac);
      if (found) {
        note_ok_ = true;
        note_ = fr ? "Ses portraits viendront avec lui." : "Its portraits come with it.";
      }
    }
  }

  // WWE 2K15 / 2K16: pac\menu\menuHD2.pac, SFAG / SFBG / SFCG "NNNN" (2K16: no SFBG, SFAG again), TGA 512 x 512,
  // 512 x 512, 256 x 256 framed as WWE '12's; made into WWE '12's DDS (DXT5, no mips; SD SSFA / SSFB 512 x 256).
  // (2K14 has WWE '12's SSFA-C; 2K17's SSFaceHD.pac has hashed names: not read yet.) The pac (~70 MB) is kept.
  int Portraits2K(int number, const std::function<Bytes(const char *)> &read_pac) {
    const std::string key = media::Utf8(svr_path_.parent_path().parent_path().wstring());
    if (key != menu2_of_) {
      menu2_of_ = key;
      menu2_ = {};
      svrfmt::ArchiveOpen(read_pac("menu/menuHD2.pac"), menu2_, nullptr);
    }
    char name[8];
    snprintf(name, sizeof(name), "%04d", number);
    svrfmt::Image pic[3];
    for (const auto &k : menu2_.root.kids)
      if (k.name == name && k.group.size() == 4 && k.group[0] == 'S' && k.group[1] == 'F' && k.group[3] == 'G' &&
          k.group[2] >= 'A' && k.group[2] <= 'C')
        Tga(svrfmt::ArchiveData(k), pic[k.group[2] - 'A']);
    if (pic[1].w == 0)
      pic[1] = pic[0];
    if (pic[0].w == 0 || pic[2].w == 0)
      return 0;
    const int size[2][3][2] = {{{512, 256}, {512, 256}, {256, 256}}, {{512, 512}, {512, 512}, {256, 256}}};
    for (int hd = 0; hd < 2; ++hd)
      for (int k = 0; k < 3; ++k)
        portraits_[hd][k] = svrfmt::DdsEncode(svrfmt::Resize(pic[k], size[hd][k][0], size[hd][k][1]),
                                              svrfmt::DxtFormat::kDxt5, false);
    return 6;
  }

  // An uncompressed 32-bit TGA (type 2: BGRA, bottom row first unless the descriptor's bit 5 says top first).
  static bool Tga(const Bytes &d, svrfmt::Image &out) {
    if (d.size() < 18 || d[2] != 2 || d[16] != 32)
      return false;
    const int w = d[12] | d[13] << 8, h = d[14] | d[15] << 8;
    const size_t at = 18 + d[0];
    if (w <= 0 || h <= 0 || d.size() < at + size_t(w) * h * 4)
      return false;
    const bool top_first = d[17] & 0x20;
    out.w = w;
    out.h = h;
    out.rgba.resize(size_t(w) * h * 4);
    for (int y = 0; y < h; ++y) {
      const uint8_t *row = &d[at + size_t(top_first ? y : h - 1 - y) * w * 4];
      for (int x = 0; x < w; ++x) {
        uint8_t *o = &out.rgba[(size_t(y) * w + x) * 4];
        o[0] = row[x * 4 + 2];
        o[1] = row[x * 4 + 1];
        o[2] = row[x * 4 + 0];
        o[3] = row[x * 4 + 3];
      }
    }
    return true;
  }

  // A SvR 2011 superstar's portraits: pac\DLC_HD.pac / DLC_SD.pac, SSFA/SSFB/SSFC "NNNN", WWE '12's pictures but for
  // SD's SSFA / SSFB (SvR 256 x 512, WWE '12 512 x 256, the HD picture squeezed): those made from the HD ones.
  void SvrPortraits(bool fr, int number, const std::function<Bytes(const char *)> &read_pac) {
    char name[8];
    snprintf(name, sizeof(name), "%04d", number);
    for (int hd = 0; hd < 2; ++hd) {
      svrfmt::Archive m;
      if (!svrfmt::ArchiveOpen(read_pac(hd ? "DLC_HD.pac" : "DLC_SD.pac"), m, nullptr))
        continue;
      for (const auto &k : m.root.kids)
        if (k.name == name && k.group.size() == 4 && k.group.compare(0, 3, "SSF") == 0 && k.group[3] >= 'A' &&
            k.group[3] <= 'C')
          portraits_[hd][k.group[3] - 'A'] = svrfmt::ArchiveData(k);
    }
    for (int k = 0; k < 2; ++k) {
      portraits_[0][k].clear();
      svrfmt::Image img;
      if (!portraits_[1][k].empty() && svrfmt::DdsDecode(portraits_[1][k], img))
        portraits_[0][k] = svrfmt::DdsEncode(svrfmt::Resize(img, 512, 256), svrfmt::DxtFormat::kDxt5, false);
    }
    // (all six, or none: the base's then)
    for (const auto &hd : portraits_)
      for (const auto &b : hd)
        if (b.empty()) {
          for (auto &h : portraits_)
            for (auto &x : h)
              x.clear();
          return;
        }
    note_ok_ = true;
    note_ = fr ? "Ses portraits de SvR 2011 viendront avec lui." : "Its SvR 2011 portraits come with it.";
  }

  // The SvR 2011 or WWE '13 attire over the base's first attire (port_svr.h, port13_star.h), made to fit that
  // attire's room.
  void PortSvr(bool fr) {
    note_.clear();
    ported_.clear();
    svrfmt::Archive s, w;
    std::string why;
    port_svr::Report rep;
    port13_star::Report rep13;
    const bool opened = svrfmt::ArchiveOpen(svr_file_, s, &why) && svrfmt::ArchiveOpen(base_file_, w, &why) &&
                        size_t(svr_attire_) < s.root.kids.size() && !w.root.kids.empty();
    if (!opened ||
        !(look_ == 3 ? port13_star::PortAttire(s.root.kids[size_t(svr_attire_)], w.root.kids[0], &rep13, &why)
                     : port_svr::PortAttire(s.root.kids[size_t(svr_attire_)], w.root.kids[0], &rep, &why))) {
      note_ok_ = false;
      note_ = (fr ? "Portage impossible : " : "Can't port it: ") + why;
      SetModel(ported_);
      return;
    }
    const size_t budget = w.root.kids[0].stored.size();  // (before Save: the base's attire as shipped)
    svrfmt::ArchiveTouch(w.root);
    const Bytes out = svrfmt::ArchiveSave(w);
    arena_edit::Arena fit;
    if (!fit.Load(out, &why, 0)) {
      note_ok_ = false;
      note_ = (fr ? "Portage illisible : " : "Unreadable port: ") + why;
      return;
    }
    const auto halved = fit.Fit(budget, ported_, true);
    reduced_.clear();
    if (!halved.empty()) {
      reduced_ = fr ? "Réduites de moitié pour tenir dans la place d'une tenue :" : "Halved to fit an attire's room:";
      for (const auto &n : halved)
        reduced_ += " " + n;
    }
    note_ok_ = true;
    char b[200];
    snprintf(b, sizeof(b), fr ? "Portée : %d maillages convertis (tenue 1 du nouveau catcheur)."
                              : "Ported: %d meshes converted (the new superstar's attire 1).",
             look_ == 3 ? rep13.meshes : rep.meshes);
    note_ = b;
    if (!portraits_[1][0].empty())
      note_ += look_ == 2 ? (fr ? " Ses portraits de SvR 2011 viendront avec lui." : " Its SvR 2011 portraits come with it.")
                          : (fr ? " Ses portraits viendront avec lui." : " Its portraits come with it.");
    SetModel(ported_);
  }

  void Draw3D(bool fr) {
    if (model_.empty()) {
      ImGui::TextDisabled("%s", fr ? "Pas encore de modèle." : "No model yet.");
      return;
    }
    if (view_stale_) {
      view_stale_ = false;
      view_error_.clear();
      if (!view_.Load(device_, model_, &view_error_, 0))
        view_error_ = (fr ? "Vue 3D impossible : " : "No 3D view: ") + view_error_;
    }
    if (!view_error_.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", view_error_.c_str());
      return;
    }
    if (ImGui::SmallButton(fr ? "Recentrer" : "Reset view"))
      view_.ResetCamera();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", fr ? "clic gauche : tourner, clic droit : déplacer, molette : zoom"
                                 : "left drag: turn, right drag: move, wheel: zoom");
    const ImVec2 size(ImGui::GetContentRegionAvail().x, std::max(200.0f, ImGui::GetContentRegionAvail().y - 4));
    ID3D11ShaderResourceView *pic = view_.Render(int(size.x), int(size.y));
    if (!pic)
      return;
    ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(pic))), size);
    if (ImGui::IsItemHovered()) {
      const ImGuiIO &io = ImGui::GetIO();
      if (io.MouseWheel != 0)
        view_.Zoom(io.MouseWheel);
      if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0))
        view_.Orbit(io.MouseDelta.x, io.MouseDelta.y);
      if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0))
        view_.Pan(io.MouseDelta.x, io.MouseDelta.y);
    }
  }

  // The mod's file is chosen first; with its own video, the video is made, then the mod (Finished).
  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    message_.clear();
    std::wstring file;
    for (unsigned char c : std::string(name_))
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    save_ = media::PickSave(fr, (file.empty() ? L"superstar" : file) + L".svrmod");
    if (save_.empty())
      return;
    if (look_ != 2)
      svr_movie_ = false;
    if (own_movie_) {
      made_ = media::TempDir() / L"made_new_superstar.bik";
      movie_.Start(made_);
      return;
    }
    Write(fr, mods, svr_movie_ ? ReadAll(svr_movie_file_) : Bytes{});
  }

  void Finished(bool fr, mods::Manager &mods) {
    if (!movie_.Ok()) {
      message_ok_ = false;
      message_ = (fr ? "La vidéo n'a pas pu être faite : " : "The movie could not be made: ") + movie_.Error();
      return;
    }
    const Bytes bik = ReadAll(made_);
    std::error_code ec;
    fs::remove(made_, ec);
    Write(fr, mods, bik);
  }

  void Write(bool fr, mods::Manager &mods, const Bytes &movie) {
    std::string id = "superstar_";
    for (unsigned char c : std::string(name_))
      id += std::isalnum(c) ? char(std::tolower(c)) : '_';
    const fs::path &save = save_;
    const Star &b = stars_[base_];
    std::string manifest = "game=wwe12\ntype=superstar\nid=" + id + "\nname=" + std::string(name_) +
                           "\nbase=" + std::to_string(b.id) + "\nversion=1.0\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + std::string(name_) + ", a new superstar starting from " +
                (chosen_ ? b.name : "an average superstar (" + b.name + ")") +
                (look_ == 3   ? ", its look ported from WWE '13 (the player's own copy)"
                 : look_ == 2 ? ", its look ported from SvR 2011 (the player's own file)"
                 : look_ == 1 ? ", with its own look"
                              : "") +
                (movie.empty()  ? ""
                 : svr_movie_ ? ", its SvR 2011 entrance video"
                              : ", its own entrance video (" + movie_.Parts() + ")") +
                ", made with the WWE '12 Mod Maker.\n";
    std::vector<std::pair<std::string, Bytes>> entries = {{"manifest.txt", Bytes(manifest.begin(), manifest.end())}};
    if (look_ != 0)
      entries.push_back({"superstar/ch.pac", model_});
    else
      entries.push_back({"superstar/look.txt", Bytes{'b', 'a', 's', 'e', '\n'}});  // (a mod needs a superstar/ file)
    if (!movie.empty())
      entries.push_back({"superstar/titantron.bik", movie});
    if (look_ >= 2)  // (its SvR 2011 / WWE '13 portraits)
      for (int hd = 0; hd < 2; ++hd)
        for (int k = 0; k < 3; ++k)
          if (!portraits_[hd][k].empty())
            entries.push_back({std::string("superstar/SSF") + char('A' + k) + (hd ? "_HD.dds" : "_SD.dds"),
                               portraits_[hd][k]});
    if (!mods::Manager::WriteZip(save, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    message_ok_ = true;
    message_ = (fr ? "Mod créé et installé : " : "Mod made and installed: ") + media::Utf8(save.wstring()) +
               (fr ? " (au prochain lancement du jeu : case DLC de l'écran de choix)"
                   : " (next time the game starts: the select screen's DLC tile)");
  }

  ID3D11Device *device_ = nullptr;
  bool listed_ = false;
  std::vector<Star> stars_;
  int base_ = -1, look_ = 0;
  int default_ = -1;     // the average superstar used without a base (index in stars_), -1 if the disc has none
  bool chosen_ = false;  // a base was chosen (else base_ is default_)
  char name_[32] = {}, author_[64] = {}, filter_[64] = {};
  Bytes base_file_, other_, ported_, model_, svr_file_;
  fs::path other_path_, svr_path_;

 public:
  const game_discs::Discs *discs_ = nullptr;  // My game discs (set by the Mod Maker)

 private:
  disc::GameDisc game_disc_;  // the other game's disc (My game discs), its superstars
  fs::path game_path_;
  uint32_t game_title_ = 0;
  std::vector<std::pair<int, std::string>> disc_list_;
  int disc_pick_ = -1;
  uint32_t star_game_ = 0x545108B4;
  int step_ = 0;  // the assistant's step
  const disc::GameDisc *disc12_ = nullptr;  // WWE '12 (its names, a guess for the 2K games)  // the WWE '13-kind game of "An attire from WWE '13 or 2K"
  std::vector<std::string> svr_attires_;
  int svr_attire_ = 0;
  arena_view::View view_;
  bool view_stale_ = true;
  std::string error_, note_, view_error_, message_, reduced_;
  bool note_ok_ = false, message_ok_ = false;
  media::MovieEditor movie_;
  bool own_movie_ = false;
  bool svr_movie_ = false;     // its SvR 2011 entrance movie, as it is
  fs::path svr_movie_file_;    // that movie (a copy, or in the player's SvR 2011 folder), "" if it has none
  media::MovieEditor svr_view_;  // (only its picture)
  fs::path save_, made_;
  svrfmt::Archive menu2_;  // a 2K game's menuHD2.pac (its portraits), for menu2_of_
  std::string menu2_of_;
  Bytes portraits_[2][3];  // a SvR 2011 / WWE '13 superstar's portraits ([SD / HD][SSFA, SSFB, SSFC]), else empty
};

}  // namespace new_star_page
