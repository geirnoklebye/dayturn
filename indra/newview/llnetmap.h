/** 
 * @file llnetmap.h
 * @brief A little map of the world with network information
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

#ifndef LL_LLNETMAP_H
#define LL_LLNETMAP_H

#include "llmath.h"
#include "lluictrl.h"
#include "v3math.h"
#include "v3dmath.h"
#include "v4color.h"
#include "llpointer.h"
#include "llcoord.h"

class LLColor4U;
class LLImageRaw;
class LLViewerTexture;
class LLFloaterMap;
class LLMenuGL;
class LLViewerRegion;

class LLNetMap : public LLUICtrl
{
public:
	struct Params 
	:	public LLInitParam::Block<Params, LLUICtrl::Params>
	{
		Optional<LLUIColor>	bg_color;

		Params()
		:	bg_color("bg_color") 
		{}
	};

protected:
	LLNetMap (const Params & p);
	friend class LLUICtrlFactory;
	friend class LLFloaterMap;

public:
	virtual ~LLNetMap();

	static const F32 MAP_SCALE_MIN;
	static const F32 MAP_SCALE_MID;
	static const F32 MAP_SCALE_MAX;

	/*virtual*/ void	draw();
	/*virtual*/ bool	handleScrollWheel(S32 x, S32 y, S32 clicks);
	/*virtual*/ bool	handleMouseDown(S32 x, S32 y, MASK mask);
	/*virtual*/ bool	handleMouseUp(S32 x, S32 y, MASK mask);
	/*virtual*/ bool	handleHover( S32 x, S32 y, MASK mask );
	/*virtual*/ bool	handleToolTip( S32 x, S32 y, MASK mask);
	/*virtual*/ void	reshape(S32 width, S32 height, bool called_from_parent = true);

	/*virtual*/ bool	handleMiddleMouseDown(S32 x, S32 y, MASK mask) { return handleMouseDown(x, y, mask | MASK_SHIFT); }
	/*virtual*/ bool	handleMiddleMouseUp(S32 x, S32 y, MASK mask) { return handleMouseUp(x, y, mask); }

	/*virtual*/ bool 	postBuild();
	/*virtual*/ bool	handleRightMouseDown( S32 x, S32 y, MASK mask );
	/*virtual*/ bool	handleClick(S32 x, S32 y, MASK mask);
	/*virtual*/ bool	handleDoubleClick( S32 x, S32 y, MASK mask );

	void			refreshParcelOverlay() { mUpdateParcelImage = true; }

    void            setScale(F32 scale);

    void            setToolTipMsg(const std::string& msg) { mToolTipMsg = msg; }
    void            setParcelNameMsg(const std::string& msg) { mParcelNameMsg = msg; }
    void            setParcelSalePriceMsg(const std::string& msg) { mParcelSalePriceMsg = msg; }
    void            setParcelSaleAreaMsg(const std::string& msg) { mParcelSaleAreaMsg = msg; }
    void            setParcelOwnerMsg(const std::string& msg) { mParcelOwnerMsg = msg; }
    void            setRegionNameMsg(const std::string& msg) { mRegionNameMsg = msg; }
    void            setToolTipHintMsg(const std::string& msg) { mToolTipHintMsg = msg; }
    void            setAltToolTipHintMsg(const std::string& msg) { mAltToolTipHintMsg = msg; }

	void			renderScaledPointGlobal( const LLVector3d& pos, const LLColor4U &color, F32 radius );

	// <FS:Ansariel> Mark avatar feature
	static bool		hasAvatarMarkColor(const LLUUID& avatar_id) { return sAvatarMarksMap.find(avatar_id) != sAvatarMarksMap.end(); }
	static bool		getAvatarMarkColor(const LLUUID& avatar_id, LLColor4& color);
	static void		setAvatarMarkColor(const LLUUID& avatar_id, const LLSD& color);
	static void		setAvatarMarkColors(const uuid_vec_t& avatar_ids, const LLSD& color);
	static void		clearAvatarMarkColor(const LLUUID& avatar_id);
	static void		clearAvatarMarkColors(const uuid_vec_t& avatar_ids);
	static void		clearAvatarMarkColors();
	static LLColor4	getAvatarColor(const LLUUID& avatar_id);
	// </FS:Ansariel>
private:
	const LLVector3d& getObjectImageCenterGlobal()	{ return mObjectImageCenterGlobal; }
	void 			renderPoint(const LLVector3 &pos, const LLColor4U &color, 
								S32 diameter, S32 relative_height = 0);

	LLVector3		globalPosToView(const LLVector3d& global_pos);
	LLVector3d		viewPosToGlobal(S32 x,S32 y);

	void			drawRing(const F32 radius, LLVector3 pos_map, const LLUIColor& colour);
	void			drawTracking( const LLVector3d& pos_global, 
								  const LLColor4& color,
								  bool draw_arrow = true);
	bool			handleToolTipAgent(const LLUUID& avatar_id);
	static void		showAvatarInspector(const LLUUID& avatar_id);
	bool			createImage(LLPointer<LLImageRaw>& rawimagep) const;
	void			createObjectImage();
	void			createParcelImage();
	void			renderPropertyLinesForRegion(const LLViewerRegion* region);

	static bool		outsideSlop(S32 x, S32 y, S32 start_x, S32 start_y, S32 slop);

	bool			mUpdateObjectImage;
	bool			mUpdateParcelImage;

	LLUIColor		mBackgroundColor;

	F32				mScale;					// Size of a region in pixels
	F32				mPixelsPerMeter;		// world meters to map pixels
	F32				mObjectMapTPM;			// texels per meter on map
	F32				mObjectMapPixels;		// Width of object map in pixels
	F32				mDotRadius;				// Size of avatar markers

	bool			mPanning;			// map is being dragged
	LLVector2		mTargetPan;
	LLVector2		mCurPan;
	LLVector2		mStartPan;		// pan offset at start of drag
	LLCoordGL		mMouseDown;			// pointer position at start of drag

	LLVector3d		mObjectImageCenterGlobal;
	LLPointer<LLImageRaw> mObjectRawImagep;
	LLPointer<LLViewerTexture>	mObjectImagep;
	LLVector3d			mParcelImageCenterGlobal;
	LLPointer<LLImageRaw>		mParcelRawImagep;
	LLPointer<LLViewerTexture>	mParcelImagep;

	LLUUID				mClosestAgentToCursor;
	LLVector3d			mClosestAgentPosition;

    std::string     mToolTipMsg;
    std::string     mParcelNameMsg;
    std::string     mParcelSalePriceMsg;
    std::string     mParcelSaleAreaMsg;
    std::string     mParcelOwnerMsg;
    std::string     mRegionNameMsg;
    std::string     mToolTipHintMsg;
    std::string     mAltToolTipHintMsg;

	// <FS:Ansariel> Mark avatar feature
	typedef std::map<LLUUID, LLColor4> avatar_marks_map_t;
	static avatar_marks_map_t sAvatarMarksMap;
public:
	void			setSelected(uuid_vec_t uuids) { sSelected=uuids; };
// <FS:CR> Minimap improvements
	void			handleShowProfile(const LLSD& sdParam) const;
	uuid_vec_t		mClosestAgentsToCursor;
	LLVector3d		mPosGlobalRightClick;
	LLUUID			mClosestAgentRightClick;
	uuid_vec_t		mClosestAgentsRightClick;
// </FS:CR>
private:
	void handleZoom(const LLSD& userdata);
	void handleStopTracking (const LLSD& userdata);
	void handleMark(const LLSD& userdata);
	void handleClearMark();
	void handleClearMarks();
	void handleOverlayToggle(const LLSD& sdParam);

	void handleAddToContactSet();
    LLHandle<LLView> mPopupMenuHandle;
	static uuid_vec_t	sSelected;
};


#endif
