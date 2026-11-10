// "Performance test" button: starts the game's built-in benchmark (wwe12_benchmark, written by Rendu) on a fresh
// data folder (Documents\wwe12_benchmark, emptied each time: the player's saves are never touched), waits for it to
// end and shows its summary (logs\benchmark_<date>_<time>.txt in that folder): the verdict, a few figures, and
// buttons to copy the text or open the folder. Same steps as tools/dev/lancer_benchmark.bat. Nothing is sent.
// One pass (a 1 vs 1 match with both entrances, then 30 s of match); the game closes by itself, so "finished" = no game process for a few seconds in a row.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <string>

#include "diagnostics.h"
#include "game_process.h"
#include "imgui.h"

namespace bench {

namespace fs = std::filesystem;

class Runner {
 public:
  // false = the button is greyed ("Coming soon"). Set to true only once the user has validated the result panel (1.3).
  static constexpr bool kAvailable = false;
  enum class State { Idle, Running, Done, Failed };

  bool Busy() const { return state_ == State::Running; }

  // What the "Apply the advised settings" button does (provided by the launcher, which owns the settings):
  //   apply(verdict)  changes the settings for the verdict ("cpu" / "gpu") and returns a one-line description of what
  //                   changed ("" = nothing to change). It keeps a backup, so undo() can put everything back.
  //   can_undo()      true while that backup exists.   undo()  restores it.
  struct Hooks {
    std::function<std::string(const std::string &verdict, bool fr)> apply;
    std::function<bool()> can_undo;
    std::function<void()> undo;
  };

  // exe: the game program the PLAY button would start. enabled: false when the game's files are missing.
  void Draw(bool fr, const fs::path &exe, const fs::path &exe_dir, const fs::path &documents, bool enabled,
            const Hooks &hooks = {}) {
    hooks_ = hooks;
    root_ = documents / "wwe12_benchmark";
    Poll();
    ImGui::BeginDisabled(!kAvailable || state_ == State::Running || !enabled);
    if (ImGui::Button(fr ? "Test de performance" : "Performance test"))
      Start(fr, exe, exe_dir);
    ImGui::EndDisabled();
    if (!kAvailable) {
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", fr ? "Bientôt disponible." : "Coming soon.");
      return;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      ImGui::SetTooltip("%s", fr ? "Lance le jeu, qui joue tout seul un match fixe (1 contre 1 avec entrées) pendant environ "
                                   "3 minutes 30 : ne touchez à rien. Le jeu se ferme tout seul à la fin. Le "
                                   "résultat dit si ça tourne bien et ce qui limite. Votre sauvegarde n'est pas touchée "
                                   "(dossier de test séparé). Rien n'est envoyé."
                                 : "Starts the game, which plays a fixed match by itself (1 vs 1 with entrances) for "
                                   "about 3.5 minutes: don't touch anything. The game closes by itself at the end. "
                                   "The result says how well it runs and what limits it. Your save is not touched "
                                   "(separate test folder). Nothing is sent.");
    switch (state_) {
    case State::Running:
      ImGui::SameLine();
      ImGui::TextDisabled("%s", fr ? "Test en cours... (environ 3 min 30, le jeu se ferme tout seul)"
                                   : "Running... (about 3.5 min, the game closes by itself)");
      break;
    case State::Failed:
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s",
                         fr ? "Le test n'a pas produit de résultat (jeu fermé trop tôt ?). Voir les journaux du jeu."
                            : "The test produced no result (game closed too early?). See the game's logs.");
      break;
    case State::Done:
      DrawResult(fr);
      break;
    default:
      break;
    }
  }

 private:
  void Start(bool fr, const fs::path &exe, const fs::path &exe_dir) {
    msg_.clear();
    if (game::IsRunning()) {
      state_ = State::Failed;
      return;
    }
    // A very weak PC (2 cores or fewer) will give a poor result and take longer: say so before starting.
    SYSTEM_INFO sys{};
    GetSystemInfo(&sys);
    const bool weak = sys.dwNumberOfProcessors <= 2;
    std::wstring ask = fr ? L"Cela lance le jeu, qui joue tout seul un match fixe (1 contre 1 avec entrées) pendant environ 3 minutes 30 : ne touchez "
                            L"à rien. Le jeu se ferme tout seul à la fin. Votre sauvegarde n'est pas touchée."
                          : L"This starts the game, which plays a fixed match by itself (1 vs 1 with entrances) for about 3.5 minutes: don't touch "
                            L"anything. The game closes by itself at the end. Your save is not touched.";
    if (weak)
      ask += fr ? L"\n\nVotre PC a 2 cœurs ou moins : le test sera long et le résultat probablement faible."
                : L"\n\nYour PC has 2 cores or fewer: the test will be slow and the result probably poor.";
    ask += fr ? L"\n\nContinuer ?" : L"\n\nContinue?";
    if (MessageBoxW(GetActiveWindow(), ask.c_str(), L"WWE '12 PC", MB_YESNO | MB_ICONQUESTION) != IDYES)
      return;
    results_.clear();
    advice_msg_.clear();
    std::error_code ec;
    if (root_.filename() != L"wwe12_benchmark")  // (never empty any other folder)
      return;
    fs::remove_all(root_, ec);
    const std::wstring path = exe.wstring();
    std::wstring cmd = L"\"" + path + L"\" --user_data_root \"" + root_.wstring() + L"\" --wwe12_benchmark=true";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(path.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, exe_dir.wstring().c_str(), &si,
                        &pi)) {
      state_ = State::Failed;
      return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    state_ = State::Running;
    started_ = Clock::now();
    last_seen_ = started_;
  }

  void Poll() {
    if (state_ != State::Running)
      return;
    const auto now = Clock::now();
    if (now - last_check_ < std::chrono::milliseconds(1000))
      return;
    last_check_ = now;
    if (game::IsRunning()) {
      last_seen_ = now;
      return;
    }
    // No game for a while: after the restart gap (a few seconds), the test is over or went wrong.
    if (now - last_seen_ < std::chrono::seconds(12) || now - started_ < std::chrono::seconds(15))
      return;
    text_ = Newest();
    ParseResults();
    state_ = (text_.empty() || results_.empty()) ? State::Failed : State::Done;
  }

  // The "[results]" block: one key=value per line (written by the game, see wwe12_benchmark.cpp).
  void ParseResults() {
    results_.clear();
    const size_t p = text_.find("[results]");
    if (p == std::string::npos)
      return;
    size_t pos = text_.find('\n', p);
    while (pos != std::string::npos && pos < text_.size()) {
      ++pos;
      const size_t e = text_.find('\n', pos);
      std::string line = text_.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
      while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
        line.pop_back();
      const size_t eq = line.find('=');
      if (eq != std::string::npos && eq > 0)
        results_[line.substr(0, eq)] = line.substr(eq + 1);
      pos = e;
    }
  }

  std::string Get(const char *key) const {
    const auto it = results_.find(key);
    return it == results_.end() ? std::string() : it->second;
  }
  double Num(const char *key) const {
    try {
      return std::stod(Get(key));
    } catch (...) {
      return 0;
    }
  }

  // The most recent finished summary (benchmark_<date>_<time>.txt, not the pass-1 temp file).
  std::string Newest() const {
    std::error_code ec;
    fs::path best;
    fs::file_time_type best_t{};
    for (const auto &e : fs::directory_iterator(root_ / "logs", ec)) {
      const std::wstring n = e.path().filename().wstring();
      if (n.rfind(L"benchmark_", 0) != 0 || e.path().extension() != L".txt")
        continue;
      const auto t = fs::last_write_time(e.path(), ec);
      if (best.empty() || t > best_t) {
        best = e.path();
        best_t = t;
      }
    }
    if (best.empty())
      return {};
    std::ifstream f(best, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
  }

  void DrawResult(bool fr) {
    const std::string verdict = Get("verdict");
    const double fps = Num("match_fps");
    const ImVec4 green(0.55f, 0.85f, 0.45f, 1.0f), gold(0.94f, 0.75f, 0.24f, 1.0f), red(1.0f, 0.45f, 0.35f, 1.0f);
    if (verdict == "failed") {
      ImGui::TextColored(red, "%s", fr ? "Le test n'a pas pu mesurer un match. Copiez le résultat et envoyez-le avec votre message."
                                       : "The test could not measure a match. Copy the result and send it with your message.");
    } else {
      // The simple note: smooth (60), playable, too slow.
      const bool smooth = fps >= 57.0, playable = fps >= 30.0;
      const char *note = smooth ? (fr ? "Fluide" : "Smooth") : playable ? (fr ? "Jouable" : "Playable") : (fr ? "Trop lent" : "Too slow");
      ImGui::TextColored(smooth ? green : playable ? gold : red, "%s : %.0f %s", note, fps, fr ? "images/s en match" : "fps in matches");
      ImGui::TextDisabled(fr ? "Pire image : %.0f ms  |  images > 20 ms : %s  |  chargement : %s s"
                             : "Worst frame: %.0f ms  |  frames > 20 ms: %s  |  loading: %s s",
                          Num("match_worst_ms"), Get("match_frames_over_20ms").c_str(), Get("load_s").c_str());
      if (verdict == "cpu")
        ImGui::TextWrapped("%s", fr ? "Ce qui limite : le processeur. Conseil : passer à 30 images/s en match (expérimental) "
                                      "et retirer la foule."
                                    : "What limits it: the processor. Advice: 30 fps in matches (experimental) and no crowd.");
      else if (verdict == "gpu")
        ImGui::TextWrapped("%s", fr ? "Ce qui limite : la carte graphique ou l'écran. Conseil : une résolution plus basse."
                                    : "What limits it: the graphics card or the display. Advice: a lower resolution.");
      else if (verdict == "ok")
        ImGui::TextWrapped("%s", fr ? "Rien à changer : le jeu tourne à 60 images/s en match." : "Nothing to change: the game runs at 60 fps in matches.");
      // The advised settings: only on an explicit click, always with a way back.
      if ((verdict == "cpu" || verdict == "gpu") && hooks_.apply) {
        if (ImGui::Button(fr ? "Appliquer les réglages conseillés" : "Apply the advised settings"))
          advice_msg_ = hooks_.apply(verdict, fr);
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("%s", fr ? "Change quelques réglages (indiqués après le clic). Vous pouvez tout remettre comme avant."
                                     : "Changes a few settings (listed after the click). You can put everything back.");
      }
      if (hooks_.can_undo && hooks_.can_undo()) {
        ImGui::SameLine();
        if (ImGui::Button(fr ? "Annuler les réglages conseillés" : "Undo the advised settings")) {
          hooks_.undo();
          advice_msg_ = fr ? "Réglages remis comme avant." : "Settings put back as before.";
        }
      }
      if (!advice_msg_.empty())
        ImGui::TextColored(green, "%s", advice_msg_.c_str());
    }
    ImGui::TextDisabled("%s | %s | %s", Get("cpu").c_str(), Get("gpu").c_str(), Get("api").c_str());
    if (ImGui::Button(fr ? "Copier le résultat" : "Copy the result"))
      msg_ = diag::CopyToClipboard(text_) ? (fr ? "Copié. Collez-le dans votre message." : "Copied. Paste it in your message.")
                                          : (fr ? "Copie impossible." : "Could not copy.");
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Ouvrir le dossier du résultat" : "Open the result folder")) {
      std::error_code ec;
      ShellExecuteW(nullptr, L"open", (root_ / "logs").wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    if (!msg_.empty())
      ImGui::TextColored(green, "%s", msg_.c_str());
  }

  using Clock = std::chrono::steady_clock;
  State state_ = State::Idle;
  fs::path root_;
  std::string text_, msg_, advice_msg_;
  std::map<std::string, std::string> results_;
  Hooks hooks_;
  Clock::time_point started_{}, last_seen_{}, last_check_{};
};

}  // namespace bench
