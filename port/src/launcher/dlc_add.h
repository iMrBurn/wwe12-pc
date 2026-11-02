// "Add DLC..." button: copies the packages the player picks into the dlc folder next to the exe. The game
// installs what is in that folder at its next start (src/wwe12_dlc.cpp, not touched here). Before copying,
// each file is checked to be an STFS package of marketplace content (type 2) for WWE '12; the others are
// refused with the reason. A file already in the folder is replaced only if the player says so.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "imgui.h"
#include "stfs_check.h"

namespace dlcadd {

namespace fs = std::filesystem;

class Adder {
 public:
  ~Adder() { Wait(); }
  void Wait() {
    if (worker_.joinable())
      worker_.join();
  }
  bool Busy() const { return busy_.load(); }

  // The button and the result lines. `exe_dir`: the folder of the exe (the dlc folder is next to it).
  void Draw(bool fr, const fs::path &exe_dir) {
    ImGui::BeginDisabled(busy_.load());
    if (ImGui::Button(fr ? "Ajouter des DLC..." : "Add DLC..."))
      PickAndCopy(fr, exe_dir / "dlc");
    ImGui::EndDisabled();
    std::lock_guard lock(mutex_);
    if (!lines_.empty()) {
      ImGui::PushTextWrapPos(0);
      for (size_t i = 0; i < lines_.size(); ++i)
        ImGui::TextColored(i == 0 && ok_ ? ImVec4(0.45f, 0.85f, 0.45f, 1.0f) : ImVec4(1.0f, 0.45f, 0.35f, 1.0f),
                           "%s", lines_[i].c_str());
      ImGui::PopTextWrapPos();
    }
  }

 private:
  static std::string Name(const fs::path &f) {
    const std::u8string u8 = f.filename().u8string();  // ImGui draws UTF-8
    return std::string(u8.begin(), u8.end());
  }

  static std::string Why(bool fr, const stfs::Result &r) {
    char buf[160];
    switch (r.verdict) {
    case stfs::Verdict::Archive:
      return fr ? "archive (zip, rar ou 7z) : décompressez-la d'abord" : "an archive (zip, rar or 7z): unpack it first";
    case stfs::Verdict::DiskImage:
      return fr ? "image complète d'un support de console, pas un paquet"
                : "a whole console storage image, not one package";
    case stfs::Verdict::TooSmall: return fr ? "trop petit pour être un paquet" : "too small to be a package";
    case stfs::Verdict::Unreadable: return fr ? "impossible à ouvrir" : "could not be opened";
    case stfs::Verdict::WrongGame:
      snprintf(buf, sizeof(buf), fr ? "paquet d'un autre jeu (titre %08X)" : "a package of another game (title %08X)",
               r.title);
      return buf;
    case stfs::Verdict::WrongType:
      snprintf(buf, sizeof(buf), fr ? "pas un DLC (type de contenu %08X)" : "not a DLC (content type %08X)", r.type);
      return buf;
    default: return fr ? "pas un paquet Xbox 360 (STFS)" : "not an Xbox 360 package (STFS)";
    }
  }

  void PickAndCopy(bool fr, const fs::path &dlc_dir) {
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
      return;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_ALLOWMULTISELECT);
    std::vector<fs::path> picked;
    if (SUCCEEDED(dlg->Show(GetActiveWindow()))) {
      IShellItemArray *items = nullptr;
      if (SUCCEEDED(dlg->GetResults(&items))) {
        DWORD n = 0;
        items->GetCount(&n);
        for (DWORD i = 0; i < n; ++i) {
          IShellItem *item = nullptr;
          if (SUCCEEDED(items->GetItemAt(i, &item))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
              picked.emplace_back(p);
              CoTaskMemFree(p);
            }
            item->Release();
          }
        }
        items->Release();
      }
    }
    dlg->Release();
    if (picked.empty())
      return;

    // Checks first, so the player is asked about replacing only for files that would really be copied.
    std::vector<fs::path> good;
    std::vector<std::string> lines;
    std::error_code ec;
    int refused = 0;
    for (const fs::path &f : picked) {
      const stfs::Result r = stfs::Check(f, stfs::kTypeMarketplace);
      if (r.verdict != stfs::Verdict::Ok) {
        lines.push_back(Name(f) + ": " + Why(fr, r));
        ++refused;
        continue;
      }
      const fs::path dest = dlc_dir / f.filename();
      if (fs::exists(dest, ec)) {
        if (fs::equivalent(f, dest, ec))
          continue;  // already the file in the folder
        const std::wstring q = (fr ? L"Ce DLC est déjà dans le dossier dlc :\n" : L"This DLC is already in the dlc folder:\n") +
                               f.filename().wstring() + (fr ? L"\n\nLe remplacer ?" : L"\n\nReplace it?");
        if (MessageBoxW(GetActiveWindow(), q.c_str(), L"WWE12 Launcher", MB_YESNO | MB_ICONQUESTION) != IDYES)
          continue;
      }
      good.push_back(f);
    }
    if (good.empty()) {
      SetResult(false, fr ? "Aucun DLC ajouté." : "No DLC added.", lines);
      return;
    }
    Wait();
    busy_ = true;
    worker_ = std::thread([this, fr, dlc_dir, good, lines, refused]() mutable {
      std::error_code cec;
      fs::create_directories(dlc_dir, cec);
      int added = 0;
      for (const fs::path &f : good) {
        const fs::path dest = dlc_dir / f.filename();
        fs::path tmp = dest;
        tmp += L".part";
        fs::copy_file(f, tmp, fs::copy_options::overwrite_existing, cec);
        if (!cec) {
          fs::remove(dest, cec);
          fs::rename(tmp, dest, cec);
        }
        if (cec) {
          fs::remove(tmp, cec);
          lines.push_back(Name(f) + ": " + (fr ? "copie impossible" : "copy failed"));
        } else {
          ++added;
        }
      }
      char head[200];
      if (added > 0)
        snprintf(head, sizeof(head),
                 fr ? "%d DLC ajouté(s), %d refusé(s). Ils seront installés au prochain lancement du jeu."
                    : "%d DLC added, %d refused. They are installed the next time you start the game.",
                 added, refused);
      else
        snprintf(head, sizeof(head), "%s", fr ? "Aucun DLC ajouté." : "No DLC added.");
      SetResult(added > 0, head, lines);
      busy_ = false;
    });
  }

  void SetResult(bool ok, const std::string &head, const std::vector<std::string> &details) {
    std::lock_guard lock(mutex_);
    ok_ = ok;
    lines_.clear();
    lines_.push_back(head);
    lines_.insert(lines_.end(), details.begin(), details.end());
  }

  std::thread worker_;
  std::atomic<bool> busy_{false};
  std::mutex mutex_;
  bool ok_ = false;
  std::vector<std::string> lines_;
};

}  // namespace dlcadd
