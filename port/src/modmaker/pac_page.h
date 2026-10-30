// Mod Maker, "Game files" page: a read-only look inside the game's archives (.pac) - the game's own (from its
// folder or its .iso) or any file. A tree of every level (EPAC groups and entries, PACH lists inside them, BPE
// undone) with Kaiko's svrfmt/archive.h; an entry shows what it is, a DDS or a texture bundle its pictures, text
// its text; Export writes it out (unpacked). Replace puts another file in an entry's place; the changed archive is
// saved as a file or, for one of the game's own, as a mod (type=files, files/<its path>). The game's files stay as
// they are. The game finds an entry of its archives by its place (plist360.arc has every entry's sector and size):
// the changed archive is written over the original in place (InPlace), each changed entry in its own room.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "game_disc.h"
#include "bpe_like.h"
#include "game_process.h"
#include "imgui.h"
#include "media.h"
#include "mod_version.h"
#include "mods.h"
#include "svrfmt/archive.h"
#include "svrfmt/texture.h"

namespace pac_page {

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

class Browser {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &disc, mods::Manager &mods) {
    device_ = device;
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Regardez ce qu'il y a dans les archives du jeu (.pac) : modèles, textures, textes... "
                                  "Rien n'est modifié ; « Exporter » enregistre une partie à part, pour l'étudier ou la "
                                  "retoucher."
                                : "Look inside the game's archives (.pac): models, textures, texts... Nothing is "
                                  "changed; \"Export\" saves a part on its own, to study or edit it.");
    // The game's folders, then their .pac files.
    if (disc.Found()) {
      if (dirs_.empty()) {
        dirs_.push_back("pac");
        for (const auto &d : disc.ListDirs("pac"))
          dirs_.push_back("pac/" + d);
      }
      ImGui::SetNextItemWidth(200);
      if (ImGui::BeginCombo("##dir", dirs_[dir_].c_str())) {
        for (int i = 0; i < int(dirs_.size()); ++i)
          if (ImGui::Selectable(dirs_[i].c_str(), i == dir_)) {
            dir_ = i;
            files_.clear();
          }
        ImGui::EndCombo();
      }
      if (files_.empty()) {
        for (const auto &f : disc.List(dirs_[dir_]))
          if (f.size() > 4 && _stricmp(f.c_str() + f.size() - 4, ".pac") == 0)
            files_.push_back(f);
        std::sort(files_.begin(), files_.end(), [](const std::string &a, const std::string &b) {
          return _stricmp(a.c_str(), b.c_str()) < 0;
        });
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(260);
      if (ImGui::BeginCombo("##file", fr ? "Ouvrir une archive du jeu..." : "Open a game archive...",
                            ImGuiComboFlags_HeightLarge)) {
        for (const auto &f : files_)
          if (ImGui::Selectable(f.c_str())) {
            std::vector<uint8_t> bytes;
            if (disc.Read(dirs_[dir_] + "/" + f, bytes)) {
              Open(fr, dirs_[dir_] + "/" + f, std::move(bytes));
              game_path_ = title_.empty() ? "" : dirs_[dir_] + "/" + f;
            }
            else
              error_ = (fr ? "Lecture impossible : " : "Could not read: ") + f;
          }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
    }
    if (ImGui::Button(fr ? "Ouvrir un fichier..." : "Open a file...")) {
      const fs::path p = PickPac(fr);
      if (!p.empty()) {
        std::ifstream f(p, std::ios::binary);
        Open(fr, media::Utf8(p.filename().wstring()),
             std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()));
        game_path_.clear();
      }
    }
    if (!error_.empty())
      ImGui::TextColored(red, "%s", error_.c_str());
    if (title_.empty())
      return;
    ImGui::Text("%s", title_.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%s, %s)", archive_.format.c_str(), Human(archive_.original.size()).c_str());
    if (!changed_.empty())
      SaveBar(fr, mods);

    // Tree | details.
    const float tree_w = std::max(320.0f, ImGui::GetContentRegionAvail().x * 0.42f);
    ImGui::BeginChild("tree", ImVec2(tree_w, 0), ImGuiChildFlags_Borders);
    Tree(archive_.root, "root");
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("details", ImVec2(0, 0), ImGuiChildFlags_Borders);
    Details(fr);
    ImGui::EndChild();
  }

 private:
  static std::string Human(size_t b) {
    char s[32];
    if (b >= (1u << 20))
      snprintf(s, sizeof(s), "%.1f MB", b / 1048576.0);
    else if (b >= 1024)
      snprintf(s, sizeof(s), "%.1f KB", b / 1024.0);
    else
      snprintf(s, sizeof(s), "%zu B", b);
    return s;
  }

  static fs::path PickPac(bool fr) {
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
      return {};
    const COMDLG_FILTERSPEC filter[] = {{fr ? L"Archives (*.pac)" : L"Archives (*.pac)", L"*.pac"}, {L"*.*", L"*.*"}};
    dlg->SetFileTypes(2, filter);
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
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

  void Open(bool fr, const std::string &title, std::vector<uint8_t> bytes) {
    archive_ = svrfmt::Archive();
    changed_.clear();
    save_msg_.clear();
    selected_ = nullptr;
    pictures_.clear();
    error_.clear();
    std::string why;
    if (!svrfmt::ArchiveOpen(bytes, archive_, &why)) {
      title_.clear();
      error_ = (fr ? "Pas une archive lisible : " : "Not a readable archive: ") + title + (why.empty() ? "" : " (" + why + ")");
      return;
    }
    title_ = title;
    export_name_ = fs::path(title).stem().string();
  }

  static std::string Label(const svrfmt::ArcNode &n) {
    std::string l = n.name.empty() ? std::to_string(n.id) : n.name;
    if (!n.group.empty())
      l = n.group + " / " + l;
    return l;
  }

  void Tree(svrfmt::ArcNode &n, const char *id) {
    for (size_t i = 0; i < n.kids.size(); ++i) {
      svrfmt::ArcNode &k = n.kids[i];
      ImGui::PushID(int(i));
      // A node that can hold others gets an arrow; its children are read the first time it opens.
      const bool container = k.kind != svrfmt::ArcKind::Leaf || (!k.parsed && LooksLikeContainer(k));
      ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
      if (!container)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
      if (selected_ == &k)
        flags |= ImGuiTreeNodeFlags_Selected;
      const std::string label = Label(k) + "   " + Human(k.stored.size());
      const bool open = ImGui::TreeNodeEx(id, flags, "%s", label.c_str());
      if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        Select(k);
      if (container && open) {
        if (!k.parsed)
          svrfmt::ArchiveExpand(k);
        Tree(k, "kid");
        ImGui::TreePop();
      }
      ImGui::PopID();
    }
  }

  static bool LooksLikeContainer(const svrfmt::ArcNode &n) {
    const std::string kind = svrfmt::ArchiveKindOf(n);
    return kind == "PACH" || kind == "EPAC" || kind == "EPK8" || kind == "SHDC" || kind == "HSPC";
  }

  void Select(svrfmt::ArcNode &n) {
    selected_ = &n;
    pictures_.clear();
    text_.clear();
    data_ = svrfmt::ArchiveData(n);
    kind_ = svrfmt::ArchiveKindOf(n);
    if (kind_ == "DDS") {
      AddPicture(Label(n), data_);
    } else if (kind_ == "textures") {
      std::vector<svrfmt::BundleTexture> texs;
      if (svrfmt::BundleRead(data_, texs))
        for (const auto &t : texs)
          AddPicture(t.name, t.data);
    } else if (kind_ == "text") {
      text_.assign(data_.begin(), data_.begin() + std::min<size_t>(data_.size(), 64 * 1024));
    }
  }

  struct Pic {
    std::string name, info;
    std::vector<uint8_t> dds;
    ComPtr<ID3D11ShaderResourceView> tex;
    int w = 0, h = 0;
  };

  void AddPicture(const std::string &name, const std::vector<uint8_t> &dds) {
    Pic p;
    p.name = name;
    p.dds = dds;
    svrfmt::DdsInfo info;
    if (svrfmt::DdsInfoOf(dds, info)) {
      static const char *fmts[] = {"DXT1", "DXT3", "DXT5", "ARGB", "?", "ARGB 4444"};
      p.info = std::to_string(info.w) + " x " + std::to_string(info.h) + " " + fmts[int(info.format)];
    }
    svrfmt::Image img;
    if (device_ && svrfmt::DdsDecode(dds, img) && img.w > 0) {
      D3D11_TEXTURE2D_DESC td{};
      td.Width = UINT(img.w);
      td.Height = UINT(img.h);
      td.MipLevels = 1;
      td.ArraySize = 1;
      td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      td.SampleDesc.Count = 1;
      td.Usage = D3D11_USAGE_DEFAULT;
      td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      D3D11_SUBRESOURCE_DATA init{img.rgba.data(), UINT(img.w * 4), 0};
      ComPtr<ID3D11Texture2D> t;
      if (SUCCEEDED(device_->CreateTexture2D(&td, &init, &t)))
        device_->CreateShaderResourceView(t.Get(), nullptr, &p.tex);
      p.w = img.w;
      p.h = img.h;
    }
    pictures_.push_back(std::move(p));
  }

  void Details(bool fr) {
    if (!selected_) {
      ImGui::TextDisabled("%s", fr ? "Choisissez une entrée à gauche." : "Choose an entry on the left.");
      return;
    }
    static const std::pair<const char *, const char *> kinds[] = {
        {"DDS", "texture (DDS)"},     {"textures", "textures"},   {"model", "3D model (JBOY)"},
        {"motions", "animations"},     {"text", "text"},           {"PACH", "list (PACH)"},
        {"EPAC", "archive (EPAC)"},    {"data", "data"}};
    static const char *kinds_fr[] = {"texture (DDS)", "textures", "modèle 3D (JBOY)", "animations", "texte",
                                     "liste (PACH)",  "archive (EPAC)", "données"};
    std::string what = kind_;
    for (size_t i = 0; i < std::size(kinds); ++i)
      if (kind_ == kinds[i].first)
        what = fr ? kinds_fr[i] : kinds[i].second;
    ImGui::Text("%s", Label(*selected_).c_str());
    ImGui::TextDisabled("%s  -  %s%s", what.c_str(), Human(data_.size()).c_str(),
                        selected_->wrap == svrfmt::ArcWrap::Bpe ? (fr ? " (compressé BPE dans le fichier)"
                                                                      : " (BPE-compressed in the file)")
                                                                : "");
    if (ImGui::Button(fr ? "Exporter..." : "Export...")) {
      const std::string ext = kind_ == "DDS" ? ".dds" : kind_ == "text" ? ".txt" : ".bin";
      std::wstring name;
      for (char c : export_name_ + "_" + Label(*selected_))
        name += std::isalnum(static_cast<unsigned char>(c)) ? wchar_t(c) : L'_';
      const fs::path out = PickSaveAny(fr, name + std::wstring(ext.begin(), ext.end()));
      if (!out.empty()) {
        std::ofstream f(out, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char *>(data_.data()), std::streamsize(data_.size()));
        status_ = f ? (fr ? "Exporté : " : "Exported: ") + media::Utf8(out.wstring())
                    : (fr ? "Échec de l'export." : "Export failed.");
      }
    }
    if (selected_->kind == svrfmt::ArcKind::Leaf && !LooksLikeContainer(*selected_)) {
      ImGui::SameLine();
      if (ImGui::Button(fr ? "Remplacer..." : "Replace...")) {
        const fs::path in = PickAny(fr);
        std::ifstream f(in, std::ios::binary);
        if (!in.empty() && f) {
          const std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
          const std::string kind = kind_;
          const std::vector<uint8_t> old = selected_->stored;
          svrfmt::ArchiveReplace(*selected_, raw);
          if (svrfmt::IsBpe(old) && selected_->wrap == svrfmt::ArcWrap::Bpe)
            selected_->stored = bpe_like::EncodeLike(raw, old);  // (Yuke's blocks kept: much less growth)
          changed_.push_back(Label(*selected_) + "  <-  " + media::Utf8(in.filename().wstring()));
          save_msg_.clear();
          Select(*selected_);
          status_ = kind == "DDS" && kind_ != "DDS" ? (fr ? "Attention : ce n'est pas un DDS, l'original en était un."
                                                          : "Careful: this isn't a DDS, the original was one.")
                                                    : (fr ? "Remplacé (pas encore enregistré)." : "Replaced (not saved yet).");
        }
      }
    }
    if (!status_.empty()) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", status_.c_str());
    }
    ImGui::Separator();
    for (size_t i = 0; i < pictures_.size(); ++i) {
      Pic &p = pictures_[i];
      ImGui::PushID(int(i));
      ImGui::Text("%s", p.name.c_str());
      ImGui::SameLine();
      ImGui::TextDisabled("%s", p.info.c_str());
      if (kind_ == "textures") {
        ImGui::SameLine();
        if (ImGui::SmallButton(fr ? "Exporter le DDS" : "Export the DDS")) {
          std::wstring name;
          for (char c : p.name)
            name += std::isalnum(static_cast<unsigned char>(c)) ? wchar_t(c) : L'_';
          const fs::path out = PickSaveAny(fr, name + L".dds");
          if (!out.empty())
            std::ofstream(out, std::ios::binary | std::ios::trunc)
                .write(reinterpret_cast<const char *>(p.dds.data()), std::streamsize(p.dds.size()));
        }
      }
      if (p.tex) {
        const float max_w = std::min(ImGui::GetContentRegionAvail().x, 512.0f);
        const float scale = std::min(1.0f, max_w / float(p.w));
        // A checkerboard behind, so transparent parts show.
        const ImVec2 at = ImGui::GetCursorScreenPos(), size(p.w * scale, p.h * scale);
        ImDrawList *dl = ImGui::GetWindowDrawList();
        for (float y = 0; y < size.y; y += 16)
          for (float x = 0; x < size.x; x += 16)
            dl->AddRectFilled(ImVec2(at.x + x, at.y + y),
                              ImVec2(at.x + std::min(x + 16, size.x), at.y + std::min(y + 16, size.y)),
                              (int(x / 16) + int(y / 16)) % 2 ? IM_COL32(60, 60, 64, 255) : IM_COL32(40, 40, 44, 255));
        ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(p.tex.Get()))), size);
      } else {
        ImGui::TextDisabled("%s", fr ? "(aperçu impossible)" : "(no preview)");
      }
      ImGui::PopID();
    }
    if (!text_.empty())
      ImGui::TextUnformatted(text_.c_str());
    if (kind_ == "model")
      ImGui::TextDisabled("%s", fr ? "Aperçu des modèles 3D : plus tard." : "3D model preview: later.");
  }

  // The changes, then: save as a mod (one of the game's archives) or as a file, or forget them.
  void SaveBar(bool fr, mods::Manager &mods) {
    const ImVec4 green(0.55f, 0.85f, 0.45f, 1.0f), red(1.0f, 0.45f, 0.35f, 1.0f);
    ImGui::SeparatorText(fr ? "Changements" : "Changes");
    for (const auto &c : changed_)
      ImGui::BulletText("%s", c.c_str());
    if (!game_path_.empty()) {
      ImGui::SetNextItemWidth(220);
      ImGui::InputTextWithHint("##pac_mod_name", fr ? "Nom du mod" : "Mod name", mod_name_, sizeof(mod_name_));
      ImGui::SameLine();
      ImGui::SetNextItemWidth(160);
      ImGui::InputTextWithHint("##pac_mod_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_,
                               sizeof(author_));
      mod_version::Input("Version##pac_version", version_, sizeof(version_));
      ImGui::SameLine();
      ImGui::BeginDisabled(!mod_name_[0] || game::IsRunning());
      if (ImGui::Button(fr ? "Créer le mod et l'installer..." : "Make the mod and install it..."))
        SaveMod(fr, mods);
      ImGui::EndDisabled();
      ImGui::SameLine();
    }
    if (ImGui::Button(fr ? "Enregistrer l'archive sous..." : "Save the archive as...")) {
      std::vector<uint8_t> out;
      const std::string why = Build(fr, out);
      save_ok_ = why.empty();
      save_msg_ = why;
      if (save_ok_) {
        const std::string base = fs::path(title_).filename().string();
        const fs::path to = PickSaveAny(fr, std::wstring(base.begin(), base.end()));
        if (!to.empty()) {
          std::ofstream f(to, std::ios::binary | std::ios::trunc);
          f.write(reinterpret_cast<const char *>(out.data()), std::streamsize(out.size()));
          save_ok_ = bool(f);
          save_msg_ = f ? (fr ? "Enregistré : " : "Saved: ") + media::Utf8(to.wstring())
                        : (fr ? "Échec de l'écriture." : "Could not write it.");
        } else {
          save_msg_.clear();
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Tout annuler" : "Undo all")) {
      std::vector<uint8_t> bytes = archive_.original;
      const std::string path = game_path_, title = title_;
      Open(fr, title, std::move(bytes));
      game_path_ = path;
    }
    if (!save_msg_.empty())
      ImGui::TextColored(save_ok_ ? green : red, "%s", save_msg_.c_str());
  }

  // The changed archive, in place (""), else why it can't be.
  std::string Build(bool fr, std::vector<uint8_t> &out) {
    if (archive_.xcompress)
      return fr ? "Archive compressée (XMemCompress, WWE '13 et après) : pas encore modifiable."
                : "Compressed archive (XMemCompress, WWE '13 and later): it can't be changed yet.";
    const std::vector<uint8_t> saved = svrfmt::ArchiveSave(archive_);
    return InPlace(fr, archive_.original, saved, out);
  }

 public:
  // `saved` (the rebuilt archive) written over `orig`: every top-level entry stays where it was and keeps its size in
  // the table (zeros after a smaller one); a changed entry may not be bigger than it was, nor (BPE) unpack to more.
  // A changed BPE entry is packed again keeping Yuke's blocks (the rebuild packs it whole, a few % bigger).
  static std::string InPlace(bool fr, const std::vector<uint8_t> &orig, const std::vector<uint8_t> &saved,
                             std::vector<uint8_t> &out) {
    std::vector<svrfmt::EpacRow> was, now;
    if (orig.size() < 0x4000 || !svrfmt::EpacTable(orig, orig.size(), was) || !svrfmt::EpacTable(saved, saved.size(), now))
      return fr ? "Seules les archives EPAC / EPK8 (celles de WWE '12) s'enregistrent pour l'instant."
                : "Only EPAC / EPK8 archives (WWE '12's) can be saved for now.";
    if (was.size() != now.size())
      return fr ? "Le nombre d'entrées a changé." : "The number of entries changed.";
    out = orig;
    for (size_t i = 0; i < now.size(); ++i) {
      const auto &a = was[i], &b = now[i];
      if (a.group != b.group || a.name != b.name)
        return fr ? "L'ordre des entrées a changé." : "The entries' order changed.";
      const uint8_t *ob = orig.data() + a.offset;
      std::vector<uint8_t> entry(saved.begin() + ptrdiff_t(b.offset), saved.begin() + ptrdiff_t(b.offset + b.size));
      if (b.size == a.size && !memcmp(entry.data(), ob, a.size))
        continue;
      std::vector<uint8_t> raw;
      if (svrfmt::IsBpe(entry) && a.size >= 16 && !memcmp(ob, "BPE ", 4) && svrfmt::BpeDecode(entry, raw))
        entry = bpe_like::EncodeLike(raw, std::vector<uint8_t>(ob, ob + a.size));
      const uint32_t size = uint32_t(entry.size());
      const uint8_t *nb = entry.data();
      const std::string what = a.group + " / " + a.name;
      if (size > a.size)
        return what + (fr ? " : " : ": ") + Human(size) + (fr ? " au lieu de " : " instead of ") + Human(a.size) +
               (fr ? ". Le jeu trouve chaque entrée à sa place : elle ne peut pas grandir. Prenez un fichier plus "
                     "petit (une image moins grande, ou en DXT1)."
                   : ". The game finds each entry at its place: it can't grow. Take a smaller file (a smaller "
                     "picture, or DXT1).");
      auto unpacked = [](const uint8_t *p, uint32_t size) {
        return size >= 16 && !memcmp(p, "BPE ", 4) ? uint32_t(p[12] | p[13] << 8 | p[14] << 16 | uint32_t(p[15]) << 24)
                                                   : size;
      };
      if (unpacked(nb, size) > unpacked(ob, a.size))
        return what + (fr ? " : une fois décompressée, elle serait plus grosse qu'avant (" : ": unpacked, it would be "
                             "bigger than before (") +
               Human(unpacked(nb, size)) + (fr ? " au lieu de " : " instead of ") + Human(unpacked(ob, a.size)) + ").";
      std::copy(nb, nb + size, out.begin() + ptrdiff_t(a.offset));
      std::fill(out.begin() + ptrdiff_t(a.offset + size), out.begin() + ptrdiff_t(a.offset + a.size), uint8_t(0));
    }
    return {};
  }

 private:
  void SaveMod(bool fr, mods::Manager &mods) {
    std::vector<uint8_t> out;
    save_ok_ = false;
    save_msg_ = Build(fr, out);
    if (!save_msg_.empty())
      return;
    std::wstring file;
    std::string id = "files_";
    for (unsigned char c : std::string(mod_name_)) {
      id += std::isalnum(c) ? char(std::tolower(c)) : '_';
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    }
    const fs::path save = media::PickSave(fr, (file.empty() ? L"files" : file) + L".svrmod");
    if (save.empty())
      return;
    std::string manifest = "game=wwe12\ntype=files\nid=" + id + "\nname=" + std::string(mod_name_) +
                           "\nversion=" + mod_version::Of(version_) + "\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + std::string(mod_name_) + ", " + game_path_ + " changed (";
    for (size_t i = 0; i < changed_.size(); ++i)
      manifest += (i ? ", " : "") + changed_[i].substr(0, changed_[i].find("  <-"));
    manifest += "), made with the WWE '12 Mod Maker.\n";
    if (!mods::Manager::WriteZip(save, {{"manifest.txt", std::vector<uint8_t>(manifest.begin(), manifest.end())},
                                        {"files/" + game_path_, out}})) {
      save_msg_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    save_ok_ = true;
    save_msg_ = (fr ? "Mod créé et installé : " : "Mod made and installed: ") + media::Utf8(save.wstring());
  }

  static fs::path PickAny(bool fr) {
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
      return {};
    const COMDLG_FILTERSPEC filter[] = {{L"*.*", L"*.*"}};
    dlg->SetFileTypes(1, filter);
    dlg->SetTitle(fr ? L"Remplacer par" : L"Replace with");
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
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

  static fs::path PickSaveAny(bool fr, const std::wstring &name) {
    IFileSaveDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
      return {};
    dlg->SetFileName(name.c_str());
    dlg->SetTitle(fr ? L"Exporter" : L"Export");
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

  ID3D11Device *device_ = nullptr;
  std::vector<std::string> dirs_, files_;
  int dir_ = 0;
  svrfmt::Archive archive_;
  std::string title_, export_name_, error_, status_, kind_, text_;
  std::string game_path_;              // the open archive's path in the game ("pac/menu/menu.pac"), "" for a file
  std::vector<std::string> changed_;   // the entries replaced, for the list and the mod's description
  std::string save_msg_;
  bool save_ok_ = false;
  char mod_name_[64] = {}, author_[64] = {}, version_[16] = {};
  svrfmt::ArcNode *selected_ = nullptr;
  std::vector<uint8_t> data_;
  std::vector<Pic> pictures_;
};

}  // namespace pac_page
