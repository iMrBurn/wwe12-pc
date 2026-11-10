// ROSTERS page: lists the wrestlers of the save with their brand (RAW / SmackDown / ...) and moves the chosen ones.
// Writing only touches the four brand bytes of chosen, consistent records and the checksum, after a backup, with a
// check and an undo. Format and rules: docs/launcher_rosters.md.
// File: <wwe12>\B13EBABEBABEBABE\54510890\00000001\SaveData.dat\SaveData.Dat, a plain file the game writes.
//   28-byte header = 7 big-endian u32 {0x4F490, 0x5EE90, 0x38, 0x13, 9, 2, sum}; sum = plain sum (mod 2^32) of the bytes
//   28 .. 28 + 0xAE358. Then the wrestlers' records, 286 bytes each: record(i) = 28 + i * 286, i = 0 .. 322, the wrestler's
//   number (50 .. 372) = i + 50, stored as a big-endian u16 at +40. Inside a record: +24..+27 brand (four equal bytes; the
//   Universe reads +27), +42 full name (ASCII), +178 short name, +234 gender.
// Brand values: 0 RAW, 1 SmackDown, 6 no brand ("Other": legends, DLC, free agents), 4 exists (19 wrestlers) with an unknown
// meaning: shown as "Other (4)", never written. A record with different brand bytes or a wrong number is "?" (untouchable).
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"

namespace rosters {

namespace fs = std::filesystem;

struct Row {
  int index = 0, id = 0;
  std::string name;
  int brand = -1;  // 0, 1, 4, 6, or -1 = "?" (inconsistent)
};

class Reader {
 public:
  static constexpr size_t kHeader = 28, kRecord = 286, kCount = 323, kSumSpan = 0xAE358;

  // Reads the file; returns "" when fine, else the reason (the list stays empty).
  std::string Load(const fs::path &file) {
    rows_.clear();
    sum_ok_ = false;
    empty_ = 0;
    try {
      std::error_code ec;
      if (!fs::exists(file, ec))
        return "nosave";
      std::ifstream f(file, std::ios::binary);
      const std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
      if (d.size() < kHeader + kSumSpan || d.size() < kHeader + kCount * kRecord)
        return "format";
      if (U32(d, 0) != 0x4F490 || U32(d, 4) != 0x5EE90 || U32(d, 8) != 0x38)
        return "format";
      uint32_t sum = 0;
      for (size_t i = kHeader; i < kHeader + kSumSpan; ++i)
        sum += d[i];
      sum_ok_ = sum == U32(d, 24);
      for (size_t i = 0; i < kCount; ++i) {
        const size_t o = kHeader + i * kRecord;
        Row r;
        r.index = int(i);
        r.id = (d[o + 40] << 8) | d[o + 41];
        for (size_t k = o + 42; k < o + 42 + 60 && d[k] >= 32 && d[k] < 127; ++k)
          r.name += char(d[k]);
        const uint8_t b = d[o + 27];
        const bool same = d[o + 24] == b && d[o + 25] == b && d[o + 26] == b;
        if (same && (b == 0 || b == 1 || b == 4 || b == 6) && r.id == int(i) + 50)
          r.brand = b;
        if (r.name == "0")
          r.name.clear();  // (the game stores "0" for wrestlers whose name comes from its own text tables)
        if (r.name.empty() && r.id == 0)
        {
          ++empty_;  // an empty slot (number 0, no name): not listed, never written
          continue;
        }
        if (r.name.empty())
          r.name = "(#" + std::to_string(r.id) + ")";
        rows_.push_back(std::move(r));
      }
      return sum_ok_ ? "" : "sum";
    } catch (...) {
      rows_.clear();
      return "format";
    }
  }

  const std::vector<Row> &Rows() const { return rows_; }
  bool SumOk() const { return sum_ok_; }
  int Empty() const { return empty_; }

 private:
  static uint32_t U32(const std::vector<uint8_t> &d, size_t o) {
    return (uint32_t(d[o]) << 24) | (uint32_t(d[o + 1]) << 16) | (uint32_t(d[o + 2]) << 8) | d[o + 3];
  }
  std::vector<Row> rows_;
  bool sum_ok_ = false;
  int empty_ = 0;
};

inline const char *BrandName(int brand, bool fr) {
  switch (brand) {
  case 0: return "RAW";
  case 1: return "SmackDown";
  case 6: return fr ? "Sans marque" : "No brand";
  case 4: return fr ? "Autre (4)" : "Other (4)";
  default: return "?";
  }
}

// ---- Writing (session 2). Only the four brand bytes +24..+27 of chosen, consistent records are changed, then the sum. ----

inline bool ReadAll(const fs::path &file, std::vector<uint8_t> &d) {
  try {
    std::ifstream f(file, std::ios::binary);
    if (!f)
      return false;
    d.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
  } catch (...) {
    return false;
  }
}

inline uint32_t ReadU32(const std::vector<uint8_t> &d, size_t o) {
  return (uint32_t(d[o]) << 24) | (uint32_t(d[o + 1]) << 16) | (uint32_t(d[o + 2]) << 8) | d[o + 3];
}

inline uint32_t SumOf(const std::vector<uint8_t> &d) {
  uint32_t sum = 0;
  for (size_t i = Reader::kHeader; i < Reader::kHeader + Reader::kSumSpan; ++i)
    sum += d[i];
  return sum;
}

// Header as expected and the stored sum matches: the only files this page writes.
inline bool Valid(const std::vector<uint8_t> &d) {
  return d.size() >= Reader::kHeader + Reader::kSumSpan && d.size() >= Reader::kHeader + Reader::kCount * Reader::kRecord &&
         ReadU32(d, 0) == 0x4F490 && ReadU32(d, 4) == 0x5EE90 && ReadU32(d, 8) == 0x38 && SumOf(d) == ReadU32(d, 24);
}

// Writes next to the file then replaces it (never a half-written save).
inline bool WriteAtomic(const fs::path &file, const std::vector<uint8_t> &d) {
  try {
    fs::path tmp = file;
    tmp += L".rosters.tmp";
    {
      std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
      f.write(reinterpret_cast<const char *>(d.data()), std::streamsize(d.size()));
      f.flush();
      if (!f)
        return false;
    }
    std::error_code ec;
    fs::rename(tmp, file, ec);  // replaces an existing file
    if (ec) {
      fs::remove(tmp, ec);
      return false;
    }
    return true;
  } catch (...) {
    return false;
  }
}

// A record may be changed only when it is consistent: number (u16 big-endian at +40) = index + 50, four equal brand
// bytes, and a brand we know how to write (0, 1, 6). Brand 4 and "?" records are never touched.
inline bool Changeable(const std::vector<uint8_t> &d, size_t i) {
  const size_t o = Reader::kHeader + i * Reader::kRecord;
  const int id = (d[o + 40] << 8) | d[o + 41];
  const uint8_t b = d[o + 27];
  return id == int(i) + 50 && d[o + 24] == b && d[o + 25] == b && d[o + 26] == b && (b == 0 || b == 1 || b == 6);
}

class Editor {
 public:
  // Returns "" when done, else why not: "read" / "format" (not a save we know), "record" (a chosen record is not
  // consistent), "write", "verify" (re-read differs: the original was put back).
  std::string Apply(const fs::path &file, const std::vector<std::pair<int, int>> &changes) {
    std::vector<uint8_t> d;
    if (!ReadAll(file, d))
      return "read";
    if (!Valid(d))
      return "format";
    const std::vector<uint8_t> before = d;
    for (const auto &c : changes) {
      if (c.first < 0 || c.first >= int(Reader::kCount) || !Changeable(d, size_t(c.first)) ||
          !(c.second == 0 || c.second == 1 || c.second == 6))
        return "record";
      const size_t o = Reader::kHeader + size_t(c.first) * Reader::kRecord;
      for (int k = 24; k <= 27; ++k)
        d[o + k] = uint8_t(c.second);
    }
    const uint32_t sum = SumOf(d);
    d[24] = uint8_t(sum >> 24), d[25] = uint8_t(sum >> 16), d[26] = uint8_t(sum >> 8), d[27] = uint8_t(sum);
    if (!WriteAtomic(file, d))
      return "write";
    std::vector<uint8_t> check;
    if (!ReadAll(file, check) || check != d || !Valid(check)) {
      WriteAtomic(file, before);
      return "verify";
    }
    before_ = before;
    after_sum_ = sum;
    return {};
  }

  bool CanUndo() const { return !before_.empty(); }

  // Puts the save back as it was before the last change, only if nothing else changed it since (the game saved).
  std::string Undo(const fs::path &file) {
    std::vector<uint8_t> d;
    if (before_.empty() || !ReadAll(file, d) || !Valid(d))
      return "read";
    if (SumOf(d) != after_sum_)
      return "changed";
    if (!WriteAtomic(file, before_))
      return "write";
    before_.clear();
    return {};
  }

 private:
  std::vector<uint8_t> before_;
  uint32_t after_sum_ = 0;
};

class Page {
 public:
  struct Env {
    bool can_write = false;                            // the game is closed and no other save operation runs
    std::function<std::string(std::string &)> backup;  // makes a backup of the save; returns its name ("" = failed)
  };

  void Draw(bool fr, const fs::path &save_file, const Env &env) {
    if (!loaded_ || ImGui::Button(fr ? "Relire la sauvegarde" : "Reload the save"))
      Reload(save_file);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", fr ? "Un catcheur n'est modifié que si vous cliquez sur « Appliquer »."
                                 : "A wrestler is changed only when you click \"Apply\".");
    ImGui::Dummy(ImVec2(0, 4));
    if (reason_ == "nosave") {
      ImGui::TextWrapped("%s", fr ? "Aucune sauvegarde trouvée. Lancez le jeu une fois et enregistrez."
                                  : "No save found. Start the game once and save.");
      return;
    }
    if (reason_ == "format") {
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s",
                         fr ? "Cette sauvegarde n'a pas le format attendu : la liste n'est pas affichée."
                            : "This save does not have the expected format: the list is not shown.");
      return;
    }
    const bool sum_ok = reason_ != "sum";
    if (!sum_ok)
      ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s",
                         fr ? "La somme de contrôle de la sauvegarde ne correspond pas : la liste est en lecture seule."
                            : "The save's checksum does not match: the list is read only.");
    if (!env.can_write)
      ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s",
                         fr ? "Le jeu est ouvert (ou une opération de sauvegarde tourne) : fermez-le pour modifier."
                            : "The game is open (or a save operation is running): close it to make changes.");

    int count[3] = {0, 0, 0}, other = 0, unknown = 0;  // RAW, SmackDown, no brand; Other (4); ?
    for (const Row &r : reader_.Rows()) {
      if (r.brand == 0) ++count[0];
      else if (r.brand == 1) ++count[1];
      else if (r.brand == 6) ++count[2];
      else if (r.brand == 4) ++other;
      else ++unknown;
    }
    ImGui::Text("RAW %d   SmackDown %d   %s %d   %s %d   ?  %d   (%s %d)", count[0], count[1], fr ? "Sans marque" : "No brand",
                count[2], fr ? "Autre (4)" : "Other (4)", other, unknown, fr ? "emplacements vides" : "empty slots",
                reader_.Empty());
    if (count[0] < 15 || count[1] < 15)
      ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s",
                         fr ? "Le mode Univers veut au moins 15 catcheurs par émission."
                            : "Universe mode wants at least 15 wrestlers on each show.");

    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##find", fr ? "Chercher un nom" : "Search a name", find_, sizeof(find_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    const char *filters[] = {fr ? "Toutes les marques" : "All brands", "RAW", "SmackDown", fr ? "Sans marque" : "No brand",
                             fr ? "Autre (4)" : "Other (4)", "?"};
    ImGui::Combo("##brand", &filter_, filters, 6);

    // The move: tick wrestlers below, choose the brand, apply.
    const bool can_apply = sum_ok && env.can_write && !selected_.empty();
    ImGui::Text("%s: %d", fr ? "Cochés" : "Ticked", int(selected_.size()));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    const char *targets[] = {"RAW", "SmackDown", fr ? "Sans marque" : "No brand"};
    ImGui::Combo("##target", &target_, targets, 3);
    ImGui::SameLine();
    ImGui::BeginDisabled(!can_apply);
    if (ImGui::Button(fr ? "Appliquer..." : "Apply..."))
      ImGui::OpenPopup("##confirm");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Tout décocher" : "Untick all"))
      selected_.clear();
    ImGui::SameLine();
    ImGui::BeginDisabled(!(env.can_write && editor_.CanUndo()));
    if (ImGui::Button(fr ? "Annuler le dernier changement" : "Undo the last change")) {
      const std::string e = editor_.Undo(save_file);
      if (e.empty())
        msg_ = fr ? "Dernier changement annulé." : "Last change undone.";
      else if (e == "changed")
        msg_ = fr ? "La sauvegarde a changé depuis : rien n'a été annulé (onglet SAVES pour restaurer une copie)."
                  : "The save changed since: nothing was undone (SAVES tab to restore a backup).";
      else
        msg_ = fr ? "Annulation impossible." : "Could not undo.";
      msg_ok_ = e.empty();
      Reload(save_file);
    }
    ImGui::EndDisabled();
    DrawConfirm(fr, save_file, env, count);
    if (!msg_.empty())
      ImGui::TextColored(msg_ok_ ? ImVec4(0.55f, 0.85f, 0.45f, 1.0f) : ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", msg_.c_str());

    std::string needle = find_;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    static const int kFilterBrand[] = {-2, 0, 1, 6, 4, -1};
    if (ImGui::BeginTable("rosters", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                            ImGuiTableFlags_Sortable | ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0, 0))) {
      ImGui::TableSetupColumn("##tick", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 34);
      ImGui::TableSetupColumn(fr ? "Nom" : "Name", ImGuiTableColumnFlags_DefaultSort);
      ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn(fr ? "Marque" : "Brand", ImGuiTableColumnFlags_WidthFixed, 160);
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableHeadersRow();
      std::vector<const Row *> shown;
      for (const Row &r : reader_.Rows()) {
        if (kFilterBrand[filter_] != -2 && r.brand != kFilterBrand[filter_])
          continue;
        if (!needle.empty()) {
          std::string n = r.name;
          std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return (char)std::tolower(c); });
          if (n.find(needle) == std::string::npos)
            continue;
        }
        shown.push_back(&r);
      }
      if (const ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsCount > 0) {
        const int col = specs->Specs[0].ColumnIndex;
        const bool asc = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
        std::stable_sort(shown.begin(), shown.end(), [&](const Row *a, const Row *b) {
          if (col == 2) return asc ? a->id < b->id : a->id > b->id;
          if (col == 3) return asc ? a->brand < b->brand : a->brand > b->brand;
          return asc ? a->name < b->name : a->name > b->name;
        });
      }
      for (const Row *r : shown) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const bool ok = r->brand == 0 || r->brand == 1 || r->brand == 6;  // (Other (4) and ? are never changed)
        bool on = selected_.count(r->index) > 0;
        ImGui::PushID(r->index);
        ImGui::BeginDisabled(!ok);
        if (ImGui::Checkbox("##t", &on)) {
          if (on)
            selected_.insert(r->index);
          else
            selected_.erase(r->index);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r->name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%d", r->id);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(BrandName(r->brand, fr));
      }
      ImGui::EndTable();
    }
  }

 private:
  void Reload(const fs::path &save_file) {
    reason_ = reader_.Load(save_file);
    loaded_ = true;
    selected_.clear();
  }

  void DrawConfirm(bool fr, const fs::path &save_file, const Env &env, const int count[3]) {
    if (!ImGui::BeginPopupModal("##confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
      return;
    static const int kBrandOf[] = {0, 1, 6};
    const int target = kBrandOf[target_];
    // What the shows will look like afterwards (for the 15-wrestler warning).
    int after[3] = {count[0], count[1], count[2]};
    int moved = 0;
    for (const Row &r : reader_.Rows()) {
      if (!selected_.count(r.index) || r.brand == target)
        continue;
      ++moved;
      after[r.brand == 0 ? 0 : r.brand == 1 ? 1 : 2]--;
      after[target == 0 ? 0 : target == 1 ? 1 : 2]++;
    }
    ImGui::Text(fr ? "Mettre %d catcheur(s) sur : %s ?" : "Put %d wrestler(s) on: %s?", moved, BrandName(target, fr));
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 34.0f);
    ImGui::TextWrapped("%s", fr ? "Une copie de votre sauvegarde est faite d'abord (onglet SAVES). Gardez le jeu fermé."
                                : "A backup of your save is made first (SAVES tab). Keep the game closed.");
    ImGui::TextWrapped("%s", fr ? "Dans le mode Univers, un catcheur déplacé peut encore apparaître une fois sur un carton déjà "
                                  "placé, et un champion garde son titre. Le « Reset » de l'Univers ne remet pas les catcheurs "
                                  "ajoutés à leur marque d'origine."
                                : "In Universe mode, a moved wrestler may still appear once on an already booked card, and a "
                                  "champion keeps the title. Universe's \"Reset\" does not put added wrestlers back on their "
                                  "original brand.");
    if (after[0] < 15 || after[1] < 15)
      ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s %d / %d",
                         fr ? "Attention : moins de 15 catcheurs sur une émission (RAW / SmackDown) :"
                            : "Warning: fewer than 15 wrestlers on a show (RAW / SmackDown):",
                         after[0], after[1]);
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::BeginDisabled(!env.can_write || moved == 0);
    if (ImGui::Button(fr ? "Appliquer" : "Apply", ImVec2(140, 0))) {
      std::vector<std::pair<int, int>> changes;
      for (const Row &r : reader_.Rows())
        if (selected_.count(r.index) && r.brand != target)
          changes.push_back({r.index, target});
      std::string err;
      const std::string made = env.backup ? env.backup(err) : std::string();
      if (made.empty()) {
        msg_ = fr ? "Impossible de faire la copie de sécurité : rien n'a été changé."
                  : "Could not make the backup: nothing was changed.";
        msg_ok_ = false;
      } else {
        const std::string e = editor_.Apply(save_file, changes);
        msg_ok_ = e.empty();
        if (e.empty())
          msg_ = (fr ? "Fait. Copie de sécurité : " : "Done. Backup: ") + made;
        else if (e == "verify")
          msg_ = fr ? "La vérification a échoué : la sauvegarde d'origine a été remise."
                    : "The check failed: the original save was put back.";
        else
          msg_ = (fr ? "Rien n'a été changé (" : "Nothing was changed (") + e + ").";
        Reload(save_file);
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Annuler" : "Cancel", ImVec2(140, 0)))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  Reader reader_;
  Editor editor_;
  bool loaded_ = false, msg_ok_ = false;
  std::string reason_, msg_;
  char find_[64] = "";
  int filter_ = 0, target_ = 0;
  std::set<int> selected_;
};

}  // namespace rosters
