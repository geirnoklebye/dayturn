/**
* @file llpanelprofile.cpp
* @brief Profile panel implementation
*
* $LicenseInfo:firstyear=2022&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2022, Linden Research, Inc.
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
#include "llpanelprofile.h"

#include "llagent.h"
#include "llavataractions.h"
// [SL:KB] - World-Mute | Checked: Catznip-3.6
#include "llfloaterblocked.h"
// [/SL:KB]
#include "llfloaterreg.h"
#include "llcommandhandler.h"
#include "llnotificationsutil.h"
#include "llpanelpicks.h"
#include "llviewernetwork.h"

static const std::string PANEL_PICKS = "panel_picks";

std::string getProfileURL(const std::string& agent_name)
{
	std::string url = "[WEB_PROFILE_URL][AGENT_NAME]";
	LLSD subs;
	subs["WEB_PROFILE_URL"] = LLGridManager::getInstance()->getWebProfileURL();
	subs["AGENT_NAME"] = agent_name;
	url = LLWeb::expandURLSubstitutions(url, subs);
	LLStringUtil::toLower(url);
	return url;
}

class LLProfileHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLProfileHandler() : LLCommandHandler("profile", UNTRUSTED_THROTTLE) { }

	bool handle(const LLSD& params, const LLSD& query_map, const std::string& grid,
		LLMediaCtrl* web)
	{
		if (params.size() < 1) return false;
		std::string agent_name = params[0];
		LL_INFOS() << "Profile, agent_name " << agent_name << LL_ENDL;
		std::string url = getProfileURL(agent_name);
		LLWeb::loadURLInternal(url);

		return true;
	}
};
LLProfileHandler gProfileHandler;

class LLAgentHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLAgentHandler() : LLCommandHandler("agent", UNTRUSTED_THROTTLE) { }

	bool handle(const LLSD& params, const LLSD& query_map, const std::string& grid,
		LLMediaCtrl* web)
	{
		if (params.size() < 2) return false;
		LLUUID avatar_id;
		if (!avatar_id.set(params[0], FALSE))
		{
			return false;
		}

		const std::string verb = params[1].asString();
		if (verb == "about")
		{
			LLAvatarActions::showProfile(avatar_id);
			return true;
		}

		if (verb == "inspect")
		{
			LLFloaterReg::showInstance("inspect_avatar", LLSD().with("avatar_id", avatar_id));
			return true;
		}

		if (verb == "im")
		{
			LLAvatarActions::startIM(avatar_id);
			return true;
		}

		if (verb == "pay")
		{
			if (!LLUI::getInstance()->mSettingGroups["config"]->getbool("EnableAvatarPay"))
			{
				LLNotificationsUtil::add("NoAvatarPay", LLSD(), LLSD(), std::string("SwitchToStandardSkinAndQuit"));
				return true;
			}

			LLAvatarActions::pay(avatar_id);
			return true;
		}

		if (verb == "offerteleport")
		{
			LLAvatarActions::offerTeleport(avatar_id);
			return true;
		}

		if (verb == "requestfriend")
		{
			LLAvatarActions::requestFriendshipDialog(avatar_id);
			return true;
		}

		if (verb == "removefriend")
		{
			LLAvatarActions::removeFriendDialog(avatar_id);
			return true;
		}

		if (verb == "mute")
		{
			if (! LLAvatarActions::isBlocked(avatar_id))
			{
				LLAvatarActions::toggleBlock(avatar_id);
// [SL:KB] - World-Mute | Checked: Catznip-3.6
				LLFloaterBlocked::showMuteAndSelect(avatar_id);
// [/SL:KB]
			}
			return true;
		}

		if (verb == "unmute")
		{
			if (LLAvatarActions::isBlocked(avatar_id))
			{
				LLAvatarActions::toggleBlock(avatar_id);
			}
			return true;
		}

		if (verb == "block")
		{
			if (params.size() > 2)
			{
				const std::string object_name = LLURI::unescape(params[2].asString());
				LLMute mute(avatar_id, object_name, LLMute::OBJECT);
				LLMuteList::getInstance()->add(mute);
// [SL:KB] - World-Mute | Checked: Catznip-3.6
				LLFloaterBlocked::showMuteAndSelect(mute.mID);
// [/SL:KB]
//				LLPanelBlockedList::showPanelAndSelect(mute.mID);
			}
			return true;
		}

		if (verb == "unblock")
		{
			if (params.size() > 2)
			{
				const std::string object_name = params[2].asString();
				LLMute mute(avatar_id, object_name, LLMute::OBJECT);
				LLMuteList::getInstance()->remove(mute);
			}
			return true;
		}
		return false;
	}
};
LLAgentHandler gAgentHandler;


//-- LLPanelProfile::ChildStack begins ----------------------------------------
LLPanelProfile::ChildStack::ChildStack()
:	mParent(NULL)
{
}

LLPanelProfile::ChildStack::~ChildStack()
{
	while (mStack.size() != 0)
	{
		view_list_t& top = mStack.back();
		for (view_list_t::const_iterator it = top.begin(); it != top.end(); ++it)
		{
			LLView* viewp = *it;
			if (viewp)
			{
				viewp->die();
			}
		}
		mStack.pop_back();
	}
}

void LLPanelProfile::ChildStack::setParent(LLPanel* parent)
{
	llassert_always(parent != NULL);
	mParent = parent;
}

/// Save current parent's child views and remove them from the child list.
bool LLPanelProfile::ChildStack::push()
{
	view_list_t vlist = *mParent->getChildList();

	for (view_list_t::const_iterator it = vlist.begin(); it != vlist.end(); ++it)
	{
		LLView* viewp = *it;
		mParent->removeChild(viewp);
	}

	mStack.push_back(vlist);
	dump();
	return true;
}

/// Restore saved children (adding them back to the child list).
bool LLPanelProfile::ChildStack::pop()
{
	if (mStack.size() == 0)
	{
		LL_WARNS() << "Empty stack" << LL_ENDL;
		llassert(mStack.size() == 0);
		return false;
	}

	view_list_t& top = mStack.back();
	for (view_list_t::const_iterator it = top.begin(); it != top.end(); ++it)
	{
		LLView* viewp = *it;
		mParent->addChild(viewp);
	}

	mStack.pop_back();
	dump();
	return true;
}

/// Temporarily add all saved children back.
void LLPanelProfile::ChildStack::preParentReshape()
{
	mSavedStack = mStack;
	while(mStack.size() > 0)
	{
		pop();
	}
}

/// Add the temporarily saved children back.
void LLPanelProfile::ChildStack::postParentReshape()
{
	mStack = mSavedStack;
	mSavedStack = stack_t();

	for (stack_t::const_iterator stack_it = mStack.begin(); stack_it != mStack.end(); ++stack_it)
	{
		const view_list_t& vlist = (*stack_it);
		for (view_list_t::const_iterator list_it = vlist.begin(); list_it != vlist.end(); ++list_it)
		{
			LLView* viewp = *list_it;
			LL_DEBUGS() << "removing " << viewp->getName() << LL_ENDL;
			mParent->removeChild(viewp);
		}
	}
}

void LLPanelProfile::ChildStack::dump()
{
	unsigned lvl = 0;
	LL_DEBUGS() << "child stack dump:" << LL_ENDL;
	for (stack_t::const_iterator stack_it = mStack.begin(); stack_it != mStack.end(); ++stack_it, ++lvl)
	{
		std::ostringstream dbg_line;
		dbg_line << "lvl #" << lvl << ":";
		const view_list_t& vlist = (*stack_it);
		for (view_list_t::const_iterator list_it = vlist.begin(); list_it != vlist.end(); ++list_it)
		{
			dbg_line << " " << (*list_it)->getName();
		}
		LL_DEBUGS() << dbg_line.str() << LL_ENDL;
	}
}

//-- LLPanelProfile::ChildStack ends ------------------------------------------

LLPanelProfile::LLPanelProfile()
 : LLPanel()
 , mAvatarId(LLUUID::null)
{
	mChildStack.setParent(this);
}

bool LLPanelProfile::postBuild()
{
	LLPanelPicks* panel_picks = findChild<LLPanelPicks>(PANEL_PICKS);
	panel_picks->setProfilePanel(this);

	getTabContainer()[PANEL_PICKS] = panel_picks;

	return true;
}

// virtual
void LLPanelProfile::reshape(S32 width, S32 height, bool called_from_parent)
{
	// Temporarily add saved children back and reshape them.
	mChildStack.preParentReshape();
	LLPanel::reshape(width, height, called_from_parent);
	mChildStack.postParentReshape();
}

void LLPanelProfile::onOpen(const LLSD& key)
{
	getTabContainer()[PANEL_PICKS]->onOpen(getAvatarId());

	// support commands to open further pieces of UI
	if (key.has("show_tab_panel"))
	{
		std::string panel = key["show_tab_panel"].asString();
		if (panel == "create_classified")
		{
			LLPanelPicks* picks = dynamic_cast<LLPanelPicks *>(getTabContainer()[PANEL_PICKS]);
			if (picks)
			{
				picks->createNewClassified();
			}
		}
		else if (panel == "classified_details")
		{
			LLPanelPicks* picks = dynamic_cast<LLPanelPicks *>(getTabContainer()[PANEL_PICKS]);
			if (picks)
			{
				LLSD params = key;
				params.erase("show_tab_panel");
				params.erase("open_tab_name");
				picks->openClassifiedInfo(params);
			}
		}
		else if (panel == "edit_classified")
		{
			LLPanelPicks* picks = dynamic_cast<LLPanelPicks *>(getTabContainer()[PANEL_PICKS]);
			if (picks)
			{
				LLSD params = key;
				params.erase("show_tab_panel");
				params.erase("open_tab_name");
				picks->openClassifiedEdit(params);
			}
		}
		else if (panel == "create_pick")
		{
			LLPanelPicks* picks = dynamic_cast<LLPanelPicks *>(getTabContainer()[PANEL_PICKS]);
			if (picks)
			{
				picks->createNewPick();
			}
		}
		else if (panel == "edit_pick")
		{
			LLPanelPicks* picks = dynamic_cast<LLPanelPicks *>(getTabContainer()[PANEL_PICKS]);
			if (picks)
			{
				LLSD params = key;
				params.erase("show_tab_panel");
				params.erase("open_tab_name");
				picks->openPickEdit(params);
			}
		}
	}
}

void LLPanelProfile::onTabSelected(const LLSD& param)
{
	std::string tab_name = param.asString();
	if (NULL != getTabContainer()[tab_name])
	{
		getTabContainer()[tab_name]->onOpen(getAvatarId());
	}
}

void LLPanelProfile::openPanel(LLPanel* panel, const LLSD& params)
{
	// Hide currently visible panel (STORM-690).
	mChildStack.push();

	// Add the panel or bring it to front.
	if (panel->getParent() != this)
	{
		addChild(panel);
	}
	else
	{
		sendChildToFront(panel);
	}

	panel->setVisible(true);
	panel->setFocus(true); // prevent losing focus by the floater
	panel->onOpen(params);

	LLRect new_rect = getRect();
	panel->reshape(new_rect.getWidth(), new_rect.getHeight());
	new_rect.setLeftTopAndSize(0, new_rect.getHeight(), new_rect.getWidth(), new_rect.getHeight());
	panel->setRect(new_rect);
}

void LLPanelProfile::closePanel(LLPanel* panel)
{
	panel->setVisible(false);

	if (panel->getParent() == this) 
	{
		removeChild(panel);

		// Make the underlying panel visible.
		mChildStack.pop();

		// Prevent losing focus by the floater
		const child_list_t* child_list = getChildList();
		if (child_list->size() > 0)
		{
			child_list->front()->setFocus(true);
		}
		else
		{
			LL_WARNS() << "No underlying panel to focus." << LL_ENDL;
		}
	}
}

S32 LLPanelProfile::notifyParent(const LLSD& info)
{
	std::string action = info["action"];
	// lets update Picks list after Pick was saved
	if("save_new_pick" == action)
	{
		onOpen(info);
		return 1;
	}

	return LLPanel::notifyParent(info);
}
