//
//          Copyright (c) 2018, Scientific Toolworks, Inc.
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Jason Haslam
//

#include "Application.h"
#include "ui/window/MainWindow.h"
#import <AppKit/AppKit.h>
#include <QUrl>

@interface Launcher : NSObject
- (void)openInPawmmit:(NSPasteboard *)pboard
             userData:(NSString *)userData
                error:(NSString **)error;
@end

@implementation Launcher
- (void)openInPawmmit:(NSPasteboard *)pboard
             userData:(NSString *)userData
                error:(NSString **)error {
  NSArray *classes = [NSArray arrayWithObject:[NSURL class]];
  NSDictionary *options = [NSDictionary dictionary];
  NSArray *urls = [pboard readObjectsForClasses:classes options:options];
  for (id url in urls)
    MainWindow::open(QUrl::fromNSURL([url filePathURL]).toLocalFile(), false);
  [pboard clearContents];
}
@end

void Application::registerService() {
  Launcher *launcher = [[Launcher alloc] init];
  [[NSApplication sharedApplication] setServicesProvider:launcher];
}
