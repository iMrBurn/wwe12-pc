// Mod Maker, "Moves & entrances" page (2026-11-03): a superstar of another game (My game discs) lends his moves,
// entrance, victory scene and theme to a superstar WWE '12 has (Kaiko's SvR 2011 Mod Maker has such a page; players
// asked for it). What is taken of him is NEW SUPERSTAR's (star_port.h). The mod (.svrmod, type=moves, kind folder
// Moves) holds manifest superstar=<WWE '12 number>, moves=, entrance_own= and superstar/ files as a superstar mod's
// (entrance/, victory/, objects/, theme.wem, moves/); at startup the game puts them in that superstar's record in its
// mods pack and in the save's copy of it (src/native/wwe12_superstar_mods.cpp, superstar_pack.h Moveset) and serves
// the entrance and the victory scene (in place of his own: wwe12_arena_pages.cpp). Switched off, his own come back at
// the next start. Several on one superstar: the last one in folder order is used.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <functional>
#include <future>
#include <string>
#include <utility>
#include <vector>

#include "arena_page.h"
#include "dlc_stars.h"
#include "game_disc.h"
#include "game_discs.h"
#include "game_icons.h"
#include "game_process.h"
#include "imgui.h"
#include "media.h"
#include "mod_version.h"
#include "mods.h"
#include "move_preview.h"
#include "moves_edit.h"
#include "star_port.h"
#include "ui_style.h"

namespace moves_page {

namespace fs = std::filesystem;
using svrfmt::Bytes;

class Page {
 public:
  const game_discs::Discs *discs_ = nullptr;  // My game discs (set by the Mod Maker)
  fs::path user_root_;                         // Documents\wwe12 (set by the Mod Maker): the official DLC installed
  bool want_discs_ = false;                   // the player asked for My game discs (the Mod Maker opens it)

  void Draw(bool fr, ID3D11Device *dev, const disc::GameDisc &disc, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f);
    disc12_ = &disc;
    device_ = dev;
    if (!disc.Found()) {
      ImGui::TextColored(red, "%s", fr ? "Fichiers du jeu introuvables." : "Game files not found.");
      return;
    }
    if (!listed_) {
      List(disc);
      // (test aid: WWE12_MM_MOVES=<superstar number>,<tab>[,<move to play>] opens on him and that tab)
      if (const char *t = getenv("WWE12_MM_MOVES")) {
        int id = 0, tab = 0, play = 0;
        sscanf(t, "%d,%d,%d", &id, &tab, &play);
        for (int i = 0; i < int(stars12_.size()); ++i)
          if (stars12_[size_t(i)].id == id)
            target_ = i, target_by_player_ = true;
        tab_ = std::clamp(tab, 0, 3);
        test_play_ = play;
      }
      // (and WWE12_MM_PORT=<game title hex>,<his number>: the From another game tab on him, step 3)
      if (const char *t = getenv("WWE12_MM_PORT")) {
        unsigned title = 0;
        int number = 0;
        sscanf(t, "%x,%d", &title, &number);
        ChooseGame(title);
        for (const auto &[n, nm] : list_.list)
          if (n == number)
            Pick(n, nm);
        use_added_moves_ = true;
        step_ = 2;
        tab_ = 3;
        test_new_ = getenv("WWE12_MM_PLAYNEW") != nullptr;
      }
    }
    if (test_new_ && cat_tried_ && !parts_.new_moves.empty()) {
      preview_.Play(parts_.new_moves.back().high, parts_.new_moves.back().name);
      test_new_ = false;
    }
    if (test_play_ && cat_tried_) {
      PlayMove(test_play_ > 1 ? test_play_ : moves_edit::Catalog::At(cat_.pro[Who()], 0x026));
      test_play_ = 0;
    }
    if (!cat_tried_) {  // (the game's moves and its roster's movesets; the motions' places in m.pac)
      cat_tried_ = true;
      cat_.Load(disc);
      motions_.Build(disc);
    }
    // The tabs, as Kaiko's page: the moves, the taunts, the entrance (by hand, with the game's own), and from another
    // game (the assistant). One mod for one superstar holds them all.
    const char *tabs_fr[] = {"Prises", "Provocations", "Entrée", "Depuis un autre jeu"};
    const char *tabs_en[] = {"Moves", "Taunts", "Entrance", "From another game"};
    for (int i = 0; i < 4; ++i) {
      if (i)
        ImGui::SameLine();
      const bool on = tab_ == i;
      if (on)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.12f, 0.18f, 1.0f));
      if (ImGui::Button(fr ? tabs_fr[i] : tabs_en[i], ImVec2(0, 28)))
        tab_ = i;
      if (on)
        ImGui::PopStyleColor();
    }
    ImGui::Separator();
    if (tab_ == 3)
      DrawPort(fr, mods);
    else
      DrawHand(fr, disc, mods);
  }

  // The assistant: a superstar of another game lends his moves, entrance, victory scene and theme.
  void DrawPort(bool fr, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Donnez à un catcheur de WWE '12 les prises, l'entrée, la scène de victoire et la musique "
                                  "d'un catcheur de vos autres jeux. Le mod se coupe dans le launcher : le catcheur "
                                  "retrouve les siennes."
                                : "Give a WWE '12 superstar the moves, entrance, victory scene and theme of a superstar of "
                                  "your other games. Turn the mod off in the launcher: the superstar gets his own back.");
    // The assistant: the game, its superstar, what to take, for whom, the mod.
    const char *steps_fr[] = {"1. Jeu", "2. Catcheur", "3. Ce qu'on prend", "4. Pour qui", "5. Terminer"};
    const char *steps_en[] = {"1. Game", "2. Superstar", "3. What to take", "4. For whom", "5. Finish"};
    PollCollect();
    const int reach = !GameSet() ? 0 : pick_ < 0 || picking_ ? 1 : !AnyTaken() ? 2 : target_ < 0 ? 3 : 4;
    step_ = std::min(step_, reach);
    const float h = std::max(320.0f, ImGui::GetContentRegionAvail().y - 72);
    ImGui::BeginChild("mv_steps", ImVec2(190, h), ImGuiChildFlags_Borders);
    for (int i = 0; i < 5; ++i) {
      ImGui::BeginDisabled(i > reach);
      if (ImGui::Selectable(fr ? steps_fr[i] : steps_en[i], step_ == i, 0, ImVec2(0, 26)))
        step_ = i;
      ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    const bool with_preview = step_ == 2 && use_moves_ && HasMoves();
    ImGui::BeginChild("mv_body", ImVec2(with_preview ? -360.0f : 0.0f, h), ImGuiChildFlags_Borders);
    ImGui::PushTextWrapPos(0);
    switch (step_) {
    case 0: StepGame(fr); break;
    case 1: StepSuperstar(fr); break;
    case 2:
      if (picking_)
        DrawBusy(fr);
      else
        StepTake(fr);
      break;
    case 3: StepTarget(fr); break;
    default: StepFinish(fr, mods); break;
    }
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    if (with_preview) {
      ImGui::SameLine();
      ImGui::BeginChild("mv_port_preview", ImVec2(0, h), ImGuiChildFlags_Borders);
      PreviewPanel(fr);
      ImGui::EndChild();
    }
    ImGui::BeginDisabled(step_ == 0);
    if (ImGui::Button(fr ? "Retour" : "Back", ImVec2(110, 0)))
      --step_;
    ImGui::EndDisabled();
    ImGui::SameLine();
    char prog[32];
    snprintf(prog, sizeof(prog), fr ? "Étape %d sur 5" : "Step %d of 5", step_ + 1);
    ImGui::ProgressBar((step_ + 1) / 5.0f, ImVec2(ImGui::GetContentRegionAvail().x - 120, 0), prog);
    ImGui::SameLine();
    ImGui::BeginDisabled(step_ >= 4 || step_ + 1 > reach);
    if (ImGui::Button(fr ? "Suivant" : "Next", ImVec2(110, 0)))
      ++step_;
    ImGui::EndDisabled();
    // What keeps it from being done.
    if (!GameSet())
      ImGui::TextColored(red, "%s", fr ? "Choisissez un jeu de MES DISQUES (étape 1)." : "Choose a game of MY GAME DISCS (step 1).");
    else if (pick_ < 0)
      ImGui::TextColored(red, "%s", fr ? "Choisissez son catcheur (étape 2)." : "Choose its superstar (step 2).");
    else if (!AnyTaken())
      ImGui::TextColored(red, "%s", fr ? "Cochez au moins une chose à prendre (étape 3)." : "Tick at least one thing to take (step 3).");
    else if (target_ < 0)
      ImGui::TextColored(red, "%s", fr ? "Choisissez le catcheur de WWE '12 qui les reçoit (étape 4)."
                                       : "Choose the WWE '12 superstar who gets them (step 4).");
    else
      ImGui::TextColored(green, "%s", fr ? "Tout est prêt." : "All set.");
  }

 private:
  struct Star {
    int id = 0;
    std::string name;
  };

  bool GameSet() const { return game_ && discs_ && !discs_->PathOf(game_).empty(); }
  bool HasMoves() const { return !parts_.moves.moves.empty(); }
  bool HasEntrance() const { return !parts_.entrance.empty() && parts_.entrance_number > 0; }
  bool HasVictory() const { return !parts_.victory.empty(); }
  bool HasTheme() const { return !theme_.stream.empty() && tool_; }
  bool AnyTaken() const {
    return (use_moves_ && HasMoves()) || (use_entrance_ && HasEntrance()) || (use_victory_ && HasVictory()) ||
           (use_theme_ && HasTheme());
  }
  static std::string Lower(std::string s) {
    for (auto &c : s)
      c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
  }
  // A name compared on its letters and digits only ("Jeff Hardy ?" and "JEFF HARDY" alike).
  static std::string Key(const std::string &t) {
    std::string k;
    for (unsigned char c : t)
      if (std::isalnum(c))
        k += char(std::toupper(c));
    return k;
  }

  // WWE '12's superstars (pac/ch/chNNN.pac with a name), as NEW SUPERSTAR's base list.
  void List(const disc::GameDisc &disc) {
    listed_ = true;
    const auto names = disc::SuperstarNames(disc);
    for (const auto &f : disc.List("pac/ch")) {
      if (!dlc_stars::DiscStarFile(f))
        continue;
      Star s;
      s.id = atoi(f.c_str() + 2);
      const auto it = names.find(s.id);
      if (it == names.end())
        continue;
      s.name = it->second;
      stars12_.push_back(s);
    }
    // (and the official DLC superstars installed - Batista, Randy Savage...: their CHAR / PRO records are in the disc's
    // chEtc.pac, where a moves mod goes; they weren't listed, a player looked for Batista here, 2026-11-08)
    std::set<int> have;
    for (const auto &s : stars12_)
      have.insert(s.id);
    for (const auto &d : dlc_stars::List(user_root_, have))
      stars12_.push_back({d.id, d.name + " (DLC)"});
    std::sort(stars12_.begin(), stars12_.end(), [](const Star &a, const Star &b) { return a.name < b.name; });
  }

  // ---- by hand: the moves, the taunts, the entrance (the game's own), and the mod

  int Who() const { return target_ >= 0 ? stars12_[size_t(target_)].id : 0; }
  // His move in slot `off` as the game has it.
  int Own(int off) const {
    const auto it = cat_.pro.find(Who());
    return it != cat_.pro.end() ? moves_edit::Catalog::At(it->second, off) : 0;
  }
  // The moves the port gives (from another game, the ticked ones), by slot.
  std::map<int, int> PortMoves() const {
    std::map<int, int> out;
    if (!(use_moves_ && HasMoves()))
      return out;
    for (const auto &[off, id] : star_port::MovesToWrite(parts_.moves, use_added_moves_))
      if (!port_skip_.count(off))
        out[off] = id;
    return out;
  }
  // The move slot `off` falls back to without a change by hand: the port's, else his own.
  int Under(int off) const {
    const auto p = PortMoves();
    const auto it = p.find(off);
    return it != p.end() ? it->second : Own(off);
  }
  // His move in slot `off` with the mod: changed by hand, else the port's, else his own.
  int Current(int off) const {
    const auto e = edits_.find(off);
    return e != edits_.end() ? e->second : Under(off);
  }
  // His opponent in the preview: another superstar of the game.
  int Opponent() const { return Who() == 139 ? 161 : 139; }
  void PlayMove(int id) {
    if (disc12_)
      preview_.Play(motions_.Full(*disc12_, id), cat_.Name(id));
  }
  void PreviewPanel(bool fr) {
    const int who = Who() ? Who() : 139;
    preview_.Set(device_, *disc12_, who, Opponent());
    if (!preview_.HasMotion() && cat_.pro.count(who))  // (standing: his signature move's first frame)
      preview_.Stand(motions_.Full(*disc12_, moves_edit::Catalog::At(cat_.pro.at(who), 0x022)));
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    preview_.Draw(fr, ImVec2(avail.x, std::max(200.0f, avail.y - 70)));
    if (!Who())
      ImGui::TextDisabled("%s", fr ? "(montré sur John Cena : choisissez le catcheur pour le voir sur lui)"
                                   : "(shown on John Cena: choose the superstar to see it on him)");
  }

  // The WWE '12 superstar the mod is for (every tab: the same one).
  void TargetPicker(bool fr) {
    ImGui::SetNextItemWidth(320);
    if (ImGui::BeginCombo(fr ? "Catcheur de WWE '12##hand" : "WWE '12 superstar##hand",
                          target_ < 0 ? (fr ? "Choisir..." : "Choose...") : stars12_[size_t(target_)].name.c_str(),
                          ImGuiComboFlags_HeightLarge)) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
      ImGui::InputTextWithHint("##hand_target_filter", fr ? "Rechercher" : "Search", target_filter_, sizeof(target_filter_));
      const std::string want = Lower(target_filter_);
      for (int i = 0; i < int(stars12_.size()); ++i) {
        if (!want.empty() && Lower(stars12_[size_t(i)].name).find(want) == std::string::npos)
          continue;
        if (ImGui::Selectable(stars12_[size_t(i)].name.c_str(), i == target_) && i != target_) {
          target_ = i;
          target_by_player_ = true;
          message_.clear();
        }
      }
      ImGui::EndCombo();
    }
    if (edits_for_ != Who()) {  // (the changes are his: another superstar starts from his own)
      edits_.clear();
      entrance_of_ = 0;
      edits_for_ = Who();
    }
  }

  void DrawHand(bool fr, const disc::GameDisc &disc, mods::Manager &mods) {
    (void)disc;
    TargetPicker(fr);
    if (!cat_.loaded) {
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s",
                         fr ? "Les prises du jeu n'ont pas pu être lues (misc.pac, chEtc.pac)."
                            : "The game's moves could not be read (misc.pac, chEtc.pac).");
      return;
    }
    if (target_ < 0) {
      ImGui::TextDisabled("%s", fr ? "Choisissez le catcheur dont on change les prises, les provocations ou l'entrée."
                                   : "Choose the superstar whose moves, taunts or entrance are changed.");
      return;
    }
    const float h = std::max(300.0f, ImGui::GetContentRegionAvail().y - 110);
    ImGui::BeginChild("mv_hand", ImVec2(-360.0f, h), ImGuiChildFlags_Borders);
    ImGui::PushTextWrapPos(0);
    if (tab_ == 0) {
      ImGui::TextWrapped("%s", fr ? "Ses prises case par case. Changer... propose d'abord les prises que les catcheurs du jeu "
                                    "mettent dans cette case, puis les autres du même genre. Jouer la montre sur lui."
                                  : "His moves slot by slot. Change... offers the moves the game's superstars put in that "
                                    "slot first, then the others of the same kind. Play shows it on him.");
      ImGui::SetNextItemWidth(300);
      ImGui::InputTextWithHint("##slot_find", fr ? "Chercher dans ses prises" : "Find in his moves", find_, sizeof(find_));
      for (int sct : {3, 4})
        SectionSlots(fr, sct, true);
      for (int sct = 0; sct < moves_edit::SectionCount(); ++sct)
        if (sct > 4 && moves_edit::SectionInfo(sct).shown)
          SectionSlots(fr, sct, false);
    } else if (tab_ == 1) {
      ImGui::TextWrapped("%s", fr ? "Ses provocations (et son retour) : celles de n'importe quel catcheur du jeu."
                                  : "His taunts (and his comeback): any superstar of the game's.");
      for (int sct : {1, 2})
        SectionSlots(fr, sct, true, false);
    } else {
      EntranceTab(fr);
    }
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("mv_hand_preview", ImVec2(0, h), ImGuiChildFlags_Borders);
    PreviewPanel(fr);
    ImGui::EndChild();
    FinishBar(fr, mods);
  }

  // A section's slots (those the roster doesn't all have alike), each with its move, Change... and Play.
  void SectionSlots(bool fr, int sct, bool open, bool search = true) {
    const int a = moveset_port::detail::kSections[sct][0], b = moveset_port::detail::kSections[sct][1];
    std::vector<int> slots;
    for (int off = a; off < b; off += 2)
      if (!cat_.fixed.count(off) && cat_.slot_moves.count(off) && !cat_.slot_moves.at(off).empty())
        slots.push_back(off);
    if (slots.empty())
      return;
    const std::string want = search ? Lower(find_) : std::string();
    if (!want.empty()) {  // (a search: only the slots whose move has those letters)
      std::vector<int> keep;
      for (int off : slots)
        if (Lower(cat_.Name(Current(off))).find(want) != std::string::npos)
          keep.push_back(off);
      slots.swap(keep);
      if (slots.empty())
        return;
      open = true;
    }
    const auto &info = moves_edit::SectionInfo(sct);
    char head[96];
    snprintf(head, sizeof(head), "%s (%zu)###sec%d", fr ? info.fr : info.en, slots.size(), sct);
    if (!want.empty())
      ImGui::SetNextItemOpen(true);
    if (!ImGui::CollapsingHeader(head, open ? ImGuiTreeNodeFlags_DefaultOpen : 0))
      return;
    int n = 0;
    for (int off : slots)
      SlotRow(fr, off, ++n);
  }

  void SlotRow(bool fr, int off, int n) {
    ImGui::PushID(off);
    const int now = Current(off), own = Own(off), under = Under(off);
    const bool edited = edits_.count(off) != 0, ported = !edited && under != own;
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%2d.", n);
    ImGui::SameLine(44);
    const ImVec4 col = edited   ? ImVec4(0.94f, 0.75f, 0.24f, 1.0f)
                       : ported ? ImVec4(0.55f, 0.85f, 0.45f, 1.0f)
                                : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImGui::TextColored(col, "%s", cat_.Name(now).c_str());
    if (edited || ported) {
      ImGui::SameLine();
      ImGui::TextDisabled(edited ? (fr ? "(changée ; avant : %s)" : "(changed; before: %s)")
                                 : (fr ? "(portée ; la sienne : %s)" : "(ported; his own: %s)"),
                          cat_.Name(edited ? under : own).c_str());
    }
    ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 8, ImGui::GetContentRegionMax().x - 200));
    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("##change", fr ? "Changer..." : "Change...", ImGuiComboFlags_HeightLarge)) {
      if (ImGui::IsWindowAppearing()) {
        pick_find_[0] = 0;
        ImGui::SetKeyboardFocusHere();
      }
      ImGui::SetNextItemWidth(300);
      ImGui::InputTextWithHint("##pick_find", fr ? "Rechercher" : "Search", pick_find_, sizeof(pick_find_));
      const std::string want = Lower(pick_find_);
      if (edited && ImGui::Selectable(((fr ? "Remettre : " : "Back to: ") + cat_.Name(under)).c_str()))
        edits_.erase(off);
      std::set<int> seen;
      auto item = [&](int id) {
        if (!seen.insert(id).second || (!want.empty() && Lower(cat_.Name(id)).find(want) == std::string::npos))
          return;
        const std::string label = cat_.Name(id) + "##" + std::to_string(id);
        if (ImGui::Selectable(label.c_str(), id == now)) {
          if (id == under)
            edits_.erase(off);
          else
            edits_[off] = id;
          PlayMove(id);
        }
      };
      ImGui::SeparatorText(fr ? "Dans cette case chez les catcheurs du jeu" : "In this slot for the game's superstars");
      for (int id : cat_.slot_moves[off])
        item(id);
      ImGui::SeparatorText(fr ? "Les autres du même genre" : "The others of the same kind");
      for (int id : cat_.section_moves[moveset_port::detail::SectionOf(off)])
        item(id);
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!motions_.Has(now));
    if (ImGui::Button(fr ? "Jouer" : "Play", ImVec2(70, 0)))
      PlayMove(now);
    ImGui::EndDisabled();
    ImGui::PopID();
  }

  // Another superstar of the game's entrance (his entrance, its music and its titantron), or his own.
  void EntranceTab(bool fr) {
    ImGui::TextWrapped("%s", fr ? "Son entrée : la sienne, ou celle d'un autre catcheur du jeu (son entrée, sa musique et sa "
                                  "vidéo de titantron). Celle d'un catcheur d'un autre jeu : l'onglet Depuis un autre jeu."
                                : "His entrance: his own, or another superstar of the game's (his entrance, its music and "
                                  "its titantron video). One of another game's superstar: the From another game tab.");
    ImGui::Dummy(ImVec2(0, 4));
    std::string now = fr ? "La sienne" : "His own";
    for (const auto &st : stars12_)
      if (st.id == entrance_of_)
        now = st.name;
    ImGui::SetNextItemWidth(320);
    if (ImGui::BeginCombo(fr ? "Entrée" : "Entrance", now.c_str(), ImGuiComboFlags_HeightLarge)) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
      ImGui::InputTextWithHint("##ent_find", fr ? "Rechercher" : "Search", ent_find_, sizeof(ent_find_));
      if (ImGui::Selectable(fr ? "La sienne" : "His own", entrance_of_ == 0))
        entrance_of_ = 0;
      const std::string want = Lower(ent_find_);
      for (const auto &st : stars12_) {
        if (st.id == Who() || (!want.empty() && Lower(st.name).find(want) == std::string::npos))
          continue;
        if (ImGui::Selectable((st.name + "##e" + std::to_string(st.id)).c_str(), st.id == entrance_of_))
          entrance_of_ = st.id;
      }
      ImGui::EndCombo();
    }
    if (use_entrance_ && HasEntrance())
      ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f),
                         fr ? "L'entrée de %s (onglet Depuis un autre jeu) passe avant celle-ci."
                            : "%s's entrance (From another game tab) goes before this one.",
                         pick_name_.c_str());
  }

  bool AnyChange() const { return AnyTaken() || !edits_.empty() || entrance_of_ > 0; }

  // Under the tabs by hand: what the mod holds, its name, the button.
  void FinishBar(bool fr, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    char what[220];
    snprintf(what, sizeof(what), fr ? "Le mod : %zu prise(s) changée(s) à la main%s%s" : "The mod: %zu move(s) changed by hand%s%s",
             edits_.size(), entrance_of_ ? (fr ? ", l'entrée d'un autre" : ", another's entrance") : "",
             AnyTaken() ? (fr ? ", et ce qui est pris d'un autre jeu" : ", and what is taken from another game") : "");
    ImGui::TextDisabled("%s", what);
    if (name_[0] == 0 && target_ >= 0)
      snprintf(name_, sizeof(name_), "%s moves", stars12_[size_t(target_)].name.c_str());
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##hand_name", fr ? "Nom du mod" : "Mod name", name_, sizeof(name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::InputTextWithHint("##hand_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_, sizeof(author_));
    ImGui::SameLine();
    ImGui::BeginDisabled(!name_[0] || target_ < 0 || !AnyChange() || game::IsRunning());
    if (ImGui::Button(fr ? "Créer le mod et l'installer..." : "Make the mod and install it...", ImVec2(240, 0)))
      Make(fr, mods);
    ImGui::EndDisabled();
    if (message_ok_) {
      ImGui::SameLine();
      ImGui::BeginDisabled(game::IsRunning());
      if (ImGui::Button(fr ? "Tester en jeu" : "Test in game", ImVec2(140, 0)))
        arena_page::LaunchGame();
      ImGui::EndDisabled();
    }
    if (game::IsRunning())
      ImGui::TextColored(red, "%s", fr ? "Le jeu est lancé : fermez-le pour installer le mod."
                                       : "The game is running: close it to install the mod.");
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
  }

  // From another game: his moves by section, each ticked or not, what WWE '12 makes of it, and Play.
  void PortTable(bool fr) {
    std::map<int, int> to;  // (slot -> the WWE '12 move it becomes)
    for (const auto &[off, id] : parts_.moves.moves)
      to[off] = id;
    ImGui::Dummy(ImVec2(0, 2));
    if (ImGui::SmallButton(fr ? "Tout prendre" : "Choose all"))
      port_skip_.clear();
    ImGui::SameLine();
    if (ImGui::SmallButton(fr ? "Ne rien prendre" : "Choose none"))
      for (const auto &r : parts_.moves.rows)
        port_skip_.insert(r.off);
    ImGui::SameLine();
    ImGui::TextDisabled(fr ? "%zu prise(s) choisie(s)" : "%zu move(s) chosen", PortMoves().size());
    int last = -1;
    for (const auto &r : parts_.moves.rows) {
      const int sct = moveset_port::detail::SectionOf(r.off);
      if (sct != last) {
        last = sct;
        ImGui::SeparatorText(fr ? moves_edit::SectionInfo(sct).fr : moves_edit::SectionInfo(sct).en);
      }
      ImGui::PushID(r.off);
      const auto t = to.find(r.off);
      const bool can = t != to.end() && (!r.new_move || use_added_moves_);
      ImGui::BeginDisabled(!can);
      bool on = can && !port_skip_.count(r.off);
      if (ImGui::Checkbox("##take", &on)) {
        if (on)
          port_skip_.erase(r.off);
        else
          port_skip_.insert(r.off);
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::TextUnformatted(r.from.empty() ? "?" : r.from.c_str());
      ImGui::SameLine(ImGui::GetContentRegionMax().x * 0.45f);
      if (t != to.end() && !r.new_move)
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f), fr ? "WWE '12 l'a : %s" : "WWE '12 has it: %s", r.to.c_str());
      else if (t != to.end())
        ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s",
                           use_added_moves_ ? (fr ? "ajoutée au jeu (expérimental)" : "added to the game (experimental)")
                                            : (fr ? "WWE '12 ne l'a pas : cochez Prises ajoutées pour l'ajouter"
                                                  : "WWE '12 hasn't it: tick Added moves to add it"));
      else
        ImGui::TextDisabled("%s", r.kept_why == 2   ? (fr ? "WWE '12 ne l'a pas : la sienne reste" : "WWE '12 hasn't it: his own stays")
                                  : r.kept_why == 3 ? (fr ? "pas ce genre de case : la sienne reste" : "not that kind of slot: his own stays")
                                                    : (fr ? "la sienne reste" : "his own stays"));
      if (t != to.end() && !r.new_move) {
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 70);
        ImGui::BeginDisabled(!motions_.Has(t->second));
        if (ImGui::Button(fr ? "Jouer" : "Play", ImVec2(64, 0)))
          PlayMove(t->second);
        ImGui::EndDisabled();
      } else if (r.new_move) {  // (one WWE '12 hasn't: its motion from his game, as it would be added)
        for (const auto &m : parts_.new_moves)
          if (m.number == r.from_id) {
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 70);
            if (ImGui::Button(fr ? "Jouer" : "Play", ImVec2(64, 0)))
              preview_.Play(m.high, m.name);
            break;
          }
      }
      ImGui::PopID();
    }
  }

  // Step 1: the game, one row each (from My game discs).
  void StepGame(bool fr) {
    ImGui::TextWrapped("%s", fr ? "De quel jeu vient le catcheur ? Les jeux de MES DISQUES sont prêts ; ajoutez les autres "
                                  "là-bas une fois pour toutes."
                                : "Which game does the superstar come from? The games of MY GAME DISCS are ready; add the "
                                  "others there once and for all.");
    ImGui::Dummy(ImVec2(0, 4));
    // (a simple list with each game's icon from the player's disc, the user's wish, 2026-11-09: a grid of cards before)
    game_icons::DrawList(
        fr, device_, discs_, icons_, game_,
        [](const game_discs::Known &k) { return k.title != 0x54510890 && k.superstars != game_discs::Can::kNo; },
        [](uint32_t t) { return star_port::GameName(t); },
        [fr](uint32_t) { return fr ? "prises, entrée, victoire, musique" : "moves, entrance, victory, theme"; },
        [this](uint32_t t) { ChooseGame(t); }, [this]() { want_discs_ = true; });
  }

  void ChooseGame(uint32_t title) {
    if (title != game_) {
      game_ = title;
      star_port::ListGame(discs_, disc12_, title, list_);
      pick_ = -1;
      pick_name_.clear();
      parts_ = {};
      theme_ = {};
      themes_.clear();
      filter_[0] = 0;
    }
    step_ = 1;
  }

  // Step 2: his superstar in that game.
  void StepSuperstar(bool fr) {
    ImGui::TextWrapped(fr ? "Le catcheur de %s dont on prend les prises, l'entrée, la victoire ou la musique."
                          : "The %s superstar whose moves, entrance, victory or theme are taken.",
                       star_port::GameName(game_));
    if (list_.list.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s",
                         fr ? "Aucun catcheur trouvé sur ce disque." : "No superstar found on that disc.");
      return;
    }
    ImGui::SetNextItemWidth(320);
    ImGui::InputTextWithHint("##mv_filter", fr ? "Rechercher" : "Search", filter_, sizeof(filter_));
    ImGui::BeginChild("mv_list", ImVec2(0, std::max(160.0f, ImGui::GetContentRegionAvail().y - 60)), ImGuiChildFlags_Borders);
    const std::string want = Lower(filter_);
    const bool old = game_ == 0x5451080B || game_ == 0x545107E0;  // (SvR 2008 / 2007: chNN.pac)
    for (const auto &[n, nm] : list_.list) {
      if (!want.empty() && Lower(nm).find(want) == std::string::npos)
        continue;
      char label[96];
      snprintf(label, sizeof(label), old ? "%s  (ch%02d)" : "%s  (ch%03d)", nm.c_str(), n);
      if (ImGui::Selectable(label, n == pick_, Busy() ? ImGuiSelectableFlags_Disabled : 0) && n != pick_)
        Pick(n, nm);
    }
    ImGui::EndChild();
    if (picking_)
      DrawBusy(fr);
    else if (pick_ >= 0)
      ImGui::TextDisabled(fr ? "Choisi : %s" : "Chosen: %s", pick_name_.c_str());
  }

  // His parts (star_port::Collect) and theme; what can be taken ticked; WWE '12's superstar of the same name proposed.
  void Pick(int number, const std::string &name) {
    pick_ = number;
    port_skip_.clear();
    pick_name_ = name.size() > 2 && name.compare(name.size() - 2, 2, " ?") == 0 ? name.substr(0, name.size() - 2) : name;
    parts_ = {};
    use_moves_ = use_entrance_ = use_victory_ = use_theme_ = false;
    picking_ = true;
    Collect([this, number]() {
      picking_ = false;
      star_port::FindTheme(list_, number, theme_, &themes_);
      theme_pick_ = 0;
      tool_ = !star_port::Vgmstream(discs_).empty();
      use_moves_ = HasMoves();
      use_entrance_ = HasEntrance();
      use_victory_ = HasVictory();
      use_theme_ = HasTheme();
      if (!target_by_player_) {
        target_ = -1;
        for (int i = 0; i < int(stars12_.size()); ++i)
          if (Key(stars12_[size_t(i)].name) == Key(pick_name_))
            target_ = i;
      }
      snprintf(name_, sizeof(name_), "%s (%s)", pick_name_.c_str(), star_port::GameName(game_));
      message_.clear();
    });
  }

  // His parts, read in the background (seconds: his moves WWE '12 hasn't are made then - the window froze, "Not
  // responding", 2026-11-09); `then` runs once they're in (PollCollect, each frame).
  void Collect(std::function<void()> then = {}) {
    if (pick_ < 0 || !discs_ || !disc12_)
      return;
    if (collecting_.valid())
      collecting_.wait();  // (one at a time: the list is greyed out meanwhile, this is only a safety)
    std::string listed;
    for (const auto &[n, nm] : list_.list)
      if (n == pick_)
        listed = nm;
    const disc::GameDisc *w12 = disc12_;
    const game_discs::Discs *discs = discs_;
    const uint32_t game = game_;
    const int pick = pick_, base = target_ >= 0 ? stars12_[size_t(target_)].id : -1;
    star_port::Music12 *music = &music12_;
    collecting_for_ = target_;
    after_collect_ = std::move(then);
    collecting_ = std::async(std::launch::async, [=]() {
      star_port::Parts p;
      star_port::Collect(*w12, *discs, game, pick, listed, base, *music, p);
      return p;
    });
  }
  bool Busy() const { return collecting_.valid(); }
  void PollCollect() {
    if (!collecting_.valid() || collecting_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
      return;
    parts_ = collecting_.get();
    collected_for_ = collecting_for_;
    auto then = std::move(after_collect_);
    after_collect_ = {};
    if (then)
      then();
  }
  void DrawBusy(bool fr) {
    static const char *kDots[] = {"", ".", "..", "..."};
    ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), fr ? "Lecture de %s dans %s%s" : "Reading %s in %s%s",
                       pick_name_.c_str(), star_port::GameName(game_), kDots[int(ImGui::GetTime() * 3) % 4]);
    ImGui::TextDisabled("%s", fr ? "(ses prises, son entrée, sa victoire, sa musique : quelques secondes)"
                                 : "(his moves, entrance, victory and theme: a few seconds)");
  }

  // Step 3: what to take, each with whether it can be.
  void StepTake(bool fr) {
    const char *from = star_port::GameName(game_);
    ImGui::TextWrapped(fr ? "Ce que %s apporte de %s (ce qui manque est grisé, avec pourquoi)."
                          : "What %s brings from %s (what's missing is greyed out, with why).",
                       pick_name_.c_str(), from);
    ImGui::Dummy(ImVec2(0, 4));
    char label[200];
    // moves
    snprintf(label, sizeof(label), "%s",
             fr ? "Ses prises (les cases sans équivalent restent celles du catcheur)"
                : "His moves (the slots without a match stay the superstar's own)");
    Choice(HasMoves(), use_moves_, HasMoves() ? label : (fr ? "Ses prises" : "His moves"), MovesWhy(fr));
    if (use_moves_ && HasMoves()) {
      ImGui::Indent(24);
      star_port::DrawAddedMovesChoice(fr, use_added_moves_, parts_.new_moves.size());
      PortTable(fr);
      ImGui::Unindent(24);
    }
    // entrance
    snprintf(label, sizeof(label), fr ? "Son entrée (n° %d, %d variantes ; essai)" : "His entrance (no. %d, %d variants; a test)",
             parts_.entrance_number,
             int(std::count_if(parts_.entrance.begin(), parts_.entrance.end(), [](const auto &p) { return p.kind == "EVP"; })));
    Choice(HasEntrance(), use_entrance_, HasEntrance() ? label : (fr ? "Son entrée" : "His entrance"),
           fr ? "pas d'entrée à lui trouvée dans son jeu" : "no entrance of his found in his game");
    if (use_entrance_ && HasEntrance() && parts_.one_take)
      ImGui::TextDisabled("%s", fr ? "(entrée de 2K15 d'un seul tenant : sortie de l'intro cachée de WWE '12, avec la foule et l'annonceur)"
                                   : "(a 2K15 entrance in one piece: moved out of WWE '12's hidden intro, with the crowd and the ring announcer)");
    // victory scene
    Choice(HasVictory(), use_victory_, fr ? "Sa scène de victoire (essai)" : "His victory scene (a test)",
           fr ? "pas de scène de victoire à lui trouvée dans son jeu" : "no victory scene of his found in his game");
    if ((use_entrance_ && HasEntrance()) || (use_victory_ && HasVictory()))
      if (!parts_.objects.empty())
        ImGui::TextDisabled(fr ? "(avec %zu objet(s) que WWE '12 n'a pas)" : "(with %zu object(s) WWE '12 hasn't)",
                            parts_.objects.size());
    // theme (vgmstream looked for again: the player may have just put it there)
    const bool tool = tool_ = !star_port::Vgmstream(discs_).empty();
    snprintf(label, sizeof(label), fr ? "Sa musique d'entrée (%s)" : "His entrance theme (%s)", theme_.what.c_str());
    Choice(HasTheme(), use_theme_, HasTheme() ? label : (fr ? "Sa musique d'entrée" : "His entrance theme"),
           theme_.stream.empty() ? (fr ? "pas de musique à lui trouvée dans son jeu" : "no theme of his found in his game")
                                 : (fr ? "vgmstream manque : il est livré avec le Mod Maker, dans le dossier vgmstream à côté de lui "
                                         "(le remettre depuis le zip de la version)"
                                       : "vgmstream is missing: it comes with the Mod Maker, in the vgmstream folder beside it "
                                         "(put it back from the release zip)"));
    // (more than one: its profile's and its number's, which may differ: WWE '13 Mankind's profile plays Mick Foley's)
    if (tool && themes_.size() > 1 && use_theme_) {
      ImGui::Indent(24);
      ImGui::SetNextItemWidth(320);
      if (ImGui::BeginCombo("##mv_theme_pick", themes_[size_t(theme_pick_)].what.c_str())) {
        for (int i = 0; i < int(themes_.size()); ++i) {
          const std::string item = themes_[size_t(i)].what + (i == 0 ? (fr ? "  (celle de sa fiche)" : "  (its profile's)")
                                                                     : (fr ? "  (une autre trouvée pour lui)" : "  (another one found for it)"));
          if (ImGui::Selectable(item.c_str(), i == theme_pick_)) {
            theme_pick_ = i;
            theme_ = themes_[size_t(i)];
          }
        }
        ImGui::EndCombo();
      }
      ImGui::Unindent(24);
    }
    if (use_theme_ && HasTheme() && !(use_entrance_ && HasEntrance()))
      ImGui::TextDisabled("%s", fr ? "(sans son entrée, la musique joue pendant l'entrée du catcheur de WWE '12)"
                                   : "(without his entrance, the theme plays during the WWE '12 superstar's own entrance)");
  }

  // A thing to take: ticked or not, or greyed out with why.
  static void Choice(bool can, bool &use, const char *label, const std::string &why) {
    ImGui::BeginDisabled(!can);
    bool on = use && can;
    if (ImGui::Checkbox(label, &on))
      use = on;
    ImGui::EndDisabled();
    if (!can) {
      ImGui::SameLine();
      ImGui::TextDisabled("(%s)", why.c_str());
    }
  }

  std::string MovesWhy(bool fr) const {
    if (star_port::SvrOld(game_))
      return fr ? std::string("les prises de ") + star_port::GameName(game_) + " ne sont pas encore lues"
                : std::string(star_port::GameName(game_)) + "'s moves aren't read yet";
    if (parts_.moves.rows.empty() && !parts_.moves_why.empty())
      return (fr ? "pas lues : " : "not read: ") + parts_.moves_why;
    return fr ? "aucune trouvée dans WWE '12" : "none found in WWE '12";
  }

  // Step 4: the WWE '12 superstar who gets them.
  void StepTarget(bool fr) {
    ImGui::TextWrapped("%s", fr ? "Le catcheur de WWE '12 qui les reçoit, à la place des siens tant que le mod est actif."
                                : "The WWE '12 superstar who gets them, in place of his own while the mod is on.");
    ImGui::SetNextItemWidth(320);
    if (ImGui::BeginCombo(fr ? "Catcheur de WWE '12" : "WWE '12 superstar",
                          target_ < 0 ? (fr ? "Choisir..." : "Choose...") : stars12_[size_t(target_)].name.c_str(),
                          ImGuiComboFlags_HeightLarge)) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
      ImGui::InputTextWithHint("##mv_target_filter", fr ? "Rechercher" : "Search", target_filter_, sizeof(target_filter_));
      const std::string want = Lower(target_filter_);
      for (int i = 0; i < int(stars12_.size()); ++i) {
        if (!want.empty() && Lower(stars12_[size_t(i)].name).find(want) == std::string::npos)
          continue;
        if (ImGui::Selectable(stars12_[size_t(i)].name.c_str(), i == target_)) {
          target_ = i;
          target_by_player_ = true;
          message_.clear();
        }
      }
      ImGui::EndCombo();
    }
    // (a superstar WWE '12 hasn't is made with NEW SUPERSTAR)
    ImGui::TextDisabled("%s", fr ? "(les catcheurs de WWE '12 et de ses DLC installés ; pour un autre : page NOUVEAU CATCHEUR)"
                                 : "(WWE '12's superstars and its installed DLC's; for another: the NEW SUPERSTAR page)");
    // (2K16 / 2K17's effects stand in with his own in WWE '12 when no older game has them: made again for him)
    if (target_ >= 0 && collected_for_ != target_ && (game_ == 0x315A07D2 || game_ == 0x545408BA))
      Recollect();
    if (target_ < 0)
      return;
    const std::string &who = stars12_[size_t(target_)].name;
    ImGui::Dummy(ImVec2(0, 4));
    std::string what;
    auto add = [&](bool on, const char *en, const char *f) {
      if (on)
        what += std::string(what.empty() ? "" : ", ") + (fr ? f : en);
    };
    add(use_moves_ && HasMoves(), "his moves", "ses prises");
    add(use_entrance_ && HasEntrance(), "his entrance", "son entrée");
    add(use_victory_ && HasVictory(), "his victory scene", "sa scène de victoire");
    add(use_theme_ && HasTheme(), "his theme", "sa musique");
    ImGui::TextWrapped(fr ? "%s aura %s de %s (%s)." : "%s will have %s of %s (%s).", who.c_str(), what.c_str(),
                       pick_name_.c_str(), star_port::GameName(game_));
    ImGui::TextDisabled("%s", fr ? "Le mod coupé dans le launcher, il retrouve les siens au lancement suivant. Pendant que le "
                                   "mod est actif, ses prises changées dans le jeu (Modifier les prises) sont remises à "
                                   "celles du mod à chaque lancement."
                                 : "With the mod turned off in the launcher, he gets his own back at the next start. While "
                                   "the mod is on, his moves changed in the game (Edit Moveset) go back to the mod's at "
                                   "each start.");
  }

  // Its parts again for another WWE '12 superstar (only his effects may change), the choices kept.
  void Recollect() {
    const bool m = use_moves_, e = use_entrance_, v = use_victory_;
    Collect([this, m, e, v]() {
      use_moves_ = m && HasMoves();
      use_entrance_ = e && HasEntrance();
      use_victory_ = v && HasVictory();
    });
  }

  // Step 5: the mod.
  void StepFinish(bool fr, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    ImGui::SetNextItemWidth(320);
    ImGui::InputTextWithHint(fr ? "Nom du mod" : "Mod name", fr ? "(dans le launcher)" : "(in the launcher)", name_,
                             sizeof(name_));
    ImGui::SetNextItemWidth(320);
    ImGui::InputTextWithHint(fr ? "Auteur" : "Author", fr ? "(facultatif)" : "(optional)", author_, sizeof(author_));
    mod_version::Input("Version##mv_version", version_, sizeof(version_));
    ImGui::Dummy(ImVec2(0, 6));
    if (target_ >= 0)
      ImGui::TextWrapped(fr ? "Le mod (.svrmod) est enregistré où vous voulez, pour le partager, puis installé. Dans le jeu : "
                              "%s, au prochain lancement."
                            : "The mod (.svrmod) is saved where you want, to share it, then installed. In the game: %s, "
                              "next time the game starts.",
                         stars12_[size_t(target_)].name.c_str());
    const bool ready = name_[0] && target_ >= 0 && AnyTaken() && !Busy();  // (not while his parts are read again)
    ImGui::BeginDisabled(!ready || game::IsRunning());
    if (ImGui::Button(fr ? "Créer le mod et l'installer..." : "Make the mod and install it...", ImVec2(260, 36)))
      Make(fr, mods);
    ImGui::EndDisabled();
    if (Busy())
      DrawBusy(fr);
    if (message_ok_) {
      ImGui::SameLine();
      ImGui::BeginDisabled(game::IsRunning());
      if (ImGui::Button(fr ? "Tester en jeu" : "Test in game", ImVec2(160, 36)))
        arena_page::LaunchGame();
      ImGui::EndDisabled();
    }
    if (!name_[0]) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", fr ? "(donnez-lui un nom)" : "(give it a name)");
    }
    if (game::IsRunning())
      ImGui::TextColored(red, "%s", fr ? "Le jeu est lancé : fermez-le pour installer le mod."
                                       : "The game is running: close it to install the mod.");
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
  }

  // The mod: chosen where to save, written (manifest.txt, superstar/...), installed.
  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    message_.clear();
    const Star &who = stars12_[size_t(target_)];
    std::wstring file;
    std::string id = "moves_" + std::to_string(who.id) + "_";
    for (unsigned char c : std::string(name_)) {
      id += std::isalnum(c) ? char(std::tolower(c)) : '_';
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    }
    save_ = media::PickSave(fr, (file.empty() ? L"moves" : file) + L".svrmod");
    if (save_.empty())
      return;
    // (the moves: the port's ticked ones, then those changed by hand over them)
    std::map<int, int> all = PortMoves();
    for (const auto &[off, id] : edits_)
      all[off] = id;
    const bool moves = !all.empty(), entrance = use_entrance_ && HasEntrance(),
               victory = use_victory_ && HasVictory(), theme = use_theme_ && HasTheme(),
               other_entrance = !entrance && entrance_of_ > 0;
    std::string manifest = "game=wwe12\ntype=moves\nid=" + id + "\nname=" + std::string(name_) +
                           "\nsuperstar=" + std::to_string(who.id) + "\nversion=" + mod_version::Of(version_) + "\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    if (moves)  // (the game puts them over his own)
      manifest += "moves=" + moveset_port::ManifestValue(std::vector<std::pair<int, int>>(all.begin(), all.end())) + "\n";
    if (entrance)  // (superstar/entrance/, served by the game under that number)
      manifest += "entrance_own=" + std::to_string(parts_.entrance_number) + "\n";
    else if (other_entrance)  // (that superstar's entrance, music and video numbers: the game's own)
      manifest += "entrance=" + std::to_string(entrance_of_) + "\n";
    std::string what;
    for (const auto &[on, t] : {std::pair<bool, const char *>{moves, "moves"}, {entrance, "entrance"},
                                {victory, "victory scene"}, {theme, "theme"}})
      if (on)
        what += std::string(what.empty() ? "" : ", ") + t;
    std::string desc = who.name;
    if (!what.empty() && !pick_name_.empty())
      desc += " with the " + what + " of " + pick_name_ + " from " + star_port::GameName(game_) + " (the player's own copy)";
    if (!edits_.empty())
      desc += std::string(desc == who.name ? " with " : ", and ") + std::to_string(edits_.size()) + " move(s) changed by hand";
    if (other_entrance)
      for (const auto &st : stars12_)
        if (st.id == entrance_of_)
          desc += ", " + st.name + "'s entrance";
    manifest += "description=" + desc + ", made with the WWE '12 Mod Maker.\n";
    // (superstar/moves.txt: what it gives, for people; a mod needs a superstar/ file even with moves= alone)
    std::string notes = desc + "\n";
    for (const auto &[off, id] : all) {
      char line[200];
      snprintf(line, sizeof(line), "%03X  %s -> %s%s\n", off, cat_.Name(Own(off)).c_str(), cat_.Name(id).c_str(),
               edits_.count(off) ? "  (by hand)" : "");
      notes += line;
    }
    std::vector<std::pair<std::string, Bytes>> entries = {{"manifest.txt", Bytes(manifest.begin(), manifest.end())},
                                                          {"superstar/moves.txt", Bytes(notes.begin(), notes.end())}};
    if (entrance)
      for (const auto &part : parts_.entrance)
        entries.push_back({entrance13::ModPath(part), part.data});
    if (victory)  // (the game serves it as his own, 6000 + his number)
      for (const auto &part : parts_.victory)
        entries.push_back({"superstar/victory/" + part.kind + ".bin", part.data});
    if (entrance || victory)  // (their objects WWE '12 hasn't: the game adds them to evtobj.pac)
      for (const auto &part : parts_.objects)
        entries.push_back({entrance13::ModPath(part), part.data});
    if (theme) {  // (Wwise Vorbis as the game's, else PCM; with the entrance it is played as its number, else as 900 + his)
      Bytes pcm = star_port::ThemeWem(star_port::Vgmstream(discs_), theme_, entrance ? &parts_.entrance : nullptr,
                                      parts_.entrance_number, Bytes{}, entries);
      if (pcm.empty()) {
        message_ = fr ? "La musique n'a pas pu être convertie (vgmstream)." : "The theme could not be converted (vgmstream).";
        return;
      }
      entries.push_back({"superstar/theme.wem", std::move(pcm)});
    }
    std::set<int> used;  // (the moves the mod gives: an added one left unticked isn't brought)
    for (const auto &[off, id] : all)
      used.insert(id);
    if (moves && use_added_moves_)  // (his moves WWE '12 hasn't: the game adds them to its pack)
      for (const auto &m : parts_.new_moves) {
        if (!used.count(m.number))
          continue;
        const std::string n = "superstar/moves/" + std::to_string(m.number);
        entries.push_back({n + ".waze", m.waze});
        entries.push_back({n + "_low.yanm", m.low});
        entries.push_back({n + "_high.yanm", m.high});
        if (!m.magg.empty())
          entries.push_back({n + ".magg", m.magg});
        if (!m.m3.empty())
          entries.push_back({n + ".m3", m.m3});
        if (!m.mvmot.empty())
          entries.push_back({n + ".mvmot", m.mvmot});
      }
    if (!mods::Manager::WriteZip(save_, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save_.wstring());
      return;
    }
    mods.InstallFile(fr, save_);
    message_ok_ = true;
    message_ = (fr ? "Mod créé et installé : " : "Mod made and installed: ") + media::Utf8(save_.wstring()) +
               (fr ? " (au prochain lancement du jeu)" : " (next time the game starts)");
  }

  const disc::GameDisc *disc12_ = nullptr;  // WWE '12
  bool listed_ = false;
  std::vector<Star> stars12_;  // WWE '12's superstars, by name
  int step_ = 0;
  uint32_t game_ = 0;          // the game chosen (its title id), 0: none yet
  star_port::GameList list_;   // ... its superstars
  char filter_[64] = {};
  int pick_ = -1;              // his number in that game, -1: none yet
  std::string pick_name_;      // ... his name
  star_port::Parts parts_;     // what can be taken of him
  star_port::Music12 music12_;
  theme_source::Theme theme_;  // his theme (theme_source.h), stream empty if none
  std::vector<theme_source::Theme> themes_;
  int theme_pick_ = 0;
  bool tool_ = false;          // the player's vgmstream is there (the theme is made PCM with it)
  bool use_moves_ = true, use_entrance_ = true, use_victory_ = true, use_theme_ = true;
  bool use_added_moves_ = false;  // his moves WWE '12 hasn't, added (experimental, off by default: 2026-11-05)
  int target_ = -1;            // the WWE '12 superstar (index in stars12_), -1: none yet
  bool target_by_player_ = false;  // ... chosen by the player (else the one of the same name, proposed)
  int collected_for_ = -1;
  std::future<star_port::Parts> collecting_;  // his parts being read (Collect)
  std::function<void()> after_collect_;
  int collecting_for_ = -1;
  bool picking_ = false;  // the first read of a superstar just picked (steps 3 on wait for it)     // the target parts_ was collected with (2K16 / 2K17's effects)
  char target_filter_[64] = {};
  char name_[64] = {}, author_[64] = {}, version_[16] = {};
  fs::path save_;
  std::string message_;
  bool message_ok_ = false;
  // by hand
  ID3D11Device *device_ = nullptr;
  game_icons::Icons icons_;  // the games' icons, read from the discs in the background
  int tab_ = 0;                       // 0 moves, 1 taunts, 2 entrance, 3 from another game
  moves_edit::Catalog cat_;           // the game's moves and its roster's movesets
  moves_edit::MotionIndex motions_;   // where the moves' motions are (Play)
  bool cat_tried_ = false;
  move_preview::Preview preview_;
  std::map<int, int> edits_;          // slot (PRO offset) -> the move chosen by hand
  int edits_for_ = -1;                // the superstar they are for
  int entrance_of_ = 0;               // another superstar of the game whose entrance he gets, 0: his own
  std::set<int> port_skip_;           // the port's slots left out (unticked)
  char find_[64] = {}, pick_find_[64] = {}, ent_find_[64] = {};
  int test_play_ = 0;
  bool test_new_ = false;  // (test aid: a move to play once loaded)
};

}  // namespace moves_page
