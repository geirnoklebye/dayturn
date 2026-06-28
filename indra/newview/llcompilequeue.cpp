/** 
 * @file llcompilequeue.cpp
 * @brief LLCompileQueueData class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

/**
 *
 * Implementation of the script queue which keeps an array of object
 * UUIDs and manipulates all of the scripts on each of them. 
 *
 */


#include "llviewerprecompiledheaders.h"

#include "llcompilequeue.h"

#include "llagent.h"
#include "llchat.h"
#include "llfloaterreg.h"
#include "llviewerwindow.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llresmgr.h"

#include "llbutton.h"
#include "lldir.h"
#include "llnotificationsutil.h"
#include "llviewerstats.h"
#include "llfilesystem.h"
#include "lluictrlfactory.h"
#include "lltrans.h"

#include "llselectmgr.h"
#include "llexperiencecache.h"

#include "llviewerassetupload.h"
#include "llcorehttputil.h"
#include <map>

// used to resolve item IDs to the floater handle the message reply is for
typedef std::map<std::string, LLHandle<LLFloaterScriptQueue>> CQMAP;
// KKA-678 Add initialisation
CQMAP compile_queue_floater_handles = {};
	
namespace
{

    const std::string QUEUE_EVENTPUMP_NAME("ScriptActionQueue");
    const F32 QUEUE_INVENTORY_FETCH_TIMEOUT = 300.f;

    // ObjectIventoryFetcher is an adapter between the LLVOInventoryListener::inventoryChanged 
    // callback mechanism and the LLEventPump coroutine architecture allowing the 
    // coroutine to wait for the inventory event.
    class ObjectInventoryFetcher: public LLVOInventoryListener
    {
    public:
        typedef std::shared_ptr<ObjectInventoryFetcher> ptr_t;

        ObjectInventoryFetcher(LLEventPump &pump, LLViewerObject* object, void* user_data) :
            mPump(pump),
            LLVOInventoryListener()
        {
            registerVOInventoryListener(object, this);
        }

        virtual void inventoryChanged(LLViewerObject* object,
            LLInventoryObject::object_list_t* inventory,
            S32 serial_num,
            void* user_data);

        void fetchInventory() 
        {
            requestVOInventory();
        }

        const LLInventoryObject::object_list_t & getInventoryList() const { return mInventoryList; }

    private:
        LLInventoryObject::object_list_t    mInventoryList;
        LLEventPump &                       mPump;
    };

    class HandleScriptUserData
    {
    public:
        HandleScriptUserData(const std::string &pumpname) :
            mPumpname(pumpname)
        { }

        const std::string &getPumpName() const { return mPumpname; }

    private:
        std::string mPumpname;
    };


}

// *NOTE$: A minor specialization of LLScriptAssetUpload, it does not require a buffer 
// (and does not save a buffer to the cache) and it finds the compile queue window and 
// displays a compiling message.
class LLQueuedScriptAssetUpload : public LLScriptAssetUpload
{
public:
    LLQueuedScriptAssetUpload(LLUUID taskId, LLUUID itemId, LLUUID assetId, TargetType_t targetType,
            bool isRunning, std::string scriptName, LLUUID queueId, LLUUID exerienceId, taskUploadFinish_f finish) :
        LLScriptAssetUpload(taskId, itemId, targetType, isRunning, 
            exerienceId, std::string(), finish),
        mScriptName(scriptName),
        mQueueId(queueId)
    {
        setAssetId(assetId);
    }

    virtual LLSD prepareUpload()
    {
        /* *NOTE$: The parent class (LLScriptAssetUpload will attempt to save 
         * the script buffer into to the cache.  Since the resource is already in 
         * the cache we don't want to do that.  Just put a compiling message in
         * the window and move on
         */
        LLFloaterCompileQueue* queue = LLFloaterReg::findTypedInstance<LLFloaterCompileQueue>("compile_queue", LLSD(mQueueId));
        if (queue)
        {
            std::string message = std::string("Compiling \"") + getScriptName() + std::string("\"...");

            queue->getChild<LLScrollListCtrl>("queue output")->addSimpleElement(message, ADD_BOTTOM);
        }

        return LLSDMap("success", LLSD::Boolean(true));
    }


    std::string getScriptName() const { return mScriptName; }

private:
    void setScriptName(const std::string &scriptName) { mScriptName = scriptName; }

    LLUUID mQueueId;
    std::string mScriptName;
};

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

struct LLScriptQueueData
{
	LLUUID mQueueID;
	LLUUID mTaskId;
	LLPointer<LLInventoryItem> mItem;
	LLHost mHost;
	LLUUID mExperienceId;
	std::string mExperiencename;
	LLScriptQueueData(const LLUUID& q_id, const LLUUID& task_id, LLInventoryItem* item) :
		mQueueID(q_id), mTaskId(task_id), mItem(new LLInventoryItem(item)) {}

};

///----------------------------------------------------------------------------
/// Class LLFloaterScriptQueue
///----------------------------------------------------------------------------

// Default constructor
LLFloaterScriptQueue::LLFloaterScriptQueue(const LLSD& key) :
	LLFloater(key),
	mDone(false),
	mFull(false),
	mMono(false),
	mDeleteName(""),
	mDeleteOnly(false)
{
	
}

// Destroys the object
LLFloaterScriptQueue::~LLFloaterScriptQueue()
{
}

bool LLFloaterScriptQueue::postBuild()
{
	childSetAction("close",onCloseBtn,this);
	getChildView("close")->setEnabled(false);
	getChild<LLUICtrl>("copy_btn")->setCommitCallback(boost::bind(&LLFloaterScriptQueue::onClickCopyToClipboard, this));
	setVisible(true);
	return true;
}

// static
void LLFloaterScriptQueue::onCloseBtn(void* user_data)
{
	LLFloaterScriptQueue* self = (LLFloaterScriptQueue*)user_data;
	// if we were doing a Locate Full aka Status we also need to clean up the map
	// the map is item uuid to floater handle so there is probably more than one entry to clear
	if (! compile_queue_floater_handles.empty())
	{
		CQMAP::iterator it;
		it = compile_queue_floater_handles.begin();
		while (it != compile_queue_floater_handles.end())
		{
			if (self == it->second.get())
			{
				compile_queue_floater_handles.erase(it++);
			}
			else
			{
				++it;
			}
		}
	}
	self->closeFloater();
}

void LLFloaterScriptQueue::onClickCopyToClipboard()
{
	LLScrollListCtrl *temp = getChild<LLScrollListCtrl>("queue output", true);
	temp->selectAll();
	temp->copy();
	temp->deselect();
}

void LLFloaterScriptQueue::addObject(const LLUUID& id, std::string name)
{
    ObjectData obj = { id, name };
    mObjectList.push_back(obj);
}

bool LLFloaterScriptQueue::start()
{
	// Locate and Locate Full aka Status do not modify, so no need for a notification
	// Delete is dangerous and needs a stern notification
	// Change running state or reset also modify state but less seriously than delete
	
	if (mStartString == LLTrans::getString("DeleteQueueStart"))
	{
			if (mDeleteOnly)
			{
				LLSD args;
				args["[NAME]"] = getDeleteName();

				LLNotificationsUtil::add("ConfirmScriptQueueDeleteOnly", args, LLSD(), boost::bind(&LLFloaterScriptQueue::onScriptQueueConfirmation, this, _1, _2));
			}
			else
			{
				LLNotificationsUtil::add("ConfirmScriptQueueDelete", LLSD(), LLSD(), boost::bind(&LLFloaterScriptQueue::onScriptQueueConfirmation, this, _1, _2));
			}
	}
	else if (mStartString == LLTrans::getString("LocateQueueStart"))
	{
		return confirmedStart();
	}
	else
	{
		LLNotificationsUtil::add("ConfirmScriptQueueModify", LLSD(), LLSD(), boost::bind(&LLFloaterScriptQueue::onScriptQueueConfirmation, this, _1, _2));
	}	
	// this is fibbing a bit; we may actually decide to abort via notification response
	return true;
}

bool LLFloaterScriptQueue::onScriptQueueConfirmation(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option != 0)
	{
		closeFloater();
		return true;
	}

	return confirmedStart();
}

// actually start, either after a confirmation or directly depending on the operation
bool LLFloaterScriptQueue::confirmedStart()
{	
	std::string buffer;

	LLStringUtil::format_map_t args;
	args["[START]"] = mStartString;
	args["[COUNT]"] = llformat ("%d", mObjectList.size());
	buffer = getString ("Starting", args);
	
	// <FS:Ansariel> Improve log output
	//getChild<LLScrollListCtrl>("queue output")->addSimpleElement(buffer, ADD_BOTTOM);
	addStringMessage(buffer);
	// </FS:Ansariel>

	return startQueue();
}

// bring across Ansariel's version that keeps the cursor at the bottom of the scroll list
void LLFloaterScriptQueue::addProcessingMessage(const std::string &message, const LLSD &args)
{
    std::string buffer(LLTrans::getString(message, args));

    // <FS:Ansariel> Improve log output
    //getChild<LLScrollListCtrl>("queue output")->addSimpleElement(buffer, ADD_BOTTOM);
    addStringMessage(buffer);
    // </FS:Ansariel>
}

void LLFloaterScriptQueue::addStringMessage(const std::string &message)
{
    // <FS:Ansariel> Improve log output
    //getChild<LLScrollListCtrl>("queue output")->addSimpleElement(message, ADD_BOTTOM);
    LLScrollListCtrl* ctrl = getChild<LLScrollListCtrl>("queue output");
    bool is_at_end = ctrl->getScrollbar()->isAtEnd();
    ctrl->addSimpleElement(message, ADD_BOTTOM);
    if (is_at_end)
    {
        ctrl->setScrollPos(ctrl->getScrollPos() + 1);
    }
    // </FS:Ansariel>
}


bool LLFloaterScriptQueue::isDone() const
{
	return (mCurrentObjectID.isNull() && (mObjectList.size() == 0));
}

///----------------------------------------------------------------------------
/// Class LLFloaterCompileQueue
///----------------------------------------------------------------------------
LLFloaterCompileQueue::LLFloaterCompileQueue(const LLSD& key)
  : LLFloaterScriptQueue(key)
{
	setTitle(LLTrans::getString("CompileQueueTitle"));
	setStartString(LLTrans::getString("CompileQueueStart"));
														 															 
}

LLFloaterCompileQueue::~LLFloaterCompileQueue()
{ 
}

void LLFloaterCompileQueue::experienceIdsReceived( const LLSD& content )
{
	for(LLSD::array_const_iterator it  = content.beginArray(); it != content.endArray(); ++it)
	{
		mExperienceIds.insert(it->asUUID());
	}
}

bool LLFloaterCompileQueue::hasExperience( const LLUUID& id ) const
{
	return mExperienceIds.find(id) != mExperienceIds.end();
}

// //Attempt to record this asset ID.  If it can not be inserted into the set 
// //then it has already been processed so return false.

void LLFloaterCompileQueue::handleHTTPResponse(std::string pumpName, const LLSD &expresult)
{
    LLEventPumps::instance().post(pumpName, expresult);
}

// *TODO: handleSCriptRetrieval is passed into the cache via a legacy C function pointer
// future project would be to convert these to C++ callables (std::function<>) so that 
// we can use bind and remove the userData parameter.
// 
void LLFloaterCompileQueue::handleScriptRetrieval(const LLUUID& assetId, 
    LLAssetType::EType type, void* userData, S32 status, LLExtStat extStatus)
{
    LLSD result(LLSD::emptyMap());

    result["asset_id"] = assetId;
    if (status)
    {
        result["error"] = status;
     
        if (status == LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE)
        {
            result["message"] = LLTrans::getString("CompileQueueProblemDownloading") + (":");
            result["alert"] = LLTrans::getString("CompileQueueScriptNotFound");
        }
        else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
        {
            result["message"] = LLTrans::getString("CompileQueueInsufficientPermFor") + (":");
            result["alert"] = LLTrans::getString("CompileQueueInsufficientPermDownload");
        }
        else
        {
            result["message"] = LLTrans::getString("CompileQueueUnknownFailure");
        }
    }

    LLEventPumps::instance().post(((HandleScriptUserData *)userData)->getPumpName(), result);

}

/*static*/
void LLFloaterCompileQueue::processExperienceIdResults(LLSD result, LLUUID parent)
{
    LLFloaterCompileQueue* queue = LLFloaterReg::findTypedInstance<LLFloaterCompileQueue>("compile_queue", parent);
    if (!queue)
        return;

    queue->experienceIdsReceived(result["experience_ids"]);

    // getDerived handle gets a handle that can be resolved to a parent class of the derived object.
    LLHandle<LLFloaterScriptQueue> hFloater(queue->getDerivedHandle<LLFloaterScriptQueue>());

    // note subtle difference here: getDerivedHandle in this case is for an LLFloaterCompileQueue
    fnQueueAction_t fn = boost::bind(LLFloaterCompileQueue::processScript,
        queue->getDerivedHandle<LLFloaterCompileQueue>(), _1, _2, _3);


    LLCoros::instance().launch("ScriptQueueCompile", boost::bind(LLFloaterScriptQueue::objectScriptProcessingQueueCoro,
        queue->mStartString,
        hFloater,
        queue->mObjectList,
        fn));

}

/// This is a utility function to be bound and called from objectScriptProcessingQueueCoro.
/// Do not call directly. It may throw a LLCheckedHandle<>::Stale exception.
bool LLFloaterCompileQueue::processScript(LLHandle<LLFloaterCompileQueue> hfloater,
    const LLPointer<LLViewerObject> &object, LLInventoryObject* inventory, LLEventPump &pump)
{
    if (LLApp::isExiting())
    {
        // Reply from coroutine came on shutdown
        // We are quiting, don't start any more coroutines!
        return true;
    }

    LLSD result;
    LLCheckedHandle<LLFloaterCompileQueue> floater(hfloater);
    // Dereferencing floater may fail. If they do they throw LLExeceptionStaleHandle.
    // which is caught in objectScriptProcessingQueueCoro
    bool monocompile = floater->mMono;


    // Initial test to see if we can (or should) attempt to compile the script.
    LLInventoryItem *item = dynamic_cast<LLInventoryItem *>(inventory);

    if (!item)
    {
        LL_WARNS("SCRIPTQ") << "item retrieved is not an LLInventoryItem." << LL_ENDL;
        return true;
    }

    if (!item->getPermissions().allowModifyBy(gAgent.getID(), gAgent.getGroupID()) ||
        !item->getPermissions().allowCopyBy(gAgent.getID(), gAgent.getGroupID()))
    {
        std::string buffer = "Skipping: " + item->getName() + "(Permissions)";
        floater->addStringMessage(buffer);
        return true;
    }

    // Attempt to retrieve the experience
    LLUUID experienceId;
    {
        LLExperienceCache::instance().fetchAssociatedExperience(inventory->getParentUUID(), inventory->getUUID(),
            boost::bind(&LLFloaterCompileQueue::handleHTTPResponse, pump.getName(), _1));

        result = llcoro::suspendUntilEventOnWithTimeout(pump, QUEUE_INVENTORY_FETCH_TIMEOUT,
            LLSDMap("timeout", LLSD::Boolean(true)));

        floater.check();

        if (result.has("timeout"))
        {   // A timeout filed in the result will always be true if present.
            LLStringUtil::format_map_t args;
            args["[OBJECT_NAME]"] = inventory->getName();
            std::string buffer = floater->getString("Timeout", args);
            floater->addStringMessage(buffer);
            return true;
        }

        if (result.has(LLExperienceCache::EXPERIENCE_ID))
        {
            experienceId = result[LLExperienceCache::EXPERIENCE_ID].asUUID();
            if (!floater->hasExperience(experienceId))
            {
                floater->addProcessingMessage("CompileNoExperiencePerm", 
                    LLSDMap("SCRIPT", inventory->getName())
                        ("EXPERIENCE", result[LLExperienceCache::NAME].asString()));
                return true;
            }
        }

    }

    if (!gAssetStorage)
    {
        // viewer likely is shutting down
        return true;
    }

    {
        HandleScriptUserData    userData(pump.getName());


        // request the asset
        gAssetStorage->getInvItemAsset(LLHost(),
            gAgent.getID(),
            gAgent.getSessionID(),
            item->getPermissions().getOwner(),
            object->getID(),
            item->getUUID(),
            item->getAssetUUID(),
            item->getType(),
            &LLFloaterCompileQueue::handleScriptRetrieval,
            &userData);

        result = llcoro::suspendUntilEventOnWithTimeout(pump, QUEUE_INVENTORY_FETCH_TIMEOUT,
            LLSDMap("timeout", LLSD::Boolean(true)));
    }

    if (result.has("timeout"))
    {   // A timeout filed in the result will always be true if present.
        LLStringUtil::format_map_t args;
        args["[OBJECT_NAME]"] = inventory->getName();
        std::string buffer = floater->getString("Timeout", args);
        floater->addStringMessage(buffer);
        return true;
    }

    if (result.has("error"))
    {
        LL_WARNS("SCRIPTQ") << "Inventory fetch returned with error. Code: " << result["error"].asString() << LL_ENDL;
        std::string buffer = result["message"].asString() + " " + inventory->getName();
        floater->addStringMessage(buffer);

        if (result.has("alert"))
        {
            LLSD args;
            args["MESSAGE"] = result["alert"].asString();
            LLNotificationsUtil::add("SystemMessage", args);
        }
        return true;
    }

    LLUUID assetId = result["asset_id"];

    std::string url = object->getRegion()->getCapability("UpdateScriptTask");

    {
        LLResourceUploadInfo::ptr_t uploadInfo = std::make_shared<LLQueuedScriptAssetUpload>(object->getID(),
            inventory->getUUID(), 
            assetId, 
            monocompile ? LLScriptAssetUpload::MONO : LLScriptAssetUpload::LSL2,
            true, 
            inventory->getName(), 
            LLUUID(), 
            experienceId, 
            boost::bind(&LLFloaterCompileQueue::handleHTTPResponse, pump.getName(), _4));

        LLViewerAssetUpload::EnqueueInventoryUpload(url, uploadInfo);
    }

    result = llcoro::suspendUntilEventOnWithTimeout(pump, QUEUE_INVENTORY_FETCH_TIMEOUT, LLSDMap("timeout", LLSD::Boolean(true)));

    floater.check();

    if (result.has("timeout"))
    { // A timeout filed in the result will always be true if present.
        LLStringUtil::format_map_t args;
        args["[OBJECT_NAME]"] = inventory->getName();
        std::string buffer = floater->getString("Timeout", args);
        floater->addStringMessage(buffer);
        return true;
    }

    // Bytecode save completed
    if (result["compiled"])
    {
        std::string buffer = std::string("Compilation of \"") + inventory->getName() + std::string("\" succeeded");

        floater->addStringMessage(buffer);
        LL_INFOS() << buffer << LL_ENDL;
    }
    else
    {
        LLSD compile_errors = result["errors"];
        std::string buffer = std::string("Compilation of \"") + inventory->getName() + std::string("\" failed:");
        floater->addStringMessage(buffer);
        for (LLSD::array_const_iterator line = compile_errors.beginArray();
            line < compile_errors.endArray(); line++)
        {
            std::string str = line->asString();
            str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());

            floater->addStringMessage(str);
        }
        LL_INFOS() << result["errors"] << LL_ENDL;
    }

    return true;
}

bool LLFloaterCompileQueue::startQueue()
{
    LLViewerRegion* region = gAgent.getRegion();
    if (region)
    {
        std::string lookup_url = region->getCapability("GetCreatorExperiences");
        if (!lookup_url.empty())
        {
            LLCoreHttpUtil::HttpCoroutineAdapter::completionCallback_t success =
                boost::bind(&LLFloaterCompileQueue::processExperienceIdResults, _1, getKey().asUUID());

            LLCoreHttpUtil::HttpCoroutineAdapter::completionCallback_t failure =
                boost::bind(&LLFloaterCompileQueue::processExperienceIdResults, LLSD(), getKey().asUUID());

            LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet(lookup_url,
                success, failure);
            return true;
        }
    }

    return true;
}


///----------------------------------------------------------------------------
/// Class LLFloaterResetQueue
///----------------------------------------------------------------------------

LLFloaterResetQueue::LLFloaterResetQueue(const LLSD& key)
  : LLFloaterScriptQueue(key)
{
	setTitle(LLTrans::getString("ResetQueueTitle"));
	setStartString(LLTrans::getString("ResetQueueStart"));
}

LLFloaterResetQueue::~LLFloaterResetQueue()
{ 
}

/// This is a utility function to be bound and called from objectScriptProcessingQueueCoro.
/// Do not call directly. It may throw a LLCheckedHandle<>::Stale exception.
bool LLFloaterResetQueue::resetObjectScripts(LLHandle<LLFloaterScriptQueue> hfloater,
    const LLPointer<LLViewerObject> &object, LLInventoryObject* inventory, LLEventPump &pump)
{
    LLCheckedHandle<LLFloaterScriptQueue> floater(hfloater);
    // Dereferencing floater may fail. If they do they throw LLExeceptionStaleHandle.
    // which is caught in objectScriptProcessingQueueCoro

    std::string buffer;
    buffer = floater->getString("Resetting") + (": ") + inventory->getName();
    floater->addStringMessage(buffer);
    
    LLMessageSystem* msg = gMessageSystem;
    msg->newMessageFast(_PREHASH_ScriptReset);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    msg->nextBlockFast(_PREHASH_Script);
    msg->addUUIDFast(_PREHASH_ObjectID, object->getID());
    msg->addUUIDFast(_PREHASH_ItemID, inventory->getUUID());
    msg->sendReliable(object->getRegion()->getHost());

    return true;
}

bool LLFloaterResetQueue::startQueue()
{
    // Bind the resetObjectScripts method into a QueueAction function and pass it
    // into the object queue processing coroutine.
    fnQueueAction_t fn = boost::bind(LLFloaterResetQueue::resetObjectScripts,
        getDerivedHandle<LLFloaterScriptQueue>(), _1, _2, _3);

    LLCoros::instance().launch("ScriptResetQueue", boost::bind(LLFloaterScriptQueue::objectScriptProcessingQueueCoro,
        mStartString,
        getDerivedHandle<LLFloaterScriptQueue>(),
        mObjectList,
        fn));

    return true;
}

///----------------------------------------------------------------------------
/// Class LLFloaterRunQueue
///----------------------------------------------------------------------------

LLFloaterRunQueue::LLFloaterRunQueue(const LLSD& key)
  : LLFloaterScriptQueue(key)
{
	setTitle(LLTrans::getString("RunQueueTitle"));
	setStartString(LLTrans::getString("RunQueueStart"));
}

LLFloaterRunQueue::~LLFloaterRunQueue()
{ 
}

/// This is a utility function to be bound and called from objectScriptProcessingQueueCoro.
/// Do not call directly. It may throw a LLCheckedHandle<>::Stale exception.
bool LLFloaterRunQueue::runObjectScripts(LLHandle<LLFloaterScriptQueue> hfloater,
    const LLPointer<LLViewerObject> &object, LLInventoryObject* inventory, LLEventPump &pump)
{
    LLCheckedHandle<LLFloaterScriptQueue> floater(hfloater);
    // Dereferencing floater may fail. If they do they throw LLExeceptionStaleHandle.
    // which is caught in objectScriptProcessingQueueCoro

    std::string buffer;
    buffer = floater->getString("Running") + (": ") + inventory->getName();
    floater->addStringMessage(buffer);

    LLMessageSystem* msg = gMessageSystem;
    msg->newMessageFast(_PREHASH_SetScriptRunning);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    msg->nextBlockFast(_PREHASH_Script);
    msg->addUUIDFast(_PREHASH_ObjectID, object->getID());
    msg->addUUIDFast(_PREHASH_ItemID, inventory->getUUID());
    msg->addboolFast(_PREHASH_Running, true);
    msg->sendReliable(object->getRegion()->getHost());

    return true;
}

bool LLFloaterRunQueue::startQueue()
{
    LLHandle<LLFloaterScriptQueue> hFloater(getDerivedHandle<LLFloaterScriptQueue>());
    fnQueueAction_t fn = boost::bind(LLFloaterRunQueue::runObjectScripts, hFloater, _1, _2, _3);

    LLCoros::instance().launch("ScriptRunQueue", boost::bind(LLFloaterScriptQueue::objectScriptProcessingQueueCoro,
        mStartString,
        hFloater,
        mObjectList,
        fn));

    return true;
}


///----------------------------------------------------------------------------
/// Class LLFloaterNotRunQueue
///----------------------------------------------------------------------------

LLFloaterNotRunQueue::LLFloaterNotRunQueue(const LLSD& key)
  : LLFloaterScriptQueue(key)
{
	setTitle(LLTrans::getString("NotRunQueueTitle"));
	setStartString(LLTrans::getString("NotRunQueueStart"));
}

LLFloaterNotRunQueue::~LLFloaterNotRunQueue()
{ 
}

/// This is a utility function to be bound and called from objectScriptProcessingQueueCoro.
/// Do not call directly. It may throw a LLCheckedHandle<>::Stale exception.
bool LLFloaterNotRunQueue::stopObjectScripts(LLHandle<LLFloaterScriptQueue> hfloater, 
    const LLPointer<LLViewerObject> &object, LLInventoryObject* inventory, LLEventPump &pump)
{
    LLCheckedHandle<LLFloaterScriptQueue> floater(hfloater);
    // Dereferencing floater may fail. If they do they throw LLExeceptionStaleHandle.
    // which is caught in objectScriptProcessingQueueCoro

    std::string buffer;
    buffer = floater->getString("NotRunning") + (": ") + inventory->getName();
    floater->addStringMessage(buffer);

    LLMessageSystem* msg = gMessageSystem;
    msg->newMessageFast(_PREHASH_SetScriptRunning);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    msg->nextBlockFast(_PREHASH_Script);
    msg->addUUIDFast(_PREHASH_ObjectID, object->getID());
    msg->addUUIDFast(_PREHASH_ItemID, inventory->getUUID());
    msg->addboolFast(_PREHASH_Running, false);
    msg->sendReliable(object->getRegion()->getHost());

    return true;
}

bool LLFloaterNotRunQueue::startQueue()
{
    LLHandle<LLFloaterScriptQueue> hFloater(getDerivedHandle<LLFloaterScriptQueue>());

    fnQueueAction_t fn = boost::bind(&LLFloaterNotRunQueue::stopObjectScripts, hFloater, _1, _2, _3);
    LLCoros::instance().launch("ScriptQueueNotRun", boost::bind(LLFloaterScriptQueue::objectScriptProcessingQueueCoro,
        mStartString,
        hFloater,
        mObjectList,
        fn));

    return true;
}

///----------------------------------------------------------------------------
/// Class LLFloaterLocateQueue
///----------------------------------------------------------------------------

LLFloaterLocateQueue::LLFloaterLocateQueue(const LLSD& key)
  : LLFloaterScriptQueue(key)
{
	setTitle(LLTrans::getString("LocateQueueTitle"));
	setStartString(LLTrans::getString("LocateQueueStart"));
}

LLFloaterLocateQueue::~LLFloaterLocateQueue()
{ 
}

// static (passed on by llpreviewscript)
void LLFloaterLocateQueue::processScriptRunningReply(LLMessageSystem* msg)
{
	// KKA-678 We're getting occasional exceptions thrown in the vicinity of this
	// routine when a script is opened for editing even though compile_queue_floater_handles 
	// is (should be) empty at the time.
	// That said, the use of LLCheckedHandle really should have a landing pad
	// so convert this to a try/catch structure. The overlay paranoid debug
	// output is there in case the problem somehow still occurs.
	try
	{
		LL_INFOS() << "entry" << LL_ENDL;
		LLHandle<LLFloaterScriptQueue> hfloater;
		LLUUID item_id;
		LLUUID object_id;
		
		// KKA-678 add protection for an empty list since this is static
		// and could get called whilst there are no floaters in existence
		if (! compile_queue_floater_handles.empty())
		{
			LL_INFOS() << "map not empty" << LL_ENDL;
			msg->getUUIDFast(_PREHASH_Script, _PREHASH_ObjectID, object_id);
			msg->getUUIDFast(_PREHASH_Script, _PREHASH_ItemID, item_id);
			
			CQMAP::iterator it;
				
			it = compile_queue_floater_handles.find(object_id.asString());
			if (it != compile_queue_floater_handles.end())
			{
				LL_INFOS() << "matched object id" << LL_ENDL;
				hfloater=compile_queue_floater_handles[object_id.asString()];
				LLCheckedHandle<LLFloaterScriptQueue> floater(hfloater);
				
				bool running;
				msg->getboolFast(_PREHASH_Script, _PREHASH_Running, running);
				floater->setIsRunning(running);

				bool mono;
				msg->getboolFast(_PREHASH_Script, "Mono", mono);
				floater->setIsMono(mono);

				floater->setWaiting(false);
				LL_INFOS() << "done object handling" << LL_ENDL;
			}
		}
	}
  catch (LLCheckedHandleBase::Stale &)
  {
      // This is expected.  It means that floater has been closed before 
      // processing was completed.
      LL_INFOS() << "LLExeceptionStaleHandle caught in processScriptRunningReply! Floater has most likely been closed." << LL_ENDL;
  }
	LL_INFOS() << "exit" << LL_ENDL;
}

/// This is a utility function to be bound and called from objectScriptProcessingQueueCoro.
/// Do not call directly. It may throw a LLCheckedHandle<>::Stale exception.
bool LLFloaterLocateQueue::locateObjectScripts(LLHandle<LLFloaterScriptQueue> hfloater, 
    const LLPointer<LLViewerObject> &object, LLInventoryObject* inventory, LLEventPump &pump)
{
    LLCheckedHandle<LLFloaterScriptQueue> floater(hfloater);
    // Dereferencing floater may fail. If they do they throw LLExeceptionStaleHandle.
    // which is caught in objectScriptProcessingQueueCoro

    if (floater->getFull())
    {
				LLMessageSystem* msg = gMessageSystem;
				msg->newMessageFast(_PREHASH_GetScriptRunning);
				msg->nextBlockFast(_PREHASH_Script);
				msg->addUUIDFast(_PREHASH_ObjectID, object->getID());
				msg->addUUIDFast(_PREHASH_ItemID, inventory->getUUID());
				
				floater->setWaiting(true);		
				msg->sendReliable(object->getRegion()->getHost());
				
				time_t begin = time(NULL);
				while (floater->getWaiting() && difftime(time(NULL),begin) < 30) llcoro::suspend();
				
				if (floater->getWaiting())
				{
						//waiting flag still set, so we timed out
						floater->setWaiting(false);

            LLStringUtil::format_map_t args;
            args["[OBJECT_NAME]"] = inventory->getName();
            // we're borrowing an existing text string here so it doesn't have the usual hyphen prefix
            floater->addStringMessage("- "+floater->getString("Timeout", args));
				}
				else
				{	
            LLStringUtil::format_map_t args;
            args["[STATUS]"] = floater->getString("Status");
						args["[ITEM]"] = inventory->getName();
						if (floater->getIsRunning()) args["[RUNNING]"] = floater->getString("StatusRunning");
						else args["[RUNNING]"] = floater->getString("StatusStopped");
						if (floater->getIsMono()) args["[MONOLSL]"] = floater->getString("StatusMono");
						else args["[MONOLSL]"] = floater->getString("StatusLSL");
            floater->addStringMessage(floater->getString("ItemStatus", args));
				}
		}
    else
    {
		    std::string buffer;
		    buffer = floater->getString("Located") + (": ") + inventory->getName();
		    floater->addStringMessage(buffer);
		}
					
    return true;
}

bool LLFloaterLocateQueue::startQueue()
{
    LLHandle<LLFloaterScriptQueue> hFloater(getDerivedHandle<LLFloaterScriptQueue>());

    fnQueueAction_t fn = boost::bind(&LLFloaterLocateQueue::locateObjectScripts, hFloater, _1, _2, _3);
    LLCoros::instance().launch("ScriptQueueLocate", boost::bind(LLFloaterScriptQueue::objectScriptProcessingQueueCoro,
        mStartString,
        hFloater,
        mObjectList,
        fn));

    return true;
}

///----------------------------------------------------------------------------
/// Class LLFloaterDeleteQueue
///----------------------------------------------------------------------------

LLFloaterDeleteQueue::LLFloaterDeleteQueue(const LLSD& key)
  : LLFloaterScriptQueue(key)
{
	setTitle(LLTrans::getString("DeleteQueueTitle"));
	setStartString(LLTrans::getString("DeleteQueueStart"));
}

LLFloaterDeleteQueue::~LLFloaterDeleteQueue()
{ 
}

/// This is a utility function to be bound and called from objectScriptProcessingQueueCoro.
/// Do not call directly. It may throw a LLCheckedHandle<>::Stale exception.
bool LLFloaterDeleteQueue::deleteObjectScripts(LLHandle<LLFloaterScriptQueue> hfloater,
    const LLPointer<LLViewerObject> &object, LLInventoryObject* inventory, LLEventPump &pump)
{
    LLCheckedHandle<LLFloaterScriptQueue> floater(hfloater);
    // Dereferencing floater may fail. If they do they throw LLExeceptionStaleHandle.
    // which is caught in objectScriptProcessingQueueCoro

    std::string buffer;
   	std::string scriptname = inventory->getName();
   		
   	if (!floater->getDeleteOnly() || (floater->getDeleteOnly() && floater->getDeleteName() == scriptname))
   	{
		    buffer = floater->getString("Deleting") + (": ") + scriptname;

		    LLMessageSystem* msg = gMessageSystem;
		    msg->newMessageFast(_PREHASH_RemoveTaskInventory);
		    msg->nextBlockFast(_PREHASH_AgentData);
		    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		    msg->nextBlockFast(_PREHASH_InventoryData);
		    msg->addU32Fast(_PREHASH_LocalID, object->getLocalID());
		    msg->addUUIDFast(_PREHASH_ItemID, inventory->getUUID());
		    msg->sendReliable(object->getRegion()->getHost());
		}
		else
		{
		    buffer = floater->getString("Skipping") + (": ") + scriptname;
		}
		floater->addStringMessage(buffer);
		
    return true;
}

bool LLFloaterDeleteQueue::startQueue()
{
    LLHandle<LLFloaterScriptQueue> hFloater(getDerivedHandle<LLFloaterScriptQueue>());
    fnQueueAction_t fn = boost::bind(LLFloaterDeleteQueue::deleteObjectScripts, hFloater, _1, _2, _3);

    LLCoros::instance().launch("ScriptDeleteQueue", boost::bind(LLFloaterScriptQueue::objectScriptProcessingQueueCoro,
        mStartString,
        hFloater,
        mObjectList,
        fn));

    return true;
}

///----------------------------------------------------------------------------
/// Local function definitions
///----------------------------------------------------------------------------
void ObjectInventoryFetcher::inventoryChanged(LLViewerObject* object,
        LLInventoryObject::object_list_t* inventory, S32 serial_num, void* user_data)
{
    mInventoryList.clear();
    mInventoryList.assign(inventory->begin(), inventory->end());

    mPump.post(LLSDMap("changed", LLSD::Boolean(true)));

}

void LLFloaterScriptQueue::objectScriptProcessingQueueCoro(std::string action, LLHandle<LLFloaterScriptQueue> hfloater,
    object_data_list_t objectList, fnQueueAction_t func)
{
    LLCoros::set_consuming(true);
    LLCheckedHandle<LLFloaterScriptQueue> floater(hfloater);    
    // Dereferencing floater may fail. If they do they throw LLExeceptionStaleHandle. 
    // This is expected if the dialog closes.
    LLEventMailDrop        maildrop(QUEUE_EVENTPUMP_NAME, true);
    U32 object_count = 0;
    U32 this_script_count = 0;
    U32 object_script_count=0;
    floater->clearScriptTotals();


    try
    {
        for (object_data_list_t::iterator itObj(objectList.begin()); (itObj != objectList.end()); ++itObj)
        {
            bool firstForObject = true;
            
            object_count++;
            this_script_count = 0;
            
            LLUUID object_id = (*itObj).mObjectId;
            compile_queue_floater_handles[object_id.asString()] = hfloater;
            LL_INFOS("SCRIPTQ") << "Next object in queue with ID=" << object_id.asString() << LL_ENDL;

            LLPointer<LLViewerObject> obj = gObjectList.findObject(object_id);
            LLInventoryObject::object_list_t inventory;
            if (obj)
            {
                ObjectInventoryFetcher::ptr_t fetcher(new ObjectInventoryFetcher(maildrop, obj, NULL));

                fetcher->fetchInventory();
                LLStringUtil::format_map_t args;
                // we don't do our own name lookup here, we rely on it being passed in so we use
                // slightly different messages if we don't have a name
                args["[OBJECT_NAME]"] = (*itObj).mObjectName;
                                    
                // include the link number (link number deriving routine adapated
                // from FS additions in the edit floater) However if we find it's not a linkset
                // display the original message instead to reduce visual clutter
                
                // it appears that the ObjectList we're iterating is reverse ordered in
                // linkset numbering, but let's not rely on that... don't change the order
                // to ascending - when performing operations it makes sense to do all the
                // children before the root since the root is often the main item from a
                // scripting point of view and it's best to kick that last after operating
                // on all the subsidiary scripts
                
								if (obj && obj->getRootEdit())
								{
									LLViewerObject::child_list_t children = obj->getRootEdit()->getChildren();
									if (children.empty())
									{
										// no linked prims so use the original message
										if ( (*itObj).mObjectName == "" ) floater->addStringMessage(floater->getString("LoadingObjInvNoName", args));
		                else floater->addStringMessage(floater->getString("LoadingObjInv", args));
									}
									else if (obj->getRootEdit() == obj)
									{
										// has children but no parent, so it's a root object
										args["[LINKID]"] = "1";
										if ( (*itObj).mObjectName == "") floater->addStringMessage(floater->getString("LoadingObjInvLinkNoName", args));
		                else floater->addStringMessage(floater->getString("LoadingObjInvLink", args));
									}
									else
									{										
										S32 index = 1;
										for (LLViewerObject::child_list_t::iterator iter = children.begin(); iter != children.end(); ++iter)
										{
											index++;
											if ((*iter)->getID() == obj->getID())
											{
												args["[LINKID]"] = llformat ("%d", index);
												if ( (*itObj).mObjectName == "") floater->addStringMessage(floater->getString("LoadingObjInvLinkNoName", args));
				                else floater->addStringMessage(floater->getString("LoadingObjInvLink", args));
												break;
											}
										}
									}
								}                

								// no longer displayed, replacement message produced by above code
                //LLStringUtil::format_map_t args;
                //args["[OBJECT_NAME]"] = (*itObj).mObjectName;
                //floater->addStringMessage(floater->getString("LoadingObjInv", args));

                LLSD result = llcoro::suspendUntilEventOnWithTimeout(maildrop, QUEUE_INVENTORY_FETCH_TIMEOUT,
                    LLSDMap("timeout", LLSD::Boolean(true)));

                if (result.has("timeout"))
                {    // A timeout filed in the result will always be true if present.
                    LL_WARNS("SCRIPTQ") << "Unable to retrieve inventory for object " << object_id.asString() <<
                        ". Skipping to next object." << LL_ENDL;

                    LLStringUtil::format_map_t args;
                    args["[OBJECT_NAME]"] = (*itObj).mObjectName;
                    floater->addStringMessage(floater->getString("Timeout", args));

                    continue;
                }

                inventory.assign(fetcher->getInventoryList().begin(), fetcher->getInventoryList().end());
            }
            else
            {
                LL_WARNS("SCRIPTQ") << "Unable to retrieve object with ID of " << object_id <<
                    ". Skipping to next." << LL_ENDL;
                continue;
            }

						// Chorazin: Warning - the commented code below causes a crash, so that'll be why it's commented...
            // TODO: Get the name of the object we are looking at here so that we can display it below.
            //std::string objName = (dynamic_cast<LLInventoryObject *>(obj.get()))->getName();
            LL_DEBUGS("SCRIPTQ") << "Object has " << inventory.size() << " items." << LL_ENDL;

            for (LLInventoryObject::object_list_t::iterator itInv = inventory.begin();
                itInv != inventory.end(); ++itInv)
            {
                floater.check();

                // note, we have a smart pointer to the obj above... but if we didn't we'd check that 
                // it still exists here.

                if (((*itInv)->getType() == LLAssetType::AT_LSL_TEXT))
                {
		                this_script_count++;
		                object_script_count++;

                    LL_DEBUGS("SCRIPTQ") << "Inventory item " << (*itInv)->getUUID().asString() << "\"" << (*itInv)->getName() << "\"" << LL_ENDL;
                    if (firstForObject)
                    {
                        //floater->addStringMessage(objName + ":");
                        firstForObject = false;
                    }

                    if (!func(obj, (*itInv), maildrop))
                    {
                        continue;
                    }
                }

                // no other explicit suspension point in this loop.  func(...) MIGHT suspend
                // but offers no guarantee of doing so.
                llcoro::suspend();
            }
            floater.check();
            // only emit if this item had any scripts to save display clutter
            if (this_script_count)
            {
			        LLStringUtil::format_map_t args;
			        args["[THIS]"] = llformat ("%d", this_script_count);
			        args["[TOTAL]"] = llformat ("%d", object_script_count);
			        std::string buffer = floater->getString("KokuaExtDoneOne", args);
			        floater->addStringMessage(buffer);
			      }
        }

				// replaced by more detailed output below
        //floater->addStringMessage("Done");
		    LLStringUtil::format_map_t args;
		    std::string buffer;

        args["[COUNT]"] = llformat ("%d", object_count);
        args["[TOTAL]"] = llformat ("%d", object_script_count);
        buffer = floater->getString("KokuaExtDone", args);
        floater->addStringMessage(buffer);

        if (floater->getFull())
        {
		        args["[LSLTOTAL]"] = llformat ("%d", floater->getLSLCount());
		        args["[MONOTOTAL]"] = llformat ("%d", floater->getMonoCount());
		        args["[RUNNINGTOTAL]"] = llformat ("%d", floater->getRunningCount());
		        buffer = floater->getString("KokuaExtFullDone", args);
		        floater->addStringMessage(buffer);
		    }
            
        floater->getChildView("close")->setEnabled(true);
    }
    catch (LLCheckedHandleBase::Stale &)
    {
        // This is expected.  It means that floater has been closed before 
        // processing was completed.
        LL_INFOS("SCRIPTQ") << "LLExeceptionStaleHandle caught! Floater has most likely been closed." << LL_ENDL;
    }
}
