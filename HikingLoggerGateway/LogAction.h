/*
*	LogAction.h, Copyright Jonathan Mackey 2019
*	Handles input from the UI.
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
#ifndef LogAction_h
#define LogAction_h

#include <inttypes.h>
#include <time.h>
#include "MSPeriod.h"
class RFM69;
class XFont;
class HikeLog;

class LogAction
{
public:
							LogAction(void);
	enum EMode
	{
		eLogMode,
		eResetLogMode,		// + EResetLogState
		eStartLocSelMode,
		eEndLocSelMode,
		eReviewHikesMode,
		eBMP280SyncMode,	// + ESyncState
		eSDCardMode			// + ESDCardState
	};
		
	enum ESyncState
	{
		eBMP280SyncError,
		eBMP280Syncing,
		eBMP280SyncSuccess
	};

	enum ESDCardState
	{
		eSaveToSD,				// 0 b000
		eSavingToSD,			// 1 b001
		eEjectSDCardNoReset,	// 2 b010
		eSDSaveError,			// 3 b011
		eSDWriteSuccess,		// 4 b100
		eEjectSDCardAllowReset	// 5 b101
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

	void					Initialize(
								RFM69*					inRadio,
								HikeLog*				inHikeLog);
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
	void					IncrementMode(
								bool					inIncrement);
	void					IncrementValue(
								bool					inIncrement);
	void					Update(void);
protected:
	RFM69*		mRadio;
	HikeLog*	mHikeLog;
	MSPeriod	mBMP280Period;
	MSPeriod	m3ButtonRemotePeriod;
	uint16_t	mLocIndex;	// for eStartLocSelMode and eEndLocSelMode
	uint16_t	mHikeRef;
	uint8_t		mMode;
	uint8_t		mSyncState;
	uint8_t		mSDCardState;
	uint8_t		mResetLogState;
	uint8_t		mLogStateModifier;
	uint8_t		mReviewState;
	bool		mSDCardPresent;
	
	void					SetActionIndex(
								uint8_t					inActionIndex);
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

};

#endif // LogAction_h
