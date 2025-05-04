/** 
 * @file llfloateradvancedbuildoptions.cpp
 * @brief LLFloaterAdvancedBuildOptions class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copvright (C) 2013, Nicky Perian <nickyperian@yahoo.com>
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

/**
 * Panel for setting global object-editing options, specifically
 * grid size and spacing.
 */ 

#include "llviewerprecompiledheaders.h"

#include "llfloateradvancedbuildoptions.h"
#include "lluictrlfactory.h"
#include "llcombobox.h"
#include "llselectmgr.h"
#include "llfloatertools.h"
//
// Methods
//

LLFloaterAdvancedBuildOptions::LLFloaterAdvancedBuildOptions(const LLSD& key)
  : LLFloater(key)
{
}

LLFloaterAdvancedBuildOptions::~LLFloaterAdvancedBuildOptions()
{}

bool LLFloaterAdvancedBuildOptions::postBuild()
{
	// <NP: disable build constraints>
	gFloaterTools->updateToolsSizeLimits();
	// </NP: disable build constraints>
	return true;
}

// virtual
void LLFloaterAdvancedBuildOptions::onOpen(const LLSD& key)
{
	mObjectSelection = LLSelectMgr::getInstance()->getEditSelection();
}

// virtual
void LLFloaterAdvancedBuildOptions::onClose(bool app_quitting)
{
	mObjectSelection = NULL;
}
