/**
* @file llpanelprofile.h
* @brief Profile panel
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

#ifndef LL_LLPANELPROFILE_H
#define LL_LLPANELPROFILE_H

#include "llavatarpropertiesprocessor.h"
#include "llcallingcard.h"
#include "llfloater.h"
#include "llpanel.h"
#include "llpanelavatar.h"
#include "llmediactrl.h"
#include "llvoiceclient.h"

// class LLPanelProfileClassifieds;
// class LLTabContainer;

// class LLPanelProfileSecondLife;
// class LLPanelProfileWeb;
// class LLPanelProfilePicks;
// class LLPanelProfileFirstLife;
// class LLPanelProfileNotes;

class LLAvatarName;
class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLIconCtrl;
class LLTabContainer;
class LLTextBox;
class LLTextureCtrl;
class LLMediaCtrl;
class LLGroupList;
class LLTextBase;
class LLMenuButton;
class LLLineEditor;
class LLTextEditor;
class LLPanelProfileClassifieds;
class LLPanelProfilePicks;
class LLViewerFetchedTexture;

std::string getProfileURL(const std::string& agent_name);

/**
* Base class for Profile View and My Profile.
*/
class LLPanelProfile : public LLPanel
{
	LOG_CLASS(LLPanelProfile);

public:
	/*virtual*/ bool postBuild();
	/*virtual*/ void reshape(S32 width, S32 height, bool called_from_parent = true);
	/*virtual*/ void onOpen(const LLSD& key);

	virtual void openPanel(LLPanel* panel, const LLSD& params);

	virtual void closePanel(LLPanel* panel);

	S32 notifyParent(const LLSD& info);

protected:

	LLPanelProfile();

	virtual void onTabSelected(const LLSD& param);

	const LLUUID& getAvatarId() { return mAvatarId; }

	void setAvatarId(const LLUUID& avatar_id) { mAvatarId = avatar_id; }

	typedef std::map<std::string, LLPanelProfileTab*> profile_tabs_t;

	profile_tabs_t& getTabContainer() { return mTabContainer; }

private:

	//-- ChildStack begins ----------------------------------------------------
	class ChildStack
	{
		LOG_CLASS(LLPanelProfile::ChildStack);
	public:
		ChildStack();
		~ChildStack();
		void setParent(LLPanel* parent);

		bool push();
		bool pop();
		void preParentReshape();
		void postParentReshape();

	private:
		void dump();

		typedef LLView::child_list_t view_list_t;
		typedef std::list<view_list_t> stack_t;

		stack_t		mStack;
		stack_t		mSavedStack;
		LLPanel*	mParent;
	};
	//-- ChildStack ends ------------------------------------------------------

	profile_tabs_t mTabContainer;
	ChildStack		mChildStack;
	LLUUID mAvatarId;
};

#endif //LL_LLPANELPROFILE_H
