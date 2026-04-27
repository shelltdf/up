// macOS Cocoa：与 GTK/Linux 端一致的 up-gui 主窗口（同一 up_gui_settings.txt 与 configure 传参；与 Windows 版 platform/win32_gui.cpp 行为对齐）。

#import <Cocoa/Cocoa.h>

#include "platform/cocoa_gui.hpp"

#include "gui_persist.hpp"

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace up::gui::platform::cocoa {

namespace persist = up::gui::persist;

std::atomic<bool> g_busy{false};

persist::PersistedEnv g_persist{};
std::filesystem::path g_settings_file;
std::filesystem::path g_up_exe;

NSTextField* g_tf_cwd;
NSTextField* g_tf_build;
NSTextField* g_tf_install;
NSTextField* g_tf_extra;
NSTextField* g_tf_run;
NSTextField* g_tf_test;
NSTableView* g_table_scan;
NSMutableArray<NSString*>* g_scan_rows;
NSTextView* g_log;
NSTextField* g_status;

static void trim_ascii(std::string& s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.erase(0, 1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
}

static std::string ns_to_utf8(NSString* s) {
  if (!s)
    return {};
  return std::string([s UTF8String]);
}

static NSString* utf8_to_ns(const std::string& s) {
  return [NSString stringWithUTF8String:s.c_str()];
}

static void log_append(NSString* line) {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (g_log) {
      NSTextStorage* st = [g_log textStorage];
      [st appendAttributedString:[[NSAttributedString alloc] initWithString:line]];
      [g_log scrollRangeToVisible:NSMakeRange([[g_log string] length], 0)];
    }
  });
}

static void log_line(const std::string& s) {
  log_append([NSString stringWithUTF8String:(s + "\n").c_str()]);
}

static void set_status(const char* s) {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (g_status)
      [g_status setStringValue:[NSString stringWithUTF8String:s]];
  });
}

static std::vector<std::string> collect_scans() {
  std::vector<std::string> out;
  for (NSString* row in g_scan_rows)
    if (row.length)
      out.push_back([row UTF8String]);
  return out;
}

static void save_ui() {
  g_persist.browse_cwd = persist::path_to_portable_utf8(ns_to_utf8([g_tf_cwd stringValue]));
  persist::save_settings(g_settings_file, g_persist);
}

static void append_install_dir_flag(std::string& args, const std::string& inst_edit, std::string& err) {
  std::string inst = persist::path_to_portable_utf8(inst_edit);
  trim_ascii(inst);
  if (inst.empty()) {
    err = "Install Dir empty";
    return;
  }
  const std::string leaf = persist::intermediate_leaf_from_build_dir_field(inst);
  if (leaf.empty()) {
    err = "Invalid install dir leaf";
    return;
  }
  args += " --install-dir-name ";
  args += persist::shell_single_quote(leaf);
}

static void run_up_async(const std::string& args_no_exe) {
  if (g_busy.exchange(true)) {
    log_line("[busy] previous run still in progress");
    g_busy = false;
    return;
  }
  const std::string cwd = persist::path_to_portable_utf8(ns_to_utf8([g_tf_cwd stringValue]));
  if (cwd.empty()) {
    log_line("[error] Set CWD first.");
    g_busy = false;
    return;
  }
  if (!std::filesystem::exists(g_up_exe)) {
    log_line("[error] `up` not found next to this executable.");
    g_busy = false;
    return;
  }
  std::string extra = ns_to_utf8([g_tf_extra stringValue]);
  trim_ascii(extra);

  std::thread([args_no_exe, cwd, extra]() {
    const std::filesystem::path cwd_p(cwd);
    std::string cmd = persist::shell_single_quote(g_up_exe.generic_string()) + " " + args_no_exe;
    if (!extra.empty())
      cmd += " " + extra;
    std::string out;
    int code = -1;
    const bool ok = persist::run_shell_in_dir(cwd_p, cmd, out, code);
    if (!ok)
      log_line("[error] failed to spawn shell");
    else {
      if (!out.empty())
        log_line(out);
      log_line("[exit " + std::to_string(code) + "]");
    }
    g_busy = false;
    set_status("Ready");
  }).detach();
  set_status("Running up…");
}

static std::string build_configure_args_line() {
  const std::string cwd = persist::path_to_portable_utf8(ns_to_utf8([g_tf_cwd stringValue]));
  if (cwd.empty()) {
    log_line("[error] Set CWD first.");
    return {};
  }
  std::string leaf, qerr;
  if (!persist::query_print_build_dir_name(g_up_exe, std::filesystem::path(cwd), ns_to_utf8([g_tf_build stringValue]),
                                              g_persist, leaf, qerr)) {
    log_line("[error] print-build-dir-name: " + qerr);
    return {};
  }
  std::string args = "configure";
  persist::append_scan_args_utf8(args, collect_scans(), cwd);
  persist::append_configure_env_opts(g_persist, args);
  args += " --build-dir-name ";
  args += persist::shell_single_quote(leaf);
  return args;
}

@interface UpGuiCtrl : NSObject <NSTableViewDataSource, NSTableViewDelegate, NSWindowDelegate>
@end

static void upgui_apply_open_panel_directory(NSOpenPanel* p, NSString* primaryPath, const std::string& fallbackUtf8) {
  NSString* s = primaryPath;
  if (s.length == 0 && !fallbackUtf8.empty())
    s = utf8_to_ns(fallbackUtf8);
  if (s.length == 0)
    return;
  NSString* expanded = [s stringByExpandingTildeInPath];
  BOOL isDir = NO;
  if ([[NSFileManager defaultManager] fileExistsAtPath:expanded isDirectory:&isDir] && isDir)
    [p setDirectoryURL:[NSURL fileURLWithPath:expanded isDirectory:YES]];
}

@implementation UpGuiCtrl

- (void)browseCwd:(id)sender {
  (void)sender;
  NSOpenPanel* p = [NSOpenPanel openPanel];
  [p setCanChooseFiles:NO];
  [p setCanChooseDirectories:YES];
  [p setAllowsMultipleSelection:NO];
  upgui_apply_open_panel_directory(p, [g_tf_cwd stringValue], g_persist.browse_cwd);
  if ([p runModal] == NSModalResponseOK) {
    NSURL* u = [[p URLs] firstObject];
    if (u) {
      std::string path = persist::path_to_portable_utf8([[u path] UTF8String]);
      [g_tf_cwd setStringValue:utf8_to_ns(path)];
    }
  }
}

- (void)scanAdd:(id)sender {
  (void)sender;
  NSOpenPanel* pan = [NSOpenPanel openPanel];
  [pan setCanChooseDirectories:YES];
  [pan setCanChooseFiles:NO];
  upgui_apply_open_panel_directory(pan, nil, g_persist.browse_scan);
  if ([pan runModal] == NSModalResponseOK) {
    NSURL* u = [[pan URLs] firstObject];
    if (u) {
      NSString* path = [u path];
      g_persist.browse_scan = persist::path_to_portable_utf8([path UTF8String]);
      [g_scan_rows addObject:path];
      [g_table_scan reloadData];
    }
  }
}

- (void)scanRemove:(id)sender {
  (void)sender;
  const NSInteger row = [g_table_scan selectedRow];
  if (row >= 0 && row < (NSInteger)[g_scan_rows count]) {
    [g_scan_rows removeObjectAtIndex:static_cast<NSUInteger>(row)];
    [g_table_scan reloadData];
  }
}

#if UP_ENABLE_REVERSE
- (void)doReverse:(id)sender {
  (void)sender;
  run_up_async("reverse");
}
#endif

- (void)doConfigure:(id)sender {
  (void)sender;
  const std::string cfg = build_configure_args_line();
  if (cfg.empty())
    return;
  run_up_async(cfg);
}

- (void)doBuild:(id)sender {
  (void)sender;
  std::string args = "build";
  const std::string leaf = persist::intermediate_leaf_from_build_dir_field(ns_to_utf8([g_tf_build stringValue]));
  if (leaf.empty()) {
    log_line("[error] Set Build Dir first.");
    return;
  }
  args += " --build-dir-name ";
  args += persist::shell_single_quote(leaf);
  run_up_async(args);
}

- (void)doRun:(id)sender {
  (void)sender;
  std::string tgt = ns_to_utf8([g_tf_run stringValue]);
  trim_ascii(tgt);
  if (tgt.empty()) {
    log_line("[error] Run target empty.");
    return;
  }
  std::string err, args = "run";
  append_install_dir_flag(args, ns_to_utf8([g_tf_install stringValue]), err);
  if (!err.empty()) {
    log_line("[error] " + err);
    return;
  }
  args += " ";
  args += persist::shell_single_quote(tgt);
  run_up_async(args);
}

- (void)doTest:(id)sender {
  (void)sender;
  std::string tgt = ns_to_utf8([g_tf_test stringValue]);
  trim_ascii(tgt);
  std::string err, args = "test";
  append_install_dir_flag(args, ns_to_utf8([g_tf_install stringValue]), err);
  if (!err.empty()) {
    log_line("[error] " + err);
    return;
  }
  if (!tgt.empty()) {
    args += " ";
    args += persist::shell_single_quote(tgt);
  }
  run_up_async(args);
}

- (void)doPack:(id)sender {
  (void)sender;
  std::string err, args = "pack";
  append_install_dir_flag(args, ns_to_utf8([g_tf_install stringValue]), err);
  if (!err.empty()) {
    log_line("[error] " + err);
    return;
  }
  run_up_async(args);
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tv {
  (void)tv;
  return (NSInteger)[g_scan_rows count];
}

- (id)tableView:(NSTableView*)tv objectValueForTableColumn:(NSTableColumn*)col row:(NSInteger)row {
  (void)tv;
  (void)col;
  if (row < 0 || row >= (NSInteger)[g_scan_rows count])
    return @"";
  return g_scan_rows[static_cast<NSUInteger>(row)];
}

- (void)windowWillClose:(NSNotification*)n {
  (void)n;
  save_ui();
  [NSApp terminate:nil];
}

@end

static NSButton* makeBtn(NSString* title, id target, SEL act) {
  NSButton* b = [[NSButton alloc] initWithFrame:NSZeroRect];
  [b setTitle:title];
  [b setBezelStyle:NSBezelStyleRounded];
  [b setTarget:target];
  [b setAction:act];
  return b;
}

static NSTextField* makeLabel(NSString* t) {
  NSTextField* f = [[NSTextField alloc] initWithFrame:NSZeroRect];
  [f setStringValue:t];
  [f setBezeled:NO];
  [f setDrawsBackground:NO];
  [f setEditable:NO];
  [f setSelectable:NO];
  return f;
}

static NSTextField* makeField() {
  NSTextField* f = [[NSTextField alloc] initWithFrame:NSZeroRect];
  [f setEditable:YES];
  return f;
}

static void build_window() {
  g_settings_file = persist::settings_path_near_executable();
  g_up_exe = persist::executable_parent_dir() / "up";
  (void)persist::load_settings(g_settings_file, g_persist);
  g_scan_rows = [NSMutableArray array];

  {
    const std::filesystem::path icon_path = persist::executable_parent_dir() / "up_gui.png";
    if (std::filesystem::exists(icon_path)) {
      NSString* ip = [NSString stringWithUTF8String:icon_path.string().c_str()];
      NSImage* img = [[NSImage alloc] initWithContentsOfFile:ip];
      if (img)
        [NSApp setApplicationIconImage:img];
    }
  }

  const CGFloat W = 920, H = 680;
  NSRect rect = NSMakeRect(0, 0, W, H);
  NSWindow* win = [[NSWindow alloc] initWithContentRect:rect styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                                            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                                                backing:NSBackingStoreBuffered defer:NO];
  [win setTitle:@"up-gui"];
  UpGuiCtrl* ctrl = [[UpGuiCtrl alloc] init];
  [win setDelegate:ctrl];

  NSView* root = [win contentView];
  [root setAutoresizesSubviews:YES];

  NSMenu* mainBar = [[NSMenu alloc] initWithTitle:@""];
  NSMenuItem* appItem = [[NSMenuItem alloc] initWithTitle:@"up-gui" action:nil keyEquivalent:@""];
  NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"up-gui"];
  NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit up-gui" action:@selector(terminate:) keyEquivalent:@"q"];
  [quitItem setTarget:NSApp];
  [appMenu addItem:quitItem];
  [appItem setSubmenu:appMenu];
  [mainBar addItem:appItem];
  NSMenuItem* actTop = [[NSMenuItem alloc] initWithTitle:@"Actions" action:nil keyEquivalent:@""];
  NSMenu* actMenu = [[NSMenu alloc] initWithTitle:@"Actions"];
  NSMenuItem* m;
#if UP_ENABLE_REVERSE
  m = [[NSMenuItem alloc] initWithTitle:@"Reverse" action:@selector(doReverse:) keyEquivalent:@"j"];
  [m setTarget:ctrl];
  [actMenu addItem:m];
#endif
  m = [[NSMenuItem alloc] initWithTitle:@"Configure" action:@selector(doConfigure:) keyEquivalent:@"k"];
  [m setTarget:ctrl];
  [actMenu addItem:m];
  m = [[NSMenuItem alloc] initWithTitle:@"Build" action:@selector(doBuild:) keyEquivalent:@"b"];
  [m setTarget:ctrl];
  [actMenu addItem:m];
  m = [[NSMenuItem alloc] initWithTitle:@"Run" action:@selector(doRun:) keyEquivalent:@"r"];
  [m setTarget:ctrl];
  [actMenu addItem:m];
  m = [[NSMenuItem alloc] initWithTitle:@"Test" action:@selector(doTest:) keyEquivalent:@"t"];
  [m setTarget:ctrl];
  [actMenu addItem:m];
  m = [[NSMenuItem alloc] initWithTitle:@"Pack" action:@selector(doPack:) keyEquivalent:@"p"];
  [m setTarget:ctrl];
  [actMenu addItem:m];
  [actTop setSubmenu:actMenu];
  [mainBar addItem:actTop];
  [NSApp setMainMenu:mainBar];

  __block CGFloat y = H - 40;
  NSButton* b1 = makeBtn(@"Configure", ctrl, @selector(doConfigure:));
  NSButton* b2 = makeBtn(@"Build", ctrl, @selector(doBuild:));
  NSButton* b3 = makeBtn(@"Run", ctrl, @selector(doRun:));
  NSButton* b4 = makeBtn(@"Test", ctrl, @selector(doTest:));
  NSButton* b5 = makeBtn(@"Pack", ctrl, @selector(doPack:));
#if UP_ENABLE_REVERSE
  NSButton* b0 = makeBtn(@"Reverse", ctrl, @selector(doReverse:));
#endif
  CGFloat x = 20;
#if UP_ENABLE_REVERSE
  for (NSButton* b in @[ b0, b1, b2, b3, b4, b5 ]) {
#else
  for (NSButton* b in @[ b1, b2, b3, b4, b5 ]) {
#endif
    [b setFrame:NSMakeRect(x, y, 100, 28)];
    [root addSubview:b];
    x += 108;
  }
  y -= 44;

  auto place_row = ^(NSString* label, NSTextField** field, BOOL browse, CGFloat height) {
    NSTextField* lab = makeLabel(label);
    [lab setFrame:NSMakeRect(20, y - height + 4, 100, height)];
    [root addSubview:lab];
    *field = makeField();
    [*field setFrame:NSMakeRect(130, y - height, W - 170 - (browse ? 100 : 0), height)];
    [root addSubview:*field];
    if (browse) {
      NSButton* bb = makeBtn(@"Browse…", ctrl, @selector(browseCwd:));
      [bb setFrame:NSMakeRect(W - 120, y - height, 90, height)];
      [root addSubview:bb];
    }
    y -= height + 8;
  };

  place_row(@"CWD", &g_tf_cwd, YES, 24);
  y -= 8;

  NSTextField* scanLab = makeLabel(@"Scan");
  [scanLab setFrame:NSMakeRect(20, y - 100, 100, 20)];
  [root addSubview:scanLab];
  g_table_scan = [[NSTableView alloc] initWithFrame:NSMakeRect(130, y - 100, W - 260, 100)];
  NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:@"p"];
  [[col headerCell] setStringValue:@"Scan roots"];
  [col setWidth:W - 280];
  [g_table_scan addTableColumn:col];
  [g_table_scan setDataSource:ctrl];
  [g_table_scan setDelegate:ctrl];
  NSScrollView* sv = [[NSScrollView alloc] initWithFrame:NSMakeRect(130, y - 100, W - 260, 100)];
  [sv setDocumentView:g_table_scan];
  [sv setHasVerticalScroller:YES];
  [root addSubview:sv];
  NSButton* sa = makeBtn(@"+Add", ctrl, @selector(scanAdd:));
  [sa setFrame:NSMakeRect(W - 120, y - 40, 90, 28)];
  [root addSubview:sa];
  NSButton* sd = makeBtn(@"-Remove", ctrl, @selector(scanRemove:));
  [sd setFrame:NSMakeRect(W - 120, y - 80, 90, 28)];
  [root addSubview:sd];
  y -= 120;

  place_row(@"Build dir", &g_tf_build, NO, 24);
  place_row(@"Install dir", &g_tf_install, NO, 24);
  place_row(@"Extra args", &g_tf_extra, NO, 24);
  place_row(@"Run target", &g_tf_run, NO, 24);
  place_row(@"Test name", &g_tf_test, NO, 24);

  NSScrollView* logScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 80, W - 40, y - 100)];
  [logScroll setHasVerticalScroller:YES];
  g_log = [[NSTextView alloc] initWithFrame:[[logScroll contentView] bounds]];
  [g_log setEditable:NO];
  [g_log setFont:[NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular]];
  [logScroll setDocumentView:g_log];
  [root addSubview:logScroll];

  g_status = makeField();
  [g_status setEditable:NO];
  [g_status setFrame:NSMakeRect(20, 20, W - 40, 24)];
  [g_status setStringValue:@"Ready"];
  [root addSubview:g_status];

  if (!g_persist.browse_cwd.empty())
    [g_tf_cwd setStringValue:utf8_to_ns(g_persist.browse_cwd)];

  // Center main window on the work area of the display under the mouse (fallback: main screen).
  {
    NSScreen* sc = [NSScreen mainScreen];
    const NSPoint mouse = [NSEvent mouseLocation];
    for (NSScreen* candidate in [NSScreen screens]) {
      if (NSMouseInRect(mouse, [candidate frame], NO)) {
        sc = candidate;
        break;
      }
    }
    const NSRect vf = [sc visibleFrame];
    NSRect f = [win frame];
    f.origin.x = NSMidX(vf) - NSWidth(f) * 0.5;
    f.origin.y = NSMidY(vf) - NSHeight(f) * 0.5;
    if (NSMaxX(f) > NSMaxX(vf))
      f.origin.x = NSMaxX(vf) - NSWidth(f);
    if (NSMaxY(f) > NSMaxY(vf))
      f.origin.y = NSMaxY(vf) - NSHeight(f);
    if (f.origin.x < NSMinX(vf))
      f.origin.x = NSMinX(vf);
    if (f.origin.y < NSMinY(vf))
      f.origin.y = NSMinY(vf);
    [win setFrame:f display:NO];
  }
  [win makeKeyAndOrderFront:nil];
  log_line("up-gui (Cocoa) — settings: " + g_settings_file.generic_string());
}

int run(int argc, char** argv) {
  (void)argc;
  (void)argv;
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    build_window();
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
  }
  return 0;
}

}  // namespace up::gui::platform::cocoa
