// "Performance test" button: starts the game's built-in benchmark (wwe12_benchmark, written by Rendu) on a fresh
// data folder (Documents\wwe12_benchmark, emptied each time: the player's saves are never touched), waits for it to
// end and shows its summary (logs\benchmark_<date>_<time>.txt in that folder): the verdict, a few figures, and
// buttons to copy the text or open the folder. Same steps as tools/dev/lancer_benchmark.bat. Nothing is sent.
// The game restarts itself between the two passes, so "finished" = no game process for a few seconds in a row.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "diagnostics.h"
#include "game_process.h"
#include "imgui.h"

namespace bench {

namespace fs = std::filesystem;

class Runner {
 public:
  enum class State { Idle, Running, Done, Failed };

  bool Busy() const { return state_ == State::Running; }

  // exe: the game program the PLAY button would start. enabled: false when the game's files are missing.
  void Draw(bool fr, const fs::path &exe, const fs::path &exe_dir, const fs::path &documents, bool enabled) {
    root_ = documents / "wwe12_benchmark";
    Poll();
    ImGui::BeginDisabled(state_ == State::Running || !enabled);
    if (ImGui::Button(fr ? "Test de performance" : "Performance test"))
      Start(fr, exe, exe_dir);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      ImGui::SetTooltip("%s", fr ? "Lance le jeu : vous choisissez un match d'exhibition à la main, puis le test mesure l'entrée et "
                                   "30 s de match sans que vous touchiez à rien. Le jeu se relance une fois : choisissez "
                                   "le même match. Le résultat dit si ça tourne bien et ce qui limite. Votre sauvegarde "
                                   "n'est pas touchée (dossier de test séparé)."
                                 : "Starts the game: you pick an exhibition match by hand, then the test measures the "
                                   "entrance and 30 s of match, hands off. The game restarts once: pick the same match. "
                                   "The result says how well it runs and what limits it. Your save is not touched "
                                   "(separate test folder).");
    switch (state_) {
    case State::Running:
      ImGui::SameLine();
      ImGui::TextDisabled("%s", fr ? "Test en cours... (le jeu se ferme et se relance seul une fois)"
                                   : "Running... (the game closes and restarts by itself once)");
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
    if (MessageBoxW(GetActiveWindow(),
                    fr ? L"Cela lance le jeu. Choisissez un match d'exhibition à la main (PLAY, EXHIBITION...) : dès que "
                         L"l'entrée commence, le test mesure seul. Le jeu se relance ensuite une fois : choisissez le "
                         L"même match. Votre sauvegarde n'est pas touchée. Continuer ?"
                       : L"This starts the game. Pick an exhibition match by hand (PLAY, EXHIBITION...): as soon as the "
                         L"entrance starts, the test measures by itself. The game then restarts once: pick the same "
                         L"match. Your save is not touched. Continue?",
                    L"WWE '12 PC", MB_YESNO | MB_ICONQUESTION) != IDYES)
      return;
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
    state_ = text_.empty() ? State::Failed : State::Done;
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
    // The verdict line is the one the player wants; the whole text is for the bug report.
    std::string verdict;
    size_t p = text_.find("Verdict (pass 2):");
    if (p != std::string::npos) {
      const size_t e = text_.find('\n', p);
      verdict = text_.substr(p, e == std::string::npos ? std::string::npos : e - p);
    }
    ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s", verdict.empty() ? (fr ? "Test terminé." : "Test finished.") : verdict.c_str());
    // First line (version, date) and the machine line, as a short reminder of what was measured.
    size_t pos = 0;
    for (int i = 0; i < 3 && pos < text_.size(); ++i) {
      const size_t e = text_.find('\n', pos);
      const std::string line = text_.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
      if (!line.empty())
        ImGui::TextDisabled("%s", line.c_str());
      if (e == std::string::npos)
        break;
      pos = e + 1;
    }
    if (ImGui::Button(fr ? "Copier le résultat" : "Copy the result"))
      msg_ = diag::CopyToClipboard(text_) ? (fr ? "Copié. Collez-le dans votre message." : "Copied. Paste it in your message.")
                                          : (fr ? "Copie impossible." : "Could not copy.");
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Ouvrir le dossier du résultat" : "Open the result folder")) {
      std::error_code ec;
      ShellExecuteW(nullptr, L"open", (root_ / "logs").wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    if (!msg_.empty())
      ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f), "%s", msg_.c_str());
  }

  using Clock = std::chrono::steady_clock;
  State state_ = State::Idle;
  fs::path root_;
  std::string text_, msg_;
  Clock::time_point started_{}, last_seen_{}, last_check_{};
};

}  // namespace bench
