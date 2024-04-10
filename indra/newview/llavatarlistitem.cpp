/** 
 * @file llavatarlistitem.cpp
 * @brief avatar list item source file
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

#include <boost/signals2.hpp>

#include "llavataractions.h"
#include "llavatarlist.h"
#include "llavatarlistitem.h"

#include "llbutton.h"
#include "llfloaterreg.h"
#include "lltextutil.h"

#include "llagent.h"
#include "llavatarnamecache.h"
#include "llavatariconctrl.h"
#include "lloutputmonitorctrl.h"
#include "lltooldraganddrop.h"
#include "llviewercontrol.h"
#include "llworld.h"

#include "lggcontactsets.h"

bool LLAvatarListItem::sStaticInitialized = false;
S32 LLAvatarListItem::sLeftPadding = 0;
S32 LLAvatarListItem::sNameRightPadding = 0;
S32 LLAvatarListItem::sChildrenWidths[LLAvatarListItem::ALIC_COUNT];

static LLWidgetNameRegistry::StaticRegistrar sRegisterAvatarListItemParams(&typeid(LLAvatarListItem::Params), "avatar_list_item");

LLAvatarListItem::Params::Params()
:	default_style("default_style"),
	voice_call_invited_style("voice_call_invited_style"),
	voice_call_joined_style("voice_call_joined_style"),
	voice_call_left_style("voice_call_left_style"),
	online_style("online_style"),
	offline_style("offline_style"),
	name_right_pad("name_right_pad", 0)
{};


LLAvatarListItem::LLAvatarListItem(bool not_from_ui_factory/* = true*/)
:	LLPanel(),
	LLFriendObserver(),
	mAvatarIcon(nullptr),
	mAvatarName(nullptr),
	mLastInteractionTime(nullptr),
	mIconPermissionOnline(nullptr),
	mIconPermissionMap(nullptr),
	mIconPermissionEditMine(nullptr),
	mIconPermissionEditTheirs(nullptr),
	mSpeakingIndicator(nullptr),
	mInfoBtn(nullptr),
	mProfileBtn(nullptr),
	mOnlineStatus(E_UNKNOWN),
	mShowInfoBtn(true),
	mShowProfileBtn(true),
	mShowCompleteName(false),
//MK
////	mShowPermissions(false),
	mShowPermissions(true),
//mk
	mHovered(false),
	mShowDisplayName(true),
	mShowUsername(true),
	mAvatarNameCacheConnection(),
	mGreyOutUsername(""),
	mFirstSeen(time(NULL)),
	mAvStatus(0),
	mAvPosition(LLVector3d(0.0f,0.0f,0.0f)),
	mShowFirstSeen(false),
	mShowStatusFlags(false),
	mShowPaymentStatus(false),
	mPaymentStatus(nullptr),
	mAvatarAge(0),
 	//colouring based on contact sets
	mUseContactColors(false),
	mDistance(99999.9f) // arbitary large number to insure new avatars are considered outside close range until we know for sure.
{
	if (not_from_ui_factory)
	{
		buildFromFile("panel_avatar_list_item.xml");
	}
	// *NOTE: mantipov: do not use any member here. They can be uninitialized here in case instance
	// is created from the UICtrlFactory
}

LLAvatarListItem::~LLAvatarListItem()
{
	if (mAvatarId.notNull())
	{
		LLAvatarTracker::instance().removeParticularFriendObserver(mAvatarId, this);
	}

	if (mAvatarNameCacheConnection.connected())
	{
		mAvatarNameCacheConnection.disconnect();
	}
}

bool  LLAvatarListItem::postBuild()
{
	mAvatarIcon = getChild<LLAvatarIconCtrl>("avatar_icon");
	mAvatarName = getChild<LLTextBox>("avatar_name");
	mLastInteractionTime = getChild<LLTextBox>("last_interaction");

	mIconPermissionOnline = getChild<LLIconCtrl>("permission_online_icon");
	mIconPermissionMap = getChild<LLIconCtrl>("permission_map_icon");
	mIconPermissionEditMine = getChild<LLIconCtrl>("permission_edit_mine_icon");
	mIconPermissionEditTheirs = getChild<LLIconCtrl>("permission_edit_theirs_icon");
	mIconPermissionOnline->setVisible(false);
	mIconPermissionMap->setVisible(false);
	mIconPermissionEditMine->setVisible(false);
	mIconPermissionEditTheirs->setVisible(false);

	// radar
	mNearbyRange = getChild<LLTextBox>("radar_range");
	mNearbyRange->setValue("N/A");
	mNearbyRange->setVisible(false);
	mFirstSeenDisplay = getChild<LLTextBox>("first_seen");
	mFirstSeenDisplay->setValue("");
	mFirstSeenDisplay->setVisible(false);
	mAvatarAgeDisplay = getChild<LLTextBox>("avatar_age");
	mAvatarAgeDisplay->setVisible(false);
	mAvatarAgeDisplay->setValue("N/A");
	mPaymentStatus = getChild<LLIconCtrl>("payment_info");
	mPaymentStatus->setVisible(false);
	
	// TODO: Status flags

	mSpeakingIndicator = getChild<LLOutputMonitorCtrl>("speaking_indicator");
	mSpeakingIndicator->setChannelState(LLOutputMonitorCtrl::UNDEFINED_CHANNEL);
	mInfoBtn = getChild<LLButton>("info_btn");
	mProfileBtn = getChild<LLButton>("profile_btn");

	mInfoBtn->setVisible(false);
	mInfoBtn->setClickedCallback(boost::bind(&LLAvatarListItem::onInfoBtnClick, this));

	mProfileBtn->setVisible(false);
	mProfileBtn->setClickedCallback(boost::bind(&LLAvatarListItem::onProfileBtnClick, this));

	if (!sStaticInitialized)
	{
		// Remember children widths including their padding from the next sibling,
		// so that we can hide and show them again later.
		initChildrenWidths(this);

		// Right padding between avatar name text box and nearest visible child.
		sNameRightPadding = LLUICtrlFactory::getDefaultParams<LLAvatarListItem>().name_right_pad;

		sStaticInitialized = true;
	}

	return true;
}

void LLAvatarListItem::handleVisibilityChange ( bool new_visibility )
{
    //Adjust positions of icons (info button etc) when 
    //speaking indicator visibility was changed/toggled while panel was closed (not visible)
    if(new_visibility && mSpeakingIndicator->getIndicatorToggled())
    {
        updateChildren();
        mSpeakingIndicator->setIndicatorToggled(false);
    }
}

void LLAvatarListItem::fetchAvatarName()
{
	if (mAvatarId.notNull())
	{
		if (mAvatarNameCacheConnection.connected())
		{
			mAvatarNameCacheConnection.disconnect();
		}
		mAvatarNameCacheConnection = LLAvatarNameCache::get(getAvatarId(), boost::bind(&LLAvatarListItem::onAvatarNameCache, this, _2));
//MK
		showPermissions(mShowPermissions);
//mk
	}
}

S32 LLAvatarListItem::notifyParent(const LLSD& info)
{
	if (info.has("visibility_changed"))
	{
		updateChildren();
		return 1;
	}
	return LLPanel::notifyParent(info);
}

void LLAvatarListItem::onMouseEnter(S32 x, S32 y, MASK mask)
{
	getChildView("hovered_icon")->setVisible( true);
	mInfoBtn->setVisible(mShowInfoBtn);
	mProfileBtn->setVisible(mShowProfileBtn);

	mHovered = true;
	LLPanel::onMouseEnter(x, y, mask);

	showPermissions(mShowPermissions);
	updateChildren();
}

void LLAvatarListItem::onMouseLeave(S32 x, S32 y, MASK mask)
{
	getChildView("hovered_icon")->setVisible( false);
	mInfoBtn->setVisible(false);
	mProfileBtn->setVisible(false);

	mHovered = false;
	LLPanel::onMouseLeave(x, y, mask);

//MK
////	showPermissions(false);
//mk
	updateChildren();
}

// virtual, called by LLAvatarTracker
void LLAvatarListItem::changed(U32 mask)
{
	// no need to check mAvatarId for null in this case
	setOnline(LLAvatarTracker::instance().isBuddyOnline(mAvatarId));

	if (mask & LLFriendObserver::POWERS)
	{
//MK
////		showPermissions(mShowPermissions && mHovered);
		showPermissions(true);
//mk
		updateChildren();
	}
}

void LLAvatarListItem::setOnline(bool online)
{
	// *FIX: setName() overrides font style set by setOnline(). Not an issue ATM.

	if (mOnlineStatus != E_UNKNOWN && (bool) mOnlineStatus == online)
		return;

	mOnlineStatus = (EOnlineStatus) online;

	// Change avatar name font style depending on the new online status.
	setState(online ? IS_ONLINE : IS_OFFLINE);
}

void LLAvatarListItem::setAvatarName(const std::string& name)
{
	setNameInternal(name, mHighlihtSubstring);
}

void LLAvatarListItem::setAvatarToolTip(const std::string& tooltip)
{
	mAvatarName->setToolTip(tooltip);
}

void LLAvatarListItem::setHighlight(const std::string& highlight)
{
	setNameInternal(mAvatarName->getText(), mHighlihtSubstring = highlight);
}

void LLAvatarListItem::setState(EItemState item_style)
{
	const LLAvatarListItem::Params& params = LLUICtrlFactory::getDefaultParams<LLAvatarListItem>();

	switch(item_style)
	{
	default:
	case IS_DEFAULT:
		mAvatarNameStyle = params.default_style();
		break;
	case IS_VOICE_INVITED:
		mAvatarNameStyle = params.voice_call_invited_style();
		break;
	case IS_VOICE_JOINED:
		mAvatarNameStyle = params.voice_call_joined_style();
		break;
	case IS_VOICE_LEFT:
		mAvatarNameStyle = params.voice_call_left_style();
		break;
	case IS_ONLINE:
		mAvatarNameStyle = params.online_style();
		break;
	case IS_OFFLINE:
		mAvatarNameStyle = params.offline_style();
		break;
	}

	// *NOTE: You cannot set the style on a text box anymore, you must
	// rebuild the text.  This will cause problems if the text contains
	// hyperlinks, as their styles will be wrong.
	setNameInternal(mAvatarName->getText(), mHighlihtSubstring);

	icon_color_map_t& item_icon_color_map = getItemIconColorMap();
	mAvatarIcon->setColor(item_icon_color_map[item_style]);
}

void LLAvatarListItem::setAvatarId(const LLUUID& id, const LLUUID& session_id, bool ignore_status_changes/* = false*/, bool is_resident/* = true*/)
{
	if (mAvatarId.notNull())
		LLAvatarTracker::instance().removeParticularFriendObserver(mAvatarId, this);

	mAvatarId = id;
	mSpeakingIndicator->setSpeakerId(id, session_id);

	// We'll be notified on avatar online status changes
	if (!ignore_status_changes && mAvatarId.notNull())
		LLAvatarTracker::instance().addParticularFriendObserver(mAvatarId, this);

	if (is_resident)
	{
		mAvatarIcon->setValue(id);

		// Set avatar name.
		fetchAvatarName();
	}
}

void LLAvatarListItem::showLastInteractionTime(bool show)
{
	mLastInteractionTime->setVisible(show);
	updateChildren();
}

void LLAvatarListItem::setLastInteractionTime(U32 secs_since)
{
	mLastInteractionTime->setValue(formatSeconds(secs_since));
}

void LLAvatarListItem::setShowInfoBtn(bool show)
{
	mShowInfoBtn = show;
}

void LLAvatarListItem::setShowProfileBtn(bool show)
{
	mShowProfileBtn = show;
}

void LLAvatarListItem::showSpeakingIndicator(bool visible)
{
	// Already done? Then do nothing.
	if (mSpeakingIndicator->getVisible() == visible)
		return;
// Disabled to not contradict with SpeakingIndicatorManager functionality. EXT-3976
// probably this method should be totally removed.
//	mSpeakingIndicator->setVisible(visible);
//	updateChildren();
}

void LLAvatarListItem::showRange(bool show)
{
	mNearbyRange->setVisible(show);
}

void LLAvatarListItem::showFirstSeen(bool show)
{
	mFirstSeenDisplay->setVisible(show);
}

void LLAvatarListItem::showPaymentStatus(bool show)
{
	mPaymentStatus->setVisible(show);
////	updateAvatarProperties();
}

void LLAvatarListItem::showStatusFlags(bool show)
{
	mShowStatusFlags=show;
}

void LLAvatarListItem::updateFirstSeen(int nb /* = 5 */)
{
	S32 seentime = (S32)difftime(time(NULL), mFirstSeen);
	S32 hours = (S32)(seentime / 3600);
	S32 mins = (S32)((seentime - hours * 3600) / 60);
	S32 secs = (S32)((seentime - hours * 3600 - mins * 60));
	mFirstSeenDisplay->setValue(llformat("%d:%02d:%02d", hours, mins, secs));
	showPaymentStatus(nb >= 5);
	showAvatarAge(nb >= 4);
	showFirstSeen(nb >= 3);
	showRange(nb >= 2);
	updateChildren();
}

#define CHAT_WHISPER_RADIUS 10.0
#define CHAT_NORMAL_RADIUS 20.0
#define CHAT_SHOUT_RADIUS 100.0

void LLAvatarListItem::setRange(F32 distance)
{
	mDistance = distance;
}

F32 LLAvatarListItem::getRange()
{
	return mDistance;
}

void LLAvatarListItem::setPosition(LLVector3d pos)
{
	mAvPosition = pos;
}

LLVector3d LLAvatarListItem::getPosition()
{
	return mAvPosition;
}

void LLAvatarListItem::setAvStatus(S32 statusFlags)
{
	mAvStatus = statusFlags;
}

S32 LLAvatarListItem::getAvStatus()
{
	return mAvStatus;
}

time_t LLAvatarListItem::getFirstSeen()
{
	return mFirstSeen;
}

void LLAvatarListItem::setFirstSeen(time_t seentime)
{
	mFirstSeen = seentime;
}

void LLAvatarListItem::setAvatarIconVisible(bool visible)
{
	// Already done? Then do nothing.
	if (mAvatarIcon->getVisible() == visible)
	{
		return;
	}

	// Show/hide avatar icon.
	mAvatarIcon->setVisible(visible);
	updateChildren();
}

void LLAvatarListItem::showDisplayName(bool show)
{
	mShowDisplayName = show;
	updateAvatarName();
}

void LLAvatarListItem::showUsername(bool show)
{
	mShowUsername = show;
	updateAvatarName();
}

void LLAvatarListItem::onInfoBtnClick()
{
	LLFloaterReg::showInstance("inspect_avatar", LLSD().with("avatar_id", mAvatarId));
}

void LLAvatarListItem::onProfileBtnClick()
{
	LLAvatarActions::showProfile(mAvatarId);
}

bool LLAvatarListItem::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if(mInfoBtn->getRect().pointInRect(x, y))
	{
		onInfoBtnClick();
		return true;
	}
	if(mProfileBtn->getRect().pointInRect(x, y))
	{
		onProfileBtnClick();
		return true;
	}
	return LLPanel::handleDoubleClick(x, y, mask);
}

void LLAvatarListItem::setValue( const LLSD& value )
{
	if (!value.isMap()) return;;
	if (!value.has("selected")) return;
	getChildView("selected_icon")->setVisible( value["selected"]);
}

const LLUUID& LLAvatarListItem::getAvatarId() const
{
	return mAvatarId;
}

std::string LLAvatarListItem::getAvatarName() const
{
	return mAvatarName->getValue();
}

std::string LLAvatarListItem::getAvatarToolTip() const
{
	return mAvatarName->getToolTip();
}

void LLAvatarListItem::updateAvatarName()
{
	fetchAvatarName();
}

void LLAvatarListItem::showAvatarAge(bool display)
{
//MK
	// I need to deactivate this for now, because the observer for this datum tends to corrupt
	// the list of observers in LLAvatarPropertiesProcessor, leading to a quick crash very often.
	return;
////	mAvatarAgeDisplay->setVisible(display);
////	updateAvatarProperties();
//mk
}

void LLAvatarListItem::updateAvatarProperties()
{
	// NOTE: typically we request these once on creation to avoid excess traffic/processing. 
	//This means updates to these properties won't typically be seen while target is in nearby range.
	LLAvatarPropertiesProcessor* processor = LLAvatarPropertiesProcessor::getInstance();
	processor->addObserver(mAvatarId, this);
	processor->sendAvatarPropertiesRequest(mAvatarId);
}

//== PRIVATE SECITON ==========================================================


void LLAvatarListItem::processProperties(void* data, EAvatarProcessorType type)
{
	
	// route the data to the inspector
	if (data
		&& type == APT_PROPERTIES)
	{
		LLAvatarData* avatar_data = static_cast<LLAvatarData*>(data);
		mAvatarAge = ((LLDate::now().secondsSinceEpoch()  - (avatar_data->born_on).secondsSinceEpoch()) / 86400);
		mAvatarAgeDisplay->setValue(mAvatarAge);

		if (mShowPaymentStatus)
		{
			mPaymentStatus->setVisible(avatar_data->flags);
		}
		
	}
}


void LLAvatarListItem::setNameInternal(const std::string& name, const std::string& highlight)
{
		//colouring based on contact sets
		if (mUseContactColors)
		{
				// this is currently only intended to be used for the nearby list, so it doesn't matter
				// that we're losing the ability to show a name in grey for offline because everyone in
				// the nearby list must be online
				LGGContactSets* contactsets = LGGContactSets::getInstance();
				LLUIColorTable& colortable = LLUIColorTable::instance();

				LLStyle::Params contactHighlight = LLStyle::Params();

				LLColor4 name_color = colortable.getColor("AvatarListItemIconDefaultColor", LLColor4::white).get();
				name_color = contactsets->colorize(mAvatarId, name_color, LGG_CS_NEARBY);
				contactsets->hasFriendColorThatShouldShow(mAvatarId, LGG_CS_NEARBY, name_color);

				contactHighlight.color = name_color;
				mAvatarName->setText(name, contactHighlight);
		}
    else if(mShowCompleteName && highlight.empty())
    {
        LLTextUtil::textboxSetGreyedVal(mAvatarName, mAvatarNameStyle, name, mGreyOutUsername);
    }
    else
    {
        LLTextUtil::textboxSetHighlightedVal(mAvatarName, mAvatarNameStyle, name, highlight);
    }
}

void LLAvatarListItem::onAvatarNameCache(const LLAvatarName& av_name)
{
	mAvatarNameCacheConnection.disconnect();

	mGreyOutUsername = "";
	std::string name_string = mShowCompleteName? av_name.getCompleteName(false) : av_name.getDisplayName();
	if(av_name.getCompleteName() != av_name.getUserName())
	{
	    mGreyOutUsername = "[ " + av_name.getUserName(true) + " ]";
	    LLStringUtil::toLower(mGreyOutUsername);
	}
	setAvatarName(name_string);
	setAvatarToolTip(av_name.getUserName());

	//requesting the list to resort
	notifyParent(LLSD().with("sort", LLSD()));
}

// Convert given number of seconds to a string like "23 minutes", "15 hours" or "3 years",
// taking i18n into account. The format string to use is taken from the panel XML.
std::string LLAvatarListItem::formatSeconds(U32 secs)
{
	static const U32 LL_ALI_MIN		= 60;
	static const U32 LL_ALI_HOUR	= LL_ALI_MIN	* 60;
	static const U32 LL_ALI_DAY		= LL_ALI_HOUR	* 24;
	static const U32 LL_ALI_WEEK	= LL_ALI_DAY	* 7;
	static const U32 LL_ALI_MONTH	= LL_ALI_DAY	* 30;
	static const U32 LL_ALI_YEAR	= LL_ALI_DAY	* 365;

	std::string fmt; 
	U32 count = 0;

	if (secs >= LL_ALI_YEAR)
	{
		fmt = "FormatYears"; count = secs / LL_ALI_YEAR;
	}
	else if (secs >= LL_ALI_MONTH)
	{
		fmt = "FormatMonths"; count = secs / LL_ALI_MONTH;
	}
	else if (secs >= LL_ALI_WEEK)
	{
		fmt = "FormatWeeks"; count = secs / LL_ALI_WEEK;
	}
	else if (secs >= LL_ALI_DAY)
	{
		fmt = "FormatDays"; count = secs / LL_ALI_DAY;
	}
	else if (secs >= LL_ALI_HOUR)
	{
		fmt = "FormatHours"; count = secs / LL_ALI_HOUR;
	}
	else if (secs >= LL_ALI_MIN)
	{
		fmt = "FormatMinutes"; count = secs / LL_ALI_MIN;
	}
	else
	{
		fmt = "FormatSeconds"; count = secs;
	}

	LLStringUtil::format_map_t args;
	args["[COUNT]"] = llformat("%u", count);
	return getString(fmt, args);
}

// static
LLAvatarListItem::icon_color_map_t& LLAvatarListItem::getItemIconColorMap()
{
	static icon_color_map_t item_icon_color_map;
	if (!item_icon_color_map.empty()) return item_icon_color_map;

	item_icon_color_map.insert(
		std::make_pair(IS_DEFAULT,
		LLUIColorTable::instance().getColor("AvatarListItemIconDefaultColor", LLColor4::white)));

	item_icon_color_map.insert(
		std::make_pair(IS_VOICE_INVITED,
		LLUIColorTable::instance().getColor("AvatarListItemIconVoiceInvitedColor", LLColor4::white)));

	item_icon_color_map.insert(
		std::make_pair(IS_VOICE_JOINED,
		LLUIColorTable::instance().getColor("AvatarListItemIconVoiceJoinedColor", LLColor4::white)));

	item_icon_color_map.insert(
		std::make_pair(IS_VOICE_LEFT,
		LLUIColorTable::instance().getColor("AvatarListItemIconVoiceLeftColor", LLColor4::white)));

	item_icon_color_map.insert(
		std::make_pair(IS_ONLINE,
		LLUIColorTable::instance().getColor("AvatarListItemIconOnlineColor", LLColor4::white)));

	item_icon_color_map.insert(
		std::make_pair(IS_OFFLINE,
		LLUIColorTable::instance().getColor("AvatarListItemIconOfflineColor", LLColor4::white)));

	return item_icon_color_map;
}

// static
void LLAvatarListItem::initChildrenWidths(LLAvatarListItem* avatar_item)
{
	//speaking indicator width + padding
	S32 speaking_indicator_width = avatar_item->getRect().getWidth() - avatar_item->mSpeakingIndicator->getRect().mLeft;

	//profile btn width + padding
	S32 profile_btn_width = avatar_item->mSpeakingIndicator->getRect().mLeft - avatar_item->mProfileBtn->getRect().mLeft;

	//info btn width + padding
	S32 info_btn_width = avatar_item->mProfileBtn->getRect().mLeft - avatar_item->mInfoBtn->getRect().mLeft;

	// online permission icon width + padding
	S32 permission_online_width = avatar_item->mInfoBtn->getRect().mLeft - avatar_item->mIconPermissionOnline->getRect().mLeft;

	// map permission icon width + padding
	S32 permission_map_width = avatar_item->mIconPermissionOnline->getRect().mLeft - avatar_item->mIconPermissionMap->getRect().mLeft;

	// edit my objects permission icon width + padding
	S32 permission_edit_mine_width = avatar_item->mIconPermissionMap->getRect().mLeft - avatar_item->mIconPermissionEditMine->getRect().mLeft;

	// edit their objects permission icon width + padding
	S32 permission_edit_theirs_width = avatar_item->mIconPermissionEditMine->getRect().mLeft - avatar_item->mIconPermissionEditTheirs->getRect().mLeft;

	// last interaction time textbox width + padding
	S32 last_interaction_time_width = avatar_item->mIconPermissionEditTheirs->getRect().mLeft - avatar_item->mLastInteractionTime->getRect().mLeft;

	// avatar icon width + padding
	S32 icon_width = avatar_item->mAvatarName->getRect().mLeft - avatar_item->mAvatarIcon->getRect().mLeft;

	sLeftPadding = avatar_item->mAvatarIcon->getRect().mLeft;

	S32 index = ALIC_COUNT;
	sChildrenWidths[--index] = icon_width;
	sChildrenWidths[--index] = 0; // for avatar name we don't need its width, it will be calculated as "left available space"
	sChildrenWidths[--index] = last_interaction_time_width;
	sChildrenWidths[--index] = permission_edit_theirs_width;
	sChildrenWidths[--index] = permission_edit_mine_width;
	sChildrenWidths[--index] = permission_map_width;
	sChildrenWidths[--index] = permission_online_width;
	sChildrenWidths[--index] = info_btn_width;
	sChildrenWidths[--index] = profile_btn_width;
	sChildrenWidths[--index] = speaking_indicator_width;
	llassert(index == 0);
}

void LLAvatarListItem::updateChildren()
{
	LL_DEBUGS("AvatarItemReshape") << LL_ENDL;
	LL_DEBUGS("AvatarItemReshape") << "Updating for: " << getAvatarName() << LL_ENDL;

	S32 name_new_width = getRect().getWidth();
	S32 ctrl_new_left = name_new_width;
	S32 name_new_left = sLeftPadding;

	// iterate through all children and set them into correct position depend on each child visibility
	// assume that child indexes are in back order: the first in Enum is the last (right) in the item
	// iterate & set child views starting from right to left
	for (S32 i = 0; i < ALIC_COUNT; ++i)
	{
		// skip "name" textbox, it will be processed out of loop later
		if (ALIC_NAME == i) continue;

		LLView* control = getItemChildView((EAvatarListItemChildIndex)i);

		LL_DEBUGS("AvatarItemReshape") << "Processing control: " << control->getName() << LL_ENDL;
		// skip invisible views
		if (!control->getVisible()) continue;

		S32 ctrl_width = sChildrenWidths[i]; // including space between current & left controls

		// decrease available for 
		name_new_width -= ctrl_width;
		LL_DEBUGS("AvatarItemReshape") << "width: " << ctrl_width << ", name_new_width: " << name_new_width << LL_ENDL;

		LLRect control_rect = control->getRect();
		LL_DEBUGS("AvatarItemReshape") << "rect before: " << control_rect << LL_ENDL;

		if (ALIC_ICON == i)
		{
			// assume that this is the last iteration,
			// so it is not necessary to save "ctrl_new_left" value calculated on previous iterations
			ctrl_new_left = sLeftPadding;
			name_new_left = ctrl_new_left + ctrl_width;
		}
		else
		{
			ctrl_new_left -= ctrl_width;
		}

		LL_DEBUGS("AvatarItemReshape") << "ctrl_new_left: " << ctrl_new_left << LL_ENDL;

		control_rect.setLeftTopAndSize(
			ctrl_new_left,
			control_rect.mTop,
			control_rect.getWidth(),
			control_rect.getHeight());

		LL_DEBUGS("AvatarItemReshape") << "rect after: " << control_rect << LL_ENDL;
		control->setShape(control_rect);
	}

	// set size and position of the "name" child
	LLView* name_view = getItemChildView(ALIC_NAME);
	LLRect name_view_rect = name_view->getRect();
	LL_DEBUGS("AvatarItemReshape") << "name rect before: " << name_view_rect << LL_ENDL;

	// apply paddings
	name_new_width -= sLeftPadding;
	name_new_width -= sNameRightPadding;

	name_view_rect.setLeftTopAndSize(
		name_new_left,
		name_view_rect.mTop,
		name_new_width,
		name_view_rect.getHeight());

	name_view->setShape(name_view_rect);

	LL_DEBUGS("AvatarItemReshape") << "name rect after: " << name_view_rect << LL_ENDL;
}

void LLAvatarListItem::setShowPermissions(bool show)
{
	mShowPermissions=show;
	showPermissions(show);
}

bool LLAvatarListItem::showPermissions(bool visible)
{
	const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
	if(relation && visible)
	{
		mIconPermissionOnline->setVisible(relation->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS));
		mIconPermissionMap->setVisible(relation->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION));
		mIconPermissionEditMine->setVisible(relation->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS));
		mIconPermissionEditTheirs->setVisible(relation->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS));
	}
	else
	{
		mIconPermissionOnline->setVisible(false);
		mIconPermissionMap->setVisible(false);
		mIconPermissionEditMine->setVisible(false);
		mIconPermissionEditTheirs->setVisible(false);
	}

	return nullptr != relation;
}

LLView* LLAvatarListItem::getItemChildView(EAvatarListItemChildIndex child_view_index)
{
	LLView* child_view = mAvatarName;

	switch (child_view_index)
	{
	case ALIC_ICON:
		child_view = mAvatarIcon;
		break;
	case ALIC_NAME:
		child_view = mAvatarName;
		break;
	case ALIC_INTERACTION_TIME:
		child_view = mLastInteractionTime;
		break;
	case ALIC_SPEAKER_INDICATOR:
		child_view = mSpeakingIndicator;
		break;
	case ALIC_PERMISSION_ONLINE:
		child_view = mIconPermissionOnline;
		break;
	case ALIC_PERMISSION_MAP:
		child_view = mIconPermissionMap;
		break;
	case ALIC_PERMISSION_EDIT_MINE:
		child_view = mIconPermissionEditMine;
		break;
	case ALIC_PERMISSION_EDIT_THEIRS:
		child_view = mIconPermissionEditTheirs;
		break;
	case ALIC_INFO_BUTTON:
		child_view = mInfoBtn;
		break;
	case ALIC_PROFILE_BUTTON:
		child_view = mProfileBtn;
		break;
	default:
		LL_WARNS("AvatarItemReshape") << "Unexpected child view index is passed: " << child_view_index << LL_ENDL;
		// leave child_view untouched
	}
	
	return child_view;
}

// EOF
