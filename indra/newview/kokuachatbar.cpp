/** 
 * @file kokuachatbar.c
 * @author Chorazin Allen <chorazinallen@gmail.com>
 * @brief Chat bar floater without IMs or history
 *
 * Abstracted from llfloaterregiondebugconsole / llfloaterimnearbychat
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

#include "kokuachatbar.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llautoreplace.h"
#include "llchatentry.h"
#include "llfloaterimnearbychat.h"
#include "llfloaterreg.h"
#include "lltrans.h"
#include "llviewerchat.h"

KokuaChatBar::KokuaChatBar(LLSD const & key)
: LLFloater(key),
	mInputEditorHeightPad(0),
	mInputEditorWidthPad(0),
	mInputEditor(nullptr)
{

}

KokuaChatBar::~KokuaChatBar()
{
	releaseFocus();
}

bool KokuaChatBar::postBuild()
{
	mInputEditor = getChild<LLChatEntry>("kc_chat_editor");
	mInputEditor->setCommitOnFocusLost(false);
	mInputEditor->setPassDelete(true);
	mInputEditor->setFont(LLViewerChat::getChatFont());
	mInputEditor->setAutoreplaceCallback(boost::bind(&LLAutoReplace::autoreplaceCallback, LLAutoReplace::getInstance(), _1, _2, _3, _4, _5));
	mInputEditor->setCommitCallback(boost::bind(&KokuaChatBar::onChatBoxCommit, this));
	mInputEditor->setKeystrokeCallback(boost::bind(&KokuaChatBar::onChatBoxKeystroke, this));
	mInputEditor->setFocusLostCallback(boost::bind(&KokuaChatBar::onChatBoxFocusLost, this));
	mInputEditor->setFocusReceivedCallback(boost::bind(&KokuaChatBar::onChatBoxFocusReceived, this));
	mInputEditor->setLabel(LLTrans::getString("NearbyChatTitle"));
	mInputEditor->setTextExpandedCallback(boost::bind(&KokuaChatBar::reshapeChatLayoutPanel, this));
	mInputEditorHeightPad = getRect().getHeight() - mInputEditor->getRect().getHeight();
	mInputEditorWidthPad = getRect().getWidth() - mInputEditor->getRect().getWidth();
	enableResizeCtrls(true, true, false); // don't allow height change since we manage it dynamically based on input size

	setTitle(LLTrans::getString("KokuaChatBar"));

	return true;
}

void KokuaChatBar::handleReshape(const LLRect& new_rect, bool by_user)
{
	// The floater is being resized. This keeps the input area to the same size. Note that oversize input areas won't persist
	// and will get right-sized when input causes a linewrap. We avoid this effect by forcing resizing to horizontal only.

	LLRect floater_rect;
	LLFloater::handleReshape(new_rect, by_user);
	floater_rect = getRect();	
	if (mInputEditor) mInputEditor->reshape(floater_rect.getWidth() - mInputEditorWidthPad,floater_rect.getHeight() - mInputEditorHeightPad, true);
	storeRectControl();
}


void KokuaChatBar::reshapeChatLayoutPanel()
{
	// Resizes floater to match chat input area. Called when the chat area grows a line (up to maximum
	// defined in floater_chatbar.xml) and when chat is committed and the chat input area shrinks back
	// to a single line. If the floater is within the main area of the screen it will expand both top
	// and bottom and retain the centre point's y position. However, if near top or bottom the system
	// takes care of preventing ingress outside of visible areas or into the toolbox zone
	reshape(getRect().getWidth(), mInputEditor->getRect().getHeight() + mInputEditorHeightPad, false);
	if (mInputEditor->getText().size() == 0)
	{
		storeRectControl();
	}
}

void KokuaChatBar::onChatBoxCommit()
{
	sendChat(CHAT_TYPE_NORMAL);
}

void KokuaChatBar::onChatFontChange(LLFontGL* fontp)
{
	if (mInputEditor)
	{
		mInputEditor->setFont(fontp);
	}
}

void KokuaChatBar::onOpen(const LLSD& key)
{
	mInputEditor->setFocus(true);
}
	
void KokuaChatBar::onChatBoxKeystroke()
{
	// We use FloaterIMNearbyChat to handle this to avoid code duplication
	LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::findTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
	if (nearby_chat) nearby_chat->onChatBoxKeystrokeWithText(mInputEditor);
}

void KokuaChatBar::sendChat( EChatType type )
{
	// We use FloaterIMNearbyChat to handle this to avoid code duplication
	if (mInputEditor)
	{
		LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::findTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
		if (nearby_chat) nearby_chat->sendChatWithText(type,mInputEditor->getWText());

		mInputEditor->setText(LLStringExplicit(""));

		gAgent.stopTyping();

		if (gSavedSettings.getbool("KokuaCloseChatBarOnReturn"))
		{
			mInputEditor->setFocus(false);
			closeFloater();
		}
		else if (gSavedSettings.getbool("CloseChatOnReturn"))
		{
			// If the user wants to stop chatting on hitting return, lose focus
			// and go out of chat mode.
			mInputEditor->setFocus(false);
		}
	}
}

// static
void KokuaChatBar::onChatBoxFocusLost()
{
	// stop typing animation
	gAgent.stopTyping();
}

void KokuaChatBar::onChatBoxFocusReceived()
{
	mInputEditor->setEnabled(!gDisconnected);
}

// virtual
bool KokuaChatBar::handleKeyHere( KEY key, MASK mask )
{
	bool handled = false;

	if( KEY_RETURN == key && mask == MASK_CONTROL)
	{
		// shout
		sendChat(CHAT_TYPE_SHOUT);
		handled = true;
	}
	else if (KEY_RETURN == key && mask == MASK_SHIFT)
	{
		// whisper
		sendChat(CHAT_TYPE_WHISPER);
		handled = true;
	}

	return handled;
}

// static 
void KokuaChatBar::startChat(const char* line)
{
	KokuaChatBar* nearby_chat = LLFloaterReg::getTypedInstance<KokuaChatBar>("kokua_chatbar");
	if (nearby_chat)
	{
		nearby_chat->show();
		nearby_chat->mInputEditor->setEnabled(!gDisconnected);

		if (line)
		{
			std::string line_string(line);
			nearby_chat->mInputEditor->setText(line_string);
		}

		nearby_chat->mInputEditor->endOfDoc();
		nearby_chat->mInputEditor->setFocus(true);
	}
}

void KokuaChatBar::show()
{
		openFloater(getKey());
}