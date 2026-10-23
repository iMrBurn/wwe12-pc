// Mod Maker, "Game files" page: a read-only look inside the game's archives (.pac) - the game's own (from its
// folder or its .iso) or any file. A tree of every level (EPAC groups and entries, PACH lists inside them, BPE
// undone) with Kaiko's svrfmt/archive.h; an entry shows what it is, a DDS or a texture bundle its pictures, text
// its text; Export writes it out (unpacked). Nothing is ever written back: the game's files stay as they are.
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
#include "imgui.h"
#include "media.h"
#include "svrfmt/archive.h"
#include "svrfmt/texture.h"

namespace pac_page {

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

class Browser {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &disc) {
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
            if (disc.Read(dirs_[dir_] + "/" + f, bytes))
              Open(fr, dirs_[dir_] + "/" + f, std::move(bytes));
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
      }
    }
    if (!error_.empty())
      ImGui::TextColored(red, "%s", error_.c_str());
    if (title_.empty())
      return;
    ImGui::Text("%s", title_.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%s, %s)", archive_.format.c_str(), Human(archive_.original.size()).c_str());

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
      static const char *fmts[] = {"DXT1", "DXT3", "DXT5", "ARGB", "?"};
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
  svrfmt::ArcNode *selected_ = nullptr;
  std::vector<uint8_t> data_;
  std::vector<Pic> pictures_;
};

}  // namespace pac_page
