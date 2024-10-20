/** 
 * @file lluploaddialog.h
 * @brief LLUploadDialog class header file
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_UPLOADDIALOG_H
#define LL_UPLOADDIALOG_H

#include "llpanel.h"
#include "lltextbox.h"
			
class LLUploadDialog : public LLPanel
{
public:
	// Use this function to open a modal dialog and display it until the user presses the "close" button.
	static LLUploadDialog*	modalUploadDialog(const std::string& msg);		// Message to display
	static void				modalUploadFinished();		// Message to display

	static bool				modalUploadIsFinished() { return (sDialog == nullptr); }

	void setMessage( const std::string& msg );

private:
	LLUploadDialog( const std::string& msg);
	virtual ~LLUploadDialog();	// No you can't kill it.  It can only kill itself.

	LLTextBox* mLabelBox[16];

private:
	static LLUploadDialog*	sDialog;  // Hidden singleton instance, created and destroyed as needed.
};

#endif  // LL_UPLOADDIALOG_H
