/*
*	LogLayout.cpp, Copyright Jonathan Mackey 2019
*	Manages the display layout
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
*
*	The 240x240 display supports 5 lines of regualr 36pt text, and up to 7 if
*	the area reserved for the descent is reduced.  The decent is only needed for
*	glyphs that draw below the baseline.  If no lowercase characters are used,
*	the baseline can be manually adjusted to allow for more lines.
*
*	The FontHeader.height is the only font metric field used in the header by
*	XFont, the ascent and descent are ignored.
*	For 6 lines the height is manually set to 40.
*	For 7 lines the height is manually set to 34.
*/
#include "LogLayout.h"
#include "LogTempPres.h"
#include "LogDateTime.h"
#include "BMP280Utils.h"
#include "HikeLog.h"
#include "LogAction.h"
#include "DisplayController.h"


const char kStartStr[] PROGMEM = "START";
const char kResumeStr[] PROGMEM = "RESUME";
const char kStopStr[] PROGMEM = "STOP";
const char kDoneStr[] PROGMEM = "DONE";
const char kSwapLocsStr[] PROGMEM = "SWAP LOCS";

const char kStartLocStr[] PROGMEM = "START LOC";
const char kEndLocStr[] PROGMEM = "END LOC";
const char kLogStartIsEndErrorStr[] PROGMEM = "START == END!";

const char kSavedHikesStr[] PROGMEM = "SAVED HIKES";
const char kNoneFoundStr[] PROGMEM = "(NONE FOUND)";
const char kGainStr[] PROGMEM = "GAIN ";

const char kBMP280ErrorStr[] PROGMEM = "SYNC BMP ERR";
const char kBMP280PressEnterToSyncStr[] PROGMEM = "[ENTER] 2 SYNC";
const char kBMP280SyncStr[] PROGMEM = "SYNCING BMP";
const char kBMP280SyncSuccessStr[] PROGMEM = "BMP SYNCD";

const LogLayout::SString_PDesc kSyncStateDesc[] PROGMEM =
{
	{kBMP280ErrorStr, XFont::eRed},
	{kBMP280SyncStr, XFont::eYellow},
	{kBMP280SyncSuccessStr, XFont::eGreen}
};

const char kSaveToSDStr[] PROGMEM = "SAVE TO SD";
const char kSaveLocsStr[] PROGMEM = "SAVE LOCS";
const char kUpdateLocsStr[] PROGMEM = "UPDATE LOCS";
const char* kSDActionStr[] = {kSaveToSDStr, kSaveLocsStr, kUpdateLocsStr};

const char kSavingStr[] PROGMEM = "SAVING...";
const char kUpdatingStr[] PROGMEM = "UPDATING...";
const char kEjectSDCardStr[] PROGMEM = "EJECT SD CARD";
const char kSDErrorStr[] PROGMEM = "SD ERROR";
const char kSavedStr[] PROGMEM = "SAVED";
const char kUpdatedStr[] PROGMEM = "UPDATED";

const LogLayout::SString_PDesc kSDCardStateDesc[] PROGMEM =
{
	{kSavingStr, XFont::eYellow},		// eSavingToSD
	{kUpdatingStr, XFont::eYellow},		// eUpdatingFromSD
	{kEjectSDCardStr, XFont::eRed},		// eEjectSDCardNoReset
	{kSDErrorStr, XFont::eRed},			// eSDError
	{kSavedStr, XFont::eGreen},			// eSDSavedSuccess
	{kUpdatedStr, XFont::eGreen},		// eSDUpdateSuccess
	{kEjectSDCardStr, XFont::eGreen}	// eEjectSDCardAllowReset
};

const char kResetStr[] PROGMEM = "RESET LOG";
const char kResetVerifyYesStr[] PROGMEM = "(YES)";
const char kResetVerifyNoStr[] PROGMEM = "(NO)";
const char kResetSuccessStr[] PROGMEM = "RESET DONE";
const char kResetErrorStr[] PROGMEM = "RESET FAILED";

const LogLayout::SString_PDesc kResetLogStateDesc[] PROGMEM =
{
	{kResetVerifyYesStr, XFont::eGreen},
	{kResetVerifyNoStr, XFont::eRed},
	{kResetSuccessStr, XFont::eGreen},
	{kResetErrorStr, XFont::eRed}
};

/********************************* LogLayout **********************************/
LogLayout::LogLayout(void)
{
}

/********************************* Initialize *********************************/
void LogLayout::Initialize(
	LogAction*			inLogAction,
	HikeLog*			inHikeLog,
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
void LogLayout::DrawLocation(
	uint16_t	inLocIndex,
	uint8_t		inFirstLine)
{
	HikeLocations::GetInstance().GoToLocation(inLocIndex);
	const SHikeLocationLink&	link = HikeLocations::GetInstance().GetCurrent();
	MoveTo(inFirstLine);
	SetTextColor(eOrange);
	DrawStr(link.loc.name, true);
	char elevationStr[32];
	int32_t	elevation = link.loc.elevation;
	strcpy(&elevationStr[BMP280Utils::Int32ToIntStr(elevation*100, elevationStr)], LogTempPres::GetInstance().GetAltitudeSuffixStr());
	MoveTo(inFirstLine+1);
	SetTextColor(0xFBC0);
	DrawStr(elevationStr, true);
}

/********************************** DrawTime **********************************/
void LogLayout::DrawTime(
	time32_t	inTime,
	bool		inShowingAMPM)
{
	char timeStr[32];
	bool isPM = LogDateTime::CreateTimeStr(inTime, timeStr);
	DrawStr(timeStr);
	if (inShowingAMPM)
	{
		SetFont(mSmallFont);
		DrawStr(isPM ? " PM":" AM");
		SetFont(mNormalFont);
	}
}


/***************************** DrawIndexedDescStr *****************************/
void LogLayout::DrawIndexedDescStr(
	const SString_PDesc*	inStringList,
	uint8_t					inStrIndex,
	bool					inHasOptions,
	bool					inCentered)
{
	SString_PDesc	stringDesc;
	memcpy_P(&stringDesc, &inStringList[inStrIndex], sizeof(SString_PDesc));
	DrawTextOption(stringDesc.descStr, stringDesc.color, inHasOptions, inCentered);
}

/******************************* DrawTextOption *******************************/
void LogLayout::DrawTextOption(
	const char*	inStrRef,
	uint16_t	inColor,
	bool		inHasOptions,
	bool		inCentered)
{
	char descStr[32];
	if (inHasOptions)
	{
		SetTextColor(eWhite);
		DrawStr("<");
		DrawRightJustified(">");
	}
	
	SetTextColor(inColor);
	strcpy_P(descStr, inStrRef);
	if (inCentered)
	{
		DrawCentered(descStr);
	} else
	{
		DrawStr(descStr);
	}
}

/********************************* ClearLines *********************************/
void LogLayout::ClearLines(
	uint8_t	inStartLine,
	uint8_t	inNumLines)
{
	MoveTo(inStartLine,0);
	mDisplay->FillBlock(43*inNumLines, 240, eBlack);
}

/*********************************** Update ***********************************/
/*
*	The display is 240 x 240.  This allows for 5 lines of text at 43px per line.
*	What is drawn is determined by the mode as defined in the LogAction class.
*/
void LogLayout::Update(
	bool	inUpdateAll)
{
	bool	timeChanged = false;
	float	altitude = 0;
	
	/*
	*	For all modes except for review mode, draw the time, elevation, and
	*	temperature on the last two text lines relative to the bottom of the display.
	*/
	uint8_t	mode = mLogAction->Mode();
	// Update all if going from eReviewHikesMode to any other mode
	inUpdateAll = inUpdateAll || (mode != mPrevMode && mPrevMode == LogAction::eReviewHikesMode);
	
	if (mode != LogAction::eReviewHikesMode)
	{
		if (inUpdateAll)
		{
			if (mPrevMode == LogAction::eReviewHikesMode)
			{
				MoveTo(3);
				mDisplay->FillBlock(25, 240, eBlack);
			}
			/*
			*	Draw a 2 pixel horizontal line halfway between line 3 relative to
			*	the top and line 4 relative to the bottom.  This is a 25 pixel gap,
			*	so 13 will place the line in the center.
			*/
			mDisplay->MoveTo(240-(43*2)-13, 0);
			mDisplay->FillBlock(2, 240, eGray);
			// Move to line 4 relative to the display bottom.
			mDisplay->MoveTo(240-(43*2), 0);
			mDisplay->FillBlock(43*2, 240, eBlack);
		}
	
		if (inUpdateAll ||
			LogDateTime::TimeChanged())
		{
			timeChanged = true;
			LogDateTime::ResetTimeChanged();
			char timeStr[32];
			bool isPM = LogDateTime::CreateTimeStr(timeStr);
			mDisplay->MoveTo(240-(43*2), 45);	// Line 4 relative to the display bottom
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
					// The width of an 18pt A - the width of a P = 1
					mDisplay->FillBlock(mFontRows, 1, eBlack);
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
			mDisplay->MoveTo(240-43, 0);	// Line 5 relative to the display bottom
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
				EraseTillColumn(86);	// Erase any artifacts when moving to a
										// shorter string.
			}
		}
		/*
		*	If everything is being updated OR the pressure has changed AND
		*	the pressure reading is valid AND
		*	the starting altitude has been set THEN
		*	draw the current altitude
		*/
		if ((inUpdateAll ||
			LogTempPres::GetInstance().PressureChanged()) &&
			LogTempPres::GetInstance().IsValid() &&
			LogTempPres::GetInstance().StartingAltitude())
		{
			char altitudeStr[32];
			altitude = LogTempPres::GetInstance().CalcCurrentAltitude();
			LogTempPres::GetInstance().CreateAltitudeStr(altitude, altitudeStr);
			mDisplay->MoveTo(240-43, 0);	// Line 5 relative to the display bottom
			SetTextColor(eYellow);
			DrawRightJustified(altitudeStr);
		}
	}
	inUpdateAll = inUpdateAll || mode != mPrevMode;
	mPrevMode = mode;

	switch (mode)
	{
		/*
		*	When there are two options the state modifier determines the
		*	option displayed.
		*
		*	Line 1:
		*	if not running the options are "SWAP" or "START"
		*	if running the option is "STOP"
		*	if paused/stopped the options are "DONE" or "RESUME"
		*	If starting == ending loc, "START == END!" is displayed.
		*	Line 2:
		*	If log is running the elapsed time and percentage remaining.
		*/
		case LogAction::eLogMode:
		{
			uint8_t	logState = mHikeLog->GetLogState() + mLogAction->LogStateModifier();
			if (inUpdateAll ||
				logState != mLogState)
			{
				if (inUpdateAll ||
					!mHikeLog->Active() ||
					(logState != mLogState && mLogState <= HikeLog::eNotRunning))
				{
					ClearLines();
				} else
				{
					ClearLines(0, 2);
				}
				mLogState = logState;
				MoveTo(0);
				{
					switch(logState)
					{
						case HikeLog::eStopped + HikeLog::eModifier:
							DrawTextOption(kResumeStr, eGreen, true, true);
							break;
						case HikeLog::eNotRunning + HikeLog::eModifier:
							mLocIndex = 0;	// Start and Swap display the start and end location names
							DrawTextOption(kStartStr, eGreen, true, true);
							break;
						case HikeLog::eRunning:
						case HikeLog::eRunning + HikeLog::eModifier:
							DrawTextOption(kStopStr, eRed, false, true);
							break;
						case HikeLog::eStopped:
							DrawTextOption(kDoneStr, eWhite, true, true);
							break;
						case HikeLog::eNotRunning:
							mLocIndex = 0;	// Start and Swap display the start and end location names
							DrawTextOption(kSwapLocsStr, eWhite, true, true);
							break;
						case HikeLog::eCantRun:
						case HikeLog::eCantRun + HikeLog::eModifier:
							DrawTextOption(kLogStartIsEndErrorStr, eYellow, false, true);
							break;
					}
					break;
				}

			}
			if (mHikeLog->Active())
			{
				if (inUpdateAll || timeChanged)
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
						MoveTo(2);
						char timeStr[32];
						LogDateTime::CreateTimeStr(elapsedTime, timeStr);
						SetTextColor(eYellow);
						DrawStr(timeStr);
					}
				}
				if (altitude != 0)
				{
					char altitudeStr[32];
					MoveTo(2);
					LogTempPres::GetInstance().CreateAltitudePercentageStr(altitude, altitudeStr);
					uint16_t	textWidth = DrawRightJustified(altitudeStr);
					mDisplay->MoveToColumn(122);
					EraseTillColumn(240 - textWidth);
				}
			} else
			{
				uint16_t	locIndex = mHikeLog->StartingLocIndex();
				if (inUpdateAll ||
					locIndex != mLocIndex)
				{
					mLocIndex = locIndex;
					HikeLocations::GetInstance().GoToLocation(locIndex);
					MoveTo(1);
					SetTextColor(eGreen);
					DrawStr(HikeLocations::GetInstance().GetCurrent().loc.name, true);
					HikeLocations::GetInstance().GoToLocation(mHikeLog->EndingLocIndex());
					MoveTo(2);
					SetTextColor(eRed);
					DrawStr(HikeLocations::GetInstance().GetCurrent().loc.name, true);
				}
			}
			break;
		}
		case LogAction::eStartLocSelMode:
		case LogAction::eEndLocSelMode:
		{
			bool	isStart = mode == LogAction::eStartLocSelMode;
			if (inUpdateAll)
			{
				ClearLines();
				MoveTo(0);
				DrawTextOption(isStart ? kStartLocStr : kEndLocStr, eWhite, true, true);
			}
			
			uint16_t	locIndex = mLogAction->LocIndex();
			if (inUpdateAll ||
				locIndex != mLocIndex)
			{
				mLocIndex = locIndex;
				DrawLocation(locIndex);
			}
			break;
		}
		case LogAction::eBMP280SyncMode:
		{
			uint8_t	syncState = mLogAction->SyncState();
			
			
			if (inUpdateAll ||
				syncState != mSyncState)
			{
				ClearLines();
				MoveTo(0);
				mSyncState = syncState;
				DrawIndexedDescStr(kSyncStateDesc, syncState, false, true);
				MoveTo(2);
				if (syncState == LogAction::eBMP280SyncError)
				{
					DrawTextOption(kBMP280PressEnterToSyncStr, eWhite, false, true);
				}
			}
			break;
		}
		case LogAction::eResetLogMode:
		{
			uint8_t	resetLogState = mLogAction->ResetLogState();
			
			
			if (inUpdateAll ||
				resetLogState != mResetLogState)
			{
				if (inUpdateAll || resetLogState >= LogAction::eResetSuccess)
				{
					ClearLines();
					MoveTo(0);
					DrawTextOption(kResetStr, eWhite, resetLogState <= LogAction::eResetVerifyNo, true);
				} else
				{
					ClearLines(1,1);
				}
				mResetLogState = resetLogState;
				MoveTo(1);
				DrawIndexedDescStr(kResetLogStateDesc, resetLogState, false, true);
			}
			break;
		}
		case LogAction::eSDCardMode:
		{
			uint8_t	sdCardState = mLogAction->SDCardState();
			uint8_t	sdCardAction = mLogAction->SDCardAction();
			
			if (inUpdateAll ||
				sdCardState != mSDCardState ||
				sdCardAction != mSDCardAction)
			{
				mSDCardState = sdCardState;
				mSDCardAction = sdCardAction;
				ClearLines();
				if (sdCardState == LogAction::eSDCardIdle)
				{
					MoveTo(0);
					DrawTextOption(kSDActionStr[sdCardAction], eCyan, true, true);
				} else
				{
					MoveTo(1);
					DrawIndexedDescStr(kSDCardStateDesc, sdCardState, false, true);
				}
			}
			break;
		}
		case LogAction::eReviewHikesMode:
		{
			uint16_t	hikeRef = mLogAction->HikeRef();
			uint8_t reviewState = mLogAction->ReviewState();
			SHikeSummary	hikeSummary;
			bool	hasSavedHikes = mHikeLog->GetSavedHike(hikeRef, hikeSummary);
			inUpdateAll = inUpdateAll || hikeRef != mHikeRef;
			
			if (inUpdateAll)
			{
				mDisplay->Fill();
				mHikeRef = hikeRef;
				if (hasSavedHikes)
				{
					char tempStr[32];
					LogDateTime::CreateDateStr(hikeSummary.startTime, tempStr);
					MoveTo(0);
					SetTextColor(eWhite);
					DrawStr("<");
					DrawRightJustified(">");
					SetTextColor(eCyan);
					DrawCentered(tempStr);
				} else
				{
					MoveTo(0);
					DrawTextOption(kSavedHikesStr, eWhite, false, true);
					MoveTo(2);
					DrawTextOption(kNoneFoundStr, eYellow, false, true);
				}
			} else if (mReviewState != reviewState)
			{
				MoveTo(1);
				mDisplay->FillBlock(240-43, 240, eBlack);
				inUpdateAll = true;
			}
			mReviewState = reviewState;
			if (inUpdateAll &&
				hasSavedHikes)
			{
				char tempStr[32];
				if (reviewState == LogAction::eReviewLocs)
				{
					DrawLocation(hikeSummary.startingLocIndex, 1);
					/*
					*	The Temperature is drawn on the right.
					*/

					uint8_t strLen = LogTempPres::GetInstance().CreateTempStr(hikeSummary.startTemp, tempStr) + 2;
					strcpy(&tempStr[strLen], LogTempPres::GetInstance().GetTempSuffixStr());
					SetTextColor(eMagenta);
					DrawRightJustified(tempStr);
					DrawLocation(hikeSummary.endingLocIndex, 3);
					strLen = LogTempPres::GetInstance().CreateTempStr(hikeSummary.endTemp, tempStr) + 2;
					strcpy(&tempStr[strLen], LogTempPres::GetInstance().GetTempSuffixStr());
					SetTextColor(eMagenta);
					DrawRightJustified(tempStr);
				/*
				*	Else draw the elevation gain, and the start, end, and
				*	elapsed time.  The day of week is placed on the bottom line.
				*/
				} else
				{
					HikeLocations::GetInstance().GoToLocation(hikeSummary.endingLocIndex);
					int32_t	elevation = HikeLocations::GetInstance().GetCurrent().loc.elevation;
					HikeLocations::GetInstance().GoToLocation(hikeSummary.startingLocIndex);
					elevation -= HikeLocations::GetInstance().GetCurrent().loc.elevation;
					strcpy(&tempStr[BMP280Utils::Int32ToIntStr(elevation*100, tempStr)], LogTempPres::GetInstance().GetAltitudeSuffixStr());
					MoveTo(1);
					DrawTextOption(kGainStr, eWhite, false, false);
					SetTextColor(0xFBC0);
					DrawStr(tempStr);
					MoveTo(2);
					SetTextColor(eGreen);
					DrawTime(hikeSummary.startTime, true);
					LogDateTime::CreateDayOfWeekStr(hikeSummary.startTime, tempStr);
					DrawRightJustified(tempStr);
					MoveTo(3);
					SetTextColor(eRed);
					DrawTime(hikeSummary.endTime, true);
					LogDateTime::CreateDayOfWeekStr(hikeSummary.endTime, tempStr);
					DrawRightJustified(tempStr);
					MoveTo(4);
					SetTextColor(eYellow);
					DrawTime(hikeSummary.endTime - hikeSummary.startTime, false);
				}
			}
			break;
		}
	}
}
