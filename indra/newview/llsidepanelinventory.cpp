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
#include "llappviewer.h"
#include "llavataractions.h"
#include "llbutton.h"
#include "lldate.h"
#include "llfirstuse.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llfoldertype.h"
#include "llfolderview.h"
#include "llinventorybridge.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llinventoryobserver.h"
#include "llinventorypanel.h"
#include "lllayoutstack.h"
#include "lloutfitobserver.h"
#include "llpanelmaininventory.h"
#include "llpanelmarketplaceinbox.h"
#include "llselectmgr.h"
#include "llsidepaneliteminfo.h"
#include "llsidepaneltaskinfo.h"
#include "llstring.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "llviewermedia.h"
#include "llviewernetwork.h"
#include "llweb.h"

#include "llfiltereditor.h"

static LLPanelInjector<LLSidepanelInventory> t_inventory("sidepanel_inventory");

//
// Constants
//

// No longer want the inbox panel to auto-expand since it creates issues with the "new" tag time stamp
#define AUTO_EXPAND_INBOX	0

static const char * const INBOX_BUTTON_NAME = "inbox_btn";
static const char * const INBOX_LAYOUT_PANEL_NAME = "inbox_layout_panel";
static const char * const INVENTORY_LAYOUT_STACK_NAME = "inventory_layout_stack";
static const char * const MARKETPLACE_INBOX_PANEL = "marketplace_inbox";

bool LLSidepanelInventory::sInboxInitalized = false; // <FS:Ansariel> Inbox panel randomly shown on secondary inventory windows

//
// Helpers
//
class LLInboxAddedObserver : public LLInventoryCategoryAddedObserver
{
public:
	LLInboxAddedObserver(LLSidepanelInventory * sidepanelInventory)
		: LLInventoryCategoryAddedObserver()
		, mSidepanelInventory(sidepanelInventory)
	{
	}
	
	void done()
	{
		for (cat_vec_t::iterator it = mAddedCategories.begin(); it != mAddedCategories.end(); ++it)
		{
			LLViewerInventoryCategory* added_category = *it;
			
			LLFolderType::EType added_category_type = added_category->getPreferredType();
			
			switch (added_category_type)
			{
				case LLFolderType::FT_INBOX:
					mSidepanelInventory->enableInbox(true);
					mSidepanelInventory->observeInboxModifications(added_category->getUUID());
					break;
				default:
					break;
			}
		}
	}
	
private:
	LLSidepanelInventory * mSidepanelInventory;
};

//
// Implementation
//

LLSidepanelInventory::LLSidepanelInventory()
	: LLPanel()
	, mItemPanel(NULL)
	, mPanelMainInventory(NULL)
	, mInboxEnabled(false)
	, mCategoriesObserver(NULL)
	, mInboxAddedObserver(NULL)
{
	//buildFromFile( "panel_inventory.xml"); // Called from LLRegisterPanelClass::defaultPanelClassBuilder()
}

LLSidepanelInventory::~LLSidepanelInventory()
{
	// <FS:Ansariel> FIRE-17603: Received Items button sometimes vanishing
	//LLLayoutPanel* inbox_layout_panel = getChild<LLLayoutPanel>(INBOX_LAYOUT_PANEL_NAME);
	LLLayoutPanel* inbox_layout_panel = findChild<LLLayoutPanel>(INBOX_LAYOUT_PANEL_NAME);
	if (inbox_layout_panel)
	{
	// </FS:Ansariel>

	// Save the InventoryMainPanelHeight in settings per account
	gSavedPerAccountSettings.setS32("InventoryInboxHeight", inbox_layout_panel->getTargetDim());
	// <FS:Ansariel> FIRE-17603: Received Items button sometimes vanishing
	}
	// </FS:Ansariel>

	if (mCategoriesObserver && gInventory.containsObserver(mCategoriesObserver))
	{
		gInventory.removeObserver(mCategoriesObserver);
	}
	delete mCategoriesObserver;
	
	if (mInboxAddedObserver && gInventory.containsObserver(mInboxAddedObserver))
	{
		gInventory.removeObserver(mInboxAddedObserver);
	}
	delete mInboxAddedObserver;
}

void handleInventoryDisplayInboxChanged()
{
	LLSidepanelInventory* sidepanel_inventory = LLFloaterSidePanelContainer::getPanel<LLSidepanelInventory>("inventory");
	if (sidepanel_inventory)
	{
		sidepanel_inventory->enableInbox(gSavedSettings.getbool("InventoryDisplayInbox"));
	}
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
		
		mShopBtn = mInventoryPanel->getChild<LLButton>("shop_btn");
		mShopBtn->setClickedCallback(boost::bind(&LLSidepanelInventory::onShopButtonClicked, this));

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
	
	// Received items inbox setup
	if (!sInboxInitalized) // <FS:Ansariel> Inbox panel randomly shown on secondary inventory window
	{
		// <FS:Ansariel> FIRE-17603: Received Items button sometimes vanishing
		//LLLayoutStack* inv_stack = getChild<LLLayoutStack>(INVENTORY_LAYOUT_STACK_NAME);
		LLLayoutStack* inv_stack = findChild<LLLayoutStack>(INVENTORY_LAYOUT_STACK_NAME);
		if (inv_stack)
		{
		// </FS:Ansariel>

		// Set up button states and callbacks
		LLButton * inbox_button = getChild<LLButton>(INBOX_BUTTON_NAME);

		inbox_button->setCommitCallback(boost::bind(&LLSidepanelInventory::onToggleInboxBtn, this));

		// Get the previous inbox state from "InventoryInboxToggleState" setting.
		bool is_inbox_collapsed = !inbox_button->getToggleState();

		// Restore the collapsed inbox panel state
		LLLayoutPanel* inbox_panel = getChild<LLLayoutPanel>(INBOX_LAYOUT_PANEL_NAME);
		inv_stack->collapsePanel(inbox_panel, is_inbox_collapsed);
		if (!is_inbox_collapsed)
		{
			inbox_panel->setTargetDim(gSavedPerAccountSettings.getS32("InventoryInboxHeight"));
		}

		// Set the inbox visible based on debug settings (final setting comes from http request below)
		// <FS:Ansariel> FIRE-17603: Received Items button sometimes vanishing
		//enableInbox(gSavedSettings.getBOOL("InventoryDisplayInbox"));
		enableInbox(!gSavedSettings.getbool("FSShowInboxFolder") || gSavedSettings.getbool("FSAlwaysShowInboxButton"));
		}
		// </FS:Ansariel>

		// Trigger callback for after login so we can setup to track inbox changes after initial inventory load
		LLAppViewer::instance()->setOnLoginCompletedCallback(boost::bind(&LLSidepanelInventory::updateInbox, this));

		// <FS:Ansariel> Optional hiding of Received Items folder aka Inbox
		gSavedSettings.getControl("FSShowInboxFolder")->getSignal()->connect(boost::bind(&LLSidepanelInventory::refreshInboxVisibility, this));
		gSavedSettings.getControl("FSAlwaysShowInboxButton")->getSignal()->connect(boost::bind(&LLSidepanelInventory::refreshInboxVisibility, this));

		sInboxInitalized = true; // <FS:Ansariel> Inbox panel randomly shown on secondary inventory window
	}

	// <FS:Ansariel> Optional hiding of Received Items folder aka Inbox
	//gSavedSettings.getControl("InventoryDisplayInbox")->getCommitSignal()->connect(boost::bind(&handleInventoryDisplayInboxChanged));

	// Update the verbs buttons state.
	updateVerbs();

	return true;
}

void LLSidepanelInventory::updateInbox()
{
	//
	// Track inbox folder changes
	//
	const LLUUID inbox_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_INBOX, true);
	
	// Set up observer to listen for creation of inbox if it doesn't exist
	if (inbox_id.isNull())
	{
		observeInboxCreation();
	}
	// Set up observer for inbox changes, if we have an inbox already
	else 
	{
        // Consolidate Received items
        // We shouldn't have to do that but with a client/server system relying on a "well known folder" convention,
        // things can get messy and conventions broken. This call puts everything back together in its right place.
        gInventory.consolidateForType(inbox_id, LLFolderType::FT_INBOX);
        
		// Enable the display of the inbox if it exists
		enableInbox(true);

		observeInboxModifications(inbox_id);
	}
}

void LLSidepanelInventory::observeInboxCreation()
{
	//
	// Set up observer to track inbox folder creation
	//
	
	if (mInboxAddedObserver == NULL)
	{
		mInboxAddedObserver = new LLInboxAddedObserver(this);
		
		gInventory.addObserver(mInboxAddedObserver);
	}
}

void LLSidepanelInventory::observeInboxModifications(const LLUUID& inboxID)
{
	//
	// Silently do nothing if we already have an inbox inventory panel set up
	// (this can happen multiple times on the initial session that creates the inbox)
	//

	if (mInventoryPanelInbox.get() != NULL)
	{
		return;
	}

	//
	// Track inbox folder changes
	//

	if (inboxID.isNull())
	{
		LL_WARNS() << "Attempting to track modifications to non-existent inbox" << LL_ENDL;
		return;
	}

	if (mCategoriesObserver == NULL)
	{
		mCategoriesObserver = new LLInventoryCategoriesObserver();
		gInventory.addObserver(mCategoriesObserver);
	}

	mCategoriesObserver->addCategory(inboxID, boost::bind(&LLSidepanelInventory::onInboxChanged, this, inboxID));

	//
	// Trigger a load for the entire contents of the Inbox
	//

	LLInventoryModelBackgroundFetch::instance().start(inboxID);

	//
	// Set up the inbox inventory view
	//

	LLPanelMarketplaceInbox * inbox = getChild<LLPanelMarketplaceInbox>(MARKETPLACE_INBOX_PANEL);
    LLInventoryPanel* inventory_panel = inbox->setupInventoryPanel();
	mInventoryPanelInbox = inventory_panel->getInventoryPanelHandle();
}

void LLSidepanelInventory::enableInbox(bool enabled)
{
	mInboxEnabled = enabled;
	
	LLLayoutPanel * inbox_layout_panel = getChild<LLLayoutPanel>(INBOX_LAYOUT_PANEL_NAME);
	// <FS:Ansariel> Optional hiding of Received Items folder aka Inbox
	//inbox_layout_panel->setVisible(enabled);
	inbox_layout_panel->setVisible(enabled && (!gSavedSettings.getbool("FSShowInboxFolder") || gSavedSettings.getbool("FSAlwaysShowInboxButton")));
}

// <FS:Ansariel> Optional hiding of Received Items folder aka Inbox
void LLSidepanelInventory::refreshInboxVisibility()
{
	enableInbox(mInboxEnabled);
}
// </FS:Ansariel> Optional hiding of Received Items folder aka Inbox

void LLSidepanelInventory::openInbox()
{
	if (mInboxEnabled)
	{
		getChild<LLButton>(INBOX_BUTTON_NAME)->setToggleState(true);
		onToggleInboxBtn();
	}
}

void LLSidepanelInventory::onInboxChanged(const LLUUID& inbox_id)
{
	// Trigger a load of the entire inbox so we always know the contents and their creation dates for sorting
	LLInventoryModelBackgroundFetch::instance().start(inbox_id);

#if AUTO_EXPAND_INBOX
	// Expand the inbox since we have fresh items
	if (mInboxEnabled)
	{
		getChild<LLButton>(INBOX_BUTTON_NAME)->setToggleState(true);
		onToggleInboxBtn();
	}
#endif
}

void LLSidepanelInventory::onToggleInboxBtn()
{
	LLButton* inboxButton = getChild<LLButton>(INBOX_BUTTON_NAME);
	LLLayoutPanel* inboxPanel = getChild<LLLayoutPanel>(INBOX_LAYOUT_PANEL_NAME);
	LLLayoutStack* inv_stack = getChild<LLLayoutStack>(INVENTORY_LAYOUT_STACK_NAME);
	
	const bool inbox_expanded = inboxButton->getToggleState();
	
	// Expand/collapse the indicated panel
	inv_stack->collapsePanel(inboxPanel, !inbox_expanded);

	if (inbox_expanded)
	{
		inboxPanel->setTargetDim(gSavedPerAccountSettings.getS32("InventoryInboxHeight"));
		if (inboxPanel->isInVisibleChain())
	{
		gSavedPerAccountSettings.setU32("LastInventoryInboxActivity", time_corrected());
	}
}
	else
	{
		gSavedPerAccountSettings.setS32("InventoryInboxHeight", inboxPanel->getTargetDim());
	}

}

void LLSidepanelInventory::onOpen(const LLSD& key)
{
	LLFirstUse::newInventory(false);
	mPanelMainInventory->setFocusFilterEditor();
#if AUTO_EXPAND_INBOX
	// Expand the inbox if we have fresh items
	LLPanelMarketplaceInbox * inbox = findChild<LLPanelMarketplaceInbox>(MARKETPLACE_INBOX_PANEL);
	if (inbox && (inbox->getFreshItemCount() > 0))
	{
		getChild<LLButton>(INBOX_BUTTON_NAME)->setToggleState(true);
		onToggleInboxBtn();
	}
#else
	if (mInboxEnabled && getChild<LLButton>(INBOX_BUTTON_NAME)->getToggleState())
	{
		gSavedPerAccountSettings.setU32("LastInventoryInboxActivity", time_corrected());
	}
#endif

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

void LLSidepanelInventory::onShopButtonClicked()
{
	LLWeb::loadURL(gSavedSettings.getString("MarketplaceURL"));
}

void LLSidepanelInventory::performActionOnSelection(const std::string &action)
{
	LLFolderViewItem* current_item = mPanelMainInventory->getActivePanel()->getRootFolder()->getCurSelectedItem();
	if (!current_item)
	{
		if (mInventoryPanelInbox.get() && mInventoryPanelInbox.get()->getRootFolder())
		{
			current_item = mInventoryPanelInbox.get()->getRootFolder()->getCurSelectedItem();
		}

		if (!current_item)
		{
			return;
		}
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
 	mShopBtn->setVisible(true);

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
		 	mShopBtn->setVisible(false);
			break;
		case LLInventoryType::IT_SOUND:
			mPlayBtn->setVisible(true);
			mPlayBtn->setEnabled(true);
			mPlayBtn->setToolTip(LLTrans::getString("InventoryPlaySoundTooltip"));
			mShopBtn->setVisible(false);
			break;
		case LLInventoryType::IT_GESTURE:
			mPlayBtn->setVisible(true);
			mPlayBtn->setEnabled(true);
			mPlayBtn->setToolTip(LLTrans::getString("InventoryPlayGestureTooltip"));
			mShopBtn->setVisible(false);
			break;
		case LLInventoryType::IT_ANIMATION:
			mPlayBtn->setVisible(true);
			mPlayBtn->setEnabled(true);
			mPlayBtn->setEnabled(true);
			mPlayBtn->setToolTip(LLTrans::getString("InventoryPlayAnimationTooltip"));
			mShopBtn->setVisible(false);
			break;
		case LLInventoryType::IT_LANDMARK:
			mTeleportBtn->setVisible(true);
			mTeleportBtn->setEnabled(true);
		 	mShopBtn->setVisible(false);
			break;
		default:
			break;
	}
}

bool LLSidepanelInventory::canShare()
{
	LLInventoryPanel* inbox = mInventoryPanelInbox.get();

	// Avoid flicker in the Recent tab while inventory is being loaded.
	if ( (!inbox || !inbox->getRootFolder() || inbox->getRootFolder()->getSelectionList().empty())
		&& (mPanelMainInventory && !mPanelMainInventory->getActivePanel()->getRootFolder()->hasVisibleChildren()) )
	{
		return false;
	}

	return ( (mPanelMainInventory ? LLAvatarActions::canShareSelectedItems(mPanelMainInventory->getActivePanel()) : false)
			|| (inbox ? LLAvatarActions::canShareSelectedItems(inbox) : false) );
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
		if (mInventoryPanelInbox.get() && mInventoryPanelInbox.get()->getRootFolder())
		{
			current_item = mInventoryPanelInbox.get()->getRootFolder()->getCurSelectedItem();
		}

		if (!current_item)
		{
			return NULL;
		}
	}
	const LLUUID &item_id = static_cast<LLFolderViewModelItemInventory*>(current_item->getViewModelItem())->getUUID();
	LLInventoryItem *item = gInventory.getItem(item_id);
	return item;
}

U32 LLSidepanelInventory::getSelectedCount()
{
	int count = 0;

	std::set<LLFolderViewItem*> selection_list = mPanelMainInventory->getActivePanel()->getRootFolder()->getSelectionList();
	count += selection_list.size();

	if ((count == 0) && mInboxEnabled && mInventoryPanelInbox.get() && mInventoryPanelInbox.get()->getRootFolder())
	{
		selection_list = mInventoryPanelInbox.get()->getRootFolder()->getSelectionList();

		count += selection_list.size();
	}

	return count;
}

LLInventoryPanel *LLSidepanelInventory::getActivePanel()
{
	if (!getVisible())
	{
		return NULL;
	}
	if (mInventoryPanel->getVisible())
	{
		return mPanelMainInventory->getActivePanel();
	}
	return NULL;
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

void LLSidepanelInventory::clearSelections(bool clearMain, bool clearInbox)
{
	if (clearMain)
	{
		LLInventoryPanel * inv_panel = getActivePanel();
		
		if (inv_panel)
		{
			inv_panel->getRootFolder()->clearSelection();
		}
	}
	
	if (clearInbox && mInboxEnabled && mInventoryPanelInbox.get())
	{
		mInventoryPanelInbox.get()->getRootFolder()->clearSelection();
	}
	
	updateVerbs();
}

std::set<LLFolderViewItem*> LLSidepanelInventory::getInboxSelectionList()
{
	std::set<LLFolderViewItem*> inventory_selected_uuids;
	
	if (mInboxEnabled && mInventoryPanelInbox.get() && mInventoryPanelInbox.get()->getRootFolder())
	{
		inventory_selected_uuids = mInventoryPanelInbox.get()->getRootFolder()->getSelectionList();
	}
	
	return inventory_selected_uuids;
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
