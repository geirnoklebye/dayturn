/*
 * @file fsexportperms.cpp
 * @brief Export permissions check
 * @authors Cinder Biscuits
 *
 * $LicenseInfo:firstyear=2013&license=LGPLV2.1$
 * Copyright (C) 2013 Cinder Biscuits <cinder.roxley@phoenixviewer.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write a love letter 
 * to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#include "llviewerprecompiledheaders.h"

#include "fsexportperms.h"
#include "llagent.h"
#include "llinventoryfunctions.h"
#include "llmeshrepository.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"
#include "llvovolume.h"
#include "llworld.h"
#include "xform.h"

#define FOLLOW_PERMS 1

extern bool gIsInSecondLife; //Opensim or SecondLife

bool FSExportPermsCheck::canExportNode(LLSelectNode* node, bool dae)
{
	if (!node)
	{
		LL_WARNS("export") << "No node, bailing!" << LL_ENDL;
		return false;
	}
	bool exportable = false;
	
	LLViewerObject* object = node->getObject();
    if (gIsInSecondLife)
    {
        LLUUID creator(node->mPermissions->getCreator());
        exportable = (object->permYouOwner() && gAgentID == creator);
        if (!exportable)
        {
            // Megaprim check
            F32 max_object_size = LLWorld::getInstance()->getRegionMaxPrimScale();
            LLVector3 vec = object->getScale();
            if (vec.mV[VX] > max_object_size || vec.mV[VY] > max_object_size || vec.mV[VZ] > max_object_size)
            {
                exportable = (creator == LLUUID("7ffd02d0-12f4-48b4-9640-695708fd4ae4") // Zwagoth Klaar
                              || creator == gAgentID);
            }
        }
    }
    else if (!gIsInSecondLife)//we are in Opensim
    {
        if(gAgent.getRegion() -> getRegionAllowsExport())
        {
            LL_INFOS("export") << "region allows export" << LL_ENDL;
            exportable = (object->permYouOwner() && gAgentID == node->mPermissions->getCreator());
        }
        else
        {
            LL_INFOS("export") << "region does not allow export" << LL_ENDL;
            exportable = (object->permYouOwner() && object->permModify() && object->permCopy() && object->permTransfer());
        }
    }

	// We've got perms on the object itself, let's check for sculptmaps and meshes!
	if (exportable)
	{
		exportable = false;
		LLVOVolume* volobjp = nullptr;

		if (object->getPCode() == LL_PCODE_VOLUME)
		{
			volobjp = (LLVOVolume *)object;
		}

		if (volobjp && volobjp->isSculpted())
		{
			const LLSculptParams* sculpt_params = (const LLSculptParams *)object->getParameterEntry(LLNetworkData::PARAMS_SCULPT);
            if (gIsInSecondLife)
            {
                if (volobjp->isMesh())
                {
                    if (dae)
                    {
                        exportable = gMeshRepo.getCreatorFromHeader(sculpt_params->getSculptTexture()) == gAgentID;
                    }
                    else
                    {
                        // can not export mesh to oxp
                        LL_INFOS("export") << "Mesh can not be exported to oxp." << LL_ENDL;
                        return false;
                    }
                }
                else if (sculpt_params)
                {
                    LLViewerFetchedTexture* imagep = LLViewerTextureManager::getFetchedTexture(sculpt_params->getSculptTexture());
                    if (imagep->mComment.find("a") != imagep->mComment.end())
                    {
                        exportable = (LLUUID(imagep->mComment["a"]) == gAgentID);
                    }
    
                    if (!exportable)
                    {
                        LLUUID asset_id = sculpt_params->getSculptTexture();
                        LLViewerInventoryCategory::cat_array_t cats;
                        LLViewerInventoryItem::item_array_t items;
                        LLAssetIDMatches asset_id_matches(asset_id);
                        gInventory.collectDescendentsIf(LLUUID::null, cats, items,
                                                        LLInventoryModel::INCLUDE_TRASH,
                                                        asset_id_matches);
                            
                        for (S32 i = 0; i < items.size() && !exportable; ++i)
                        {
                            const LLPermissions perms = items[i]->getPermissions();
                            exportable = perms.getCreator() == gAgentID;
                        }
                    }
    
                    if (!exportable)
                    {
                        LL_INFOS("export") << "Sculpt map has failed permissions check." << LL_ENDL;
                    }
                }
            }
            else if (!gIsInSecondLife)  // we are in OpenSim
            {
                if (sculpt_params && !volobjp->isMesh())
                {
                    LLUUID asset_id = sculpt_params->getSculptTexture();
                    LLViewerInventoryCategory::cat_array_t cats;
                    LLViewerInventoryItem::item_array_t items;
                    LLAssetIDMatches asset_id_matches(asset_id);
                    gInventory.collectDescendentsIf(LLUUID::null, cats, items,
                                                    LLInventoryModel::INCLUDE_TRASH,
                                                    asset_id_matches);
                    
                    for (S32 i = 0; i < items.size() && !exportable; ++i)
                    {
                        const LLPermissions perms = items[i]->getPermissions();
                        exportable = (perms.getMaskOwner() & PERM_EXPORT) == PERM_EXPORT;
                    }
                    
                    if (!exportable)
                    {
                        LL_INFOS("export") << "Sculpt map has failed permissions check." << LL_ENDL;
                    }
                }
            }
            else
            {
                // No sculpt or mesh
                exportable = true;
            }
        }
    }
    return exportable;
}
#if !FOLLOW_PERMS
#error "You didn't think it would be that easy, did you? :P"
#endif

bool FSExportPermsCheck::canExportAsset(LLUUID asset_id, std::string* name, std::string* description)
{
	bool exportable = false;
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLAssetIDMatches asset_id_matches(asset_id);
	gInventory.collectDescendentsIf(LLUUID::null,
									cats,
									items,
									LLInventoryModel::INCLUDE_TRASH,
									asset_id_matches);
	
	if (items.size())
	{
		// use the name of the first match
		(*name) = items[0]->getName();
		(*description) = items[0]->getDescription();

        for (S32 i = 0; i < items.size() && !exportable; ++i)
		{
            LLPermissions perms = items[i]->getPermissions();
			if (!gIsInSecondLife)
			{
                exportable = true;
            }
			else
            {
                exportable = (perms.getMaskBase() & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED;
            }
		}
	}
	
	return exportable;
}

