/**
 * @file kokuafloaterbulkrename.cpp, adapted from llfloaterbulkpermissions.cpp
 * @author Michelle2 Zenovka (original) Chorazin Allen for kokuafloaterbulkrename
 * @brief A floater which allows task inventory item's names to be changed en masse.
 *
 * $LicenseInfo:firstyear=2008&license=viewerlgpl$
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
#include <regex>
#include "kokuafloaterbulkrename.h"
#include "llfloaterperms.h" // for utilities
#include "llagent.h"
#include "llchat.h"
#include "llinventorydefines.h"
#include "llviewerwindow.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llresmgr.h"
#include "llbutton.h"
#include "lldir.h"
#include "llviewerstats.h"
#include "lluictrlfactory.h"
#include "llselectmgr.h"
#include "llcheckboxctrl.h"

#include "roles_constants.h" // for GP_OBJECT_MANIPULATE


KokuaFloaterBulkRename::KokuaFloaterBulkRename(const LLSD& seed) 
:	LLFloater(seed),
	mDone(FALSE)
{
	mID.generate();
	mCommitCallbackRegistrar.add("BulkRename.Ok",		boost::bind(&KokuaFloaterBulkRename::onOkBtn, this));
	mCommitCallbackRegistrar.add("BulkRename.Apply",	boost::bind(&KokuaFloaterBulkRename::onApplyBtn, this));
	mCommitCallbackRegistrar.add("BulkRename.Close",	boost::bind(&KokuaFloaterBulkRename::onCloseBtn, this));
	mCommitCallbackRegistrar.add("BulkRename.CheckAll",	boost::bind(&KokuaFloaterBulkRename::onCheckAll, this));
	mCommitCallbackRegistrar.add("BulkRename.UncheckAll",	boost::bind(&KokuaFloaterBulkRename::onUncheckAll, this));
}

BOOL KokuaFloaterBulkRename::postBuild()
{
	//with these being named BulkChange rather than BulkPermissions I'm inclined to not create my own set for renaming
	mBulkChangeIncludeAnimations = gSavedSettings.getBOOL("BulkChangeIncludeAnimations");
	mBulkChangeIncludeBodyParts = gSavedSettings.getBOOL("BulkChangeIncludeBodyParts");
	mBulkChangeIncludeClothing = gSavedSettings.getBOOL("BulkChangeIncludeClothing");
	mBulkChangeIncludeGestures = gSavedSettings.getBOOL("BulkChangeIncludeGestures");
	mBulkChangeIncludeNotecards = gSavedSettings.getBOOL("BulkChangeIncludeNotecards");
	mBulkChangeIncludeObjects = gSavedSettings.getBOOL("BulkChangeIncludeObjects");
	mBulkChangeIncludeScripts = gSavedSettings.getBOOL("BulkChangeIncludeScripts");
	mBulkChangeIncludeSounds = gSavedSettings.getBOOL("BulkChangeIncludeSounds");
	mBulkChangeIncludeTextures = gSavedSettings.getBOOL("BulkChangeIncludeTextures");
	mBulkChangeIncludeSettings = gSavedSettings.getBOOL("BulkChangeIncludeSettings");

	return TRUE;
}

void KokuaFloaterBulkRename::doApply()
{
	// Inspects a stream of selected object contents and adds modifiable ones to the given array.
	class ModifiableGatherer : public LLSelectedNodeFunctor
	{
	public:
		ModifiableGatherer(std::vector<LLUUID>& q) : mQueue(q) { mQueue.reserve(32); }
		virtual bool apply(LLSelectNode* node)
		{
			if( node->allowOperationOnNode(PERM_MODIFY, GP_OBJECT_MANIPULATE) )
			{
				mQueue.push_back(node->getObject()->getID());
			}
			return true;
		}
	private:
		std::vector<LLUUID>& mQueue;
	};
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("queue output");
	list->deleteAllItems();
	list->setEnabled(true);
	ModifiableGatherer gatherer(mObjectIDs);
	LLSelectMgr::getInstance()->getSelection()->applyToNodes(&gatherer);
	if(mObjectIDs.empty())
	{
		list->addCommentText(getString("nothing_to_modify_text"));
	}
	else
	{
		mDone = FALSE;
		if (!start())
		{
			LL_WARNS() << "Unexpected bulk permission change failure." << LL_ENDL;
		}
	}
}


// This is the callback method for the viewer object currently being
// worked on.
// NOT static, virtual!
void KokuaFloaterBulkRename::inventoryChanged(LLViewerObject* viewer_object,
											 LLInventoryObject::object_list_t* inv,
											 S32,
											 void* q_id)
{
	//LL_INFOS() << "changed object: " << viewer_object->getID() << LL_ENDL;

	//Remove this listener from the object since its
	//listener callback is now being executed.
	
	//We remove the listener here because the function
	//removeVOInventoryListener removes the listener from a ViewerObject
	//which it internally stores.
	
	//If we call this further down in the function, calls to handleInventory
	//and nextObject may update the interally stored viewer object causing
	//the removal of the incorrect listener from an incorrect object.
	
	//Fixes SL-6119:Recompile scripts fails to complete
	removeVOInventoryListener();

	if (viewer_object && inv && (viewer_object->getID() == mCurrentObjectID) )
	{
		handleInventory(viewer_object, inv);
	}
	else
	{
		// something went wrong...
		// note that we're not working on this one, and move onto the
		// next object in the list.
		LL_WARNS() << "No inventory for " << mCurrentObjectID << LL_ENDL;
		nextObject();
	}
}

void KokuaFloaterBulkRename::onOkBtn()
{
	doApply();
	closeFloater();
}

void KokuaFloaterBulkRename::onApplyBtn()
{
	doApply();
}

void KokuaFloaterBulkRename::onCloseBtn()
{
	gSavedSettings.setBOOL("BulkChangeIncludeAnimations", mBulkChangeIncludeAnimations);
	gSavedSettings.setBOOL("BulkChangeIncludeBodyParts", mBulkChangeIncludeBodyParts);
	gSavedSettings.setBOOL("BulkChangeIncludeClothing", mBulkChangeIncludeClothing);
	gSavedSettings.setBOOL("BulkChangeIncludeGestures", mBulkChangeIncludeGestures);
	gSavedSettings.setBOOL("BulkChangeIncludeNotecards", mBulkChangeIncludeNotecards);
	gSavedSettings.setBOOL("BulkChangeIncludeObjects", mBulkChangeIncludeObjects);
	gSavedSettings.setBOOL("BulkChangeIncludeScripts", mBulkChangeIncludeScripts);
	gSavedSettings.setBOOL("BulkChangeIncludeSounds", mBulkChangeIncludeSounds);
	gSavedSettings.setBOOL("BulkChangeIncludeTextures", mBulkChangeIncludeTextures);
	gSavedSettings.setBOOL("BulkChangeIncludeSettings", mBulkChangeIncludeSettings);
	closeFloater();
}

BOOL KokuaFloaterBulkRename::start()
{
	mSearchRegExp = getChild<LLUICtrl>("search_term")->getValue().asString();
	//LL_INFOS() << "Search term is " << mSearchRegExp << LL_ENDL;
	mReplaceWith = getChild<LLUICtrl>("replace_term")->getValue().asString();
	//LL_INFOS() << "Replace term is " << mReplaceWith << LL_ENDL;
	// note: number of top-level objects to modify is mObjectIDs.size().
	getChild<LLScrollListCtrl>("queue output")->setCommentText(getString("start_text"));
	return nextObject();
}

// Go to the next object and start if found. Returns false if no objects left, true otherwise.
BOOL KokuaFloaterBulkRename::nextObject()
{
	S32 count;
	BOOL successful_start = FALSE;
	do
	{
		count = mObjectIDs.size();
		//LL_INFOS() << "Objects left to process = " << count << LL_ENDL;
		mCurrentObjectID.setNull();
		if(count > 0)
		{
			successful_start = popNext();
			//LL_INFOS() << (successful_start ? "successful" : "unsuccessful") << LL_ENDL; 
		}
	} while((mObjectIDs.size() > 0) && !successful_start);

	if(isDone() && !mDone)
	{
		getChild<LLScrollListCtrl>("queue output")->addCommentText(getString("done_text"));
		mDone = TRUE;
	}
	return successful_start;
}

// Pop the top object off of the queue.
// Return TRUE if the queue has started, otherwise FALSE.
BOOL KokuaFloaterBulkRename::popNext()
{
	// get the head element from the container, and attempt to get its inventory.
	BOOL rv = FALSE;
	S32 count = mObjectIDs.size();
	if(mCurrentObjectID.isNull() && (count > 0))
	{
		mCurrentObjectID = mObjectIDs.at(0);
		//LL_INFOS() << "mCurrentID: " << mCurrentObjectID << LL_ENDL;
		mObjectIDs.erase(mObjectIDs.begin());
		LLViewerObject* obj = gObjectList.findObject(mCurrentObjectID);
		if(obj)
		{
			//LL_INFOS() << "requesting inv for " << mCurrentObjectID << LL_ENDL;
			LLUUID* id = new LLUUID(mID);
			registerVOInventoryListener(obj,id);
			requestVOInventory();
			rv = TRUE;
		}
		else
		{
			LL_INFOS()<<"NULL LLViewerObject" <<LL_ENDL;
		}
	}
	return rv;
}


void KokuaFloaterBulkRename::doCheckUncheckAll(BOOL check)
{
	gSavedSettings.setBOOL("BulkChangeIncludeAnimations", check);
	gSavedSettings.setBOOL("BulkChangeIncludeBodyParts" , check);
	gSavedSettings.setBOOL("BulkChangeIncludeClothing"  , check);
	gSavedSettings.setBOOL("BulkChangeIncludeGestures"  , check);
	gSavedSettings.setBOOL("BulkChangeIncludeNotecards" , check);
	gSavedSettings.setBOOL("BulkChangeIncludeObjects"   , check);
	gSavedSettings.setBOOL("BulkChangeIncludeScripts"   , check);
	gSavedSettings.setBOOL("BulkChangeIncludeSounds"    , check);
	gSavedSettings.setBOOL("BulkChangeIncludeTextures"  , check);
	gSavedSettings.setBOOL("BulkChangeIncludeSettings"  , check);
}


void KokuaFloaterBulkRename::handleInventory(LLViewerObject* viewer_obj, LLInventoryObject::object_list_t* inv)
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("queue output");

	LLInventoryObject::object_list_t::const_iterator it = inv->begin();
	LLInventoryObject::object_list_t::const_iterator end = inv->end();
	for ( ; it != end; ++it)
	{
		LLAssetType::EType asstype = (*it)->getType();
		if(
			( asstype == LLAssetType::AT_ANIMATION && gSavedSettings.getBOOL("BulkChangeIncludeAnimations")) ||
			( asstype == LLAssetType::AT_BODYPART  && gSavedSettings.getBOOL("BulkChangeIncludeBodyParts" )) ||
			( asstype == LLAssetType::AT_CLOTHING  && gSavedSettings.getBOOL("BulkChangeIncludeClothing"  )) ||
			( asstype == LLAssetType::AT_GESTURE   && gSavedSettings.getBOOL("BulkChangeIncludeGestures"  )) ||
			( asstype == LLAssetType::AT_NOTECARD  && gSavedSettings.getBOOL("BulkChangeIncludeNotecards" )) ||
			( asstype == LLAssetType::AT_OBJECT    && gSavedSettings.getBOOL("BulkChangeIncludeObjects"   )) ||
			( asstype == LLAssetType::AT_LSL_TEXT  && gSavedSettings.getBOOL("BulkChangeIncludeScripts"   )) ||
			( asstype == LLAssetType::AT_SOUND     && gSavedSettings.getBOOL("BulkChangeIncludeSounds"    )) ||
			( asstype == LLAssetType::AT_SETTINGS  && gSavedSettings.getBOOL("BulkChangeIncludeSettings"  )) ||
			( asstype == LLAssetType::AT_TEXTURE   && gSavedSettings.getBOOL("BulkChangeIncludeTextures"  )))
		{
			LLViewerObject* object = gObjectList.findObject(viewer_obj->getID());

			if (object)
			{
				LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
				LLViewerInventoryItem* new_item = (LLViewerInventoryItem*)item;
				LLPermissions perm(new_item->getPermissions());

				std::string invname= item->getName();
				
				LLUIString status_text;

				if (gAgent.allowOperation(PERM_MODIFY, perm, GP_OBJECT_MANIPULATE))
				{
					// Do we get a match on the regex?
					if (std::regex_search(item->getName(), (std::regex)mSearchRegExp))
					{
						std::string new_name;
						new_name = std::regex_replace(item->getName(), (std::regex)mSearchRegExp, mReplaceWith);
						//LL_INFOS() << "Got a hit on " << item->getName() << " with " << mSearchRegExp << " renamed to " << new_name << LL_ENDL;
						new_item->rename(new_name);
						status_text = getString("renaming_text");
						status_text.setArg("[NEWNAME]", new_name);
					}
					updateInventory(object,new_item,TASK_INVENTORY_ITEM_KEY,FALSE);
				}
				else
				{
					status_text = getString("nomod_text");
				}
				status_text.setArg("[NAME]", invname.c_str());
				
				list->addCommentText(status_text.getString());
			}
		}
	}
	nextObject();
}

// Avoid inventory callbacks etc by just fire and forgetting the message with the name update
// we could do this via LLViewerObject::updateInventory but that uses inventory call backs and
// we would have a dodgy item iterator

void KokuaFloaterBulkRename::updateInventory(LLViewerObject* object, LLViewerInventoryItem* item, U8 key, bool is_new)
{
	// This slices the object into what we're concerned about on the viewer. 
	// The simulator will take the permissions and transfer ownership.
	//LL_INFOS() << "Processing " << item->getName() << LL_ENDL;
	LLPointer<LLViewerInventoryItem> task_item =
		new LLViewerInventoryItem(item->getUUID(), mID, item->getPermissions(),
								  item->getAssetUUID(), item->getType(),
								  item->getInventoryType(),
								  item->getName(), item->getDescription(),
								  item->getSaleInfo(),
								  item->getFlags(),
								  item->getCreationDate());
	task_item->setTransactionID(item->getTransactionID());
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_UpdateTaskInventory);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_UpdateData);
	msg->addU32Fast(_PREHASH_LocalID, object->mLocalID);
	msg->addU8Fast(_PREHASH_Key, key);
	msg->nextBlockFast(_PREHASH_InventoryData);
	task_item->packMessage(msg);
	msg->sendReliable(object->getRegion()->getHost());
}