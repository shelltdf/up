#import <Cocoa/Cocoa.h>

// 供 cocoa_gui_window.mm / cocoa_gui_handlers.mm 共享的控制器前向声明（实现仅在 handlers）。
@interface UpGuiCtrl : NSObject <NSTableViewDataSource, NSTableViewDelegate, NSWindowDelegate>
@end
