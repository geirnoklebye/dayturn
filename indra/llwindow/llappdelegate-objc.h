/**
 * @file llappdelegate-objc.h
 * @brief Class interface for the Mac version's application delegate.
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#import <Cocoa/Cocoa.h>
#import "llopenglview-objc.h"

@interface LLApplication : NSApplication
@end

@interface LLAppDelegate : NSObject <NSApplicationDelegate> {
	LLNSWindow __unsafe_unretained *window;
	NSWindow __unsafe_unretained *inputWindow;
	LLNonInlineTextView __unsafe_unretained *inputView;
	NSTimer *frameTimer;
	NSString *currentInputLanguage;
    std::string secondLogPath;
}

@property (assign) IBOutlet LLNSWindow *window;
@property (assign) IBOutlet NSWindow *inputWindow;
@property (assign) IBOutlet LLNonInlineTextView *inputView;

@property (retain) NSString *currentInputLanguage;

- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent;

- (void)applicationDidBecomeActive:(NSNotification *)notification;

- (void)applicationDidResignActive:(NSNotification *)notification;

- (void)applicationDidHide:(NSNotification *)notification;

- (void)applicationDidUnhide:(NSNotification *)notification;

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;


- (void) oneFrame;
- (void) showInputWindow:(bool)show withEvent:(NSEvent*)textEvent;
- (void) languageUpdated;
- (bool) romanScript;


- (void)applicationWillTerminate:(NSNotification *)notification;
@end

