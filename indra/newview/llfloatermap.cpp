/** 
 * @file llfloatermap.cpp
 * @brief The "mini-map" or radar in the upper right part of the screen.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

// self include
#include "llfloatermap.h"

// Library includes
#include "llfloaterreg.h"
#include "llfontgl.h"
#include "llglheaders.h"

// Viewer includes
#include "llagentcamera.h"
#include "llviewercontrol.h"
#include "llnetmap.h"
#include "lltracker.h"
#include "llviewercamera.h"
#include "lldraghandle.h"
#include "lltextbox.h"
#include "llfloaterworldmap.h"
#include "llagent.h"

//
// Constants
//

// The minor cardinal direction labels are hidden if their height is more
// than this proportion of the map.
const F32 MAP_MINOR_DIR_THRESHOLD = 0.07f;

//
// Member functions
//

LLFloaterMap::LLFloaterMap(const LLSD& key) 
	: LLFloater(key),
	  mTextBoxEast(NULL),
	  mTextBoxNorth(NULL),
	  mTextBoxWest(NULL),
	  mTextBoxSouth(NULL),
	  mTextBoxSouthEast(NULL),
	  mTextBoxNorthEast(NULL),
	  mTextBoxNorthWest(NULL),
	  mTextBoxSouthWest(NULL),
	  mMap(NULL)
{
}

LLFloaterMap::~LLFloaterMap()
{
}

bool LLFloaterMap::postBuild()
{
    mMap = getChild<LLNetMap>("Net Map");
    mMap->setToolTipMsg(getString("ToolTipMsg"));
    mMap->setParcelNameMsg(getString("ParcelNameMsg"));
    mMap->setParcelSalePriceMsg(getString("ParcelSalePriceMsg"));
    mMap->setParcelSaleAreaMsg(getString("ParcelSaleAreaMsg"));
    mMap->setParcelOwnerMsg(getString("ParcelOwnerMsg"));
    mMap->setRegionNameMsg(getString("RegionNameMsg"));
    mMap->setToolTipHintMsg(getString("ToolTipHintMsg"));
    mMap->setAltToolTipHintMsg(getString("AltToolTipHintMsg"));
    sendChildToBack(mMap);

    mTextBoxNorth     = getChild<LLTextBox>("floater_map_north");
    mTextBoxEast      = getChild<LLTextBox>("floater_map_east");
    mTextBoxWest      = getChild<LLTextBox>("floater_map_west");
    mTextBoxSouth     = getChild<LLTextBox>("floater_map_south");
    mTextBoxSouthEast = getChild<LLTextBox>("floater_map_southeast");
    mTextBoxNorthEast = getChild<LLTextBox>("floater_map_northeast");
    mTextBoxSouthWest = getChild<LLTextBox>("floater_map_southwest");
    mTextBoxNorthWest = getChild<LLTextBox>("floater_map_northwest");

    mTextBoxNorth->reshapeToFitText();
    mTextBoxEast->reshapeToFitText();
    mTextBoxWest->reshapeToFitText();
    mTextBoxSouth->reshapeToFitText();
    mTextBoxSouthEast->reshapeToFitText();
    mTextBoxNorthEast->reshapeToFitText();
    mTextBoxSouthWest->reshapeToFitText();
    mTextBoxNorthWest->reshapeToFitText();

    updateMinorDirections();

    // Get the drag handle all the way in back
    sendChildToBack(getDragHandle());

    // keep onscreen
    gFloaterView->adjustToFitScreen(this, false);

    return true;
}

bool LLFloaterMap::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	// If floater is minimized, minimap should be shown on doubleclick (STORM-299)
	if (isMinimized())
	{
		setMinimized(false);
		return true;
	}

	LLVector3d pos_global = mMap->viewPosToGlobal(x, y);
	
	LLTracker::stopTracking(false);
	LLFloaterWorldMap* world_map = LLFloaterWorldMap::getInstance();
	if (world_map)
	{
		world_map->trackLocation(pos_global);
	}

	if (gSavedSettings.getbool("DoubleClickTeleport"))
	{
		// If DoubleClickTeleport is on, double clicking the minimap will teleport there
		gAgent.teleportViaLocationLookAt(pos_global);
	}
	else if (gSavedSettings.getbool("DoubleClickShowWorldMap"))
	{
		LLFloaterReg::showInstance("world_map");
	}
	return true;
}

void LLFloaterMap::setDirectionPos( LLTextBox* text_box, F32 rotation )
{
    // Rotation is in radians.
    // Rotation of 0 means x = 1, y = 0 on the unit circle.

	F32 map_half_height = (F32)(getRect().getHeight() / 2) - getHeaderHeight()/2;
	F32 map_half_width = (F32)(getRect().getWidth() / 2) ;
	F32 text_half_height = (F32)(text_box->getRect().getHeight() / 2);
	F32 text_half_width = (F32)(text_box->getRect().getWidth() / 2);
	F32 radius = llmin( map_half_height - text_half_height, map_half_width - text_half_width );

	// Inset by a little to account for position display.
	radius -= 8.f;

	text_box->setOrigin( 
		ll_round(map_half_width - text_half_width + radius * cos( rotation )),
		ll_round(map_half_height - text_half_height + radius * sin( rotation )) );
}

void LLFloaterMap::updateMinorDirections()
{
	if (mTextBoxNorthEast == NULL)
	{
		return;
	}

	// Hide minor directions if they cover too much of the map
	bool show_minors = mTextBoxNorthEast->getRect().getHeight() < MAP_MINOR_DIR_THRESHOLD *
		llmin(getRect().getWidth(), getRect().getHeight());

	mTextBoxNorthEast->setVisible(show_minors);
	mTextBoxNorthWest->setVisible(show_minors);
	mTextBoxSouthWest->setVisible(show_minors);
	mTextBoxSouthEast->setVisible(show_minors);
}

// virtual
void LLFloaterMap::draw()
{
	F32 rotation = 0;

	static LLUICachedControl<bool> rotate_map("MiniMapRotate", true);
	if( rotate_map )
	{
		// rotate subsequent draws to agent rotation
		rotation = atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );
	}

	setDirectionPos( mTextBoxEast,  rotation );
	setDirectionPos( mTextBoxNorth, rotation + F_PI_BY_TWO );
	setDirectionPos( mTextBoxWest,  rotation + F_PI );
	setDirectionPos( mTextBoxSouth, rotation + F_PI + F_PI_BY_TWO );

	setDirectionPos( mTextBoxNorthEast, rotation +						F_PI_BY_TWO / 2);
	setDirectionPos( mTextBoxNorthWest, rotation + F_PI_BY_TWO +		F_PI_BY_TWO / 2);
	setDirectionPos( mTextBoxSouthWest, rotation + F_PI +				F_PI_BY_TWO / 2);
	setDirectionPos( mTextBoxSouthEast, rotation + F_PI + F_PI_BY_TWO + F_PI_BY_TWO / 2);

	// Note: we can't just gAgent.check cameraMouselook() because the transition states are wrong.
	if(gAgentCamera.cameraMouselook())
	{
		setMouseOpaque(false);
		getDragHandle()->setMouseOpaque(false);
	}
	else
	{
		setMouseOpaque(true);
		getDragHandle()->setMouseOpaque(true);
	}
	
	LLFloater::draw();
}

void LLFloaterMap::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLFloater::reshape(width, height, called_from_parent);
	
	updateMinorDirections();
}

void LLFloaterMap::handleZoom(const LLSD& userdata)
{
	std::string level = userdata.asString();
	
	F32 scale = 0.0f;
	if (level == std::string("default"))
	{
		LLControlVariable *pvar = gSavedSettings.getControl("MiniMapScale");
		if(pvar)
		{
			pvar->resetToDefault();
			scale = gSavedSettings.getF32("MiniMapScale");
		}
	}
	else if (level == std::string("close"))
		scale = LLNetMap::MAP_SCALE_MAX;
	else if (level == std::string("medium"))
		scale = LLNetMap::MAP_SCALE_MID;
	else if (level == std::string("far"))
		scale = LLNetMap::MAP_SCALE_MIN;
	if (scale != 0.0f)
	{
		mMap->setScale(scale);
	}
}

LLFloaterMap* LLFloaterMap::getInstance()
{
	return LLFloaterReg::getTypedInstance<LLFloaterMap>("mini_map");
}
