// Entrance movies: the VIDEOS page (a superstar's movie from any video or picture, installed as a .svrmod: type=media,
// files/movies/titantron/<id>.bik; the file number is the superstar's id, see game_disc.h) and the movie editor the
// NEW SUPERSTAR page uses too. The movie itself is made by Kaiko's USER MOVIES maker (video/movie_maker.c, Bink 1 at
// 320 x 320, 30 fps: the top 320 x 220 is the titantron, the big screen; the bottom 320 x 100 the minitron, the
// stage screen under it, and the ramp strip), each part the game's own or the player's. docs/mod_maker.md, step 3.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "game_disc.h"
#include "game_process.h"
#include "imgui.h"
#include "mods.h"

extern "C" {
#include "video/bink_decode.h"
#include "video/movie_maker.h"
}

namespace media {

namespace fs = std::filesystem;

inline std::string Utf8(const std::wstring &w) {
  if (w.empty())
    return {};
  std::string s(WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr, nullptr), 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), s.data(), int(s.size()), nullptr, nullptr);
  return s;
}

inline fs::path PickSave(bool fr, const std::wstring &name) {
  IFileSaveDialog *dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
    return {};
  const COMDLG_FILTERSPEC filter[] = {{L"Mods (*.svrmod)", L"*.svrmod"}};
  dlg->SetFileTypes(1, filter);
  dlg->SetDefaultExtension(L"svrmod");
  dlg->SetFileName(name.c_str());
  dlg->SetTitle(fr ? L"Enregistrer le mod" : L"Save the mod");
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

// An entrance movie in the making: its top (the titantron, the big screen) and its bottom (the minitron, the screen
// under it on the stage and the ramp strip), each the game's own movie or a video / picture of the player's (the
// minitron can also be the titantron's), with the before / after pictures; made by movie_make on a worker thread.
class MovieEditor {
 public:
  ~MovieEditor() { Wait(); }

  // The game's movie of this superstar (a .bik on disk), "" if it has none: the "before" picture and the parts kept.
  void SetGame(ID3D11Device *device, const fs::path &movie) {
    device_ = device;
    game_movie_ = movie;
    game_tex_.Reset();
    if (game_movie_.empty()) {
      top_ = top_ == kGame ? kOwn : top_;
      bottom_ = bottom_ == kGame ? kSameAsTop : bottom_;
    } else {
      WCHAR err[256];
      if (BinkReader *r = bink_open(game_movie_.c_str(), err, 256)) {
        const int w = bink_width(r), h = bink_height(r), frame = std::min(bink_frames(r) - 1, 90);
        for (int i = 0; i <= frame; ++i)
          bink_next(r);
        std::vector<uint8_t> bgra(size_t(w) * h * 4);
        bink_bgra(r, bgra.data());
        bink_close(r);
        game_tex_ = Texture(bgra.data(), w, h);
      }
    }
    MakePreview();
  }

  // The choices and the pictures. Returns nothing: Ready() says whether there is something to make.
  void Draw(bool fr, const char *id) {
    ImGui::PushID(id);
    ImGui::BeginDisabled(Busy());
    const bool have_game = !game_movie_.empty();
    // Titantron.
    ImGui::Text("%s", fr ? "Titantron (grand écran)" : "Titantron (big screen)");
    ImGui::SameLine(190);
    ImGui::BeginDisabled(!have_game);
    if (ImGui::RadioButton(fr ? "Celui du jeu##t" : "The game's##t", top_ == kGame)) {
      top_ = kGame;
      MakePreview();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::RadioButton(fr ? "Le vôtre##t" : "Yours##t", top_ == kOwn)) {
      top_ = kOwn;
      MakePreview();
    }
    if (top_ == kOwn) {
      ImGui::SameLine();
      if (ImGui::Button(fr ? "Vidéo ou image...##t" : "Video or picture...##t")) {
        const fs::path p = PickSource(fr);
        if (!p.empty()) {
          top_file_ = p;
          MakePreview();
        }
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", top_file_.empty() ? "" : Utf8(top_file_.filename().wstring()).c_str());
      const char *fits_en[] = {"Whole picture (black bars)", "Fill the screen (edges cut)", "Stretch"};
      const char *fits_fr[] = {"Image entière (bandes noires)", "Remplir l'écran (bords coupés)", "Étirer"};
      ImGui::Dummy(ImVec2(182, 0));
      for (int i = 0; i < 3; ++i) {
        ImGui::SameLine();
        if (ImGui::RadioButton(fr ? fits_fr[i] : fits_en[i], fit_ == i)) {
          fit_ = i;
          MakePreview();
        }
      }
    }
    // Minitron.
    ImGui::Text("%s", fr ? "Minitron (écran de la scène)" : "Minitron (stage screen)");
    ImGui::SameLine(190);
    ImGui::BeginDisabled(!have_game);
    if (ImGui::RadioButton(fr ? "Celui du jeu##b" : "The game's##b", bottom_ == kGame)) {
      bottom_ = kGame;
      MakePreview();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(top_ != kOwn);
    if (ImGui::RadioButton(fr ? "Comme le titantron##b" : "As the titantron##b", bottom_ == kSameAsTop)) {
      bottom_ = kSameAsTop;
      MakePreview();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::RadioButton(fr ? "Le vôtre##b" : "Yours##b", bottom_ == kOwn)) {
      bottom_ = kOwn;
      MakePreview();
    }
    if (bottom_ == kOwn) {
      ImGui::SameLine();
      if (ImGui::Button(fr ? "Vidéo ou image...##b" : "Video or picture...##b")) {
        const fs::path p = PickSource(fr);
        if (!p.empty()) {
          bottom_file_ = p;
          MakePreview();
        }
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", bottom_file_.empty() ? "" : Utf8(bottom_file_.filename().wstring()).c_str());
    }
    ImGui::SetNextItemWidth(260);
    ImGui::SliderInt(fr ? "Durée max." : "Max. length", &max_seconds_, 0, 180,
                     max_seconds_ ? "%d s" : (fr ? "toute la vidéo" : "whole video"));
    ImGui::SetNextItemWidth(260);
    ImGui::SliderFloat(fr ? "Aperçu à" : "Preview at", &preview_at_, 0.0f, 60.0f, "%.0f s");
    if (ImGui::IsItemDeactivatedAfterEdit())
      MakePreview();
    ImGui::EndDisabled();

    // Before / after.
    const float s = 200;
    ImGui::BeginGroup();
    ImGui::TextDisabled("%s", fr ? "Dans le jeu" : "In the game");
    Picture(game_tex_.Get(), s);
    ImGui::EndGroup();
    ImGui::SameLine(0, 24);
    ImGui::BeginGroup();
    ImGui::TextDisabled("%s", fr ? "Votre vidéo" : "Your movie");
    Picture(new_tex_.Get(), s);
    ImGui::EndGroup();
    if (!preview_err_.empty())
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", preview_err_.c_str());
    ImGui::PopID();
  }

  // Something of the player's, and every part has its picture.
  bool Ready() const {
    const bool top_ok = top_ == kGame ? !game_movie_.empty() : !top_file_.empty();
    const bool bottom_ok = bottom_ == kGame ? !game_movie_.empty() : bottom_ == kSameAsTop ? top_ == kOwn : !bottom_file_.empty();
    return top_ok && bottom_ok && (top_ == kOwn || bottom_ != kGame);
  }
  // What the movie is made of, for a mod's description.
  std::string Parts() const {
    return std::string(top_ == kOwn ? "own titantron" : "the game's titantron") + ", " +
           (bottom_ == kGame ? "the game's minitron" : bottom_ == kSameAsTop ? "the same minitron" : "own minitron");
  }

  // Makes the movie into `out` on a worker thread.
  void Start(const fs::path &out) {
    if (!notify_) {
      WNDCLASSW wc{};
      wc.lpfnWndProc = ProgressProc;
      wc.hInstance = GetModuleHandleW(nullptr);
      wc.lpszClassName = L"WWE12MovieProgress";
      RegisterClassW(&wc);
      notify_ = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
      SetWindowLongPtrW(notify_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
    memset(&job_, 0, sizeof(job_));
    wcsncpy_s(job_.video, TopSource().c_str(), _TRUNCATE);
    wcsncpy_s(job_.bottom, BottomSource().c_str(), _TRUNCATE);
    job_.fit = top_ == kOwn ? fit_ : MOVIE_FILL;  // (the game's titantron is already 16:9)
    job_.max_seconds = max_seconds_;
    wcsncpy_s(job_.out, out.c_str(), _TRUNCATE);
    job_.notify = notify_;
    job_.msg = WM_APP + 0x31;
    progress_ = 0;
    done_ = false;
    worker_ = std::thread([this] {
      CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      ok_ = movie_make(&job_) != 0;
      CoUninitialize();
      done_ = true;
    });
  }
  bool Busy() const { return worker_.joinable(); }
  // True once when a movie started is done: Ok(), Error(), Seconds().
  bool Finished() {
    if (!worker_.joinable() || !done_)
      return false;
    worker_.join();
    return true;
  }
  bool Ok() const { return ok_; }
  std::string Error() const { return Utf8(job_.err); }
  int Seconds() const { return job_.frames / 30; }
  // The progress bar and its Stop button, while it runs.
  void DrawProgress(bool fr) {
    if (!worker_.joinable())
      return;
    ImGui::SameLine();
    ImGui::ProgressBar(progress_ / 100.0f, ImVec2(220, 0));
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Arrêter" : "Stop"))
      job_.cancel = 1;
  }

 private:
  enum { kGame = 0, kOwn = 1, kSameAsTop = 2 };

  std::wstring TopSource() const { return top_ == kOwn ? top_file_.wstring() : game_movie_.wstring(); }
  std::wstring BottomSource() const {
    return bottom_ == kGame ? game_movie_.wstring() : bottom_ == kSameAsTop ? top_file_.wstring() : bottom_file_.wstring();
  }

  void Wait() {
    if (worker_.joinable()) {
      job_.cancel = 1;
      worker_.join();
    }
  }

  void MakePreview() {
    new_tex_.Reset();
    preview_err_.clear();
    if (!Ready())
      return;
    std::vector<uint8_t> bgra(size_t(MOVIE_W) * MOVIE_H * 4);
    WCHAR err[512] = {};
    if (movie_preview(TopSource().c_str(), BottomSource().c_str(), top_ == kOwn ? fit_ : MOVIE_FILL, preview_at_,
                      bgra.data(), err, 512))
      new_tex_ = Texture(bgra.data(), MOVIE_W, MOVIE_H);
    else
      preview_err_ = Utf8(err);
  }

  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Texture(const uint8_t *bgra, int w, int h) {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = UINT(w);
    td.Height = UINT(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{bgra, UINT(w * 4), 0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    if (device_ && SUCCEEDED(device_->CreateTexture2D(&td, &init, &tex)))
      device_->CreateShaderResourceView(tex.Get(), nullptr, &srv);
    return srv;
  }

  static void Picture(ID3D11ShaderResourceView *srv, float s) {
    if (srv) {
      ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(srv))), ImVec2(s, s));
    } else {
      const ImVec2 p = ImGui::GetCursorScreenPos();
      ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + s, p.y + s), IM_COL32(0, 0, 0, 255));
      ImGui::Dummy(ImVec2(s, s));
    }
  }

  static fs::path PickSource(bool fr) {
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
      return {};
    const COMDLG_FILTERSPEC filter[] = {
        {fr ? L"Vidéos et images" : L"Videos and pictures",
         L"*.mp4;*.m4v;*.mov;*.wmv;*.avi;*.mkv;*.webm;*.bik;*.png;*.jpg;*.jpeg;*.bmp;*.gif"},
        {L"*.*", L"*.*"}};
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

  // movie_make reports its progress as window messages: a message-only window gets them.
  static LRESULT CALLBACK ProgressProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_APP + 0x31) {
      auto *self = reinterpret_cast<MovieEditor *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
      if (self)
        self->progress_ = int(wp);
      return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
  }

  ID3D11Device *device_ = nullptr;
  fs::path game_movie_, top_file_, bottom_file_;
  int top_ = kOwn, bottom_ = kGame;
  int fit_ = MOVIE_FILL;
  int max_seconds_ = 60;
  float preview_at_ = 3.0f;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> game_tex_, new_tex_;
  std::string preview_err_;

  HWND notify_ = nullptr;
  MovieJob job_{};
  std::thread worker_;
  std::atomic<bool> done_{false};
  std::atomic<int> progress_{0};
  bool ok_ = false;
};

// The temporary folder for movies being made and game movies copied out.
inline fs::path TempDir() {
  wchar_t tmp[MAX_PATH];
  GetTempPathW(MAX_PATH, tmp);
  const fs::path dir = fs::path(tmp) / L"WWE12 Mod Maker";
  std::error_code ec;
  fs::create_directories(dir, ec);
  return dir;
}

// A game movie (movies/titantron/<file>) copied out of the game files, "" if it has none.
inline fs::path CopyGameMovie(const disc::GameDisc &disc, const std::string &file) {
  std::vector<uint8_t> bytes;
  if (!disc.Read("movies/titantron/" + file, bytes))
    return {};
  const fs::path p = TempDir() / ("game_" + file);
  std::ofstream(p, std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  return p;
}

// VIDEOS page: a superstar's entrance movie (titantron and minitron) from any video or picture, installed as a
// .svrmod (type=media, files/movies/titantron/<id>.bik; the file number is the superstar's id, see game_disc.h).
class EntranceVideos {
 public:
  void Draw(bool fr, ID3D11Device *device, const fs::path &exe_dir, mods::Manager &mods) {
    device_ = device;
    if (!loaded_)
      Load(exe_dir);
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Changez la vidéo d'entrée d'un catcheur : le titantron (le grand écran) et le "
                                  "minitron (l'écran de la scène, sous le grand écran), ensemble ou l'un sans l'autre, "
                                  "avec une vidéo ou une image à vous. Le Mod Maker en fait un mod (.svrmod) et "
                                  "l'installe ; vous pouvez aussi partager le fichier."
                                : "Change a superstar's entrance video: the titantron (the big screen) and the minitron "
                                  "(the stage screen, under the big screen), together or one without the other, with "
                                  "a video or a picture of yours. The Mod Maker makes a mod (.svrmod) of it and "
                                  "installs it; you can share the file too.");
    ImGui::Dummy(ImVec2(0, 6));
    if (!disc_.Found() || stars_.empty()) {
      ImGui::TextColored(red, "%s", fr ? "Fichiers du jeu introuvables (dossier game ou .iso à côté du Mod Maker)."
                                       : "Game files not found (game folder or .iso next to the Mod Maker).");
      return;
    }
    if (editor_.Finished())
      Finish(fr, mods);
    ImGui::BeginDisabled(editor_.Busy());

    // The superstar.
    ImGui::SetNextItemWidth(360);
    const std::string current = star_ < 0 ? (fr ? "Choisir..." : "Choose...") : Label(stars_[star_]);
    if (ImGui::BeginCombo(fr ? "Catcheur" : "Superstar", current.c_str(), ImGuiComboFlags_HeightLarge)) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
      ImGui::InputTextWithHint("##filter", fr ? "Rechercher" : "Search", filter_, sizeof(filter_));
      for (int i = 0; i < int(stars_.size()); ++i) {
        const std::string l = Label(stars_[i]);
        if (filter_[0] && !Contains(l, filter_))
          continue;
        if (ImGui::Selectable(l.c_str(), i == star_)) {
          star_ = i;
          editor_.SetGame(device_, CopyGameMovie(disc_, stars_[star_].file));
        }
      }
      ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (star_ < 0)
      return;
    editor_.Draw(fr, "videos");
    ImGui::BeginDisabled(editor_.Busy());
    ImGui::SetNextItemWidth(260);
    ImGui::InputText(fr ? "Auteur (facultatif)" : "Author (optional)", author_, sizeof(author_));
    ImGui::EndDisabled();

    // Make it.
    const bool running = game::IsRunning();
    ImGui::BeginDisabled(editor_.Busy() || running || !editor_.Ready());
    if (ImGui::Button(fr ? "Créer le mod et l'installer..." : "Make the mod and install it..."))
      Start(fr);
    ImGui::EndDisabled();
    editor_.DrawProgress(fr);
    if (running)
      ImGui::TextColored(red, "%s", fr ? "Le jeu est lancé : fermez-le pour installer le mod."
                                       : "The game is running: close it to install the mod.");
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
  }

 private:
  struct Star {
    int id = 0;
    std::string file, name;  // file: "139.bik"
  };

  static std::string Label(const Star &s) {
    return s.name.empty() ? "Video " + s.file.substr(0, s.file.size() - 4) : s.name;
  }
  static bool Contains(const std::string &s, const char *what) {
    std::string a = s, b = what;
    for (auto *x : {&a, &b})
      std::transform(x->begin(), x->end(), x->begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return a.find(b) != std::string::npos;
  }
  // Every movie of movies/titantron: the superstars' (by name) first, then the others by number.
  void Load(const fs::path &exe_dir) {
    loaded_ = true;
    disc_.Open(exe_dir);
    const auto names = disc::SuperstarNames(disc_);
    for (const auto &f : disc_.List("movies/titantron")) {
      if (f.size() != 7 || _stricmp(f.c_str() + 3, ".bik") != 0)
        continue;
      Star s;
      s.file = f;
      s.id = atoi(f.c_str());
      const auto it = names.find(s.id);
      if (it != names.end())
        s.name = it->second;
      stars_.push_back(s);
    }
    std::sort(stars_.begin(), stars_.end(), [](const Star &a, const Star &b) {
      if (a.name.empty() != b.name.empty())
        return !a.name.empty();
      return a.name.empty() ? a.id < b.id : a.name < b.name;
    });
  }

  void Start(bool fr) {
    const Star &s = stars_[star_];
    std::wstring name;
    for (char c : Label(s))
      name += std::isalnum(static_cast<unsigned char>(c)) ? wchar_t(c) : L'_';
    out_ = PickSave(fr, name + L"_entrance.svrmod");
    if (out_.empty())
      return;
    made_ = TempDir() / ("made_" + s.file);
    message_.clear();
    editor_.Start(made_);
  }

  // On the UI thread once the movie is made: the .svrmod, then its install.
  void Finish(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    if (!editor_.Ok()) {
      message_ = (fr ? "La vidéo n'a pas pu être faite : " : "The movie could not be made: ") + editor_.Error();
      return;
    }
    const Star &s = stars_[star_];
    std::ifstream f(made_, std::ios::binary);
    std::vector<uint8_t> bik((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    std::error_code ec;
    fs::remove(made_, ec);
    const std::string number = s.file.substr(0, 3);
    std::string manifest = "game=wwe12\ntype=media\nid=entrance_video_" + number + "\nname=" +
                           (fr ? "Vidéo d'entrée : " : "Entrance video: ") + Label(s) + "\nversion=1.0\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + Label(s) + "'s entrance movie (movies/titantron/" + s.file + ": " +
                editor_.Parts() + "), made with the WWE '12 Mod Maker.\n";
    if (!mods::Manager::WriteZip(out_, {{"manifest.txt", std::vector<uint8_t>(manifest.begin(), manifest.end())},
                                        {"files/movies/titantron/" + s.file, std::move(bik)}})) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + Utf8(out_.wstring());
      return;
    }
    mods.InstallFile(fr, out_);
    message_ok_ = true;
    message_ = (fr ? "Mod créé et installé (" : "Mod made and installed (") + std::to_string(editor_.Seconds()) +
               " s) : " + Utf8(out_.wstring());
  }

  ID3D11Device *device_ = nullptr;
  bool loaded_ = false;
  disc::GameDisc disc_;
  std::vector<Star> stars_;
  int star_ = -1;
  char filter_[64] = {};
  fs::path out_, made_;
  char author_[64] = {};
  MovieEditor editor_;
  std::string message_;
  bool message_ok_ = false;
};

// MODS page, "Textures": the step-by-step guide (dump -> edit -> mod) and a texture mod made from a folder of
// the player's pictures (type=textures: every .png / .dds whose name ends in _<16 hex digits>, under textures/).
class TextureMods {
 public:
  void Draw(bool fr) {
    ImGui::Text("%s", fr ? "Changer une image du jeu (menus, renders, logos...)" : "Change a picture of the game (menus, "
                                                                                    "renders, logos...)");
    const char *steps_en[] = {
        "Tick \"Dump textures\" above, then play: go where the picture shows (menu, superstar select...). Every "
        "texture shown is saved once as a PNG in texture_dumps.",
        "Open texture_dumps and find the picture (size_format_hash.png). Untick the dump when you're done: it "
        "slows the game down a little.",
        "Copy it into the textures folder and edit it with any image editor (Paint.NET, GIMP, Photoshop...). Keep "
        "the end of its name (_ and the 16 characters of the hash); it can be bigger than the original (sharper), "
        "transparency is kept.",
        "Start the game again: your picture replaces the game's.",
        "To share it: put your pictures in a folder of their own, then \"Make a texture mod...\" below. The "
        ".svrmod installs with \"Install a mod...\"."};
    const char *steps_fr[] = {
        "Cochez « Dump des textures » ci-dessus, puis jouez : allez là où l'image s'affiche (menu, choix du "
        "catcheur...). Chaque texture affichée est enregistrée une fois en PNG dans texture_dumps.",
        "Ouvrez texture_dumps et trouvez l'image (taille_format_empreinte.png). Décochez le dump quand vous avez "
        "fini : il ralentit un peu le jeu.",
        "Copiez-la dans le dossier textures et retouchez-la avec n'importe quel logiciel d'image (Paint.NET, GIMP, "
        "Photoshop...). Gardez la fin de son nom (_ et les 16 caractères de l'empreinte) ; elle peut être plus "
        "grande que l'originale (plus nette), la transparence est gardée.",
        "Relancez le jeu : votre image remplace celle du jeu.",
        "Pour la partager : mettez vos images dans un dossier à part, puis « Créer un mod de textures... » "
        "ci-dessous. Le .svrmod s'installe avec « Installer un mod... »."};
    for (int i = 0; i < 5; ++i) {
      ImGui::Text("%d.", i + 1);
      ImGui::SameLine();
      ImGui::TextWrapped("%s", fr ? steps_fr[i] : steps_en[i]);
    }
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##texname", fr ? "Nom du mod" : "Mod name", name_, sizeof(name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##texauthor", fr ? "Auteur (facultatif)" : "Author (optional)", author_,
                             sizeof(author_));
    if (ImGui::Button(fr ? "Créer un mod de textures..." : "Make a texture mod..."))
      Make(fr);
    if (!message_.empty())
      ImGui::TextColored(ok_ ? ImVec4(0.55f, 0.85f, 0.45f, 1.0f) : ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s",
                         message_.c_str());
  }

 private:
  // A texture pack file: name ending in _<16 hex digits>, .png or .dds (game: wwe12_texture_packs.h).
  static bool IsPackFile(const fs::path &p) {
    std::wstring ext = p.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    const std::wstring stem = p.stem().wstring();
    if ((ext != L".png" && ext != L".dds") || stem.size() < 17 || stem[stem.size() - 17] != L'_')
      return false;
    return std::all_of(stem.end() - 16, stem.end(), [](wchar_t c) { return iswxdigit(c) != 0; });
  }

  static fs::path PickFolder(bool fr) {
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
      return {};
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(fr ? L"Dossier de vos images" : L"Folder of your pictures");
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

  void Make(bool fr) {
    message_.clear();
    ok_ = false;
    const fs::path dir = PickFolder(fr);
    if (dir.empty())
      return;
    std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
    std::error_code ec;
    size_t skipped = 0;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
      if (!it->is_regular_file(ec))
        continue;
      if (!IsPackFile(it->path())) {
        ++skipped;
        continue;
      }
      std::ifstream f(it->path(), std::ios::binary);
      std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
      const std::u8string rel = fs::relative(it->path(), dir, ec).generic_u8string();
      entries.push_back({"textures/" + std::string(rel.begin(), rel.end()), std::move(bytes)});
    }
    if (entries.empty()) {
      message_ = fr ? "Aucune image de pack dans ce dossier (nom finissant par _<empreinte>.png ou .dds)."
                    : "No pack picture in this folder (name ending in _<hash>.png or .dds).";
      return;
    }
    const std::string name = name_[0] ? name_ : Utf8(dir.filename().wstring());
    std::wstring file;
    std::string id;
    for (unsigned char c : name) {
      const bool keep = std::isalnum(c) != 0;
      file += keep ? wchar_t(c) : L'_';
      id += keep ? char(c) : '_';
    }
    const fs::path out = PickSave(fr, file + L".svrmod");
    if (out.empty())
      return;
    std::string manifest = "game=wwe12\ntype=textures\nid=textures_" + id + "\nname=" + name + "\nversion=1.0\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + std::to_string(entries.size()) + " texture(s), made with the WWE '12 launcher.\n";
    entries.insert(entries.begin(), {"manifest.txt", std::vector<uint8_t>(manifest.begin(), manifest.end())});
    if (!mods::Manager::WriteZip(out, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + Utf8(out.wstring());
      return;
    }
    ok_ = true;
    message_ = (fr ? "Mod créé (" : "Mod made (") + std::to_string(entries.size() - 1) +
               (fr ? " image(s)) : " : " picture(s)): ") + Utf8(out.wstring());
    if (skipped)
      message_ += fr ? " ; " + std::to_string(skipped) + " fichier(s) laissé(s) de côté (nom sans empreinte)."
                     : "; " + std::to_string(skipped) + " file(s) left out (no hash in the name).";
  }

  char name_[64] = {}, author_[64] = {};
  std::string message_;
  bool ok_ = false;
};

}  // namespace media
