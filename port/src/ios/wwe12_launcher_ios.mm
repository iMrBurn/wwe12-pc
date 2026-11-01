// iOS launcher: a screen shown before the game starts, with three tabs: Play, DLC and Install.
// Adapted from the launcher of Ke619's SvR 2011 iOS port (MIT, https://github.com/Ke619/WWE-SvR11-IOS),
// whose Import-folder idea is kept on purpose: LiveContainer users can't rely on the Files app picker
// inside the container, so a disc image or DLC package copied to Documents/Import shows up here with
// an Install / Add button. The same actions are also offered through the system file picker.
// A file named skip_launcher in Documents turns the launcher off.
//
// What the game itself reads (port/src/wwe12_app.h, wwe12_dlc.cpp): the disc image as
// Documents/game/<name>.iso, DLC packages in Documents/dlc, installed by the game at its next start.
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#define W12_BG [UIColor colorWithRed:0.04 green:0.04 blue:0.06 alpha:1.0]
#define W12_RED [UIColor colorWithRed:0.75 green:0.05 blue:0.08 alpha:1.0]
#define W12_DIM [UIColor colorWithWhite:0.6 alpha:1.0]
#define W12_TEXT [UIColor whiteColor]

static const uint32_t kTitleId = 0x54510890;  // WWE '12
static const uint64_t kSector = 2048;
static const uint64_t kBases[] = {0, 0x18300000ULL, 0xFD90000ULL, 0x2080000ULL};  // XISO, XGD2, XGD1, XGD3

// ---- paths and small helpers ----

static std::string Docs() {
  const char* home = getenv("HOME");
  return std::string(home ? home : "") + "/Documents";
}
static std::string GameDir() { return Docs() + "/game"; }
static std::string DlcDir() { return Docs() + "/dlc"; }
static std::string ImportDir() { return Docs() + "/Import"; }
static NSString* NS(const std::string& s) { return [NSString stringWithUTF8String:s.c_str()]; }

static bool IsFile(const std::string& p, uint64_t* size = nullptr) {
  struct stat st;
  if (stat(p.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return false;
  if (size) *size = static_cast<uint64_t>(st.st_size);
  return true;
}
static bool EndsWith(const std::string& s, const char* suffix) {
  const size_t n = std::strlen(suffix);
  return s.size() >= n && strcasecmp(s.c_str() + s.size() - n, suffix) == 0;
}
static std::string SafeName(std::string n) {
  for (char& c : n) {
    if (c == '/' || c == '\\' || c == ':') c = '_';
  }
  if (n.empty() || n[0] == '.') n = "_" + n;
  return n;
}
static NSString* Human(uint64_t bytes) {
  if (bytes >= (1ULL << 30)) return [NSString stringWithFormat:@"%.1f GB", static_cast<double>(bytes) / 1073741824.0];
  if (bytes >= (1ULL << 20)) return [NSString stringWithFormat:@"%.0f MB", static_cast<double>(bytes) / 1048576.0];
  return [NSString stringWithFormat:@"%.0f KB", static_cast<double>(bytes) / 1024.0];
}
static uint64_t FreeBytes(const std::string& dir) {
  struct statfs s;
  if (statfs(dir.c_str(), &s) != 0) return 0;
  return static_cast<uint64_t>(s.f_bavail) * static_cast<uint64_t>(s.f_bsize);
}
static void MakeDir(const std::string& p) {
  [[NSFileManager defaultManager] createDirectoryAtPath:NS(p) withIntermediateDirectories:YES attributes:nil error:nil];
}

// Regular files directly in `dir`, sorted by name; ".part" and hidden files left out.
static std::vector<std::string> ListFiles(const std::string& dir) {
  std::vector<std::string> out;
  if (DIR* d = opendir(dir.c_str())) {
    while (const struct dirent* e = readdir(d)) {
      const std::string n = e->d_name;
      if (n.empty() || n[0] == '.' || EndsWith(n, ".part")) continue;
      if (IsFile(dir + "/" + n)) out.push_back(n);
    }
    closedir(d);
  }
  std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
    return strcasecmp(a.c_str(), b.c_str()) < 0;
  });
  return out;
}

static uint32_t Le16(const uint8_t* b) { return uint32_t(b[0]) | uint32_t(b[1]) << 8; }
static uint32_t Le32(const uint8_t* b) { return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24; }
static uint32_t Be32(const uint8_t* b) { return uint32_t(b[3]) | uint32_t(b[2]) << 8 | uint32_t(b[1]) << 16 | uint32_t(b[0]) << 24; }

static bool Pread(int fd, uint64_t at, void* buf, size_t n) {
  uint8_t* p = static_cast<uint8_t*>(buf);
  while (n > 0) {
    const ssize_t r = pread(fd, p, n, static_cast<off_t>(at));
    if (r <= 0) return false;
    p += r;
    at += static_cast<uint64_t>(r);
    n -= static_cast<size_t>(r);
  }
  return true;
}

// ---- checking what the player picked ----

// An Xbox 360 disc image holding WWE '12: the XDVDFS partition (its magic at sector 32, at one of the
// usual offsets of a plain / XGD1 / XGD2 / XGD3 image), then default.xex's title ID in the root folder.
// Returns an error text, or "" when it is the game.
static std::string CheckDisc(int fd) {
  struct stat st;
  if (fstat(fd, &st) != 0) return "the file can't be read";
  const uint64_t size = static_cast<uint64_t>(st.st_size);
  for (uint64_t base : kBases) {
    const uint64_t at = base + 32 * kSector;
    uint8_t vd[28];
    if (at + sizeof(vd) > size || !Pread(fd, at, vd, sizeof(vd))) continue;
    if (std::memcmp(vd, "MICROSOFT*XBOX*MEDIA", 20) != 0) continue;
    const uint32_t rootSector = Le32(vd + 20), rootSize = Le32(vd + 24);
    if (rootSize == 0 || rootSize > (16u << 20)) return "the disc's file table is unreadable";
    std::vector<uint8_t> t(rootSize);
    if (!Pread(fd, base + uint64_t(rootSector) * kSector, t.data(), t.size())) return "the disc's file table is unreadable";
    size_t pos = 0;
    while (pos + 14 <= t.size()) {
      const size_t nameLen = t[pos + 13];
      if (Le16(&t[pos]) == 0xFFFF || nameLen == 0) {
        pos = (pos / kSector + 1) * kSector;
        continue;
      }
      if (pos + 14 + nameLen > t.size()) break;
      const std::string name(reinterpret_cast<const char*>(&t[pos + 14]), nameLen);
      if (strcasecmp(name.c_str(), "default.xex") == 0) {
        const uint64_t xexAt = base + uint64_t(Le32(&t[pos + 4])) * kSector;
        std::vector<uint8_t> h(4096);
        if (!Pread(fd, xexAt, h.data(), h.size()) || std::memcmp(h.data(), "XEX2", 4) != 0)
          return "default.xex can't be read";
        const uint32_t count = Be32(&h[20]);
        for (uint32_t i = 0; i < count && 24 + size_t(i) * 8 + 8 <= h.size(); ++i) {
          if (Be32(&h[24 + i * 8]) != 0x00040006) continue;  // execution info
          uint8_t info[24];
          if (!Pread(fd, xexAt + Be32(&h[24 + i * 8 + 4]), info, sizeof(info))) return "default.xex can't be read";
          const uint32_t id = Be32(info + 12);
          if (id == kTitleId) return "";
          char buf[96];
          std::snprintf(buf, sizeof(buf), "this is another game (its title ID is %08X)", id);
          return buf;
        }
        return "default.xex has no title ID";
      }
      pos = (pos + 14 + nameLen + 3) & ~size_t(3);
    }
    return "the image has no default.xex";
  }
  return "this isn't an Xbox 360 disc image";
}

// A WWE '12 DLC package (STFS: "CON " / "LIVE" / "PIRS", the title ID at 0x360, content type at 0x344;
// type 1 is a save, which is not DLC). Returns an error text, or "".
static std::string CheckDlc(int fd) {
  uint8_t h[0x364];
  if (!Pread(fd, 0, h, sizeof(h))) return "not a package";
  if (std::memcmp(h, "CON ", 4) != 0 && std::memcmp(h, "LIVE", 4) != 0 && std::memcmp(h, "PIRS", 4) != 0)
    return "not an Xbox 360 package";
  if (Be32(h + 0x360) != kTitleId) return "not a WWE '12 package";
  if (Be32(h + 0x344) == 1) return "that's a save, not DLC";
  return "";
}

// ---- copying ----

typedef void (^Report)(double fraction, NSString* text);

// Copies `from` (an open descriptor) to `to` through <to>.part. Returns "" or an error text.
static std::string CopyFd(int from, uint64_t size, const std::string& to, NSString* what, Report report) {
  MakeDir(to.substr(0, to.rfind('/')));
  const std::string part = to + ".part";
  const int out = open(part.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (out < 0) return "can't write " + to;
  std::vector<uint8_t> buf(8u << 20);
  uint64_t done = 0;
  while (done < size) {
    const size_t n = static_cast<size_t>(std::min<uint64_t>(buf.size(), size - done));
    if (!Pread(from, done, buf.data(), n)) {
      close(out);
      unlink(part.c_str());
      return "the file can't be read";
    }
    size_t off = 0;
    while (off < n) {
      const ssize_t w = write(out, buf.data() + off, n - off);
      if (w <= 0) {
        close(out);
        unlink(part.c_str());
        return "can't write (is the phone full?)";
      }
      off += static_cast<size_t>(w);
    }
    done += n;
    if (report) report(double(done) / double(size), [NSString stringWithFormat:@"%@ %d%% (%@ of %@)", what, int(done * 100 / size), Human(done), Human(size)]);
  }
  close(out);
  unlink(to.c_str());
  if (rename(part.c_str(), to.c_str()) != 0) return "can't write " + to;
  return "";
}

// ---- UI pieces ----

static UILabel* Label(NSString* text, CGFloat size, UIColor* color, BOOL bold) {
  UILabel* l = [[UILabel alloc] init];
  l.text = text;
  l.numberOfLines = 0;
  l.textColor = color;
  l.font = bold ? [UIFont boldSystemFontOfSize:size] : [UIFont systemFontOfSize:size];
  return l;
}

static UIButton* Button(NSString* title, BOOL filled, void (^onTap)(void)) {
  UIButtonConfiguration* cfg = filled ? [UIButtonConfiguration filledButtonConfiguration] : [UIButtonConfiguration grayButtonConfiguration];
  cfg.title = title;
  if (filled) cfg.baseBackgroundColor = W12_RED;
  UIButton* b = [UIButton buttonWithConfiguration:cfg primaryAction:[UIAction actionWithHandler:^(__kindof UIAction* a) { onTap(); }]];
  return b;
}

// ---- settings: Documents/wwe12.toml (flat "key = value" lines, the file the game reads; same keys as PC) ----

static std::string TomlPath() { return Docs() + "/wwe12.toml"; }

static std::string Trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

static std::vector<std::string> TomlLines() {
  std::vector<std::string> out;
  if (FILE* f = std::fopen(TomlPath().c_str(), "r")) {
    char line[1024];
    while (std::fgets(line, sizeof(line), f)) {
      std::string s(line);
      while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
      out.push_back(s);
    }
    std::fclose(f);
  }
  return out;
}

// True and the key's text of a line "key = value" (comments and sections do not count).
static bool TomlSplit(const std::string& line, std::string* key, std::string* value) {
  const std::string t = Trim(line);
  if (t.empty() || t[0] == '#' || t[0] == '[') return false;
  const size_t eq = t.find('=');
  if (eq == std::string::npos) return false;
  *key = Trim(t.substr(0, eq));
  *value = Trim(t.substr(eq + 1));
  return true;
}

static std::string TomlGet(const char* key, const char* fallback) {
  for (const std::string& l : TomlLines()) {
    std::string k, v;
    if (TomlSplit(l, &k, &v) && k == key) {
      if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
      return v;
    }
  }
  return fallback;
}

static void TomlSet(const char* key, const std::string& value) {
  std::vector<std::string> lines = TomlLines();
  bool done = false;
  for (std::string& l : lines) {
    std::string k, v;
    if (TomlSplit(l, &k, &v) && k == key) {
      l = std::string(key) + " = " + value;
      done = true;
    }
  }
  if (!done) lines.push_back(std::string(key) + " = " + value);
  if (FILE* f = std::fopen(TomlPath().c_str(), "w")) {
    for (const std::string& l : lines) std::fprintf(f, "%s\n", l.c_str());
    std::fclose(f);
  }
}

// App-only settings (the on-screen controller): read by wwe12_touch_ios.mm.
static NSInteger PrefInt(NSString* key, NSInteger fallback) {
  NSNumber* n = [[NSUserDefaults standardUserDefaults] objectForKey:key];
  return n ? [n integerValue] : fallback;
}

// ---- cards, rows and controls ----

static UIStackView* Card(UIStackView* page, NSString* title) {
  UIStackView* inner = [[UIStackView alloc] init];
  inner.axis = UILayoutConstraintAxisVertical;
  inner.spacing = 12.0;
  inner.layoutMargins = UIEdgeInsetsMake(14, 16, 16, 16);
  inner.layoutMarginsRelativeArrangement = YES;
  inner.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.07];
  inner.layer.cornerRadius = 14.0;
  inner.clipsToBounds = YES;
  [inner addArrangedSubview:Label([title uppercaseString], 13.0, W12_RED, YES)];
  [page addArrangedSubview:inner];
  return inner;
}

static UIView* Row(NSString* title, UIView* control) {
  UILabel* l = Label(title, 15.0, W12_TEXT, NO);
  [l setContentCompressionResistancePriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
  [control setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
  UIStackView* row = [[UIStackView alloc] initWithArrangedSubviews:@[ l, control ]];
  row.spacing = 12.0;
  row.alignment = UIStackViewAlignmentCenter;
  return row;
}

static UISegmentedControl* Segment(NSArray<NSString*>* items, NSInteger selected, void (^onChange)(NSInteger)) {
  UISegmentedControl* s = [[UISegmentedControl alloc] initWithItems:items];
  s.selectedSegmentIndex = selected;
  s.selectedSegmentTintColor = W12_RED;
  [s setTitleTextAttributes:@{NSForegroundColorAttributeName : W12_TEXT} forState:UIControlStateNormal];
  [s setTitleTextAttributes:@{NSForegroundColorAttributeName : W12_TEXT} forState:UIControlStateSelected];
  [s addAction:[UIAction actionWithHandler:^(__kindof UIAction* a) { onChange(((UISegmentedControl*)a.sender).selectedSegmentIndex); }]
      forControlEvents:UIControlEventValueChanged];
  return s;
}

static UISwitch* Toggle(BOOL on, void (^onChange)(BOOL)) {
  UISwitch* s = [[UISwitch alloc] init];
  s.on = on;
  s.onTintColor = W12_RED;
  [s addAction:[UIAction actionWithHandler:^(__kindof UIAction* a) { onChange(((UISwitch*)a.sender).isOn); }]
      forControlEvents:UIControlEventValueChanged];
  return s;
}

// A row with a choice stored in wwe12.toml: `values` are the toml texts, in the order of `names`.
static UIView* TomlChoice(NSString* title, const char* key, NSArray<NSString*>* names, NSArray<NSString*>* values,
                          const char* fallback) {
  const std::string now = TomlGet(key, fallback);
  NSInteger sel = -1;
  for (NSUInteger i = 0; i < values.count; ++i) {
    if (now == [values[i] UTF8String]) sel = static_cast<NSInteger>(i);
  }
  const std::string k = key;
  return Row(title, Segment(names, sel, ^(NSInteger i) { TomlSet(k.c_str(), [values[i] UTF8String]); }));
}

static UIView* TomlSwitch(NSString* title, const char* key, bool fallback) {
  const std::string now = TomlGet(key, fallback ? "true" : "false");
  const std::string k = key;
  return Row(title, Toggle(now == "true", ^(BOOL on) { TomlSet(k.c_str(), on ? "true" : "false"); }));
}

// What the game will find: an .iso in game/ (or next to the saves), or an extracted game folder.
static std::string FindGame() {
  for (const std::string& dir : {GameDir(), Docs()}) {
    for (const std::string& n : ListFiles(dir)) {
      if (EndsWith(n, ".iso")) return dir + "/" + n;
    }
  }
  if (IsFile(GameDir() + "/default.xex")) return GameDir() + "/default.xex";
  return "";
}

// Installed name of a package (the game keeps 42 characters) and what it installed.
static std::string DlcInstallName(const std::string& file) { return file.size() > 42 ? file.substr(0, 42) : file; }
static std::string DlcInstalledDir(const std::string& file) {
  return Docs() + "/wwe12/0000000000000000/54510890/00000002/" + DlcInstallName(file);
}

// ---- saves: Documents/wwe12/<profile>/54510890 and the achievements file; a backup mirrors both
// under wwe12/SaveBackups/<date>/ (the same layout as the PC and Android versions) ----

static std::string SaveDir() { return Docs() + "/wwe12/B13EBABEBABEBABE/54510890"; }
static std::string AchFile() { return Docs() + "/wwe12/achievements/54510890.toml"; }
static std::string BackupsDir() { return Docs() + "/wwe12/SaveBackups"; }

static bool PathExists(const std::string& p) { return [[NSFileManager defaultManager] fileExistsAtPath:NS(p)]; }

static void CopyReplace(const std::string& from, const std::string& to) {
  NSFileManager* fm = [NSFileManager defaultManager];
  [fm removeItemAtPath:NS(to) error:nil];
  MakeDir(to.substr(0, to.rfind('/')));
  [fm copyItemAtPath:NS(from) toPath:NS(to) error:nil];
}

// Backup names, newest first.
static std::vector<std::string> BackupNames() {
  std::vector<std::string> out;
  NSArray* names = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:NS(BackupsDir()) error:nil];
  for (NSString* n in names) {
    if (![n hasPrefix:@"."]) out.push_back(std::string([n UTF8String]));
  }
  std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return a > b; });
  return out;
}

// Copies the save and the achievements. Returns the backup's name, or "" when there is nothing yet.
static std::string MakeBackup(const std::string& label, bool automatic) {
  if (!PathExists(SaveDir()) && !PathExists(AchFile())) return "";
  NSDateFormatter* fmt = [[NSDateFormatter alloc] init];
  fmt.dateFormat = @"yyyy-MM-dd_HHmmss";
  fmt.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
  std::string name = std::string([[fmt stringFromDate:[NSDate date]] UTF8String]);
  if (automatic) name += "_auto";
  else if (!label.empty()) name += "_" + SafeName(label);
  const std::string dest = BackupsDir() + "/" + name;
  if (PathExists(SaveDir())) CopyReplace(SaveDir(), dest + "/B13EBABEBABEBABE/54510890");
  if (PathExists(AchFile())) CopyReplace(AchFile(), dest + "/achievements/54510890.toml");
  if (automatic) {  // only the latest 10 automatic ones are kept
    int seen = 0;
    for (const std::string& n : BackupNames()) {
      if (!EndsWith(n, "_auto")) continue;
      if (++seen > 10) [[NSFileManager defaultManager] removeItemAtPath:NS(BackupsDir() + "/" + n) error:nil];
    }
  }
  return name;
}

@interface Wwe12LauncherViewController : UIViewController <UIDocumentPickerDelegate>
@property(nonatomic, copy) void (^onPlay)(void);
@end

@implementation Wwe12LauncherViewController {
  UIView* _content;
  NSInteger _page;
  BOOL _busy;
  BOOL _pickingDlc;
  NSString* _message;        // result of the last action, shown on the Install and DLC tabs
  UILabel* _progressLabel;   // live text while copying
  UIProgressView* _bar;
  UITextField* _backupName;
}

- (UIScrollView*)scrollPage:(UIStackView**)outStack {
  UIScrollView* scroll = [[UIScrollView alloc] init];
  UIStackView* stack = [[UIStackView alloc] init];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.spacing = 12.0;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  [scroll addSubview:stack];
  [NSLayoutConstraint activateConstraints:@[
    [stack.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor constant:8.0],
    [stack.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor constant:-8.0],
    [stack.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor],
    [stack.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor],
    [stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor],
  ]];
  *outStack = stack;
  return scroll;
}

- (void)addProgressTo:(UIStackView*)stack {
  _progressLabel = Label(_busy ? @"Working..." : (_message ? _message : @""), 14.0, [UIColor colorWithRed:0.94 green:0.75 blue:0.24 alpha:1.0], NO);
  _bar = [[UIProgressView alloc] initWithProgressViewStyle:UIProgressViewStyleDefault];
  _bar.hidden = !_busy;
  [stack addArrangedSubview:_progressLabel];
  [stack addArrangedSubview:_bar];
}

// ---- Play ----

- (UIView*)buildPlayPage {
  const std::string game = FindGame();
  UIFontDescriptor* heavy = [[UIFont systemFontOfSize:64.0 weight:UIFontWeightHeavy].fontDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitItalic];
  UIFont* logoFont = [UIFont fontWithDescriptor:heavy size:64.0];
  NSMutableAttributedString* logo = [[NSMutableAttributedString alloc]
      initWithString:@"WWE" attributes:@{NSFontAttributeName : logoFont, NSForegroundColorAttributeName : W12_TEXT}];
  [logo appendAttributedString:[[NSAttributedString alloc]
                                   initWithString:@" '12" attributes:@{NSFontAttributeName : logoFont, NSForegroundColorAttributeName : W12_RED}]];
  UILabel* title = [[UILabel alloc] init];
  title.attributedText = logo;
  title.textAlignment = NSTextAlignmentCenter;
  NSString* version = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
  UILabel* sub = Label([NSString stringWithFormat:@"iPhone port  %@", version ? version : @""], 14.0, W12_DIM, NO);
  sub.textAlignment = NSTextAlignmentCenter;
  NSString* info = game.empty() ? @"Game: NOT found.\nOpen the Install tab to add your disc image."
                                : [NSString stringWithFormat:@"Game: %@", NS(game.substr(game.rfind('/') + 1))];
  UILabel* status = Label(info, 13.0, [UIColor colorWithWhite:0.8 alpha:1.0], NO);
  status.textAlignment = NSTextAlignmentCenter;
  __weak Wwe12LauncherViewController* weakSelf = self;
  UIButton* play = Button(@"PLAY", YES, ^{
    Wwe12LauncherViewController* s = weakSelf;
    if (s && s.onPlay) s.onPlay();
  });
  play.enabled = !game.empty();

  UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[ title, sub, status, play ]];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.spacing = 16.0;
  stack.alignment = UIStackViewAlignmentCenter;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* page = [[UIView alloc] init];
  [page addSubview:stack];
  [NSLayoutConstraint activateConstraints:@[
    [stack.centerXAnchor constraintEqualToAnchor:page.centerXAnchor],
    [stack.centerYAnchor constraintEqualToAnchor:page.centerYAnchor],
    [stack.widthAnchor constraintLessThanOrEqualToAnchor:page.widthAnchor],
    [play.widthAnchor constraintEqualToConstant:260.0],
  ]];
  return page;
}

// ---- Install ----

- (UIView*)buildInstallPage {
  UIStackView* stack = nil;
  UIScrollView* page = [self scrollPage:&stack];
  __weak Wwe12LauncherViewController* weakSelf = self;

  const std::string game = FindGame();
  [stack addArrangedSubview:Label(@"Install the game", 20.0, W12_TEXT, YES)];
  [stack addArrangedSubview:Label(game.empty() ? @"No game installed yet." : [NSString stringWithFormat:@"Installed: %@", NS(game.substr(game.rfind('/') + 1))],
                                  14.0, W12_DIM, NO)];
  [self addProgressTo:stack];

  [stack addArrangedSubview:Button(@"Choose a disc image (.iso)", YES, ^{ [weakSelf pickFiles:NO]; })];
  [stack addArrangedSubview:Label(@"Or copy the .iso into the Import folder (works in LiveContainer too), then press Install next to it:", 13.0, W12_DIM, NO)];
  UILabel* path = Label(NS(ImportDir()), 11.0, W12_DIM, NO);
  path.font = [UIFont monospacedSystemFontOfSize:11.0 weight:UIFontWeightRegular];
  [stack addArrangedSubview:path];

  MakeDir(ImportDir());
  BOOL any = NO;
  for (const std::string& name : ListFiles(ImportDir())) {
    if (!EndsWith(name, ".iso")) continue;
    any = YES;
    uint64_t size = 0;
    IsFile(ImportDir() + "/" + name, &size);
    NSString* ns = NS(name);
    UIStackView* row = [[UIStackView alloc] init];
    row.spacing = 8.0;
    row.alignment = UIStackViewAlignmentCenter;
    UILabel* l = Label([NSString stringWithFormat:@"%@  (%@)", ns, Human(size)], 14.0, W12_TEXT, NO);
    [l setContentCompressionResistancePriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
    [row addArrangedSubview:l];
    [row addArrangedSubview:Button(@"Install", YES, ^{ [weakSelf installImported:ns]; })];
    [row addArrangedSubview:Button(@"Delete", NO, ^{
      unlink((ImportDir() + "/" + std::string([ns UTF8String])).c_str());
      [weakSelf showPage:1];
    })];
    [stack addArrangedSubview:row];
  }
  if (!any) [stack addArrangedSubview:Label(@"No .iso in the Import folder yet.", 13.0, W12_DIM, NO)];

  if (!game.empty()) {
    [stack addArrangedSubview:Button(@"Remove the installed game", NO, ^{
      const std::string g = FindGame();
      if (!g.empty()) unlink(g.c_str());
      [weakSelf setMessage:@"Game removed."];
    })];
  }
  return page;
}

- (void)setMessage:(NSString*)m {
  _message = m;
  [self showPage:_page];
}

- (void)setProgress:(double)f text:(NSString*)t {
  dispatch_async(dispatch_get_main_queue(), ^{
    self->_progressLabel.text = t;
    self->_bar.hidden = NO;
    self->_bar.progress = static_cast<float>(f);
  });
}

// A disc image from the Import folder: moved into game/ (same volume, so no second copy of 7 GB).
- (void)installImported:(NSString*)name {
  if (_busy) return;
  _busy = YES;
  _message = nil;
  [self showPage:1];
  const std::string src = ImportDir() + "/" + std::string([name UTF8String]);
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    std::string err;
    const int fd = open(src.c_str(), O_RDONLY);
    if (fd < 0) err = "can't open the file";
    else {
      err = CheckDisc(fd);
      close(fd);
    }
    if (err.empty()) {
      MakeDir(GameDir());
      for (const std::string& old : ListFiles(GameDir())) {
        if (EndsWith(old, ".iso")) unlink((GameDir() + "/" + old).c_str());
      }
      if (rename(src.c_str(), (GameDir() + "/" + SafeName(src.substr(src.rfind('/') + 1))).c_str()) != 0) err = "can't move the file into the game folder";
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      self->_busy = NO;
      [self setMessage:err.empty() ? @"Installed. Open the Play tab." : [NSString stringWithFormat:@"Can't install: %@.", NS(err)]];
    });
  });
}

// ---- DLC ----

- (UIView*)buildDlcPage {
  UIStackView* stack = nil;
  UIScrollView* page = [self scrollPage:&stack];
  __weak Wwe12LauncherViewController* weakSelf = self;

  [stack addArrangedSubview:Label(@"DLC", 20.0, W12_TEXT, YES)];
  [self addProgressTo:stack];

  const std::vector<std::string> have = ListFiles(DlcDir());
  [stack addArrangedSubview:Label(have.empty() ? @"No DLC yet." : [NSString stringWithFormat:@"%lu DLC package(s):", (unsigned long)have.size()], 14.0, W12_TEXT, NO)];
  for (const std::string& name : have) {
    const BOOL installed = [[NSFileManager defaultManager] fileExistsAtPath:NS(DlcInstalledDir(name))];
    NSString* ns = NS(name);
    UIStackView* row = [[UIStackView alloc] init];
    row.spacing = 8.0;
    row.alignment = UIStackViewAlignmentCenter;
    UILabel* l = Label([NSString stringWithFormat:@"%@%@", ns, installed ? @"  (installed)" : @"  (installed at next start)"], 12.0, W12_TEXT, NO);
    [l setContentCompressionResistancePriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
    [row addArrangedSubview:l];
    [row addArrangedSubview:Button(@"Remove", NO, ^{
      const std::string n = std::string([ns UTF8String]);
      [[NSFileManager defaultManager] removeItemAtPath:NS(DlcInstalledDir(n)) error:nil];
      unlink((Docs() + "/wwe12/0000000000000000/54510890/Headers/00000002/" + DlcInstallName(n) + ".header").c_str());
      unlink((DlcDir() + "/" + n).c_str());
      [weakSelf setMessage:@"DLC removed."];
    })];
    [stack addArrangedSubview:row];
  }

  [stack addArrangedSubview:Button(@"Add DLC files", YES, ^{ [weakSelf pickFiles:YES]; })];
  [stack addArrangedSubview:Label(@"Pick the DLC packages of your Xbox 360 (files with long names such as 961C4A43...54, from Content/0000000000000000/54510890/00000002). Only WWE '12 packages are taken. Or copy them into the Import folder:", 12.0, W12_DIM, NO)];
  UILabel* path = Label(NS(ImportDir()), 11.0, W12_DIM, NO);
  path.font = [UIFont monospacedSystemFontOfSize:11.0 weight:UIFontWeightRegular];
  [stack addArrangedSubview:path];

  MakeDir(ImportDir());
  std::vector<std::string> others;
  for (const std::string& n : ListFiles(ImportDir())) {
    if (!EndsWith(n, ".iso")) others.push_back(n);
  }
  if (others.empty()) {
    [stack addArrangedSubview:Label(@"Nothing in the Import folder yet.", 13.0, W12_DIM, NO)];
  } else {
    [stack addArrangedSubview:Button([NSString stringWithFormat:@"Add all %lu file(s) from Import", (unsigned long)others.size()], YES, ^{
      [weakSelf addImportedDlc];
    })];
  }
  return page;
}

// Every package of the Import folder, moved into dlc/ (files that are not WWE '12 DLC stay).
- (void)addImportedDlc {
  if (_busy) return;
  _busy = YES;
  _message = nil;
  [self showPage:2];
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    int added = 0, skipped = 0, failed = 0;
    for (const std::string& n : ListFiles(ImportDir())) {
      if (EndsWith(n, ".iso")) continue;
      const std::string src = ImportDir() + "/" + n;
      const int fd = open(src.c_str(), O_RDONLY);
      if (fd < 0) { ++failed; continue; }
      const std::string err = CheckDlc(fd);
      close(fd);
      if (!err.empty()) { ++skipped; continue; }
      MakeDir(DlcDir());
      if (rename(src.c_str(), (DlcDir() + "/" + SafeName(n)).c_str()) == 0) ++added; else ++failed;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      self->_busy = NO;
      [self setMessage:[self dlcReport:added skipped:skipped failed:failed]];
    });
  });
}

- (NSString*)dlcReport:(int)added skipped:(int)skipped failed:(int)failed {
  NSMutableString* r = [NSMutableString stringWithFormat:@"%d DLC added", added];
  if (skipped > 0) [r appendFormat:@", %d file(s) skipped (not a WWE '12 DLC package)", skipped];
  if (failed > 0) [r appendFormat:@", %d failed (storage full?)", failed];
  [r appendString:added > 0 ? @". They are installed the next time you start the game." : @"."];
  return r;
}

// ---- the system file picker (both tabs) ----

- (void)pickFiles:(BOOL)dlc {
  if (_busy) return;
  _pickingDlc = dlc;
  UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeData ] asCopy:NO];
  picker.allowsMultipleSelection = dlc;
  picker.delegate = self;
  [self presentViewController:picker animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  if (_busy || urls.count == 0) return;
  _busy = YES;
  _message = nil;
  const BOOL dlc = _pickingDlc;
  [self showPage:dlc ? 2 : 1];
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    NSString* report = nil;
    int added = 0, skipped = 0, failed = 0;
    for (NSURL* url in urls) {
      const BOOL scoped = [url startAccessingSecurityScopedResource];
      const std::string path = std::string([url.path UTF8String]);
      const std::string name = SafeName(path.substr(path.rfind('/') + 1));
      uint64_t size = 0;
      const int fd = open(path.c_str(), O_RDONLY);
      if (fd < 0 || !IsFile(path, &size)) {
        if (fd >= 0) close(fd);
        ++failed;
        if (scoped) [url stopAccessingSecurityScopedResource];
        continue;
      }
      std::string err = dlc ? CheckDlc(fd) : CheckDisc(fd);
      if (err.empty()) {
        const std::string dest = (dlc ? DlcDir() : GameDir()) + "/" + (dlc || EndsWith(name, ".iso") ? name : name + ".iso");
        if (!dlc && size + (64ULL << 20) > FreeBytes(Docs())) {
          err = "not enough free space (" + std::string([[Human(size) description] UTF8String]) + " needed)";
        } else {
          if (!dlc) {
            MakeDir(GameDir());
            for (const std::string& old : ListFiles(GameDir())) {
              if (EndsWith(old, ".iso")) unlink((GameDir() + "/" + old).c_str());
            }
          }
          err = CopyFd(fd, size, dest, dlc ? NS(name) : @"Installing", ^(double f, NSString* t) { [self setProgress:f text:t]; });
        }
        if (err.empty()) ++added;
        else ++failed;
        if (!err.empty() && !dlc) report = [NSString stringWithFormat:@"Can't install: %@.", NS(err)];
      } else if (dlc) {
        ++skipped;
      } else {
        report = [NSString stringWithFormat:@"Can't install: %@.", NS(err)];
      }
      close(fd);
      if (scoped) [url stopAccessingSecurityScopedResource];
    }
    NSString* finalReport = dlc ? [self dlcReport:added skipped:skipped failed:failed]
                                : (report ? report : @"Installed. Open the Play tab.");
    dispatch_async(dispatch_get_main_queue(), ^{
      self->_busy = NO;
      [self setMessage:finalReport];
    });
  });
}

// ---- Settings (the same keys as the PC version's wwe12.toml) ----

- (UIView*)buildSettingsPage {
  UIStackView* stack = nil;
  UIScrollView* page = [self scrollPage:&stack];
  __weak Wwe12LauncherViewController* weakSelf = self;

  // One tap sets resolution, shadows, crowd, blur and texture sharpness.
  UIStackView* presets = Card(stack, @"Quick presets");
  [presets addArrangedSubview:Label(@"Sets resolution, shadows, crowd, blur and texture sharpness in one tap.", 12.0, W12_DIM, NO)];
  struct Preset { const char* name; const char* res; const char* shadows; const char* crowd; const char* dof; const char* aniso; };
  static const Preset kPresets[] = {
      {"Fastest", "540", "false", "false", "false", "0"},
      {"Balanced", "720", "true", "false", "false", "4"},
      {"Quality", "1080", "true", "true", "true", "16"},
  };
  UIStackView* presetRow = [[UIStackView alloc] init];
  presetRow.spacing = 10.0;
  presetRow.distribution = UIStackViewDistributionFillEqually;
  for (const Preset& p : kPresets) {
    const Preset* pp = &p;
    [presetRow addArrangedSubview:Button([NSString stringWithUTF8String:p.name], NO, ^{
      TomlSet("svr_render_scale", pp->res);
      TomlSet("wwe12_shadows", pp->shadows);
      TomlSet("wwe12_crowd", pp->crowd);
      TomlSet("wwe12_dof", pp->dof);
      TomlSet("bd_anisotropy", pp->aniso);
      [weakSelf setMessage:@"Preset applied."];
    })];
  }
  [presets addArrangedSubview:presetRow];
  if (_message) [presets addArrangedSubview:Label(_message, 13.0, [UIColor colorWithRed:0.94 green:0.75 blue:0.24 alpha:1.0], NO)];

  UIStackView* game = Card(stack, @"Game");
  [game addArrangedSubview:TomlChoice(@"Language", "user_language", @[ @"EN", @"DE", @"FR", @"ES", @"IT" ],
                                      @[ @"1", @"3", @"4", @"5", @"6" ], "1")];
  [game addArrangedSubview:TomlChoice(@"Loading screens", "wwe12_load_slice_ms", @[ @"Original", @"2x faster" ], @[ @"0", @"12" ], "12")];

  UIStackView* gfx = Card(stack, @"Graphics");
  [gfx addArrangedSubview:TomlChoice(@"Resolution (lower = faster)", "svr_render_scale", @[ @"540p", @"720p", @"1080p", @"Auto" ],
                                     @[ @"540", @"720", @"1080", @"0" ], "0")];
  [gfx addArrangedSubview:TomlChoice(@"Picture", "bd_aspect_ratio", @[ @"16:9 + bars", @"Stretch" ], @[ @"5", @"6" ], "5")];
  [gfx addArrangedSubview:TomlChoice(@"Texture sharpness at an angle", "bd_anisotropy", @[ @"Off", @"2x", @"4x", @"8x", @"16x" ],
                                     @[ @"0", @"2", @"4", @"8", @"16" ], "16")];
  [gfx addArrangedSubview:TomlSwitch(@"Shadows", "wwe12_shadows", true)];
  [gfx addArrangedSubview:TomlSwitch(@"Crowd", "wwe12_crowd", true)];
  [gfx addArrangedSubview:TomlSwitch(@"Depth of field (blur, heavier)", "wwe12_dof", true)];
  [gfx addArrangedSubview:TomlSwitch(@"60 fps in entrances (heavier)", "wwe12_60fps", false)];
  [gfx addArrangedSubview:TomlSwitch(@"V-sync", "bd_vsync", true)];
  [gfx addArrangedSubview:TomlSwitch(@"FPS counter", "wwe12_show_fps", false)];
  [stack addArrangedSubview:Label(@"Changes apply the next time you press PLAY.", 12.0, W12_DIM, NO)];
  return page;
}

// ---- Controls (the on-screen buttons) ----

- (UIView*)buildControlsPage {
  UIStackView* stack = nil;
  UIScrollView* page = [self scrollPage:&stack];
  UIStackView* card = Card(stack, @"On-screen buttons");
  [card addArrangedSubview:Row(@"Show the buttons", Toggle(PrefInt(@"touch_controls", 1) != 0, ^(BOOL on) {
                                 [[NSUserDefaults standardUserDefaults] setInteger:on ? 1 : 0 forKey:@"touch_controls"];
                               }))];
  static const NSInteger kOpacity[] = {30, 50, 70, 90};
  NSInteger now = PrefInt(@"touch_opacity", 50), sel = 1;
  for (int i = 0; i < 4; ++i) {
    if (kOpacity[i] == now) sel = i;
  }
  [card addArrangedSubview:Row(@"Opacity", Segment(@[ @"30%", @"50%", @"70%", @"90%" ], sel, ^(NSInteger i) {
                                 [[NSUserDefaults standardUserDefaults] setInteger:kOpacity[i] forKey:@"touch_opacity"];
                               }))];
  [stack addArrangedSubview:Label(@"A Bluetooth controller (Xbox, PlayStation...) works too: connect it in the iPhone's Settings before pressing PLAY.", 12.0, W12_DIM, NO)];
  return page;
}

// ---- Support: the logs, to send with a bug report ----

// The newest file named like `prefix*.log` in Documents and its wwe12 / logs sub-folders.
static std::string NewestLog(const char* prefix) {
  std::string best;
  time_t bestTime = 0;
  for (const std::string& dir : {Docs(), Docs() + "/logs", Docs() + "/wwe12", Docs() + "/wwe12/logs"}) {
    for (const std::string& n : ListFiles(dir)) {
      if (n.compare(0, std::strlen(prefix), prefix) != 0 || !EndsWith(n, ".log")) continue;
      struct stat st;
      if (stat((dir + "/" + n).c_str(), &st) == 0 && st.st_mtime >= bestTime) {
        bestTime = st.st_mtime;
        best = dir + "/" + n;
      }
    }
  }
  return best;
}

- (UIView*)buildSupportPage {
  UIStackView* stack = nil;
  UIScrollView* page = [self scrollPage:&stack];
  __weak Wwe12LauncherViewController* weakSelf = self;
  UIStackView* logs = Card(stack, @"Bug report");
  [logs addArrangedSubview:Label(@"After a problem, send the last game log and the start-up log.", 13.0, W12_DIM, NO)];
  [logs addArrangedSubview:Button(@"Share the last logs", YES, ^{
    NSMutableArray* items = [NSMutableArray array];
    for (const char* prefix : {"wwe12_", "boot"}) {
      const std::string f = NewestLog(prefix);
      if (!f.empty()) [items addObject:[NSURL fileURLWithPath:NS(f)]];
    }
    Wwe12LauncherViewController* s = weakSelf;
    if (!s) return;
    if (items.count == 0) {
      [s setMessage:@"No log yet: play once first."];
      return;
    }
    UIActivityViewController* share = [[UIActivityViewController alloc] initWithActivityItems:items applicationActivities:nil];
    share.popoverPresentationController.sourceView = s.view;
    [s presentViewController:share animated:YES completion:nil];
  })];
  if (_message) [logs addArrangedSubview:Label(_message, 13.0, [UIColor colorWithRed:0.94 green:0.75 blue:0.24 alpha:1.0], NO)];

  UIStackView* reset = Card(stack, @"Reset");
  [reset addArrangedSubview:Label(@"Puts every setting back to its default (your saves and DLC are not touched).", 13.0, W12_DIM, NO)];
  [reset addArrangedSubview:Button(@"Reset the settings", NO, ^{
    unlink(TomlPath().c_str());
    NSUserDefaults* d = [NSUserDefaults standardUserDefaults];
    [d removeObjectForKey:@"touch_controls"];
    [d removeObjectForKey:@"touch_opacity"];
    [weakSelf setMessage:@"Settings reset. The defaults are back the next time the game starts."];
  })];
  return page;
}

// ---- Saves ----

- (void)confirm:(NSString*)message action:(void (^)(void))action {
  UIAlertController* a = [UIAlertController alertControllerWithTitle:nil message:message preferredStyle:UIAlertControllerStyleAlert];
  [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
  [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDestructive handler:^(UIAlertAction*) { action(); }]];
  [self presentViewController:a animated:YES completion:nil];
}

- (UIView*)buildSavesPage {
  UIStackView* stack = nil;
  UIScrollView* page = [self scrollPage:&stack];
  __weak Wwe12LauncherViewController* weakSelf = self;

  UIStackView* mk = Card(stack, @"Back up");
  [mk addArrangedSubview:Label(@"A backup copies your save and your achievements (not the DLC). Restoring or deleting always backs up your current save first.", 13.0, W12_DIM, NO)];
  _backupName = [[UITextField alloc] init];
  _backupName.placeholder = @"Backup name (optional)";
  _backupName.borderStyle = UITextBorderStyleRoundedRect;
  _backupName.autocorrectionType = UITextAutocorrectionTypeNo;
  [mk addArrangedSubview:_backupName];
  [mk addArrangedSubview:Button(@"Back up now", YES, ^{
    Wwe12LauncherViewController* s = weakSelf;
    if (!s) return;
    const std::string label = s->_backupName.text ? std::string([s->_backupName.text UTF8String]) : std::string();
    const std::string made = MakeBackup(label, false);
    [s setMessage:made.empty() ? @"Nothing to back up yet: play and save once first."
                               : [NSString stringWithFormat:@"Backup created: %@", NS(made)]];
  })];
  if (_message) [mk addArrangedSubview:Label(_message, 13.0, [UIColor colorWithRed:0.94 green:0.75 blue:0.24 alpha:1.0], NO)];

  UIStackView* list = Card(stack, @"Backups");
  const std::vector<std::string> names = BackupNames();
  if (names.empty()) [list addArrangedSubview:Label(@"No backup yet.", 14.0, W12_DIM, NO)];
  for (const std::string& n : names) {
    NSString* ns = NS(n);
    UIStackView* row = [[UIStackView alloc] init];
    row.spacing = 8.0;
    row.alignment = UIStackViewAlignmentCenter;
    UILabel* l = Label(ns, 12.0, W12_TEXT, NO);
    [l setContentCompressionResistancePriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
    [row addArrangedSubview:l];
    [row addArrangedSubview:Button(@"Restore", NO, ^{
      [weakSelf confirm:@"Replace your current save with this backup? Your current save is backed up first." action:^{
        const std::string name = std::string([ns UTF8String]);
        const std::string from = BackupsDir() + "/" + name;
        MakeBackup("", true);
        [[NSFileManager defaultManager] removeItemAtPath:NS(SaveDir()) error:nil];
        [[NSFileManager defaultManager] removeItemAtPath:NS(AchFile()) error:nil];
        if (PathExists(from + "/B13EBABEBABEBABE/54510890")) CopyReplace(from + "/B13EBABEBABEBABE/54510890", SaveDir());
        if (PathExists(from + "/achievements/54510890.toml")) CopyReplace(from + "/achievements/54510890.toml", AchFile());
        [weakSelf setMessage:@"Save restored. It is used the next time you press PLAY."];
      }];
    })];
    [row addArrangedSubview:Button(@"Delete", NO, ^{
      [weakSelf confirm:@"Delete this backup?" action:^{
        [[NSFileManager defaultManager] removeItemAtPath:NS(BackupsDir() + "/" + std::string([ns UTF8String])) error:nil];
        [weakSelf setMessage:@"Backup deleted."];
      }];
    })];
    [list addArrangedSubview:row];
  }
  [list addArrangedSubview:Label(@"The backups are in Documents/wwe12/SaveBackups: copy a folder out with the Files app to keep it, or copy one in to restore it here.", 12.0, W12_DIM, NO)];

  UIStackView* del = Card(stack, @"Your save");
  [del addArrangedSubview:Label(@"Starts over with an empty save. It is backed up first.", 13.0, W12_DIM, NO)];
  [del addArrangedSubview:Button(@"Delete my save", NO, ^{
    [weakSelf confirm:@"Delete your save? A backup is made first." action:^{
      MakeBackup("", true);
      [[NSFileManager defaultManager] removeItemAtPath:NS(SaveDir()) error:nil];
      [[NSFileManager defaultManager] removeItemAtPath:NS(AchFile()) error:nil];
      [weakSelf setMessage:@"Save deleted (a backup was made)."];
    }];
  })];
  return page;
}

// ---- the screen ----

- (void)showPage:(NSInteger)index {
  _page = index;
  [self.view endEditing:YES];
  for (UIView* v in [_content.subviews copy]) [v removeFromSuperview];
  UIView* page = index == 1   ? [self buildInstallPage]
                 : index == 2 ? [self buildDlcPage]
                 : index == 3 ? [self buildSettingsPage]
                 : index == 4 ? [self buildControlsPage]
                 : index == 5 ? [self buildSupportPage]
                 : index == 6 ? [self buildSavesPage]
                              : [self buildPlayPage];
  page.translatesAutoresizingMaskIntoConstraints = NO;
  [_content addSubview:page];
  [NSLayoutConstraint activateConstraints:@[
    [page.topAnchor constraintEqualToAnchor:_content.topAnchor],
    [page.bottomAnchor constraintEqualToAnchor:_content.bottomAnchor],
    [page.leadingAnchor constraintEqualToAnchor:_content.leadingAnchor],
    [page.trailingAnchor constraintEqualToAnchor:_content.trailingAnchor],
  ]];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = W12_BG;
  // Tab order: Play, Install, DLC, Settings, Controls, Support, Saves.
  UISegmentedControl* tabs = [[UISegmentedControl alloc] initWithItems:@[ @"Play", @"Install", @"DLC", @"Settings", @"Controls", @"Support", @"Saves" ]];
  tabs.selectedSegmentIndex = 0;
  tabs.selectedSegmentTintColor = W12_RED;
  [tabs setTitleTextAttributes:@{NSForegroundColorAttributeName : W12_TEXT} forState:UIControlStateNormal];
  [tabs setTitleTextAttributes:@{NSForegroundColorAttributeName : W12_TEXT} forState:UIControlStateSelected];
  tabs.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:tabs];
  _content = [[UIView alloc] init];
  _content.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_content];
  UILayoutGuide* safe = self.view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [tabs.topAnchor constraintEqualToAnchor:safe.topAnchor constant:8.0],
    [tabs.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:40.0],
    [tabs.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-40.0],
    [_content.topAnchor constraintEqualToAnchor:tabs.bottomAnchor constant:10.0],
    [_content.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:40.0],
    [_content.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-40.0],
    [_content.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor constant:-4.0],
  ]];
  __weak Wwe12LauncherViewController* weakSelf = self;
  [tabs addAction:[UIAction actionWithHandler:^(__kindof UIAction* action) {
    Wwe12LauncherViewController* s = weakSelf;
    if (!s) return;
    s->_message = nil;
    [s showPage:((UISegmentedControl*)action.sender).selectedSegmentIndex];
  }] forControlEvents:UIControlEventValueChanged];
  [self showPage:0];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (BOOL)prefersStatusBarHidden { return YES; }

@end

// Called at the start of main(), before the game makes its window: shows the launcher and returns when
// Play is pressed (SDL already owns UIApplicationMain, so a nested run loop keeps UIKit alive).
// While it is up, the on-screen controller (wwe12_touch_ios.mm) stays away so the buttons get the touches.
extern "C" volatile int Wwe12IosLauncherActive = 0;

extern "C" void Wwe12IosLauncherShow(void) {
  @autoreleasepool {
    if (IsFile(Docs() + "/skip_launcher")) {
      std::fprintf(stderr, "[boot] launcher: skipped (skip_launcher file)\n");
      return;
    }
    UIWindowScene* scene = nil;
    for (UIScene* candidate in [UIApplication sharedApplication].connectedScenes) {
      if ([candidate isKindOfClass:[UIWindowScene class]]) {
        scene = (UIWindowScene*)candidate;
        break;
      }
    }
    if (!scene) {
      std::fprintf(stderr, "[boot] launcher: no window scene, starting the game directly\n");
      return;
    }
    __block BOOL play = NO;
    Wwe12LauncherViewController* controller = [[Wwe12LauncherViewController alloc] init];
    controller.onPlay = ^{ play = YES; };
    Wwe12IosLauncherActive = 1;
    UIWindow* window = [[UIWindow alloc] initWithWindowScene:scene];
    window.rootViewController = controller;
    [window makeKeyAndVisible];
    std::fprintf(stderr, "[boot] launcher: shown\n");
    NSRunLoop* loop = [NSRunLoop currentRunLoop];
    while (!play) {
      BOOL ran = [loop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
      if (!ran) usleep(10000);
    }
    std::fprintf(stderr, "[boot] launcher: Play pressed\n");
    window.hidden = YES;
    // let UIKit finish hiding it before the game makes its own window
    for (int i = 0; i < 6; ++i) {
      [loop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    window.rootViewController = nil;
    Wwe12IosLauncherActive = 0;
  }
}

// ---- foreground / background ----
// iOS stops a game that submits GPU work while it is not the active app. These notifications keep
// Wwe12IosInactive, which wwe12_ios.cpp's per-frame hook waits on.
extern "C" volatile int Wwe12IosInactive = 0;

__attribute__((constructor(104))) static void Wwe12IosWatchActivity() {
  NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
  void (^inactive)(NSNotification*) = ^(NSNotification*) { Wwe12IosInactive = 1; };
  void (^active)(NSNotification*) = ^(NSNotification*) { Wwe12IosInactive = 0; };
  [center addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:nil usingBlock:inactive];
  [center addObserverForName:UIApplicationDidEnterBackgroundNotification object:nil queue:nil usingBlock:inactive];
  [center addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:nil usingBlock:active];
  [center addObserverForName:UIApplicationWillEnterForegroundNotification object:nil queue:nil usingBlock:active];
}
