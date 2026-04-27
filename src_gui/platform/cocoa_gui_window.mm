#import "platform/cocoa_gui_internal.h"

#import <Cocoa/Cocoa.h>

#include "platform/cocoa_gui_log.hpp"
#include "platform/cocoa_gui_state.hpp"
#include "platform/cocoa_gui_string.hpp"
#include "platform/cocoa_gui_window.hpp"

#include "gui_persist.hpp"

#include <filesystem>
#include <string>

namespace up::gui::platform::cocoa {

namespace persist = up::gui::persist;

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

void build_window() {
  cs().settings_file = persist::settings_path_near_executable();
  cs().up_exe = persist::executable_parent_dir() / "up";
  (void)persist::load_settings(cs().settings_file, cs().persist);
  cs().scan_rows = [NSMutableArray array];

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

  place_row(@"CWD", &cs().tf_cwd, YES, 24);
  y -= 8;

  NSTextField* scanLab = makeLabel(@"Scan");
  [scanLab setFrame:NSMakeRect(20, y - 100, 100, 20)];
  [root addSubview:scanLab];
  cs().table_scan = [[NSTableView alloc] initWithFrame:NSMakeRect(130, y - 100, W - 260, 100)];
  NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:@"p"];
  [[col headerCell] setStringValue:@"Scan roots"];
  [col setWidth:W - 280];
  [cs().table_scan addTableColumn:col];
  [cs().table_scan setDataSource:ctrl];
  [cs().table_scan setDelegate:ctrl];
  NSScrollView* sv = [[NSScrollView alloc] initWithFrame:NSMakeRect(130, y - 100, W - 260, 100)];
  [sv setDocumentView:cs().table_scan];
  [sv setHasVerticalScroller:YES];
  [root addSubview:sv];
  NSButton* sa = makeBtn(@"+Add", ctrl, @selector(scanAdd:));
  [sa setFrame:NSMakeRect(W - 120, y - 40, 90, 28)];
  [root addSubview:sa];
  NSButton* sd = makeBtn(@"-Remove", ctrl, @selector(scanRemove:));
  [sd setFrame:NSMakeRect(W - 120, y - 80, 90, 28)];
  [root addSubview:sd];
  y -= 120;

  place_row(@"Build dir", &cs().tf_build, NO, 24);
  place_row(@"Install dir", &cs().tf_install, NO, 24);
  place_row(@"Extra args", &cs().tf_extra, NO, 24);
  place_row(@"Run target", &cs().tf_run, NO, 24);
  place_row(@"Test name", &cs().tf_test, NO, 24);

  NSScrollView* logScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 80, W - 40, y - 100)];
  [logScroll setHasVerticalScroller:YES];
  cs().log = [[NSTextView alloc] initWithFrame:[[logScroll contentView] bounds]];
  [cs().log setEditable:NO];
  [cs().log setFont:[NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular]];
  [logScroll setDocumentView:cs().log];
  [root addSubview:logScroll];

  cs().status = makeField();
  [cs().status setEditable:NO];
  [cs().status setFrame:NSMakeRect(20, 20, W - 40, 24)];
  [cs().status setStringValue:@"Ready"];
  [root addSubview:cs().status];

  if (!cs().persist.browse_cwd.empty())
    [cs().tf_cwd setStringValue:utf8_to_ns(cs().persist.browse_cwd)];

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
  log_line("up-gui (Cocoa) — settings: " + cs().settings_file.generic_string());
}

}  // namespace up::gui::platform::cocoa
