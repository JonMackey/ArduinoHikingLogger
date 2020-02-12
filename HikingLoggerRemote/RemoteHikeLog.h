/*
*	RemoteHikeLog.h, Copyright Jonathan Mackey 2019
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
#ifndef RemoteHikeLog_h
#define RemoteHikeLog_h

#include "HikeLocations.h"
typedef uint32_t time32_t;

class RemoteHikeLog
{
public:
	enum ELogState
	{
		eCantRun,
		eNotRunning,
		eRunning,
		eStopped
	};
							RemoteHikeLog(void);
	bool					Active(void) const
								{return(mStartTime != 0);}
	uint8_t					GetLogState(void) const;
	inline time32_t			StartTime(void) const
								{return(mStartTime);}
	inline time32_t			EndTime(void) const
								{return(mEndTime);}
	time32_t					ElapsedTime(void) const;
	bool					IsFull(void) const
								{return(mIsFull);}
	uint16_t&				GetLocIndex(
								bool					inStart)
								{return(inStart ? mStartingLocIndex : mEndingLocIndex);}
	SHikeLocationLink&		GetLocLink(
								bool					inStart)
								{return(inStart ? mStartingLoc : mEndingLoc);}
	void					SwapLocIndexes(void);
	void					Sync(
								time32_t				inStartTime,
								time32_t				inEndTime,
								uint16_t				inStartingLocIndex,
								uint16_t				inEndingLocIndex,
								bool					inLogIsFull);
	bool					StartingLocNeedsUpdate(void)
								{return(mStartingLoc.loc.name[0] == 0);}
	bool					EndingLocNeedsUpdate(void)
								{return(mEndingLoc.loc.name[0] == 0);}
	void					UpdateLoc(
								uint16_t				inLocIndex,
								const SHikeLocationLink& inLocLink);
	void					UpdateStartingAltitude(void) const;
protected:
	time32_t			mStartTime;
	time32_t			mEndTime;
	uint16_t			mStartingLocIndex;
	uint16_t			mEndingLocIndex;
	SHikeLocationLink	mStartingLoc;
	SHikeLocationLink	mEndingLoc;
	bool				mIsFull;
};

#endif // RemoteHikeLog_h
