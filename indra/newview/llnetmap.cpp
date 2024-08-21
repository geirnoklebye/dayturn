/** 
 * @file llnetmap.cpp
 * @author James Cook
 * @brief Display of surrounding regions, objects, and agents. 
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2001-2010, Linden Research, Inc.
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

#include "llnetmap.h"

// Library includes (should move below)
#include "indra_constants.h"
#include "llavatarnamecache.h"
#include "llmath.h"
#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "lllocalcliprect.h"
#include "llrender.h"
#include "llresmgr.h"
#include "llui.h"
#include "lltooltip.h"

#include "llglheaders.h"

// Viewer includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h" // for gDisconnected
#include "llavataractions.h"
#include "llfloatersidepanelcontainer.h"
#include "llcallingcard.h" // LLAvatarTracker
#include "llfloaterworldmap.h"
#include "llparcel.h"
#include "lltracker.h"
#include "llsurface.h"
#include "llurlmatch.h"
#include "llurlregistry.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewertexture.h"
#include "llviewertexturelist.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworld.h"
#include "llworldmapview.h"		// shared draw code
#include "lggcontactsets.h"

static LLDefaultChildRegistry::Register<LLNetMap> r1("net_map");

const F32 LLNetMap::MAP_SCALE_MIN = 32;
const F32 LLNetMap::MAP_SCALE_MID = 1024;
const F32 LLNetMap::MAP_SCALE_MAX = 4096;

const F32 MAP_SCALE_ZOOM_FACTOR = 1.04f; // Zoom in factor per click of scroll wheel (4%)
const F32 MIN_DOT_RADIUS = 3.5f;
const F32 DOT_SCALE = 0.75f;
const F32 MIN_PICK_SCALE = 2.f;
const S32 MOUSE_DRAG_SLOP = 2;		// How far the mouse needs to move before we think it's a drag
const F32 WIDTH_PIXELS = 2.f;
const S32 CIRCLE_STEPS = 100;

LLNetMap::avatar_marks_map_t LLNetMap::sAvatarMarksMap; // <FS:Ansariel>

const F64 COARSEUPDATE_MAX_Z = 1020.0f;
//static
uuid_vec_t LLNetMap::sSelected;

LLNetMap::LLNetMap (const Params & p)
:	LLUICtrl (p),
	mBackgroundColor (p.bg_color()),
	mScale( MAP_SCALE_MID ),
	mPixelsPerMeter( MAP_SCALE_MID / REGION_WIDTH_METERS ),
	mObjectMapTPM(0.f),
	mObjectMapPixels(0.f),
	mTargetPan(0.f, 0.f),
	mCurPan(0.f, 0.f),
	mStartPan(0.f, 0.f),
	mMouseDown(0, 0),
	mPanning(false),
	mUpdateObjectImage(false),
	mUpdateParcelImage(false),
	mObjectImageCenterGlobal( gAgentCamera.getCameraPositionGlobal() ),
	mObjectRawImagep(),
	mObjectImagep(),
	mParcelImageCenterGlobal( gAgentCamera.getCameraPositionGlobal() ),
	mParcelRawImagep(),
	mParcelImagep(),
	mClosestAgentToCursor(),
	mToolTipMsg()
{
	mScale = gSavedSettings.getF32("MiniMapScale");
	mPixelsPerMeter = mScale / REGION_WIDTH_METERS;
	mDotRadius = llmax(DOT_SCALE * mPixelsPerMeter, MIN_DOT_RADIUS);
}

LLNetMap::~LLNetMap()
{
	auto menu = static_cast<LLMenuGL*>(mPopupMenuHandle.get());
	if (menu)
	{
		menu->die();
		mPopupMenuHandle.markDead();
	}
}

bool LLNetMap::postBuild()
{
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	
	registrar.add("Minimap.Zoom", boost::bind(&LLNetMap::handleZoom, this, _2));
	registrar.add("Minimap.Tracker", boost::bind(&LLNetMap::handleStopTracking, this, _2));
	// <FS:Ansariel>
	registrar.add("Minimap.Mark", boost::bind(&LLNetMap::handleMark, this, _2));
	registrar.add("Minimap.ClearMark", boost::bind(&LLNetMap::handleClearMark, this));
	registrar.add("Minimap.ClearMarks", boost::bind(&LLNetMap::handleClearMarks, this));
	// </FS:Ansariel>
	registrar.add("Minimap.ShowProfile", boost::bind(&LLNetMap::handleShowProfile, this, _2));
	registrar.add("Minimap.ToggleOverlay", boost::bind(&LLNetMap::handleOverlayToggle, this, _2));

	registrar.add("Minimap.AddToContactSet", boost::bind(&LLNetMap::handleAddToContactSet, this));
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
	LLViewerParcelMgr::instance().setCollisionUpdateCallback(boost::bind(&LLNetMap::refreshParcelOverlay, this));
	LLViewerParcelOverlay::setUpdateCallback(boost::bind(&LLNetMap::refreshParcelOverlay, this));

	LLMenuGL* menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_mini_map.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	mPopupMenuHandle = menu->getHandle();
	return true;
}

void LLNetMap::setScale( F32 scale )
{
	scale = llclamp(scale, MAP_SCALE_MIN, MAP_SCALE_MAX);
	mCurPan *= scale / mScale;
	mScale = scale;
	
	if (mObjectImagep.notNull())
	{
		F32 width = (F32)(getRect().getWidth());
		F32 height = (F32)(getRect().getHeight());
		F32 diameter = sqrt(width * width + height * height);
		F32 region_widths = diameter / mScale;
		F32 meters = region_widths * LLWorld::getInstance()->getRegionWidthInMeters();
		F32 num_pixels = (F32)mObjectImagep->getWidth();
		mObjectMapTPM = num_pixels / meters;
		mObjectMapPixels = diameter;
	}

	mPixelsPerMeter = mScale / REGION_WIDTH_METERS;
	mDotRadius = llmax(DOT_SCALE * mPixelsPerMeter, MIN_DOT_RADIUS);

	gSavedSettings.setF32("MiniMapScale", mScale);

	mUpdateObjectImage = true;
	mUpdateParcelImage = true;
}


///////////////////////////////////////////////////////////////////////////////////

void LLNetMap::draw()
{
    if (!LLWorld::instanceExists())
    {
        return;
    }
 	static LLFrameTimer map_timer;
	//static LLUIColor map_avatar_color = LLUIColorTable::instance().getColor("MapAvatarColor", LLColor4::white);
	//static LLUIColor map_avatar_friend_color = LLUIColorTable::instance().getColor("MapAvatarFriendColor", LLColor4::white);
	static LLUIColor map_track_color = LLUIColorTable::instance().getColor("MapTrackColor", LLColor4::white);
	//static LLUIColor map_track_disabled_color = LLUIColorTable::instance().getColor("MapTrackDisabledColor", LLColor4::white);
	static LLUIColor map_frustum_color = LLUIColorTable::instance().getColor("MapFrustumColor", LLColor4::white);
	static LLUIColor map_frustum_rotating_color = LLUIColorTable::instance().getColor("MapFrustumRotatingColor", LLColor4::white);
	static LLUIColor map_whisper_ring_color = LLUIColorTable::instance().getColor("MapWhisperRingColor", LLColor4::blue); // <FS:LO> FIRE-17460 Add Whisper Chat Ring to Minimap
	static LLUIColor map_chat_ring_color = LLUIColorTable::instance().getColor("MapChatRingColor", LLColor4::yellow);
	static LLUIColor map_shout_ring_color = LLUIColorTable::instance().getColor("MapShoutRingColor", LLColor4::red);
	
	if (mObjectImagep.isNull())
	{
		createObjectImage();
	}

	if (mParcelImagep.isNull()) {
		createParcelImage();
	}

	static LLUICachedControl<bool> auto_center("MiniMapAutoCenter", true);
	if (auto_center)
	{
		mCurPan = lerp(mCurPan, mTargetPan, LLSmoothInterpolation::getInterpolant(0.1f));
	}

	// Prepare a scissor region
	F32 rotation = 0;

	gGL.pushMatrix();
	gGL.pushUIMatrix();
	
	LLVector3 offset = gGL.getUITranslation();
	LLVector3 scale = gGL.getUIScale();

	gGL.loadIdentity();
	gGL.loadUIIdentity();

	gGL.scalef(scale.mV[0], scale.mV[1], scale.mV[2]);
	gGL.translatef(offset.mV[0], offset.mV[1], offset.mV[2]);

	{
		LLLocalClipRect clip(getLocalRect());
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

			gGL.matrixMode(LLRender::MM_MODELVIEW);

			// Draw background rectangle
			LLColor4 background_color = mBackgroundColor.get();
			gGL.color4fv( background_color.mV );
			gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0);
		}

		// region 0,0 is in the middle
		S32 center_sw_left = getRect().getWidth() / 2 + llfloor(mCurPan.mV[VX]);
		S32 center_sw_bottom = getRect().getHeight() / 2 + llfloor(mCurPan.mV[VY]);

		gGL.pushMatrix();

		gGL.translatef( (F32) center_sw_left, (F32) center_sw_bottom, 0.f);

		static LLUICachedControl<bool> rotate_map("MiniMapRotate", true);
		if( rotate_map )
		{
			// rotate subsequent draws to agent rotation
			rotation = atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );
			gGL.rotatef( rotation * RAD_TO_DEG, 0.f, 0.f, 1.f);
		}

		// figure out where agent is
		S32 region_width = ll_round(LLWorld::getInstance()->getRegionWidthInMeters());

		for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
			 iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
		{
			LLViewerRegion* regionp = *iter;
			// Find x and y position relative to camera's center.
			LLVector3 origin_agent = regionp->getOriginAgent();
			LLVector3 rel_region_pos = origin_agent - gAgentCamera.getCameraPositionAgent();
			F32 relative_x = (rel_region_pos.mV[0] / region_width) * mScale;
			F32 relative_y = (rel_region_pos.mV[1] / region_width) * mScale;

			// background region rectangle
			F32 bottom =	relative_y;
			F32 left =		relative_x;
			const F32 real_width(regionp->getWidth());
			F32 top =		bottom + (real_width / region_width) * mScale ;
			F32 right =		left + (real_width / region_width) * mScale ;

			if (regionp == gAgent.getRegion())
			{
				gGL.color4f(1.f, 1.f, 1.f, 1.f);
			}
			else
			{
				gGL.color4f(0.8f, 0.8f, 0.8f, 1.f);
			}

			if (!regionp->isAlive())
			{
				gGL.color4f(1.f, 0.5f, 0.5f, 1.f);
			}

			static LLCachedControl<bool> use_world_map_textures(gSavedSettings, "MiniMapWorldMapTextures", true);
			bool fRenderTerrain = true;

			if (use_world_map_textures) {
				const LLViewerRegion::tex_matrix_t& tiles(regionp->getWorldMapTiles());
				for (S32 i(0), scaled_width(real_width / region_width), square_width(scaled_width * scaled_width);
					i < square_width; ++i)
				{
					const F32 y(i / scaled_width);
					const F32 x(i - y * scaled_width);
					const F32 local_left(left + x * mScale);
					const F32 local_right(local_left + mScale);
					const F32 local_bottom(bottom + y * mScale);
					const F32 local_top(local_bottom + mScale);
					LLViewerTexture* pRegionImage = tiles[x * scaled_width + y];
					if (pRegionImage && pRegionImage->hasGLTexture())
					{
						gGL.getTexUnit(0)->bind(pRegionImage);
						gGL.begin(LLRender::QUADS);
							gGL.texCoord2f(0.f, 1.f);
							gGL.vertex2f(local_left, local_top);
							gGL.texCoord2f(0.f, 0.f);
							gGL.vertex2f(local_left, local_bottom);
							gGL.texCoord2f(1.f, 0.f);
							gGL.vertex2f(local_right, local_bottom);
							gGL.texCoord2f(1.f, 1.f);
							gGL.vertex2f(local_right, local_top);
						gGL.end();
						pRegionImage->setBoostLevel(LLViewerTexture::BOOST_MAP_VISIBLE);
						fRenderTerrain = false;
					}
				}
			}

			if (fRenderTerrain) 
			{
				// Draw using texture.
				gGL.getTexUnit(0)->bind(regionp->getLand().getSTexture());
				gGL.begin(LLRender::QUADS);
					gGL.texCoord2f(0.f, 1.f);
					gGL.vertex2f(left, top);
					gGL.texCoord2f(0.f, 0.f);
					gGL.vertex2f(left, bottom);
					gGL.texCoord2f(1.f, 0.f);
					gGL.vertex2f(right, bottom);
					gGL.texCoord2f(1.f, 1.f);
					gGL.vertex2f(right, top);
				gGL.end();

				// Draw water
				gGL.flush();
				{
					if (regionp->getLand().getWaterTexture())
					{
						gGL.getTexUnit(0)->bind(regionp->getLand().getWaterTexture());
						gGL.begin(LLRender::QUADS);
							gGL.texCoord2f(0.f, 1.f);
							gGL.vertex2f(left, top);
							gGL.texCoord2f(0.f, 0.f);
							gGL.vertex2f(left, bottom);
							gGL.texCoord2f(1.f, 0.f);
							gGL.vertex2f(right, bottom);
							gGL.texCoord2f(1.f, 1.f);
							gGL.vertex2f(right, top);
						gGL.end();
					}
				}
            	gGL.flush();
			}
		}

		//
		//	locate the centre of the object layer, accounting for panning
		//
		LLVector3 new_center = globalPosToView(gAgentCamera.getCameraPositionGlobal());
		new_center.mV[VX] -= mCurPan.mV[VX];
		new_center.mV[VY] -= mCurPan.mV[VY];
		new_center.mV[VZ] = 0.f;
		LLVector3d posCenterGlobal = viewPosToGlobal(llfloor(new_center.mV[VX]), llfloor(new_center.mV[VY]));

		static LLCachedControl<bool> show_property_lines(gSavedSettings, "MiniMapPropertyLines", false);

		if (show_property_lines && (mUpdateParcelImage || dist_vec_squared2D(mParcelImageCenterGlobal, posCenterGlobal) > 9.0f)) {
			mUpdateParcelImage = false;
			mParcelImageCenterGlobal = posCenterGlobal;

			U8 *texture_data = mParcelRawImagep->getData();
			if (texture_data) {
				memset(texture_data, 0, mParcelImagep->getWidth() * mParcelImagep->getHeight() * mParcelImagep->getComponents());
			}

			//
			//	draw property lines and for-sale blocks each region
			//
			for (LLWorld::region_list_t::const_iterator region_iter = LLWorld::getInstance()->getRegionList().begin(); region_iter != LLWorld::getInstance()->getRegionList().end(); region_iter++) {
				const LLViewerRegion *region = *region_iter;

				renderPropertyLinesForRegion(region);
			}
			mParcelImagep->setSubImage(mParcelRawImagep, 0, 0, mParcelImagep->getWidth(), mParcelImagep->getHeight());
		}

		//
		//	redraw object layer periodically
		//
		static LLCachedControl<bool> show_objects(gSavedSettings, "MiniMapObjects", true);
		if (show_objects && (mUpdateObjectImage || map_timer.getElapsedTimeF32() > 0.5f)) {
			mUpdateObjectImage = false;
			mObjectImageCenterGlobal = posCenterGlobal;

			// create the base texture
			U8 *default_texture = mObjectRawImagep->getData();
			if (default_texture) {
				memset(default_texture, 0, mObjectImagep->getWidth() * mObjectImagep->getHeight() * mObjectImagep->getComponents());
			}

			// Draw objects
			gObjectList.renderObjectsForMap(*this);
			mObjectImagep->setSubImage(mObjectRawImagep, 0, 0, mObjectImagep->getWidth(), mObjectImagep->getHeight());
			map_timer.reset();
		}

		LLVector3 camera_position = gAgentCamera.getCameraPositionAgent();
		LLVector3 map_center_agent;

		F32 image_half_width = 0.5f*mObjectMapPixels;
		F32 image_half_height = 0.5f*mObjectMapPixels;

		if (show_objects) {
			LLVector3 map_center_agent = gAgent.getPosAgentFromGlobal(mObjectImageCenterGlobal) - camera_position;
			map_center_agent.mV[VX] *= mScale / region_width;
			map_center_agent.mV[VY] *= mScale / region_width;
			gGL.getTexUnit(0)->bind(mObjectImagep);

			gGL.begin(LLRender::QUADS);
				gGL.texCoord2f(0.f, 1.f);
				gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, image_half_height + map_center_agent.mV[VY]);
				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, map_center_agent.mV[VY] - image_half_height);
				gGL.texCoord2f(1.f, 0.f);
				gGL.vertex2f(image_half_width + map_center_agent.mV[VX], map_center_agent.mV[VY] - image_half_height);
				gGL.texCoord2f(1.f, 1.f);
				gGL.vertex2f(image_half_width + map_center_agent.mV[VX], image_half_height + map_center_agent.mV[VY]);
			gGL.end();
		}

		if (show_property_lines) {
			LLVector3 map_center_agent = gAgent.getPosAgentFromGlobal(mParcelImageCenterGlobal) - camera_position;
			map_center_agent.mV[VX] *= mScale / region_width;
			map_center_agent.mV[VY] *= mScale / region_width;

			gGL.getTexUnit(0)->bind(mParcelImagep);
			gGL.begin(LLRender::QUADS);
				gGL.texCoord2f(0.f, 1.f);
				gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, image_half_height + map_center_agent.mV[VY]);
				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, map_center_agent.mV[VY] - image_half_height);
				gGL.texCoord2f(1.f, 0.f);
				gGL.vertex2f(image_half_width + map_center_agent.mV[VX], map_center_agent.mV[VY] - image_half_height);
				gGL.texCoord2f(1.f, 1.f);
				gGL.vertex2f(image_half_width + map_center_agent.mV[VX], image_half_height + map_center_agent.mV[VY]);
			gGL.end();
		}

		gGL.popMatrix();

		// Mouse pointer in local coordinates
		S32 local_mouse_x;
		S32 local_mouse_y;
		LLUI::getInstance()->getMousePositionLocal(this, &local_mouse_x, &local_mouse_y);
		bool local_mouse = this->pointInView(local_mouse_x, local_mouse_y);
		mClosestAgentToCursor.setNull();
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3)
		mClosestAgentsToCursor.clear();
// [/SL:KB]
		F32 closest_dist_squared = F32_MAX; // value will be overridden in the loop
		F32 min_pick_dist_squared = (mDotRadius * MIN_PICK_SCALE) * (mDotRadius * MIN_PICK_SCALE);

		LLVector3 pos_map;
		uuid_vec_t avatar_ids;
		std::vector<LLVector3d> positions;
		bool unknown_relative_z;

		LLWorld::getInstance()->getAvatars(&avatar_ids, &positions, gAgentCamera.getCameraPositionGlobal());

		// Draw avatars
		for (U32 i = 0; i < avatar_ids.size(); i++)
		{
			LLUUID uuid = avatar_ids[i];
			// Skip self, we'll draw it later
			if (uuid == gAgent.getID()) continue;

			pos_map = globalPosToView(positions[i]);

			if (uuid == gAgent.getID()) {
				//
				//	no need to plot our own position here
				//	as that will be taken care of later
				//
				continue;
			}

			unknown_relative_z = false;

			if (positions[i].mdV[VZ] == -1.f) {
				if (camera_position.mV[VZ] >= COARSEUPDATE_MAX_Z) {
					//
					//	no exact data and cam
					//	is high up.  we don't
					//	know if avatar is above
					//	or below us
					//
					unknown_relative_z = true;
				}
				else {
					//
					//	no exact data but cam is
					//	below 1020.  avatar is
					//	definitely above us so
					//	bump Z-offset so we get
					//	the "up" chevron
					//
					pos_map.mV[VZ] = F32_MAX;
				}
			}
			// </FS:Ansariel>	
			
			LLColor4 color = getAvatarColor(uuid);	// <FS:CR>

			LLWorldMapView::drawAvatar(
				pos_map.mV[VX], pos_map.mV[VY], 
				color, 
				pos_map.mV[VZ], mDotRadius,
				unknown_relative_z);

			if(uuid.notNull())
			{
				bool selected = false;
				uuid_vec_t::iterator sel_iter = sSelected.begin();
				for (; sel_iter != sSelected.end(); sel_iter++)
				{
					if(*sel_iter == uuid)
					{
						selected = true;
						break;
					}
				}
				if(selected)
				{
					if( (pos_map.mV[VX] < 0) ||
						(pos_map.mV[VY] < 0) ||
						(pos_map.mV[VX] >= getRect().getWidth()) ||
						(pos_map.mV[VY] >= getRect().getHeight()) )
					{
						S32 x = ll_round( pos_map.mV[VX] );
						S32 y = ll_round( pos_map.mV[VY] );
						LLWorldMapView::drawTrackingCircle( getRect(), x, y, color, 1, 10);
					} else
					{
						LLWorldMapView::drawTrackingDot(pos_map.mV[VX],pos_map.mV[VY],color,0.f);
					}
				}
			}

			if (local_mouse)
			{
				F32 dist_to_cursor_squared = dist_vec_squared(LLVector2(pos_map.mV[VX], pos_map.mV[VY]), 
												LLVector2(local_mouse_x,local_mouse_y));

				if (dist_to_cursor_squared < min_pick_dist_squared)
				{
					if (dist_to_cursor_squared < closest_dist_squared)
					{
						closest_dist_squared = dist_to_cursor_squared;
						mClosestAgentToCursor = uuid;
						mClosestAgentPosition = positions[i];
					}
					mClosestAgentsToCursor.push_back(uuid);
				}
			}
		}

		// Draw dot for autopilot target
		if (gAgent.getAutoPilot())
		{
			drawTracking( gAgent.getAutoPilotTargetGlobal(), map_track_color );
		}
		else
		{
			LLTracker::ETrackingStatus tracking_status = LLTracker::getTrackingStatus();
			if (  LLTracker::TRACKING_AVATAR == tracking_status )
			{
				drawTracking( LLAvatarTracker::instance().getGlobalPos(), map_track_color );
			} 
			else if ( LLTracker::TRACKING_LANDMARK == tracking_status
					|| LLTracker::TRACKING_LOCATION == tracking_status )
			{
				drawTracking( LLTracker::getTrackedPositionGlobal(), map_track_color );
			}
		}

		// Draw dot for self avatar position
		static LLUIColor self_tag_color = LLUIColorTable::instance().getColor("MapAvatarSelfColor", LLColor4::yellow); // <FS:CR> FIRE-1061
		LLVector3d pos_global = gAgent.getPositionGlobal();
		pos_map = globalPosToView(pos_global);
		S32 dot_width = ll_round(mDotRadius * 2.f);
		LLUIImagePtr you = LLWorldMapView::sAvatarYouLargeImage;
		if (you)
		{
			you->draw(ll_round(pos_map.mV[VX] - mDotRadius),
					  ll_round(pos_map.mV[VY] - mDotRadius),
					  dot_width,
					  dot_width,
					  self_tag_color);	// <FS:CR> FIRE-1061

			F32 dist_to_cursor_squared = dist_vec_squared(
				LLVector2(pos_map.mV[VX], pos_map.mV[VY]),
				LLVector2(local_mouse_x,local_mouse_y)
			);
			if(dist_to_cursor_squared < min_pick_dist_squared && dist_to_cursor_squared < closest_dist_squared) {
				mClosestAgentToCursor = gAgent.getID();
				mClosestAgentPosition = pos_global;
			}

			//
			//	draw chat range rings if enabled
			//
			static LLUICachedControl<bool> chat_ring("MiniMapChatRing", true);
			// <FS:LO> FIRE-22954 Make each chat range ring in the minimap optional
			static LLUICachedControl<bool> fs_whisper_ring("FSMiniMapWhisperRing", true);
			static LLUICachedControl<bool> fs_chat_ring("FSMiniMapChatRing", true);
			static LLUICachedControl<bool> fs_shout_ring("FSMiniMapShoutRing", true);
			// </FS:LO>
			if (chat_ring) {
				if (fs_whisper_ring) drawRing(CHAT_WHISPER_RADIUS, pos_map, map_whisper_ring_color);
				if (fs_chat_ring) drawRing(CHAT_NORMAL_RADIUS, pos_map, map_chat_ring_color);
				if (fs_shout_ring) drawRing(CHAT_SHOUT_RADIUS, pos_map, map_shout_ring_color);
			}
		}

		// Draw frustum
		F32 meters_to_pixels = mScale/ LLWorld::getInstance()->getRegionWidthInMeters();

		F32 horiz_fov = LLViewerCamera::getInstance()->getView() * LLViewerCamera::getInstance()->getAspect();
		F32 far_clip_meters = LLViewerCamera::getInstance()->getFar();
		F32 far_clip_pixels = far_clip_meters * meters_to_pixels;

		F32 half_width_meters = far_clip_meters * tan( horiz_fov / 2 );
		F32 half_width_pixels = half_width_meters * meters_to_pixels;
	
		F32 ctr_x = (F32)center_sw_left;
		F32 ctr_y = (F32)center_sw_bottom;


		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		if( rotate_map )
		{
			gGL.color4fv((map_frustum_color()).mV);

			gGL.begin( LLRender::TRIANGLES  );
				gGL.vertex2f( ctr_x, ctr_y );
				gGL.vertex2f( ctr_x - half_width_pixels, ctr_y + far_clip_pixels );
				gGL.vertex2f( ctr_x + half_width_pixels, ctr_y + far_clip_pixels );
			gGL.end();
		}
		else
		{
			gGL.color4fv((map_frustum_rotating_color()).mV);
		
			// If we don't rotate the map, we have to rotate the frustum.
			gGL.pushMatrix();
			gGL.translatef( ctr_x, ctr_y, 0 );
			gGL.rotatef( atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] ) * RAD_TO_DEG, 0.f, 0.f, -1.f);
			gGL.begin( LLRender::TRIANGLES  );
				gGL.vertex2f( 0, 0 );
				gGL.vertex2f( -half_width_pixels, far_clip_pixels );
				gGL.vertex2f(  half_width_pixels, far_clip_pixels );
			gGL.end();
			gGL.popMatrix();
		}
	}
	
	gGL.popMatrix();
	gGL.popUIMatrix();

	LLUICtrl::draw();
}

void LLNetMap::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLUICtrl::reshape(width, height, called_from_parent);
	createObjectImage();
	createParcelImage();
}

LLVector3 LLNetMap::globalPosToView(const LLVector3d& global_pos)
{
	LLVector3d camera_position = gAgentCamera.getCameraPositionGlobal();

	LLVector3d relative_pos_global = global_pos - camera_position;
	LLVector3 pos_local;
	pos_local.setVec(relative_pos_global);  // convert to floats from doubles

	mPixelsPerMeter = mScale / LLWorld::getInstance()->getRegionWidthInMeters();
	pos_local.mV[VX] *= mPixelsPerMeter;
	pos_local.mV[VY] *= mPixelsPerMeter;
	// leave Z component in meters

	static LLUICachedControl<bool> rotate_map("MiniMapRotate", true);
	if( rotate_map )
	{
		F32 radians = atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );
		LLQuaternion rot(radians, LLVector3(0.f, 0.f, 1.f));
		pos_local.rotVec( rot );
	}

	pos_local.mV[VX] += getRect().getWidth() / 2 + mCurPan.mV[VX];
	pos_local.mV[VY] += getRect().getHeight() / 2 + mCurPan.mV[VY];

	return pos_local;
}

void LLNetMap::drawRing(const F32 radius, const LLVector3 pos_map, const LLUIColor& colour)
{
	F32 meters_to_pixels = mScale / REGION_WIDTH_METERS;
	F32 radius_pixels = radius * meters_to_pixels;

	glMatrixMode(GL_MODELVIEW);
	gGL.pushMatrix();
	gGL.translatef((F32)pos_map.mV[VX], (F32)pos_map.mV[VY], 0.f);
	gl_ring(radius_pixels, WIDTH_PIXELS, colour, colour, CIRCLE_STEPS, false);
	gGL.popMatrix();
}

void LLNetMap::drawTracking(const LLVector3d& pos_global, const LLColor4& color, 
							bool draw_arrow )
{
	LLVector3 pos_local = globalPosToView(pos_global);
	if( (pos_local.mV[VX] < 0) ||
		(pos_local.mV[VY] < 0) ||
		(pos_local.mV[VX] >= getRect().getWidth()) ||
		(pos_local.mV[VY] >= getRect().getHeight()) )
	{
		if (draw_arrow)
		{
			S32 x = ll_round( pos_local.mV[VX] );
			S32 y = ll_round( pos_local.mV[VY] );
			LLWorldMapView::drawTrackingCircle( getRect(), x, y, color, 1, 10 );
			LLWorldMapView::drawTrackingArrow( getRect(), x, y, color );
		}
	}
	else
	{
		LLWorldMapView::drawTrackingDot(pos_local.mV[VX], 
										pos_local.mV[VY], 
										color,
										pos_local.mV[VZ]);
	}
}

LLVector3d LLNetMap::viewPosToGlobal( S32 x, S32 y )
{
	x -= ll_round(getRect().getWidth() / 2 + mCurPan.mV[VX]);
	y -= ll_round(getRect().getHeight() / 2 + mCurPan.mV[VY]);

	LLVector3 pos_local( (F32)x, (F32)y, 0 );

	F32 radians = - atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );

	static LLUICachedControl<bool> rotate_map("MiniMapRotate", true);
	if( rotate_map )
	{
		LLQuaternion rot(radians, LLVector3(0.f, 0.f, 1.f));
		pos_local.rotVec( rot );
	}

	pos_local *= ( LLWorld::getInstance()->getRegionWidthInMeters() / mScale );
	
	LLVector3d pos_global;
	pos_global.setVec( pos_local );
	pos_global += gAgentCamera.getCameraPositionGlobal();

	return pos_global;
}

bool LLNetMap::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	// note that clicks are reversed from what you'd think: i.e. > 0  means zoom out, < 0 means zoom in
	F32 new_scale = mScale * pow(MAP_SCALE_ZOOM_FACTOR, -clicks);
	F32 old_scale = mScale;

	setScale(new_scale);

	static LLUICachedControl<bool> auto_center("MiniMapAutoCenter", true);
	if (!auto_center)
	{
		// Adjust pan to center the zoom on the mouse pointer
		LLVector2 zoom_offset;
		zoom_offset.mV[VX] = x - getRect().getWidth() / 2;
		zoom_offset.mV[VY] = y - getRect().getHeight() / 2;
		mCurPan -= zoom_offset * mScale / old_scale - zoom_offset;
	}

	return true;
}

bool LLNetMap::handleToolTip(S32 x, S32 y, MASK mask)
{
    if (gDisconnected)
    {
        return false;
    }

    // If the cursor is near an avatar on the minimap, a mini-inspector will be
    // shown for the avatar, instead of the normal map tooltip.
    if (handleToolTipAgent(mClosestAgentToCursor))
    {
        return true;
    }

    LLRect sticky_rect;
    S32 SLOP = 4;
    localPointToScreen(x - SLOP, y - SLOP, &(sticky_rect.mLeft), &(sticky_rect.mBottom));
    sticky_rect.mRight = sticky_rect.mLeft + 2 * SLOP;
    sticky_rect.mTop   = sticky_rect.mBottom + 2 * SLOP;

    std::string parcel_name_msg;
    std::string parcel_sale_price_msg;
    std::string parcel_sale_area_msg;
    std::string parcel_owner_msg;
    std::string region_name_msg;

    LLVector3d      posGlobal = viewPosToGlobal(x, y);
    LLViewerRegion *region    = LLWorld::getInstance()->getRegionFromPosGlobal(posGlobal);
    if (region)
    {
        std::string region_name = region->getName();
        if (!region_name.empty())
        {
            region_name_msg = mRegionNameMsg;
            LLStringUtil::format(region_name_msg, {{"[REGION_NAME]", region_name}});
        }

        // Only show parcel information in the tooltip if property lines are visible. Otherwise, the parcel the tooltip is referring to is
        // ambiguous.
        if (gSavedSettings.getbool("MiniMapPropertyLines"))
        {
            LLViewerParcelMgr::getInstance()->setHoverParcel(posGlobal);
            LLParcel *hover_parcel = LLViewerParcelMgr::getInstance()->getHoverParcel();
            if (hover_parcel)
            {
                std::string parcel_name = hover_parcel->getName();
                if (!parcel_name.empty())
                {
                    parcel_name_msg = mParcelNameMsg;
                    LLStringUtil::format(parcel_name_msg, {{"[PARCEL_NAME]", parcel_name}});
                }

                const LLUUID      parcel_owner          = hover_parcel->getOwnerID();
                std::string       parcel_owner_name_url = LLSLURL("agent", parcel_owner, "inspect").getSLURLString();
                static LLUrlMatch parcel_owner_name_url_match;
                LLUrlRegistry::getInstance()->findUrl(parcel_owner_name_url, parcel_owner_name_url_match);
                if (!parcel_owner_name_url_match.empty())
                {
                    parcel_owner_msg              = mParcelOwnerMsg;
                    std::string parcel_owner_name = parcel_owner_name_url_match.getLabel();
                    LLStringUtil::format(parcel_owner_msg, {{"[PARCEL_OWNER]", parcel_owner_name}});
                }

                if (hover_parcel->getForSale())
                {
                    const LLUUID auth_buyer_id = hover_parcel->getAuthorizedBuyerID();
                    const LLUUID agent_id      = gAgent.getID();
                    bool         show_for_sale = auth_buyer_id.isNull() || auth_buyer_id == agent_id || parcel_owner == agent_id;
                    if (show_for_sale)
                    {
                        S32 price        = hover_parcel->getSalePrice();
                        S32 area         = hover_parcel->getArea();
                        F32 cost_per_sqm = 0.0f;
                        if (area > 0)
                        {
                            cost_per_sqm = F32(price) / area;
                        }
                        std::string formatted_price          = LLResMgr::getInstance()->getMonetaryString(price);
                        std::string formatted_cost_per_meter = llformat("%.1f", cost_per_sqm);
                        parcel_sale_price_msg                = mParcelSalePriceMsg;
                        LLStringUtil::format(parcel_sale_price_msg,
                                             {{"[PRICE]", formatted_price}, {"[PRICE_PER_SQM]", formatted_cost_per_meter}});
                        std::string formatted_area = llformat("%d", area);
                        parcel_sale_area_msg       = mParcelSaleAreaMsg;
                        LLStringUtil::format(parcel_sale_area_msg, {{"[AREA]", formatted_area}});
                    }
                }
            }
        }
    }

    std::string tool_tip_hint_msg;
    if (gSavedSettings.getbool("DoubleClickTeleport"))
    {
        tool_tip_hint_msg = mAltToolTipHintMsg;
    }
    else if (gSavedSettings.getbool("DoubleClickShowWorldMap"))
    {
        tool_tip_hint_msg = mToolTipHintMsg;
    }

    LLStringUtil::format_map_t args;
    args["[PARCEL_NAME_MSG]"]       = parcel_name_msg.empty() ? "" : parcel_name_msg + '\n';
    args["[PARCEL_SALE_PRICE_MSG]"] = parcel_sale_price_msg.empty() ? "" : parcel_sale_price_msg + '\n';
    args["[PARCEL_SALE_AREA_MSG]"]  = parcel_sale_area_msg.empty() ? "" : parcel_sale_area_msg + '\n';
    args["[PARCEL_OWNER_MSG]"]      = parcel_owner_msg.empty() ? "" : parcel_owner_msg + '\n';
    args["[REGION_NAME_MSG]"]       = region_name_msg.empty() ? "" : region_name_msg + '\n';
    args["[TOOL_TIP_HINT_MSG]"]     = tool_tip_hint_msg.empty() ? "" : tool_tip_hint_msg + '\n';

    std::string msg                 = mToolTipMsg;
    LLStringUtil::format(msg, args);
    if (msg.back() == '\n')
    {
        msg.resize(msg.size() - 1);
    }
    LLToolTipMgr::instance().show(LLToolTip::Params().message(msg).sticky_rect(sticky_rect));

    return true;
}

bool LLNetMap::handleToolTipAgent(const LLUUID& avatar_id)
{
	LLAvatarName av_name;
	if (avatar_id.isNull() || !LLAvatarNameCache::get(avatar_id, &av_name))
	{
		return false;
	}

	// only show tooltip if same inspector not already open
	LLFloater* existing_inspector = LLFloaterReg::findInstance("inspect_avatar");
	if (!existing_inspector
		|| !existing_inspector->getVisible()
		|| existing_inspector->getKey()["avatar_id"].asUUID() != avatar_id)
	{
		LLInspector::Params p;
		p.fillFrom(LLUICtrlFactory::instance().getDefaultParams<LLInspector>());
		p.message(av_name.getCompleteName());
		p.image.name("Inspector_I");
		p.click_callback(boost::bind(showAvatarInspector, avatar_id));
		p.visible_time_near(6.f);
		p.visible_time_far(3.f);
		p.delay_time(0.35f);
		p.wrap(false);

		LLToolTipMgr::instance().show(p);
	}
	return true;
}

// static
void LLNetMap::showAvatarInspector(const LLUUID& avatar_id)
{
	LLSD params;
	params["avatar_id"] = avatar_id;

	if (LLToolTipMgr::instance().toolTipVisible())
	{
		LLRect rect = LLToolTipMgr::instance().getToolTipRect();
		params["pos"]["x"] = rect.mLeft;
		params["pos"]["y"] = rect.mTop;
	}

	LLFloaterReg::showInstance("inspect_avatar", params);
}

void LLNetMap::renderScaledPointGlobal( const LLVector3d& pos, const LLColor4U &color, F32 radius_meters )
{
	LLVector3 local_pos;
	local_pos.setVec( pos - mObjectImageCenterGlobal );

	S32 diameter_pixels = ll_round(2 * radius_meters * mObjectMapTPM);
	renderPoint( local_pos, color, diameter_pixels );
}


void LLNetMap::renderPoint(const LLVector3 &pos_local, const LLColor4U &color, 
						   S32 diameter, S32 relative_height)
{
	if (diameter <= 0)
	{
		return;
	}

	const S32 image_width = (S32)mObjectImagep->getWidth();
	const S32 image_height = (S32)mObjectImagep->getHeight();

	S32 x_offset = ll_round(pos_local.mV[VX] * mObjectMapTPM + image_width / 2);
	S32 y_offset = ll_round(pos_local.mV[VY] * mObjectMapTPM + image_height / 2);

	if ((x_offset < 0) || (x_offset >= image_width))
	{
		return;
	}
	if ((y_offset < 0) || (y_offset >= image_height))
	{
		return;
	}

	U8 *datap = mObjectRawImagep->getData();

	S32 neg_radius = diameter / 2;
	S32 pos_radius = diameter - neg_radius;
	S32 x, y;

	if (relative_height > 0)
	{
		// ...point above agent
		S32 px, py;

		// vertical line
		px = x_offset;
		for (y = -neg_radius; y < pos_radius; y++)
		{
			py = y_offset + y;
			if ((py < 0) || (py >= image_height))
			{
				continue;
			}
			S32 offset = px + py * image_width;
			((U32*)datap)[offset] = color.asRGBA();
		}

		// top line
		py = y_offset + pos_radius - 1;
		for (x = -neg_radius; x < pos_radius; x++)
		{
			px = x_offset + x;
			if ((px < 0) || (px >= image_width))
			{
				continue;
			}
			S32 offset = px + py * image_width;
			((U32*)datap)[offset] = color.asRGBA();
		}
	}
	else
	{
		// ...point level with agent
		for (x = -neg_radius; x < pos_radius; x++)
		{
			S32 p_x = x_offset + x;
			if ((p_x < 0) || (p_x >= image_width))
			{
				continue;
			}

			for (y = -neg_radius; y < pos_radius; y++)
			{
				S32 p_y = y_offset + y;
				if ((p_y < 0) || (p_y >= image_height))
				{
					continue;
				}
				S32 offset = p_x + p_y * image_width;
				((U32*)datap)[offset] = color.asRGBA();
			}
		}
	}
}

bool LLNetMap::createImage(LLPointer<LLImageRaw>& rawimagep) const
{
	// Find the size of the side of a square that surrounds the circle that surrounds getRect().
	// ... which is, the diagonal of the rect.
	F32 width = (F32)getRect().getWidth();
	F32 height = (F32)getRect().getHeight();
	S32 square_size = ll_round( sqrt(width*width + height*height) );

	// Find the least power of two >= the minimum size.
	const S32 MIN_SIZE = 64;
	const S32 MAX_SIZE = 512;
	S32 img_size = MIN_SIZE;
	while( (img_size*2 < square_size ) && (img_size < MAX_SIZE) )
	{
		img_size <<= 1;
	}

	if (rawimagep.isNull() || rawimagep->getWidth() != img_size || rawimagep->getHeight() != img_size) {
		rawimagep = new LLImageRaw(img_size, img_size, 4);
		U8 *data = rawimagep->getData();

		if (data) {
			memset(data, 0, img_size * img_size * 4);
		}
		return true;
	}
	return false;
}

bool LLNetMap::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (!(mask & MASK_SHIFT)) return false;

	// Start panning
	gFocusMgr.setMouseCapture(this);

	mStartPan = mCurPan;
	mMouseDown.mX = x;
	mMouseDown.mY = y;
	return true;
}

bool LLNetMap::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if(abs(mMouseDown.mX-x)<3 && abs(mMouseDown.mY-y)<3)
		handleClick(x,y,mask);

	if (hasMouseCapture())
	{
		if (mPanning)
		{
			// restore mouse cursor
			S32 local_x, local_y;
			local_x = mMouseDown.mX + llfloor(mCurPan.mV[VX] - mStartPan.mV[VX]);
			local_y = mMouseDown.mY + llfloor(mCurPan.mV[VY] - mStartPan.mV[VY]);
			LLRect clip_rect = getRect();
			clip_rect.stretch(-8);
			clip_rect.clipPointToRect(mMouseDown.mX, mMouseDown.mY, local_x, local_y);
			LLUI::getInstance()->setMousePositionLocal(this, local_x, local_y);

			// finish the pan
			mPanning = false;

			mMouseDown.set(0, 0);

			// auto centre
			mTargetPan.setZero();
		}
		gViewerWindow->showCursor();
		gFocusMgr.setMouseCapture(nullptr);
		return true;
	}
	return false;
}

void LLNetMap::handleShowProfile(const LLSD& sdParam) const
{
	const std::string strParam = sdParam.asString();
	if ("closest" == strParam)
	{
		LLAvatarActions::showProfile(mClosestAgentRightClick);
	}
	else if ("place" == strParam)
	{
		LLSD sdParams;
		sdParams["type"] = "remote_place";
		sdParams["x"] = mPosGlobalRightClick.mdV[VX];
		sdParams["y"] = mPosGlobalRightClick.mdV[VY];
		sdParams["z"] = mPosGlobalRightClick.mdV[VZ];

		LLFloaterSidePanelContainer::showPanel("places", sdParams);
	}
}
bool LLNetMap::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	auto menu = static_cast<LLMenuGL*>(mPopupMenuHandle.get());
	if (menu)
	{
		menu->buildDrawLabels();
		menu->updateParent(LLMenuGL::sMenuContainer);
		menu->setItemEnabled("Stop Tracking", LLTracker::isTracking(nullptr));
		LLMenuGL::showPopup(this, menu, x, y);
	}
	return true;
}

bool LLNetMap::handleClick(S32 x, S32 y, MASK mask)
{
	// TODO: allow clicking an avatar on minimap to select avatar in the nearby avatar list
	// if(mClosestAgentToCursor.notNull())
	//     mNearbyList->selectUser(mClosestAgentToCursor);
	// Needs a registered observer i guess to accomplish this without using
	// globals to tell the mNearbyList in llpeoplepanel to select the user
	return true;
}

bool LLNetMap::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	LLVector3d pos_global = viewPosToGlobal(x, y);

	bool double_click_teleport = gSavedSettings.getbool("DoubleClickTeleport");
	bool double_click_show_world_map = gSavedSettings.getbool("DoubleClickShowWorldMap");

	if (double_click_teleport || double_click_show_world_map)
	{
		// If we're not tracking a beacon already, double-click will set one 
		if (!LLTracker::isTracking(nullptr))
		{
			LLFloaterWorldMap* world_map = LLFloaterWorldMap::getInstance();
			if (world_map)
			{
				world_map->trackLocation(pos_global);
			}
		}
	}

	if (double_click_teleport)
	{
		// If DoubleClickTeleport is on, double clicking the minimap will teleport there
		gAgent.teleportViaLocationLookAt(pos_global);
	}
	else if (double_click_show_world_map)
	{
		LLFloaterReg::showInstance("world_map");
	}
	return true;
}

void LLNetMap::handleOverlayToggle(const LLSD& sdParam)
{
	const std::string control = sdParam.asString();
	gSavedSettings.setbool(control, !gSavedSettings.getbool(control));
	mUpdateParcelImage = true;
}

void LLNetMap::renderPropertyLinesForRegion(const LLViewerRegion* region)
{
	const S32 imgWidth = (S32)mParcelImagep->getWidth();
	const S32 imgHeight = (S32)mParcelImagep->getHeight();

	const LLVector3 originLocal(region->getOriginGlobal() - mParcelImageCenterGlobal);
	const S32 originX = llround(originLocal.mV[VX] * mObjectMapTPM + imgWidth / 2);
	const S32 originY = llround(originLocal.mV[VY] * mObjectMapTPM + imgHeight / 2);

	U32* pTextureData = (U32*)mParcelRawImagep->getData();

  	static LLUIColor map_property_line_colour = LLUIColorTable::instance().getColor("NetMapPropertyLineColor", LLColor4(LLColor3(LLColor4::yellow), 0.5f));

  	//no longer used - same colours as land overlay are used now
	//static LLUIColor map_for_sale_colour = LLUIColorTable::instance().getColor("NetMapPropertyForSaleColor", LLColor4::yellow);

	//const U32 colour_for_sale = LLColor4U(map_for_sale_colour.get()).asRGBA();
	const U32 colour_property_lines = region->isAlive() ? LLColor4U(map_property_line_colour.get()).asRGBA() : LLColor4U(255, 128, 128, 255).asRGBA();

	//
	//	draw the north and east region borders
	//
	const F32 real_width(region->getWidth());
	const S32 borderY = originY + llround(real_width * mObjectMapTPM);
	if (borderY >= 0 && borderY < imgHeight) {
		S32 curX = llclamp(originX, 0, imgWidth), endX = llclamp(originX + (S32)llround(real_width * mObjectMapTPM), 0, imgWidth - 1);
		for (; curX <= endX; curX++) {
			pTextureData[borderY * imgWidth + curX] = colour_property_lines;
		}
	}

	const S32 borderX = originX + llround(real_width * mObjectMapTPM);
	if (borderX >= 0 && borderX < imgWidth) {
		S32 curY = llclamp(originY, 0, imgHeight), endY = llclamp(originY + (S32)lround(real_width * mObjectMapTPM), 0, imgHeight - 1);
		for (; curY <= endY; curY++) {
			pTextureData[curY * imgWidth + borderX] = colour_property_lines;
		}
	}

	//
	//	render parcel lines
	//
	static const F32 GRID_STEP = PARCEL_GRID_STEP_METERS;
	static const S32 GRIDS_PER_EDGE = real_width / GRID_STEP;

	const U8 *ownerp = region->getParcelOverlay()->getOwnership();
	const U8 *collisionp = (region->getHandle() == LLViewerParcelMgr::instance().getCollisionRegionHandle()) ? LLViewerParcelMgr::instance().getCollisionBitmap() : NULL;

	static LLCachedControl<bool> for_sale_parcels(gSavedSettings, "MiniMapForSaleParcels", false);
	static LLCachedControl<bool> collision_parcels(gSavedSettings, "MiniMapCollisionParcels", true);
	static LLCachedControl<bool> land_owners(gSavedSettings, "MiniMapLandOwners", false);

	const LLColor4U colour_owned = LLUIColorTable::instance().getColor("PropertyColorOther").get();
	const LLColor4U colour_group = LLUIColorTable::instance().getColor("PropertyColorGroup").get();
	const LLColor4U colour_self  = LLUIColorTable::instance().getColor("PropertyColorSelf").get();
	const LLColor4U colour_for_sale  = LLUIColorTable::instance().getColor("PropertyColorForSale").get();
	const LLColor4U colour_auction  = LLUIColorTable::instance().getColor("PropertyColorAuction").get();
	const LLColor4U colour_default = LLColor4U(255, 128, 128, 192);

	for (S32 row = 0; row < GRIDS_PER_EDGE; row++) {
		for (S32 col = 0; col < GRIDS_PER_EDGE; col++) {
			S32 collision_idx = (row * GRIDS_PER_EDGE) + col;
			S32 overlay = ownerp[collision_idx];

			bool detected_owned = ((overlay & PARCEL_COLOR_MASK) == PARCEL_OWNED);
			bool detected_group = ((overlay & PARCEL_COLOR_MASK) == PARCEL_GROUP);
			bool detected_self = ((overlay & PARCEL_COLOR_MASK) == PARCEL_SELF);
			bool detected_for_sale = ((overlay & PARCEL_COLOR_MASK) == PARCEL_FOR_SALE);
			bool detected_auction = ((overlay & PARCEL_COLOR_MASK) == PARCEL_AUCTION);
			bool detected_collision = (collisionp && (collisionp[collision_idx / 8] & (1 << (collision_idx % 8))));

			if (!detected_owned && !detected_group && ! detected_self && !detected_for_sale && !detected_auction && !detected_collision && !(overlay & (PARCEL_SOUTH_LINE | PARCEL_WEST_LINE))) {
				continue;
			}

			const S32 posX = originX + llround(col * GRID_STEP * mObjectMapTPM);
			const S32 posY = originY + llround(row * GRID_STEP * mObjectMapTPM);

			if ((for_sale_parcels && (detected_auction || detected_for_sale)) || 
				(land_owners && (detected_owned || detected_group || detected_self || detected_for_sale || detected_auction)) || 
				(collision_parcels && detected_collision)) {
				S32 curY = llclamp(posY, 0, imgHeight), endY = llclamp(posY + (S32)llround(GRID_STEP * mObjectMapTPM), 0, imgHeight - 1);
				for (; curY <= endY; curY++) {
					S32 curX = llclamp(posX, 0, imgWidth) , endX = llclamp(posX + (S32)llround(GRID_STEP * mObjectMapTPM), 0, imgWidth - 1);
					for (; curX <= endX; curX++) {
						LLColor4U this_colour = colour_default;
						if (for_sale_parcels || land_owners) {
							if (detected_auction) this_colour = colour_auction;
							else if (detected_for_sale) this_colour = colour_for_sale;
						}
						// match the land owner highlighting behaviour where owned parcels override for sale status
						if (land_owners) {
							if (detected_owned) this_colour = colour_owned;
							else if (detected_group) this_colour = colour_group;
							else if (detected_self) this_colour = colour_self;
						}
						pTextureData[(curY * imgWidth) + curX] = this_colour.asRGBA();
					}
				}
			}

			if (overlay & PARCEL_SOUTH_LINE) {
				if (posY >= 0 && posY < imgHeight) {
					S32 curX = llclamp(posX, 0, imgWidth), endX = llclamp(posX + (S32)llround(GRID_STEP * mObjectMapTPM), 0, imgWidth - 1);
					for (; curX <= endX; curX++) {
						pTextureData[(posY * imgWidth) + curX] = colour_property_lines;
					}
				}
			}

			if (overlay & PARCEL_WEST_LINE) {
				if (posX >= 0 && posX < imgWidth) {
					S32 curY = llclamp(posY, 0, imgHeight), endY = llclamp(posY + (S32)llround(GRID_STEP * mObjectMapTPM), 0, imgHeight - 1);
					for (; curY <= endY; curY++) {
						pTextureData[(curY * imgWidth) + posX] = colour_property_lines;
					}
				}
			}
		}
	}
}

void LLNetMap::createObjectImage()
{
        if (createImage(mObjectRawImagep)) {
                mObjectImagep = LLViewerTextureManager::getLocalTexture( mObjectRawImagep.get(), false);
	}

        setScale(mScale);
        mUpdateObjectImage = true;
}

void LLNetMap::createParcelImage()
{
	if (createImage(mParcelRawImagep)) {
		mParcelImagep = LLViewerTextureManager::getLocalTexture(mParcelRawImagep.get(), false);
	}
	mUpdateParcelImage = true;
}


// static
bool LLNetMap::outsideSlop( S32 x, S32 y, S32 start_x, S32 start_y, S32 slop )
{
	S32 dx = x - start_x;
	S32 dy = y - start_y;

	return (dx <= -slop || slop <= dx || dy <= -slop || slop <= dy);
}

bool LLNetMap::handleHover( S32 x, S32 y, MASK mask )
{
	if (hasMouseCapture())
	{
		if (mPanning || outsideSlop(x, y, mMouseDown.mX, mMouseDown.mY, MOUSE_DRAG_SLOP))
		{
			if (!mPanning)
			{
				// just started panning, so hide cursor
				mPanning = true;
				gViewerWindow->hideCursor();
			}

			LLVector2 delta(static_cast<F32>(gViewerWindow->getCurrentMouseDX()),
							static_cast<F32>(gViewerWindow->getCurrentMouseDY()));

			// Set pan to value at start of drag + offset
			mCurPan += delta;
			mTargetPan = mCurPan;

			gViewerWindow->moveCursorToCenter();
		}

		// Doesn't really matter, cursor should be hidden
		gViewerWindow->setCursor( UI_CURSOR_TOOLPAN );
	}
	else
	{
		if (mask & MASK_SHIFT)
		{
			// If shift is held, change the cursor to hint that the map can be dragged
			gViewerWindow->setCursor( UI_CURSOR_TOOLPAN );
		}
		else
		{
			gViewerWindow->setCursor( UI_CURSOR_CROSS );
		}
	}

	return true;
}

void LLNetMap::handleZoom(const LLSD& userdata)
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
		setScale(scale);
	}
}

// <FS:Ansariel> Mark avatar feature
void LLNetMap::handleMark(const LLSD& userdata)
{
	setAvatarMarkColors(mClosestAgentsRightClick, userdata);
}

void LLNetMap::handleClearMark()
{
	clearAvatarMarkColors(mClosestAgentsRightClick);
}

void LLNetMap::handleClearMarks()
{
	clearAvatarMarkColors();
}

// static
bool LLNetMap::getAvatarMarkColor(const LLUUID& avatar_id, LLColor4& color)
{
	avatar_marks_map_t::iterator found = sAvatarMarksMap.find(avatar_id);
	if (found != sAvatarMarksMap.end())
	{
		color = found->second;
		return true;
	}
	return false;
}

// static
void LLNetMap::setAvatarMarkColor(const LLUUID& avatar_id, const LLSD& color)
{
	uuid_vec_t ids;
	ids.push_back(avatar_id);
	setAvatarMarkColors(ids, color);
}

// static
void LLNetMap::setAvatarMarkColors(const uuid_vec_t& avatar_ids, const LLSD& color)
{
	// Use the name as color definition name from colors.xml
	LLColor4 mark_color = LLUIColorTable::instance().getColor(color.asString(), LLColor4::green);

	for (uuid_vec_t::const_iterator it = avatar_ids.begin(); it != avatar_ids.end(); ++it)
	{
		sAvatarMarksMap[*it] = mark_color;
	}
}

// static
void LLNetMap::clearAvatarMarkColor(const LLUUID& avatar_id)
{
	uuid_vec_t ids;
	ids.push_back(avatar_id);
	clearAvatarMarkColors(ids);
}

// static
void LLNetMap::clearAvatarMarkColors(const uuid_vec_t& avatar_ids)
{
	for (uuid_vec_t::const_iterator it = avatar_ids.begin(); it != avatar_ids.end(); ++it)
	{
		avatar_marks_map_t::iterator found = sAvatarMarksMap.find(*it);
		if (found != sAvatarMarksMap.end())
		{
			sAvatarMarksMap.erase(found);
		}
	}
}

// static
void LLNetMap::clearAvatarMarkColors()
{
	sAvatarMarksMap.clear();
}

// static
LLColor4 LLNetMap::getAvatarColor(const LLUUID& avatar_id)
{
	static LLUIColor map_avatar_color = LLUIColorTable::instance().getColor("MapAvatarColor", LLColor4::white);
	LLColor4 color = map_avatar_color;

	LGGContactSets& cs_instance = LGGContactSets::instance();

	// Color "special" avatars with special colors (Friends, muted, Lindens, etc)
	color = cs_instance.colorize(avatar_id, color, LGG_CS_MINIMAP);

	// Color based on contact sets prefs
	if (cs_instance.hasFriendColorThatShouldShow(avatar_id, LGG_CS_MINIMAP))
	{
		color = cs_instance.getFriendColor(avatar_id);
	}

	// Mark Avatars with special colors
	avatar_marks_map_t::iterator found = sAvatarMarksMap.find(avatar_id);
	if (found != sAvatarMarksMap.end())
	{
		color = found->second;
	}

	return color;
}
//</FS:Ansariel>


void LLNetMap::handleStopTracking (const LLSD& userdata)
{
	auto menu = static_cast<LLMenuGL*>(mPopupMenuHandle.get());
	if (menu)
	{
		menu->setItemEnabled ("Stop Tracking", false);
		LLTracker::stopTracking (LLTracker::isTracking(nullptr));
	}
}
void LLNetMap::handleAddToContactSet()
{
	LLAvatarActions::addToContactSet(mClosestAgentsRightClick);
}
