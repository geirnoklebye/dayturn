/** 
 * @file llfloatertools.cpp
 * @brief The edit tools, including move, position, land, etc.
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

#include "llviewerprecompiledheaders.h"

#include "llfloatertools.h"

#include "llfontgl.h"
#include "llcoord.h"
//#include "llgl.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldraghandle.h"
#include "llerror.h"
#include "llfloaterbuildoptions.h"
#include "llfloatermediasettings.h"
#include "llfloateropenobject.h"
#include "llfloaterobjectweights.h"
#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "llmediaentry.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "llpanelcontents.h"
#include "llpanelface.h"
#include "llpanelland.h"
#include "llpanelobjectinventory.h"
#include "llpanelobject.h"
#include "llpanelvolume.h"
#include "llpanelpermissions.h"
#include "llparcel.h"
#include "llradiogroup.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llslider.h"
#include "llspinctrl.h"
#include "llstatusbar.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltoolbrush.h"
#include "lltoolcomp.h"
#include "lltooldraganddrop.h"
#include "lltoolface.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolgrab.h"
#include "lltoolindividual.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "lltoolpipette.h"
#include "lltoolplacer.h"
#include "lltoolselectland.h"
#include "lltrans.h"
#include "llui.h"
#include "llviewercontrol.h"
#include "llviewerjoystick.h"
#include "llviewerregion.h"
#include "llviewermenu.h"
#include "llviewernetwork.h"
#include "llviewerparcelmgr.h"
#include "llviewerwindow.h"
#include "llvovolume.h"
#include "lluictrlfactory.h"
#include "qtoolalign.h"
#include "llmeshrepository.h"
#include "llworld.h"

// additional includes for right click passes to selection feature
#include "llmutelist.h"
#include "llviewermenu.h"

// Globals
LLFloaterTools *gFloaterTools = NULL;
bool LLFloaterTools::sShowObjectCost = true;
bool LLFloaterTools::sPreviousFocusOnAvatar = false;

const std::string PANEL_NAMES[LLFloaterTools::PANEL_COUNT] =
{
	std::string("General"), 	// PANEL_GENERAL,
	std::string("Object"), 	// PANEL_OBJECT,
	std::string("Features"),	// PANEL_FEATURES,
	std::string("Texture"),	// PANEL_FACE,
	std::string("Content"),	// PANEL_CONTENTS,
};


// Local prototypes
void commit_grid_mode(LLUICtrl *ctrl);
void commit_select_component(void *data);
void click_show_more(void*);
void click_popup_info(void*);
void click_popup_done(void*);
void click_popup_minimize(void*);
void commit_slider_dozer_force(LLUICtrl *);
void click_apply_to_selection(void*);
void commit_radio_group_focus(LLUICtrl* ctrl);
void commit_radio_group_move(LLUICtrl* ctrl);
void commit_radio_group_edit(LLUICtrl* ctrl);
void commit_radio_group_land(LLUICtrl* ctrl);
void commit_slider_zoom(LLUICtrl *ctrl);

/**
 * Class LLLandImpactsObserver
 *
 * An observer class to monitor parcel selection and update
 * the land impacts data from a parcel containing the selected object.
 */
class LLLandImpactsObserver : public LLParcelObserver
{
public:
	virtual void changed()
	{
		LLFloaterTools* tools_floater = LLFloaterReg::getTypedInstance<LLFloaterTools>("build");
		if(tools_floater)
		{
			tools_floater->updateLandImpacts();
		}
	}
};

//static
void*	LLFloaterTools::createPanelPermissions(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelPermissions = new LLPanelPermissions();
	return floater->mPanelPermissions;
}
//static
void*	LLFloaterTools::createPanelObject(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelObject = new LLPanelObject();
	return floater->mPanelObject;
}

//static
void*	LLFloaterTools::createPanelVolume(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelVolume = new LLPanelVolume();
	return floater->mPanelVolume;
}

//static
void*	LLFloaterTools::createPanelFace(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelFace = new LLPanelFace();
	return floater->mPanelFace;
}

//static
void*	LLFloaterTools::createPanelContents(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelContents = new LLPanelContents();
	return floater->mPanelContents;
}

//static
void*	LLFloaterTools::createPanelLandInfo(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelLandInfo = new LLPanelLandInfo();
	return floater->mPanelLandInfo;
}

static	const std::string	toolNames[]={
	"ToolCube",
	"ToolPrism",
	"ToolPyramid",
	"ToolTetrahedron",
	"ToolCylinder",
	"ToolHemiCylinder",
	"ToolCone",
	"ToolHemiCone",
	"ToolSphere",
	"ToolHemiSphere",
	"ToolTorus",
	"ToolTube",
	"ToolRing",
	"ToolTree",
	"ToolGrass"};
LLPCode toolData[]={
	LL_PCODE_CUBE,
	LL_PCODE_PRISM,
	LL_PCODE_PYRAMID,
	LL_PCODE_TETRAHEDRON,
	LL_PCODE_CYLINDER,
	LL_PCODE_CYLINDER_HEMI,
	LL_PCODE_CONE,
	LL_PCODE_CONE_HEMI,
	LL_PCODE_SPHERE,
	LL_PCODE_SPHERE_HEMI,
	LL_PCODE_TORUS,
	LLViewerObject::LL_VO_SQUARE_TORUS,
	LLViewerObject::LL_VO_TRIANGLE_TORUS,
	LL_PCODE_LEGACY_TREE,
	LL_PCODE_LEGACY_GRASS};

bool	LLFloaterTools::postBuild()
{	
	// Hide until tool selected
	setVisible(false);

	// Since we constantly show and hide this during drags, don't
	// make sounds on visibility changes.
	setSoundFlags(LLView::SILENT);

	getDragHandle()->setEnabled( !gSavedSettings.getbool("ToolboxAutoMove") );

	LLRect rect;
	mBtnFocus			= getChild<LLButton>("button focus");//btn;
	mBtnMove			= getChild<LLButton>("button move");
	mBtnEdit			= getChild<LLButton>("button edit");
	mBtnCreate			= getChild<LLButton>("button create");
	mBtnLand			= getChild<LLButton>("button land" );
	mTextStatus			= getChild<LLTextBox>("text status");
	mRadioGroupFocus	= getChild<LLRadioGroup>("focus_radio_group");
	mRadioGroupMove		= getChild<LLRadioGroup>("move_radio_group");
	mRadioGroupEdit		= getChild<LLRadioGroup>("edit_radio_group");
	mBtnGridOptions		= getChild<LLButton>("Options...");
	mBtnLink			= getChild<LLButton>("link_btn");
	mBtnUnlink			= getChild<LLButton>("unlink_btn");
	mBtnGiveMenu    = getChild<LLButton>("give_menu");

	// <FS:PP> FIRE-14493: Buttons to cycle through linkset
	mBtnPrevPart		= getChild<LLButton>("prev_part_btn");
	mBtnNextPart		= getChild<LLButton>("next_part_btn");
	// </FS:PP>
	
	mCheckSelectIndividual	= getChild<LLCheckBoxCtrl>("checkbox edit linked parts");	
	getChild<LLUICtrl>("checkbox edit linked parts")->setValue(gSavedSettings.getbool("EditLinkedParts"));
	mCheckSnapToGrid		= getChild<LLCheckBoxCtrl>("checkbox snap to grid");
	getChild<LLUICtrl>("checkbox snap to grid")->setValue(gSavedSettings.getbool("SnapEnabled"));
	mCheckStretchUniform	= getChild<LLCheckBoxCtrl>("checkbox uniform");
	getChild<LLUICtrl>("checkbox uniform")->setValue(gSavedSettings.getbool("ScaleUniform"));
	mCheckStretchTexture	= getChild<LLCheckBoxCtrl>("checkbox stretch textures");
	getChild<LLUICtrl>("checkbox stretch textures")->setValue(gSavedSettings.getbool("ScaleStretchTextures"));
	mComboGridMode			= getChild<LLComboBox>("combobox grid mode");

	//
	// Create Buttons
	//

	for(size_t t=0; t<LL_ARRAY_SIZE(toolNames); ++t)
	{
		LLButton *found = getChild<LLButton>(toolNames[t]);
		if(found)
		{
			found->setClickedCallback(boost::bind(&LLFloaterTools::setObjectType, toolData[t]));
			mButtons.push_back( found );
		}else{
			LL_WARNS() << "Tool button not found! DOA Pending." << LL_ENDL;
		}
	}
	mCheckCopySelection = getChild<LLCheckBoxCtrl>("checkbox copy selection");
	getChild<LLUICtrl>("checkbox copy selection")->setValue(gSavedSettings.getbool("CreateToolCopySelection"));
	mCheckSticky = getChild<LLCheckBoxCtrl>("checkbox sticky");
	getChild<LLUICtrl>("checkbox sticky")->setValue(gSavedSettings.getbool("CreateToolKeepSelected"));
	mCheckCopyCenters = getChild<LLCheckBoxCtrl>("checkbox copy centers");
	getChild<LLUICtrl>("checkbox copy centers")->setValue(gSavedSettings.getbool("CreateToolCopyCenters"));
	mCheckCopyRotates = getChild<LLCheckBoxCtrl>("checkbox copy rotates");
	getChild<LLUICtrl>("checkbox copy rotates")->setValue(gSavedSettings.getbool("CreateToolCopyRotates"));

	mRadioGroupLand			= getChild<LLRadioGroup>("land_radio_group");
	mBtnApplyToSelection	= getChild<LLButton>("button apply to selection");
	mSliderDozerSize		= getChild<LLSlider>("slider brush size");
	getChild<LLUICtrl>("slider brush size")->setValue(gSavedSettings.getF32("LandBrushSize"));
	mSliderDozerForce		= getChild<LLSlider>("slider force");
	// the setting stores the actual force multiplier, but the slider is logarithmic, so we convert here
	getChild<LLUICtrl>("slider force")->setValue(log10(gSavedSettings.getF32("LandBrushForce")));

	mCostTextBorder = getChild<LLViewBorder>("cost_text_border");

	mTab = getChild<LLTabContainer>("Object Info Tabs");
	if(mTab)
	{
		mTab->setFollows(FOLLOWS_TOP | FOLLOWS_LEFT);
		mTab->setBorderVisible(false);
		mTab->selectFirstTab();
	}

	mStatusText["rotate"] = getString("status_rotate");
	mStatusText["scale"] = getString("status_scale");
	mStatusText["move"] = getString("status_move");
	mStatusText["modifyland"] = getString("status_modifyland");
	mStatusText["camera"] = getString("status_camera");
	mStatusText["grab"] = getString("status_grab");
	mStatusText["place"] = getString("status_place");
	mStatusText["selectland"] = getString("status_selectland");

	sShowObjectCost = gSavedSettings.getbool("ShowObjectRenderingCost");
	
	return true;
}

// Create the popupview with a dummy center.  It will be moved into place
// during LLViewerWindow's per-frame hover processing.
LLFloaterTools::LLFloaterTools(const LLSD& key)
:	LLFloater(key),
	mBtnFocus(NULL),
	mBtnMove(NULL),
	mBtnEdit(NULL),
	mBtnCreate(NULL),
	mBtnLand(NULL),
	mTextStatus(NULL),

	mRadioGroupFocus(NULL),
	mRadioGroupMove(NULL),
	mRadioGroupEdit(NULL),

	mCheckSelectIndividual(NULL),

	mCheckSnapToGrid(NULL),
	mBtnGridOptions(NULL),
	mComboGridMode(NULL),
	mCheckStretchUniform(NULL),
	mCheckStretchTexture(NULL),
	mCheckStretchUniformLabel(NULL),

	mBtnRotateLeft(NULL),
	mBtnRotateReset(NULL),
	mBtnRotateRight(NULL),

	mBtnLink(NULL),
	mBtnUnlink(NULL),
	mBtnGiveMenu(NULL),

	// <FS:PP> FIRE-14493: Buttons to cycle through linkset
	mBtnPrevPart(NULL),
	mBtnNextPart(NULL),
	// </FS:PP>

	mBtnDelete(NULL),
	mBtnDuplicate(NULL),
	mBtnDuplicateInPlace(NULL),

	mCheckSticky(NULL),
	mCheckCopySelection(NULL),
	mCheckCopyCenters(NULL),
	mCheckCopyRotates(NULL),
	mRadioGroupLand(NULL),
	mSliderDozerSize(NULL),
	mSliderDozerForce(NULL),
	mBtnApplyToSelection(NULL),

	mTab(NULL),
	mPanelPermissions(NULL),
	mPanelObject(NULL),
	mPanelVolume(NULL),
	mPanelContents(NULL),
	mPanelFace(NULL),
	mPanelLandInfo(NULL),

	mCostTextBorder(NULL),
	mTabLand(NULL),

	mLandImpactsObserver(NULL),

	mDirty(true),
	mHasSelection(true)
{
	gFloaterTools = this;

	setAutoFocus(false);
	mFactoryMap["General"] = LLCallbackMap(createPanelPermissions, this);//LLPanelPermissions
	mFactoryMap["Object"] = LLCallbackMap(createPanelObject, this);//LLPanelObject
	mFactoryMap["Features"] = LLCallbackMap(createPanelVolume, this);//LLPanelVolume
	mFactoryMap["Texture"] = LLCallbackMap(createPanelFace, this);//LLPanelFace
	mFactoryMap["Contents"] = LLCallbackMap(createPanelContents, this);//LLPanelContents
	mFactoryMap["land info panel"] = LLCallbackMap(createPanelLandInfo, this);//LLPanelLandInfo
	
	mCommitCallbackRegistrar.add("BuildTool.setTool",			boost::bind(&LLFloaterTools::setTool,this, _2));
	mCommitCallbackRegistrar.add("BuildTool.commitZoom",		boost::bind(&commit_slider_zoom, _1));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioFocus",	boost::bind(&commit_radio_group_focus, _1));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioMove",	boost::bind(&commit_radio_group_move,_1));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioEdit",	boost::bind(&commit_radio_group_edit,_1));

	mCommitCallbackRegistrar.add("BuildTool.gridMode",			boost::bind(&commit_grid_mode,_1));
	mCommitCallbackRegistrar.add("BuildTool.selectComponent",	boost::bind(&commit_select_component, this));
	mCommitCallbackRegistrar.add("BuildTool.gridOptions",		boost::bind(&LLFloaterTools::onClickGridOptions,this));
	mCommitCallbackRegistrar.add("BuildTool.applyToSelection",	boost::bind(&click_apply_to_selection, this));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioLand",	boost::bind(&commit_radio_group_land,_1));
	mCommitCallbackRegistrar.add("BuildTool.LandBrushForce",	boost::bind(&commit_slider_dozer_force,_1));
	mCommitCallbackRegistrar.add("BuildTool.GiveMenu",			boost::bind(&LLFloaterTools::onClickBtnGiveMenu,this));

	mCommitCallbackRegistrar.add("BuildTool.LinkObjects",		boost::bind(&LLSelectMgr::linkObjects, LLSelectMgr::getInstance()));
	mCommitCallbackRegistrar.add("BuildTool.UnlinkObjects",		boost::bind(&LLSelectMgr::unlinkObjects, LLSelectMgr::getInstance()));
	mCommitCallbackRegistrar.add("BuildTool.CopyKeys",			boost::bind(&LLFloaterTools::onClickBtnCopyKeys,this));
	mLandImpactsObserver = new LLLandImpactsObserver();
	LLViewerParcelMgr::getInstance()->addObserver(mLandImpactsObserver);
}

LLFloaterTools::~LLFloaterTools()
{
	// children automatically deleted
	gFloaterTools = NULL;

	LLViewerParcelMgr::getInstance()->removeObserver(mLandImpactsObserver);
	delete mLandImpactsObserver;
}

void LLFloaterTools::setStatusText(const std::string& text)
{
	// <FS:ND> Can be 0 during login
	if( !mTextStatus )
		return;
	// </FS:ND>

	std::map<std::string, std::string>::iterator iter = mStatusText.find(text);
	if (iter != mStatusText.end())
	{
		mTextStatus->setText(iter->second);
	}
	else
	{
		mTextStatus->setText(text);
	}
}

void LLFloaterTools::refresh()
{
	const S32 INFO_WIDTH = getRect().getWidth();
	const S32 INFO_HEIGHT = 384;
	LLRect object_info_rect(0, 0, INFO_WIDTH, -INFO_HEIGHT);
	bool all_volume = LLSelectMgr::getInstance()->selectionAllPCode( LL_PCODE_VOLUME );

	S32 idx_features = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_FEATURES]);
	S32 idx_face = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_FACE]);
	S32 idx_contents = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_CONTENTS]);

	S32 selected_index = mTab->getCurrentPanelIndex();

	if (!all_volume && (selected_index == idx_features || selected_index == idx_face ||
		selected_index == idx_contents))
	{
		mTab->selectFirstTab();
	}

	mTab->enableTabButton(idx_features, all_volume);
	mTab->enableTabButton(idx_face, all_volume);
	mTab->enableTabButton(idx_contents, all_volume);

	// Refresh object and prim count labels
	LLLocale locale(LLLocale::USER_LOCALE);
	
	// <FS:KC>
	// KKA-744 Various tweaks for more information
	std::string desc_string;
	std::string num_string;
	S32 linkset_num = 0;
	S32 faces_num = 0;
	S32 prim_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();
		
	//KKA-744 Show the linkset number when working with faces too, so we need it either way
	if (prim_count == 1)
	{
		LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
		if (objectp && objectp->getRootEdit())
		{
			LLViewerObject::child_list_t children = objectp->getRootEdit()->getChildren();
			if (children.empty())
			{
				linkset_num = 0;
			}
			else if (objectp->getRootEdit()->isSelected())
				linkset_num = 1; //root prim is always link one
			else
			{
				S32 index = 1;
				for (LLViewerObject::child_list_t::iterator iter = children.begin(); iter != children.end(); ++iter)
				{
					index++;
					if ((*iter)->isSelected())
					{
						linkset_num = index;
						break;
					}
				}
			}
			faces_num = objectp->getNumTEs();
		}
	}

	if (prim_count == 1 && LLToolMgr::getInstance()->getCurrentTool() == LLToolFace::getInstance())
	{
		//KKA-744 show the linkset id as well as face number(s)
		desc_string = getString("link_number") + llformat(" %d  ",linkset_num) + getString("selected_faces");
		
		LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
		LLSelectNode* nodep = LLSelectMgr::getInstance()->getSelection()->getFirstRootNode();
		if(!objectp || !nodep)
		{
			objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
			nodep = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
		}

		if (objectp && faces_num == LLSelectMgr::getInstance()->getSelection()->getTECount())
			num_string = "ALL_SIDES";
		else if (objectp && nodep)
		{
			//S32 count = 0;
			for (S32 i = 0; i < faces_num; i++)
			{
				if (nodep->isTESelected(i))
				{
					if (!num_string.empty())
						num_string.append(", ");
					num_string.append(llformat("%d", i));
					//count++;
				}
			}
		}
	}
	else if (prim_count == 1 && gSavedSettings.getbool("EditLinkedParts"))
	{
		//KKA-744 display total number of faces when one link item is selected
		desc_string = getString("link_number");
		num_string = llformat("%d ",linkset_num) + getString("total_faces") + llformat(" %d",faces_num);
	}
	else
	{
		//KKA-744 display a selected linkset total if we can't show something more detailed
		//...providing there's a selection
		if ( ! LLSelectMgr::getInstance()->getSelection()->isEmpty())
		{
			desc_string = getString("link_total");
			num_string = llformat("%d",LLSelectMgr::getInstance()->getSelection()->getObjectCount());
		}
	}

	getChild<LLUICtrl>("link_num_obj_count")->setTextArg("[DESC]", desc_string);
	getChild<LLUICtrl>("link_num_obj_count")->setTextArg("[NUM]", num_string);
	// </FS:KC>
#if 0
	if (!gMeshRepo.meshRezEnabled())
	{		
		std::string obj_count_string;
		LLResMgr::getInstance()->getIntegerString(obj_count_string, LLSelectMgr::getInstance()->getSelection()->getRootObjectCount());
		getChild<LLUICtrl>("selection_count")->setTextArg("[OBJ_COUNT]", obj_count_string);
		std::string prim_count_string;
		LLResMgr::getInstance()->getIntegerString(prim_count_string, LLSelectMgr::getInstance()->getSelection()->getObjectCount());
		getChild<LLUICtrl>("selection_count")->setTextArg("[PRIM_COUNT]", prim_count_string);

		// calculate selection rendering cost
		if (sShowObjectCost)
		{
			std::string prim_cost_string;
			S32 render_cost = LLSelectMgr::getInstance()->getSelection()->getSelectedObjectRenderCost();
			LLResMgr::getInstance()->getIntegerString(prim_cost_string, render_cost);
			getChild<LLUICtrl>("RenderingCost")->setTextArg("[COUNT]", prim_cost_string);
		}
		
		// disable the object and prim counts if nothing selected
		bool have_selection = ! LLSelectMgr::getInstance()->getSelection()->isEmpty();
		getChildView("link_num_obj_count")->setEnabled(have_selection);
		getChildView("obj_count")->setEnabled(have_selection);
		getChildView("prim_count")->setEnabled(have_selection);
		getChildView("RenderingCost")->setEnabled(have_selection && sShowObjectCost);
	}
	else
#endif
	{
        LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
        S32 prim_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();
        F32 link_cost = selection->getSelectedLinksetCost();
        S32 link_count = selection->getRootObjectCount();
        S32 object_count = selection->getObjectCount();

		LLCrossParcelFunctor func;
		if (LLSelectMgr::getInstance()->getSelection()->applyToRootObjects(&func, true))
		{
			// Selection crosses parcel bounds.
			// We don't display remaining land capacity in this case.
			const LLStringExplicit empty_str("");
			childSetTextArg("remaining_capacity", "[CAPACITY_STRING]", empty_str);
		}
		else
		{
			LLViewerObject* selected_object = mObjectSelection->getFirstObject();
			if (selected_object)
			{
				// Select a parcel at the currently selected object's position.
				// <FS:Ansariel> FIRE-20387: Editing HUD attachment shows [CAPACITY_STRING] in tools floater
				//LLViewerParcelMgr::getInstance()->selectParcelAt(selected_object->getPositionGlobal());
				if (!selected_object->isAttachment())
				{
					LLViewerParcelMgr::getInstance()->selectParcelAt(selected_object->getPositionGlobal());
				}
				else
				{
					const LLStringExplicit empty_str("");
					childSetTextArg("remaining_capacity", "[CAPACITY_STRING]", empty_str);
				}
				// </FS:Ansariel>
            }
            else
            {
                LL_WARNS() << "Failed to get selected object" << LL_ENDL;
            }
        }

        if (object_count == 1)
        {
            // "selection_faces" shouldn't be visible if not LLToolFace::getInstance()
            // But still need to be populated in case user switches

            std::string faces_str = "";

            for (LLObjectSelection::iterator iter = selection->begin(); iter != selection->end();)
            {
                LLObjectSelection::iterator nextiter = iter++; // not strictly needed, we have only one object
                LLSelectNode* node = *nextiter;
                LLViewerObject* object = (*nextiter)->getObject();
                if (!object)
                    continue;
                S32 num_tes = llmin((S32)object->getNumTEs(), (S32)object->getNumFaces());
                for (S32 te = 0; te < num_tes; ++te)
                {
                    if (node->isTESelected(te))
                    {
                        if (!faces_str.empty())
                        {
                            faces_str += ", ";
                        }
                        faces_str += llformat("%d", te);
                    }
                }
            }

            childSetTextArg("selection_faces", "[FACES_STRING]", faces_str);
        }

        bool show_faces = (object_count == 1)
                          && LLToolFace::getInstance() == LLToolMgr::getInstance()->getCurrentTool();
        getChildView("selection_faces")->setVisible(show_faces);

		LLStringUtil::format_map_t selection_args;
		selection_args["OBJ_COUNT"] = llformat("%.1d", link_count);
		if (((S32)link_cost) == 0)
		{
			selection_args["LAND_IMPACT"] = llformat("%.1d", (S32)prim_count);
		}
		else
		{
		selection_args["LAND_IMPACT"] = llformat("%.1d", (S32)link_cost);
		}
		std::ostringstream selection_info;

		selection_info << getString("status_selectcount", selection_args);

		getChild<LLTextBox>("selection_count")->setText(selection_info.str());
	}

	// <FS> disable the object and prim counts if nothing selected
	// KKA-744 we now toggle visibility rather than enabled and always have something to show if there's a selection
	bool have_selection = ! LLSelectMgr::getInstance()->getSelection()->isEmpty();
	getChildView("link_num_obj_count")->setVisible(have_selection);
	// </FS>

	// Refresh child tabs
	mPanelPermissions->refresh();
	mPanelObject->refresh();
	mPanelVolume->refresh();
	mPanelFace->refresh();
    mPanelFace->refreshMedia();
	mPanelContents->refresh();
	mPanelLandInfo->refresh();

	// Refresh the advanced weights floater
	LLFloaterObjectWeights* object_weights_floater = LLFloaterReg::findTypedInstance<LLFloaterObjectWeights>("object_weights");
	if(object_weights_floater && object_weights_floater->getVisible())
	{
		object_weights_floater->refresh();
	}
}

void LLFloaterTools::draw()
{
    bool has_selection = !LLSelectMgr::getInstance()->getSelection()->isEmpty();
    if(!has_selection && (mHasSelection != has_selection))
    {
        mDirty = true;
    }
    mHasSelection = has_selection;

    if (mDirty)
	{
		refresh();
		mDirty = false;
	}

	//	mCheckSelectIndividual->set(gSavedSettings.getbool("EditLinkedParts"));
	LLFloater::draw();
}

void LLFloaterTools::dirty()
{
	mDirty = true;
	LLFloaterOpenObject* instance = LLFloaterReg::findTypedInstance<LLFloaterOpenObject>("openobject");
	if (instance) instance->dirty();
}

// Clean up any tool state that should not persist when the
// floater is closed.
void LLFloaterTools::resetToolState()
{
	gCameraBtnZoom = true;
	gCameraBtnOrbit = false;
	gCameraBtnPan = false;

	gGrabBtnSpin = false;
	gGrabBtnVertical = false;
}

void LLFloaterTools::updatePopup(LLCoordGL center, MASK mask)
{
	LLTool *tool = LLToolMgr::getInstance()->getCurrentTool();

	// HACK to allow seeing the buttons when you have the app in a window.
	// Keep the visibility the same as it 
	if (tool == gToolNull)
	{
		return;
	}

	if ( isMinimized() )
	{	// SL looks odd if we draw the tools while the window is minimized
		return;
	}
	
	// Focus buttons
	bool focus_visible = (	tool == LLToolCamera::getInstance() );

	mBtnFocus	->setToggleState( focus_visible );

	mRadioGroupFocus->setVisible( focus_visible );
	getChildView("slider zoom")->setVisible( focus_visible);
	getChildView("slider zoom")->setEnabled(gCameraBtnZoom);

	if (!gCameraBtnOrbit &&
		!gCameraBtnPan &&
		!(mask == MASK_ORBIT) &&
		!(mask == (MASK_ORBIT | MASK_ALT)) &&
		!(mask == MASK_PAN) &&
		!(mask == (MASK_PAN | MASK_ALT)) )
	{
		mRadioGroupFocus->setValue("radio zoom");
	}
	else if (	gCameraBtnOrbit || 
				(mask == MASK_ORBIT) ||
				(mask == (MASK_ORBIT | MASK_ALT)) )
	{
		mRadioGroupFocus->setValue("radio orbit");
	}
	else if (	gCameraBtnPan ||
				(mask == MASK_PAN) ||
				(mask == (MASK_PAN | MASK_ALT)) )
	{
		mRadioGroupFocus->setValue("radio pan");
	}

	// multiply by correction factor because volume sliders go [0, 0.5]
	getChild<LLUICtrl>("slider zoom")->setValue(gAgentCamera.getCameraZoomFraction() * 0.5f);

	// Move buttons
	bool move_visible = (tool == LLToolGrab::getInstance());

	if (mBtnMove) mBtnMove	->setToggleState( move_visible );

	// HACK - highlight buttons for next click
	mRadioGroupMove->setVisible(move_visible);
	if (!(gGrabBtnSpin || 
		gGrabBtnVertical || 
		(mask == MASK_VERTICAL) || 
		(mask == MASK_SPIN)))
	{
		mRadioGroupMove->setValue("radio move");
	}
	else if ((mask == MASK_VERTICAL) ||
			 (gGrabBtnVertical && (mask != MASK_SPIN)))
	{
		mRadioGroupMove->setValue("radio lift");
	}
	else if ((mask == MASK_SPIN) || 
			 (gGrabBtnSpin && (mask != MASK_VERTICAL)))
	{
		mRadioGroupMove->setValue("radio spin");
	}

	// Edit buttons
	bool edit_visible = tool == LLToolCompTranslate::getInstance() ||
						tool == LLToolCompRotate::getInstance() ||
						tool == LLToolCompScale::getInstance() ||
						tool == LLToolFace::getInstance() ||
						tool == LLToolIndividual::getInstance() ||
						tool == QToolAlign::getInstance() ||
						tool == LLToolPipette::getInstance();

	mBtnEdit	->setToggleState( edit_visible );
	mRadioGroupEdit->setVisible( edit_visible );
	//bool linked_parts = gSavedSettings.getbool("EditLinkedParts");
	static LLCachedControl<bool> linked_parts(gSavedSettings,  "EditLinkedParts");	
	//getChildView("RenderingCost")->setVisible( !linked_parts && (edit_visible || focus_visible || move_visible) && sShowObjectCost);

	mBtnLink->setVisible(edit_visible);
	mBtnUnlink->setVisible(edit_visible);
	mBtnGiveMenu->setVisible(edit_visible);

	mBtnLink->setEnabled(LLSelectMgr::instance().enableLinkObjects());
	mBtnUnlink->setEnabled(LLSelectMgr::instance().enableUnlinkObjects());

	// <FS:PP> FIRE-14493: Buttons to cycle through linkset
	mBtnPrevPart->setVisible(edit_visible);
	mBtnNextPart->setVisible(edit_visible);

	bool select_btn_enabled = (!LLSelectMgr::getInstance()->getSelection()->isEmpty()
								&& (linked_parts || LLToolFace::getInstance() == LLToolMgr::getInstance()->getCurrentTool()));
	mBtnPrevPart->setEnabled(select_btn_enabled);
	mBtnNextPart->setEnabled(select_btn_enabled);
	// </FS:PP>

	if (mCheckSelectIndividual)
	{
		mCheckSelectIndividual->setVisible(edit_visible);
		//mCheckSelectIndividual->set(gSavedSettings.getbool("EditLinkedParts"));
	}

	if ( tool == LLToolCompTranslate::getInstance() )
	{
		mRadioGroupEdit->setValue("radio position");
	}
	else if ( tool == LLToolCompRotate::getInstance() )
	{
		mRadioGroupEdit->setValue("radio rotate");
	}
	else if ( tool == LLToolCompScale::getInstance() )
	{
		mRadioGroupEdit->setValue("radio stretch");
	}
	else if ( tool == LLToolFace::getInstance() )
	{
		mRadioGroupEdit->setValue("radio select face");
	}
	else if ( tool == QToolAlign::getInstance() )
	{
		mRadioGroupEdit->setValue("radio align");
	}
	if (mComboGridMode) 
	{
		mComboGridMode->setVisible( edit_visible );
		S32 index = mComboGridMode->getCurrentIndex();
		mComboGridMode->removeall();

		switch (mObjectSelection->getSelectType())
		{
			case SELECT_TYPE_HUD:
				mComboGridMode->add(getString("grid_screen_text"));
				mComboGridMode->add(getString("grid_local_text"));
				break;
			case SELECT_TYPE_WORLD:
				mComboGridMode->add(getString("grid_world_text"));
				mComboGridMode->add(getString("grid_local_text"));
				mComboGridMode->add(getString("grid_reference_text"));
				break;
			case SELECT_TYPE_ATTACHMENT:
				mComboGridMode->add(getString("grid_attachment_text"));
				mComboGridMode->add(getString("grid_local_text"));
				mComboGridMode->add(getString("grid_reference_text"));
				break;
		}

		mComboGridMode->setCurrentByIndex(index);
	}

	// Snap to grid disabled for grab tool - very confusing
	if (mCheckSnapToGrid) mCheckSnapToGrid->setVisible( edit_visible /* || tool == LLToolGrab::getInstance() */ );
	if (mBtnGridOptions) mBtnGridOptions->setVisible( edit_visible /* || tool == LLToolGrab::getInstance() */ );

	//mCheckSelectLinked	->setVisible( edit_visible );
	if (mCheckStretchUniform) mCheckStretchUniform->setVisible( edit_visible );
	if (mCheckStretchTexture) mCheckStretchTexture->setVisible( edit_visible );
	if (mCheckStretchUniformLabel) mCheckStretchUniformLabel->setVisible( edit_visible );

	// Create buttons
	bool create_visible = (tool == LLToolCompCreate::getInstance());

	mBtnCreate	->setToggleState(	tool == LLToolCompCreate::getInstance() );

	if (mCheckCopySelection
		&& mCheckCopySelection->get())
	{
		// don't highlight any placer button
		for (std::vector<LLButton*>::size_type i = 0; i < mButtons.size(); i++)
		{
			mButtons[i]->setToggleState(false);
			mButtons[i]->setVisible( create_visible );
		}
	}
	else
	{
		// Highlight the correct placer button
		for( S32 t = 0; t < (S32)mButtons.size(); t++ )
		{
			LLPCode pcode = LLToolPlacer::getObjectType();
			LLPCode button_pcode = toolData[t];
			bool state = (pcode == button_pcode);
			mButtons[t]->setToggleState( state );
			mButtons[t]->setVisible( create_visible );
		}
	}

	if (mCheckSticky) mCheckSticky		->setVisible( create_visible );
	if (mCheckCopySelection) mCheckCopySelection	->setVisible( create_visible );
	if (mCheckCopyCenters) mCheckCopyCenters	->setVisible( create_visible );
	if (mCheckCopyRotates) mCheckCopyRotates	->setVisible( create_visible );

	if (mCheckCopyCenters && mCheckCopySelection) mCheckCopyCenters->setEnabled( mCheckCopySelection->get() );
	if (mCheckCopyRotates && mCheckCopySelection) mCheckCopyRotates->setEnabled( mCheckCopySelection->get() );

	// Land buttons
	bool land_visible = (tool == LLToolBrushLand::getInstance() || tool == LLToolSelectLand::getInstance() );

	mCostTextBorder->setVisible(!land_visible);

	if (mBtnLand)	mBtnLand	->setToggleState( land_visible );

	mRadioGroupLand->setVisible( land_visible );
	if ( tool == LLToolSelectLand::getInstance() )
	{
		mRadioGroupLand->setValue("radio select land");
	}
	else if ( tool == LLToolBrushLand::getInstance() )
	{
		S32 dozer_mode = gSavedSettings.getS32("RadioLandBrushAction");
		switch(dozer_mode)
		{
		case 0:
			mRadioGroupLand->setValue("radio flatten");
			break;
		case 1:
			mRadioGroupLand->setValue("radio raise");
			break;
		case 2:
			mRadioGroupLand->setValue("radio lower");
			break;
		case 3:
			mRadioGroupLand->setValue("radio smooth");
			break;
		case 4:
			mRadioGroupLand->setValue("radio noise");
			break;
		case 5:
			mRadioGroupLand->setValue("radio revert");
			break;
		default:
			break;
		}
	}

	if (mBtnApplyToSelection)
	{
		mBtnApplyToSelection->setVisible( land_visible );
		mBtnApplyToSelection->setEnabled( land_visible && !LLViewerParcelMgr::getInstance()->selectionEmpty() && tool != LLToolSelectLand::getInstance());
	}
	if (mSliderDozerSize)
	{
		mSliderDozerSize	->setVisible( land_visible );
		getChildView("Bulldozer:")->setVisible( land_visible);
		getChildView("Dozer Size:")->setVisible( land_visible);
	}
	if (mSliderDozerForce)
	{
		mSliderDozerForce	->setVisible( land_visible );
		getChildView("Strength:")->setVisible( land_visible);
	}

	bool have_selection = !LLSelectMgr::getInstance()->getSelection()->isEmpty();

	getChildView("selection_count")->setVisible(!land_visible && have_selection);
	getChildView("remaining_capacity")->setVisible(!land_visible && have_selection);
    getChildView("selection_faces")->setVisible(LLToolFace::getInstance() == LLToolMgr::getInstance()->getCurrentTool()
                                                && LLSelectMgr::getInstance()->getSelection()->getObjectCount() == 1);
	getChildView("selection_empty")->setVisible(!land_visible && !have_selection);
	
	mTab->setVisible(!land_visible);
	mPanelLandInfo->setVisible(land_visible);
}


// virtual
bool LLFloaterTools::canClose()
{
	// don't close when quitting, so camera will stay put
	return !LLApp::isExiting();
}

// virtual
void LLFloaterTools::onOpen(const LLSD& key)
{
	mParcelSelection = LLViewerParcelMgr::getInstance()->getFloatingParcelSelection();
	mObjectSelection = LLSelectMgr::getInstance()->getEditSelection();
	
	std::string panel = key.asString();
	if (!panel.empty())
	{
		mTab->selectTabByName(panel);
	}

	LLTool* tool = LLToolMgr::getInstance()->getCurrentTool();
	if (tool == LLToolCompInspect::getInstance()
		|| tool == LLToolDragAndDrop::getInstance())
	{
		// Something called floater up while it was supressed (during drag n drop, inspect),
		// so it won't be getting any layout or visibility updates, update once
		// further updates will come from updateLayout()
		LLCoordGL select_center_screen;
		MASK	mask = gKeyboard->currentMask(true);
		updatePopup(select_center_screen, mask);
	}
	
	//gMenuBarView->setItemVisible("BuildTools", true);
}

// virtual
void LLFloaterTools::onClose(bool app_quitting)
{
	mTab->setVisible(false);

	LLViewerJoystick::getInstance()->moveAvatar(false);

	// destroy media source used to grab media title
	mPanelFace->unloadMedia();

    // Different from handle_reset_view in that it doesn't actually 
	//   move the camera if EditCameraMovement is not set.
	gAgentCamera.resetView(gSavedSettings.getbool("EditCameraMovement"));
	
	// exit component selection mode
	LLSelectMgr::getInstance()->promoteSelectionToRoot();
	gSavedSettings.setbool("EditLinkedParts", false);

	gViewerWindow->showCursor();

	resetToolState();

	mParcelSelection = NULL;
	mObjectSelection = NULL;

	// Switch back to basic toolset
	LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);
	// we were already in basic toolset, using build tools
	// so manually reset tool to default (pie menu tool)
	LLToolMgr::getInstance()->getCurrentToolset()->selectFirstTool();

	//gMenuBarView->setItemVisible("BuildTools", false);
	LLFloaterReg::hideInstance("media_settings");

	// hide the advanced object weights floater
	LLFloaterReg::hideInstance("object_weights");

	// prepare content for next call
	mPanelContents->clearContents();

	if(sPreviousFocusOnAvatar)
	{
		sPreviousFocusOnAvatar = false;
		gAgentCamera.setAllowChangeToFollow(true);
	}
}

void click_popup_info(void*)
{
}

void click_popup_done(void*)
{
	handle_reset_view();
}

void commit_radio_group_move(LLUICtrl* ctrl)
{
	LLRadioGroup* group = (LLRadioGroup*)ctrl;
	std::string selected = group->getValue().asString();
	if (selected == "radio move")
	{
		gGrabBtnVertical = false;
		gGrabBtnSpin = false;
	}
	else if (selected == "radio lift")
	{
		gGrabBtnVertical = true;
		gGrabBtnSpin = false;
	}
	else if (selected == "radio spin")
	{
		gGrabBtnVertical = false;
		gGrabBtnSpin = true;
	}
}

void commit_radio_group_focus(LLUICtrl* ctrl)
{
	LLRadioGroup* group = (LLRadioGroup*)ctrl;
	std::string selected = group->getValue().asString();
	if (selected == "radio zoom")
	{
		gCameraBtnZoom = true;
		gCameraBtnOrbit = false;
		gCameraBtnPan = false;
	}
	else if (selected == "radio orbit")
	{
		gCameraBtnZoom = false;
		gCameraBtnOrbit = true;
		gCameraBtnPan = false;
	}
	else if (selected == "radio pan")
	{
		gCameraBtnZoom = false;
		gCameraBtnOrbit = false;
		gCameraBtnPan = true;
	}
}

void commit_slider_zoom(LLUICtrl *ctrl)
{
	// renormalize value, since max "volume" level is 0.5 for some reason
	F32 zoom_level = (F32)ctrl->getValue().asReal() * 2.f; // / 0.5f;
	gAgentCamera.setCameraZoomFraction(zoom_level);
}

void commit_slider_dozer_force(LLUICtrl *ctrl)
{
	// the slider is logarithmic, so we exponentiate to get the actual force multiplier
	F32 dozer_force = pow(10.f, (F32)ctrl->getValue().asReal());
	gSavedSettings.setF32("LandBrushForce", dozer_force);
}

void click_apply_to_selection(void*)
{
	LLToolBrushLand::getInstance()->modifyLandInSelectionGlobal();
}

void commit_radio_group_edit(LLUICtrl *ctrl)
{
	bool show_owners = gSavedSettings.getbool("ShowParcelOwners");

	LLRadioGroup* group = (LLRadioGroup*)ctrl;
	std::string selected = group->getValue().asString();
	if (selected == "radio position")
	{
		LLFloaterTools::setEditTool( LLToolCompTranslate::getInstance() );
	}
	else if (selected == "radio rotate")
	{
		LLFloaterTools::setEditTool( LLToolCompRotate::getInstance() );
	}
	else if (selected == "radio stretch")
	{
		LLFloaterTools::setEditTool( LLToolCompScale::getInstance() );
	}
	else if (selected == "radio select face")
	{
		LLFloaterTools::setEditTool( LLToolFace::getInstance() );
	}
		else if (selected == "radio align")
	{
		LLFloaterTools::setEditTool( QToolAlign::getInstance() );
	}

	gSavedSettings.setbool("ShowParcelOwners", show_owners);
}

void commit_radio_group_land(LLUICtrl* ctrl)
{
	LLRadioGroup* group = (LLRadioGroup*)ctrl;
	std::string selected = group->getValue().asString();
	if (selected == "radio select land")
	{
		LLFloaterTools::setEditTool( LLToolSelectLand::getInstance() );
	}
	else
	{
		LLFloaterTools::setEditTool( LLToolBrushLand::getInstance() );
		S32 dozer_mode = gSavedSettings.getS32("RadioLandBrushAction");
		if (selected == "radio flatten")
			dozer_mode = 0;
		else if (selected == "radio raise")
			dozer_mode = 1;
		else if (selected == "radio lower")
			dozer_mode = 2;
		else if (selected == "radio smooth")
			dozer_mode = 3;
		else if (selected == "radio noise")
			dozer_mode = 4;
		else if (selected == "radio revert")
			dozer_mode = 5;
		gSavedSettings.setS32("RadioLandBrushAction", dozer_mode);
	}
}

void commit_select_component(void *data)
{
	LLFloaterTools* floaterp = (LLFloaterTools*)data;

	//forfeit focus
	if (gFocusMgr.childHasKeyboardFocus(floaterp))
	{
		gFocusMgr.setKeyboardFocus(NULL);
	}

	bool select_individuals = floaterp->mCheckSelectIndividual->get();
	gSavedSettings.setbool("EditLinkedParts", select_individuals);
	floaterp->dirty();

	if (select_individuals)
	{
		LLSelectMgr::getInstance()->demoteSelectionToIndividuals();
	}
	else
	{
		LLSelectMgr::getInstance()->promoteSelectionToRoot();
	}
}

// static 
void LLFloaterTools::setObjectType( LLPCode pcode )
{
	LLToolPlacer::setObjectType( pcode );
	gSavedSettings.setbool("CreateToolCopySelection", false);
	gFocusMgr.setMouseCapture(NULL);
}

void commit_grid_mode(LLUICtrl *ctrl)
{
	LLComboBox* combo = (LLComboBox*)ctrl;

	LLSelectMgr::getInstance()->setGridMode((EGridMode)combo->getCurrentIndex());
}

// static
void LLFloaterTools::setGridMode(S32 mode)
{
	LLFloaterTools* tools_floater = LLFloaterReg::getTypedInstance<LLFloaterTools>("build");
	if (!tools_floater || !tools_floater->mComboGridMode)
	{
		return;
	}

	tools_floater->mComboGridMode->setCurrentByIndex(mode);
}

void LLFloaterTools::onClickGridOptions()
{
//	LLFloaterReg::showInstance("advancedbuild_options"); //Includes build_options and advanced build options
	//LLFloaterReg::showInstance("build_options");
	LLFloater* floaterp = LLFloaterReg::showInstance("advancedbuild_options");
	// position floater next to build tools, not over
	floaterp->setShape(gFloaterView->findNeighboringPosition(this, floaterp), true);
}

// static
void LLFloaterTools::setEditTool(void* tool_pointer)
{
	LLTool *tool = (LLTool *)tool_pointer;
	LLToolMgr::getInstance()->getCurrentToolset()->selectTool( tool );
}

void LLFloaterTools::setTool(const LLSD& user_data)
{
	std::string control_name = user_data.asString();
	if(control_name == "Focus")
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool((LLTool *) LLToolCamera::getInstance() );
	else if (control_name == "Move" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *)LLToolGrab::getInstance() );
	else if (control_name == "Edit" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *) LLToolCompTranslate::getInstance());
	else if (control_name == "Create" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *) LLToolCompCreate::getInstance());
	else if (control_name == "Land" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *) LLToolSelectLand::getInstance());
	else
		LL_WARNS()<<" no parameter name "<<control_name<<" found!! No Tool selected!!"<< LL_ENDL;
}

void LLFloaterTools::onFocusReceived()
{
	LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);
	LLFloater::onFocusReceived();
}

void LLFloaterTools::updateLandImpacts()
{
	LLParcel *parcel = mParcelSelection->getParcel();
	if (!parcel)
	{
		return;
	}

	S32 rezzed_prims = parcel->getSimWidePrimCount();
	S32 total_capacity = parcel->getSimWideMaxPrimCapacity();
	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if (region)
	{
		S32 max_tasks_per_region = (S32)region->getMaxTasks();
		total_capacity = llmin(total_capacity, max_tasks_per_region);
	}
	std::string remaining_capacity_str = "";

	bool show_mesh_cost = gMeshRepo.meshRezEnabled();
	if (show_mesh_cost)
	{
		LLStringUtil::format_map_t remaining_capacity_args;
		remaining_capacity_args["LAND_CAPACITY"] = llformat("%d", total_capacity - rezzed_prims);
		remaining_capacity_str = getString("status_remaining_capacity", remaining_capacity_args);
	}

	childSetTextArg("remaining_capacity", "[CAPACITY_STRING]", remaining_capacity_str);

	// Update land impacts info in the weights floater
	LLFloaterObjectWeights* object_weights_floater = LLFloaterReg::findTypedInstance<LLFloaterObjectWeights>("object_weights");
	if(object_weights_floater)
	{
		object_weights_floater->updateLandImpacts(parcel);
	}
}

void LLFloaterTools::onClickBtnGiveMenu()
{
	const LLRect& currentRectFloater = gFloaterTools->getRect();	
	const LLRect& currentRectButton = mBtnGiveMenu->getRect();
	S32 x = currentRectFloater.mLeft + currentRectButton.mLeft;
	S32 y = currentRectFloater.mBottom + currentRectButton.mBottom;

	// this code is adapated from lltoolpie
	LLViewerObject *object = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
	if (object)
	{
		bool is_other_attachment = (object->isAttachment() && !object->isHUDAttachment() && !object->permYouOwner());
		if (object->isAvatar() || is_other_attachment)
		{
			// Find the attachment's avatar
			while( object && object->isAttachment())
			{
				object = (LLViewerObject*)object->getParent();
				llassert(object);
			}

			if (!object)
			{
				return; // unexpected, but escape
			}

			// Object is an avatar, so check for mute by id.
			LLVOAvatar* avatar = (LLVOAvatar*)object;
			std::string name = avatar->getFullname();
			std::string mute_msg;
			if (LLMuteList::getInstance()->isMuted(avatar->getID(), avatar->getFullname()))
			{
				mute_msg = LLTrans::getString("UnmuteAvatar");
			}
			else
			{
				mute_msg = LLTrans::getString("MuteAvatar");
			}

			if (is_other_attachment)
			{
				gMenuAttachmentOther->getChild<LLUICtrl>("Avatar Mute")->setValue(mute_msg);
				gMenuAttachmentOther->show(x, y);
			}
			else
			{
				gMenuAvatarOther->getChild<LLUICtrl>("Avatar Mute")->setValue(mute_msg);
				gMenuAvatarOther->show(x, y);
			}
		}
		else if (object->isAttachment())
		{
			gMenuAttachmentSelf->show(x, y);
		}
		else
		{
			// BUG: What about chatting child objects?
			std::string name;
			LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstRootNode();
			if (node)
			{
				name = node->mName;
			}

			gMenuObject->show(x, y);

			LLToolPie::getInstance()->showVisualContextMenuEffect();
		}
	}	
}

struct LLFloaterToolsCopyKeysFunctor : public LLSelectedObjectFunctor
{
	LLFloaterToolsCopyKeysFunctor(std::string& strOut, std::string& strSep) : mOutput(strOut), mSep(strSep) {}
	virtual bool apply(LLViewerObject* object)
	{
		if (!mOutput.empty())
			mOutput.append(mSep);

		mOutput.append(object->getID().asString());

		return true;
	}
private:
	std::string& mOutput;
	std::string& mSep;
};

void LLFloaterTools::onClickBtnCopyKeys()
{
	std::string separator = gSavedSettings.getString("FSCopyObjKeySeparator");
	std::string stringKeys;
	LLFloaterToolsCopyKeysFunctor copy_keys(stringKeys, separator);
	bool copied = LLSelectMgr::getInstance()->getSelection()->applyToObjects(&copy_keys);
	if (copied)
	{
		LLView::getWindow()->copyTextToClipboard(utf8str_to_wstring(stringKeys));
	}
}


