// The games of My game discs as a simple list, each with its own icon (the title image of its default.xex, read from
// the player's disc in the background: xex_icon.h; nothing shipped) - the user's wish for MOVES & ENTRANCES' first
// step, 2026-11-09 (a grid of cards before).
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <chrono>
#include <functional>
#include <future>
#include <map>

#include "game_disc.h"
#include "game_discs.h"
#include "imgui.h"
#include "ui_style.h"
#include "wic_image.h"
#include "xex_icon.h"

namespace game_icons {

using Microsoft::WRL::ComPtr;

class Icons {
 public:
  // The icon of `title`'s disc as a texture, null while it loads (or when there is none)
  ID3D11ShaderResourceView *Get(ID3D11Device *device, const game_discs::Discs *discs, uint32_t title) {
    if (const auto it = done_.find(title); it != done_.end())
      return it->second.Get();
    if (!device || !discs)
      return nullptr;
    auto p = pending_.find(title);
    if (p == pending_.end()) {
      const std::filesystem::path path = discs->PathOf(title);
      if (path.empty())
        return nullptr;
      pending_[title] = std::async(std::launch::async, [path]() {
        disc::GameDisc d;
        svrfmt::Bytes x;
        if (!d.OpenPath(path) || !d.Read("default.xex", x))
          return xex_icon::Bytes{};
        return xex_icon::TitleImage(x);
      });
      return nullptr;
    }
    if (p->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
      return nullptr;
    const auto png = p->second.get();
    pending_.erase(p);
    ComPtr<ID3D11ShaderResourceView> srv;
    svrfmt::Image img;
    if (wic_image::LoadMemory(png, img) && img.w > 0) {
      D3D11_TEXTURE2D_DESC td{};
      td.Width = UINT(img.w), td.Height = UINT(img.h), td.MipLevels = 1, td.ArraySize = 1;
      td.Format = DXGI_FORMAT_R8G8B8A8_UNORM, td.SampleDesc.Count = 1, td.Usage = D3D11_USAGE_DEFAULT;
      td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      D3D11_SUBRESOURCE_DATA init{img.rgba.data(), UINT(img.w * 4), 0};
      ComPtr<ID3D11Texture2D> tex;
      if (SUCCEEDED(device->CreateTexture2D(&td, &init, &tex)))
        device->CreateShaderResourceView(tex.Get(), nullptr, &srv);
    }
    done_[title] = srv;  // (null when the disc has none: not asked again)
    return srv.Get();
  }

 private:
  std::map<uint32_t, ComPtr<ID3D11ShaderResourceView>> done_;
  std::map<uint32_t, std::future<xex_icon::Bytes>> pending_;
};

// One row per game: its icon, its name, `what` (what is taken from it), on the right whether its disc is set. A game
// with its disc is chosen by a click on its row (on_choose); one without offers to add it (on_add).
inline void DrawList(bool fr, ID3D11Device *device, const game_discs::Discs *discs, Icons &icons, uint32_t chosen,
                     const std::function<bool(const game_discs::Known &)> &listed,
                     const std::function<const char *(uint32_t)> &name, const std::function<const char *(uint32_t)> &what,
                     const std::function<void(uint32_t)> &on_choose, const std::function<void()> &on_add) {
  const float row_h = 64, icon = 48;
  for (const auto &k : game_discs::kKnown) {
    if (!listed(k))
      continue;
    const bool have = discs && !discs->PathOf(k.title).empty(), is = chosen == k.title;
    ImGui::PushID(int(k.title));
    const ImVec2 p0 = ImGui::GetCursorPos(), s0 = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.22f, 0.08f, 0.85f));
    if (ImGui::Selectable("##row", is, (have ? 0 : ImGuiSelectableFlags_Disabled) | ImGuiSelectableFlags_AllowOverlap,
                          ImVec2(w, row_h)) &&
        have)
      on_choose(k.title);
    ImGui::PopStyleColor();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 ip(s0.x + 8, s0.y + (row_h - icon) * 0.5f);
    if (ID3D11ShaderResourceView *srv = have ? icons.Get(device, discs, k.title) : nullptr)
      dl->AddImage(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(srv))), ip, ImVec2(ip.x + icon, ip.y + icon),
                   ImVec2(0, 0), ImVec2(1, 1), have ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 90));
    else
      dl->AddRectFilled(ip, ImVec2(ip.x + icon, ip.y + icon), IM_COL32(40, 40, 44, 255), 4.0f);
    ImGui::SetCursorPos(ImVec2(p0.x + icon + 22, p0.y + 8));
    ImGui::PushFont(ui::g_font_title, 24.0f);
    ImGui::TextColored(have ? ImVec4(0.94f, 0.75f, 0.24f, 1.0f) : ImVec4(0.55f, 0.53f, 0.5f, 1.0f), "%s", name(k.title));
    ImGui::PopFont();
    ImGui::SetCursorPos(ImVec2(p0.x + icon + 22, p0.y + 38));
    ImGui::TextDisabled("%s", what(k.title));
    const float right = p0.x + w - 230;
    if (have) {
      ImGui::SetCursorPos(ImVec2(right, p0.y + 22));
      ImGui::TextColored(is ? ImVec4(0.94f, 0.75f, 0.24f, 1.0f) : ImVec4(0.55f, 0.85f, 0.45f, 1.0f), "%s",
                         is ? (fr ? "Choisi" : "Chosen") : (fr ? "Disque prêt : cliquez" : "Disc set: click"));
    } else {
      ImGui::SetCursorPos(ImVec2(right, p0.y + 18));
      if (ImGui::Button(fr ? "Ajouter le disque" : "Add its disc", ImVec2(220, 0)))
        on_add();
    }
    // (the row's size given back with an item: a bare SetCursorPos past the content trips ImGui's check, "Code uses
    // SetCursorPos() to extend window boundaries", seen by the user 2026-11-09)
    ImGui::SetCursorPos(p0);
    ImGui::Dummy(ImVec2(w, row_h));
    ImGui::PopID();
  }
}

}  // namespace game_icons
