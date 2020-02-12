/*
*	LogAction.cpp, Copyright Jonathan Mackey 2019
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
#include <Arduino.h>
#include "LogAction.h"
#include "LogPacket.h"
#include "HikeLog.h"
#include "LogTempPres.h"
#include "LogDateTime.h"
#include "RFM69.h"    // https://github.com/LowPowerLab/RFM69

const uint32_t	k3ButtonRemoteDefaultTime = 100;	// milliseconds

//#define DEBUG_RADIO	1

/********************************* LogAction **********************************/
LogAction::LogAction(void)
: mSDCardPresent(false)
{
}

/********************************* Initialize *********************************/
void LogAction::Initialize(
	RFM69*		inRadio,
	HikeLog*	inHikeLog)
{
	mRadio = inRadio;
	mHikeLog = inHikeLog;
	mMode = eBMP280SyncMode;
	mSyncState = eBMP280Syncing;
}

/******************************** GoToLogMode *********************************/
void LogAction::GoToLogMode(void)
{
	IncrementMode(true);
	if (mMode <= eReviewHikesMode)
	{
		mMode = eLogMode;
		mLogStateModifier = HikeLog::eModifier;
	}
}

/******************************* IncrementMode ********************************/
void LogAction::IncrementMode(
	bool	inIncrement)
{
	uint8_t	mode = mMode;
	// No active hike
	//	No sync error: eLogMode <> eStartLocSelMode <> eEndLocSelMode <> eReviewHikesMode <> eLogMode
	//	Sync error: eLogMode <> eBMP280SyncMode <> eStartLocSelMode <> eEndLocSelMode <> eReviewHikesMode <> eLogMode
	// Active hike
	//	No sync error: eLogMode <> eReviewHikesMode <> eLogMode
	//	Sync error: eLogMode <> eBMP280SyncMode <> eReviewHikesMode <> eLogMode
	switch (mode)
	{
		case eLogMode:
			if (mHikeLog->Active())
			{
				mode = inIncrement ? (mSyncState != eBMP280SyncError ? eReviewHikesMode : eBMP280SyncMode) : eReviewHikesMode;
			} else
			{
				mode = inIncrement ? (mSyncState != eBMP280SyncError ? eStartLocSelMode : eBMP280SyncMode) : eReviewHikesMode;
			}
			break;
		case eStartLocSelMode:
			mode = inIncrement ? eEndLocSelMode : (mSyncState != eBMP280SyncError ? eLogMode : eBMP280SyncMode);
			break;
		case eEndLocSelMode:
			mode = inIncrement ? eReviewHikesMode : eStartLocSelMode;
			break;
		case eReviewHikesMode:
			if (mHikeLog->Active())
			{
				mode = inIncrement ? eLogMode : (mSyncState != eBMP280SyncError ? eLogMode : eBMP280SyncMode);
			} else
			{
				mode = inIncrement ? eLogMode : eEndLocSelMode;
			}
			mode = inIncrement ? eLogMode : eEndLocSelMode;
			break;
		case eResetLogMode:
			mode = inIncrement ? eLogMode : eReviewHikesMode;
			break;
		case eBMP280SyncMode:
			/*
			*	Don't allow mode changes while syncing
			*/
			if (mSyncState != eBMP280Syncing)
			{
				if (mHikeLog->Active())
				{
					mode = inIncrement ? eLogMode : eReviewHikesMode;
				} else
				{
					mode = inIncrement ? eStartLocSelMode : eLogMode;
				}
			}
			break;
		//eSDCardMode:	Moving in or out of eSDCardMode only occurs when the
		//				card is inserted or removed.
	}
	mMode = mode;
	
	/*
	*	Set any initial states when changing to a mode.
	*/
	switch (mode)
	{
		case eLogMode:
			mLogStateModifier = HikeLog::eModifier;
			break;
		case eResetLogMode:
			mResetLogState = eResetVerifyNo;
			break;
		case eStartLocSelMode:
			mLocIndex = mHikeLog->StartingLocIndex();
			break;
		case eEndLocSelMode:
			mLocIndex = mHikeLog->EndingLocIndex();
			break;
		case eReviewHikesMode:
			mHikeRef = mHikeLog->GetSavedHikesLastRef();
			mReviewState = eReviewLocs;
			break;
	}
}

/******************************** EnterPressed ********************************/
void LogAction::EnterPressed(void)
{
	switch (mMode)
	{
		case eLogMode:
			switch(mHikeLog->GetLogState() + mLogStateModifier)
			{
				case HikeLog::eStopped + HikeLog::eModifier:
				case HikeLog::eNotRunning + HikeLog::eModifier:
					// Start or Resume
					// If the log isn't active, then start
					// if the log is stopped, then resume
					// Only allow start if the BMP280 is responding
					if (LogTempPres::GetInstance().IsValid() ||
						mHikeLog->GetLogState() == HikeLog::eStopped + HikeLog::eModifier)
					{
						mHikeLog->StartLog();
					}
					break;
				case HikeLog::eRunning:
				case HikeLog::eRunning + HikeLog::eModifier:
					mHikeLog->StopLog();
					break;
				case HikeLog::eStopped:
					mHikeLog->EndLog();
					break;
				case HikeLog::eNotRunning:
					mHikeLog->SwapLocIndexes();
					break;
			}
			break;
		case eStartLocSelMode:
			mHikeLog->StartingLocIndex() = mLocIndex;
			mHikeLog->UpdateStartingAltitude();
			IncrementMode(true);
			break;
		case eEndLocSelMode:
			mMode = eLogMode;
			mLogStateModifier = HikeLog::eModifier;
			break;
		case eResetLogMode:
			if (mResetLogState == eResetVerifyYes)
			{
				mResetLogState = mHikeLog->InitializeLog() ? eResetSuccess : eResetError;
			}
			break;
		case eReviewHikesMode:
			mReviewState = !mReviewState;
			break;
		case eBMP280SyncMode:
			switch (mSyncState)
			{
				case eBMP280SyncError:
					mSyncState = eBMP280Syncing;
					break;
				case eBMP280SyncSuccess:
					mMode = eLogMode;
					mLogStateModifier = HikeLog::eModifier;
					break;
			}
			break;
		case eSDCardMode:
			if (mSDCardPresent)
			{
				switch(mSDCardState)
				{
					case eSaveToSD:
						mSDCardState = eSavingToSD;
						break;
					case eSDSaveError:
						mSDCardState = eEjectSDCardNoReset;
						break;
					case eSDWriteSuccess:
						mSDCardState = eEjectSDCardAllowReset;
						break;
				}
			}
			break;
	}
}

/*********************************** Update ***********************************/
/*
*	Called from loop() just after the layout has updated.  Any states that
*	need time are handled here.
*/
void LogAction::Update(void)
{
	switch (mMode)
	{
		case eSDCardMode:
			if (mSDCardState == eSavingToSD)
			{
				mSDCardState = mHikeLog->SaveLogToSD() ? eSDWriteSuccess : eSDSaveError;
			}
			break;
	}
}

/******************************* IncrementValue *******************************/
void LogAction::IncrementValue(
	bool	inIncrement)
{
	switch (mMode)
	{
		case eLogMode:
			mLogStateModifier = mLogStateModifier ? 0:HikeLog::eModifier;
			break;
		case eStartLocSelMode:
		case eEndLocSelMode:
			HikeLocations::GetInstance().GoToLocation(mLocIndex);
			
			if (inIncrement)
			{
				HikeLocations::GetInstance().Next();
			} else
			{
				HikeLocations::GetInstance().Previous();
			}
			mLocIndex = HikeLocations::GetInstance().GetCurrentIndex();
			break;
		case eResetLogMode:
			/*
			*	Reset log mode only has options when choosing yes or no.
			*/
			switch (mResetLogState)
			{
				case eResetVerifyNo:
					mResetLogState = eResetVerifyYes;
					break;
				case eResetVerifyYes:
					mResetLogState = eResetVerifyNo;
					break;
			}
			break;
		case eReviewHikesMode:
			if (inIncrement)
			{
				mHikeRef = mHikeLog->GetNextSavedHikeRef(mHikeRef);
			} else
			{
				mHikeRef = mHikeLog->GetPrevSavedHikeRef(mHikeRef);
			}
			break;
	}
}

/****************************** SetSDCardPresent ******************************/
void LogAction::SetSDCardPresent(
	bool	inSDCardPresent)
{
	mSDCardPresent = inSDCardPresent;
	/*
	*	If the card was just inserted THEN
	*	set the mode and initial state.
	*/
	if (inSDCardPresent)
	{
		mMode = eSDCardMode;
		mSDCardState = mHikeLog->Active() ? eEjectSDCardNoReset : eSaveToSD;
	/*
	*	Else the card was just ejected
	*/
	} else
	{
		// If eSDWriteSuccess OR eEjectSDCardAllowReset
		mMode = (mSDCardState & eSDWriteSuccess) != 0 ? eResetLogMode : eLogMode;
		mResetLogState = eResetVerifyNo;
	}
}

/**************************** CheckRadioForPackets ****************************/
/*
	Approximately every 4.5 seconds the BMP280 remote broadcasts the current
	temperature and pressure.  Immediately after this broadcast the radio
	listens for any requests from the 3 button remote.  If no request is made
	within a period of approximately 1/4 second, the radio is put to sleep.  If
	a reqest is received, meaning the 3 button remote is awake and active, the
	radio stays in receive mode till the next BMP280 broadcast.  This pattern
	repeats till the the 3 button remote goes to sleep.  For as long as the 3
	button remote is awake, it will request a synchronization packet after each
	BMP280 broadcast.  The 3 button remote will also send packets in response to
	user actions.  The 3 button remote will not make any requests during the
	period that the BMP280 broadcast is expected.
*/
void LogAction::CheckRadioForPackets(
	bool	inDisplayIsOff)
{
	if (mMode != eBMP280SyncMode ||
		mSyncState != eBMP280Syncing)
	{
		/*
		*	If the BMP280 packet is about to arrive...
		*/
		if (mBMP280Period.Passed())
		{
			/*
			*	Callng receiveDone will turn on the receiver and immediately
			*	return false if the the receiver isn't already on.  If it is on,
			*	true is returned if a packet has arrived, or false otherwise.
			*/
			if (mRadio->receiveDone())
			{
				/*
				*	If the packet received is a BMP280 packet...
				*/
				if (HandleBMP280PacketRx())
				{
					/*
					*	Set the time that the receiver will be left on for the 3
					*	button remote to respond if the period has passed.  Note
					*	that if the 3 button remote does respond, the radio will
					*	be left on till period expires or another packet arrives
					*	from the remote.  If a packet arrives the period is
					*	restarted.
					*/
					if (m3ButtonRemotePeriod.Passed())
					{
						m3ButtonRemotePeriod.Set(inDisplayIsOff ? k3ButtonRemoteDefaultTime : 0x4000);
						m3ButtonRemotePeriod.Start();
					}

					/*
					*	Put the radio in recieve mode to wait for any requests from the 3
					*	button remote.
					*	
					*	Note that receiveDone can't return true on the first call after the
					*	previous packet arrived, so it's safe to ignore its return value.
					*/
					mRadio->receiveDone();
				#ifdef DEBUG_RADIO
					Serial.println(F("-"));
				#endif
				} else
				{
					/*
					*	On the (extremely) off chance that a packet from the 3
					*	button remote arrived during the period that the BMP280
					*	packet is extected to arrive...
					*/
					HandlePacketRx();
				}
			/*
			*	If there is no response from the BMP280 after 15 seconds THEN
			*	try to resync
			*/
			} else if (mBMP280Period.ElapsedTime() > 15000)
			{
				/*
				*	If the resync fails the mBMP280Period is disabled (set to
				*	zero, Passed will return false.)  The user will have to
				*	manually resync if this occurs.
				*/
				mMode = eBMP280SyncMode;
				mSyncState = eBMP280Syncing;
			}
		/*
		*	Else if a packet has arrived...
		*/
		} else if (!m3ButtonRemotePeriod.Passed() ||
			(RFM69::_mode == RF69_MODE_RX && RFM69::hasData()))
		{
			if (mRadio->receiveDone())
			{
				HandlePacketRx();
			}
		/*
		*	Else if the period for receiving packets from the 3 button remote
		*	has passed AND
		*	the radio isn't asleep THEN
		*	put it to sleep.
		*/
		} else if (RFM69::_mode != RF69_MODE_SLEEP)
		{
			mRadio->sleep();
		#ifdef DEBUG_RADIO
			Serial.println(F("sleep"));
		#endif
		}
	} else
	{
		SyncWithBMP280Remote();
		m3ButtonRemotePeriod.Set(inDisplayIsOff ? k3ButtonRemoteDefaultTime : 0x4000);
		m3ButtonRemotePeriod.Start();
		/*
		*	Put the radio in recieve mode to wait for any requests from the 3
		*	button remote.
		*	
		*	Note that receiveDone can't return true on the first call after the
		*	previous packet arrived, so it's safe to ignore its return value.
		*/
		mRadio->receiveDone();
	#ifdef DEBUG_RADIO
		Serial.println(F("-"));
	#endif
	}
}

/**************************** SyncWithBMP280Remote ****************************/
/*
*	The BMP280 remote sends a packet containing the current temperature and
*	barometric pressure approximately every 4.5 seconds.  This routine first
*	verifies that the remote is operating and sending valid data, then it
*	measures the time till the next packet, the mBMP280Period.
*/
void LogAction::SyncWithBMP280Remote(void)
{
	MSPeriod	timeout(8100);
	mBMP280Period.Set(0);	// Set to 0 to show the BMP280 isn't synced
	timeout.Start();
	/*
	*	Loop while the timeout period hasn't passed AND
	*	the BMP280 hasn't been synced
	*/
	while (!timeout.Passed() && !mBMP280Period.Get())
	{
		if (mRadio->receiveDone())
		{
			Log::SBMP280Packet*	packet = (Log::SBMP280Packet*)RFM69::DATA;
			if (packet->message == Log::kBMP280)
			{
				timeout.Start();
				while (!timeout.Passed())
				{
					if (mRadio->receiveDone() &&
						packet->message == Log::kBMP280)
					{
						mBMP280Period.Set(timeout.ElapsedTime()); 
						mBMP280Period.Start(-Log::kBMP280AcquisitionTime);
						LogTempPres::GetInstance().Set(packet->temp, packet->pres);
						mSyncState = eBMP280SyncSuccess;
						if (!mHikeLog->Active())
						{
							mHikeLog->UpdateStartingAltitude();
						}
						break;
					}
				}
			}/* else
			{
				Ignore any packets received while syncing 
			}*/
		}
	}
	if (!mBMP280Period.Get())
	{
		/*
		*	Stop logging temp and pressure when the BMP280 isn't responding
		*	but don't stop the entire log (still measure elapsed time)
		*/
		LogTempPres::GetInstance().MakeInvalid();
		mSyncState = eBMP280SyncError;
	}
}

/**************************** HandleBMP280PacketRx ****************************/
bool LogAction::HandleBMP280PacketRx(void)
{
	Log::SBMP280Packet*	packet = (Log::SBMP280Packet*)RFM69::DATA;
	bool	handled = packet->message == Log::kBMP280;
	if (handled)
	{
		uint32_t	measuredPeriod = mBMP280Period.ElapsedTime() - Log::kBMP280AcquisitionTime;
		mBMP280Period.Start(-Log::kBMP280AcquisitionTime);
		/*
		*	The RC oscillator on the ATtiny84A BMP280 remote changes based on
		*	the ambient temperature.  The code below attempts to adjust for
		*	these swings by tweaking the period.
		*
		*	If the measured period is within the normal range THEN follow the
		*	change as an average to avoid large swings.
		*/
		if (measuredPeriod > 4000 && measuredPeriod < 5000)
		{
			mBMP280Period.Set((mBMP280Period.Get() + measuredPeriod)/2);
		/*
		*	Else if a large change in the oscillator frequency occured THEN
		*	follow it faster.
		*
		*	It got here because the change was so great the receiver wasn't on
		*	and it missed a packet and stayed on till the next packet arrived. 
		*	The measuredPeriod is less than twice the normal period.
		*
		*	This can happen if:
		*		- there are other devices using the same frequency.
		*		- the signal is weak.
		*		- there's a large temperature drop.
		*/
		} else if (measuredPeriod > 8000 && measuredPeriod < 10000)
		{
			mBMP280Period.Set(measuredPeriod / 2);
		}
		//Serial.println(packet->pres, HEX);

		/*
		*	Logger auto stop is not being used because it's not working.  It's
		*	based on the pressure staying the same for the timeout period, and
		*	this will almost never happen.  The pressure is constantly changing
		*	even when the BMP280 remote isn't moving.
		*/
	#if 0
		if (LogTempPres::GetInstance().Set(packet->temp, packet->pres) > kLoggerAutoStopTimeout)
		{
			// Does nothing if the log is already stopped.
			Stop(LogTempPres::GetInstance().TimePressureChanged());
		}
	#else
		LogTempPres::GetInstance().Set(packet->temp, packet->pres);
	#endif
	}
	return(handled);
}

/******************************* InitSyncPacket *******************************/
uint8_t LogAction::InitSyncPacket(
	uint8_t*	inPacket)
{
	Log::SSyncPacket*	packet = (Log::SSyncPacket*)inPacket;
	packet->message = Log::kSync;
	packet->time = LogDateTime::Time();
	packet->startTime = mHikeLog->StartTime();
	packet->endTime = mHikeLog->EndTime();
	packet->startLocIndex = mHikeLog->StartingLocIndex();
	packet->endLocIndex = mHikeLog->EndingLocIndex();
	packet->logIsFull = mHikeLog->IsFull();
	return(sizeof(Log::SSyncPacket));			
}

/***************************** InitLocationPacket *****************************/
uint8_t LogAction::InitLocationPacket(
	uint16_t	inLocIndex,
	uint8_t*	inPacket)
{
	Log::SLocnPacket*	packet = (Log::SLocnPacket*)inPacket;
	packet->message = Log::kHikeLocation;
	HikeLocations::GetInstance().GoToLocation(inLocIndex);
	const SHikeLocationLink&	reqLoc = HikeLocations::GetInstance().GetCurrent();
	packet->locIndex = inLocIndex;
	packet->link.next = HikeLocations::GetInstance().GetNextIndex();
	packet->link.prev = HikeLocations::GetInstance().GetPreviousIndex();
	memcpy(&packet->link.loc, &reqLoc.loc, sizeof(SHikeLocation));
	return(sizeof(Log::SLocnPacket));			
}

/******************************* HandlePacketRx *******************************/
/*
*	Handles packets sent by the 3 button remote.
*/
void LogAction::HandlePacketRx(void)
{
	uint8_t	packetBuff[sizeof(Log::SLocnPacket)];	// sizeof the largest packet struct
	uint8_t	packetSize = 0;
	switch (((Log::SPacket*)RFM69::DATA)->message)
	{
		case Log::kGetLocation:
			packetSize = InitLocationPacket(((Log::SLocnIndexPacket*)RFM69::DATA)->locIndex,
											packetBuff);
			break;
		case Log::kSetStartLocation:
			if (!mHikeLog->Active())
			{
				uint16_t	locIndex = ((Log::SLocnIndexPacket*)RFM69::DATA)->locIndex;
				mHikeLog->StartingLocIndex() = locIndex;
				mHikeLog->UpdateStartingAltitude();
				packetSize = InitLocationPacket(locIndex, packetBuff);
				if (mMode == eStartLocSelMode)
				{
					mLocIndex = locIndex;
				}
			}
			break;
		case Log::kSetEndLocation:
			if (!mHikeLog->Active())
			{
				uint16_t	locIndex = ((Log::SLocnIndexPacket*)RFM69::DATA)->locIndex;
				mHikeLog->EndingLocIndex() = locIndex;
				packetSize = InitLocationPacket(locIndex, packetBuff);
				if (mMode == eEndLocSelMode)
				{
					mLocIndex = locIndex;
				}
			}
			break;
		case Log::kStartLog:	// Start or resume the log
		{
			uint8_t	logState = mHikeLog->GetLogState();
			if (logState == HikeLog::eStopped ||
				logState == HikeLog::eNotRunning)
			{
				mMode = eLogMode;
				mLogStateModifier = HikeLog::eModifier;
				mHikeLog->StartLog(((Log::STimePacket*)RFM69::DATA)->time);
			}
			break;
		}
		case Log::kStopLog:
			if (mHikeLog->GetLogState() == HikeLog::eRunning)
			{
				mMode = eLogMode;
				mLogStateModifier = HikeLog::eModifier;
				mHikeLog->StopLog(((Log::STimePacket*)RFM69::DATA)->time);
			}
			break;
		case Log::kEndLog:	// aka Done
			if (mHikeLog->GetLogState() == HikeLog::eStopped)
			{
				mMode = eLogMode;
				mLogStateModifier = 0;
				mHikeLog->EndLog();
			}
			break;
		case Log::kSwapLocIndexes:
			if (mHikeLog->GetLogState() == HikeLog::eNotRunning)
			{
				mMode = eLogMode;
				mLogStateModifier = 0;
				mHikeLog->SwapLocIndexes();
			}
			break;
		case Log::kSyncBMP280:
			// Ignore if currently syncing OR
			// the BMP280 is currently synced.
			if (mSyncState != eBMP280SyncMode &&
				mBMP280Period.Get() == 0)
			{
				mMode = eBMP280SyncMode;
				mSyncState = eBMP280Syncing;
			}
			break;
		/*
		*	Just in case a BMP280 packet arrives outside of the expected period.
		*/
		case Log::kBMP280:
			packetSize = 1;	// ACK_REQUESTED is false for Log::kBMP280
			HandleBMP280PacketRx();
			break;
		default:
			// Log::kGetSync:  handled below by call to InitSyncPacket
			break;

	}
	if (packetSize == 0)
	{
		packetSize = InitSyncPacket(packetBuff);
	}
	
	if (RFM69::ACK_REQUESTED)
	{
		mRadio->sendACK(packetBuff, packetSize);
		mRadio->receiveDone();	// Immediately move back into Rx mode
		/*
		*	Extend the period till the next expected BMP280 packet. The value used
		*	to set the m3ButtonRemotePeriod is just a value large enough to last
		*	until the next BMP280 packet or remote packet.
		*/
		m3ButtonRemotePeriod.Set(0x4000);
		m3ButtonRemotePeriod.Start();
	#ifdef DEBUG_RADIO
		Serial.println(F("+"));
	#endif
	}
}