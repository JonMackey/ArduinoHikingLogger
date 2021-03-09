/*
*	RemoteHikeLog.cpp, Copyright Jonathan Mackey 2019
*	Class to log the data associated with a single hike.
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
//#include <Arduino.h>
#include "RemoteHikeLog.h"
#include "UnixTime.h"
#include "LogTempPres.h"
#include <string.h>

/********************************** RemoteHikeLog ***********************************/
RemoteHikeLog::RemoteHikeLog(void)
	: mStartTime(0), mEndTime(0),
	mStartingLocIndex(0), mEndingLocIndex(0), mIsFull(false)
{
	mStartingLoc.loc.name[0] = 0;
	mEndingLoc.loc.name[0] = 0;
}

/******************************** GetLogState *********************************/
uint8_t RemoteHikeLog::GetLogState(void) const
{
	return(mStartTime != 0 ? (mEndTime == 0 ? eRunning : eStopped) :
			(mStartingLocIndex != mEndingLocIndex ? eNotRunning : eCantRun));
}

/******************************* SwapLocIndexes *******************************/
void RemoteHikeLog::SwapLocIndexes(void)
{
	uint16_t	tmp = mStartingLocIndex;
	mStartingLocIndex = mEndingLocIndex;
	mEndingLocIndex = tmp;
	SHikeLocationLink	tmpLocLink;
	memcpy(&tmpLocLink, &mStartingLoc, sizeof(SHikeLocationLink));
	memcpy(&mStartingLoc, &mEndingLoc, sizeof(SHikeLocationLink));
	memcpy(&mEndingLoc, &tmpLocLink, sizeof(SHikeLocationLink));
}

/********************************* ElapsedTime ********************************/
time32_t RemoteHikeLog::ElapsedTime(void) const
{
	// if the log is stopped ? () : if there is an active log ? () : no active log, return 0
	return(mEndTime ? (mEndTime - mStartTime) : (mStartTime ? (UnixTime::Time() - mStartTime) : 0));
}

/************************************ Sync ************************************/
void RemoteHikeLog::Sync(
	time32_t	inStartTime,
	time32_t	inEndTime,
	uint16_t	inStartingLocIndex,
	uint16_t	inEndingLocIndex,
	bool		inLogIsFull)
{
	if (inStartingLocIndex != mStartingLocIndex)
	{
		if (inStartingLocIndex == mEndingLocIndex)
		{
			if (inEndingLocIndex == mStartingLocIndex)
			{
				SwapLocIndexes();
			} else
			{
				memcpy(&mStartingLoc, &mEndingLoc, sizeof(SHikeLocationLink));
			}
		} else
		{
			mStartingLocIndex = inStartingLocIndex;
			mStartingLoc.loc.name[0] = 0;	// Mark this location for update
		}
	}
	if (inEndingLocIndex != mEndingLocIndex)
	{
		mEndingLocIndex = inEndingLocIndex;
		mEndingLoc.loc.name[0] = 0;	// Mark this location for update
	}
	
	/*
	*	If the log just started THEN
	*	update the starting and ending altitudes.
	*/
	if (mStartTime == 0 &&
		inStartTime != 0)
	{
		UpdateStartingAltitude();
	}
	mStartTime = inStartTime;
	mEndTime = inEndTime;
	mIsFull = inLogIsFull;
}

/********************************* UpdateLoc **********************************/
void RemoteHikeLog::UpdateLoc(
	uint16_t					inLocIndex,
	const SHikeLocationLink&	inLocLink)
{
	/*
	*	Because both start and end indexes can be equal, both may be updated.
	*/
	if (mStartingLocIndex == inLocIndex)
	{
		memcpy(&mStartingLoc, &inLocLink, sizeof(SHikeLocationLink));
		LogTempPres::GetInstance().SetStartingAltitude(mStartingLoc.loc.elevation);
	}
	if (mEndingLocIndex == inLocIndex)
	{
		memcpy(&mEndingLoc, &inLocLink, sizeof(SHikeLocationLink));
		LogTempPres::GetInstance().SetEndingAltitude(mEndingLoc.loc.elevation);
	}
}

/************************** UpdateStartingAltitude ****************************/
void RemoteHikeLog::UpdateStartingAltitude(void) const
{
	if (mStartingLoc.loc.name[0] != 0)
	{
		LogTempPres::GetInstance().SetStartingAltitude(mStartingLoc.loc.elevation);
	}
	if (mEndingLoc.loc.name[0] != 0)
	{
		LogTempPres::GetInstance().SetEndingAltitude(mEndingLoc.loc.elevation);
	}
}


