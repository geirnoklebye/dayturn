/** 
 * @file llavataractions.h
 * @brief Friend-related actions (add, remove, offer teleport, etc)
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

#ifndef LL_LLAVATARACTIONS_H
#define LL_LLAVATARACTIONS_H

#include "llsd.h"
#include "lluuid.h"

#include <string>
#include <vector>

class LLAvatarName;
class LLInventoryPanel;
class LLFloater;
class LLView;
class LLViewerRegion;

/**
 * Friend-related actions (add, remove, offer teleport, etc)
 */
class LLAvatarActions
{
public:
	/**
	 * Show a dialog explaining what friendship entails, then request friendship.
	 */
	static void requestFriendshipDialog(const LLUUID& id, const std::string& name);

	/**
	 * Show a dialog explaining what friendship entails, then request friendship.
	 */
	static void requestFriendshipDialog(const LLUUID& id);

	/**
	 * Show a friend removal dialog.
	 */
	static void removeFriendDialog(const LLUUID& id);
	static void removeFriendsDialog(const uuid_vec_t& ids);
	
	/**
	 * Show teleport offer dialog.
	 */
	static void offerTeleport(const LLUUID& invitee);
	static void offerTeleport(const uuid_vec_t& ids);

	/**
	 * Start instant messaging session.
	 */
	static void startIM(const LLUUID& id);

	/**
	 * End instant messaging session.
	 */
	static void endIM(const LLUUID& id);

	/**
	 * Start an avatar-to-avatar voice call with another user
	 */
	static void startCall(const LLUUID& id);

	/**
	 * Start an ad-hoc conference voice call with multiple users in a specific IM floater.
	 */
	static void startAdhocCall(const uuid_vec_t& ids, const LLUUID& floater_id = LLUUID::null);

	/**
	 * Start conference chat with the given avatars in a specific IM floater.
	 */
	static void startConference(const uuid_vec_t& ids, const LLUUID& floater_id = LLUUID::null);

	/**
	 * Show avatar profile.
	 */
	static void showProfile(const LLUUID& id);
	static void hideProfile(const LLUUID& id);
	static bool profileVisible(const LLUUID& id);
	static LLFloater* getProfileFloater(const LLUUID& id);
//<FS:KC legacy profiles>
    static void showProfileLegacy(const LLUUID& id);
//</FS:KC legacy profiles>

	/**
	 * Show avatar on world map.
	 */
	static void showOnMap(const LLUUID& id);

	/**
	 * Give money to the avatar.
	 */
	static void pay(const LLUUID& id);

	/**
	 * Request teleport from other avatar
	 */
	static void teleportRequest(const LLUUID& id);
	static void teleport_request_callback(const LLSD& notification, const LLSD& response);

	/**
	 * Share items with the avatar.
	 */
	static void share(const LLUUID& id);

	/**
	 * Copy complete avatar name to the clipboard.
	 */
	static void copyName(const LLUUID &id);

	/**
	 * Copy avatar UUID to the clipboard.
	 */
	static void copyUUID(const LLUUID &id);

	/**
	 * Toggle avatar rights (see online status, see on map, edit objects)
	 */
	static void toggleAvatarRights(const LLUUID &id, S32 rights);

	/**
	 * Copy avatar profile URI to the clipboard.
	 */
	static void copyProfileSLURL(const LLUUID &id);

	/**
	 * Share items with the picked avatars.
	 */
	static void shareWithAvatars(LLView * panel);

	/**
	 * Block/unblock the avatar by id.
	 * Returns true if blocked, returns false if unblocked
	 */
	static bool toggleBlock(const LLUUID& id);

	/**
	 * Mute/unmute avatar.
	 */
	static void toggleMute(const LLUUID& id, U32 flags);

	/**
	 * Block/unblock the avatar voice.
	 */
	static void toggleMuteVoice(const LLUUID& id);

	/**
	 * Teleport the avatar home.
	 */
	static void teleportHome(const LLUUID &id);
	static void teleportHomeCommit(const LLSD &notification, const LLSD &response);

	/**
	 * Estate ban the avatar.
	 */
	static void estateBan(const LLUUID &id);
	static void estateBanCommit(const LLSD &notification, const LLSD &response);

	/**
	 * @return true if the avatar is on land you own or manage
	 */
	static bool isOnYourLand(const LLUUID &id);

	/**
	 * Return true if avatar with "id" is a friend
	 */
	static bool isFriend(const LLUUID& id);

	/**
	 * @return true if the avatar is blocked
	 */
	static bool isBlocked(const LLUUID& id);

	/**
	 * @return true if the avatar voice is blocked
	 */
	static bool isVoiceMuted(const LLUUID& id);

	/**
	 * @return true if you can block the avatar
	 */
	static bool canBlock(const LLUUID& id);

	/**
	 * Return true if the avatar is in a P2P voice call with a given user
	 */
	/* AD *TODO: Is this function needed any more?
		I fixed it a bit(added check for canCall), but it appears that it is not used
		anywhere. Maybe it should be removed?
	static bool isCalling(const LLUUID &id);*/

	/**
	 * @return true if call to the resident can be made
	 */

	static bool canCall();
	/**
	 * Invite avatar to a group.
	 */	
	static void inviteToGroup(const LLUUID& id);
	
	static void freezeAvatar(const LLUUID& id);

	static void ejectAvatar(const LLUUID& id, bool ban_enabled = false);
	/**
	 * Kick avatar off grid
	 */	
	static void kick(const LLUUID& id);

	/**
	 * Freeze avatar
	 */	
	static void freeze(const LLUUID& id);

	/**
	 * Unfreeze avatar
	 */	
	static void unfreeze(const LLUUID& id);

	/**
	 * Open csr page for avatar
	 */	
	static void csr(const LLUUID& id, std::string name);
	
	/**
	 * [FS:CR] Add avatar to contact set
	 */
	static void addToContactSet(const LLUUID& agent_id);
	static void addToContactSet(const uuid_vec_t& agent_ids);

	/**
	 * Checks whether we can offer a teleport to the avatar, only offline friends
	 * cannot be offered a teleport.
	 *
	 * @return false if avatar is a friend and not visibly online
	 */
	static bool canOfferTeleport(const LLUUID& id);

	/**
	 * @return false if any one of the specified avatars a friend and not visibly online
	 */
	static bool canOfferTeleport(const uuid_vec_t& ids);

	// <FS:Ansariel> Extra request teleport
	static bool canRequestTeleport(const LLUUID& id);

	/**
	 * Checks whether all items selected in the given inventory panel can be shared
	 *
	 * @param inv_panel Inventory panel to get selection from. If NULL, the active inventory panel is used.
	 *
	 * @return false if the selected items cannot be shared or the active inventory panel cannot be obtained
	 */
	static bool canShareSelectedItems(LLInventoryPanel* inv_panel = nullptr);

	/**
	 * Builds a string of residents' display names separated by "words_separator" string.
	 *
	 * @param avatar_names - a vector of given avatar names from which resulting string is built
	 * @param residents_string - the resulting string
	 */
	static void buildResidentsString(std::vector<LLAvatarName> avatar_names, std::string& residents_string, bool complete_name = false);

	/**
	 * Builds a string of residents' display names separated by "words_separator" string.
	 *
	 * @param avatar_uuids - a vector of given avatar uuids from which resulting string is built
	 * @param residents_string - the resulting string
	 */
	static void buildResidentsString(const uuid_vec_t& avatar_uuids, std::string& residents_string);

	/**
	 * Opens the chat history for avatar
	 */
	static void viewChatHistory(const LLUUID& id);
	static void report(const LLUUID& idAgent);

	static std::set<LLUUID> getInventorySelectedUUIDs(LLInventoryPanel* active_panel = NULL);

private:
	static bool callbackAddFriendWithMessage(const LLSD& notification, const LLSD& response);
	static bool handleRemove(const LLSD& notification, const LLSD& response);
	static bool handlePay(const LLSD& notification, const LLSD& response, LLUUID avatar_id);
	static bool handleFreezeAvatar(const LLSD& notification, const LLSD& response);
	static bool handleEjectAvatar(const LLSD& notification, const LLSD& response);
	static bool handleKick(const LLSD& notification, const LLSD& response);
	static bool handleFreeze(const LLSD& notification, const LLSD& response);
	static bool handleUnfreeze(const LLSD& notification, const LLSD& response);
	static void callback_invite_to_group(LLUUID group_id, LLUUID id);

	static void confirmModifyRights(const LLUUID &id, const bool grant, const S32 rights);
	static void rightsConfirmationCallback(const LLSD &notification, const LLSD &response, const LLUUID &id, const S32 rights);

	// Just request friendship, no dialog.
	static void requestFriendship(const LLUUID& target_id, const std::string& target_name, const std::string& message);
};

#endif // LL_LLAVATARACTIONS_H
