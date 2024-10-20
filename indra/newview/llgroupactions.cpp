/** 
 * @file llgroupactions.cpp
 * @brief Group-related actions (join, leave, new, delete, etc)
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

#include "llgroupactions.h"

#include "message.h"

#include "llagent.h"
#include "llcommandhandler.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llgroupmgr.h"
#include "llfloaterimcontainer.h"
#include "llimview.h" // for gIMMgr
#include "llnotificationsutil.h"
#include "llstartup.h"
#include "llstatusbar.h"	// can_afford_transaction()
#include "groupchatlistener.h"
#include "exogroupmutelist.h"
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2011-01-23 (Catznip-3.0)
#include "llpanelgroup.h"
#include "llviewercontrol.h"
// [/SL:KB]
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2012-10-17 (Catznip-3.3)
#include "llviewercontrol.h"
// [/SL:KB]

//
// Globals
//
static GroupChatListener sGroupChatListener;

class LLGroupHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLGroupHandler() : LLCommandHandler("group", UNTRUSTED_THROTTLE) { }

    virtual bool canHandleUntrusted(
        const LLSD& params,
        const LLSD& query_map,
        LLMediaCtrl* web,
        const std::string& nav_type)
    {
        if (params.size() < 1)
        {
            return true; // don't block, will fail later
        }

        if (nav_type == NAV_TYPE_CLICKED
            || nav_type == NAV_TYPE_EXTERNAL)
        {
            return true;
        }

        const std::string verb = params[0].asString();
		return (verb != "create");
    }

	bool handle(const LLSD& tokens,
                const LLSD& query_map,
                const std::string& grid,
                LLMediaCtrl* web)
	{
		if (LLStartUp::getStartupState() < STATE_STARTED)
		{
			return true;
		}

		if (!LLUI::getInstance()->mSettingGroups["config"]->getbool("EnableGroupInfo"))
		{
			LLNotificationsUtil::add("NoGroupInfo", LLSD(), LLSD(), std::string("SwitchToStandardSkinAndQuit"));
			return true;
		}

		if (tokens.size() < 1)
		{
			return false;
		}

		if (tokens[0].asString() == "create")
		{
			LLGroupActions::createGroup();
			return true;
		}

		if (tokens.size() < 2)
		{
			return false;
		}

		if (tokens[0].asString() == "list")
		{
			if (tokens[1].asString() == "show")
			{
				LLSD params;
				params["people_panel_tab_name"] = "groups_panel";
				LLFloaterSidePanelContainer::showPanel("people", "panel_people", params);
				return true;
			}
            return false;
		}

		LLUUID group_id;
		if (!group_id.set(tokens[0], false))
		{
			return false;
		}

		if (tokens[1].asString() == "about")
		{
			if (group_id.isNull())
				return true;

			LLGroupActions::show(group_id);

			return true;
		}
		if (tokens[1].asString() == "inspect")
		{
			if (group_id.isNull())
				return true;
			LLGroupActions::inspect(group_id);
			return true;
		}
		return false;
	}
};
LLGroupHandler gGroupHandler;

// This object represents a pending request for specified group member information
// which is needed to check whether avatar can leave group
class LLFetchGroupMemberData : public LLGroupMgrObserver
{
public:
	LLFetchGroupMemberData(const LLUUID& group_id) : 
		mGroupId(group_id),
		mRequestProcessed(false),
		LLGroupMgrObserver(group_id) 
	{
		LL_INFOS() << "Sending new group member request for group_id: "<< group_id << LL_ENDL;
		LLGroupMgr* mgr = LLGroupMgr::getInstance();
		// register ourselves as an observer
		mgr->addObserver(this);
		// send a request
		mgr->sendGroupPropertiesRequest(group_id);
		mgr->sendCapGroupMembersRequest(group_id);
	}

	~LLFetchGroupMemberData()
	{
		if (!mRequestProcessed)
		{
			// Request is pending
			LL_WARNS() << "Destroying pending group member request for group_id: "
				<< mGroupId << LL_ENDL;
		}
		// Remove ourselves as an observer
		LLGroupMgr::getInstance()->removeObserver(this);
	}

	void changed(LLGroupChange gc)
	{
		if (gc == GC_PROPERTIES && !mRequestProcessed)
		{
			LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupId);
			if (!gdatap)
			{
				LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData() was NULL" << LL_ENDL;
			} 
			else if (!gdatap->isMemberDataComplete())
			{
				LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData()->isMemberDataComplete() was FALSE" << LL_ENDL;
				processGroupData();
				mRequestProcessed = true;
			}
		}
	}

	LLUUID getGroupId() { return mGroupId; }
	virtual void processGroupData() = 0;
protected:
	LLUUID mGroupId;
    bool mRequestProcessed;
};

class LLFetchLeaveGroupData: public LLFetchGroupMemberData
{
public:
	 LLFetchLeaveGroupData(const LLUUID& group_id)
		 : LLFetchGroupMemberData(group_id)
	 {}
	 void processGroupData()
	 {
		 LLGroupActions::processLeaveGroupDataResponse(mGroupId);
	 }
     void changed(LLGroupChange gc)
     {
         if (gc == GC_PROPERTIES && !mRequestProcessed)
         {
             LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupId);
             if (!gdatap)
             {
                 LL_WARNS() << "GroupData was NULL" << LL_ENDL;
             } 
             else
             {
                 processGroupData();
                 mRequestProcessed = true;
             }
         }
     }
};

LLFetchLeaveGroupData* gFetchLeaveGroupData = nullptr;

// static
void LLGroupActions::search()
{
	LLFloaterReg::showInstance("search", LLSD().with("collection", "groups"));
}

// static
void LLGroupActions::startCall(const LLUUID& group_id)
{
	// create a new group voice session
	LLGroupData gdata;

	if (!gAgent.getGroupData(group_id, gdata))
	{
		LL_WARNS() << "Error getting group data" << LL_ENDL;
		return;
	}

	LLUUID session_id = gIMMgr->addSession(gdata.mName, IM_SESSION_GROUP_START, group_id, true);
	if (session_id == LLUUID::null)
	{
		LL_WARNS() << "Error adding session" << LL_ENDL;
		return;
	}

	// start the call
	gIMMgr->autoStartCallOnStartup(session_id);

	make_ui_sound("UISndStartIM");
}

// static
void LLGroupActions::join(const LLUUID& group_id)
{
	if (!gAgent.canJoinGroups())
	{
		LLNotificationsUtil::add("JoinedTooManyGroups");
		return;
	}

	LLGroupMgrGroupData* gdatap = 
		LLGroupMgr::getInstance()->getGroupData(group_id);

	if (gdatap)
	{
		S32 cost = gdatap->mMembershipFee;
		LLSD args;
		args["COST"] = llformat("%d", cost);
		args["NAME"] = gdatap->mName;
		LLSD payload;
		payload["group_id"] = group_id;

		if (can_afford_transaction(cost))
		{
			if(cost > 0)
				LLNotificationsUtil::add("JoinGroupCanAfford", args, payload, onJoinGroup);
			else
				LLNotificationsUtil::add("JoinGroupNoCost", args, payload, onJoinGroup);
				
		}
		else
		{
			LLNotificationsUtil::add("JoinGroupCannotAfford", args, payload);
		}
	}
	else
	{
		LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData(" << group_id 
			<< ") was NULL" << LL_ENDL;
	}
}

// static
bool LLGroupActions::onJoinGroup(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (option == 1)
	{
		// user clicked cancel
		return false;
	}

	LLGroupMgr::getInstance()->
		sendGroupMemberJoin(notification["payload"]["group_id"].asUUID());
	return false;
}

// static
void LLGroupActions::leave(const LLUUID& group_id)
{
	if (group_id.isNull())
	{
		return;
	}

	LLGroupData group_data;
	if (gAgent.getGroupData(group_id, group_data))
	{
		LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_id);
		if (!gdatap || !gdatap->isMemberDataComplete())
		{
			if (gFetchLeaveGroupData != nullptr)
			{
				delete gFetchLeaveGroupData;
				gFetchLeaveGroupData = nullptr;
			}
			gFetchLeaveGroupData = new LLFetchLeaveGroupData(group_id);
		}
		else
		{
			processLeaveGroupDataResponse(group_id);
		}
	}
}

//static
void LLGroupActions::processLeaveGroupDataResponse(const LLUUID group_id)
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_id);
	LLUUID agent_id = gAgent.getID();
	LLGroupMgrGroupData::member_list_t::iterator mit = gdatap->mMembers.find(agent_id);
	//get the member data for the group
	if ( mit != gdatap->mMembers.end() )
	{
		LLGroupMemberData* member_data = (*mit).second;

		if ( member_data && member_data->isOwner() && gdatap->mMemberCount == 1)
		{
			LLNotificationsUtil::add("OwnerCannotLeaveGroup");
			return;
		}
	}
	LLSD args;
	args["GROUP"] = gdatap->mName;
	LLSD payload;
	payload["group_id"] = group_id;
	LLNotificationsUtil::add("GroupLeaveConfirmMember", args, payload, onLeaveGroup);
}

// static
void LLGroupActions::activate(const LLUUID& group_id)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ActivateGroup);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_GroupID, group_id);
	gAgent.sendReliableMessage();
}

//static bool isGroupUIVisible()
//{
//	static LLPanel* panel = 0;
//	if(!panel)
//		panel = LLSideTray::getInstance()->getPanel("panel_group_info_sidetray");
//	if(!panel)
//		return false;
//	return panel->isInVisibleChain();
//}
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2013-07-08 (Catznip-3.4)
typedef enum
{
	GV_FLOATER,   // Group is visible as a floater
	GV_SIDEPANEL, // Group is visible in the people sidepanel
	GV_NONE       // Group is not visible
} EGroupVisibility; 

static EGroupVisibility getGroupVisible(const LLUUID& idGroup)
{
	// Sanity check
	if (idGroup.isNull())
	{
		return GV_NONE;
	}

	// Check if we have the group open as a floater
	if (NULL != LLFloaterReg::findInstance("floater_group_info", idGroup))
	{
		return GV_FLOATER;
	}

	// Check if we have the group open in the people sidepanel
	static LLPanelGroup* s_pPanelGroupInfo = nullptr;
	if (!s_pPanelGroupInfo)
	{
		s_pPanelGroupInfo = LLFloaterSidePanelContainer::findPanel<LLPanelGroup>("people", "panel_group_info_sidetray");
	}
	return ( (s_pPanelGroupInfo) && (s_pPanelGroupInfo->isInVisibleChain()) && (idGroup == s_pPanelGroupInfo->getID()) ) ? GV_SIDEPANEL : GV_NONE;
}
// [/SL:KB]

// static 
void LLGroupActions::inspect(const LLUUID& group_id)
{
	LLFloaterReg::showInstance("inspect_group", LLSD().with("group_id", group_id));
}

// static
void LLGroupActions::show(const LLUUID& group_id)
{
	if (group_id.isNull())
		return;

// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2011-01-23 (Catznip-2.5)
	if (!gSavedSettings.getbool("ShowGroupFloaters"))
	{
// [/SL:KB]
		LLSD params;
		params["group_id"] = group_id;
		params["open_tab_name"] = "panel_group_info_sidetray";

		LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	    LLFloater *floater = LLFloaterReg::getTypedInstance<LLFloaterSidePanelContainer>("people");
	    if (!floater->isFrontmost())
	    {
	        floater->setVisibleAndFrontmost(true, params);
	    }
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2011-01-23 (Catznip-2.5)
	}
	else
	{
		LLFloater* pFloater = LLFloaterReg::getInstance("floater_group_info", group_id);
		if (pFloater)
		{
			pFloater->openFloater(LLSD().with("group_id", group_id));
		}
	}
// [/SL:KB]
}

// [SL:KB] - Patch: Notification-GroupCreateNotice | Checked: 2012-02-16 (Catznip-3.2)
// static
void LLGroupActions::showNotices(const LLUUID& group_id)
{
	if (group_id.isNull())
		return;

	LLSD sdParams;
	sdParams["group_id"] = group_id;
	sdParams["action"] = "view_notices";

// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2012-10-17 (Catznip-3.3)
	if (!gSavedSettings.getbool("ShowGroupFloaters"))
	{
// [/SL:KB]
		sdParams["open_tab_name"] = "panel_group_info_sidetray";

		LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", sdParams);
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2012-10-17 (Catznip-3.3)
	}
	else
	{
		LLFloaterReg::showInstance("floater_group_info", sdParams);
	}
// [/SL:KB]
}

// static
void LLGroupActions::viewChatHistory(const LLUUID& group_id)
{
	LLFloaterReg::showInstance("preview_conversation", group_id, true);
}
// [/SL:KB]

//void LLGroupActions::refresh_notices()
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2011-01-23 (Catznip-2.5)
void LLGroupActions::refresh_notices(const LLUUID& group_id)
// [/SL:KB]
{
//	if(!isGroupUIVisible())
//		return;
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2013-07-08 (Catznip-3.4)
	EGroupVisibility eGroupVisibility = getGroupVisible(group_id);
	if (GV_NONE == eGroupVisibility)
		return;
// [/SL:KB]

	LLSD params;
//	params["group_id"] = LLUUID::null;
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2011-01-23 (Catznip-2.5)
	params["group_id"] = group_id;
// [/SL:KB]
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "refresh_notices";

//	LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2011-01-23 (Catznip-2.5)
	if ( (!gSavedSettings.getbool("ShowGroupFloaters")) || (GV_SIDEPANEL == eGroupVisibility) )
	{
		LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	}
	else
	{
		LLFloater* pFloater = LLFloaterReg::getInstance("floater_group_info", group_id);
		if (pFloater)
		{
			pFloater->openFloater(params);
		}
	}
// [/SL:KB]
}

//static 
void LLGroupActions::refresh(const LLUUID& group_id)
{
//	if(!isGroupUIVisible())
//		return;
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2013-07-08 (Catznip-3.4)
	EGroupVisibility eGroupVisibility = getGroupVisible(group_id);
	if (GV_NONE == eGroupVisibility)
		return;
// [/SL:KB]

	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "refresh";

//	LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2011-01-23 (Catznip-2.5)
	if ( (!gSavedSettings.getbool("ShowGroupFloaters")) || (GV_SIDEPANEL == eGroupVisibility) )
	{
		LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	}
	else
	{
		LLFloater* pFloater = LLFloaterReg::getInstance("floater_group_info", group_id);
		if (pFloater)
		{
			pFloater->openFloater(params);
		}
	}
// [/SL:KB]
}

//static 
void LLGroupActions::createGroup()
{
	LLSD params;
	params["group_id"] = LLUUID::null;
	params["open_tab_name"] = "panel_group_creation_sidetray";
	params["action"] = "create";

	LLFloaterSidePanelContainer::showPanel("people", "panel_group_creation_sidetray", params);

}
//static
void LLGroupActions::closeGroup(const LLUUID& group_id)
{
//	if(!isGroupUIVisible())
//		return;
// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2013-07-08 (Catznip-3.4)
	EGroupVisibility eGroupVisibility = getGroupVisible(group_id);
	if (GV_NONE == eGroupVisibility)
		return;
// [/SL:KB]

	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "close";

	LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);

// [SL:KB] - Patch: UI-GroupFloaters | Checked: 2011-01-23 (Catznip-2.5)
	LLFloaterReg::hideInstance("floater_group_info", group_id);
// [/SL:KB]
}


// static
LLUUID LLGroupActions::startIM(const LLUUID& group_id)
{
	if (group_id.isNull()) return LLUUID::null;

	LLGroupData group_data;
	if (gAgent.getGroupData(group_id, group_data))
	{
		// <exodus>
		// Unmute the group if the user tries to start a session with it.
		exoGroupMuteList::instance().remove(group_id);
		// </exodus>
		LLUUID session_id = gIMMgr->addSession(
			group_data.mName,
			IM_SESSION_GROUP_START,
			group_id);
		if (session_id != LLUUID::null)
		{
			LLFloaterIMContainer::getInstance()->showConversation(session_id);
		}
		make_ui_sound("UISndStartIM");
		return session_id;
	}
	else
	{
		// this should never happen, as starting a group IM session
		// relies on you belonging to the group and hence having the group data
		make_ui_sound("UISndInvalidOp");
		return LLUUID::null;
	}
}

// static
void LLGroupActions::endIM(const LLUUID& group_id)
{
	if (group_id.isNull())
		return;
	
	LLUUID session_id = gIMMgr->computeSessionID(IM_SESSION_GROUP_START, group_id);
	if (session_id != LLUUID::null)
	{
		gIMMgr->leaveSession(session_id);
	}
}

// static
bool LLGroupActions::isInGroup(const LLUUID& group_id)
{
	// *TODO: Move all the LLAgent group stuff into another class, such as
	// this one.
	return gAgent.isInGroup(group_id);
}

// [SL:KB] - Patch: Notification-GroupCreateNotice | Checked: 2012-02-16 (Catznip-3.2)
// static
bool LLGroupActions::hasPowerInGroup(const LLUUID& group_id, U64 power)
{
	return gAgent.hasPowerInGroup(group_id, power);
}

// static
bool LLGroupActions::hasChatHistory(const LLUUID& group_id)
{
	return LLLogChat::isTranscriptExist(group_id, true);
}
// [/SL:KB]

// static
bool LLGroupActions::isAvatarMemberOfGroup(const LLUUID& group_id, const LLUUID& avatar_id)
{
	if(group_id.isNull() || avatar_id.isNull())
	{
		return false;
	}

	LLGroupMgrGroupData* group_data = LLGroupMgr::getInstance()->getGroupData(group_id);
	if(!group_data)
	{
		return false;
	}

	if(group_data->mMembers.end() == group_data->mMembers.find(avatar_id))
	{
		return false;
	}

	return true;
}

//-- Private methods ----------------------------------------------------------

// static
bool LLGroupActions::onLeaveGroup(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLUUID group_id = notification["payload"]["group_id"].asUUID();
	if(option == 0)
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_LeaveGroupRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_GroupData);
		msg->addUUIDFast(_PREHASH_GroupID, group_id);
		gAgent.sendReliableMessage();
	}
	return false;
}
