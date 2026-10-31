// iOS version: on-screen controller drawn over the game (like android/TouchControls.java, same default
// layout). Its state goes to a virtual SDL gamepad that the game sees like a real controller
// (sdl_input_driver.cpp opens it at SDL_EVENT_GAMEPAD_ADDED, player 1 if no other pad is plugged).
// Hidden while a real controller (Bluetooth...) is connected. Not compiled on any other platform.
#import <UIKit/UIKit.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

namespace {

std::mutex g_mutex;
SDL_JoystickID g_id = 0;
SDL_Joystick* g_joystick = nullptr;

// SDL numbers the buttons of a virtual gamepad in the order of this mask (the n-th button present
// has index n): same table as android/wwe12_touch.cpp.
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

// Game rumble -> the phone's haptic engine.
bool Rumble(void*, Uint16 low, Uint16 high) {
  if (!low && !high) return true;
  static CFAbsoluteTime last = 0;
  const CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
  if (now - last < 0.12) return true;
  last = now;
  dispatch_async(dispatch_get_main_queue(), ^{
    UIImpactFeedbackGenerator* g = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
    [g impactOccurred];
  });
  return true;
}

// The pad can only be created once the game has started SDL's gamepads: the caller retries.
bool Attach() {
  if (g_joystick) return true;
  if (!SDL_WasInit(SDL_INIT_GAMEPAD)) return false;
  SDL_VirtualJoystickDesc desc;
  SDL_INIT_INTERFACE(&desc);
  desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
  desc.vendor_id = 0x045E;  // shown as an Xbox 360 pad: Xbox icons and layout
  desc.product_id = 0x028E;
  desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
  desc.nbuttons = static_cast<Uint16>(__builtin_popcount(kButtonMask));
  desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
  desc.button_mask = kButtonMask;
  desc.name = "WWE12 Touch Controls";
  desc.Rumble = Rumble;
  g_id = SDL_AttachVirtualJoystick(&desc);
  if (!g_id) return false;
  g_joystick = SDL_OpenJoystick(g_id);
  if (!g_joystick) {
    SDL_DetachVirtualJoystick(g_id);
    g_id = 0;
    return false;
  }
  return true;
}

Sint16 Axis(float v) {
  v = std::max(-1.0f, std::min(1.0f, v));
  return static_cast<Sint16>(v * 32767.0f);
}

// Triggers: SDL reads a virtual axis on its whole range (-32768 = released, 32767 = pressed).
Sint16 Trigger(float v) {
  v = std::max(0.0f, std::min(1.0f, v));
  return static_cast<Sint16>(v * 65535.0f - 32768.0f);
}

void SendState(int buttons, float lx, float ly, float rx, float ry, float lt, float rt) {
  std::lock_guard lock(g_mutex);
  if (!Attach()) return;
  for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && b < 32; ++b) {
    if (kButtonMask & (1u << b)) SDL_SetJoystickVirtualButton(g_joystick, JoystickIndex(b), (buttons >> b) & 1);
  }
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_LEFTX, Axis(lx));
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_LEFTY, Axis(ly));
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_RIGHTX, Axis(rx));
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_RIGHTY, Axis(ry));
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, Trigger(lt));
  SDL_SetJoystickVirtualAxis(g_joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, Trigger(rt));
}

constexpr int kTrigL = 1, kTrigR = 2;

struct Control {
  const char* label;
  int button;  // SDL_GamepadButton, -1 = none
  int trig;    // kTrigL / kTrigR
  int stick;   // 0 none, 1 left, 2 right
  float fx, fy, fr;  // centre (fraction of width / height), radius (fraction of height)
  UIColor* color;
  CGPoint c;
  float r;
  const void* touch;  // the finger holding it
  float ax, ay;       // stick knob (-1..1)
};

UIColor* Rgb(int r, int g, int b) { return [UIColor colorWithRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:1]; }

}  // namespace

@interface WWE12TouchView : UIView
- (void)tick;
@end

@implementation WWE12TouchView {
  std::vector<Control> _c;
  CGSize _laidOut;
  BOOL _shown;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.multipleTouchEnabled = YES;
    self.opaque = NO;
    self.backgroundColor = UIColor.clearColor;
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _shown = YES;
    UIColor* gray = Rgb(150, 150, 150);
    // Same default layout as TouchControls.buildDefaults() (Android).
    _c = {
        {"", -1, 0, 1, 0.13f, 0.66f, 0.17f, gray},
        {"▲", SDL_GAMEPAD_BUTTON_DPAD_UP, 0, 0, 0.30f, 0.70f, 0.055f, gray},
        {"▼", SDL_GAMEPAD_BUTTON_DPAD_DOWN, 0, 0, 0.30f, 0.90f, 0.055f, gray},
        {"◀", SDL_GAMEPAD_BUTTON_DPAD_LEFT, 0, 0, 0.25f, 0.80f, 0.055f, gray},
        {"▶", SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 0, 0, 0.35f, 0.80f, 0.055f, gray},
        {"LT", -1, kTrigL, 0, 0.06f, 0.16f, 0.075f, gray},
        {"LB", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 0, 0, 0.17f, 0.16f, 0.075f, gray},
        {"A", SDL_GAMEPAD_BUTTON_SOUTH, 0, 0, 0.86f, 0.80f, 0.08f, Rgb(90, 180, 70)},
        {"B", SDL_GAMEPAD_BUTTON_EAST, 0, 0, 0.94f, 0.62f, 0.08f, Rgb(210, 60, 60)},
        {"X", SDL_GAMEPAD_BUTTON_WEST, 0, 0, 0.78f, 0.62f, 0.08f, Rgb(60, 120, 220)},
        {"Y", SDL_GAMEPAD_BUTTON_NORTH, 0, 0, 0.86f, 0.44f, 0.08f, Rgb(230, 200, 50)},
        {"", -1, 0, 2, 0.66f, 0.80f, 0.12f, gray},
        {"RB", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 0, 0, 0.83f, 0.16f, 0.075f, gray},
        {"RT", -1, kTrigR, 0, 0.94f, 0.16f, 0.075f, gray},
        {"BACK", SDL_GAMEPAD_BUTTON_BACK, 0, 0, 0.42f, 0.08f, 0.06f, gray},
        {"START", SDL_GAMEPAD_BUTTON_START, 0, 0, 0.58f, 0.08f, 0.06f, gray},
    };
  }
  return self;
}

- (void)layoutControls {
  const CGSize s = self.bounds.size;
  if (CGSizeEqualToSize(s, _laidOut)) return;
  _laidOut = s;
  for (auto& k : _c) {
    k.c = CGPointMake(k.fx * s.width, k.fy * s.height);
    k.r = k.fr * s.height;
  }
}

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  return _shown ? self : nil;  // swallow every touch: the game reads the pad, not the mouse
}

- (void)send {
  int buttons = 0;
  float lt = 0, rt = 0, sx[3] = {0, 0, 0}, sy[3] = {0, 0, 0};
  for (const auto& k : _c) {
    if (!k.touch) continue;
    if (k.button >= 0) buttons |= 1 << k.button;
    if (k.trig == kTrigL) lt = 1;
    if (k.trig == kTrigR) rt = 1;
    if (k.stick) {
      // 10 % dead zone, rescaled so the stick still reaches full tilt.
      const float len = std::hypot(k.ax, k.ay);
      if (len > 0.1f) {
        const float m = std::min(1.0f, (len - 0.1f) / 0.9f) / len;
        sx[k.stick] = k.ax * m;
        sy[k.stick] = k.ay * m;
      }
    }
  }
  SendState(buttons, sx[1], sy[1], sx[2], sy[2], lt, rt);
  [self setNeedsDisplay];
}

- (int)controlAt:(CGPoint)p stickOK:(BOOL)stickOK {
  int best = -1;
  float bestD = 1e9f;
  for (size_t i = 0; i < _c.size(); ++i) {
    const Control& k = _c[i];
    if (k.touch) continue;
    if (k.stick && !stickOK) continue;
    const float d = std::hypot(p.x - k.c.x, p.y - k.c.y);
    const float reach = k.r * (k.stick ? 1.3f : 1.25f);
    if (d <= reach && d / reach < bestD) {
      bestD = d / reach;
      best = static_cast<int>(i);
    }
  }
  return best;
}

- (void)updateStick:(Control&)k at:(CGPoint)p {
  float dx = (p.x - k.c.x) / k.r, dy = (p.y - k.c.y) / k.r;
  const float len = std::hypot(dx, dy);
  if (len > 1) {
    dx /= len;
    dy /= len;
  }
  k.ax = dx;
  k.ay = dy;
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [self layoutControls];
  for (UITouch* t in touches) {
    const CGPoint p = [t locationInView:self];
    const int i = [self controlAt:p stickOK:YES];
    if (i < 0) continue;
    _c[i].touch = (__bridge const void*)t;
    if (_c[i].stick) [self updateStick:_c[i] at:p];
  }
  [self send];
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  for (UITouch* t in touches) {
    const CGPoint p = [t locationInView:self];
    const void* key = (__bridge const void*)t;
    for (size_t i = 0; i < _c.size(); ++i) {
      Control& k = _c[i];
      if (k.touch != key) continue;
      if (k.stick) {
        [self updateStick:k at:p];
      } else if (std::hypot(p.x - k.c.x, p.y - k.c.y) > k.r * 1.25f) {
        // Slid off a button: release it and take the one under the finger (d-pad rolling).
        k.touch = nullptr;
        const int j = [self controlAt:p stickOK:NO];
        if (j >= 0) _c[j].touch = key;
      }
      break;
    }
  }
  [self send];
}

- (void)releaseTouches:(NSSet<UITouch*>*)touches {
  for (UITouch* t in touches) {
    const void* key = (__bridge const void*)t;
    for (auto& k : _c) {
      if (k.touch != key) continue;
      k.touch = nullptr;
      k.ax = k.ay = 0;
    }
  }
  [self send];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { [self releaseTouches:touches]; }
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { [self releaseTouches:touches]; }

// Called twice a second: retries the virtual pad until the game has started SDL's gamepads, and
// hides the buttons while a real controller is connected.
- (void)tick {
  {
    std::lock_guard lock(g_mutex);
    Attach();
  }
  BOOL real = NO;
  Class gc = NSClassFromString(@"GCController");
  if (gc) real = [[gc performSelector:@selector(controllers)] count] > 0;
  if (real == _shown) {
    _shown = !real;
    self.hidden = real;
    if (real) {
      for (auto& k : _c) {
        k.touch = nullptr;
        k.ax = k.ay = 0;
      }
      [self send];
    }
  }
}

- (void)drawRect:(CGRect)rect {
  [self layoutControls];
  CGContextRef g = UIGraphicsGetCurrentContext();
  for (const auto& k : _c) {
    const BOOL on = k.touch != nullptr;
    const CGRect box = CGRectMake(k.c.x - k.r, k.c.y - k.r, 2 * k.r, 2 * k.r);
    CGContextSetFillColorWithColor(g, [k.color colorWithAlphaComponent:on ? 0.55 : 0.28].CGColor);
    CGContextFillEllipseInRect(g, box);
    CGContextSetStrokeColorWithColor(g, [k.color colorWithAlphaComponent:0.8].CGColor);
    CGContextSetLineWidth(g, 2);
    CGContextStrokeEllipseInRect(g, box);
    if (k.stick) {
      const float kr = k.r * 0.45f;
      const CGRect knob = CGRectMake(k.c.x + k.ax * k.r - kr, k.c.y + k.ay * k.r - kr, 2 * kr, 2 * kr);
      CGContextSetFillColorWithColor(g, [UIColor colorWithWhite:1 alpha:on ? 0.6 : 0.35].CGColor);
      CGContextFillEllipseInRect(g, knob);
    } else if (k.label[0]) {
      NSString* s = [NSString stringWithUTF8String:k.label];
      NSDictionary* a = @{
        NSFontAttributeName : [UIFont boldSystemFontOfSize:k.r * (strlen(k.label) > 2 ? 0.42f : 0.8f)],
        NSForegroundColorAttributeName : [UIColor colorWithWhite:1 alpha:0.9]
      };
      const CGSize sz = [s sizeWithAttributes:a];
      [s drawAtPoint:CGPointMake(k.c.x - sz.width / 2, k.c.y - sz.height / 2) withAttributes:a];
    }
  }
}

@end

namespace {

WWE12TouchView* g_view = nil;

UIWindow* FindGameWindow() {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    if (![scene isKindOfClass:UIWindowScene.class]) continue;
    UIWindow* w = ((UIWindowScene*)scene).windows.firstObject;
    if (w) return w;
  }
  return nil;
}

void Tick() {
  UIWindow* w = FindGameWindow();
  if (!w) return;  // the game has not made its window yet
  if (!g_view || g_view.superview != w) {
    g_view = [[WWE12TouchView alloc] initWithFrame:w.bounds];
    [w addSubview:g_view];
  }
  [w bringSubviewToFront:g_view];
  [g_view tick];
}

// Before main(): the timer starts once the main run loop runs (UIApplicationMain, by SDL_main).
__attribute__((constructor(103))) void IosTouchStart() {
  dispatch_async(dispatch_get_main_queue(), ^{
    [NSTimer scheduledTimerWithTimeInterval:0.5 repeats:YES block:^(NSTimer*) { Tick(); }];
  });
}

}  // namespace
