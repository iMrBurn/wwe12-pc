// CAW page: the player's Created Superstars (after the CAW pictures page and caw_import of Kaiko's SvR 2011 Mod Maker:
// caw_page.cpp, launcher/caw_import.c; credit in docs/mod_maker.md). The picture the game shows for a CAW in its lists
// (made at FINALIZE) replaced from any picture; a CAW exported to a file (.wwe12caw) and imported into another save.
//
// WWE '12's files (measured on a CAW of the user's save, 2026-10-30; Kaiko's SvR 2011 offsets moved):
//  - Documents\wwe12\B13EBABEBABEBABE\54510890\00000001\NNCreateSuperStar.cas\SaveData.Dat (1,357,028 bytes; NN = the
//    slot, its header's display name "NN+1.CUSTOM SUPERSTAR"): a checksum, big-endian, at +12 and again at +0x14B254
//    and at the end (+0x14B4E0); four attire slots of 149,528 bytes at 755,044 + attire x 149,528, each a small table
//    (0x220 bytes) then the 256 x 512 render (DXT5, 16-bit words swapped, no header) and at +0x20414 the 128 x 128
//    thumb; the CAW's record copied at +0x14A7D4 (its name at +0x2A). The checksum is a sum of the file's fields: the
//    pictures are summed as plain bytes (Kaiko), so changing them moves it by the change of their byte sum.
//  - its record in SaveData.dat: 0x6E4 bytes at 0x1B9E8 + slot x 0x6E4 (the .cas's copy up to +0x574, then the save's
//    own part, +0x684 the .cas's checksum: the game checks it); SaveData.dat's own checksum (+0x18) is the byte sum of
//    0x1C .. 0x1C + its two section sizes (big-endian at +0 and +4). Slot size and count (50) are Kaiko's SvR 2011 ones
//    moved by WWE '12's longer record: not yet seen with a second CAW.
// Every change copies the save first (Documents\wwe12\SaveBackups, as the launcher's automatic backups).
#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "arena_page.h"
#include "imgui.h"
#include "media.h"
#include "mods.h"
#include "svrfmt/texture.h"
#include "wic_image.h"

namespace caw_page {

namespace fs = std::filesystem;
using svrfmt::Bytes;

constexpr size_t kCasSize = 1357028, kSlotAt = 755044, kSlotSize = 149528, kRenderAt = 0x220, kRenderBytes = 131072,
                 kThumbAt = 0x20414, kThumbBytes = 16384, kRecInCas = 0x14A7D4, kSum1 = 12, kSum2 = 0x14B254,
                 kSum3 = 0x14B4E0, kRecBase = 0x1B9E8, kRecSize = 0x6E4, kRecShared = 0x574, kRecSum = 0x684, kName = 0x2A;
constexpr int kMaxCaw = 50, kAttires = 4;

inline uint32_t Be32(const Bytes &b, size_t at) {
  return uint32_t(b[at]) << 24 | uint32_t(b[at + 1]) << 16 | uint32_t(b[at + 2]) << 8 | b[at + 3];
}
inline void PutBe32(Bytes &b, size_t at, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b[at + size_t(i)] = uint8_t(v >> (24 - 8 * i));
}
inline uint64_t Sum(const Bytes &b, size_t from, size_t n) {
  uint64_t s = 0;
  for (size_t i = from; i < from + n && i < b.size(); ++i)
    s += b[i];
  return s;
}
// SaveData.dat's checksum range (0x1C .. 0x1C + the two section sizes).
inline bool SaveSumRange(const Bytes &sd, size_t &to) {
  if (sd.size() < 0x1C)
    return false;
  to = 0x1C + size_t(Be32(sd, 0)) + size_t(Be32(sd, 4));
  return to <= sd.size();
}
inline bool SaveSumOk(const Bytes &sd) {
  size_t to = 0;
  return SaveSumRange(sd, to) && Be32(sd, 0x18) == uint32_t(Sum(sd, 0x1C, to - 0x1C));
}
inline void SaveSumFix(Bytes &sd) {
  size_t to = 0;
  if (SaveSumRange(sd, to))
    PutBe32(sd, 0x18, uint32_t(Sum(sd, 0x1C, to - 0x1C)));
}

// The game's DXT5 picture (words swapped, no header) as a DDS, and back.
inline Bytes ToDds(const Bytes &cas, size_t at, size_t n, int w, int h) {
  Bytes d(128, 0);
  auto le = [&](size_t o, uint32_t v) {
    for (int i = 0; i < 4; ++i)
      d[o + size_t(i)] = uint8_t(v >> (8 * i));
  };
  memcpy(d.data(), "DDS ", 4);
  le(4, 124), le(8, 0x1007), le(12, uint32_t(h)), le(16, uint32_t(w)), le(20, uint32_t(n)), le(28, 1);
  le(76, 32), le(80, 4);
  memcpy(&d[84], "DXT5", 4);
  le(108, 0x1000);
  d.insert(d.end(), cas.begin() + long(at), cas.begin() + long(at + n));
  for (size_t i = 128; i + 1 < d.size(); i += 2)
    std::swap(d[i], d[i + 1]);
  return d;
}
inline Bytes FromImage(const svrfmt::Image &img, size_t n) {
  Bytes dds = svrfmt::DdsEncode(img, svrfmt::DxtFormat::kDxt5, false);
  if (dds.size() < n)
    return {};
  Bytes raw(dds.end() - long(n), dds.end());
  for (size_t i = 0; i + 1 < raw.size(); i += 2)
    std::swap(raw[i], raw[i + 1]);
  return raw;
}
// A picture fitted whole into w x h (transparent around it, standing on the bottom edge).
inline svrfmt::Image FitInto(const svrfmt::Image &img, int W, int H) {
  const double k = (std::min)(double(W) / img.w, double(H) / img.h);
  const int w = (std::max)(1, int(img.w * k + 0.5)), h = (std::max)(1, int(img.h * k + 0.5));
  const svrfmt::Image fitted = svrfmt::Resize(img, w, h);
  svrfmt::Image out;
  out.w = W, out.h = H;
  out.rgba.assign(size_t(W) * H * 4, 0);
  const int x0 = (W - w) / 2, y0 = H - h;
  for (int y = 0; y < h; ++y)
    std::copy_n(&fitted.rgba[size_t(y) * w * 4], size_t(w) * 4, &out.rgba[(size_t(y0 + y) * W + x0) * 4]);
  return out;
}

inline fs::path PickSaveCaw(bool fr, const std::wstring &name) {
  IFileSaveDialog *dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
    return {};
  const COMDLG_FILTERSPEC filter[] = {{L"CAW (*.wwe12caw)", L"*.wwe12caw"}};
  dlg->SetFileTypes(1, filter);
  dlg->SetDefaultExtension(L"wwe12caw");
  dlg->SetFileName(name.c_str());
  dlg->SetTitle(fr ? L"Exporter le CAW" : L"Export the CAW");
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
  return picked;
}

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, const fs::path &user_root) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Vos catcheurs créés (CAW) : changer l'image que le jeu montre dans ses listes, "
                                  "exporter un CAW dans un fichier pour le partager, importer celui d'un autre joueur. "
                                  "Votre sauvegarde est copiée avant chaque changement (SaveBackups). Fermez le jeu avant."
                                : "Your created superstars (CAW): change the picture the game shows in its lists, export "
                                  "a CAW to a file to share it, import another player's. Your save is copied before "
                                  "every change (SaveBackups). Close the game first.");
    if (root_ != user_root || ImGui::Button(fr ? "Relire la sauvegarde" : "Read the save again")) {
      root_ = user_root;
      Load(fr, device);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(game::IsRunning());
    if (ImGui::Button(fr ? "Importer un CAW..." : "Import a CAW..."))
      Import(fr, device);
    ImGui::EndDisabled();
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
    if (caws_.empty()) {
      ImGui::TextDisabled("%s", fr ? "Aucun CAW dans la sauvegarde." : "No CAW in the save.");
      return;
    }
    for (size_t i = 0; i < caws_.size(); ++i) {
      Caw &c = caws_[i];
      ImGui::PushID(int(i));
      ImGui::SeparatorText(c.label.c_str());
      for (int a = 0; a < kAttires; ++a) {
        if (!c.attires[a].present)
          continue;
        ImGui::PushID(a);
        if (a && ImGui::GetContentRegionAvail().x > 260)
          ImGui::SameLine();
        ImGui::BeginGroup();
        arena_page::DrawPicture(c.attires[a].shown, 150);
        ImGui::Text(fr ? "Tenue %d%s" : "Attire %d%s", a + 1, c.attires[a].changed ? " *" : "");
        ImGui::BeginDisabled(game::IsRunning());
        if (ImGui::SmallButton(fr ? "Changer l'image..." : "Change the picture..."))
          Pick(fr, device, c, a);
        ImGui::EndDisabled();
        ImGui::EndGroup();
        ImGui::PopID();
      }
      bool changed = false;
      for (const auto &at : c.attires)
        changed |= at.changed;
      ImGui::BeginDisabled(!changed || game::IsRunning());
      if (ImGui::Button(fr ? "Enregistrer dans la sauvegarde" : "Save into the save"))
        SavePictures(fr, device, c);
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button(fr ? "Exporter..." : "Export..."))
        Export(fr, c);
      ImGui::PopID();
    }
  }

 private:
  struct Attire {
    bool present = false, changed = false;
    Bytes render, thumb;  // the replacement's bytes as the game keeps them, when changed
    arena_page::Thumb shown;
  };
  struct Caw {
    fs::path file, header;
    int slot = -1;
    std::string name, label;
    Attire attires[kAttires];
  };

  fs::path SaveDir() const { return root_ / "B13EBABEBABEBABE" / "54510890"; }
  fs::path SaveData() const { return SaveDir() / "00000001" / "SaveData.dat" / "SaveData.Dat"; }

  static bool Read(const fs::path &p, Bytes &out) {
    std::ifstream f(p, std::ios::binary);
    if (!f)
      return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
  }
  static bool Write(const fs::path &p, const Bytes &b) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char *>(b.data()), std::streamsize(b.size()));
    return bool(f);
  }
  static std::string NameOf(const Bytes &cas) {
    std::string n;
    for (size_t i = kRecInCas + kName; i < kRecInCas + kName + 31 && cas[i]; ++i)
      n += char(cas[i]);
    return n;
  }

  void Load(bool fr, ID3D11Device *device) {
    caws_.clear();
    std::error_code ec;
    for (fs::directory_iterator it(SaveDir() / "00000001", ec), end; !ec && it != end; it.increment(ec)) {
      const std::string dir = it->path().filename().string();
      if (!it->is_directory(ec) || dir.size() != 21 || dir.substr(2) != "CreateSuperStar.cas" || !isdigit(dir[0]) ||
          !isdigit(dir[1]))
        continue;
      Caw c;
      c.slot = (dir[0] - '0') * 10 + (dir[1] - '0');
      for (fs::directory_iterator f(it->path(), ec), e2; !ec && f != e2; f.increment(ec))
        if (f->is_regular_file(ec))
          c.file = f->path();
      c.header = SaveDir() / "Headers" / "00000001" / (dir + ".header");
      Bytes cas;
      if (c.file.empty() || !Read(c.file, cas) || cas.size() != kCasSize)
        continue;
      c.name = NameOf(cas);
      char label[96];
      snprintf(label, sizeof(label), "%02d. %s", c.slot + 1, c.name.c_str());
      c.label = label;
      for (int a = 0; a < kAttires; ++a) {
        const size_t at = kSlotAt + size_t(a) * kSlotSize + kRenderAt;
        c.attires[a].present = Sum(cas, at, kRenderBytes) != 0;
        if (c.attires[a].present)
          c.attires[a].shown = arena_page::MakeThumb(device, ToDds(cas, at, kRenderBytes, 256, 512));
      }
      caws_.push_back(std::move(c));
    }
    std::sort(caws_.begin(), caws_.end(), [](const Caw &a, const Caw &b) { return a.slot < b.slot; });
    (void)fr;
  }

  void Pick(bool fr, ID3D11Device *device, Caw &c, int a) {
    const fs::path p = arena_page::PickOpen(fr, false);
    svrfmt::Image img;
    if (p.empty() || !wic_image::Load(p, img) || img.w <= 0 || img.h <= 0)
      return;
    const svrfmt::Image render = FitInto(img, 256, 512);
    // The thumb: the top of the render (the head and shoulders), 128 x 128.
    svrfmt::Image top;
    top.w = 256, top.h = 256;
    top.rgba.assign(render.rgba.begin(), render.rgba.begin() + 256 * 256 * 4);
    Attire &at = c.attires[a];
    at.render = FromImage(render, kRenderBytes);
    at.thumb = FromImage(svrfmt::Resize(top, 128, 128), kThumbBytes);
    if (at.render.empty() || at.thumb.empty())
      return;
    at.changed = true;
    Bytes shown(kCasSize, 0);
    std::copy(at.render.begin(), at.render.end(), shown.begin());
    at.shown = arena_page::MakeThumb(device, ToDds(shown, 0, kRenderBytes, 256, 512));
  }

  // The save copied as the launcher's automatic backups (SaveBackups\<date>\, backup.txt "auto" + why).
  bool Backup(const std::string &why) {
    const fs::path all = root_ / "SaveBackups";
    std::error_code ec;
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &tm);
    std::string name = stamp;
    for (int n = 2; fs::exists(all / name, ec); ++n)
      name = std::string(stamp) + "_" + std::to_string(n);
    const fs::path dest = all / name;
    fs::create_directories(dest / "B13EBABEBABEBABE", ec);
    fs::copy(SaveDir(), dest / "B13EBABEBABEBABE" / "54510890", fs::copy_options::recursive, ec);
    if (ec) {
      fs::remove_all(dest, ec);
      return false;
    }
    std::ofstream(dest / "backup.txt") << "auto\n" << why << "\n";
    return true;
  }

  void SavePictures(bool fr, ID3D11Device *device, Caw &c) {
    message_ok_ = false;
    Bytes cas, sd;
    if (!Read(c.file, cas) || cas.size() != kCasSize || !Read(SaveData(), sd)) {
      message_ = fr ? "Sauvegarde illisible." : "The save can't be read.";
      return;
    }
    const uint32_t was = Be32(cas, kSum1);
    const size_t rec = kRecBase + size_t(c.slot) * kRecSize;
    if (Be32(cas, kSum2) != was || Be32(cas, kSum3) != was || rec + kRecSize > sd.size() ||
        Be32(sd, rec + kRecSum) != was || NameOf(cas) != std::string(reinterpret_cast<const char *>(&sd[rec + kName])) ||
        !SaveSumOk(sd)) {
      message_ = fr ? "Ce CAW ne ressemble pas à ce que le Mod Maker connaît : rien n'est changé."
                    : "This CAW doesn't look as the Mod Maker knows it: nothing changed.";
      return;
    }
    if (!Backup("before the CAW picture of " + c.name + " changed (Mod Maker)")) {
      message_ = fr ? "La copie de la sauvegarde a échoué : rien n'est changé." : "The save couldn't be copied: nothing changed.";
      return;
    }
    int64_t delta = 0;
    for (int a = 0; a < kAttires; ++a) {
      Attire &at = c.attires[a];
      if (!at.changed)
        continue;
      const size_t base = kSlotAt + size_t(a) * kSlotSize;
      delta -= int64_t(Sum(cas, base + kRenderAt, kRenderBytes) + Sum(cas, base + kThumbAt, kThumbBytes));
      std::copy(at.render.begin(), at.render.end(), cas.begin() + long(base + kRenderAt));
      std::copy(at.thumb.begin(), at.thumb.end(), cas.begin() + long(base + kThumbAt));
      delta += int64_t(Sum(cas, base + kRenderAt, kRenderBytes) + Sum(cas, base + kThumbAt, kThumbBytes));
    }
    const uint32_t sum = uint32_t(int64_t(was) + delta);
    PutBe32(cas, kSum1, sum), PutBe32(cas, kSum2, sum), PutBe32(cas, kSum3, sum);
    PutBe32(sd, rec + kRecSum, sum);
    SaveSumFix(sd);
    if (!Write(c.file, cas) || !Write(SaveData(), sd)) {
      message_ = fr ? "Écriture impossible (la copie est dans SaveBackups)." : "Couldn't write (the copy is in SaveBackups).";
      return;
    }
    message_ok_ = true;
    message_ = (fr ? "Image enregistrée pour " : "Picture saved for ") + c.name;
    Load(fr, device);
  }

  // A .wwe12caw: a zip of the CAW's file (caw.cas), its record (record.bin) and its package header (header.bin).
  void Export(bool fr, const Caw &c) {
    message_ok_ = false;
    Bytes cas, sd, header;
    const size_t rec = kRecBase + size_t(c.slot) * kRecSize;
    if (!Read(c.file, cas) || !Read(SaveData(), sd) || rec + kRecSize > sd.size()) {
      message_ = fr ? "Sauvegarde illisible." : "The save can't be read.";
      return;
    }
    Read(c.header, header);
    std::wstring name;
    for (unsigned char ch : c.name)
      name += std::isalnum(ch) ? wchar_t(ch) : L'_';
    const fs::path out = PickSaveCaw(fr, (name.empty() ? L"caw" : name) + L".wwe12caw");
    if (out.empty())
      return;
    std::vector<std::pair<std::string, Bytes>> entries = {
        {"caw.cas", cas}, {"record.bin", Bytes(sd.begin() + long(rec), sd.begin() + long(rec + kRecSize))}};
    if (header.size() == 0x148)
      entries.push_back({"header.bin", header});
    if (!mods::Manager::WriteZip(out, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(out.wstring());
      return;
    }
    message_ok_ = true;
    message_ = (fr ? "CAW exporté : " : "CAW exported: ") + media::Utf8(out.wstring());
  }

  // Into the first free slot (no file, its record all zero): the file, its header, its record (from a .wwe12caw,
  // else the .cas's copy and the save's own part of the user's own first CAW, or as measured), the save's checksum.
  void Import(bool fr, ID3D11Device *device) {
    message_ok_ = false;
    const fs::path src = arena_page::PickOpen(fr, false, fr ? L"CAW (*.wwe12caw, *.cas, SaveData.Dat)"
                                                            : L"CAW (*.wwe12caw, *.cas, SaveData.Dat)",
                                              L"*.wwe12caw;*.cas;*.dat");
    if (src.empty())
      return;
    Bytes cas, record, header, file;
    Read(src, file);
    if (file.size() > 4 && file[0] == 'P' && file[1] == 'K') {
      // A .wwe12caw (Export's zip: stored entries): its local headers in turn.
      for (size_t at = 0; at + 30 <= file.size() && file[at] == 'P' && file[at + 1] == 'K' && file[at + 2] == 3;) {
        auto le16 = [&](size_t o) { return size_t(file[at + o] | file[at + o + 1] << 8); };
        auto le32 = [&](size_t o) { return size_t(le16(o) | le16(o + 2) << 16); };
        const size_t size = le32(18), name_len = le16(26), data = at + 30 + name_len + le16(28);
        if (le16(8) != 0 || data + size > file.size())
          break;
        const std::string name(reinterpret_cast<const char *>(&file[at + 30]), name_len);
        Bytes bytes(file.begin() + long(data), file.begin() + long(data + size));
        if (name == "caw.cas")
          cas = std::move(bytes);
        else if (name == "record.bin")
          record = std::move(bytes);
        else if (name == "header.bin")
          header = std::move(bytes);
        at = data + size;
      }
    } else {
      cas = std::move(file);
    }
    if (cas.size() != kCasSize || Be32(cas, kSum1) != Be32(cas, kSum3)) {
      message_ = fr ? "Ce n'est pas un CAW de WWE '12." : "This isn't a WWE '12 CAW.";
      return;
    }
    Bytes sd;
    if (!Read(SaveData(), sd) || !SaveSumOk(sd)) {
      message_ = fr ? "Sauvegarde illisible ou abîmée : rien n'est changé." : "The save can't be read or is damaged: nothing changed.";
      return;
    }
    std::error_code ec;
    int slot = -1;
    for (int s = 0; s < kMaxCaw && slot < 0; ++s) {
      char dir[32];
      snprintf(dir, sizeof(dir), "%02dCreateSuperStar.cas", s);
      const size_t rec = kRecBase + size_t(s) * kRecSize;
      if (rec + kRecSize <= sd.size() && Sum(sd, rec, kRecSize) == 0 && !fs::exists(SaveDir() / "00000001" / dir, ec))
        slot = s;
    }
    if (slot < 0) {
      message_ = fr ? "Plus de place libre pour un CAW." : "No free CAW slot left.";
      return;
    }
    if (record.size() != kRecSize) {
      // The save's own part from one of this save's CAWs, else as measured on the user's.
      record.assign(kRecSize, 0);
      std::copy(cas.begin() + long(kRecInCas), cas.begin() + long(kRecInCas + kRecShared), record.begin());
      bool own = false;
      for (const auto &c : caws_) {
        const size_t rec = kRecBase + size_t(c.slot) * kRecSize;
        if (rec + kRecSize <= sd.size()) {
          std::copy(sd.begin() + long(rec + kRecShared), sd.begin() + long(rec + kRecSize), record.begin() + long(kRecShared));
          own = true;
          break;
        }
      }
      if (!own) {
        static const uint8_t kTail[] = {0x00, 0x00, 0x01, 0x00, 0x02, 0x11, 0x02, 0x05, 0xff, 0xff, 0xff, 0xff,
                                        0x00, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf1, 0x00, 0x00, 0x00, 0x29,
                                        0x00, 0x1e};
        record[0x574] = 1;
        std::copy(std::begin(kTail), std::end(kTail), record.begin() + 0x688);
        record[0x6D0] = 1, record[0x6D4] = 1;
      }
    }
    PutBe32(record, kRecSum, Be32(cas, kSum1));
    if (!Backup("before a CAW was imported (Mod Maker)")) {
      message_ = fr ? "La copie de la sauvegarde a échoué : rien n'est changé." : "The save couldn't be copied: nothing changed.";
      return;
    }
    char dir[32];
    snprintf(dir, sizeof(dir), "%02dCreateSuperStar.cas", slot);
    // Its header: a CAW package's, with this slot's names ("NN.CUSTOM SUPERSTAR" at 8, UTF-16BE; the file at 0x108).
    if (header.size() != 0x148)
      for (const auto &c : caws_)
        if (Read(c.header, header) && header.size() == 0x148)
          break;
    if (header.size() != 0x148) {
      header.assign(0x148, 0);
      header[3] = 1, header[7] = 1;
      const uint8_t title[4] = {0x54, 0x51, 0x08, 0x90};
      std::copy(title, title + 4, header.begin() + 0x140);
    }
    std::fill(header.begin() + 8, header.begin() + 0x108 + 40, uint8_t(0));
    char display[40];
    snprintf(display, sizeof(display), "%02d.CUSTOM SUPERSTAR", slot + 1);
    for (size_t i = 0; display[i]; ++i)
      header[9 + 2 * i] = uint8_t(display[i]);
    for (size_t i = 0; dir[i]; ++i)
      header[0x108 + i] = uint8_t(dir[i]);
    std::copy(record.begin(), record.end(), sd.begin() + long(kRecBase + size_t(slot) * kRecSize));
    SaveSumFix(sd);
    if (!Write(SaveDir() / "00000001" / dir / "SaveData.Dat", cas) ||
        !Write(SaveDir() / "Headers" / "00000001" / (std::string(dir) + ".header"), header) || !Write(SaveData(), sd)) {
      message_ = fr ? "Écriture impossible (la copie est dans SaveBackups)." : "Couldn't write (the copy is in SaveBackups).";
      return;
    }
    message_ok_ = true;
    message_ = (fr ? "CAW importé : " : "CAW imported: ") + NameOf(cas) + (fr ? ", place " : ", slot ") +
               std::to_string(slot + 1);
    Load(fr, device);
  }

  fs::path root_;
  std::vector<Caw> caws_;
  std::string message_;
  bool message_ok_ = false;
};

}  // namespace caw_page
