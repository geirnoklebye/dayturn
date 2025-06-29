/** 
 * @file llviewernetwork.h
 * @author James Cook
 * @brief Networking constants and globals for viewer.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * With modifications Copyright (C) 2012, arminweatherwax@lavabit.com
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

#ifndef LL_LLVIEWERNETWORK_H
#define LL_LLVIEWERNETWORK_H

#include "../llxml/llxmlnode.h"
                                                                                                       
#include <boost/function.hpp>
#include <boost/signals2.hpp>


extern const char* DEFAULT_LOGIN_PAGE;
#define KNOWN_GRIDS_SIZE 3
#define AGNI "Second Life"
#define ADITI "Second Life Beta"
#define GRID_VALUE "name"
#define GRID_LABEL_VALUE "gridname"
#define GRID_NICK_VALUE "gridnick"
#define GRID_ID_VALUE "grid_login_id"
#define GRID_LOGIN_URI_VALUE "loginuri"
#define GRID_HELPER_URI_VALUE "helperuri"
#define GRID_LOGIN_PAGE_VALUE "loginpage"
#define GRID_IS_SYSTEM_GRID_VALUE "system_grid"
#define GRID_IS_FAVORITE_VALUE "favorite"
#define GRID_REGISTER_NEW_ACCOUNT "register"
#define GRID_FORGOT_PASSWORD "password"
#define MAINGRID "grid.xmir.org:8002"
#define GRID_LOGIN_IDENTIFIER_TYPES "login_identifier_types"
// <FS:CR> Aurora Sim
#define GRID_HELP "help"
#define GRID_ABOUT "about"
#define GRID_SEARCH    "search"
#define GRID_PROFILE_URI_VALUE "profileuri"
#define GRID_SENDGRIDINFO "SendGridInfoToViewerOnLogin"
#define GRID_DIRECTORY_FEE "DirectoryFee"
#define GRID_CURRENCY_SYMBOL "CurrencySymbol"
#define GRID_REAL_CURRENCY_SYMBOL "RealCurrencySymbol"
#define GRID_MAXGROUPS "MaxGroups"
#define GRID_PLATFORM "platform"
#define GRID_MESSAGE "message"
/**
* defines slurl formats associated with various grids.
* we need to continue to support existing forms, as slurls
* are shared between viewers that may not understand newer
* forms.
*/
#define GRID_SLURL_BASE "slurl_base"
#define GRID_APP_SLURL_BASE "app_slurl_base"

class GridInfoRequestResponder;

struct GridEntry
{
	LLSD grid;
	LLXMLNodePtr info_root;
	bool set_current;
	std::string last_http_error;
};

/// Exception thrown when a grid is not valid
class LLInvalidGridName
{
public:
	LLInvalidGridName(std::string grid) : mGrid(grid)
	{
	}
	std::string name() { return mGrid; }
protected:
	std::string mGrid;
};

/**
* @brief A singleton class to manage the grids available to the viewer.
*
* This class maintains several properties for each known grid, and provides
* interfaces for obtaining each of these properties given a specified
* grid.  Grids are specified by either of two identifiers, each of which
* must be unique among all known grids:
* - grid name : DNS name for the grid
* - grid id   : a short form (conventionally a single word)
*
* This class maintains the currently selected grid, and provides short
* form accessors for each of the properties of the selected grid.
**/
class LLGridManager : public LLSingleton<LLGridManager>
{
	/// Instantiate the grid manager, load default grids, selects the default grid
	LLSINGLETON(LLGridManager);
	~LLGridManager();

public:
  typedef enum
  {
      FETCH,
      FETCHTEMP,
      SYSTEM,
      RETRY,
      LOCAL,
      FINISH,
      TRYLEGACY,
      FAIL,
      REMOVE
  } AddState;

public:
    void initGrids();
    void initSystemGrids();
    void initGridList(std::string grid_file, AddState state);
    void initCmdLineGrids();
    void resetGrids();
    /// grid list management
    bool isReadyToLogin()   {return mReadyToLogin;}

    /// add a grid to the list of grids
    void addGrid(const std::string& loginuri);
    void removeGrid(const std::string& grid);
    void reFetchGrid() { reFetchGrid(mGrid, true); }
    void reFetchGrid(const std::string& grid, bool set_current = false);
    
    /** ================================================================
     * @name Grid Identifiers
     * The id is a short form (typically one word) grid name,
     * It should be used in URL path elements or parameters
     *
     * Each grid also has a "label", intented to be a user friendly
     * descriptive form (it is used in the login panel grid menu, for example).
     */
    std::map<std::string, std::string> getKnownGrids();
    /// Return the name of a grid, given either its name or its id
    std::string getGrid( const std::string &grid );

    /// this was getGridInfo - renamed to avoid ambiguity with the OpenSim grid_info
    void getGridData(const std::string& grid, LLSD &grid_info);
    void getGridData(LLSD &grid_info) { getGridData(mGrid, grid_info); }
    /// select a given grid as the current grid.  If the grid
    /// is not a known grid, then it's assumed to be a dns name for the
    /// grid, and the various URIs will be automatically generated.
    void setGridChoice(const std::string& grid);

    ///get the grid label e.g. "Second Life"
    std::string getGridLabel() { return mGridList[mGrid][GRID_LABEL_VALUE]; }
    ///get the grid nick e.g. "agni"
    std::string getGridNick() { return mGridList[mGrid][GRID_NICK_VALUE].asString(); }
    ///get the grid e.g. "login.agni.lindenlab.com"
    std::string getGrid() const { return mGrid; }
    /// get the first (and very probably only) login URI of a specified grid
    std::string getLoginURI(const std::string& grid);
    /// get the Login URIs of the current grid
	void getLoginURIs(std::vector<std::string>& uris);

    std::string getHelperURI();
	std::string getLoginPage();
    std::string getGridLoginID() { return mGridList[mGrid][GRID_VALUE]; }
    /// was ://    std::string getGridLoginID() { return mGridList[mGrid][GRID_ID_VALUE]; }
    /// however we already have that in GRID_VALUE
    std::string getLoginPage(const std::string& grid) { return mGridList[grid][GRID_LOGIN_PAGE_VALUE]; }
    void        getLoginIdentifierTypes(LLSD& idTypes) { idTypes = mGridList[mGrid][GRID_LOGIN_IDENTIFIER_TYPES]; }

    std::string trimHypergrid(const std::string& trim);

    /** ================================================================
     * @name Update Related Properties
     * ================================================================
     */
    /// Get the update service URL base (host and path) for the selected grid
    std::string getUpdateServiceURL();
    /// build a slurl for the given region within the selected grid
    std::string getSLURLBase(const std::string& grid);
    std::string getSLURLBase() { return getSLURLBase(mGrid); }

    std::string getAppSLURLBase(const std::string& grid);
    std::string getAppSLURLBase() { return getAppSLURLBase(mGrid); }
    bool hasGrid(const std::string& grid){ return mGridList.has(grid); }
    bool isTemporary(){ return mGridList[mGrid].has("FLAG_TEMPORARY"); }
    bool isTemporary(const std::string& grid){ return mGridList[grid].has("FLAG_TEMPORARY"); }
 
    /// tell if we got this from a Hypergrid SLURL
    bool isHyperGrid(const std::string& grid) { return mGridList[grid].has("HG"); }

    /// tell if we know how to acess this grid via Hypergrid
    std::string getGatekeeper() { return getGatekeeper(mGrid); }
    std::string getGatekeeper(const std::string& grid) { return mGridList[grid].has("gatekeeper") ? mGridList[grid]["gatekeeper"].asString() : std::string(); }
    std::string getGridByProbing( const std::string &probe_for, bool case_sensitive = false);
    std::string getGridByLabel( const std::string &grid_label, bool case_sensitive = false);
    std::string getGridByGridNick( const std::string &grid_nick, bool case_sensitive = false);
    std::string getGridByHostName( const std::string &host_name, bool case_sensitive = false);
    std::string getGridByAttribute(const std::string &attribute, const std::string &attribute_value, bool case_sensitive );

    bool isSystemGrid(const std::string& grid)
    {
        return mGridList.has(grid) &&
              mGridList[grid].has(GRID_IS_SYSTEM_GRID_VALUE) &&
               mGridList[grid][GRID_IS_SYSTEM_GRID_VALUE].asBoolean();
    }
    bool isSystemGrid() { return isSystemGrid(mGrid); }
    /// Mark this grid as a favorite that should be persisited on 'save'
    /// this is currently used to persist a grid after a successful login
    void setFavorite() { mGridList[mGrid][GRID_IS_FAVORITE_VALUE] = true; }

    typedef boost::function<void(bool success)> grid_list_changed_callback_t;
    typedef boost::signals2::signal<void(bool success)> grid_list_changed_signal_t;

	boost::signals2::connection addGridListChangedCallback(grid_list_changed_callback_t cb);
	grid_list_changed_signal_t	mGridListChangedSignal;
	
    bool isInSLMain();
	bool isInSLBeta();
	bool isInOpenSim();

    bool isInSecondLife() { return (isInSLMain() || isInSLBeta()); }    // <FS:CR>
    void saveGridList();
    void clearFavorites();
    // <FS:CR> Variable parcel listing fee
    S32 sDirectoryFee;
    void setDirectoryFee(const S32 directory_fee) { sDirectoryFee = directory_fee; }
    S32 getDirectoryFee() { return sDirectoryFee; }
    // </FS:CR>
    // <FS:CR> FIRE-10567 - Variable classified fee
    S32 sClassifiedFee;
    void setClassifiedFee(const S32 classified_fee) { sClassifiedFee = classified_fee; }
    S32 getClassifiedFee() { return sClassifiedFee; }
    // <FS:CR> Variable parcel listing fee
private:
    friend class GridInfoRequestResponder;
    friend void downloadComplete( LLSD const &aData, LLGridManager* mOwner, GridEntry* mData, LLGridManager::AddState state );
    friend void downloadError( LLSD const &aData, LLGridManager* mOwner, GridEntry* mData, LLGridManager::AddState state );
    void addGrid(GridEntry* grid_info, AddState state);
    void incResponderCount(){++mResponderCount;}
    void decResponderCount(){--mResponderCount;}
    void gridInfoResponderCB(GridEntry* grid_data);

    void setGridData(const LLSD &grid_info) { mGridList[mGrid]=grid_info; }

protected:
	void updateIsInProductionGrid();

    void addSystemGrid(const std::string& label,
                       const std::string& name,
                       const std::string& login,
                       const std::string& helper,
                       const std::string& login_page,
                       const std::string& update_url_base,
                       const std::string& login_id = "");

	std::string mGrid;
	std::string mGridFile;
	LLSD mGridList;
    LLSD mConnectedGrid;
	bool mIsInSLMain;
	bool mIsInSLBeta;
	bool mIsInOpenSim;

    int mResponderCount;
    bool mReadyToLogin;
    bool mCommandLineDone;
};

const S32 MAC_ADDRESS_BYTES = 6;

#endif
