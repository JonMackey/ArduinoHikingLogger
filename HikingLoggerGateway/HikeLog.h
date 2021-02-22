/*
*	HikeLog.h, Copyright Jonathan Mackey 2019
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
#ifndef HikeLog_h
#define HikeLog_h

#include <inttypes.h>
#include "HikeLocations.h"
typedef uint32_t time32_t;

class DataStream;

/*
*
*	On the very first boot when the MCU's EEPROM is clear, or after the user
*	chooses to reset/clear the log data stream, the log data stream contains
*	two 32 bit nulls.  These nulls serve as the end of log marker.
*
*	When a hike log starts, a log header is written followed by multiple log
*	entries.  The last log entry always is followed by a 32 bit null to mark the
*	end of the log in case the data logger is interrupted before the log is
*	marked as done (such as a brownout or manual reset.)
*
*	After a normal boot, the log data stream is scanned for the end of log
*	marker.  All subsequent hike logs are appended from this point.
*
*	header:
*		non-zero startTime
*		zero endTime if not done (running or stopped).  non-zero if done
*		....
*	log entries:
*		non-zero pressure
*		....
*		zero pressure marks end of log.  When active the data stream points to
*		this entry
*		
*/
struct SHikeLogHeader
{
	time32_t		startTime;	// Unix time/date
	time32_t		endTime;
	time32_t		interval;
	SHikeLocation	start;
	SHikeLocation	end;
};

#pragma pack(push,1)
struct SHikeLogEntry
{
							SHikeLogEntry(void){}
							SHikeLogEntry(
								uint32_t				inPressure,
								int16_t					inTemperature)
								: pressure(inPressure), temperature(inTemperature){}
	uint32_t	pressure;
	int16_t		temperature;
};

struct SHikeLogLastEntry : SHikeLogEntry
{
							SHikeLogLastEntry(
								uint32_t				inPressure,
								int16_t					inTemperature)
								: SHikeLogEntry(inPressure, inTemperature), lastEntry(){}
	uint32_t	lastEntry[2];	//  value = 0 (0 pressure is invalid)
};
#pragma pack(pop)

struct SRingHeader 
{
	uint16_t	head;
	uint16_t	tail;
};

struct SHikeSummary
{
	uint16_t	startingLocIndex;
	uint16_t	endingLocIndex;
	time32_t	startTime;
	time32_t	endTime;
	int16_t		startTemp;		// degrees C (lowest after 10 minutes)
	int16_t		endTemp;		// degrees C
};

class HikeLog
{
public:
	enum ELogState
	{
		eCantRun,	// 000
		eNotRunning,// 001
		eRunning,	// 010
		eStopped,	// 011
		eModifier	// 100	Used by UI to differentiate UI states
	};
							HikeLog(void);
	bool					Initialize(
								DataStream*				inLogData,
								uint8_t					inSDSelectPin);
	bool					InitializeLog(void);
	bool					StartLog(
								time32_t					inStartTime = 0);
	bool					Active(void) const
								{return(mHike.startTime != 0);}
	uint8_t					GetLogState(void) const;
	inline time32_t			StartTime(void) const
								{return(mHike.startTime);}
	bool					EndLog(void);
	bool					IsFull(void) const;
	time32_t					SecondsTillFull(void) const;
	bool					SaveLogToSD(void);
	bool					SaveLogSummariesToSD(void);
	bool					LoadLogSummariesFromSD(void);
	void					StopLog(
								time32_t					inEndTime = 0);							
	inline time32_t			EndTime(void) const
								{return(mHike.endTime);}
	time32_t					ElapsedTime(void) const;
	bool					LogEntry(void);
	bool					LogEntryIfTime(void);
	uint16_t&				GetLocIndex(
								bool					inStart)
								{return(inStart ? mHike.startingLocIndex : mHike.endingLocIndex);}
	uint16_t&				StartingLocIndex(void)
								{return(mHike.startingLocIndex);}
	uint16_t&				EndingLocIndex(void)
								{return(mHike.endingLocIndex);}
	void					SwapLocIndexes(void);
	void					SaveLocIndexes(void) const;
	void					UpdateStartingAltitude(void) const;
	uint16_t				GetSavedHikesLastRef(void) const;
	bool					GetSavedHike(
								uint16_t				inRef,
								SHikeSummary&			outHikeSummary);
	uint16_t				GetNextSavedHikeRef(
								uint16_t				inRef);
	uint16_t				GetPrevSavedHikeRef(
								uint16_t				inRef);
	static char*			UInt32ToHexStr(
								uint32_t				inNum,
								char*					inBuffer);
protected:
	DataStream*			mLogData;
	time32_t			mNextLogTime;
	SHikeSummary		mHike;
	uint32_t			mStartDataPos;
	uint32_t			mFullDataPos;
	uint8_t				mSDSelectPin;
	static time32_t		sFileCreationTime;
	
							/*
							*	Used by SaveLogToSD to set the file creation
							*	date and time.
							*/
	static void				SDFatDateTimeCB(
								uint16_t*				outDate,
								uint16_t*				outTime);
};

#endif // HikeLog_h
