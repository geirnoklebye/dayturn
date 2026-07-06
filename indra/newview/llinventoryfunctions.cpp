/** 
 * @file llinventoryfunctions.cpp
 * @brief Implementation of the inventory view and associated stuff.
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

#include <utility> // for std::pair<>

#include "llinventoryfunctions.h"

// library includes
#include "llagent.h"
#include "llagentwearables.h"
#include "llcallingcard.h"
#include "llfloaterreg.h"
#include "llinventorydefines.h"
#include "llsdserialize.h"
#include "llfiltereditor.h"
#include "llspinctrl.h"
#include "llui.h"
#include "message.h"

// newview includes
#include "llappearancemgr.h"
#include "llappviewer.h"
#include "llavataractions.h"
#include "llclipboard.h"
#include "lldirpicker.h"
#include "lldonotdisturbnotificationstorage.h"
#include "llfloatersidepanelcontainer.h"
#include "llfocusmgr.h"
#include "llfolderview.h"
#include "llgesturemgr.h"
#include "lliconctrl.h"
#include "llimview.h"
#include "llinventorybridge.h"
#include "llinventorymodel.h"
#include "llinventorypanel.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "llpanelmaininventory.h"
#include "llpreviewanim.h"
#include "llpreviewgesture.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llpreviewsound.h"
#include "llpreviewtexture.h"
#include "llresmgr.h"
#include "llscrollbar.h"
#include "llscrollcontainer.h"
#include "llselectmgr.h"
#include "llsidepanelinventory.h"
#include "lltabcontainer.h"
#include "lltooldraganddrop.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llviewermenu.h"
#include "llviewermessage.h"
#include "llviewerfoldertype.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llwearablelist.h"

#include "aoengine.h"			// ## Zi: Animation Overrider

bool LLInventoryState::sWearNewClothing = false;
LLUUID LLInventoryState::sWearNewClothingTransactionID;

// Helper function : callback to update a folder after inventory action happened in the background
void update_folder_cb(const LLUUID& dest_folder)
{
    LLViewerInventoryCategory* dest_cat = gInventory.getCategory(dest_folder);
    gInventory.updateCategory(dest_cat);
    gInventory.notifyObservers();
}

// Generates a string containing the path to the item specified by
// item_id.
void append_path(const LLUUID& id, std::string& path)
{
	std::string temp;
	const LLInventoryObject* obj = gInventory.getObject(id);
	LLUUID parent_id;
	if(obj) parent_id = obj->getParentUUID();
	std::string forward_slash("/");
	while(obj)
	{
		obj = gInventory.getCategory(parent_id);
		if(obj)
		{
			temp.assign(forward_slash + obj->getName() + temp);
			parent_id = obj->getParentUUID();
		}
	}
	path.append(temp);
}

void rename_category(LLInventoryModel* model, const LLUUID& cat_id, const std::string& new_name)
{
	LLViewerInventoryCategory* cat;

	if (!model ||
		!get_is_category_renameable(model, cat_id) ||
		(cat = model->getCategory(cat_id)) == nullptr ||
		cat->getName() == new_name)
	{
		return;
	}

	LLSD updates;
	updates["name"] = new_name;
	update_inventory_category(cat_id, updates, NULL);
}

void copy_inventory_category(LLInventoryModel* model,
							 LLViewerInventoryCategory* cat,
							 const LLUUID& parent_id,
							 const LLUUID& root_copy_id,
							 bool move_no_copy_items )
{
	// Create the initial folder
	inventory_func_type func = boost::bind(&copy_inventory_category_content, _1, model, cat, root_copy_id, move_no_copy_items);
	gInventory.createNewCategory(parent_id, LLFolderType::FT_NONE, cat->getName(), func);
}

void copy_inventory_category_content(const LLUUID& new_cat_uuid, LLInventoryModel* model, LLViewerInventoryCategory* cat, const LLUUID& root_copy_id, bool move_no_copy_items)
{
	model->notifyObservers();

	// We need to exclude the initial root of the copy to avoid recursively copying the copy, etc...
	LLUUID root_id = (root_copy_id.isNull() ? new_cat_uuid : root_copy_id);

	// Get the content of the folder
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat->getUUID(), cat_array, item_array);

	// Copy all the items
	LLInventoryModel::item_array_t item_array_copy = *item_array;
	for (LLInventoryModel::item_array_t::iterator iter = item_array_copy.begin(); iter != item_array_copy.end(); iter++)
	{
		LLInventoryItem* item = *iter;
		LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback(boost::bind(update_folder_cb, new_cat_uuid));

		if (item->getIsLinkType())
		{
			link_inventory_object(new_cat_uuid, item->getLinkedUUID(), cb);
		}
		else if (!item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()))
		{
			// If the item is nocopy, we do nothing or, optionally, move it
			if (move_no_copy_items)
			{
				// Reparent the item
				LLViewerInventoryItem * viewer_inv_item = (LLViewerInventoryItem *)item;
				gInventory.changeItemParent(viewer_inv_item, new_cat_uuid, true);
			}
		}
		else
		{
			copy_inventory_item(
				gAgent.getID(),
				item->getPermissions().getOwner(),
				item->getUUID(),
				new_cat_uuid,
				std::string(),
				cb);
		}
	}

	// Copy all the folders
	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(); iter != cat_array_copy.end(); iter++)
	{
		LLViewerInventoryCategory* category = *iter;
		if (category->getUUID() != root_id)
		{
			copy_inventory_category(model, category, new_cat_uuid, root_id, move_no_copy_items);
		}
	}
}

class LLInventoryCollectAllItems : public LLInventoryCollectFunctor
{
public:
	virtual bool operator()(LLInventoryCategory* cat, LLInventoryItem* item)
	{
		return true;
	}
};

bool get_is_parent_to_worn_item(const LLUUID& id)
{
	const LLViewerInventoryCategory* cat = gInventory.getCategory(id);
	if (!cat)
	{
		return false;
	}

	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	LLInventoryCollectAllItems collect_all;
	gInventory.collectDescendentsIf(LLAppearanceMgr::instance().getCOF(), cats, items, LLInventoryModel::EXCLUDE_TRASH, collect_all);

	for (LLInventoryModel::item_array_t::const_iterator it = items.begin(); it != items.end(); ++it)
	{
		const LLViewerInventoryItem * const item = *it;

		llassert(item->getIsLinkType());

		LLUUID linked_id = item->getLinkedUUID();
		const LLViewerInventoryItem * const linked_item = gInventory.getItem(linked_id);

		if (linked_item)
		{
			LLUUID parent_id = linked_item->getParentUUID();

			while (!parent_id.isNull())
			{
				LLInventoryCategory * parent_cat = gInventory.getCategory(parent_id);

				if (cat == parent_cat)
				{
					return true;
				}

				parent_id = parent_cat->getParentUUID();
			}
		}
	}

	return false;
}

bool get_is_item_worn(const LLUUID& id)
{
	const LLViewerInventoryItem* item = gInventory.getItem(id);
	if (!item)
		return false;
    
    if (item->getIsLinkType() && !gInventory.getItem(item->getLinkedUUID()))
    {
        return false;
    }

	// Consider the item as worn if it has links in COF.
	if (LLAppearanceMgr::instance().isLinkedInCOF(id))
	{
		return true;
	}

	switch(item->getType())
	{
		case LLAssetType::AT_OBJECT:
		{
			if (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(item->getLinkedUUID()))
				return true;
			break;
		}
		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
			if(gAgentWearables.isWearingItem(item->getLinkedUUID()))
				return true;
			break;
		case LLAssetType::AT_GESTURE:
			if (LLGestureMgr::instance().isGestureActive(item->getLinkedUUID()))
				return true;
			break;
		default:
			break;
	}
	return false;
}

bool get_can_item_be_worn(const LLUUID& id)
{
	const LLViewerInventoryItem* item = gInventory.getItem(id);
	if (!item)
		return false;

	if (LLAppearanceMgr::instance().isLinkedInCOF(item->getLinkedUUID()))
	{
		// an item having links in COF (i.e. a worn item)
		return false;
	}

	if (gInventory.isObjectDescendentOf(id, LLAppearanceMgr::instance().getCOF()))
	{
		// a non-link object in COF (should not normally happen)
		return false;
	}
	
	const LLUUID trash_id = gInventory.findCategoryUUIDForType(
			LLFolderType::FT_TRASH);

	// item can't be worn if base obj in trash, see EXT-7015
	if (gInventory.isObjectDescendentOf(item->getLinkedUUID(),
			trash_id))
	{
		return false;
	}

	switch(item->getType())
	{
		case LLAssetType::AT_OBJECT:
		{
			if (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(item->getLinkedUUID()))
			{
				// Already being worn
				return false;
			}
			else
			{
				// Not being worn yet.
				return true;
			}
			break;
		}
		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
			if(gAgentWearables.isWearingItem(item->getLinkedUUID()))
			{
				// Already being worn
				return false;
			}
			else
			{
				// Not being worn yet.
				return true;
			}
			break;
		default:
			break;
	}
	return false;
}

bool get_is_item_removable(const LLInventoryModel* model, const LLUUID& id)
{
	if (!model)
	{
		return false;
	}

	// Can't delete an item that's in the library.
	if (!model->isObjectDescendentOf(id, gInventory.getRootFolderID()))
	{
		return false;
	}

	// ## Zi: Animation Overrider
	if(model->isObjectDescendentOf(id,AOEngine::instance().getAOFolder())
		&& gSavedPerAccountSettings.getbool("ProtectAOFolders"))
    {
		return false;
	}
	// ## Zi: Animation Overrider

	// Disable delete from COF folder; have users explicitly choose "detach/take off",
	// unless the item is not worn but in the COF (i.e. is bugged).
	if (LLAppearanceMgr::instance().getIsProtectedCOFItem(id))
	{
		if (get_is_item_worn(id))
		{
			return false;
		}
	}

	const LLInventoryObject *obj = model->getItem(id);
	if (obj && obj->getIsLinkType())
	{
		return true;
	}
	if (get_is_item_worn(id))
	{
		return false;
	}
	return true;
}

bool get_is_item_editable(const LLUUID& inv_item_id)
{
	if (const LLInventoryItem* inv_item = gInventory.getLinkedItem(inv_item_id))
	{
		switch (inv_item->getType())
		{
			case LLAssetType::AT_BODYPART:
			case LLAssetType::AT_CLOTHING:
				return gAgentWearables.isWearableModifiable(inv_item_id);
			case LLAssetType::AT_OBJECT:
				return true;
			default:
                return false;;
		}
	}
	return gAgentAvatarp->getWornAttachment(inv_item_id) != nullptr;
}

void handle_item_edit(const LLUUID& inv_item_id)
{
	if (get_is_item_editable(inv_item_id))
	{
		if (const LLInventoryItem* inv_item = gInventory.getLinkedItem(inv_item_id))
		{
			switch (inv_item->getType())
			{
				case LLAssetType::AT_BODYPART:
				case LLAssetType::AT_CLOTHING:
					LLAgentWearables::editWearable(inv_item_id);
					break;
				case LLAssetType::AT_OBJECT:
					handle_attachment_edit(inv_item_id);
					break;
				default:
					break;
			}
		}
		else
		{
			handle_attachment_edit(inv_item_id);
		}
	}
}

bool get_is_category_removable(const LLInventoryModel* model, const LLUUID& id)
{
	// NOTE: This function doesn't check the folder's children.
	// See LLFolderBridge::isItemRemovable for a function that does
	// consider the children.

	if (!model)
	{
		return false;
	}

	if (!model->isObjectDescendentOf(id, gInventory.getRootFolderID()))
	{
		return false;
	}

	// ## Zi: Animation Overrider
	if((id==AOEngine::instance().getAOFolder() || model->isObjectDescendentOf(id,AOEngine::instance().getAOFolder()))
		&& gSavedPerAccountSettings.getbool("ProtectAOFolders"))
	{
		return false;
	}
	// ## Zi: Animation Overrider

	if (!isAgentAvatarValid()) return false;

	const LLInventoryCategory* category = model->getCategory(id);
	if (!category)
	{
		return false;
	}

	const LLFolderType::EType folder_type = category->getPreferredType();
	
	if (LLFolderType::lookupIsProtectedType(folder_type))
	{
		return false;
	}

	// Can't delete the outfit that is currently being worn.
	if (folder_type == LLFolderType::FT_OUTFIT)
	{
		const LLViewerInventoryItem *base_outfit_link = LLAppearanceMgr::instance().getBaseOutfitLink();
		if (base_outfit_link && (category == base_outfit_link->getLinkedCategory()))
		{
			return false;
		}
	}

	return true;
}

bool get_is_category_renameable(const LLInventoryModel* model, const LLUUID& id)
{
	if (!model)
	{
		return false;
	}
	// ## Zi: Animation Overrider
	if((id==AOEngine::instance().getAOFolder() || model->isObjectDescendentOf(id,AOEngine::instance().getAOFolder()))
		&& gSavedPerAccountSettings.getbool("ProtectAOFolders"))
	{
		return false;
	}
	// ## Zi: Animation Overrider

	LLViewerInventoryCategory* cat = model->getCategory(id);

	if (cat && !LLFolderType::lookupIsProtectedType(cat->getPreferredType()) &&
		cat->getOwnerID() == gAgent.getID())
	{
		return true;
	}

	return false;
}

void show_task_item_profile(const LLUUID& item_uuid, const LLUUID& object_id)
{
    LLSD params;
    params["id"] = item_uuid;
    params["object"] = object_id;
    
    LLFloaterReg::showInstance("item_properties", params);
}

void show_item_profile(const LLUUID& item_uuid)
{
	LLUUID linked_uuid = gInventory.getLinkedItemID(item_uuid);
	LLFloaterSidePanelContainer::showPanel("inventory", LLSD().with("id", linked_uuid));
}

void show_item_original(const LLUUID& item_uuid)
{
    LLFloater* floater_inventory = LLFloaterReg::getInstance("inventory");
    if (!floater_inventory)
    {
        LL_WARNS() << "Could not find My Inventory floater" << LL_ENDL;
        return;
    }
    LLSidepanelInventory *sidepanel_inventory =	LLFloaterSidePanelContainer::getPanel<LLSidepanelInventory>("inventory");
    if (sidepanel_inventory)
    {
        LLPanelMainInventory* main_inventory = sidepanel_inventory->getMainInventoryPanel();
        if (main_inventory)
        {
            main_inventory->resetAllItemsFilters();
        }
        reset_inventory_filter();

        if (!LLFloaterReg::getTypedInstance<LLFloaterSidePanelContainer>("inventory")->isInVisibleChain())
        {
            LLFloaterReg::toggleInstanceOrBringToFront("inventory");
        }
        sidepanel_inventory->showInventoryPanel();

        sidepanel_inventory->selectAllItemsPanel();
        if (sidepanel_inventory->getActivePanel())
        {
            sidepanel_inventory->getActivePanel()->setSelection(gInventory.getLinkedItemID(item_uuid), TAKE_FOCUS_YES);
        }
    }
}


void reset_inventory_filter()
{
	LLSidepanelInventory *sidepanel_inventory =	LLFloaterSidePanelContainer::getPanel<LLSidepanelInventory>("inventory");
	if (sidepanel_inventory)
	{
		LLPanelMainInventory* main_inventory = sidepanel_inventory->getMainInventoryPanel();
		if (main_inventory)
		{
			main_inventory->onFilterEdit("");
		}
	}
}

// Create a new folder in destFolderId with the same name as the item name and return the uuid of the new folder
// Note: this is used locally in various situation where we need to wrap an item into a special folder
LLUUID create_folder_for_item(LLInventoryItem* item, const LLUUID& destFolderId)
{
	llassert(item);
	llassert(destFolderId.notNull());

	LLUUID created_folder_id = gInventory.createNewCategory(destFolderId, LLFolderType::FT_NONE, item->getName());
	gInventory.notifyObservers();
    
    // *TODO : Create different notifications for the various cases
	LLNotificationsUtil::add("OutboxFolderCreated");

	return created_folder_id;
}

///----------------------------------------------------------------------------
// Marketplace listings folder-depth helpers
//
// Still referenced by other inventory code (e.g. llinventorybridge.cpp,
// llinventoryfilter.cpp) to reason about position relative to the
// marketplace listings root folder.
///----------------------------------------------------------------------------

S32 depth_nesting_in_marketplace(LLUUID cur_uuid)
{
    // Get the marketplace listings root, exit with -1 (i.e. not under the marketplace listings root) if none
    // Todo: findCategoryUUIDForType is somewhat expensive with large
    // flat root folders yet we use depth_nesting_in_marketplace at
    // every turn, find a way to correctly cache this id.
    const LLUUID marketplace_listings_uuid = gInventory.findCategoryUUIDForType(LLFolderType::FT_MARKETPLACE_LISTINGS, false);
    if (marketplace_listings_uuid.isNull())
    {
        return -1;
    }
    // If not a descendant of the marketplace listings root, then the nesting depth is -1 by definition
    if (!gInventory.isObjectDescendentOf(cur_uuid, marketplace_listings_uuid))
    {
        return -1;
    }
    
    // Iterate through the parents till we hit the marketplace listings root
    // Note that the marketplace listings root itself will return 0
    S32 depth = 0;
    LLInventoryObject* cur_object = gInventory.getObject(cur_uuid);
    while (cur_uuid != marketplace_listings_uuid)
    {
        depth++;
        cur_uuid = cur_object->getParentUUID();
        cur_object = gInventory.getCategory(cur_uuid);
    }
    return depth;
}

// Returns the UUID of the marketplace listing this object is in
LLUUID nested_parent_id(LLUUID cur_uuid, S32 depth)
{
    if (depth < 1)
    {
        // For objects outside the marketplace listings root (or root itself), we return a NULL UUID
        return LLUUID::null;
    }
    else if (depth == 1)
    {
        // Just under the root, we return the passed UUID itself if it's a folder, NULL otherwise (not a listing)
        LLViewerInventoryCategory* cat = gInventory.getCategory(cur_uuid);
        return (cat ? cur_uuid : LLUUID::null);
    }

    // depth > 1
    LLInventoryObject* cur_object = gInventory.getObject(cur_uuid);
    while (depth > 1)
    {
        depth--;
        cur_uuid = cur_object->getParentUUID();
        cur_object = gInventory.getCategory(cur_uuid);
    }
    return cur_uuid;
}

void change_item_parent(const LLUUID& item_id, const LLUUID& new_parent_id)
{
	LLInventoryItem* inv_item = gInventory.getItem(item_id);
	if (inv_item)
	{
		LLInventoryModel::update_list_t update;
		LLInventoryModel::LLCategoryUpdate old_folder(inv_item->getParentUUID(), -1);
		update.push_back(old_folder);
		LLInventoryModel::LLCategoryUpdate new_folder(new_parent_id, 1);
		update.push_back(new_folder);
		gInventory.accountForUpdate(update);

		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(inv_item);
		new_item->setParent(new_parent_id);
		new_item->updateParentOnServer(false);
		gInventory.updateItem(new_item);
		gInventory.notifyObservers();
	}
}

void move_items_to_folder(const LLUUID& new_cat_uuid, const uuid_vec_t& selected_uuids)
{
    for (uuid_vec_t::const_iterator it = selected_uuids.begin(); it != selected_uuids.end(); ++it)
    {
        LLInventoryItem* inv_item = gInventory.getItem(*it);
        if (inv_item)
        {
            change_item_parent(*it, new_cat_uuid);
        }
        else
        {
            LLInventoryCategory* inv_cat = gInventory.getCategory(*it);
            if (inv_cat && !LLFolderType::lookupIsProtectedType(inv_cat->getPreferredType()))
            {
                gInventory.changeCategoryParent((LLViewerInventoryCategory*)inv_cat, new_cat_uuid, false);
            }
        }
    }

    LLFloater* floater_inventory = LLFloaterReg::getInstance("inventory");
    if (!floater_inventory)
    {
        LL_WARNS() << "Could not find My Inventory floater" << LL_ENDL;
        return;
    }
    LLSidepanelInventory *sidepanel_inventory =	LLFloaterSidePanelContainer::getPanel<LLSidepanelInventory>("inventory");
    if (sidepanel_inventory)
    {
        if (sidepanel_inventory->getActivePanel())
        {
            sidepanel_inventory->getActivePanel()->setSelection(new_cat_uuid, TAKE_FOCUS_YES);
            LLFolderViewItem* fv_folder = sidepanel_inventory->getActivePanel()->getItemByID(new_cat_uuid);
            if (fv_folder)
            {
                fv_folder->setOpen(true);
            }
        }
    }
}

bool is_only_cats_selected(const uuid_vec_t& selected_uuids)
{
    for (uuid_vec_t::const_iterator it = selected_uuids.begin(); it != selected_uuids.end(); ++it)
    {
        LLInventoryCategory* inv_cat = gInventory.getCategory(*it);
        if (!inv_cat)
        {
            return false;
        }
    }
    return true;
}

bool is_only_items_selected(const uuid_vec_t& selected_uuids)
{
    for (uuid_vec_t::const_iterator it = selected_uuids.begin(); it != selected_uuids.end(); ++it)
    {
        LLViewerInventoryItem* inv_item = gInventory.getItem(*it);
        if (!inv_item)
        {
            return false;
        }
    }
    return true;
}


void move_items_to_new_subfolder(const uuid_vec_t& selected_uuids, const std::string& folder_name)
{
    LLInventoryObject* first_item = gInventory.getObject(*selected_uuids.begin());
    if (!first_item)
    {
        return;
    }

    inventory_func_type func = boost::bind(&move_items_to_folder, _1, selected_uuids);
    gInventory.createNewCategory(first_item->getParentUUID(), LLFolderType::FT_NONE, folder_name, func);

}

std::string get_category_path(LLUUID cat_id)
{
    LLViewerInventoryCategory *cat = gInventory.getCategory(cat_id);
    std::string localized_cat_name;
    if (!LLTrans::findString(localized_cat_name, "InvFolder " + cat->getName()))
    {
        localized_cat_name = cat->getName();
    }

    if (cat->getParentUUID().notNull())
    {
        return get_category_path(cat->getParentUUID()) + " > " + localized_cat_name;
    }
    else
    {
        return localized_cat_name;
    }
}
///----------------------------------------------------------------------------
/// LLInventoryCollectFunctor implementations
///----------------------------------------------------------------------------

// static
bool LLInventoryCollectFunctor::itemTransferCommonlyAllowed(const LLInventoryItem* item)
{
	if (!item)
		return false;

	switch(item->getType())
	{
		case LLAssetType::AT_OBJECT:
		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
			if (!get_is_item_worn(item->getUUID()))
				return true;
			break;
		default:
			return true;
			break;
	}
	return false;
}

bool LLIsType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) return true;
	}
	if(item)
	{
		if(item->getType() == mType) return true;
	}
	return false;
}

bool LLIsNotType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) return false;
	}
	if(item)
	{
		if(item->getType() == mType) return false;
		else return true;
	}
	return true;
}

bool LLIsOfAssetType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) return true;
	}
	if(item)
	{
		if(item->getActualType() == mType) return true;
	}
	return false;
}

bool LLIsValidItemLink::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	LLViewerInventoryItem *vitem = dynamic_cast<LLViewerInventoryItem*>(item);
	if (!vitem) return false;
	return (vitem->getActualType() == LLAssetType::AT_LINK  && !vitem->getIsBrokenLink());
}

bool LLIsTypeWithPermissions::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) 
		{
			return true;
		}
	}
	if(item)
	{
		if(item->getType() == mType)
		{
			LLPermissions perm = item->getPermissions();
			if ((perm.getMaskBase() & mPerm) == mPerm)
			{
				return true;
			}
		}
	}
	return false;
}

bool LLBuddyCollector::operator()(LLInventoryCategory* cat,
								  LLInventoryItem* item)
{
	if(item)
	{
		if((LLAssetType::AT_CALLINGCARD == item->getType())
		   && (!item->getCreatorUUID().isNull())
		   && (item->getCreatorUUID() != gAgent.getID()))
		{
			return true;
		}
	}
	return false;
}


bool LLUniqueBuddyCollector::operator()(LLInventoryCategory* cat,
										LLInventoryItem* item)
{
	if(item)
	{
		if((LLAssetType::AT_CALLINGCARD == item->getType())
 		   && (item->getCreatorUUID().notNull())
 		   && (item->getCreatorUUID() != gAgent.getID()))
		{
			mSeen.insert(item->getCreatorUUID());
			return true;
		}
	}
	return false;
}


bool LLParticularBuddyCollector::operator()(LLInventoryCategory* cat,
											LLInventoryItem* item)
{
	if(item)
	{
		if((LLAssetType::AT_CALLINGCARD == item->getType())
		   && (item->getCreatorUUID() == mBuddyID))
		{
			return true;
		}
	}
	return false;
}


bool LLNameCategoryCollector::operator()(
	LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(cat)
	{
		if (!LLStringUtil::compareInsensitive(mName, cat->getName()))
		{
			return true;
		}
	}
	return false;
}

bool LLFindCOFValidItems::operator()(LLInventoryCategory* cat,
									 LLInventoryItem* item)
{
	// Valid COF items are:
	// - links to wearables (body parts or clothing)
	// - links to attachments
	// - links to gestures
	// - links to ensemble folders
	LLViewerInventoryItem *linked_item = ((LLViewerInventoryItem*)item)->getLinkedItem();
	if (linked_item)
	{
		LLAssetType::EType type = linked_item->getType();
		return (type == LLAssetType::AT_CLOTHING ||
				type == LLAssetType::AT_BODYPART ||
				type == LLAssetType::AT_GESTURE ||
				type == LLAssetType::AT_OBJECT);
	}
	else
	{
		LLViewerInventoryCategory *linked_category = ((LLViewerInventoryItem*)item)->getLinkedCategory();
		// BAP remove AT_NONE support after ensembles are fully working?
		return (linked_category &&
				((linked_category->getPreferredType() == LLFolderType::FT_NONE) ||
				 (LLFolderType::lookupIsEnsembleType(linked_category->getPreferredType()))));
	}
}

bool LLFindWearables::operator()(LLInventoryCategory* cat,
								 LLInventoryItem* item)
{
	if(item)
	{
		if((item->getType() == LLAssetType::AT_CLOTHING)
		   || (item->getType() == LLAssetType::AT_BODYPART))
		{
			return true;
		}
	}
	return false;
}

LLFindWearablesEx::LLFindWearablesEx(bool is_worn, bool include_body_parts)
:	mIsWorn(is_worn)
,	mIncludeBodyParts(include_body_parts)
{}

bool LLFindWearablesEx::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	LLViewerInventoryItem *vitem = dynamic_cast<LLViewerInventoryItem*>(item);
	if (!vitem) return false;

	// Skip non-wearables.
	if (!vitem->isWearableType() && vitem->getType() != LLAssetType::AT_OBJECT && vitem->getType() != LLAssetType::AT_GESTURE)
	{
		return false;
	}

	// Skip body parts if requested.
	if (!mIncludeBodyParts && vitem->getType() == LLAssetType::AT_BODYPART)
	{
		return false;
	}

	// Skip broken links.
	if (vitem->getIsBrokenLink())
	{
		return false;
	}

	return (bool) get_is_item_worn(item->getUUID()) == mIsWorn;
}

bool LLFindWearablesOfType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if (!item) return false;
	if (item->getType() != LLAssetType::AT_CLOTHING &&
		item->getType() != LLAssetType::AT_BODYPART)
	{
		return false;
	}

	LLViewerInventoryItem *vitem = dynamic_cast<LLViewerInventoryItem*>(item);
	if (!vitem || vitem->getWearableType() != mWearableType) return false;

	return true;
}

void LLFindWearablesOfType::setType(LLWearableType::EType type)
{
	mWearableType = type;
}

bool LLFindNonRemovableObjects::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if (item)
	{
		return !get_is_item_removable(&gInventory, item->getUUID());
	}
	if (cat)
	{
		return !get_is_category_removable(&gInventory, cat->getUUID());
	}

	LL_WARNS() << "Not a category and not an item?" << LL_ENDL;
	return false;
}

///----------------------------------------------------------------------------
/// LLAssetIDMatches 
///----------------------------------------------------------------------------
bool LLAssetIDMatches::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	return (item && item->getAssetUUID() == mAssetID);
}

///----------------------------------------------------------------------------
/// LLLinkedItemIDMatches 
///----------------------------------------------------------------------------
bool LLLinkedItemIDMatches::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	return (item && 
			(item->getIsLinkType()) &&
			(item->getLinkedUUID() == mBaseItemID)); // A linked item's assetID will be the compared-to item's itemID.
}

void LLSaveFolderState::setApply(bool apply)
{
	mApply = apply; 
	// before generating new list of open folders, clear the old one
	if(!apply) 
	{
		clearOpenFolders(); 
	}
}

void LLSaveFolderState::doFolder(LLFolderViewFolder* folder)
{
	LLInvFVBridge* bridge = (LLInvFVBridge*)folder->getViewModelItem();
	if(!bridge) return;
	
	if(mApply)
	{
		// we're applying the open state
		LLUUID id(bridge->getUUID());
		if(mOpenFolders.find(id) != mOpenFolders.end())
		{
			if (!folder->isOpen())
			{
				folder->setOpen(true);
			}
		}
		else
		{
			// keep selected filter in its current state, this is less jarring to user
			if (!folder->isSelected() && folder->isOpen())
			{
				folder->setOpen(false);
			}
		}
	}
	else
	{
		// we're recording state at this point
		if(folder->isOpen())
		{
			mOpenFolders.insert(bridge->getUUID());
		}
	}
}

void LLOpenFilteredFolders::doItem(LLFolderViewItem *item)
{
	if (item->passedFilter())
	{
		item->getParentFolder()->setOpenArrangeRecursively(true, LLFolderViewFolder::RECURSE_UP);
	}
}

void LLOpenFilteredFolders::doFolder(LLFolderViewFolder* folder)
{
	if (folder->LLFolderViewItem::passedFilter() && folder->getParentFolder())
	{
		folder->getParentFolder()->setOpenArrangeRecursively(true, LLFolderViewFolder::RECURSE_UP);
	}
	// if this folder didn't pass the filter, and none of its descendants did
	else if (!folder->getViewModelItem()->passedFilter() && !folder->getViewModelItem()->descendantsPassedFilter())
	{
		folder->setOpenArrangeRecursively(false, LLFolderViewFolder::RECURSE_NO);
	}
}

void LLSelectFirstFilteredItem::doItem(LLFolderViewItem *item)
{
	if (item->passedFilter() && !mItemSelected)
	{
		item->getRoot()->setSelection(item, false, false);
		if (item->getParentFolder())
		{
			item->getParentFolder()->setOpenArrangeRecursively(true, LLFolderViewFolder::RECURSE_UP);
		}
		mItemSelected = true;
	}
}

void LLSelectFirstFilteredItem::doFolder(LLFolderViewFolder* folder)
{
	// Skip if folder or item already found, if not filtered or if no parent (root folder is not selectable)
	if (!mFolderSelected && !mItemSelected && folder->LLFolderViewItem::passedFilter() && folder->getParentFolder())
	{
		folder->getRoot()->setSelection(folder, false, false);
		folder->getParentFolder()->setOpenArrangeRecursively(true, LLFolderViewFolder::RECURSE_UP);
		mFolderSelected = true;
	}
}

void LLOpenFoldersWithSelection::doItem(LLFolderViewItem *item)
{
	if (item->getParentFolder() && item->isSelected())
	{
		item->getParentFolder()->setOpenArrangeRecursively(true, LLFolderViewFolder::RECURSE_UP);
	}
}

void LLOpenFoldersWithSelection::doFolder(LLFolderViewFolder* folder)
{
	if (folder->getParentFolder() && folder->isSelected())
	{
		folder->getParentFolder()->setOpenArrangeRecursively(true, LLFolderViewFolder::RECURSE_UP);
	}
}

// Succeeds iff all selected items are bridges to objects, in which
// case returns their corresponding uuids.
bool get_selection_object_uuids(LLFolderView *root, uuid_vec_t& ids)
{
	uuid_vec_t results;
	S32 non_object = 0;
	LLFolderView::selected_items_t selectedItems = root->getSelectedItems();
	for(LLFolderView::selected_items_t::iterator it = selectedItems.begin(); it != selectedItems.end(); ++it)
	{
		LLObjectBridge *view_model = dynamic_cast<LLObjectBridge *>((*it)->getViewModelItem());

		if(view_model && view_model->getUUID().notNull())
		{
			results.push_back(view_model->getUUID());
		}
		else
		{
			non_object++;
		}
	}
	if (non_object == 0)
	{
		ids = results;
		return true;
	}
	return false;
}


void LLInventoryAction::doToSelected(LLInventoryModel* model, LLFolderView* root, const std::string& action)
{
	std::set<LLFolderViewItem*> selected_items = root->getSelectionList();

	if ("rename" == action)
	{
		root->startRenamingSelectedItem();
		return;
	}
    
	if ("delete" == action)
	{
		static bool sDisplayedAtSession = false;
		LLAllDescendentsPassedFilter f;
		for (std::set<LLFolderViewItem*>::iterator it = selected_items.begin(); (it != selected_items.end()) && (f.allDescendentsPassedFilter()); ++it)
		{
			if (LLFolderViewFolder* folder = dynamic_cast<LLFolderViewFolder*>(*it))
			{
				folder->applyFunctorRecursively(f);
			}
		}
		// Fall through to the generic confirmation if the user choose to ignore the specialized one
		if ( (!f.allDescendentsPassedFilter()) && (!LLNotifications::instance().getIgnored("DeleteFilteredItems")) )
		{
			LLNotificationsUtil::add("DeleteFilteredItems", LLSD(), LLSD(), boost::bind(&LLInventoryAction::onItemsRemovalConfirmation, _1, _2, root->getHandle()));
		}
		else
		{
			if (!sDisplayedAtSession) // ask for the confirmation at least once per session
			{
				LLNotifications::instance().setIgnored("DeleteItems", false);
				sDisplayedAtSession = true;
			}

			LLSD args;
			args["QUESTION"] = LLTrans::getString(root->getSelectedCount() > 1 ? "DeleteItems" :  "DeleteItem");
			LLNotificationsUtil::add("DeleteItems", args, LLSD(), boost::bind(&LLInventoryAction::onItemsRemovalConfirmation, _1, _2, root->getHandle()));
		}
		return;
	}
	if (("copy" == action) || ("cut" == action))
	{	
		// Clear the clipboard before we start adding things on it
		LLClipboard::instance().reset();
	}
	if ("replace_links" == action)
	{
		LLSD params;
		if (root->getSelectedCount() == 1)
		{
			LLFolderViewItem* folder_item = root->getSelectedItems().front();
			LLInvFVBridge* bridge = (LLInvFVBridge*)folder_item->getViewModelItem();

			if (bridge)
			{
				LLInventoryObject* obj = bridge->getInventoryObject();
				if (obj && obj->getType() != LLAssetType::AT_CATEGORY && obj->getActualType() != LLAssetType::AT_LINK_FOLDER)
				{
					params = LLSD(obj->getUUID());
				}
			}
		}
		LLFloaterReg::showInstance("linkreplace", params);
		return;
	}

	static const std::string change_folder_string = "change_folder_type_";
	if (action.length() > change_folder_string.length() && 
		(action.compare(0,change_folder_string.length(),"change_folder_type_") == 0))
	{
		LLFolderType::EType new_folder_type = LLViewerFolderType::lookupTypeFromXUIName(action.substr(change_folder_string.length()));
		LLFolderViewModelItemInventory* inventory_item = static_cast<LLFolderViewModelItemInventory*>(root->getViewModelItem());
		LLViewerInventoryCategory *cat = model->getCategory(inventory_item->getUUID());
		if (!cat) return;
		cat->changeType(new_folder_type);
		return;
	}


	LLMultiPreview* multi_previewp = nullptr;
	LLMultiProperties* multi_propertiesp = nullptr;

	if (("task_open" == action  || "open" == action) && selected_items.size() > 1)
	{
		bool open_multi_preview = true;

		if ("open" == action)
		{
			for (std::set<LLFolderViewItem*>::iterator set_iter = selected_items.begin(); set_iter != selected_items.end(); ++set_iter)
			{
				LLFolderViewItem* folder_item = *set_iter;
				if (folder_item)
				{
					LLInvFVBridge* bridge = dynamic_cast<LLInvFVBridge*>(folder_item->getViewModelItem());
					if (!bridge || !bridge->isMultiPreviewAllowed())
					{
						open_multi_preview = false;
						break;
					}
				}
			}
		}

		if (open_multi_preview)
		{
			multi_previewp = new LLMultiPreview();
			gFloaterView->addChild(multi_previewp);

			LLFloater::setFloaterHost(multi_previewp);
		}

	}
	else if (("task_properties" == action || "properties" == action) && selected_items.size() > 1)
	{
		multi_propertiesp = new LLMultiProperties();
		gFloaterView->addChild(multi_propertiesp);

		LLFloater::setFloaterHost(multi_propertiesp);
	}

	std::set<LLUUID> selected_uuid_set = LLAvatarActions::getInventorySelectedUUIDs();

    // copy list of applicable items into a vector for bulk handling
    uuid_vec_t ids;
    if (action == "wear" || action == "wear_add")
    {
        const LLUUID trash_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH);
        const LLUUID mp_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_MARKETPLACE_LISTINGS, false);
        std::copy_if(selected_uuid_set.begin(),
            selected_uuid_set.end(),
            std::back_inserter(ids),
            [trash_id, mp_id](LLUUID id)
        {
            if (get_is_item_worn(id)
                || LLAppearanceMgr::instance().getIsInCOF(id)
                || gInventory.isObjectDescendentOf(id, trash_id))
            {
                return false;
            }
            if (mp_id.notNull() && gInventory.isObjectDescendentOf(id, mp_id))
            {
                return false;
            }
            LLInventoryObject* obj = (LLInventoryObject*)gInventory.getObject(id);
            if (!obj)
            {
                return false;
            }
            if (obj->getIsLinkType() && gInventory.isObjectDescendentOf(obj->getLinkedUUID(), trash_id))
            {
                return false;
            }
            if (obj->getIsLinkType() && LLAssetType::lookupIsLinkType(obj->getType()))
            {
                // missing
                return false;
            }
            return true;
        }
        );
    }
    else if (isRemoveAction(action))
    {
        std::copy_if(selected_uuid_set.begin(),
            selected_uuid_set.end(),
            std::back_inserter(ids),
            [](LLUUID id)
        {
            return get_is_item_worn(id);
        }
        );
    }
    else
    {
        for (auto& item : selected_items)
        {
            ids.push_back(static_cast<LLFolderViewModelItemInventory*>((item)->getViewModelItem())->getUUID());
        }
    }

    // Check for actions that get handled in bulk
    if (action == "wear")
    {
        wear_multiple(ids, true);
    }
    else if (action == "wear_add")
    {
        wear_multiple(ids, false);
    }
    else if (isRemoveAction(action))
    {
        LLAppearanceMgr::instance().removeItemsFromAvatar(ids);
    }
    else if ("save_selected_as" == action)
    {
        (new LLDirPickerThread(boost::bind(&LLInventoryAction::saveMultipleTextures, _1, selected_items, model), std::string()))->getFile();
    }
    else if ("new_folder_from_selected" == action)
    {

        LLInventoryObject* first_item = gInventory.getObject(*ids.begin());
        if (!first_item)
        {
            return;
        }
        const LLUUID& parent_uuid = first_item->getParentUUID();
        for (uuid_vec_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
        {
            LLInventoryObject *item = gInventory.getObject(*it);
            if (!item || item->getParentUUID() != parent_uuid)
            {
                LLNotificationsUtil::add("SameFolderRequired");
                return;
            }
        }
        
        LLSD args;
        args["DESC"] = LLTrans::getString("New Folder");
 
        LLNotificationsUtil::add("CreateSubfolder", args, LLSD(),
            [ids](const LLSD& notification, const LLSD& response)
        {
            S32 opt = LLNotificationsUtil::getSelectedOption(notification, response);
            if (opt == 0)
            {
                std::string settings_name = response["message"].asString();

                LLInventoryObject::correctInventoryName(settings_name);
                if (settings_name.empty())
                {
                    settings_name = LLTrans::getString("New Folder");
                }
                move_items_to_new_subfolder(ids, settings_name);
            }
        });
    }
    else if ("ungroup_folder_items" == action)
    {
        if (ids.size() == 1)
        {
            LLInventoryCategory* inv_cat = gInventory.getCategory(*ids.begin());
            if (!inv_cat || LLFolderType::lookupIsProtectedType(inv_cat->getPreferredType()))
            {
                return;
            }
            const LLUUID &new_cat_uuid = inv_cat->getParentUUID();
            LLInventoryModel::cat_array_t* cat_array;
            LLInventoryModel::item_array_t* item_array;
            gInventory.getDirectDescendentsOf(inv_cat->getUUID(), cat_array, item_array);
            LLInventoryModel::cat_array_t cats = *cat_array;
            LLInventoryModel::item_array_t items = *item_array;

            for (LLInventoryModel::cat_array_t::const_iterator cat_iter = cats.begin(); cat_iter != cats.end(); ++cat_iter)
            {
                LLViewerInventoryCategory* cat = *cat_iter;
                if (cat)
                {
                    gInventory.changeCategoryParent(cat, new_cat_uuid, false);
                }
            }
            for (LLInventoryModel::item_array_t::const_iterator item_iter = items.begin(); item_iter != items.end(); ++item_iter)
            {
                LLViewerInventoryItem* item = *item_iter;
                if(item)
                {
                    gInventory.changeItemParent(item, new_cat_uuid, false);
                }
            }
            gInventory.removeCategory(inv_cat->getUUID());
            gInventory.notifyObservers();
        }
    }
    else
    {
        std::set<LLFolderViewItem*>::iterator set_iter;
        for (set_iter = selected_items.begin(); set_iter != selected_items.end(); ++set_iter)
        {
            LLFolderViewItem* folder_item = *set_iter;
            if(!folder_item) continue;
            LLInvFVBridge* bridge = (LLInvFVBridge*)folder_item->getViewModelItem();
            if(!bridge) continue;
            bridge->performAction(model, action);
        }
    }

	LLFloater::setFloaterHost(nullptr);
	if (multi_previewp)
	{
		multi_previewp->openFloater(LLSD());
	}
	else if (multi_propertiesp)
	{
		multi_propertiesp->openFloater(LLSD());
	}
}

void LLInventoryAction::saveMultipleTextures(const std::vector<std::string>& filenames, std::set<LLFolderViewItem*> selected_items, LLInventoryModel* model)
{
    gSavedSettings.setString("TextureSaveLocation", filenames[0]);
 
    LLMultiPreview* multi_previewp = new LLMultiPreview();
    gFloaterView->addChild(multi_previewp);

    LLFloater::setFloaterHost(multi_previewp);

    std::map<std::string, S32> tex_names_map;
    std::set<LLFolderViewItem*>::iterator set_iter;
   
    for (set_iter = selected_items.begin(); set_iter != selected_items.end(); ++set_iter)
    {
        LLFolderViewItem* folder_item = *set_iter;
        if(!folder_item) continue;
        LLTextureBridge* bridge = (LLTextureBridge*)folder_item->getViewModelItem();
        if(!bridge) continue;

        std::string tex_name = bridge->getName();
        if(!tex_names_map.insert(std::pair<std::string, S32>(tex_name, 0)).second) 
        { 
            tex_names_map[tex_name]++;
            bridge->setFileName(tex_name + llformat("_%.3d", tex_names_map[tex_name]));            
        }
        bridge->performAction(model, "save_selected_as");
    }

    LLFloater::setFloaterHost(NULL);
    if (multi_previewp)
    {
        multi_previewp->openFloater(LLSD());
    }
}

void LLInventoryAction::removeItemFromDND(LLFolderView* root)
{
    if(gAgent.isDoNotDisturb())
    {
        //Get selected items
        LLFolderView::selected_items_t selectedItems = root->getSelectedItems();
        LLFolderViewModelItemInventory * viewModel = nullptr;

        //If user is in DND and deletes item, make sure the notification is not displayed by removing the notification
        //from DND history and .xml file. Once this is done, upon exit of DND mode the item deleted will not show a notification.
        for(LLFolderView::selected_items_t::iterator it = selectedItems.begin(); it != selectedItems.end(); ++it)
        {
            viewModel = dynamic_cast<LLFolderViewModelItemInventory *>((*it)->getViewModelItem());

            if(viewModel && viewModel->getUUID().notNull())
            {
                //Will remove the item offer notification
                LLDoNotDisturbNotificationStorage::instance().removeNotification(LLDoNotDisturbNotificationStorage::offerName, viewModel->getUUID());
            }
        }
    }
}

void LLInventoryAction::onItemsRemovalConfirmation(const LLSD& notification, const LLSD& response, LLHandle<LLFolderView> root)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0 && !root.isDead() && !root.get()->isDead())
	{
		LLFolderView* folder_root = root.get();
		//Need to remove item from DND before item is removed from root folder view
		//because once removed from root folder view the item is no longer a selected item
		removeItemFromDND(folder_root);
		folder_root->removeSelectedItems();
	}
}


