/** 
 * @file llfilepicker.cpp
 * @brief OS-specific file picker
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

#include "llfilepicker.h"
#include "llworld.h"
#include "llviewerwindow.h"
#include "llkeyboard.h"
#include "lldir.h"
#include "llframetimer.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llwindow.h"	// beforeDialog()

#if LL_SDL
#include "llwindowsdl.h" // for some X/GTK utils to help with filepickers
#endif // LL_SDL

//
// Globals
//

LLFilePicker LLFilePicker::sInstance;

#if LL_WINDOWS
#define SOUND_FILTER L"Sounds (*.wav)\0*.wav\0"
#define IMAGE_FILTER L"Images (*.tga; *.bmp; *.jpg; *.jpeg; *.png)\0*.tga;*.bmp;*.jpg;*.jpeg;*.png\0"
#define ANIM_FILTER L"Animations (*.bvh; *.anim)\0*.bvh;*.anim\0"
#define COLLADA_FILTER L"Scene (*.dae)\0*.dae\0"
#define XML_FILTER L"XML files (*.xml)\0*.xml\0"
#define SLOBJECT_FILTER L"Objects (*.slobject)\0*.slobject\0"
#define RAW_FILTER L"RAW files (*.raw)\0*.raw\0"
#define MODEL_FILTER L"Model files (*.dae)\0*.dae\0"
#define SCRIPT_FILTER L"Script files (*.lsl)\0*.lsl\0"
#define DICTIONARY_FILTER L"Dictionary files (*.dic; *.xcu)\0*.dic;*.xcu\0"
// <FS:CR> Import filter
//#define IMPORT_FILTER L"Import (*.oxp; *.hpa)\0*.oxp;*.hpa\0"
#define IMPORT_FILTER L"Import (*.oxp)\0*.oxp\0"
// </FS:CR>
#endif

#ifdef LL_DARWIN
#include "llfilepicker_mac.h"
//#include <boost/algorithm/string/predicate.hpp>
#endif

//
// Implementation
//
LLFilePicker::LLFilePicker()
	: mCurrentFile(0),
	  mLocked(false)

{
	reset();

#if LL_WINDOWS
	mOFN.lStructSize = sizeof(OPENFILENAMEW);
	mOFN.hwndOwner = NULL;  // Set later
	mOFN.hInstance = NULL;
	mOFN.lpstrCustomFilter = NULL;
	mOFN.nMaxCustFilter = 0;
	mOFN.lpstrFile = NULL;							// set in open and close
	mOFN.nMaxFile = LL_MAX_PATH;
	mOFN.lpstrFileTitle = NULL;
	mOFN.nMaxFileTitle = 0;
	mOFN.lpstrInitialDir = NULL;
	mOFN.lpstrTitle = NULL;
	mOFN.Flags = 0;									// set in open and close
	mOFN.nFileOffset = 0;
	mOFN.nFileExtension = 0;
	mOFN.lpstrDefExt = NULL;
	mOFN.lCustData = 0L;
	mOFN.lpfnHook = NULL;
	mOFN.lpTemplateName = NULL;
	mFilesW[0] = '\0';
#elif LL_DARWIN
	mPickOptions = 0;
#endif

}

LLFilePicker::~LLFilePicker()
{
	// nothing
}

// utility function to check if access to local file system via file browser 
// is enabled and if not, tidy up and indicate we're not allowed to do this.
bool LLFilePicker::check_local_file_access_enabled()
{
	// if local file browsing is turned off, return without opening dialog
	bool local_file_system_browsing_enabled = gSavedSettings.getBOOL("LocalFileSystemBrowsingEnabled");
	if ( ! local_file_system_browsing_enabled )
	{
		mFiles.clear();
		return false;
	}

	return true;
}

const std::string LLFilePicker::getFirstFile()
{
	mCurrentFile = 0;
	return getNextFile();
}

const std::string LLFilePicker::getNextFile()
{
	if (mCurrentFile >= getFileCount())
	{
		mLocked = false;
		return std::string();
	}
	else
	{
		return mFiles[mCurrentFile++];
	}
}

const std::string LLFilePicker::getCurFile()
{
	if (mCurrentFile >= getFileCount())
	{
		mLocked = false;
		return std::string();
	}
	else
	{
		return mFiles[mCurrentFile];
	}
}

void LLFilePicker::reset()
{
	mLocked = false;
	mFiles.clear();
	mCurrentFile = 0;
}

#if LL_WINDOWS

bool LLFilePicker::setupFilter(ELoadFilter filter)
{
	bool res = true;
	switch (filter)
	{
    case FFLOAD_ALL:
    case FFLOAD_EXE:
		mOFN.lpstrFilter = L"All Files (*.*)\0*.*\0" \
		SOUND_FILTER \
		IMAGE_FILTER \
		ANIM_FILTER \
		L"\0";
		break;
	case FFLOAD_WAV:
		mOFN.lpstrFilter = SOUND_FILTER \
			L"\0";
		break;
	case FFLOAD_IMAGE:
		mOFN.lpstrFilter = IMAGE_FILTER \
			L"\0";
		break;
	case FFLOAD_ANIM:
		mOFN.lpstrFilter = ANIM_FILTER \
			L"\0";
		break;
	case FFLOAD_COLLADA:
		mOFN.lpstrFilter = COLLADA_FILTER \
			L"\0";
		break;
	case FFLOAD_XML:
		mOFN.lpstrFilter = XML_FILTER \
			L"\0";
		break;
	case FFLOAD_SLOBJECT:
		mOFN.lpstrFilter = SLOBJECT_FILTER \
			L"\0";
		break;
	case FFLOAD_RAW:
		mOFN.lpstrFilter = RAW_FILTER \
			L"\0";
		break;
	case FFLOAD_MODEL:
		mOFN.lpstrFilter = MODEL_FILTER \
			L"\0";
		break;
	case FFLOAD_SCRIPT:
		mOFN.lpstrFilter = SCRIPT_FILTER \
			L"\0";
		break;
	case FFLOAD_DICTIONARY:
		mOFN.lpstrFilter = DICTIONARY_FILTER \
			L"\0";
		break;
// <FS:CR> Import filter
	case FFLOAD_IMPORT:
		mOFN.lpstrFilter = IMPORT_FILTER \
			L"\0";
		break;
// </FS:CR>
	default:
		res = false;
		break;
	}
	return res;
}

bool LLFilePicker::getOpenFile(ELoadFilter filter, bool blocking)
{
	if( mLocked )
	{
		return false;
	}
	bool success = false;

	// if local file browsing is turned off, return without opening dialog
	if ( check_local_file_access_enabled() == false )
	{
		return false;
	}

	// don't provide default file selection
	mFilesW[0] = '\0';

	mOFN.hwndOwner = (HWND)gViewerWindow->getPlatformWindow();
	mOFN.lpstrFile = mFilesW;
	mOFN.nMaxFile = SINGLE_FILENAME_BUFFER_SIZE;
	mOFN.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR ;
	mOFN.nFilterIndex = 1;

	setupFilter(filter);
	
	if (blocking)
	{
		// Modal, so pause agent
		send_agent_pause();
	}

	reset();
	
	// NOTA BENE: hitting the file dialog triggers a window focus event, destroying the selection manager!!
	success = GetOpenFileName(&mOFN);
	if (success)
	{
		std::string filename = utf16str_to_utf8str(llutf16string(mFilesW));
		mFiles.push_back(filename);
	}

	if (blocking)
	{
		send_agent_resume();
		// Account for the fact that the app has been stalled.
		LLFrameTimer::updateFrameTime();
	}
	
	return success;
}

bool LLFilePicker::getMultipleOpenFiles(ELoadFilter filter, bool blocking)
{
	if( mLocked )
	{
		return false;
	}
	bool success = false;

	// if local file browsing is turned off, return without opening dialog
	if ( check_local_file_access_enabled() == false )
	{
		return false;
	}

	// don't provide default file selection
	mFilesW[0] = '\0';

	mOFN.hwndOwner = (HWND)gViewerWindow->getPlatformWindow();
	mOFN.lpstrFile = mFilesW;
	mOFN.nFilterIndex = 1;
	mOFN.nMaxFile = FILENAME_BUFFER_SIZE;
	mOFN.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR |
		OFN_EXPLORER | OFN_ALLOWMULTISELECT;

	setupFilter(filter);

	reset();
	
	if (blocking)
	{
	// Modal, so pause agent
	send_agent_pause();
	}

	// NOTA BENE: hitting the file dialog triggers a window focus event, destroying the selection manager!!
	success = GetOpenFileName(&mOFN); // pauses until ok or cancel.
	if( success )
	{
		// The getopenfilename api doesn't tell us if we got more than
		// one file, so we have to test manually by checking string
		// lengths.
		if( wcslen(mOFN.lpstrFile) > mOFN.nFileOffset )	/*Flawfinder: ignore*/
		{
			std::string filename = utf16str_to_utf8str(llutf16string(mFilesW));
			mFiles.push_back(filename);
		}
		else
		{
			mLocked = true;
			WCHAR* tptrw = mFilesW;
			std::string dirname;
			while(1)
			{
				if (*tptrw == 0 && *(tptrw+1) == 0) // double '\0'
					break;
				if (*tptrw == 0)
					tptrw++; // shouldn't happen?
				std::string filename = utf16str_to_utf8str(llutf16string(tptrw));
				if (dirname.empty())
					dirname = filename + "\\";
				else
					mFiles.push_back(dirname + filename);
				tptrw += wcslen(tptrw);
			}
		}
	}

	if (blocking)
	{
	send_agent_resume();
	}

	// Account for the fact that the app has been stalled.
	LLFrameTimer::updateFrameTime();
	return success;
}

bool LLFilePicker::getSaveFile(ESaveFilter filter, const std::string& filename, bool blocking)
{
	if( mLocked )
	{
		return false;
	}
	bool success = false;

	// if local file browsing is turned off, return without opening dialog
	if ( check_local_file_access_enabled() == false )
	{
		return false;
	}

	mOFN.lpstrFile = mFilesW;
	if (!filename.empty())
	{
		llutf16string tstring = utf8str_to_utf16str(filename);
		wcsncpy(mFilesW, tstring.c_str(), FILENAME_BUFFER_SIZE);	}	/*Flawfinder: ignore*/
	else
	{
		mFilesW[0] = '\0';
	}
	mOFN.hwndOwner = (HWND)gViewerWindow->getPlatformWindow();

	switch( filter )
	{
	case FFSAVE_ALL:
		mOFN.lpstrDefExt = NULL;
		mOFN.lpstrFilter =
			L"All Files (*.*)\0*.*\0" \
			L"WAV Sounds (*.wav)\0*.wav\0" \
			L"Targa, Bitmap Images (*.tga; *.bmp)\0*.tga;*.bmp\0" \
			L"\0";
		break;
	case FFSAVE_WAV:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.wav", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
		}
		mOFN.lpstrDefExt = L"wav";
		mOFN.lpstrFilter =
			L"WAV Sounds (*.wav)\0*.wav\0" \
			L"\0";
		break;
	case FFSAVE_TGA:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.tga", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
		}
		mOFN.lpstrDefExt = L"tga";
		mOFN.lpstrFilter =
			L"Targa Images (*.tga)\0*.tga\0" \
			L"\0";
		break;
	case FFSAVE_BMP:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.bmp", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
		}
		mOFN.lpstrDefExt = L"bmp";
		mOFN.lpstrFilter =
			L"Bitmap Images (*.bmp)\0*.bmp\0" \
			L"\0";
		break;
	case FFSAVE_PNG:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.png", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
		}
		mOFN.lpstrDefExt = L"png";
		mOFN.lpstrFilter =
			L"PNG Images (*.png)\0*.png\0" \
			L"\0";
		break;
	case FFSAVE_TGAPNG:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.png", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
			//PNG by default
		}
		mOFN.lpstrDefExt = L"png";
		mOFN.lpstrFilter =
			L"PNG Images (*.png)\0*.png\0" \
			L"Targa Images (*.tga)\0*.tga\0" \
			L"\0";
		break;
		
	case FFSAVE_JPEG:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.jpeg", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
		}
		mOFN.lpstrDefExt = L"jpg";
		mOFN.lpstrFilter =
			L"JPEG Images (*.jpg *.jpeg)\0*.jpg;*.jpeg\0" \
			L"\0";
		break;
	case FFSAVE_AVI:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.avi", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
		}
		mOFN.lpstrDefExt = L"avi";
		mOFN.lpstrFilter =
			L"AVI Movie File (*.avi)\0*.avi\0" \
			L"\0";
		break;
	case FFSAVE_ANIM:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.xaf", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
		}
		mOFN.lpstrDefExt = L"xaf";
		mOFN.lpstrFilter =
			L"XAF Anim File (*.xaf)\0*.xaf\0" \
			L"\0";
		break;
	case FFSAVE_XML:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.xml", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
		}

		mOFN.lpstrDefExt = L"xml";
		mOFN.lpstrFilter =
			L"XML File (*.xml)\0*.xml\0" \
			L"\0";
		break;
	case FFSAVE_COLLADA:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.collada", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
		}
		mOFN.lpstrDefExt = L"collada";
		mOFN.lpstrFilter =
			L"COLLADA File (*.collada)\0*.collada\0" \
			L"\0";
		break;
	case FFSAVE_RAW:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.raw", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
		}
		mOFN.lpstrDefExt = L"raw";
		mOFN.lpstrFilter =	RAW_FILTER \
							L"\0";
		break;
	case FFSAVE_J2C:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.j2c", FILENAME_BUFFER_SIZE);
		}
		mOFN.lpstrDefExt = L"j2c";
		mOFN.lpstrFilter =
			L"Compressed Images (*.j2c)\0*.j2c\0" \
			L"\0";
		break;
	case FFSAVE_SCRIPT:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.lsl", FILENAME_BUFFER_SIZE);
		}
		mOFN.lpstrDefExt = L"txt";
		mOFN.lpstrFilter = L"LSL Files (*.lsl)\0*.lsl\0" L"\0";
		break;
			
// <Firestorm>
	case FFSAVE_BEAM:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.xml", FILENAME_BUFFER_SIZE);	/*Flawfinder: ignore*/
		}
		mOFN.lpstrDefExt = L"xml";
		mOFN.lpstrFilter =
			L"XML File (*.xml)\0*.xml\0" \
			L"\0";
		break;
	case FFSAVE_EXPORT:
		if (filename.empty())
		{
			wcsncpy( mFilesW,L"untitled.oxp", FILENAME_BUFFER_SIZE);
		}
		mOFN.lpstrDefExt = L"oxp";
		mOFN.lpstrFilter = L"OXP Backup Files (*.oxp)\0*.oxp\0" L"\0";
		break;
	case FFSAVE_CSV:
		if (filename.empty())
		{
			wcsncpy( mFilesW, L"untitled.csv", FILENAME_BUFFER_SIZE);
		}
		mOFN.lpstrDefExt = L".csv";
		mOFN.lpstrFilter =
		L"Comma seperated values (*.csv)\0*.csv\0" \
		L"\0";
		break;
// </Firestorm>
	default:
		return false;
	}

 
	mOFN.nMaxFile = SINGLE_FILENAME_BUFFER_SIZE;
	mOFN.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;

	reset();

	if (blocking)
	{
	// Modal, so pause agent
	send_agent_pause();
	}

	{
		// NOTA BENE: hitting the file dialog triggers a window focus event, destroying the selection manager!!
		try
		{
			success = GetSaveFileName(&mOFN);
			if (success)
			{
				std::string filename = utf16str_to_utf8str(llutf16string(mFilesW));
				mFiles.push_back(filename);
			}
		}
		catch (...)
		{
			LOG_UNHANDLED_EXCEPTION("");
		}
		gKeyboard->resetKeys();
	}

	if (blocking)
	{
	send_agent_resume();
	}

	// Account for the fact that the app has been stalled.
	LLFrameTimer::updateFrameTime();
	return success;
}

#elif LL_DARWIN

std::unique_ptr<std::vector<std::string>> LLFilePicker::navOpenFilterProc(ELoadFilter filter) //(AEDesc *theItem, void *info, void *callBackUD, NavFilterModes filterMode)
{
    std::unique_ptr<std::vector<std::string>> allowedv(new std::vector< std::string >);
    switch(filter)
    {
        case FFLOAD_ALL:
            allowedv->push_back("wav");
            allowedv->push_back("bvh");
            allowedv->push_back("anim");
            allowedv->push_back("dae");
            allowedv->push_back("raw");
            allowedv->push_back("lsl");
            allowedv->push_back("dic");
            allowedv->push_back("xcu");
            allowedv->push_back("gif");
            allowedv->push_back("xml");
            // <FS:CR> Import filter
            allowedv->push_back("oxp");
            //allowedv->push_back("hpa");
            // </FS:CR>
        case FFLOAD_IMAGE:
            allowedv->push_back("jpg");
            allowedv->push_back("jpeg");
            allowedv->push_back("bmp");
            allowedv->push_back("tga");
            allowedv->push_back("bmpf");
            allowedv->push_back("tpic");
            allowedv->push_back("png");
            break;
        case FFLOAD_EXE:
            break;
        case FFLOAD_WAV:
            allowedv->push_back("wav");
            break;
        case FFLOAD_ANIM:
            allowedv->push_back("bvh");
            allowedv->push_back("anim");
            break;
	case FFLOAD_MODEL:
        case FFLOAD_COLLADA:
            allowedv->push_back("dae");
            break;
        case FFLOAD_XML:
            allowedv->push_back("xml");
            break;
        case FFLOAD_RAW:
            allowedv->push_back("raw");
            break;
        case FFLOAD_SCRIPT:
            allowedv->push_back("lsl");
            break;
        case FFLOAD_DICTIONARY:
            allowedv->push_back("dic");
            allowedv->push_back("xcu");
            break;
        case FFLOAD_DIRECTORY:
            break;
	// <FS:CR> Import filter
	case FFLOAD_IMPORT:
            allowedv->push_back("oxp");
            //allowedv->push_back("hpa");
	    break;
	// </FS:CR>
        default:
            LL_WARNS() << "Unsupported format." << LL_ENDL;
    }

	return allowedv;
}

bool	LLFilePicker::doNavChooseDialog(ELoadFilter filter)
{
	// if local file browsing is turned off, return without opening dialog
	if (!check_local_file_access_enabled())
	{
		return false;
	}
    
	gViewerWindow->getWindow()->beforeDialog();
    
    std::unique_ptr<std::vector<std::string>> allowed_types = navOpenFilterProc(filter);
    
    std::unique_ptr<std::vector<std::string>> filev  = doLoadDialog(allowed_types.get(),
                                                    mPickOptions);

	gViewerWindow->getWindow()->afterDialog();


    if (filev && filev->size() > 0)
    {
        mFiles.insert(mFiles.end(), filev->begin(), filev->end());
        return true;
    }
	
	return false;
}

bool	LLFilePicker::doNavSaveDialog(ESaveFilter filter, const std::string& filename)
{
	
	// Setup the type, creator, and extension
    std::string		extension, type, creator;
    
	switch (filter)
	{
		case FFSAVE_WAV:
			type = "WAVE";
			creator = "TVOD";
			extension = "wav";
			break;
		case FFSAVE_TGA:
			type = "TPIC";
			creator = "prvw";
			extension = "tga";
			break;
		case FFSAVE_TGAPNG:
			type = "PNG";
			creator = "prvw";
			extension = "png,tga,jpg,jpeg,j2c,bmp";
			break;
		case FFSAVE_BMP:
			type = "BMPf";
			creator = "prvw";
			extension = "bmp";
			break;
		case FFSAVE_JPEG:
			type = "JPEG";
			creator = "prvw";
			extension = "jpeg";
			break;
		case FFSAVE_PNG:
			type = "PNG ";
			creator = "prvw";
			extension = "png";
			break;
		case FFSAVE_AVI:
			type = "\?\?\?\?";
			creator = "\?\?\?\?";
			extension = "mov";
			break;

		case FFSAVE_ANIM:
			type = "\?\?\?\?";
			creator = "\?\?\?\?";
			extension = "xaf";
			break;
			
		case FFSAVE_XML:
			type = "\?\?\?\?";
			creator = "\?\?\?\?";
			extension = "xml";
			break;
			
		case FFSAVE_RAW:
			type = "\?\?\?\?";
			creator = "\?\?\?\?";
			extension = "raw";
			break;

		case FFSAVE_J2C:
			type = "\?\?\?\?";
			creator = "prvw";
			extension = "j2c";
			break;
		
		case FFSAVE_SCRIPT:
			type = "LSL ";
			creator = "\?\?\?\?";
			extension = "lsl";
			break;
		// <FS:CR> Export filter
		case FFSAVE_EXPORT:
			type = "OXP ";
			creator = "\?\?\?\?";
			extension = "oxp";
			break;
		case FFSAVE_COLLADA:
			type = "DAE ";
			creator = "\?\?\?\?";
			extension = "dae";
			break;
		// <FS:CR> CSV Filter
		case FFSAVE_CSV:
			type = "CSV ";
			creator = "\?\?\?\?";
			extension = "csv";
			break;
		// </FS:CR>
		case FFSAVE_BEAM:

		case FFSAVE_ALL:
		default:
			type = "\?\?\?\?";
			creator = "\?\?\?\?";
			extension = "";
			break;
	}
	
    std::string namestring = filename;
    if (namestring.empty()) namestring="Untitled";
    
//    if (! boost::algorithm::ends_with(namestring, extension) )
//    {
//        namestring = namestring + "." + extension;
//        
//    }
    
	gViewerWindow->getWindow()->beforeDialog();

	// Run the dialog
    std::unique_ptr<std::string> filev = doSaveDialog(&namestring, 
                 &type,
                 &creator,
                 &extension,
                 mPickOptions);

	gViewerWindow->getWindow()->afterDialog();

	if ( filev && !filev->empty() )
	{
        mFiles.push_back(*filev);
		return true;
    }
	
	return false;
}

bool LLFilePicker::getOpenFile(ELoadFilter filter, bool blocking)
{
	if( mLocked )
		return false;

	bool success = false;

	// if local file browsing is turned off, return without opening dialog
	if ( check_local_file_access_enabled() == false )
	{
		return false;
	}

	reset();
	
    mPickOptions &= ~F_MULTIPLE;
    mPickOptions |= F_FILE;
 
    if (filter == FFLOAD_DIRECTORY) //This should only be called from lldirpicker. 
    {

        mPickOptions |= ( F_NAV_SUPPORT | F_DIRECTORY );
        mPickOptions &= ~F_FILE;
    }

	if (filter == FFLOAD_ALL)	// allow application bundles etc. to be traversed; important for DEV-16869, but generally useful
	{
        mPickOptions |= F_NAV_SUPPORT;
	}
	
	if (blocking) // always true for mac
	{
		// Modal, so pause agent
		send_agent_pause();
	}


	success = doNavChooseDialog(filter);
		
	if (success)
	{
		if (!getFileCount())
			success = false;
	}

	if (blocking)
	{
		send_agent_resume();
		// Account for the fact that the app has been stalled.
		LLFrameTimer::updateFrameTime();
	}

	return success;
}

bool LLFilePicker::getMultipleOpenFiles(ELoadFilter filter, bool blocking)
{
	if( mLocked )
		return false;

	bool success = false;

	// if local file browsing is turned off, return without opening dialog
	if ( check_local_file_access_enabled() == false )
	{
		return false;
	}

	reset();
    
    mPickOptions |= F_FILE;

    mPickOptions |= F_MULTIPLE;
	
	if (blocking) // always true for mac
	{
	// Modal, so pause agent
	send_agent_pause();
	}
    
	success = doNavChooseDialog(filter);
    
	if (blocking)
	{
		send_agent_resume();
	}

	if (success)
	{
		if (!getFileCount())
			success = false;
		if (getFileCount() > 1)
			mLocked = true;
	}

	// Account for the fact that the app has been stalled.
	LLFrameTimer::updateFrameTime();
	return success;
}

bool LLFilePicker::getSaveFile(ESaveFilter filter, const std::string& filename, bool blocking)
{

	if( mLocked )
		return false;
	bool success = false;

	// if local file browsing is turned off, return without opening dialog
	if ( check_local_file_access_enabled() == false )
	{
		return false;
	}

	reset();
	
    mPickOptions &= ~F_MULTIPLE;

	if (blocking)
	{
	// Modal, so pause agent
	send_agent_pause();
	}

    success = doNavSaveDialog(filter, filename);

    if (success)
	{
		if (!getFileCount())
			success = false;
	}

	if (blocking)
	{
	send_agent_resume();
	}

	// Account for the fact that the app has been stalled.
	LLFrameTimer::updateFrameTime();
	return success;
}
//END LL_DARWIN


#endif
