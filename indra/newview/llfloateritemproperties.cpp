/**
 * @file llfloateritemproperties.cpp
 * @brief Implementation of the item/task properties floater
 * @author merov@lindenlab.com
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

#include "llviewerprecompiledheaders.h"

#include "llfloateritemproperties.h"

#include "llbutton.h"
#include "llselectmgr.h"
#include "llsidepanelinventorysubpanel.h"
#include "llsidepaneliteminfo.h"
#include "llsidepaneltaskinfo.h"

//-----------------------------------------------------------------------------
// LLFloaterItemProperties
//-----------------------------------------------------------------------------

LLFloaterItemProperties::LLFloaterItemProperties(const LLSD& key)
:	LLFloater(key)
{
}

LLFloaterItemProperties::~LLFloaterItemProperties()
{
}

bool LLFloaterItemProperties::postBuild()
{
    LLSidepanelInventorySubpanel* panel = findChild<LLSidepanelInventorySubpanel>("sidepanel");
    if (panel)
    {
        LLButton* back_btn = panel->getChild<LLButton>("back_btn");
        back_btn->setVisible(false);
    }

	return LLFloater::postBuild();
}

void LLFloaterItemProperties::onOpen(const LLSD& key)
{
    // Tell the panel which item it needs to visualize
    LLSidepanelInventorySubpanel* panel = findChild<LLSidepanelInventorySubpanel>("sidepanel");

    LLSidepanelItemInfo* item_panel = dynamic_cast<LLSidepanelItemInfo*>(panel);
    if (item_panel)
    {
        item_panel->setItemID(key["id"].asUUID());
        if (key.has("object"))
        {
            item_panel->setObjectID(key["object"].asUUID());
        }
    }

    LLSidepanelTaskInfo* task_panel = dynamic_cast<LLSidepanelTaskInfo*>(panel);
    if (task_panel)
    {
        task_panel->setObjectSelection(LLSelectMgr::getInstance()->getSelection());
    }
}
