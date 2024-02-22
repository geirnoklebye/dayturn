/** 
 * @file llstatgraph.h
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

#ifndef LL_LLSTATGRAPH_H
#define LL_LLSTATGRAPH_H

#include "llview.h"
#include "llframetimer.h"
#include "v4color.h"
#include "lltrace.h"

class LLStatGraph : public LLView
{
public:
	struct ThresholdParams : public LLInitParam::Block<ThresholdParams>
	{
		Mandatory<F32>	value;
		Optional<LLUIColor>	color;

		ThresholdParams()
		:	value("value"),
			color("color", LLColor4::white)
		{}
	};

	struct Thresholds : public LLInitParam::Block<Thresholds>
	{
		Multiple<ThresholdParams> threshold;

		Thresholds()
		:	threshold("threshold")
		{}
	};

	struct StatParams : public LLInitParam::ChoiceBlock<StatParams>
	{
		Alternative<LLTrace::StatType<LLTrace::CountAccumulator>* >	count_stat_float;
		Alternative<LLTrace::StatType<LLTrace::EventAccumulator>* >	event_stat_float;
		Alternative<LLTrace::StatType<LLTrace::SampleAccumulator>* >	sample_stat_float;
	};

	struct Params : public LLInitParam::Block<Params, LLView::Params>
	{
		//
		//	"stat" is now optional so that the widget can be set up
		//	in XML without it, and the actual value to use can be
		//	set up in the code later
		//
		Optional<StatParams>	stat;
		Optional<std::string>	label,
								units;
		Optional<S32>			precision;
		Optional<F32>			min,
								max;
		Optional<bool>			per_sec;
		Optional<F32>			value;
		Optional<LLUIColor>		color,
								background_color,
								border_color;
		Optional<Thresholds>	thresholds;

		Params()
		:	stat("stat"),
			label("label"),
			units("units"),
			precision("precision", 0),
			min("min", 0.f),
			max("max", 125.f),
			per_sec("per_sec", true),
			value("value", 0.f),
			color("color", LLColor4::white),
			background_color("background_color", LLColor4::transparent),
			border_color("border_color", LLColor4::black),
			thresholds("thresholds")
		{
		}
	};
	LLStatGraph(const Params&);

	typedef boost::function<void(void)> callback_t;

	void setMin(const F32 min);
	void setMax(const F32 max);
	void setStat(LLTrace::StatType<LLTrace::CountAccumulator> *stat);
	void setStat(LLTrace::StatType<LLTrace::EventAccumulator> *stat);
	void setStat(LLTrace::StatType<LLTrace::SampleAccumulator> *stat);
	void setThreshold(S32 threshold, F32 newval);
	void setClickedCallback(callback_t cb);

	virtual void draw();

	/*virtual*/ void setValue(const LLSD& value);
	
private:
	LLTrace::StatType<LLTrace::CountAccumulator>*	mNewStatFloatp;

	BOOL mPerSec;

	F32 mValue;

	F32 mMin;
	F32 mMax;
	LLFrameTimer mUpdateTimer;
	std::string mLabel;
	std::string mUnits;
	S32 mPrecision; // Num of digits of precision after dot

	LLUIColor mColor;
	LLUIColor mBackgroundColor;
	LLUIColor mBorderColor;

	struct Threshold
	{
		Threshold(F32 value, const LLUIColor& color)
		:	mValue(value),
			mColor(color)
		{}

		F32 mValue;
		LLUIColor mColor;
		bool operator <(const Threshold& other) const
		{
			return mValue < other.mValue;
		}
	};
	typedef std::vector<Threshold> threshold_vec_t;
	threshold_vec_t mThresholds;

	callback_t mClickedCallback;

	/*virtual*/ bool handleMouseDown(S32 x, S32 y, MASK mask);
	/*virtual*/ bool handleMouseUp(S32 x, S32 y, MASK mask);
	/*virtual*/ bool handleHover(S32 x, S32 y, MASK mask);
};

#endif  // LL_LLSTATGRAPH_H
