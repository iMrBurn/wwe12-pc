// Version Android : boutons tactiles. L'appli (TouchControls.java) dessine les boutons et envoie
// leur état ici ; on le donne à une manette virtuelle SDL, que le jeu voit comme une vraie manette
// (sdl_input_driver.cpp l'ouvre à SDL_EVENT_GAMEPAD_ADDED, joueur 1 si aucune autre n'est branchée).
#include <jni.h>

#include <SDL3/SDL.h>

#include <mutex>

namespace {

std::mutex g_mutex;
SDL_JoystickID g_id = 0;
SDL_Joystick *g_joystick = nullptr;

// Boutons déclarés. SDL numérote les boutons d'une manette virtuelle dans l'ordre de ce masque
// (le n-ième bouton présent a l'indice n) : un bouton de la manette n'est donc pas son numéro
// SDL_GamepadButton, voir JoystickIndex.
constexpr Uint32 kButtonMask =
    (1u << SDL_GAMEPAD_BUTTON_SOUTH) | (1u << SDL_GAMEPAD_BUTTON_EAST) |
    (1u << SDL_GAMEPAD_BUTTON_WEST) | (1u << SDL_GAMEPAD_BUTTON_NORTH) |
    (1u << SDL_GAMEPAD_BUTTON_BACK) | (1u << SDL_GAMEPAD_BUTTON_START) |
    (1u << SDL_GAMEPAD_BUTTON_LEFT_STICK) | (1u << SDL_GAMEPAD_BUTTON_RIGHT_STICK) |
    (1u << SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) | (1u << SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) |
    (1u << SDL_GAMEPAD_BUTTON_DPAD_UP) | (1u << SDL_GAMEPAD_BUTTON_DPAD_DOWN) |
    (1u << SDL_GAMEPAD_BUTTON_DPAD_LEFT) | (1u << SDL_GAMEPAD_BUTTON_DPAD_RIGHT);

int JoystickIndex(int gamepad_button) {
  return __builtin_popcount(kButtonMask & ((1u << gamepad_button) - 1));
}

// La manette se crée seulement une fois les manettes de SDL démarrées par le jeu.
bool Attach() {
  if (g_joystick)
    return true;
  if (!SDL_WasInit(SDL_INIT_GAMEPAD))
    return false;
  SDL_VirtualJoystickDesc desc;
  SDL_INIT_INTERFACE(&desc);
  desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
  desc.vendor_id = 0x045E;   // présentée comme une manette Xbox 360 : icônes et disposition Xbox
  desc.product_id = 0x028E;
  desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
  desc.nbuttons = static_cast<Uint16>(__builtin_popcount(kButtonMask));
  desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
  desc.button_mask = kButtonMask;
  desc.name = "WWE12 Touch Controls";
  g_id = SDL_AttachVirtualJoystick(&desc);
  if (!g_id)
    return false;
  g_joystick = SDL_OpenJoystick(g_id);
  if (!g_joystick) {
    SDL_DetachVirtualJoystick(g_id);
    g_id = 0;
    return false;
  }
  return true;
}

void Detach() {
  if (g_joystick)
    SDL_CloseJoystick(g_joystick);
  if (g_id)
    SDL_DetachVirtualJoystick(g_id);
  g_joystick = nullptr;
  g_id = 0;
}

Sint16 Axis(float v) {
  if (v > 1.0f)
    v = 1.0f;
  if (v < -1.0f)
    v = -1.0f;
  return static_cast<Sint16>(v * 32767.0f);
}

}  // namespace

extern "C" {

// true quand la manette virtuelle existe (l'appli réessaie tant que le jeu démarre).
JNIEXPORT jboolean JNICALL Java_io_github_imrburn_wwe12_TouchControls_nativeAttach(JNIEnv *, jclass) {
  std::lock_guard lock(g_mutex);
  return Attach();
}

JNIEXPORT void JNICALL Java_io_github_imrburn_wwe12_TouchControls_nativeDetach(JNIEnv *, jclass) {
  std::lock_guard lock(g_mutex);
  Detach();
}

// buttons : un bit par SDL_GamepadButton. Sticks de -1 à 1 (vers le haut = négatif), gâchettes de 0 à 1.
JNIEXPORT void JNICALL Java_io_github_imrburn_wwe12_TouchControls_nativeUpdate(
    JNIEnv *, jclass, jint buttons, jfloat lx, jfloat ly, jfloat rx, jfloat ry, jfloat lt, jfloat rt) {
  std::lock_guard lock(g_mutex);
  if (!Attach())
    return;
  for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && b < 32; ++b) {
    if (kButtonMask & (1u << b))
      SDL_SetJoystickVirtualButton(g_joystick, JoystickIndex(b), (buttons >> b) & 1);
  }
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_LEFTX, Axis(lx));
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_LEFTY, Axis(ly));
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_RIGHTX, Axis(rx));
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_RIGHTY, Axis(ry));
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, Axis(lt));
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, Axis(rt));
}

}  // extern "C"
