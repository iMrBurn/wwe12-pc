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
#include <string>
#include <utility>
#include <vector>

#include "arena_page.h"
#include "game_disc.h"
#include "game_discs.h"
#include "game_process.h"
#include "imgui.h"
#include "media.h"
#include "mod_version.h"
#include "mods.h"
#include "star_port.h"
#include "ui_style.h"

namespace moves_page {

namespace fs = std::filesystem;
using svrfmt::Bytes;

class Page {
 public:
  const game_discs::Discs *discs_ = nullptr;  // My game discs (set by the Mod Maker)
  bool want_discs_ = false;                   // the player asked for My game discs (the Mod Maker opens it)

  void Draw(bool fr, const disc::GameDisc &disc, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    disc12_ = &disc;
    ImGui::TextWrapped("%s", fr ? "Donnez à un catcheur de WWE '12 les prises, l'entrée, la scène de victoire et la musique "
                                  "d'un catcheur de vos autres jeux. Le mod se coupe dans le launcher : le catcheur "
                                  "retrouve les siennes."
                                : "Give a WWE '12 superstar the moves, entrance, victory scene and theme of a superstar of "
                                  "your other games. Turn the mod off in the launcher: the superstar gets his own back.");
    if (!disc.Found()) {
      ImGui::TextColored(red, "%s", fr ? "Fichiers du jeu introuvables." : "Game files not found.");
      return;
    }
    if (!listed_)
      List(disc);
    // The assistant: the game, its superstar, what to take, for whom, the mod.
    const char *steps_fr[] = {"1. Jeu", "2. Catcheur", "3. Ce qu'on prend", "4. Pour qui", "5. Terminer"};
    const char *steps_en[] = {"1. Game", "2. Superstar", "3. What to take", "4. For whom", "5. Finish"};
    const int reach = !GameSet() ? 0 : pick_ < 0 ? 1 : !AnyTaken() ? 2 : target_ < 0 ? 3 : 4;
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
    ImGui::BeginChild("mv_body", ImVec2(0, h), ImGuiChildFlags_Borders);
    ImGui::PushTextWrapPos(0);
    switch (step_) {
    case 0: StepGame(fr); break;
    case 1: StepSuperstar(fr); break;
    case 2: StepTake(fr); break;
    case 3: StepTarget(fr); break;
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
      if (f.size() != 9 || _strnicmp(f.c_str(), "ch", 2) || _stricmp(f.c_str() + 5, ".pac") || !isdigit(f[2]))
        continue;
      Star s;
      s.id = atoi(f.c_str() + 2);
      const auto it = names.find(s.id);
      if (it == names.end())
        continue;
      s.name = it->second;
      stars12_.push_back(s);
    }
    std::sort(stars12_.begin(), stars12_.end(), [](const Star &a, const Star &b) { return a.name < b.name; });
  }

  // Step 1: the game, a big tile each (from My game discs).
  void StepGame(bool fr) {
    ImGui::TextWrapped("%s", fr ? "De quel jeu vient le catcheur ? Les jeux de MES DISQUES sont prêts ; ajoutez les autres "
                                  "là-bas une fois pour toutes."
                                : "Which game does the superstar come from? The games of MY GAME DISCS are ready; add the "
                                  "others there once and for all.");
    ImGui::Dummy(ImVec2(0, 4));
    const float tile_w = 250, tile_h = 132, gap = ImGui::GetStyle().ItemSpacing.x;
    const int per_row = std::max(1, int((ImGui::GetContentRegionAvail().x + gap) / (tile_w + gap)));
    int shown = 0;
    for (const auto &k : game_discs::kKnown) {
      if (k.title == 0x54510890 || k.superstars == game_discs::Can::kNo)  // (WWE '12 itself)
        continue;
      if (shown++ % per_row)
        ImGui::SameLine();
      const bool have = discs_ && !discs_->PathOf(k.title).empty(), chosen = game_ == k.title;
      ImGui::PushID(int(k.title));
      ImGui::PushStyleColor(ImGuiCol_ChildBg, chosen ? ImVec4(0.30f, 0.22f, 0.08f, 0.85f)
                                                     : have ? ImVec4(0.12f, 0.12f, 0.14f, 0.9f)
                                                            : ImVec4(0.08f, 0.08f, 0.09f, 0.7f));
      ImGui::BeginChild("tile", ImVec2(tile_w, tile_h), ImGuiChildFlags_Borders);
      ImGui::PushFont(ui::g_font_title, 28.0f);
      ImGui::TextColored(have ? ImVec4(0.94f, 0.75f, 0.24f, 1.0f) : ImVec4(0.55f, 0.53f, 0.5f, 1.0f), "%s",
                         star_port::GameName(k.title));
      ImGui::PopFont();
      ImGui::TextDisabled("%s", star_port::SvrOld(k.title)
                                    ? (fr ? "entrée, victoire, musique" : "entrance, victory, theme")
                                    : (fr ? "prises, entrée, victoire, musique" : "moves, entrance, victory, theme"));
      if (have) {
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f), "%s", fr ? "Votre disque est prêt" : "Your disc is set");
        if (ImGui::Button(chosen ? (fr ? "Choisi" : "Chosen") : (fr ? "Choisir" : "Choose"), ImVec2(-1, 0)))
          ChooseGame(k.title);
      } else {
        ImGui::TextDisabled("%s", fr ? "Pas encore de disque" : "No disc yet");
        if (ImGui::Button(fr ? "L'ajouter dans MES DISQUES" : "Add it in MY GAME DISCS", ImVec2(-1, 0)))
          want_discs_ = true;
      }
      ImGui::EndChild();
      ImGui::PopStyleColor();
      ImGui::PopID();
    }
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
      if (ImGui::Selectable(label, n == pick_) && n != pick_)
        Pick(n, nm);
    }
    ImGui::EndChild();
    if (pick_ >= 0)
      ImGui::TextDisabled(fr ? "Choisi : %s" : "Chosen: %s", pick_name_.c_str());
  }

  // His parts (star_port::Collect) and theme; what can be taken ticked; WWE '12's superstar of the same name proposed.
  void Pick(int number, const std::string &name) {
    pick_ = number;
    pick_name_ = name.size() > 2 && name.compare(name.size() - 2, 2, " ?") == 0 ? name.substr(0, name.size() - 2) : name;
    Collect();
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
  }

  void Collect() {
    if (pick_ < 0 || !discs_ || !disc12_)
      return;
    std::string listed;
    for (const auto &[n, nm] : list_.list)
      if (n == pick_)
        listed = nm;
    star_port::Collect(*disc12_, *discs_, game_, pick_, listed, target_ >= 0 ? stars12_[size_t(target_)].id : -1,
                       music12_, parts_);
    collected_for_ = target_;
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
    snprintf(label, sizeof(label),
             fr ? "Ses prises (%d cases sur %d trouvées dans WWE '12 ; les autres restent celles du catcheur)"
                : "His moves (%d slots of %d found in WWE '12; the others stay the superstar's own)",
             parts_.moves.exact + parts_.moves.variant, int(parts_.moves.rows.size()));
    Choice(HasMoves(), use_moves_, HasMoves() ? label : (fr ? "Ses prises" : "His moves"), MovesWhy(fr));
    if (use_moves_ && HasMoves()) {
      ImGui::Indent(24);
      star_port::DrawMovesTable(fr, parts_.moves, game_, "(stays)", "(inchangée)");
      if (!parts_.new_moves.empty())
        ImGui::TextDisabled(fr ? "(%zu prises que WWE '12 n'a pas, ajoutées par le jeu ; essai)"
                               : "(%zu moves WWE '12 hasn't, added by the game; a test)",
                            parts_.new_moves.size());
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
    Collect();
    use_moves_ = m && HasMoves();
    use_entrance_ = e && HasEntrance();
    use_victory_ = v && HasVictory();
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
    const bool ready = name_[0] && target_ >= 0 && AnyTaken();
    ImGui::BeginDisabled(!ready || game::IsRunning());
    if (ImGui::Button(fr ? "Créer le mod et l'installer..." : "Make the mod and install it...", ImVec2(260, 36)))
      Make(fr, mods);
    ImGui::EndDisabled();
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
    const bool moves = use_moves_ && HasMoves(), entrance = use_entrance_ && HasEntrance(),
               victory = use_victory_ && HasVictory(), theme = use_theme_ && HasTheme();
    std::string manifest = "game=wwe12\ntype=moves\nid=" + id + "\nname=" + std::string(name_) +
                           "\nsuperstar=" + std::to_string(who.id) + "\nversion=" + mod_version::Of(version_) + "\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    if (moves)  // (the game puts them over his own)
      manifest += "moves=" + moveset_port::ManifestValue(parts_.moves.moves) + "\n";
    if (entrance)  // (superstar/entrance/, served by the game under that number)
      manifest += "entrance_own=" + std::to_string(parts_.entrance_number) + "\n";
    std::string what;
    for (const auto &[on, t] : {std::pair<bool, const char *>{moves, "moves"}, {entrance, "entrance"},
                                {victory, "victory scene"}, {theme, "theme"}})
      if (on)
        what += std::string(what.empty() ? "" : ", ") + t;
    manifest += "description=" + who.name + " with the " + what + " of " + pick_name_ + " from " +
                star_port::GameName(game_) + " (the player's own copy), made with the WWE '12 Mod Maker.\n";
    // (superstar/moves.txt: what it gives, for people; a mod needs a superstar/ file even with moves= alone)
    std::string notes = pick_name_ + " (" + star_port::GameName(game_) + ") -> " + who.name + " (" +
                        std::to_string(who.id) + "): " + what + "\n";
    if (moves)
      for (const auto &r : parts_.moves.rows)
        if (!r.to.empty()) {
          char line[160];
          snprintf(line, sizeof(line), "%03X  %s -> %s\n", r.off, r.from.c_str(), r.to.c_str());
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
    if (theme) {  // (Wwise PCM; with the entrance it is played as its number, else as 900 + his)
      Bytes pcm = star_port::ThemeWem(star_port::Vgmstream(discs_), theme_, entrance ? &parts_.entrance : nullptr,
                                      parts_.entrance_number, Bytes{}, entries);
      if (pcm.empty()) {
        message_ = fr ? "La musique n'a pas pu être convertie (vgmstream)." : "The theme could not be converted (vgmstream).";
        return;
      }
      entries.push_back({"superstar/theme.wem", std::move(pcm)});
    }
    if (moves)  // (his moves WWE '12 hasn't: the game adds them to its pack)
      for (const auto &m : parts_.new_moves) {
        const std::string n = "superstar/moves/" + std::to_string(m.number);
        entries.push_back({n + ".waze", m.waze});
        entries.push_back({n + "_low.yanm", m.low});
        entries.push_back({n + "_high.yanm", m.high});
        if (!m.magg.empty())
          entries.push_back({n + ".magg", m.magg});
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
  int target_ = -1;            // the WWE '12 superstar (index in stars12_), -1: none yet
  bool target_by_player_ = false;  // ... chosen by the player (else the one of the same name, proposed)
  int collected_for_ = -1;     // the target parts_ was collected with (2K16 / 2K17's effects)
  char target_filter_[64] = {};
  char name_[64] = {}, author_[64] = {}, version_[16] = {};
  fs::path save_;
  std::string message_;
  bool message_ok_ = false;
};

}  // namespace moves_page
