#import "platform/cocoa_gui_internal.h"

#include <string>

#include "platform/cocoa_gui_bridge.hpp"
#include "platform/cocoa_gui_log.hpp"
#include "platform/cocoa_gui_state.hpp"
#include "platform/cocoa_gui_string.hpp"

#include "gui_persist.hpp"
#include "gui_shell_actions.hpp"

namespace persist = up::gui::persist;
namespace shell = up::gui::shell;
using up::gui::platform::cocoa::collect_scans;
using up::gui::platform::cocoa::cs;
using up::gui::platform::cocoa::log_line;
using up::gui::platform::cocoa::ns_to_utf8;
using up::gui::platform::cocoa::run_up_async;
using up::gui::platform::cocoa::save_ui;
using up::gui::platform::cocoa::upgui_apply_open_panel_directory;
using up::gui::platform::cocoa::utf8_to_ns;

@implementation UpGuiCtrl

- (void)browseCwd:(id)sender {
  (void)sender;
  NSOpenPanel* p = [NSOpenPanel openPanel];
  [p setCanChooseFiles:NO];
  [p setCanChooseDirectories:YES];
  [p setAllowsMultipleSelection:NO];
  upgui_apply_open_panel_directory(p, [cs().tf_cwd stringValue], cs().persist.browse_cwd);
  if ([p runModal] == NSModalResponseOK) {
    NSURL* u = [[p URLs] firstObject];
    if (u) {
      std::string path = persist::path_to_portable_utf8([[u path] UTF8String]);
      [cs().tf_cwd setStringValue:utf8_to_ns(path)];
    }
  }
}

- (void)scanAdd:(id)sender {
  (void)sender;
  NSOpenPanel* pan = [NSOpenPanel openPanel];
  [pan setCanChooseDirectories:YES];
  [pan setCanChooseFiles:NO];
  upgui_apply_open_panel_directory(pan, nil, cs().persist.browse_scan);
  if ([pan runModal] == NSModalResponseOK) {
    NSURL* u = [[pan URLs] firstObject];
    if (u) {
      NSString* path = [u path];
      cs().persist.browse_scan = persist::path_to_portable_utf8([path UTF8String]);
      [cs().scan_rows addObject:path];
      [cs().table_scan reloadData];
    }
  }
}

- (void)scanRemove:(id)sender {
  (void)sender;
  const NSInteger row = [cs().table_scan selectedRow];
  if (row >= 0 && row < (NSInteger)[cs().scan_rows count]) {
    [cs().scan_rows removeObjectAtIndex:static_cast<NSUInteger>(row)];
    [cs().table_scan reloadData];
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
  const std::string cwd = persist::path_to_portable_utf8(ns_to_utf8([cs().tf_cwd stringValue]));
  const std::string cfg =
      shell::build_configure_args_line(cs().up_exe, cwd, ns_to_utf8([cs().tf_build stringValue]), collect_scans(),
                                       cs().persist, [](const std::string& s) { log_line(s); });
  if (cfg.empty())
    return;
  run_up_async(cfg);
}

- (void)doBuild:(id)sender {
  (void)sender;
  std::string args = "build";
  const std::string leaf = persist::intermediate_leaf_from_build_dir_field(ns_to_utf8([cs().tf_build stringValue]));
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
  std::string tgt = ns_to_utf8([cs().tf_run stringValue]);
  shell::trim_ascii_inplace(tgt);
  if (tgt.empty()) {
    log_line("[error] Run target empty.");
    return;
  }
  std::string err, args = "run";
  if (!shell::append_install_dir_flag(args, ns_to_utf8([cs().tf_install stringValue]), err)) {
    log_line("[error] " + err);
    return;
  }
  args += " ";
  args += persist::shell_single_quote(tgt);
  run_up_async(args);
}

- (void)doTest:(id)sender {
  (void)sender;
  std::string tgt = ns_to_utf8([cs().tf_test stringValue]);
  shell::trim_ascii_inplace(tgt);
  std::string err, args = "test";
  if (!shell::append_install_dir_flag(args, ns_to_utf8([cs().tf_install stringValue]), err)) {
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
  if (!shell::append_install_dir_flag(args, ns_to_utf8([cs().tf_install stringValue]), err)) {
    log_line("[error] " + err);
    return;
  }
  run_up_async(args);
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tv {
  (void)tv;
  return (NSInteger)[cs().scan_rows count];
}

- (id)tableView:(NSTableView*)tv objectValueForTableColumn:(NSTableColumn*)col row:(NSInteger)row {
  (void)tv;
  (void)col;
  if (row < 0 || row >= (NSInteger)[cs().scan_rows count])
    return @"";
  return cs().scan_rows[static_cast<NSUInteger>(row)];
}

- (void)windowWillClose:(NSNotification*)n {
  (void)n;
  save_ui();
  [NSApp terminate:nil];
}

@end
