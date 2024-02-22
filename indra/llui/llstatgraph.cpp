/** 
 * @file llstatgraph.cpp
 * @brief Simpler compact stat graph with tooltip
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

//#include "llviewerprecompiledheaders.h"
#include "linden_common.h"

#include "llstatgraph.h"
#include "llrender.h"

#include "llmath.h"
#include "llui.h"
#include "llgl.h"
#include "llglheaders.h"
#include "lltracerecording.h"
#include "lltracethreadrecorder.h"
#include "llwindow.h"
//#include "llviewercontrol.h"

///////////////////////////////////////////////////////////////////////////////////

static LLDefaultChildRegistry::Register<LLStatGraph> r("statistics_graph");

LLStatGraph::LLStatGraph(const Params& p)
:	LLView(p),
	mMin(p.min),
	mMax(p.max),
	mPerSec(p.per_sec),
	mPrecision(p.precision),
	mValue(p.value),
	mLabel(p.label),
	mUnits(p.units),
	mNewStatFloatp(p.stat.count_stat_float),
	mColor(p.color),
	mBackgroundColor(p.background_color),
	mBorderColor(p.border_color)
{
	setToolTip(p.name());

	for(LLInitParam::ParamIterator<ThresholdParams>::const_iterator it = p.thresholds.threshold.begin(), end_it = p.thresholds.threshold.end();
		it != end_it;
		++it)
	{
		mThresholds.push_back(Threshold(it->value(), it->color));
	}
}

void LLStatGraph::draw()
{
	F32 range, frac;
	range = mMax - mMin;
	if (mNewStatFloatp)
	{
		LLTrace::Recording& recording = LLTrace::get_frame_recording().getLastRecording();

		if (mPerSec)
		{
			mValue = recording.getPerSec(*mNewStatFloatp);
		}
		else
		{
			mValue = recording.getSum(*mNewStatFloatp);
		}
	}

	frac = (mValue - mMin) / range;
	frac = llmax(0.f, frac);
	frac = llmin(1.f, frac);

	if (mUpdateTimer.getElapsedTimeF32() > 0.5f)
	{
		std::string format_str;
		std::string tmp_str;
		format_str = llformat("%%s%%.%df%%s", mPrecision);
		tmp_str = llformat(format_str.c_str(), mLabel.c_str(), mValue, mUnits.c_str());
		setToolTip(tmp_str);

		mUpdateTimer.reset();
	}

	gGL.color4fv(mBackgroundColor.get().mV);
	gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0, true);

	gGL.color4fv(mBorderColor.get().mV);
	gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0, false);

	LLColor4 color = mColor.get();

	for (S32 i = mThresholds.size() - 1; i > -1; --i) {
		if (mValue > mThresholds[i].mValue) {
			color = mThresholds[i].mColor;
			break;
		}
	}
	
	gGL.color4fv(color.mV);
	gl_rect_2d(1, ll_round(frac*getRect().getHeight()), getRect().getWidth() - 1, 0, true);
}

void LLStatGraph::setMin(const F32 min)
{
	mMin = min;
}

void LLStatGraph::setMax(const F32 max)
{
	mMax = max;
}

void LLStatGraph::setStat(LLTrace::StatType<LLTrace::CountAccumulator> *stat)
{
	mNewStatFloatp = stat;
}

void LLStatGraph::setStat(LLTrace::StatType<LLTrace::EventAccumulator> *stat)
{
	mNewStatFloatp = (LLTrace::StatType<LLTrace::CountAccumulator> *)stat;
}

void LLStatGraph::setStat(LLTrace::StatType<LLTrace::SampleAccumulator> *stat)
{
	mNewStatFloatp = (LLTrace::StatType<LLTrace::CountAccumulator> *)stat;
}


void LLStatGraph::setThreshold(S32 threshold, F32 newval)
{
	mThresholds[threshold].mValue = newval;
}

void LLStatGraph::setClickedCallback(callback_t cb)
{
	mClickedCallback = boost::bind(cb);
}

bool LLStatGraph::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = LLView::handleMouseDown(x, y, mask);

	if (getSoundFlags() & MOUSE_DOWN) {
		make_ui_sound("UISndClick");
	}

	if (!handled && mClickedCallback) {
		handled = true;
	}

	if (handled) {
		//
		//	route future Mouse messages here preemptively
		//	(release on mouse up)
		//
		gFocusMgr.setMouseCapture(this);
	}

	return handled;
}

bool LLStatGraph::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = LLView::handleMouseUp(x, y, mask);

	if (getSoundFlags() & MOUSE_UP) {
		make_ui_sound("UISndClickRelease");
	}

	//
	//	we only handle the click if the click both started
	//	and ended within us
	//
	if (hasMouseCapture()) {
		//
		//	release the mouse
		//
		gFocusMgr.setMouseCapture(NULL);

		//
		//	DO THIS AT THE VERY END to allow the widget
		//	to be destroyed as a result of being clicked
		//
		if (mClickedCallback && !handled) {
			mClickedCallback();
			handled = true;
		}
	}

	return handled;
}

bool LLStatGraph::handleHover(S32 x, S32 y, MASK mask)
{
	bool handled = LLView::handleHover(x, y, mask);

	if (!handled && mClickedCallback) {
		//
		//	clickable statistics graphs change the cursor to a hand
		//
		LLUI::getInstance()->getWindow()->setCursor(UI_CURSOR_HAND);
		handled = true;
	}

	return handled;
}
