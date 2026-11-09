// WWE '12 PC: picture brightness at present (issue #48). The game sets its BRIGHTNESS option
// through the Xbox's scanout gamma ramp (SetGammaRamp -> DC_LUT registers), which the null guest
// GPU records but nothing applied; and the Xbox reshapes the picture for a TV. Both are applied in
// the present pass (reblue/gpu/present.cpp, gamma_correction_ps).
#pragma once

// Exponent of the game's gamma ramp (ramp(x) ~ x^e, fitted on the 256-entry table), 1.0 when the
// ramp is neutral, unknown, or wwe12_game_brightness is off. Cheap: refitted every 30 calls.
float Wwe12GameGammaExponent();

// wwe12_console_picture: true = the Xbox 360's TV correction (darker mid-tones), false = the
// picture as the game writes it (PC, default).
bool Wwe12ConsolePicture();
