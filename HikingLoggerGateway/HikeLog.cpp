/*
*	HikeLog.cpp, Copyright Jonathan Mackey 2019
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
#include <EEPROM.h>
#include <string.h>
#include "HikeLog.h"
#include "LogDateTime.h"
#include "LogTempPres.h"
#include "DataStream.h"
#include "SdFat.h"

const uint32_t kLogInterval = 4;	// Seconds
const uint8_t kNumEntriesPerPass = 10;
const char kFileExtStr[] PROGMEM = ".log";
const char kSummariesFilenameStr[] PROGMEM = "HikeSum.bin";
const uint32_t	kLogFileMarker = 0x484C4F47;	// HLOG

const uint8_t kStartingLocEEAddr = 4;	// uint16_t
const uint8_t kEndingLocEEAddr = 6;	// uint16_t
const uint8_t kLogInitializedEEAddr = 8;
const uint16_t kMaxHikeSummaries = 125;	// sizeof(SHikeSummary) * kMaxHikeSummaries = 2000
/*
*	EEPROM usage, 2K bytes (assumes ATmega644PA, E2END = 0x7FF)
*
*	[0]		uint8_t		networkID;
*	[1]		uint8_t		nodeID;
*	[2]		uint8_t		unassigned[2];
*	[4]		uint16_t	lastStartingLocIndex;
*	[6]		uint16_t	lastEndingLocIndex;
*	[8]		uint8_t		logInitialized;
*	[9]		uint8_t		unassigned2[23];
*	Storage of the last 100 hikes, circular storage, 16 byte struct
*	[32]	uint16_t	lastHikesHead
*	[34]	uint16_t	lastHikesTail
*	[36]	uint8_t		unassigned3[2];
*	[38]	SHikeSummary hikes[kMaxHikeSummaries];  size = 16 bytes per hike
*	[2038]	uint8_t		unassigned4[10];
*/
const uint8_t kLogRingAddressesEEAddr = 32;
const uint8_t kLogRingStorageEEAddr = 38;

time32_t HikeLog::sFileCreationTime;


/********************************** HikeLog ***********************************/
HikeLog::HikeLog(void)
{
}

/********************************* Initialize *********************************/
bool HikeLog::Initialize(
	DataStream*		inLogData,
	uint8_t			inSDSelectPin)
{
	mLogData = inLogData;
	mSDSelectPin = inSDSelectPin;
	bool	success = true;
	mHike.startTime = 0;
	
	/*
	*	Full is the position of the end of the stream minus the size of the
	*	last log entry.  Used by IsFull().
	*/
	mLogData->Seek(-(int32_t)(sizeof(SHikeLogHeader) + sizeof(SHikeLogLastEntry)), DataStream::eSeekEnd);
	mFullDataPos = mLogData->GetPos();

	/*
	*	If the log data stream has never been initialized THEN
	*	do it now.
	*/
	if (EEPROM.read(kLogInitializedEEAddr) == 0xFF)
	{
		mHike.startingLocIndex = 1;
		mHike.endingLocIndex = 1;
		SaveLocIndexes();
		success = InitializeLog();
		if (success)
		{
			EEPROM.write(kLogInitializedEEAddr, 0);	// Mark the log as having been initialzed.
		}
	/*
	*	Else, point the log data stream to the current end of the log by
	*	searching for the end-of-log record.
	*/
	} else
	{
		/*
		*	Initialize the starting and ending location indexes from EEPROM
		*/
		EEPROM.get(kStartingLocEEAddr, mHike.startingLocIndex);
		/*
		*	If the starting loc index is 0 or 0xFFFF THEN
		*	set it to something valid.
		*/
		if ((mHike.startingLocIndex + 1) < 2)
		{
			mHike.startingLocIndex = 1;
		}
		EEPROM.get(kEndingLocEEAddr, mHike.endingLocIndex);
		/*
		*	If the ending loc index is 0 or 0xFFFF THEN
		*	set it to something valid.
		*/
		if ((mHike.endingLocIndex + 1) < 2)
		{
			mHike.endingLocIndex = 1;
		}
		mLogData->Seek(0, DataStream::eSeekSet);
		SHikeLogEntry	entry[kNumEntriesPerPass];
		SHikeLogHeader	header;
		while (success)
		{
			// Read the log header
			uint32_t	startPos = mLogData->GetPos();
			success = mLogData->Read(sizeof(SHikeLogHeader), &header) == sizeof(SHikeLogHeader);
			/*
			*	If the start time is not zero THEN
			*	this is a valid log
			*/
			if (header.startTime)
			{
				while (success)
				{
					// Read up to kNumEntriesPerPass entries at at time
					uint32_t endPos = mLogData->GetPos();
					uint8_t	entriesRead = mLogData->Read(kNumEntriesPerPass*sizeof(SHikeLogEntry), entry)/sizeof(SHikeLogEntry);
					success = entriesRead > 0;
					if (success)
					{
						uint8_t i = 0;
						bool	done = false;
						for (; i < entriesRead; i++)
						{
							/*
							*	If this isn't the end of the log THEN
							*	continue
							*/
							if (entry[i].pressure)
							{
								continue;
							}
							done = true;
							/*
							*	If not all of the entries were inspected THEN
							*	rewind the data stream to point to the header of
							*	the next log (or null if this is the last log.)
							*/
							if (i != entriesRead)
							{
								endPos += ((i * sizeof(SHikeLogEntry)) + sizeof(uint32_t));
								mLogData->Seek(endPos, DataStream::eSeekSet);
							}
							break;
						}
						if (!done)
						{
							continue;
						}
						break;
					}
				}
			/*
			*	else the end of all logs has been found.
			*	Rewind to the start of this log
			*/
			} else
			{
				mLogData->Seek(startPos, DataStream::eSeekSet);
				break;
			}
		}
	}
	return(success);
}

/******************************* InitializeLog ********************************/
bool HikeLog::InitializeLog(void)
{
	
	mLogData->Seek(0, DataStream::eSeekSet);
	uint32_t	logEnd[2] = {0};
	bool	success = mLogData->Write(sizeof(logEnd), logEnd) == sizeof(logEnd);
	mLogData->Seek(0, DataStream::eSeekSet);
	return(success);
}

/******************************** GetLogState *********************************/
uint8_t HikeLog::GetLogState(void) const
{
	return(mHike.startTime != 0 ? (mHike.endTime == 0 ? eRunning : eStopped) :
			(mHike.startingLocIndex != mHike.endingLocIndex ? eNotRunning : eCantRun));
}

/******************************* SwapLocIndexes *******************************/
void HikeLog::SwapLocIndexes(void)
{
	uint16_t	tmp = mHike.startingLocIndex;
	mHike.startingLocIndex = mHike.endingLocIndex;
	mHike.endingLocIndex = tmp;
	SaveLocIndexes();
}

/******************************* SaveLocIndexes *******************************/
void HikeLog::SaveLocIndexes(void) const
{
	uint16_t	locIndex;
	EEPROM.get(kStartingLocEEAddr, locIndex);
	if (locIndex != mHike.startingLocIndex)
	{
		EEPROM.put(kStartingLocEEAddr, mHike.startingLocIndex);
	}
	EEPROM.get(kEndingLocEEAddr, locIndex);
	if (locIndex != mHike.endingLocIndex)
	{
		EEPROM.put(kEndingLocEEAddr, mHike.endingLocIndex);
	}
}

/****************************** SecondsTillFull *******************************/
/*
*	Returns the number of seconds of stream capacity remaining till full.
*/
time32_t HikeLog::SecondsTillFull(void) const
{
	// Passing Clip() a value larger than the stream capacity will return
	// the space remaining in the stream.  This value is divided by the size
	// required to store 1 log entry.  It's then multiplied by the log interval.
	return((mLogData->Clip(0xFFFFFF)/sizeof(SHikeLogEntry)) * kLogInterval);
}

/*********************************** IsFull ***********************************/
/*
*	Returns true if the current active hike log doesn't have the space to
*	add more entries.  When this happens the last entry is reused till the
*	hike ends.
*/
bool HikeLog::IsFull(void) const
{
	uint32_t	pos = mLogData->GetPos();
	if (mHike.startTime == 0)
	{
		pos += (sizeof(SHikeLogHeader) + sizeof(SHikeLogEntry));
	}
	return(pos >= mFullDataPos);
}

/********************************** StartLog **********************************/
bool HikeLog::StartLog(
	time32_t	inStartTime)
{
	bool	success = LogTempPres::GetInstance().IsValid();
	if (success)
	{
		/*
		*	If the log is not running THEN
		*	start the log.
		*/
		if (!mHike.startTime)
		{
			SHikeLogHeader logHeader;
			logHeader.startTime = mHike.startTime = inStartTime == 0 ? LogDateTime::Time() : inStartTime;
			logHeader.endTime = mHike.endTime = 0;
			logHeader.interval = kLogInterval;
			UpdateStartingAltitude();	// Sets the location, then sets the starting altitude which updates the sea level pressure
			memcpy(&logHeader.start, &HikeLocations::GetInstance().GetCurrent().loc, sizeof(SHikeLocation));
			HikeLocations::GetInstance().GoToLocation(mHike.endingLocIndex);
			memcpy(&logHeader.end, &HikeLocations::GetInstance().GetCurrent().loc, sizeof(SHikeLocation));
			LogTempPres::GetInstance().SetEndingAltitude(HikeLocations::GetInstance().GetCurrent().loc.elevation);
			mStartDataPos = mLogData->GetPos();
			success = mLogData->Write(sizeof(SHikeLogHeader), &logHeader) == sizeof(SHikeLogHeader) &&
					LogEntry();	// Write the first entry and mark the end of the log
			// Calling LogEntry() initializes mNextLogTime
			SaveLocIndexes();
			mHike.startTemp = mHike.endTemp = (int16_t)LogTempPres::GetInstance().PeekTemperature();
			/*
			*	Setup milestone notification after each quarter (25%, 50%, 75%)
			*/
			LogTempPres::GetInstance().ResetMilestone(25);
		/*
		*	Else if the log was paused THEN
		*	continue from where it left off.
		*/
		} else if (mHike.endTime)
		{
			mHike.startTime = LogDateTime::Time() - (mHike.endTime - mHike.startTime);
			mHike.endTime = 0;
			success = LogEntry();
		}
		// Else the timer is already running.
	}
	return(success);
}

/************************** UpdateStartingAltitude ****************************/
void HikeLog::UpdateStartingAltitude(void) const
{
	HikeLocations::GetInstance().GoToLocation(mHike.startingLocIndex);
	LogTempPres::GetInstance().SetStartingAltitude(HikeLocations::GetInstance().GetCurrent().loc.elevation);
}

/********************************** LogEntry **********************************/
bool HikeLog::LogEntry(void)
{
	/*
	*	A log entry is always written with two additional 32 bit nulls appended.
	*	This is done in case the MCU is reset or looses power while there's an
	*	active log.  On restart the initialization routine can locate the end of
	*	the log and set the stream pointer for the start of the next log.
	*	<header><entry<null last entry><null header>
	*	After calling LogEntry: <header><entry><entry><null last entry><null header>
	*/
	SHikeLogLastEntry	logEntry(LogTempPres::GetInstance().PeekPressure(),
								(int16_t)LogTempPres::GetInstance().PeekTemperature());
								
	/*
	*	When ascending, to account for the temperature swing from the pack being
	*	stored in a warm vehicle, take the lowest temperature in the first 23
	*	minutes as the starting temperature.
	*/
	if (LogTempPres::GetInstance().Ascending() &&
		(LogDateTime::Time() - mHike.startTime) < (60 * 23) &&
		mHike.startTemp > logEntry.temperature)
	{
		mHike.startTemp = logEntry.temperature;
	}
		 
	/*
	*	If the log is full THEN
	*	reuse the last entry until the log is marked as done.
	*/
	if (IsFull())
	{
		mLogData->Seek(-(int32_t)(sizeof(SHikeLogEntry)), DataStream::eSeekCur);
	}
	
	bool	success = mLogData->Write(sizeof(SHikeLogLastEntry), &logEntry) == sizeof(SHikeLogLastEntry);
	if (success)
	{
		mLogData->Seek(-(int32_t)(2*sizeof(uint32_t)), DataStream::eSeekCur);
		mNextLogTime = LogDateTime::Time()+kLogInterval;
	}
	return(success);
}

/******************************* LogEntryIfTime *******************************/
bool HikeLog::LogEntryIfTime(void)
{
	bool	success = true;
		if (LogTempPres::GetInstance().IsValid())

	/*
	*	If the BMP280 is active AND
	*	there is an active log AND
	*	the log is not stopped AND
	*	it's time to log an entry THEN
	*	log an entry.
	*/
	if (LogTempPres::GetInstance().IsValid() &&
		mHike.startTime &&
		mHike.endTime == 0 &&
		mNextLogTime <= LogDateTime::Time())
	{
		success = LogEntry();
	}
	return(success);
}

/*********************************** EndLog ***********************************/
bool HikeLog::EndLog(void)
{
	uint32_t	savedPos = mLogData->GetPos();
	mLogData->Seek(mStartDataPos, DataStream::eSeekSet);

	/*
	*	Write the hike summary to EEPROM if all milestones have been reached.
	*	Anything without all milestones is probably an accidental start.
	*/
	if (LogTempPres::GetInstance().PassedAllMilestones())
	{
		SRingHeader	header;
		EEPROM.get(kLogRingAddressesEEAddr, header);
		header.tail = (header.tail + sizeof(SHikeSummary)) % (sizeof(SHikeSummary)*kMaxHikeSummaries);
		if (header.tail == header.head)
		{
			header.head = (header.head + sizeof(SHikeSummary)) % (sizeof(SHikeSummary)*kMaxHikeSummaries);
		}
		EEPROM.put(kLogRingAddressesEEAddr, header);
		EEPROM.put(header.tail + kLogRingStorageEEAddr, mHike);
	}

	/*
	*	Write the start and end time from the hike summary
	*/
	bool	success = mLogData->Write(8, &mHike.startTime) == 8;
	if (success)
	{
		/*
		*	Whenever a log entry is written there is always an end marker
		*	appended consisting of two 32 bit null values.
		*	<header><entry><entry>...<null><null>
		*
		*	After writing an entry the stream pointer points to the first of
		*	the two end marker nulls so that if another log entry is written it
		*	overwrites the nulls.  To mark the log as ended the stream pointer
		*	should point to the second end marker null.  The second null
		*	represents an empty log header.  This second null gets overwritten
		*	when a new log starts:
		*	<header><entry><entry>...<null><header><entry><null><null>
		*/
		mLogData->Seek(savedPos+sizeof(uint32_t), DataStream::eSeekSet);
		mHike.startTime = 0;	// No active log
		mHike.endTime = 0;	// Log not stopped (when active)
	}
	LogTempPres::GetInstance().ResetMilestone(100);	// Disable milestone notifications
	return(success);
}

/******************************* UInt32ToHexStr *******************************/
/*
*	Returns the pointer to the char after the last char (where you would place
*	the nul terminator)
*/
char* HikeLog::UInt32ToHexStr(
	uint32_t	inNum,
	char*		inBuffer)
{
	static const char kHexChars[] = "0123456789ABCDEF";
	uint8_t i = 8;
	while (i)
	{
		i--;
		inBuffer[i] = kHexChars[inNum & 0xF];
		inNum >>= 4;
	}
	inBuffer[8] = 0;
	return(&inBuffer[8]);
}

/******************************** SaveLogToSD *********************************/
bool HikeLog::SaveLogToSD(void)
{
	SdFat sd;
	bool	success = sd.begin(mSDSelectPin);
	if (success)
	{
		uint32_t	savedPos = mLogData->GetPos();
		mLogData->Seek(0, DataStream::eSeekSet);
		SHikeLogEntry	entry[kNumEntriesPerPass];
		SHikeLogHeader	header;
		sFileCreationTime = LogDateTime::Time();
		SdFile::dateTimeCallback(SDFatDateTimeCB);
		while (success)
		{
			// Read the log header
			uint32_t	startPos = mLogData->GetPos();
			success = mLogData->Read(sizeof(SHikeLogHeader), &header) == sizeof(SHikeLogHeader);
			/*
			*	If the start time is not zero THEN
			*	this is a valid log
			*/
			if (header.startTime)
			{
				sFileCreationTime = header.startTime;
				/*
				*	Create a file to hold this log.
				*	The filename is based on the date and time.
				*	<32 bit unix date time converted to 8 byte hex string>.LOG
				*	Example: 5D960E10.LOG -> 03-OCT-2019 at 3:04:48 PM
				*/
				SdFile file;
				{
					char filename[15];
					strcpy_P(UInt32ToHexStr(header.startTime, filename), kFileExtStr);
					sd.remove(filename);
					success = file.open(filename, O_WRONLY | O_CREAT);
				}
				if (success)
				{
					success = file.write(&kLogFileMarker, sizeof(uint32_t)) == sizeof(uint32_t) &&
						file.write(&header, sizeof(SHikeLogHeader)) == sizeof(SHikeLogHeader);
					while (success)
					{
						// Read up to kNumEntriesPerPass entries at at time
						uint32_t endPos = mLogData->GetPos();
						uint8_t	entriesRead = mLogData->Read(kNumEntriesPerPass*sizeof(SHikeLogEntry), entry)/sizeof(SHikeLogEntry);
						success = entriesRead > 0;
						if (success)
						{
							uint8_t i = 0;
							bool	done = false;
							for (; i < entriesRead; i++)
							{
								/*
								*	If this isn't the end of the log THEN
								*	continue
								*/
								if (entry[i].pressure)
								{
									continue;
								}
								done = true;
								/*
								*	If not all of the entries were inspected THEN
								*	rewind the data stream to point to the header of
								*	the next log (or null if this is the last log.)
								*/
								if (i != entriesRead)
								{
									endPos += ((i * sizeof(SHikeLogEntry)) + sizeof(uint32_t));
									mLogData->Seek(endPos, DataStream::eSeekSet);
								}
								break;
							}
							if (i)
							{
								size_t	bytesToWrite = i * sizeof(SHikeLogEntry);
								success = file.write(entry, bytesToWrite) == bytesToWrite;
							}
							if (!done)
							{
								continue;
							}
							break;
						}
					}
  					file.close();
				}
			/*
			*	else the end of all logs has been found.
			*	Rewind to the start of this log
			*/
			} else
			{
				mLogData->Seek(startPos, DataStream::eSeekSet);
				break;
			}
		}
	} else
	{
		sd.initErrorHalt();
	}
	return(success);
}

/**************************** SaveLogSummariesToSD ****************************/
bool HikeLog::SaveLogSummariesToSD(void)
{
	SdFat sd;
	bool	success = sd.begin(mSDSelectPin);
	if (success)
	{
		SRingHeader		header;
		struct SSummaryBlock
		{
			SHikeSummary	summary[5];	// Assumes kMaxHikeSummaries is 125, so 125/5 = 25, no remainder
		} summaryBlock;
		EEPROM.get(kLogRingAddressesEEAddr, header);
		/*
		*	[32]	uint16_t	lastHikesHead
		*	[34]	uint16_t	lastHikesTail
		*	[36]	uint8_t		unassigned3[2];
		*	[38]	SHikeSummary hikes[kMaxHikeSummaries];  size = 16 bytes per hike
		*/
		sFileCreationTime = LogDateTime::Time();
		SdFile::dateTimeCallback(SDFatDateTimeCB);
		success = header.head != header.tail;
		if (success)
		{
			/*
			*	Create a file to hold the summaries.
			*/
			SdFile file;
			{
				char filename[15];
				strcpy_P(filename, kSummariesFilenameStr);
				sd.remove(filename);
				success = file.open(filename, O_WRONLY | O_CREAT);
			}
			if (success)
			{
				success = file.write(&header, sizeof(SRingHeader)) == sizeof(SRingHeader);
				uint16_t	summaryStorageEEAddr = kLogRingStorageEEAddr;
				for (; success &&
						summaryStorageEEAddr < (kLogRingStorageEEAddr + (sizeof(SHikeSummary) * kMaxHikeSummaries));
							summaryStorageEEAddr += sizeof(SSummaryBlock))
				{
					EEPROM.get(summaryStorageEEAddr, summaryBlock);
					success = file.write(&summaryBlock, sizeof(SSummaryBlock)) == sizeof(SSummaryBlock);
				}
				file.close();
				Serial.println(success ? F("Success!") : F("Write Error"));
			} else
			{
				Serial.println(F("Create Error"));
			}
		} else
		{
			Serial.println(F("No Summaries"));
		}
	} else
	{
		sd.initErrorHalt();
	}
	return(success);
}

/*************************** LoadLogSummariesFromSD ***************************/
bool HikeLog::LoadLogSummariesFromSD(void)
{
	SdFat sd;
	bool	success = sd.begin(mSDSelectPin);
	if (success)
	{
		SRingHeader		header;
		struct SSummaryBlock
		{
			SHikeSummary	summary[5];	// Assumes kMaxHikeSummaries is 125, so 125/5 = 25, no remainder
		} summaryBlock;
		/*
		*	Open the summaries file.
		*/
		SdFile file;
		{
			char filename[15];
			strcpy_P(filename, kSummariesFilenameStr);
			success = file.open(filename, O_RDONLY);
		}
		if (success)
		{
			success = file.read(&header, sizeof(SRingHeader)) == sizeof(SRingHeader);
			if (success)
			{
				EEPROM.put(kLogRingAddressesEEAddr, header);
				Serial.println(F("Loading "));

				uint16_t	summaryStorageEEAddr = kLogRingStorageEEAddr;
				for (; success &&
						summaryStorageEEAddr < (kLogRingStorageEEAddr + (sizeof(SHikeSummary) * kMaxHikeSummaries));
							summaryStorageEEAddr += sizeof(SSummaryBlock))
				{
					success = file.read(&summaryBlock, sizeof(SSummaryBlock)) == sizeof(SSummaryBlock);
					EEPROM.put(summaryStorageEEAddr, summaryBlock);
					Serial.print('.');
				}
			}
			file.close();
			Serial.println(success ? F("Success!") : F("Read Error"));
		} else
		{
			Serial.println(F("Open Error"));
		}
	} else
	{
		sd.initErrorHalt();
	}
	return(success);
}

/*********************************** StopLog **********************************/
void HikeLog::StopLog(
	time32_t	inEndTime)
{
	if (mHike.startTime && !mHike.endTime)
	{
		mHike.endTime = inEndTime == 0 ? LogDateTime::Time() : inEndTime;
		mHike.endTemp = (int16_t)LogTempPres::GetInstance().PeekTemperature();
	}
}
						
/********************************* ElapsedTime ********************************/
time32_t HikeLog::ElapsedTime(void) const
{
	// if the log is stopped ? () : if there is an active log ? () : no active log, return 0
	return(mHike.endTime ? (mHike.endTime - mHike.startTime) : (mHike.startTime ? (LogDateTime::Time() - mHike.startTime) : 0));
}

/****************************** SDFatDateTimeCB *******************************/
/*
*	SDFat date time callback.
*/
void HikeLog::SDFatDateTimeCB(
	uint16_t*	outDate,
	uint16_t*	outTime)
{
	LogDateTime::SDFatDateTime(sFileCreationTime, outDate, outTime);
}

// The hike summaries stored in the MCU's EEPROM

/**************************** GetSavedHikesLastRef *****************************/
uint16_t HikeLog::GetSavedHikesLastRef(void) const
{
	SRingHeader	header;
	EEPROM.get(kLogRingAddressesEEAddr, header);
	return(header.tail);
}

/******************************** GetSavedHike ********************************/
bool HikeLog::GetSavedHike(
	uint16_t		inRef,
	SHikeSummary&	outHikeSummary)
{
	SRingHeader	header;
	EEPROM.get(kLogRingAddressesEEAddr, header);
	EEPROM.get(inRef + kLogRingStorageEEAddr, outHikeSummary);
	return(header.head != header.tail);
}

/**************************** GetNextSavedHikeRef *****************************/
uint16_t HikeLog::GetNextSavedHikeRef(
	uint16_t	inRef)
{
	uint16_t	ref = (inRef + sizeof(SHikeSummary)) % (sizeof(SHikeSummary)*kMaxHikeSummaries);
	SRingHeader	header;
	EEPROM.get(kLogRingAddressesEEAddr, header);
	/*
	*	If the head doesn't follow the tail then the buffer isn't full yet so
	*	so don't go past the tail.
	*/
	if (!header.head && header.tail != (sizeof(SHikeSummary)*(kMaxHikeSummaries-1)) &&
		ref > header.tail)
	{
		ref = sizeof(SHikeSummary);
	}
	return(ref);
}

/**************************** GetPrevSavedHikeRef *****************************/
uint16_t HikeLog::GetPrevSavedHikeRef(
	uint16_t	inRef)
{
	uint16_t	ref = (inRef + (sizeof(SHikeSummary)*(kMaxHikeSummaries-1))) % (sizeof(SHikeSummary)*kMaxHikeSummaries);
	SRingHeader	header;
	EEPROM.get(kLogRingAddressesEEAddr, header);
	/*
	*	If the head doesn't follow the tail then the buffer isn't full yet so
	*	so don't go past the tail.
	*/
	if (!header.head && header.tail != (sizeof(SHikeSummary)*(kMaxHikeSummaries-1)))
	{
		if (ref > header.tail ||
			ref == 0)
		{
			ref = header.tail;
		}
	}
	return(ref);
}

