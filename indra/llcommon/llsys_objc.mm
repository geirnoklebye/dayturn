/*
 * @file llsys_objc.mm
 * @brief Some objective-c crap for llcommon
 *
 * (C) 2014 Cinder Roxley @ Second Life <cinder@alchemyviewer.org>
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LL_DARWIN
#  error "This file should only be included when building on OS X!"
#else

#import "llsys_objc.h"
#import <Foundation/Foundation.h>
#import <AppKit/NSApplication.h>

static int intAtStringIndex(NSArray *array, int index)
{
    return [(NSString *) array[index] integerValue];
}

bool LLSysDarwin::getOperatingSystemInfo(int &major, int &minor, int &patch)
{
	// Mavericks gains a nifty little method for getting OS version, prior to that
	// we have to (ugh) parse systemversion.plist. :O
	if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_8) {
		NSOperatingSystemVersion osVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
		major = osVersion.majorVersion;
		minor = osVersion.minorVersion;
		patch = osVersion.patchVersion;
	}
	else
	{
		NSString* versionString = [NSDictionary dictionaryWithContentsOfFile:
				@"/System/Library/CoreServices/SystemVersion.plist"][@"ProductVersion"];
		NSArray* versions = [versionString componentsSeparatedByString:@"."];
		NSUInteger count = [versions count];
		if (count > 0) {
			major = intAtStringIndex(versions, 0);
			if (count > 1) {
				minor = intAtStringIndex(versions, 1);
				if (count > 2) {
					patch = intAtStringIndex(versions, 2);
				}
			}
		}
	}
	return true;
}

const char* LLSysDarwin::getPreferredLanguage()
{
	NSString* lang = [NSLocale preferredLanguages][0];
	const char* ret = [lang cStringUsingEncoding:NSASCIIStringEncoding];
	return ret;
}

#endif // !LL_DARWIN
