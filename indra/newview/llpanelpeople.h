/** 
 * @file llpanelpeople.h
 * @brief Side tray "People" panel
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

#ifndef LL_LLPANELPEOPLE_H
#define LL_LLPANELPEOPLE_H

#include <llpanel.h>
#include <llfloater.h>
#include "llcallingcard.h" // for avatar tracker
#include "llfloaterwebcontent.h"
#include "llvoiceclient.h"
// [FS:CR] Contact sets
#include "lggcontactsets.h"
#include <boost/signals2.hpp>

class LLAvatarList;
class LLAvatarName;
class LLFilterEditor;
class LLGroupList;
class LLMenuButton;
class LLTabContainer;
class LLNetMap;

// Firestorm declarations
class LLMenuGL;
//class FSRadarListCtrl;

class LLPanelPeople 
	: public LLPanel
	, public LLVoiceClientStatusObserver
{
	LOG_CLASS(LLPanelPeople);
public:
	LLPanelPeople();
	virtual ~LLPanelPeople();

	/*virtual*/ bool 	postBuild();
	/*virtual*/ void	onOpen(const LLSD& key);
	/*virtual*/ bool	notifyChildren(const LLSD& info);
	// Implements LLVoiceClientStatusObserver::onChange() to enable call buttons
	// when voice is available
	/*virtual*/ void onChange(EStatusType status, const std::string &channelURI, bool proximal);

	// <FS:Ansariel> Firestorm radar
	void updateNearby(const std::vector<LLSD>& entries, const LLSD& stats);

	// internals
	class Updater;

	bool updateNearbyArrivalTime();

private:

	typedef enum e_sort_oder {
		E_SORT_BY_NAME = 0,
		E_SORT_BY_STATUS = 1,
		E_SORT_BY_MOST_RECENT = 2,
		E_SORT_BY_DISTANCE = 3,
		E_SORT_BY_RECENT_SPEAKERS = 4,
		E_SORT_BY_RECENT_ARRIVAL = 5
	} ESortOrder;

    void				    removePicker();

	// methods indirectly called by the updaters
//CA
	void					giveMessage(const LLUUID& agent_id, const LLAvatarName& av_name, const std::string& postMsg);
//ca
	void					updateFriendListHelpText();
	void					updateFriendList();
	void					updateNearbyList();
	void					updateRecentList();
//MK
    void                    updateNearbyRange();
//mk

	bool					isItemsFreeOfFriends(const uuid_vec_t& uuids);

	void					updateButtons();
	std::string				getActiveTabName() const;
	LLUUID					getCurrentItemID() const;
	void					getCurrentItemIDs(uuid_vec_t& selected_uuids) const;
//MK
    void                    reportToNearbyChat(std::string message);
//mk
	void					setSortOrder(LLAvatarList* list, ESortOrder order, bool save = true);

	// UI callbacks
	void					onFilterEdit(const std::string& search_string);
	void					onTabSelected(const LLSD& param);
	void					onAddFriendButtonClicked();
	void					onAddFriendWizButtonClicked();
	void					onDeleteFriendButtonClicked();
	void					onChatButtonClicked();
	void					onGearButtonClicked(LLUICtrl* btn);
	void					onImButtonClicked();
	void					onMoreButtonClicked();
	void					onAvatarListDoubleClicked(LLUICtrl* ctrl);
	void					onAvatarListCommitted(LLAvatarList* list);
	bool					onGroupPlusButtonValidate();
	void					onGroupMinusButtonClicked();
	void					onGroupPlusMenuItemClicked(const LLSD& userdata);

	void					onFriendsViewSortMenuItemClicked(const LLSD& userdata);
	void					onNearbyViewSortMenuItemClicked(const LLSD& userdata);
	void					onGroupsViewSortMenuItemClicked(const LLSD& userdata);
	void					onRecentViewSortMenuItemClicked(const LLSD& userdata);

	//returns false only if group is "none"
	bool					isRealGroup();
	bool					onFriendsViewSortMenuItemCheck(const LLSD& userdata);
	bool					onRecentViewSortMenuItemCheck(const LLSD& userdata);
	bool					onNearbyViewSortMenuItemCheck(const LLSD& userdata);

	void					onViewLoginNamesMenuItemToggle();
	bool					onViewLoginNamesMenuItemCheck();

	// misc callbacks
	static void				onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names);

	void					onFriendsAccordionExpandedCollapsed(LLUICtrl* ctrl, const LLSD& param, LLAvatarList* avatar_list);

	void					showAccordion(const std::string name, bool show);

	void					showFriendsAccordionsIfNeeded();

	void					onFriendListRefreshComplete(LLUICtrl*ctrl, const LLSD& param);

	void					setAccordionCollapsedByUser(LLUICtrl* acc_tab, bool collapsed);
	void					setAccordionCollapsedByUser(const std::string& name, bool collapsed);
	bool					isAccordionCollapsedByUser(LLUICtrl* acc_tab);
	bool					isAccordionCollapsedByUser(const std::string& name);

	// <FS:Ansariel> Firestorm callback handler
//	void					onRadarListDoubleClicked();
//	void					onGlobalVisToggleButtonClicked();
	// </FS:Ansariel> Firestorm callback handler

	LLFilterEditor*			mFilterEditor;
	LLTabContainer*			mTabContainer;
	LLAvatarList*			mOnlineFriendList;
	LLAvatarList*			mAllFriendList;
	LLAvatarList*			mNearbyList;
	LLAvatarList*			mContactSetList;	// [FS:CR] Contact sets
	LLAvatarList*			mRecentList;
	LLGroupList*			mGroupList;
	LLNetMap*				mMiniMap;

	std::vector<std::string> mSavedOriginalFilters;
	std::vector<std::string> mSavedFilters;

	Updater*				mFriendListUpdater;
	Updater*				mNearbyListUpdater;
	Updater*				mRecentListUpdater;
	Updater*				mButtonsUpdater;
    LLHandle< LLFloater >	mPicker;
    //MK
    LLMenuButton*            mNearbyGearButton;
    LLMenuButton*            mFriendsGearButton;
    LLMenuButton*            mGroupsGearButton;
    LLMenuButton*            mRecentGearButton;
    
    std::string                mFilterSubString;
    std::string                mFilterSubStringOrig;
    
    struct radarFields
    {
        std::string avName;
        F32 lastDistance;
        LLVector3d lastGlobalPos;
        LLUUID lastRegion;
        time_t firstSeen;
        S32 lastStatus;
    };
    std::map < LLUUID, radarFields > lastRadarSweep;
    //mk
	// [FS:CR] Contact sets
	bool					onContactSetsEnable(const LLSD& userdata);
	void					onContactSetsMenuItemClicked(const LLSD& userdata);
	void					handlePickerCallback(const uuid_vec_t& ids, const std::string& set);
	void					refreshContactSets();
	void					generateContactList(const std::string& contact_set);
	void					generateCurrentContactList();
	
	void					updateContactSets(LGGContactSets::EContactSetUpdate type);
	boost::signals2::connection mContactSetChangedConnection;
	LLComboBox* mContactSetCombo;
	// [/FS:CR]    
};

#endif //LL_LLPANELPEOPLE_H
