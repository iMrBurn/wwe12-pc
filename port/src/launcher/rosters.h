// ROSTERS page, session 1 (READ ONLY): lists the wrestlers of the save with their brand (RAW / SmackDown / ...).
// Nothing is written: the page only reads SaveData.Dat. Writing (session 2) is described in docs/launcher_rosters.md.
//
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
#include <iterator>
#include <string>
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
        if (r.name.empty())
          r.name = "(" + std::to_string(r.id) + ")";
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

 private:
  static uint32_t U32(const std::vector<uint8_t> &d, size_t o) {
    return (uint32_t(d[o]) << 24) | (uint32_t(d[o + 1]) << 16) | (uint32_t(d[o + 2]) << 8) | d[o + 3];
  }
  std::vector<Row> rows_;
  bool sum_ok_ = false;
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

class Page {
 public:
  // saves_file: the save's SaveData.Dat. game_running: the game is open (a later session refuses to write then).
  void Draw(bool fr, const fs::path &save_file) {
    if (!loaded_ || ImGui::Button(fr ? "Relire la sauvegarde" : "Reload the save")) {
      reason_ = reader_.Load(save_file);
      loaded_ = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", fr ? "Lecture seule : rien n'est modifié." : "Read only: nothing is changed.");
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
    if (reason_ == "sum")
      ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s",
                         fr ? "La somme de contrôle de la sauvegarde ne correspond pas : la liste est en lecture seule."
                            : "The save's checksum does not match: the list is read only.");

    int count[3] = {0, 0, 0}, other = 0, unknown = 0;  // RAW, SmackDown, no brand; Other (4); ?
    for (const Row &r : reader_.Rows()) {
      if (r.brand == 0) ++count[0];
      else if (r.brand == 1) ++count[1];
      else if (r.brand == 6) ++count[2];
      else if (r.brand == 4) ++other;
      else ++unknown;
    }
    ImGui::Text("RAW %d   SmackDown %d   %s %d   %s %d   ?  %d", count[0], count[1], fr ? "Sans marque" : "No brand",
                count[2], fr ? "Autre (4)" : "Other (4)", other, unknown);
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

    std::string needle = find_;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    static const int kFilterBrand[] = {-2, 0, 1, 6, 4, -1};
    if (ImGui::BeginTable("rosters", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                            ImGuiTableFlags_Sortable | ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0, 0))) {
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
          if (col == 1) return asc ? a->id < b->id : a->id > b->id;
          if (col == 2) return asc ? a->brand < b->brand : a->brand > b->brand;
          return asc ? a->name < b->name : a->name > b->name;
        });
      }
      for (const Row *r : shown) {
        ImGui::TableNextRow();
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
  Reader reader_;
  bool loaded_ = false;
  std::string reason_;
  char find_[64] = "";
  int filter_ = 0;
};

}  // namespace rosters
