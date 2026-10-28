// WWE '12 PC: what to do when the game can't create its graphics device.
//
// Without a host device the game used to keep running behind a black window, with only a line in
// the log (seen on an Intel HD 5500, a Broadwell chip Intel never shipped a Vulkan driver for on
// Windows). Called from Wwe12App::OnPreLaunchModule, which then quits.
//
// With gpu_backend = "any" (AUTO) the next build is tried first: wwe12.exe (Vulkan) starts
// wwe12_d3d12.exe, which starts wwe12_d3d11.exe; only when the last one fails too (or one is
// missing, or the player chose an API) does the player get the message.

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cwchar>
#include <filesystem>
#include <string>

#include <rex/cvar.h>

REXCVAR_DECLARE(std::string, gpu_backend);
#endif

bool Wwe12TryNextBackend() {
#if defined(_WIN32)
  const std::string &backend = REXCVAR_GET(gpu_backend);
  if (!backend.empty() && backend != "any")
    return false;
  wchar_t self[MAX_PATH] = {};
  if (!GetModuleFileNameW(nullptr, self, MAX_PATH))
    return false;
  const std::filesystem::path exe(self);
  const std::wstring name = exe.filename().wstring();
  const wchar_t *next = _wcsicmp(name.c_str(), L"wwe12.exe") == 0         ? L"wwe12_d3d12.exe"
                        : _wcsicmp(name.c_str(), L"wwe12_d3d12.exe") == 0 ? L"wwe12_d3d11.exe"
                                                                          : nullptr;
  if (!next)
    return false;
  const std::filesystem::path target = exe.parent_path() / next;
  std::error_code ec;
  if (!std::filesystem::exists(target, ec))
    return false;
  // Same arguments: the program name is replaced, the rest of the command line kept.
  std::wstring args;
  const wchar_t *cmd = GetCommandLineW();
  if (cmd) {
    const wchar_t *p = cmd;
    if (*p == L'"') {
      p = std::wcschr(p + 1, L'"');
      p = p ? p + 1 : cmd + std::wcslen(cmd);
    } else {
      while (*p && *p != L' ' && *p != L'\t')
        ++p;
    }
    args = p;
  }
  std::wstring line = L"\"" + target.wstring() + L"\"" + args;
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION pi{};
  const std::wstring dir = exe.parent_path().wstring();
  if (!CreateProcessW(target.wstring().c_str(), line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      dir.c_str(), &si, &pi))
    return false;
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
#else
  return false;
#endif
}

void Wwe12ShowVulkanError() {
#if defined(_WIN32)
  MessageBoxW(
      nullptr,
      L"WWE '12 PC could not start its graphics: none of Vulkan 1.2, Direct3D 12 or Direct3D 11 "
      L"works on this PC (or the graphics API chosen in OPTIONS > GRAPHICS > ADVANCED doesn't), "
      L"or your graphics driver is too old or incomplete.\n\n"
      L"- Update your graphics driver from the Intel, AMD or NVIDIA website, then try again.\n"
      L"- On a laptop with two graphics cards, make sure the game uses the dedicated one.\n"
      L"- Set GRAPHICS API to AUTO (launcher or game options) so the game tries every API.\n\n"
      L"WWE '12 PC n'a pas pu démarrer son affichage : ni Vulkan 1.2, ni Direct3D 12, ni Direct3D "
      L"11 ne fonctionnent sur ce PC (ou l'API graphique choisie dans OPTIONS > GRAPHISMES > AVANCÉ "
      L"ne fonctionne pas), ou votre pilote graphique est trop ancien ou incomplet.\n\n"
      L"- Mettez à jour le pilote de votre carte graphique (site d'Intel, AMD ou NVIDIA), puis "
      L"réessayez.\n"
      L"- Sur un PC portable à deux cartes graphiques, vérifiez que le jeu utilise la carte "
      L"dédiée.\n"
      L"- Mettez API GRAPHIQUE sur AUTO (launcher ou options du jeu) pour que le jeu les essaie "
      L"toutes.",
      L"WWE '12 PC", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
#endif
}
