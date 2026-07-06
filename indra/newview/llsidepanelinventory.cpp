/**
 * @file LLSidepanelInventory.cpp
 * @brief Side Bar "Inventory" panel
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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
#include "llsidepanelinventory.h"

#include "llagent.h"
#include "llappearancemgr.h"
#include "llavataractions.h"
#include "llbutton.h"
#include "llfirstuse.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llfoldertype.h"
#include "llfolderview.h"
#include "llinventorybridge.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llinventorypanel.h"
#include "lloutfitobserver.h"
#include "llpanelmaininventory.h"
#include "llselectmgr.h"
#include "llsidepaneliteminfo.h"
#include "llsidepaneltaskinfo.h"
#include "llstring.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "llviewermedia.h"
#include "llviewernetwork.h"

#include "llfiltereditor.h"

static LLPanelInjector<LLSidepanelInventory> t_inventory("sidepanel_inventory");

//
// Implementation
//

LLSidepanelInventory::LLSidepanelInventory()
	: LLPanel()
	, mItemPanel(nullptr)
	, mPanelMainInventory(nullptr)
{
	//buildFromFile( "panel_inventory.xml"); // Called from LLRegisterPanelClass::defaultPanelClassBuilder()
}

LLSidepanelInventory::~LLSidepanelInventory()
{
}

bool LLSidepanelInventory::postBuild()
{
	// UI elements from inventory panel
	{
		mInventoryPanel = getChild<LLPanel>("sidepanel_inventory_panel");

		mInfoBtn = mInventoryPanel->getChild<LLButton>("info_btn");
		mInfoBtn->setClickedCallback(boost::bind(&LLSidepanelInventory::onInfoButtonClicked, this));
		
		mShareBtn = mInventoryPanel->getChild<LLButton>("share_btn");
		mShareBtn->setClickedCallback(boost::bind(&LLSidepanelInventory::onShareButtonClicked, this));

		mWearBtn = mInventoryPanel->getChild<LLButton>("wear_btn");
		mWearBtn->setClickedCallback(boost::bind(&LLSidepanelInventory::onWearButtonClicked, this));
		
		mPlayBtn = mInventoryPanel->getChild<LLButton>("play_btn");
		mPlayBtn->setClickedCallback(boost::bind(&LLSidepanelInventory::onPlayButtonClicked, this));
		
		mTeleportBtn = mInventoryPanel->getChild<LLButton>("teleport_btn");
		mTeleportBtn->setClickedCallback(boost::bind(&LLSidepanelInventory::onTeleportButtonClicked, this));
		
		mPanelMainInventory = mInventoryPanel->getChild<LLPanelMainInventory>("panel_main_inventory");
		mPanelMainInventory->setSelectCallback(boost::bind(&LLSidepanelInventory::onSelectionChange, this, _1, _2));
		LLTabContainer* tabs = mPanelMainInventory->getChild<LLTabContainer>("inventory filter tabs");
		tabs->setCommitCallback(boost::bind(&LLSidepanelInventory::updateVerbs, this));

		/* 
		   EXT-4846 : "Can we suppress the "Landmarks" and "My Favorites" folder since they have their own Task Panel?"
		   Deferring this until 2.1.
		LLInventoryPanel *my_inventory_panel = mPanelMainInventory->getChild<LLInventoryPanel>("All Items");
		my_inventory_panel->addHideFolderType(LLFolderType::FT_LANDMARK);
		my_inventory_panel->addHideFolderType(LLFolderType::FT_FAVORITE);
		*/

		LLOutfitObserver::instance().addCOFChangedCallback(boost::bind(&LLSidepanelInventory::updateVerbs, this));
	}

	// UI elements from item panel
	{
		mItemPanel = getChild<LLSidepanelItemInfo>("sidepanel__item_panel");
		
		LLButton* back_btn = mItemPanel->getChild<LLButton>("back_btn");
		back_btn->setClickedCallback(boost::bind(&LLSidepanelInventory::onBackButtonClicked, this));
	}

	// UI elements from task panel
	{
		mTaskPanel = findChild<LLSidepanelTaskInfo>("sidepanel__task_panel");
		if (mTaskPanel)
		{
			LLButton* back_btn = mTaskPanel->getChild<LLButton>("back_btn");
			back_btn->setClickedCallback(boost::bind(&LLSidepanelInventory::onBackButtonClicked, this));
		}
	}

	// Update the verbs buttons state.
	updateVerbs();

	return true;
}

void LLSidepanelInventory::onOpen(const LLSD& key)
{
	LLFirstUse::newInventory(false);
	mPanelMainInventory->setFocusFilterEditor();

	if(key.size() == 0)
	{
		// set focus on filter editor when side tray inventory shows up
		LLFilterEditor* filter_editor = mPanelMainInventory->getChild<LLFilterEditor>("inventory search editor");
		filter_editor->setFocus(true);
		return;
	}


	mItemPanel->reset();

	if (key.has("id"))
	{
		mItemPanel->setItemID(key["id"].asUUID());
		if (key.has("object"))
		{
			mItemPanel->setObjectID(key["object"].asUUID());
		}
		showItemInfoPanel();
	}
	if (key.has("task"))
	{
		if (mTaskPanel)
			mTaskPanel->setObjectSelection(LLSelectMgr::getInstance()->getSelection());
		showTaskInfoPanel();
	}
}

void LLSidepanelInventory::onInfoButtonClicked()
{
	LLInventoryItem *item = getSelectedItem();
	if (item)
	{
		mItemPanel->reset();
		mItemPanel->setItemID(item->getUUID());
		showItemInfoPanel();
	}
}

void LLSidepanelInventory::onShareButtonClicked()
{
	LLAvatarActions::shareWithAvatars(this);
}

void LLSidepanelInventory::performActionOnSelection(const std::string &action)
{
	LLFolderViewItem* current_item = mPanelMainInventory->getActivePanel()->getRootFolder()->getCurSelectedItem();
	if (!current_item)
	{
		return;
	}

	static_cast<LLFolderViewModelItemInventory*>(current_item->getViewModelItem())->performAction(mPanelMainInventory->getActivePanel()->getModel(), action);
}

void LLSidepanelInventory::onWearButtonClicked()
{
	// Get selected items set.
	const std::set<LLUUID> selected_uuids_set = LLAvatarActions::getInventorySelectedUUIDs();
	if (selected_uuids_set.empty()) return; // nothing selected

	// Convert the set to a vector.
	uuid_vec_t selected_uuids_vec;
	for (std::set<LLUUID>::const_iterator it = selected_uuids_set.begin(); it != selected_uuids_set.end(); ++it)
	{
		selected_uuids_vec.push_back(*it);
	}

	// Wear all selected items.
	wear_multiple(selected_uuids_vec, true);
}

void LLSidepanelInventory::onPlayButtonClicked()
{
	const LLInventoryItem *item = getSelectedItem();
	if (!item)
	{
		return;
	}

	switch(item->getInventoryType())
	{
	case LLInventoryType::IT_GESTURE:
		performActionOnSelection("play");
		break;
	default:
		performActionOnSelection("open");
		break;
	}
}

void LLSidepanelInventory::onTeleportButtonClicked()
{
	performActionOnSelection("teleport");
}

void LLSidepanelInventory::onBackButtonClicked()
{
	showInventoryPanel();
}

void LLSidepanelInventory::onSelectionChange(const std::deque<LLFolderViewItem*> &items, bool user_action)
{
	updateVerbs();
}

void LLSidepanelInventory::showItemInfoPanel()
{
	mItemPanel->setVisible(true);
	if (mTaskPanel)
		mTaskPanel->setVisible(false);
	mInventoryPanel->setVisible(false);

	mItemPanel->dirty();
	mItemPanel->setIsEditing(false);
}

void LLSidepanelInventory::showTaskInfoPanel()
{
	mItemPanel->setVisible(false);
	mInventoryPanel->setVisible(false);

	if (mTaskPanel)
	{
		mTaskPanel->setVisible(true);
		mTaskPanel->dirty();
		mTaskPanel->setIsEditing(false);
	}
}

void LLSidepanelInventory::showInventoryPanel()
{
	mItemPanel->setVisible(false);
	if (mTaskPanel)
		mTaskPanel->setVisible(false);
	mInventoryPanel->setVisible(true);
	updateVerbs();
}

void LLSidepanelInventory::updateVerbs()
{
	mInfoBtn->setEnabled(false);
	mShareBtn->setEnabled(false);

	mWearBtn->setVisible(false);
	mWearBtn->setEnabled(false);
	mPlayBtn->setVisible(false);
	mPlayBtn->setEnabled(false);
	mPlayBtn->setToolTip(std::string(""));
 	mTeleportBtn->setVisible(false);
 	mTeleportBtn->setEnabled(false);

	mShareBtn->setEnabled(canShare());

	const LLInventoryItem *item = getSelectedItem();
	if (!item)
		return;

	bool is_single_selection = getSelectedCount() == 1;

	mInfoBtn->setEnabled(is_single_selection);

	switch(item->getInventoryType())
	{
		case LLInventoryType::IT_WEARABLE:
		case LLInventoryType::IT_OBJECT:
		case LLInventoryType::IT_ATTACHMENT:
			mWearBtn->setVisible(true);
			mWearBtn->setEnabled(canWearSelected());
			break;
		case LLInventoryType::IT_SOUND:
			mPlayBtn->setVisible(true);
			mPlayBtn->setEnabled(true);
			mPlayBtn->setToolTip(LLTrans::getString("InventoryPlaySoundTooltip"));
			break;
		case LLInventoryType::IT_GESTURE:
			mPlayBtn->setVisible(true);
			mPlayBtn->setEnabled(true);
			mPlayBtn->setToolTip(LLTrans::getString("InventoryPlayGestureTooltip"));
			break;
		case LLInventoryType::IT_ANIMATION:
			mPlayBtn->setVisible(true);
			mPlayBtn->setEnabled(true);
			mPlayBtn->setEnabled(true);
			mPlayBtn->setToolTip(LLTrans::getString("InventoryPlayAnimationTooltip"));
			break;
		case LLInventoryType::IT_LANDMARK:
			mTeleportBtn->setVisible(true);
			mTeleportBtn->setEnabled(true);
			break;
		default:
			break;
	}
}

bool LLSidepanelInventory::canShare()
{
	// Avoid flicker in the Recent tab while inventory is being loaded.
	if (mPanelMainInventory && !mPanelMainInventory->getActivePanel()->getRootFolder()->hasVisibleChildren())
	{
		return false;
	}

	return mPanelMainInventory ? LLAvatarActions::canShareSelectedItems(mPanelMainInventory->getActivePanel()) : false;
}


bool LLSidepanelInventory::canWearSelected()
{

	std::set<LLUUID> selected_uuids = LLAvatarActions::getInventorySelectedUUIDs();

	if (selected_uuids.empty())
		return false;

	for (std::set<LLUUID>::const_iterator it = selected_uuids.begin();
		it != selected_uuids.end();
		++it)
	{
		if (!get_can_item_be_worn(*it)) return false;
	}

	return true;
}

LLInventoryItem *LLSidepanelInventory::getSelectedItem()
{
    LLFolderView* root = mPanelMainInventory->getActivePanel()->getRootFolder();
    if (!root)
    {
        return NULL;
    }
	LLFolderViewItem* current_item = root->getCurSelectedItem();

	if (!current_item)
	{
		return nullptr;
	}
	const LLUUID &item_id = static_cast<LLFolderViewModelItemInventory*>(current_item->getViewModelItem())->getUUID();
	LLInventoryItem *item = gInventory.getItem(item_id);
	return item;
}

U32 LLSidepanelInventory::getSelectedCount()
{
	std::set<LLFolderViewItem*> selection_list = mPanelMainInventory->getActivePanel()->getRootFolder()->getSelectionList();
	return selection_list.size();
}

LLInventoryPanel *LLSidepanelInventory::getActivePanel()
{
	if (!getVisible())
	{
		return nullptr;
	}
	if (mInventoryPanel->getVisible())
	{
		return mPanelMainInventory->getActivePanel();
	}
	return nullptr;
}

void LLSidepanelInventory::selectAllItemsPanel()
{
	if (!getVisible())
	{
		return;
	}
	if (mInventoryPanel->getVisible())
	{
		 mPanelMainInventory->selectAllItemsPanel();
	}

}

bool LLSidepanelInventory::isMainInventoryPanelActive() const
{
	return mInventoryPanel->getVisible();
}

void LLSidepanelInventory::cleanup()
{
	LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("inventory");
	for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin(); iter != inst_list.end();)
	{
		LLFloaterSidePanelContainer* iv = dynamic_cast<LLFloaterSidePanelContainer*>(*iter++);
		if (iv)
		{
			iv->cleanup();
		}
	}

	// <FS:Ansariel> Secondary inventory floaters
	LLFloaterReg::const_instance_list_t& secondary_inst_list = LLFloaterReg::getFloaterList("secondary_inventory");
	for (LLFloaterReg::const_instance_list_t::const_iterator iter = secondary_inst_list.begin(); iter != secondary_inst_list.end();)
	{
		LLFloaterSidePanelContainer* iv = dynamic_cast<LLFloaterSidePanelContainer*>(*iter++);
		if (iv)
		{
			iv->cleanup();
		}
	}
	// </FS:Ansariel>
}
