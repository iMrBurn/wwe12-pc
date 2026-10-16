// SAVES page: backups of the player's save (create, restore, delete, start from scratch).
//
// The game keeps its save like an Xbox 360 does, under Documents\wwe12:
//   B13EBABEBABEBABE\54510890\...      the save package (00000001\SaveData.dat) and its header
//   achievements\54510890.toml         unlocked achievements
// (B13EBABEBABEBABE is the runtime's fixed local profile, 54510890 is WWE '12.) A backup mirrors
// exactly those two paths under SaveBackups\<date>\, so restoring is a copy back. The DLC
// (0000000000000000\...) and the caches are not part of it. Every destructive action first makes
// an automatic backup of the current save; the latest kKeepAutomatic of those are kept.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "game_process.h"
#include "imgui.h"

namespace saves {

namespace fs = std::filesystem;

struct Texts {
  const char *intro, *open_folder, *current, *none, *saved_at, *label_hint, *create, *fresh,
      *backups, *no_backups, *restore, *del, *confirm_restore, *confirm_delete, *confirm_fresh,
      *yes, *cancel, *game_running, *done_created, *done_restored, *done_deleted, *done_fresh,
      *nothing, *failed, *working, *auto_label, *empty_backup, *col_date, *col_name, *col_size,
      *restored_plain, *import_title, *import_intro, *import_folder, *import_file, *import_none,
      *import_confirm, *import_ready, *import_pending, *import_cancel, *import_result;
};

const Texts kEnglish = {
    "Your game progress lives in Documents\\wwe12. A backup copies your save and your "
    "achievements (not the DLC). Restoring, deleting or starting over always makes an "
    "automatic backup of your current save first.",
    "Open the saves folder", "Current save", "No save yet: play and save in the game.",
    "%s, last saved %s", "Backup name (optional)", "Create a backup", "Start from scratch",
    "Backups", "No backups yet.", "Restore", "Delete",
    "Restore this backup? Your current save is backed up first.",
    "Delete this backup for good?",
    "Delete your current save and start from scratch? It is backed up first, so you can restore it.",
    "Yes", "Cancel", "The game is running: close it to manage your saves.",
    "Backup created:", "Backup restored. Your previous save was kept as", "Backup deleted.",
    "Save removed. It was kept as", "Nothing to back up yet.", "Failed:", "Working...",
    "automatic, before a change", "This backup contains no save.", "Date", "Name", "Size",
    "Backup restored.",
    "Import an Xbox 360 save",
    "Pick a save copied from your Xbox 360: a single file or a whole folder (for example one "
    "made of 00EncodeDat.enc, 00CreateSuperStar.cas and SaveData.dat files). Packages with the "
    "same name as yours are replaced, your other saves stay.",
    "Import a folder...", "Import a file...", "No WWE '12 save found there.",
    "Found %d save package(s), %s. They are installed the next time you start the game "
    "(it can take a moment); packages with the same name are replaced. Your current save is "
    "backed up first.",
    "Ready: the save is installed the next time you start the game.",
    "Import waiting for the next game start.", "Cancel the import",
    "Last import: %d installed, %d failed."};

const Texts kFrench = {
    "Votre progression est dans Documents\\wwe12. Une copie contient votre sauvegarde et vos "
    "succès (pas les DLC). Restaurer, supprimer ou repartir de zéro fait toujours d'abord une "
    "copie automatique de votre sauvegarde actuelle.",
    "Ouvrir le dossier des sauvegardes", "Sauvegarde actuelle",
    "Pas encore de sauvegarde : jouez et sauvegardez dans le jeu.",
    "%s, enregistrée le %s", "Nom de la copie (facultatif)", "Créer une copie",
    "Repartir de zéro", "Copies de sauvegarde", "Aucune copie pour l'instant.", "Restaurer",
    "Supprimer", "Restaurer cette copie ? Votre sauvegarde actuelle est copiée d'abord.",
    "Supprimer cette copie définitivement ?",
    "Supprimer votre sauvegarde actuelle et repartir de zéro ? Elle est copiée d'abord, vous "
    "pourrez la restaurer.",
    "Oui", "Annuler", "Le jeu est lancé : fermez-le pour gérer vos sauvegardes.",
    "Copie créée :", "Copie restaurée. Votre sauvegarde précédente est gardée sous le nom",
    "Copie supprimée.", "Sauvegarde supprimée. Elle est gardée sous le nom",
    "Rien à copier pour l'instant.", "Échec :", "En cours...", "automatique, avant un changement",
    "Cette copie ne contient pas de sauvegarde.", "Date", "Nom", "Taille",
    "Copie restaurée.",
    "Importer une sauvegarde Xbox 360",
    "Choisissez une sauvegarde copiée depuis votre Xbox 360 : un fichier, ou un dossier entier "
    "(par exemple fait de fichiers 00EncodeDat.enc, 00CreateSuperStar.cas et SaveData.dat). Les "
    "paquets du même nom que les vôtres sont remplacés, vos autres sauvegardes restent.",
    "Importer un dossier...", "Importer un fichier...",
    "Aucune sauvegarde de WWE '12 trouvée à cet endroit.",
    "%d paquet(s) de sauvegarde trouvé(s), %s. Ils sont installés au prochain lancement du jeu "
    "(ça peut prendre un moment) ; les paquets du même nom sont remplacés. Votre sauvegarde "
    "actuelle est copiée d'abord.",
    "Prêt : la sauvegarde sera installée au prochain lancement du jeu.",
    "Import en attente du prochain lancement du jeu.", "Annuler l'import",
    "Dernier import : %d installé(s), %d en échec."};

struct Backup {
  std::string name;   // folder name, "YYYY-MM-DD_HH-MM-SS"
  std::string label;  // text the player gave (empty for automatic ones)
  bool automatic = false;
  std::string when;   // "YYYY-MM-DD HH:MM:SS"
  uintmax_t size = 0;
};

class Manager {
 public:
  void Init(const fs::path &documents_wwe12, const fs::path &exe_dir) {
    root_ = documents_wwe12;
    exe_dir_ = exe_dir;
  }

  // Waits for a running operation (the launcher is closing).
  void Wait() {
    if (worker_.joinable())
      worker_.join();
  }
  // A backup, restore, delete or import is running: the game must not start now.
  bool Busy() const { return busy_.load(); }

  void Draw(bool french) {
    const Texts &t = french ? kFrench : kEnglish;
    Refresh();
    ImGui::TextWrapped("%s", t.intro);
    ImGui::Dummy(ImVec2(0, 4));
    if (ImGui::Button(t.open_folder)) {
      std::error_code ec;
      fs::create_directories(root_, ec);
      ShellExecuteW(nullptr, L"open", root_.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Text("%s", t.current);
    if (has_active_)
      ImGui::TextDisabled(t.saved_at, FormatSize(active_size_).c_str(), active_when_.c_str());
    else
      ImGui::TextDisabled("%s", t.none);

    const bool busy = busy_.load();
    const bool blocked = busy || game_running_;
    if (game_running_)
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", t.game_running);

    ImGui::Dummy(ImVec2(0, 4));
    ImGui::BeginDisabled(blocked);
    ImGui::SetNextItemWidth(280);
    ImGui::InputTextWithHint("##label", t.label_hint, label_, sizeof(label_));
    ImGui::SameLine();
    if (ImGui::Button(t.create))
      Start(&t, Op::Create, {});
    ImGui::SameLine();
    if (ImGui::Button(t.fresh))
      Ask(Op::Fresh, {});
    ImGui::EndDisabled();

    {
      std::lock_guard lock(mutex_);
      if (busy)
        ImGui::TextDisabled("%s", t.working);
      else if (!message_.empty())
        ImGui::TextColored(message_ok_ ? ImVec4(0.55f, 0.85f, 0.45f, 1.0f)
                                       : ImVec4(1.0f, 0.45f, 0.35f, 1.0f),
                           "%s", message_.c_str());
    }

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Text("%s", t.import_title);
    ImGui::TextWrapped("%s", t.import_intro);
    ImGui::BeginDisabled(blocked || import_pending_);
    if (ImGui::Button(t.import_folder))
      PickAndScan(true);
    ImGui::SameLine();
    if (ImGui::Button(t.import_file))
      PickAndScan(false);
    ImGui::EndDisabled();
    if (import_pending_) {
      ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s", t.import_pending);
      ImGui::SameLine();
      if (ImGui::Button(t.import_cancel)) {
        std::error_code ec;
        fs::remove(exe_dir_ / "saves_import.txt", ec);
        import_pending_ = false;
      }
    }
    if (import_scan_msg_ == 1)
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", t.import_none);
    if (!import_result_.empty())
      ImGui::TextDisabled(t.import_result, import_installed_, import_failed_);

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Text("%s", t.backups);
    ImGui::BeginChild("backups", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (backups_.empty())
      ImGui::TextDisabled("%s", t.no_backups);
    else if (ImGui::BeginTable("backup_table", 4, ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn(t.col_date, ImGuiTableColumnFlags_WidthFixed, 170.0f);
      ImGui::TableSetupColumn(t.col_name, ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn(t.col_size, ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, 240.0f);
      for (const Backup &b : backups_) {
        ImGui::PushID(b.name.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(b.when.c_str());
        ImGui::TableNextColumn();
        if (b.automatic)
          ImGui::TextDisabled("%s", t.auto_label);
        else
          ImGui::TextUnformatted(b.label.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(FormatSize(b.size).c_str());
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(blocked);
        if (ImGui::Button(t.restore))
          Ask(Op::Restore, b.name);
        ImGui::SameLine();
        if (ImGui::Button(t.del))
          Ask(Op::Delete, b.name);
        ImGui::EndDisabled();
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    ImGui::EndChild();

    if (ask_) {
      ImGui::OpenPopup("##confirm");
      ask_ = false;
    }
    if (ask_import_) {
      ImGui::OpenPopup("##confirm_import");
      ask_import_ = false;
    }
    if (ImGui::BeginPopupModal("##confirm_import", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28);
      ImGui::TextWrapped(t.import_confirm, import_count_, FormatSize(import_size_).c_str());
      ImGui::PopTextWrapPos();
      ImGui::Dummy(ImVec2(0, 6));
      if (ImGui::Button(t.yes)) {
        const std::u8string u8 = import_path_.u8string();  // the game reads the list as UTF-8
        Start(&t, Op::Import, std::string(u8.begin(), u8.end()));
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button(t.cancel))
        ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("##confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      const char *question = pending_ == Op::Restore  ? t.confirm_restore
                             : pending_ == Op::Delete ? t.confirm_delete
                                                      : t.confirm_fresh;
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28);
      ImGui::TextWrapped("%s", question);
      ImGui::PopTextWrapPos();
      ImGui::Dummy(ImVec2(0, 6));
      if (ImGui::Button(t.yes)) {
        Start(&t, pending_, pending_name_);
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button(t.cancel))
        ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
  }

 private:
  enum class Op { Create, Restore, Delete, Fresh, Import };
  static constexpr int kKeepAutomatic = 10;
  static constexpr const char *kProfile = "B13EBABEBABEBABE";
  static constexpr const char *kTitle = "54510890";

  fs::path SaveDir() const { return root_ / kProfile / kTitle; }
  fs::path AchFile() const { return root_ / "achievements" / (std::string(kTitle) + ".toml"); }
  fs::path BackupsDir() const { return root_ / "SaveBackups"; }

  void Ask(Op op, const std::string &name) {
    pending_ = op;
    pending_name_ = name;
    ask_ = true;
  }

  static std::string FormatSize(uintmax_t bytes) {
    char buf[32];
    if (bytes >= (1u << 20))
      snprintf(buf, sizeof(buf), "%.1f MB", double(bytes) / double(1u << 20));
    else
      snprintf(buf, sizeof(buf), "%.0f KB", double(bytes) / 1024.0);
    return buf;
  }

  static uintmax_t DirSize(const fs::path &p) {
    uintmax_t total = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(p, ec); it != fs::recursive_directory_iterator();
         it.increment(ec)) {
      if (ec)
        break;
      if (it->is_regular_file(ec))
        total += it->file_size(ec);
    }
    return total;
  }

  static std::string FormatFileTime(fs::file_time_type ft) {
    const std::time_t tt =
        std::chrono::system_clock::to_time_t(std::chrono::clock_cast<std::chrono::system_clock>(ft));
    std::tm tm{};
    localtime_s(&tm, &tt);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
  }

  static uint32_t ReadU32BE(const unsigned char *p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
  }

  // A WWE '12 save package: an STFS file ("CON ", "LIVE" or "PIRS") of content type 1 (saved
  // game) for title 54510890 (type at 0x344, title id at 0x360, big-endian).
  static bool IsWweSavePackage(const fs::path &file) {
    std::ifstream f(file, std::ios::binary);
    unsigned char h[0x364] = {};
    if (!f.read(reinterpret_cast<char *>(h), sizeof(h)))
      return false;
    const bool stfs = !memcmp(h, "CON ", 4) || !memcmp(h, "LIVE", 4) || !memcmp(h, "PIRS", 4);
    return stfs && ReadU32BE(h + 0x344) == 1 && ReadU32BE(h + 0x360) == 0x54510890;
  }

  // Asks for a folder or a file, then counts the WWE '12 save packages in it.
  void PickAndScan(bool folder) {
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg))))
      return;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | (folder ? FOS_PICKFOLDERS : 0));
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
    if (picked.empty())
      return;
    import_count_ = 0;
    import_size_ = 0;
    std::error_code ec;
    size_t looked_at = 0;
    auto consider = [&](const fs::path &f) {
      const uintmax_t size = fs::file_size(f, ec);
      if (ec || size < 0x364)  // too small to be a package: no need to open it
        return;
      if (IsWweSavePackage(f)) {
        ++import_count_;
        import_size_ += size;
      }
    };
    if (fs::is_directory(picked, ec)) {
      for (auto it = fs::recursive_directory_iterator(picked, ec);
           it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec || ++looked_at > 20000)  // a whole disk picked by mistake: stop looking
          break;
        if (it->is_regular_file(ec))
          consider(it->path());
      }
    } else {
      consider(picked);
    }
    if (import_count_ == 0) {
      import_scan_msg_ = 1;
      return;
    }
    import_scan_msg_ = 0;
    import_path_ = picked;
    ask_import_ = true;
  }

  static bool GameRunning() { return game::IsRunning(); }

  // The list and the "current save" card, rebuilt after each operation and every few seconds.
  void Refresh() {
    using namespace std::chrono;
    const auto now = steady_clock::now();
    if (busy_ || (!dirty_ && now - last_refresh_ < seconds(3)))
      return;
    dirty_ = false;
    last_refresh_ = now;
    game_running_ = GameRunning();
    std::error_code pec;
    import_pending_ = fs::exists(exe_dir_ / "saves_import.txt", pec);
    if (std::ifstream res{exe_dir_ / "saves_import_result.txt"}) {  // written by the game
      std::string line;
      import_installed_ = import_failed_ = 0;
      while (std::getline(res, line)) {
        if (line.rfind("installed=", 0) == 0)
          import_installed_ = atoi(line.c_str() + 10);
        else if (line.rfind("failed=", 0) == 0)
          import_failed_ = atoi(line.c_str() + 7);
      }
      import_result_ = "x";
    }

    std::error_code ec;
    has_active_ = fs::exists(SaveDir(), ec);
    if (has_active_) {
      active_size_ = DirSize(SaveDir());
      fs::file_time_type newest{};
      for (auto it = fs::recursive_directory_iterator(SaveDir(), ec);
           it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec)
          break;
        if (it->is_regular_file(ec))
          newest = std::max(newest, it->last_write_time(ec));
      }
      active_when_ = FormatFileTime(newest);
    }
    backups_ = List();
  }

  std::vector<Backup> List() const {
    std::vector<Backup> out;
    std::error_code ec;
    for (const auto &e : fs::directory_iterator(BackupsDir(), ec)) {
      if (!e.is_directory(ec))
        continue;
      Backup b;
      b.name = e.path().filename().string();
      b.when = b.name;
      if (b.name.size() >= 19) {  // 2026-10-13_10-29-42 -> 2026-10-13 10:29:42
        b.when = b.name.substr(0, 10) + " " + b.name.substr(11, 8);
        std::replace(b.when.begin() + 11, b.when.end(), '-', ':');
      }
      std::ifstream info(e.path() / "backup.txt");
      std::string kind;
      if (std::getline(info, kind)) {
        b.automatic = kind == "auto";
        std::getline(info, b.label);
      }
      auto cached = size_cache_.find(b.name);  // a finished backup never changes
      b.size = cached != size_cache_.end() ? cached->second : (size_cache_[b.name] = DirSize(e.path()));
      out.push_back(std::move(b));
    }
    std::sort(out.begin(), out.end(), [](const Backup &a, const Backup &b) { return a.name > b.name; });
    return out;
  }

  void SetMessage(bool ok, const std::string &text) {
    std::lock_guard lock(mutex_);
    message_ok_ = ok;
    message_ = text;
  }

  // Copies the save and the achievements into a new backup folder. Returns its name, or "" with
  // the reason in `err` (empty `err`: there was nothing to back up).
  std::string MakeBackup(bool automatic, const std::string &label, std::string &err) {
    std::error_code ec;
    const bool has_save = fs::exists(SaveDir(), ec), has_ach = fs::exists(AchFile(), ec);
    if (!has_save && !has_ach)
      return {};
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &tm);
    std::string name = stamp;
    for (int n = 2; fs::exists(BackupsDir() / name, ec); ++n)
      name = std::string(stamp) + "_" + std::to_string(n);
    const fs::path dest = BackupsDir() / name;
    fs::create_directories(dest, ec);
    if (!ec && has_save) {
      fs::create_directories(dest / kProfile, ec);
      if (!ec)
        fs::copy(SaveDir(), dest / kProfile / kTitle,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    }
    if (!ec && has_ach) {
      fs::create_directories(dest / "achievements", ec);
      if (!ec)
        fs::copy_file(AchFile(), dest / "achievements" / AchFile().filename(),
                      fs::copy_options::overwrite_existing, ec);
    }
    if (ec) {
      err = ec.message();
      std::error_code ignore;
      fs::remove_all(dest, ignore);
      return {};
    }
    std::ofstream(dest / "backup.txt") << (automatic ? "auto" : "manual") << "\n" << label << "\n";
    if (automatic)
      PruneAutomatic();
    return name;
  }

  void PruneAutomatic() {
    int seen = 0;
    std::error_code ec;
    for (const Backup &b : List())  // newest first
      if (b.automatic && ++seen > kKeepAutomatic)
        fs::remove_all(BackupsDir() / b.name, ec);
  }

  void Start(const Texts *t, Op op, std::string name) {
    if (busy_.exchange(true))
      return;
    Wait();
    std::string label = label_;
    if (op == Op::Create)
      label_[0] = '\0';
    if (op == Op::Import)
      import_result_.clear();  // here, not in the worker: the UI thread reads it
    worker_ = std::thread([this, t, op, name, label] {
      Run(*t, op, name, label);
      dirty_ = true;
      busy_ = false;
    });
  }

  void Run(const Texts &t, Op op, const std::string &name, const std::string &label) {
    if (GameRunning()) {
      SetMessage(false, t.game_running);
      return;
    }
    std::error_code ec;
    std::string err;
    switch (op) {
    case Op::Create: {
      const std::string made = MakeBackup(false, label, err);
      if (!err.empty())
        SetMessage(false, std::string(t.failed) + " " + err);
      else if (made.empty())
        SetMessage(false, t.nothing);
      else {
        SetMessage(true, std::string(t.done_created) + " " + made);
      }
      break;
    }
    case Op::Import: {
      const std::string kept = MakeBackup(true, {}, err);
      if (!err.empty()) {  // could not protect the current save: change nothing
        SetMessage(false, std::string(t.failed) + " " + err);
        break;
      }
      std::ofstream list(exe_dir_ / "saves_import.txt", std::ios::binary | std::ios::trunc);
      list << name << "\n";
      list.close();
      if (!list) {
        SetMessage(false, std::string(t.failed) + " saves_import.txt");
        break;
      }
      std::error_code rec;
      fs::remove(exe_dir_ / "saves_import_result.txt", rec);
      SetMessage(true, t.import_ready);
      break;
    }
    case Op::Delete: {
      if (!ValidName(name))
        return;
      fs::remove_all(BackupsDir() / name, ec);
      size_cache_.erase(name);
      SetMessage(!ec, ec ? std::string(t.failed) + " " + ec.message() : std::string(t.done_deleted));
      break;
    }
    case Op::Fresh: {
      const std::string kept = MakeBackup(true, {}, err);
      if (!err.empty()) {  // no copy, no deletion
        SetMessage(false, std::string(t.failed) + " " + err);
        break;
      }
      if (kept.empty()) {
        SetMessage(false, t.nothing);
        break;
      }
      fs::remove_all(SaveDir(), ec);
      SetMessage(!ec, ec ? std::string(t.failed) + " " + ec.message()
                         : std::string(t.done_fresh) + " " + kept);
      break;
    }
    case Op::Restore: {
      if (!ValidName(name))
        return;
      const fs::path src = BackupsDir() / name;
      const bool has_save = fs::exists(src / kProfile / kTitle, ec);
      const bool has_ach = fs::exists(src / "achievements" / AchFile().filename(), ec);
      if (!has_save && !has_ach) {
        SetMessage(false, t.empty_backup);
        break;
      }
      const std::string kept = MakeBackup(true, {}, err);
      if (!err.empty()) {  // could not protect the current save: change nothing
        SetMessage(false, std::string(t.failed) + " " + err);
        break;
      }
      if (has_save) {
        fs::remove_all(SaveDir(), ec);
        if (!ec)
          fs::create_directories(SaveDir().parent_path(), ec);
        if (!ec)
          fs::copy(src / kProfile / kTitle, SaveDir(),
                   fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
      }
      if (!ec && has_ach) {
        fs::create_directories(AchFile().parent_path(), ec);
        if (!ec)
          fs::copy_file(src / "achievements" / AchFile().filename(), AchFile(),
                        fs::copy_options::overwrite_existing, ec);
      }
      if (ec)
        SetMessage(false, std::string(t.failed) + " " + ec.message() +
                              (kept.empty() ? "" : "  (" + kept + ")"));
      else
        SetMessage(true, kept.empty() ? std::string(t.restored_plain)
                                      : std::string(t.done_restored) + " " + kept);
      break;
    }
    }
  }

  static bool ValidName(const std::string &name) {
    return !name.empty() && fs::path(name).filename() == fs::path(name) && name != "." && name != "..";
  }

  fs::path root_;
  fs::path exe_dir_;
  std::thread worker_;
  std::atomic<bool> busy_{false};
  std::atomic<bool> dirty_{true};
  std::mutex mutex_;
  std::string message_;
  bool message_ok_ = true;
  char label_[64] = {};

  std::chrono::steady_clock::time_point last_refresh_{};
  bool game_running_ = false;
  bool has_active_ = false;
  uintmax_t active_size_ = 0;
  std::string active_when_;
  std::vector<Backup> backups_;
  mutable std::map<std::string, uintmax_t> size_cache_;  // backup name -> size

  bool import_pending_ = false;  // saves_import.txt waits for the next game start
  std::string import_result_;    // non-empty once the game has written its summary
  int import_installed_ = 0, import_failed_ = 0;
  int import_scan_msg_ = 0;      // 1: nothing usable in the last pick
  int import_count_ = 0;
  uintmax_t import_size_ = 0;
  fs::path import_path_;
  bool ask_import_ = false;

  bool ask_ = false;
  Op pending_ = Op::Create;
  std::string pending_name_;
};

}  // namespace saves
