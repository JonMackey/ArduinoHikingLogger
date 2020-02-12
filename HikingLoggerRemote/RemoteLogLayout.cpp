/*
*	RemoteLogLayout.cpp, Copyright Jonathan Mackey 2019
*	Manages the remote display layout
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
#include "RemoteLogLayout.h"
#include "LogTempPres.h"
#include "LogDateTime.h"
#include "RemoteHikeLog.h"
#include "RemoteLogAction.h"
#include "DisplayController.h"
#include "BMP280Utils.h"
//#include <Arduino.h>

#ifdef SUPPORT_LOC_SEL_MODES
const char kStartLocStr[] PROGMEM = "START LOC";
const char kEndLocStr[] PROGMEM = "END LOC";
#endif
const char kStartStr[] PROGMEM = "START";
const char kSwapLocsStr[] PROGMEM = "SWAP";
const char kStopStr[] PROGMEM = "STOP";
const char kResumeStr[] PROGMEM = "RESUME";
const char kDoneStr[] PROGMEM = "DONE";
const char kWaitSyncingWithBaseStr[] PROGMEM = "SYNCING BASE";
const char kLogStartIsEndErrorStr[] PROGMEM = "START == END!";
const char kPressLeftToSyncStr[] PROGMEM = "[LEFT] 2 SYNC";
const char kPressModeToExitStr[] PROGMEM = "[MODE] 2 EXIT";
const char kBMP280SyncSuccessStr[] PROGMEM = "BMP SYNCD";
const char kBMP280ErrorStr[] PROGMEM = "SYNC BMP";
const char kBMP280SyncStr[] PROGMEM = "SYNCING BMP";
//const char kFullErrorStr[] PROGMEM = "LOG FULL";

const uint16_t	kInvalidLocIndex = 0;

/*
*	Assumptions:
*		- 160 x 80 display area.
*		- All text is uppercase to allow for 3 lines (modified height)
*		- 24pt large font with the height modified to 26 pixels from 29.  This
*		  is done in the font header (see MyriadPro_Regular_24::fontHeader)
*		- 14pt small font limited to 4 glyphs
*/

/********************************* RemoteLogLayout **********************************/
RemoteLogLayout::RemoteLogLayout(void)
{
}

/********************************* Initialize *********************************/
void RemoteLogLayout::Initialize(
	RemoteLogAction*	inLogAction,
	RemoteHikeLog*		inHikeLog,
	DisplayController*	inDisplay,
	XFont::Font*		inNormalFont,
	XFont::Font*		inSmallFont)
{
	mLogAction = inLogAction;
	mHikeLog = inHikeLog;
	SetDisplay(inDisplay, inNormalFont);
	mNormalFont = inNormalFont;
	mSmallFont = inSmallFont;
}

/******************************** DrawLocation ********************************/
void RemoteLogLayout::DrawLocation(
	bool	inStart)
{
	SHikeLocationLink&	link = mHikeLog->GetLocLink(inStart);
	if (link.loc.name[0] != 0)
	{
		MoveTo(1,0);
		SetTextColor(eOrange);
		DrawStr(link.loc.name, true);
		char elevationStr[32];
		int32_t	elevation = link.loc.elevation;
		strcpy(&elevationStr[BMP280Utils::Int32ToIntStr(elevation*100, elevationStr)], LogTempPres::GetInstance().GetAltitudeSuffixStr());
		MoveTo(2,0);
		SetTextColor(0xFBC0);
		DrawStr(elevationStr, true);
		mLocIndex = mHikeLog->GetLocIndex(inStart);
	} else
	{
		mLocIndex = kInvalidLocIndex;	// No location was drawn
	}
}

/******************************* ClearLines1to3 *******************************/
/*
*	Clears lines 1 to 3 without touching the bottom 2 pixel rows used as an
*	radio activity meter.
*/
void RemoteLogLayout::ClearLines1to3(void)
{
	mDisplay->MoveTo(0,0);
	mDisplay->FillBlock(29*3, 180, eBlack);
}
char* Int16ToDecStr(
	int16_t	inNum,
	char*	inBuffer);

/*********************************** Update ***********************************/
void RemoteLogLayout::Update(
	bool	inUpdateAll)
{
	uint8_t	mode = mLogAction->Mode();
	inUpdateAll = inUpdateAll || mode != mMode;
	mMode = mode;
	
	if (inUpdateAll)
	{
		ClearLines1to3();
	}
	
	switch (mode)
	{
		/*
		*	Info Mode:
		*	If log is running the elapsed time is displayed and percentage
		*	remaining on line 1.
		*	The time on line 2.
		*	The altitude and temperature on line 3.
		*/
		case RemoteLogAction::eInfoMode:
			if (inUpdateAll ||
				LogDateTime::TimeChanged())
			{
				LogDateTime::ResetTimeChanged();
				char timeStr[32];
				if (mHikeLog->Active())
				{
					time32_t	elapsedTime = mHikeLog->ElapsedTime();
					/*
					*	If everything is being updated OR
					*	the timer is active THEN
					*	draw the elapsed time on the 2nd line
					*/
					if (inUpdateAll ||
						elapsedTime)
					{
						MoveTo(1);
						LogDateTime::CreateTimeStr(elapsedTime, timeStr);
						SetTextColor(eYellow);
						DrawStr(timeStr);
					}
				}
				bool isPM = LogDateTime::CreateTimeStr(timeStr);
				MoveTo(0,29);
				SetTextColor(eWhite);
				DrawStr(timeStr);
				uint8_t showingAMPM = LogDateTime::Format24Hour() ? 0 : (isPM ? 1 : 2);
				/*
				*	If updating everything OR
				*	the AM/PM suffix state changed (to/from AM/PM or hidden) THEN
				*	draw or erase the suffix.
				*/
				if (inUpdateAll || (mShowingAMPM != showingAMPM))
				{
					mShowingAMPM = showingAMPM;
					if (showingAMPM)
					{
						SetFont(mSmallFont);
						DrawStr(isPM ? " PM":" AM");
						SetFont(mNormalFont);
						// The width of a P is slightly less than an A, so erase any
						// artifacts left over when going from A to P.
						// The width of an 14pt A - the width of a P = 2
						mDisplay->FillBlock(mFontRows, 2, eBlack);
					}
				}
			}
			/*
			*	If everything is being updated OR
			*	the temperature has changed THEN
			*	draw the current temperature
			*/
			if (inUpdateAll ||
				LogTempPres::GetInstance().TemperatureChanged())
			{
				MoveTo(2, 0);
				SetTextColor(eMagenta);
				if (LogTempPres::GetInstance().IsValid())
				{
					/*
					*	The Temperature is drawn on the left.
					*/
					char tempStr[32];
					uint8_t strLen = LogTempPres::GetInstance().CreateTempStr(tempStr) + 2;
					strcpy(&tempStr[strLen], LogTempPres::GetInstance().GetTempSuffixStr());
					DrawStr(tempStr);
					EraseTillColumn(58);	// Erase any artifacts when moving
											// to a shorter string.
				}
			}
			if (inUpdateAll ||
				LogTempPres::GetInstance().PressureChanged())
			{
				if (LogTempPres::GetInstance().IsValid() &&
					LogTempPres::GetInstance().StartingAltitude())
				{
					char altitudeStr[32];
					float	altitude = LogTempPres::GetInstance().CalcCurrentAltitude();
					uint8_t charsInStr = LogTempPres::GetInstance().CreateAltitudeStr(altitude, altitudeStr);
					MoveTo(2,0);
					SetTextColor(eYellow);
					DrawRightJustified(altitudeStr);
				#ifndef DEBUG_RADIO
					if (mHikeLog->Active())
					{ 
						MoveTo(1,0);
						LogTempPres::GetInstance().CreateAltitudePercentageStr(altitude, altitudeStr);
						uint16_t	textWidth = DrawRightJustified(altitudeStr);
						mDisplay->MoveToColumn(105);
						EraseTillColumn(160 - textWidth);
					}
				#endif
				}/* else
				{
					// Erase the altitude string and % string
					MoveTo(2,0);
					EraseTillEndOfLine();
				}*/
			}
		#ifdef DEBUG_RADIO
			{
				/*
				*	packetTimeouts reflects the number of packets that didn't
				*	get through after using all retries.
				*/
				uint16_t	packetTimeouts = mLogAction->PacketTimeouts();
				if (inUpdateAll ||
					mPacketTimeouts != packetTimeouts)
				{
					mPacketTimeouts = packetTimeouts;
					MoveTo(1,0);
					char packetTimeoutsStr[32];
					Int16ToDecStr(packetTimeouts, packetTimeoutsStr);
					SetTextColor(eWhite);
					DrawRightJustified(packetTimeoutsStr);
				}
			}
			{
				/*
				*	A value of 2 means the most recent packet got through
				*	without a retry.  A value of 0 means there were 2 retries.
				*	If there was a timeout, packetTimeouts above will increment.
				*/
				uint8_t	waitingForPacket = mLogAction->WaitingForPacket();
				if (inUpdateAll ||
					(waitingForPacket > 0 && mWaitingForPacket != waitingForPacket))
				{
					mWaitingForPacket = waitingForPacket;
					char waitingForPacketStr[32];
					Int16ToDecStr(waitingForPacket, waitingForPacketStr);
					SetTextColor(eCyan);
					MoveTo(1, 160-48);
					DrawStr(waitingForPacketStr);
				}
			}
		#endif
			break;
		/*
		*	Log Mode:
		*	if not running options are L "SWAP", R "START" on line 1
		*		Displays starting location name on line 2
		*		Displays starting location altitude on line 3
		*	if running options are L none, R "STOP"
		*	if paused/stopped, options are L "DONE", R "RESUME"
		*		Displays ending location name on line 2
		*		Displays ending location altitude on line 3
		*	If starting == ending loc, "START == END!" is displayed on
		*	line 1, and the LR buttons are disabled
		*/
		case RemoteLogAction::eLogMode:
		{
			uint8_t	logState = mHikeLog->GetLogState();
			if (inUpdateAll ||
				logState != mLogState)
			{
				mLogState = logState;
				if (!inUpdateAll)
				{
					ClearLines1to3();
				}
				MoveTo(0,0);
				{
					char actionStr[32];
					switch(logState)
					{
						case RemoteHikeLog::eCantRun:
							SetTextColor(eYellow);
							strcpy_P(actionStr, kLogStartIsEndErrorStr);
							DrawCentered(actionStr);
							break;
						case RemoteHikeLog::eNotRunning:
							SetTextColor(eWhite);
							strcpy_P(actionStr, kSwapLocsStr);
							DrawStr(actionStr);
							SetTextColor(eGreen);
							strcpy_P(actionStr, kStartStr);
							DrawRightJustified(actionStr);
							break;
						case RemoteHikeLog::eRunning:
							SetTextColor(eRed);
							strcpy_P(actionStr, kStopStr);
							DrawRightJustified(actionStr);
							break;
						case RemoteHikeLog::eStopped:
							SetTextColor(eWhite);
							strcpy_P(actionStr, kDoneStr);
							DrawStr(actionStr);
							SetTextColor(eGreen);
							strcpy_P(actionStr, kResumeStr);
							DrawRightJustified(actionStr);
							break;
					}
				}
			}
			{
				bool	isStart = logState <= RemoteHikeLog::eNotRunning;
				if (inUpdateAll ||
					mHikeLog->GetLocIndex(isStart) != mLocIndex ||
					mLocIndex == kInvalidLocIndex)
				{
					DrawLocation(isStart);
				}
			}
			break;
		}
	#ifdef SUPPORT_LOC_SEL_MODES
		case RemoteLogAction::eStartLocSelMode:
		case RemoteLogAction::eEndLocSelMode:
		{
			bool	isStart = mode == RemoteLogAction::eStartLocSelMode;
			if (inUpdateAll)
			{
				char titleStr[32];
				strcpy_P(titleStr, isStart ? kStartLocStr : kEndLocStr);
				MoveTo(0,0);
				SetTextColor(eWhite);
				DrawStr("<");
				DrawRightJustified(">");
				DrawCentered(titleStr);
			}
			
			if (inUpdateAll ||
				mHikeLog->GetLocIndex(isStart) != mLocIndex ||
				mLocIndex == kInvalidLocIndex)
			{
				DrawLocation(isStart);
			}
			break;
		}
	#endif
		/*
		*	Error Mode:
		*	Displays "ERROR" centered on line 1
		*	Possible errors:
		*		BMP Remote not sync'd "SYNC BMP ERR".
		*		"[LEFT] 2 SYNC" is displayed on line 3
		*	When sync starts line 3 displays "SYNCING BMP"
		*	If successful line 2 displays "BMP SYNCD" and
		*	line 3 displays [MODE] 2 EXIT
		*/
		case RemoteLogAction::eBMP280SyncMode:
		{
			uint8_t	syncState = mLogAction->SyncState();
			
			
			if (inUpdateAll ||
				syncState != mSyncState)
			{
				if (!inUpdateAll)
				{
					ClearLines1to3();
				}
				MoveTo(0,0);
				mSyncState = syncState;
				{
					char actionStr[32];
					switch(syncState)
					{
						case RemoteLogAction::eBMP280SyncSuccess:
							SetTextColor(eGreen);
							strcpy_P(actionStr, kBMP280SyncSuccessStr);
							DrawStr(actionStr);
							MoveTo(2,0);
							SetTextColor(eWhite);
							strcpy_P(actionStr, kPressModeToExitStr);
							DrawStr(actionStr);
							break;
						case RemoteLogAction::eBMP280Syncing:
							SetTextColor(eYellow);
							strcpy_P(actionStr, kBMP280SyncStr);
							DrawStr(actionStr);
							break;
						case RemoteLogAction::eBMP280SyncError:
							SetTextColor(eRed);
							strcpy_P(actionStr, kBMP280ErrorStr);
							DrawStr(actionStr);
							MoveTo(2,0);
							SetTextColor(eWhite);
							strcpy_P(actionStr, kPressLeftToSyncStr);
							DrawStr(actionStr);
							break;
						/*case RemoteLogAction::eFullError:
							SetTextColor(eRed);
							strcpy_P(actionStr, kFullErrorStr);
							DrawStr(actionStr);
							MoveTo(2,0);
							SetTextColor(eWhite);
							strcpy_P(actionStr, kPressModeToExitStr);
							DrawStr(actionStr);
							break;*/
					}
				}
			}
			break;
		}
		case RemoteLogAction::eGatewaySyncMode:
			if (inUpdateAll)
			{
				char actionStr[32];
				MoveTo(0);
				SetTextColor(eYellow);
				strcpy_P(actionStr, kWaitSyncingWithBaseStr);
				DrawStr(actionStr);
			}
			break;
	}
	/*
	*	Give some feedback as to the busy state.
	*	Draws a 2 pixel high bar at the bottom of the display representing
	*	the radio activity state + the number of packets queued to be sent.
	*	The queue can hold up to 4 packets.  Any radio activity counts as 1,
	*	so the total is 5.
	*/
	{
		uint8_t	packetsInQueue = mLogAction->PacketsInQueue();
		uint8_t	busy = mLogAction->Busy();
		if (mBusy != busy ||
			mPacketsInQueue != packetsInQueue ||
			inUpdateAll)
		{
			mBusy = busy;
			mPacketsInQueue = packetsInQueue;
			uint16_t	segWidth = mDisplay->GetColumns()/5;
			uint16_t	width = packetsInQueue*segWidth;
			mDisplay->MoveTo(mDisplay->GetRows()-2, 0);
			if (packetsInQueue)
			{
				mDisplay->FillBlock(2, width, eCyan);
			}
			if (packetsInQueue < 4)
			{
				mDisplay->MoveToColumn(width);
				mDisplay->FillBlock(2, segWidth*4, eBlack);
			}
			mDisplay->MoveToColumn(segWidth*4);
			mDisplay->FillBlock(2, segWidth, busy ? eYellow:eBlack);
		}
	}
	
	
}

char* Int16ToDecStr(
	int16_t	inNum,
	char*	inBuffer)
{
	if (inNum == 0)
	{
		*(inBuffer++) = '0';
	} else
	{
		if (inNum < 0)
		{
			*(inBuffer++) = '-';
			inNum = -inNum;
		}
		int16_t num = inNum;
		for (; num/=10; inBuffer++){}
		char*	bufPtr = inBuffer;
		while (inNum)
		{
			*(bufPtr--) = (inNum % 10) + '0';
			inNum /= 10;
		}
		inBuffer++;
	}
	*inBuffer = 0;
	return(inBuffer);
}
