/*
*	LogUI.h, Copyright Jonathan Mackey 2021
*	Handles the UI.
*
*	GNU license:
*	This program is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*	Please maintain this license information along with authorship and copyright
*	notices in any redistribution of this code.
*
*/
#ifndef LogUI_h
#define LogUI_h

#include <inttypes.h>
#include <time.h>
#include "MSPeriod.h"
#include "MP3YX5200.h"	// MP3 player
#include "UnixTimeEditor.h"
#include "RFM69.h"    // https://github.com/LowPowerLab/RFM69
#include "XFont.h"

typedef uint32_t time32_t;
class HikeLog;

class LogUI : public XFont
{
public:
	struct SString_PDesc
	{
		const char*	descStr;
		uint16_t	color;
	};
							LogUI(void);
	enum EMode
	{
		eLogMode,
		eResetLogMode,			// + EResetLogState
		eStartLocSelMode,
		eEndLocSelMode,
		eReviewHikesMode,
		eSetTimeMode,
		eEditTimeMode,
		eTestMP3Mode,
		eBMP280SyncMode,		// + ESyncState
		eSDCardMode				// + ESDCardAction + ESDCardState 
	};
		
	enum ESyncState
	{
		eBMP280SyncError,
		eBMP280Syncing,
		eBMP280SyncSuccess
	};
	
	enum ESDCardAction
	{
		eSaveHikeLogUI,
		eSaveLocationsAction,
		eUpdateLocationsAction,
		eNumSDCardActions
	};

	enum ESDCardState
	{
		eSavingToSD,
		eUpdatingFromSD,
		eEjectSDCardNoReset,
		eSDError,
		eSDSavedSuccess,
		eSDUpdateSuccess,
		eEjectSDCardAllowReset,
		eSDCardIdle
	};
	
	enum EResetLogState
	{
		eResetVerifyYes,
		eResetVerifyNo,
		eResetSuccess,
		eResetError	
	};
	
	enum EReviewState
	{
		eReviewLocs,
		eReviewData
	};

	void					begin(
								HikeLog*				inHikeLog,
								DisplayController*		inDisplay,
								Font*					inNormalFont,
								Font*					inSmallFont);
	void					SetSDCardPresent(
								bool					inSDCardPresent);
	void					SetSDWriteSuccessAction(void);
	
	void					CheckRadioForPackets(
								bool					inDisplayIsOff);
	void					SyncWithBMP280Remote(void);
		
	uint8_t					Mode(void) const
								{return(mMode);}
	void					GoToLogMode(void);
	uint8_t					SyncState(void) const
								{return(mSyncState);}
	uint8_t					SDCardState(void) const
								{return(mSDCardState);}
	uint8_t					SDCardAction(void) const
								{return(mSDCardAction);}
	uint8_t					ResetLogState(void) const
								{return(mResetLogState);}
	uint8_t					LogStateModifier(void) const
								{return(mLogStateModifier);}
	uint16_t				LocIndex(void) const
								{return(mLocIndex);}
	uint16_t				HikeRef(void) const
								{return(mHikeRef);}
	uint8_t					ReviewState(void) const
								{return(mReviewState);}
								
	void					EnterPressed(void);
	void					UpDownButtonPressed(
								bool					inIncrement);
	void					LeftRightButtonPressed(
								bool					inIncrement);
	void					Update(void);
	// The following 2 routines are used by ISRs
	static void				SetSDInsertedOrRemoved(void)
								{sSDInsertedOrRemoved = true;}
	static void				SetButtonPressed(
								bool					inButtonPressed)
								{sButtonPressed = sButtonPressed || inButtonPressed;}
protected:
	RFM69		mRadio;
	MP3YX5200WithSleep mMP3Player;
	UnixTimeEditor	mUnixTimeEditor;
	HikeLog*	mHikeLog;
	Font*		mNormalFont;
	Font*		mSmallFont;
	MSPeriod	mDebouncePeriod;	// For buttons and SD card
	MSPeriod	mBMP280Period;
	MSPeriod	m3ButtonRemotePeriod;
	uint16_t	mLocIndex;	// for eStartLocSelMode and eEndLocSelMode
	uint16_t	mHikeRef;
	uint8_t		mMode;
	uint8_t		mSyncState;
	uint8_t		mSDCardState;
	uint8_t		mSDCardAction;
	uint8_t		mResetLogState;
	uint8_t		mLogStateModifier;
	uint8_t		mReviewState;
	uint8_t		mStartPinState;
	bool		mSDCardPresent;
	bool		mSleeping;

	uint8_t		mPrevLogState;
	uint8_t		mPrevMode;
	uint16_t	mPrevLocIndex;
	uint16_t	mPrevHikeRef;
	uint8_t		mPrevShowingAMPM;
	uint8_t		mPrevSyncState;
	uint8_t		mPrevResetLogState;
	uint8_t		mPrevSDCardState;
	uint8_t		mPrevSDCardAction;
	uint8_t		mPrevReviewState;
	
	static bool	sButtonPressed;
	static bool	sSDInsertedOrRemoved;
	/*
	*	All of the InitxxxPacket function declarations don't reference anything
	*	from the Log namespace to avoid having to place LogPacket.h in this
	*	header.
	*/
	uint8_t					InitSyncPacket(
								uint8_t*				inPacket);
	uint8_t					InitLocationPacket(
								uint16_t				inLocIndex,
								uint8_t*				inPacket);
	bool					HandleBMP280PacketRx(void);
	void					HandlePacketRx(void);

	void					WakeUp(void);
	void					GoToSleep(void);
	void					UpdateDisplay(void);
	void					DrawLocation(
								uint16_t				inLocIndex,
								uint8_t					inFirstLine = 1);
	void					DrawTime(
								time32_t				inTime,
								bool					inShowingAMPM);
	void					DrawIndexedDescStr(
								const SString_PDesc*	inStringList,
								uint8_t					inStrIndex,
								bool					inHasOptions,
								bool					inCentered);
	void					DrawTextOption(
								const char*				inStrRef,
								uint16_t				inColor,
								bool					inHasOptions,
								bool					inCentered);
	void					ClearLines(
								uint8_t					inStartLine = 0,
								uint8_t					inNumLines = 3);

};

#endif // LogUI_h
