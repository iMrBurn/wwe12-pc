// WWE '12 Mod Maker ("WWE12 Mod Maker.exe", next to the game): makes mods (.svrmod, game=wwe12) and looks
// inside the game's files. The launcher installs and manages mods; the Mod Maker makes them (and installs what it
// makes, through the same mods::Manager). Based on Kaiko's SvR 2011 Mod Maker (port/modmaker of
// KaikoClanworth1/wwe-svr2011-pc, with his permission): its idea of pages on a rail, its USER MOVIES maker and its
// pac readers (svrfmt/). Plan and state: docs/mod_maker.md.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <shellapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "arena_page.h"
#include "game_disc.h"
#include "media.h"
#include "mods.h"
#include "pac_page.h"
#include "new_star_page.h"
#include "superstar_page.h"
#include "toml_file.h"
#include "ui_style.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

namespace {

using namespace ui;

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<IDXGISwapChain> g_swapchain;
ComPtr<ID3D11RenderTargetView> g_rtv;
UINT g_resize_w = 0, g_resize_h = 0;

void CreateRenderTarget() {
  ComPtr<ID3D11Texture2D> back;
  g_swapchain->GetBuffer(0, IID_PPV_ARGS(&back));
  g_device->CreateRenderTargetView(back.Get(), nullptr, &g_rtv);
}

bool CreateDevice(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 2;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
  HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                             D3D11_SDK_VERSION, &sd, &g_swapchain, &g_device, nullptr, &g_context);
  if (FAILED(hr))
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
                                       &g_swapchain, &g_device, nullptr, &g_context);
  if (FAILED(hr))
    return false;
  CreateRenderTarget();
  return true;
}

// The logo (resource 101, the launcher's PNG) through WIC.
ComPtr<ID3D11ShaderResourceView> LoadLogo(int &w, int &h) {
  ComPtr<ID3D11ShaderResourceView> srv;
  HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(101), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
  if (!res)
    return srv;
  const void *bytes = LockResource(LoadResource(nullptr, res));
  const DWORD size = SizeofResource(nullptr, res);
  ComPtr<IWICImagingFactory> wic;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))))
    return srv;
  ComPtr<IWICStream> stream;
  wic->CreateStream(&stream);
  stream->InitializeFromMemory(static_cast<BYTE *>(const_cast<void *>(bytes)), size);
  ComPtr<IWICBitmapDecoder> decoder;
  if (FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
    return srv;
  ComPtr<IWICBitmapFrameDecode> frame;
  decoder->GetFrame(0, &frame);
  ComPtr<IWICFormatConverter> conv;
  wic->CreateFormatConverter(&conv);
  conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                   WICBitmapPaletteTypeCustom);
  UINT uw = 0, uh = 0;
  conv->GetSize(&uw, &uh);
  std::vector<BYTE> pixels(size_t(uw) * uh * 4);
  conv->CopyPixels(nullptr, uw * 4, UINT(pixels.size()), pixels.data());
  D3D11_TEXTURE2D_DESC td{};
  td.Width = uw;
  td.Height = uh;
  td.MipLevels = 1;
  td.ArraySize = 1;
  td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_DEFAULT;
  td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA init{pixels.data(), uw * 4, 0};
  ComPtr<ID3D11Texture2D> tex;
  if (SUCCEEDED(g_device->CreateTexture2D(&td, &init, &tex)))
    g_device->CreateShaderResourceView(tex.Get(), nullptr, &srv);
  w = int(uw);
  h = int(uh);
  return srv;
}

fs::path ExeDir() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  return fs::path(buf).parent_path();
}

void OpenFolder(const fs::path &p) {
  std::error_code ec;
  fs::create_directories(p, ec);
  ShellExecuteW(nullptr, L"open", p.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// ---------------------------------------------------------------------------------------------
// State

enum Page { kHome, kVideos, kTextures, kArenas, kStars, kNewStar, kFiles, kAbout };
Page g_page = kHome;
bool g_fr = false;  // the game's language (wwe12.toml user_language = 4)
TomlFile g_toml;
disc::GameDisc g_disc;
mods::Manager g_mods;
media::EntranceVideos g_videos;
media::TextureMods g_texture_mods;
pac_page::Browser g_files;
arena_page::Page g_arenas;
superstar_page::Page g_stars;
new_star_page::Page g_new_star;
ComPtr<ID3D11ShaderResourceView> g_logo;
int g_logo_w = 0, g_logo_h = 0;

// ---------------------------------------------------------------------------------------------
// Pages

void PageHome() {
  Title("MOD MAKER");
  ImGui::TextWrapped("%s", g_fr ? "Créez vos mods pour WWE '12 PC. Chaque page fabrique un mod (.svrmod) que vous pouvez "
                                  "installer tout de suite et partager. Le launcher (page MODS) les active, les "
                                  "désactive et les supprime."
                                : "Make your own mods for WWE '12 PC. Each page makes a mod (.svrmod) that you can "
                                  "install at once and share. The launcher (MODS page) turns them on and off and "
                                  "removes them.");
  ImGui::Dummy(ImVec2(0, 6));
  if (g_disc.Found())
    ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f), "%s", g_fr ? "Fichiers du jeu trouvés." : "Game files found.");
  else
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s",
                       g_fr ? "Fichiers du jeu introuvables : mettez le Mod Maker à côté de wwe12.exe."
                            : "Game files not found: put the Mod Maker next to wwe12.exe.");
  ImGui::Dummy(ImVec2(0, 10));
  struct Item {
    Page page;
    const char *en, *fr, *what_en, *what_fr;
  };
  const Item items[] = {
      {kVideos, "Entrance videos", "Vidéos d'entrée", "A superstar's titantron video from any video or picture.",
       "La vidéo de titantron d'un catcheur, à partir de n'importe quelle vidéo ou image."},
      {kTextures, "Textures", "Textures", "Change any picture of the game: menus, renders, logos, arenas...",
       "Changer n'importe quelle image du jeu : menus, renders, logos, arènes..."},
      {kArenas, "Arenas", "Arènes", "Change an arena's pictures, or put another arena file in its place.",
       "Changer les images d'une arène, ou mettre un autre fichier d'arène à sa place."},
      {kStars, "Superstars", "Catcheurs", "Change an attire's pictures and see the superstar in 3D.",
       "Changer les images d'une tenue et voir le catcheur en 3D."},
      {kNewStar, "New superstar", "Nouveau catcheur", "Add a superstar to the game, next to the others.",
       "Ajouter un catcheur au jeu, à côté des autres."},
      {kFiles, "Game files", "Fichiers du jeu", "Look inside the game's archives (.pac) and export what's in them.",
       "Regarder dans les archives du jeu (.pac) et exporter ce qu'elles contiennent."},
  };
  for (const auto &it : items) {
    if (ImGui::Button(g_fr ? it.fr : it.en, ImVec2(220, 0)))
      g_page = it.page;
    ImGui::SameLine();
    ImGui::TextDisabled("%s", g_fr ? it.what_fr : it.what_en);
  }
  ImGui::Dummy(ImVec2(0, 10));
  ImGui::TextDisabled("%s", g_fr ? "Bientôt : renders du menu, sons, couleurs des tenues." : "Coming: menu "
                                                                                          "renders, sounds, attire colours.");
  if (ImGui::Button(g_fr ? "Ouvrir le dossier mods" : "Open the mods folder"))
    OpenFolder(ExeDir() / "mods");
}

void PageVideos() {
  Title(g_fr ? "VIDÉOS D'ENTRÉE" : "ENTRANCE VIDEOS");
  g_videos.Draw(g_fr, g_device.Get(), ExeDir(), g_mods);
}

void PageTextures() {
  Title("TEXTURES");
  bool dump = g_toml.GetBool("wwe12_texture_dump", false);
  if (ImGui::Checkbox(g_fr ? "Dump des textures (pour faire des packs)" : "Dump textures (to make texture packs)",
                      &dump)) {
    g_toml.SetBool("wwe12_texture_dump", dump);
    g_toml.Save();
  }
  if (ImGui::Button(g_fr ? "Ouvrir texture_dumps" : "Open texture_dumps"))
    OpenFolder(ExeDir() / "texture_dumps");
  ImGui::SameLine();
  if (ImGui::Button(g_fr ? "Ouvrir textures" : "Open textures"))
    OpenFolder(ExeDir() / "textures");
  ImGui::Dummy(ImVec2(0, 6));
  g_texture_mods.Draw(g_fr);
}

void PageArenas() {
  Title(g_fr ? "ARÈNES" : "ARENAS");
  g_arenas.Draw(g_fr, g_device.Get(), g_disc, g_mods);
}

void PageStars() {
  Title(g_fr ? "CATCHEURS" : "SUPERSTARS");
  g_stars.Draw(g_fr, g_device.Get(), g_disc, g_mods);
}

void PageNewStar() {
  Title(g_fr ? "NOUVEAU CATCHEUR" : "NEW SUPERSTAR");
  g_new_star.Draw(g_fr, g_device.Get(), g_disc, g_mods);
}

void PageFiles() {
  Title(g_fr ? "FICHIERS DU JEU" : "GAME FILES");
  g_files.Draw(g_fr, g_device.Get(), g_disc);
}

void PageAbout() {
  Title(g_fr ? "À PROPOS" : "ABOUT");
  ImGui::TextWrapped("%s", g_fr ? "Mod Maker de WWE '12 PC, portage natif non officiel fait par des fans.\n\n"
                                  "Basé sur le Mod Maker du portage SmackDown vs. Raw 2011 de Kaiko, avec sa permission : "
                                  "format .svrmod, fabricant de vidéos d'entrée et encodeur Bink, lecteurs d'archives "
                                  ".pac (svrfmt), packs de textures. Lecteur Bink porté de FFmpeg (LGPL 2.1 ou plus "
                                  "récent). Logiciel libre sous licence GPL-3.0. Aucune donnée du jeu n'est fournie : "
                                  "le Mod Maker lit vos propres fichiers.\n\nSans lien avec WWE, THQ ou Yuke's."
                                : "WWE '12 PC Mod Maker, for the unofficial native port made by fans.\n\n"
                                  "Based on the Mod Maker of Kaiko's SmackDown vs. Raw 2011 port, with his permission: "
                                  "the .svrmod format, the entrance movie maker and Bink encoder, the .pac readers "
                                  "(svrfmt), texture packs. Bink reader ported from FFmpeg (LGPL 2.1 or later). Free "
                                  "software under the GPL-3.0 licence. No game data is included: the Mod Maker reads "
                                  "your own files.\n\nNot affiliated with WWE, THQ or Yuke's.");
  ImGui::Dummy(ImVec2(0, 8));
  if (ImGui::Button(g_fr ? "Page du projet sur GitHub" : "Project page on GitHub"))
    ShellExecuteW(nullptr, L"open", L"https://github.com/iMrBurn/wwe12-pc", nullptr, nullptr, SW_SHOWNORMAL);
  ImGui::SameLine();
  if (ImGui::Button(g_fr ? "Mod Maker de Kaiko (SvR 2011)" : "Kaiko's Mod Maker (SvR 2011)"))
    ShellExecuteW(nullptr, L"open", L"https://github.com/KaikoClanworth1/wwe-svr2011-pc", nullptr, nullptr,
                  SW_SHOWNORMAL);
  ImGui::Dummy(ImVec2(0, 8));
  ImGui::TextDisabled("Version %s", WWE12_VERSION);
}

void DrawUi() {
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(vp->Size);
  ImGui::Begin("modmaker", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoSavedSettings);
  ImDrawList *dl = ImGui::GetWindowDrawList();
  const ImVec2 o = vp->Pos;
  const float W = vp->Size.x, H = vp->Size.y;
  constexpr float kSide = 260;
  dl->AddRectFilled(o, ImVec2(o.x + kSide, o.y + H), IM_COL32(8, 8, 9, 255));
  Stroke(dl, ImVec2(o.x - 30, o.y + H - 34), kSide + 40, 12, kRed, 30);
  Stroke(dl, ImVec2(o.x - 30, o.y + H - 16), kSide - 40, 5, kGold, 30);
  dl->AddRectFilledMultiColor(ImVec2(o.x + kSide, o.y), ImVec2(o.x + W, o.y + H), IM_COL32(26, 22, 14, 255),
                              IM_COL32(14, 14, 16, 255), IM_COL32(14, 14, 16, 255), IM_COL32(14, 14, 16, 255));

  ImGui::SetCursorPos(ImVec2(20, 18));
  if (g_logo) {
    const float lw = kSide - 60, lh = lw * float(g_logo_h) / float(g_logo_w);
    ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(g_logo.Get()))), ImVec2(lw, lh));
  }
  ImGui::SetCursorPosX(20);
  ImGui::PushFont(g_font_title, 30.0f);
  ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "MOD MAKER");
  ImGui::PopFont();
  ImGui::Dummy(ImVec2(0, 8));
  const char *names_en[] = {"HOME", "VIDEOS", "TEXTURES", "ARENAS", "SUPERSTARS", "NEW SUPERSTAR", "GAME FILES", "ABOUT"};
  const char *names_fr[] = {"ACCUEIL", "VIDÉOS", "TEXTURES", "ARÈNES", "CATCHEURS", "NOUVEAU", "FICHIERS", "À PROPOS"};
  for (int i = 0; i < 8; ++i) {
    ImGui::SetCursorPosX(16);
    const bool sel = g_page == i;
    if (BigButton(g_fr ? names_fr[i] : names_en[i], ImVec2(kSide - 32, 44), sel ? kGold : IM_COL32(28, 28, 32, 255),
                  sel ? IM_COL32(20, 20, 20, 255) : IM_COL32(235, 235, 235, 255)))
      g_page = Page(i);
  }

  ImGui::SetCursorPos(ImVec2(kSide + 36, 26));
  ImGui::BeginChild("page", ImVec2(W - kSide - 60, H - 50));
  switch (g_page) {
  case kHome: PageHome(); break;
  case kVideos: PageVideos(); break;
  case kTextures: PageTextures(); break;
  case kArenas: PageArenas(); break;
  case kStars: PageStars(); break;
  case kNewStar: PageNewStar(); break;
  case kFiles: PageFiles(); break;
  case kAbout: PageAbout(); break;
  }
  ImGui::EndChild();
  ImGui::End();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
    return TRUE;
  switch (msg) {
  case WM_SIZE:
    if (wp != SIZE_MINIMIZED) {
      g_resize_w = LOWORD(lp);
      g_resize_h = HIWORD(lp);
    }
    return 0;
  case WM_GETMINMAXINFO: {
    auto *mm = reinterpret_cast<MINMAXINFO *>(lp);
    mm->ptMinTrackSize = {1000, 640};
    return 0;
  }
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  ImGui_ImplWin32_EnableDpiAwareness();
  g_toml.Load(ExeDir() / "wwe12.toml");
  g_fr = g_toml.GetInt("user_language", 1) == 4;
  g_disc.Open(ExeDir());
  g_mods.Init(ExeDir() / "mods");

  WNDCLASSEXW wc{sizeof(wc)};
  wc.style = CS_CLASSDC;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512) /* IDC_ARROW */);
  wc.lpszClassName = L"WWE12ModMaker";
  RegisterClassExW(&wc);
  const float dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY));
  const int w = int(1240 * dpi), h = int(820 * dpi);
  HWND hwnd = CreateWindowW(wc.lpszClassName, L"WWE '12 Mod Maker", WS_OVERLAPPEDWINDOW,
                            (GetSystemMetrics(SM_CXSCREEN) - w) / 2, (GetSystemMetrics(SM_CYSCREEN) - h) / 2, w, h,
                            nullptr, nullptr, inst, nullptr);
  if (!CreateDevice(hwnd)) {
    MessageBoxW(hwnd, L"Direct3D 11 is not available.", L"WWE '12 Mod Maker", MB_ICONERROR);
    return 1;
  }
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  SetupStyle();
  ImGui::GetStyle().ScaleAllSizes(dpi);
  ImGui::GetStyle().FontScaleDpi = dpi;
  LoadFonts();
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_device.Get(), g_context.Get());
  g_logo = LoadLogo(g_logo_w, g_logo_h);

  bool done = false;
  while (!done) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
      if (msg.message == WM_QUIT)
        done = true;
    }
    if (done)
      break;
    if (g_resize_w && g_resize_h) {
      g_rtv.Reset();
      g_swapchain->ResizeBuffers(0, g_resize_w, g_resize_h, DXGI_FORMAT_UNKNOWN, 0);
      g_resize_w = g_resize_h = 0;
      CreateRenderTarget();
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::PushFont(g_font_body, 0.0f);
    DrawUi();
    ImGui::PopFont();
    ImGui::Render();
    const float clear[4] = {0.05f, 0.05f, 0.06f, 1.0f};
    g_context->OMSetRenderTargets(1, g_rtv.GetAddressOf(), nullptr);
    g_context->ClearRenderTargetView(g_rtv.Get(), clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swapchain->Present(1, 0);
  }

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  DestroyWindow(hwnd);
  CoUninitialize();
  return 0;
}
