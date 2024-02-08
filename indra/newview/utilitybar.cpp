/**
 * @file utilitybar.cpp
 * @brief Helper class for the utility bar controls
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Zi Ree @ Second Life
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "utilitybar.h"

#include "llbutton.h"

#include "llagent.h"
#include "llviewercontrol.h"

UtilityBar::UtilityBar() :
	LLSingleton<UtilityBar>(),
	LLEventTimer(0.5f),
	mAOInterfaceButton(nullptr),
	mVolumeControlsInterfaceButton(nullptr),
	mTalkButton(nullptr),
	mPTTButton(nullptr)
{
	// Tried to do this cleanly with callbacks and controls, but ran into dead ends on every approach.
	// This helper class works, but I am not at all satisfied with it. -Zi
}

UtilityBar::~UtilityBar()
{
}

void UtilityBar::init()
{
	LLView* rootView = LLUI::getInstance()->getRootView();

	// Skip all this if we don't have a skin that needs it
	if (!rootView->findChildView("chat_bar_utility_bar_stack"))
	{
		mEventTimer.stop();
		return;
	}

	mTalkButton = rootView->findChild<LLButton>("utility_talk_button");
	mAOInterfaceButton = rootView->findChild<LLButton>("show_ao_interface_button");
	mVolumeControlsInterfaceButton = rootView->findChild<LLButton>("show_volume_controls_button");
	mPTTButton = rootView->findChild<LLButton>("utility_push_to_talk_lock_button");

	if (mTalkButton)
	{
		mTalkButton->setMouseDownCallback(boost::bind(&LLAgent::pressMicrophone, _2));
		mTalkButton->setMouseUpCallback(boost::bind(&LLAgent::releaseMicrophone, _2));
	}

	mEventTimer.start();
}

// PushToTalkToggle
// 	commit.add("Agent.PressMicrophone", boost::bind(&LLAgent::pressMicrophone, _2));
// 	commit.add("Agent.ReleaseMicrophone", boost::bind(&LLAgent::releaseMicrophone, _2));
// 	commit.add("Agent.ToggleMicrophone", boost::bind(&LLAgent::toggleMicrophone, _2));
// 	enable.add("Agent.IsMicrophoneOn", boost::bind(&LLAgent::isMicrophoneOn, _2));
// 	enable.add("Agent.IsActionAllowed", boost::bind(&LLAgent::isActionAllowed, _2));

bool UtilityBar::tick()
{
	// NOTE: copied from llstatusbar.cpp
	// This has to be resolved to callbacks or controls eventually -Zi

	if (mTalkButton)
	{
		mTalkButton->setValue(gAgent.isMicrophoneOn(LLSD()));
		mTalkButton->setEnabled(LLAgent::isActionAllowed(LLSD("speak")));
	}
	if (mPTTButton)
	{
		mPTTButton->setEnabled(LLAgent::isActionAllowed(LLSD("speak")));
	}

	return false;
}

void UtilityBar::setAOInterfaceButtonExpanded(bool expanded)
{
	if (mAOInterfaceButton)
	{
		if (expanded)
		{
			mAOInterfaceButton->setImageOverlay("arrow_down.tga");
		}
		else
		{
			mAOInterfaceButton->setImageOverlay("arrow_up.tga");
		}
	}
}

void UtilityBar::setVolumeControlsButtonExpanded(bool expanded)
{
	if (mVolumeControlsInterfaceButton)
	{
		if (expanded)
		{
			mVolumeControlsInterfaceButton->setImageOverlay("arrow_down.tga");
		}
		else
		{
			mVolumeControlsInterfaceButton->setImageOverlay("arrow_up.tga");
		}
	}
}
