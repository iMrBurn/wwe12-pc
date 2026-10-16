// WWE '12 PC: tells the player why the game can't start when Vulkan is missing.
//
// Without a host device the game used to keep running behind a black window, with only a line in
// the log (seen on an Intel HD 5500, a Broadwell chip Intel never shipped a Vulkan driver for on
// Windows). Called from Wwe12App::OnPreLaunchModule, which then quits.

#if defined(_WIN32)
#include <windows.h>
#endif

void Wwe12ShowVulkanError() {
#if defined(_WIN32)
  MessageBoxW(
      nullptr,
      L"WWE '12 PC could not start Vulkan, which the game needs to draw (Vulkan 1.2 or newer).\n\n"
      L"- Update your graphics driver from the Intel, AMD or NVIDIA website, then try again.\n"
      L"- On a laptop with two graphics cards, make sure the game uses the dedicated one.\n"
      L"- Intel HD Graphics 4000/5000 and older (for example HD 5500) have no Vulkan driver on "
      L"Windows: the game cannot run on them.\n\n"
      L"WWE '12 PC n'a pas pu démarrer Vulkan, dont le jeu a besoin pour l'affichage (Vulkan 1.2 "
      L"ou plus récent).\n\n"
      L"- Mettez à jour le pilote de votre carte graphique (site d'Intel, AMD ou NVIDIA), puis "
      L"réessayez.\n"
      L"- Sur un PC portable à deux cartes graphiques, vérifiez que le jeu utilise la carte "
      L"dédiée.\n"
      L"- Les puces Intel HD Graphics 4000/5000 et plus anciennes (par exemple HD 5500) n'ont pas "
      L"de pilote Vulkan sous Windows : le jeu ne peut pas fonctionner dessus.",
      L"WWE '12 PC", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
#endif
}
