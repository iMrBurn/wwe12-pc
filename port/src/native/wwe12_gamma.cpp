// WWE '12 PC: picture brightness at present (issue #48), see wwe12_gamma.h.
#include "wwe12_gamma.h"

#include <cmath>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/graphics/command_processor.h>
#include <rex/graphics/graphics_system.h>
#include <rex/runtime.h>

#include "core/logging.h"

REXCVAR_DEFINE_BOOL(wwe12_game_brightness, true, "WWE12",
                    "Apply the game's own BRIGHTNESS option (its gamma ramp) to the picture, as the "
                    "Xbox's video output does. false = ignore it (1.1 and older).");
REXCVAR_DEFINE_BOOL(wwe12_console_picture, false, "WWE12",
                    "Picture as the Xbox 360 shows it on a TV (its TV correction: darker mid-tones, "
                    "richer colours). false = the picture as the game writes it (PC).");

namespace {

// gamma_ramp_256_entry_table() is protected in the SDK: a derived class may name it, and a pointer
// to that member can then be used on any CommandProcessor (plain C++, no SDK patch).
struct RampPeek : rex::graphics::CommandProcessor {
  static const rex::graphics::reg::DC_LUT_30_COLOR *Table(const rex::graphics::CommandProcessor *c) {
    return (c->*(&RampPeek::gamma_ramp_256_entry_table))();
  }
};

// The 256-entry table the game uploaded (10 bits per channel), or null before the GPU exists.
const rex::graphics::reg::DC_LUT_30_COLOR *GameRampTable() {
  auto *runtime = rex::Runtime::instance();
  if (!runtime)
    return nullptr;
  auto *graphics = static_cast<rex::graphics::GraphicsSystem *>(runtime->graphics_system());
  if (!graphics || !graphics->command_processor())
    return nullptr;
  return RampPeek::Table(graphics->command_processor());
}

// Least-squares fit of v = x^e in log space over the middle of the ramp (both ends carry little
// information and log(0) is undefined). Also returns the largest gap to x^e, to tell a power curve
// from a ramp of another shape (logged: a LUT would be needed then).
float FitExponent(const rex::graphics::reg::DC_LUT_30_COLOR *t, float *max_error, bool *identity) {
  double num = 0.0, den = 0.0;
  float worst_identity = 0.0f;
  for (int i = 0; i < 256; ++i) {
    const float x = float(i) / 255.0f;
    const float v[3] = {float(t[i].color_10_red) / 1023.0f, float(t[i].color_10_green) / 1023.0f,
                        float(t[i].color_10_blue) / 1023.0f};
    for (float c : v) {
      worst_identity = std::fmax(worst_identity, std::fabs(c - x));
      if (i >= 16 && i <= 239 && c > 0.0f) {
        const double lx = std::log(double(x)), lv = std::log(double(c));
        num += lx * lv;
        den += lx * lx;
      }
    }
  }
  *identity = worst_identity <= 2.0f / 1023.0f;
  const float e = den > 0.0 ? float(num / den) : 1.0f;
  float worst = 0.0f;
  for (int i = 0; i < 256; ++i) {
    const float x = float(i) / 255.0f, p = std::pow(x, e);
    worst = std::fmax(worst, std::fabs(float(t[i].color_10_red) / 1023.0f - p));
    worst = std::fmax(worst, std::fabs(float(t[i].color_10_green) / 1023.0f - p));
    worst = std::fmax(worst, std::fabs(float(t[i].color_10_blue) / 1023.0f - p));
  }
  *max_error = worst;
  return e;
}

}  // namespace

float Wwe12GameGammaExponent() {
  static float s_exponent = 1.0f;
  static uint32_t s_calls = 0;
  static uint32_t s_last_hash = 0;
  if (!REXCVAR_GET(wwe12_game_brightness))
    return 1.0f;
  if (s_calls++ % 30 != 0)
    return s_exponent;
  const auto *t = GameRampTable();
  if (!t)
    return s_exponent;
  uint32_t hash = 2166136261u;
  for (int i = 0; i < 256; ++i)
    hash = (hash ^ t[i].value) * 16777619u;
  if (hash == s_last_hash)
    return s_exponent;
  s_last_hash = hash;
  float max_error = 0.0f;
  bool identity = false;
  const float e = FitExponent(t, &max_error, &identity);
  // Keep to sane values: a half-written table (read while the game uploads it) must not flash
  // the screen; the next refit picks up the finished one.
  s_exponent = identity ? 1.0f : std::fmin(std::fmax(e, 0.25f), 4.0f);
  BD_INFO("[gamma] game ramp: {} | exponent {:.3f} (fit error {:.3f}) | first {} {} {} mid {} {} {} "
          "last {} {} {}",
          identity ? "neutral" : "applied", s_exponent, max_error, t[0].color_10_red,
          t[0].color_10_green, t[0].color_10_blue, t[128].color_10_red, t[128].color_10_green,
          t[128].color_10_blue, t[255].color_10_red, t[255].color_10_green, t[255].color_10_blue);
  return s_exponent;
}

bool Wwe12ConsolePicture() { return REXCVAR_GET(wwe12_console_picture); }
