/*
*	RemoteLogAction.cpp, Copyright Jonathan Mackey 2019
*	Handles input from the remote UI.
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
#include "RemoteLogAction.h"
#include "LogPacket.h"
#include "RemoteHikeLog.h"
#include "LogTempPres.h"
#include "UnixTime.h"
#include "RFM69.h"    // https://github.com/LowPowerLab/RFM69

// When the gateway display is on, it can take up to 165ms or more to update
// the gateway display.  Extend the timeout to account for this.
const uint32_t	kPacketTimeout = 250;	// milliseconds

/********************************* RemoteLogAction **********************************/
RemoteLogAction::RemoteLogAction(void)
: mPacketTimoutPeriod(kPacketTimeout)
{
}

/********************************* Initialize *********************************/
void RemoteLogAction::Initialize(
	RFM69*			inRadio,
	RemoteHikeLog*	inHikeLog)
{
	mRadio = inRadio;
	mHikeLog = inHikeLog;
	
	mWaitingForPacket = 0;
	mPacketsInQueue = 0;
	mPacketQueueHead = 0;
	mPacketQueueTail = 0;
	mPacketTimeouts = 0;
	mPacketTimoutPeriod.Start();

	mMode = eBMP280SyncMode;
	mSyncState = eBMP280Syncing;
}

/********************************* Initialize *********************************/
void RemoteLogAction::Initialize(void)
{
	mWaitingForPacket = 0;
	mPacketsInQueue = 0;
	mPacketQueueHead = 0;
	mPacketQueueTail = 0;
	mPacketTimeouts = 0;
	mPacketTimoutPeriod.Start();

	mMode = eBMP280SyncMode;
	mSyncState = eBMP280Syncing;
}

/******************************** GoToInfoMode ********************************/
void RemoteLogAction::GoToInfoMode(void)
{
	ModeButtonPressed();
	if (mMode <= eBMP280SyncMode)
	{
		mMode = eInfoMode;
	}
}

/***************************** ModeButtonPressed ******************************/
void RemoteLogAction::ModeButtonPressed(void)
{
	uint8_t	mode = mMode;
	switch (mode)
	{
		case eInfoMode:
			mode = eLogMode;
			break;
		case eLogMode:
	#ifdef SUPPORT_LOC_SEL_MODES
			mode = mHikeLog->Active() ? eInfoMode : eStartLocSelMode;
	#else
			mode = eInfoMode;
	#endif
			break;
	#ifdef SUPPORT_LOC_SEL_MODES
		case eStartLocSelMode:
			mode = eEndLocSelMode;
			break;
		case eEndLocSelMode:
			mode = eInfoMode;
			break;
	#endif
		case eBMP280SyncMode:
			if (mSyncState == eBMP280SyncSuccess)
			{
				mode = eInfoMode;
			}
			break;
	}
	mMode = mode;
}

/***************************** LeftButtonPressed ******************************/
void RemoteLogAction::LeftButtonPressed(void)
{
	switch (mMode)
	{
		case eLogMode:
		{
			uint8_t	logState = mHikeLog->GetLogState();
			if (logState == RemoteHikeLog::eStopped)
			{
				QueueRequestPacket(Log::kEndLog);	// == DONE
			} else if (logState == RemoteHikeLog::eNotRunning)
			{
				QueueRequestPacket(Log::kSwapLocIndexes);
			}
			break;
		}
	#ifdef SUPPORT_LOC_SEL_MODES
		case eStartLocSelMode:
			QueueLocnIndexPacket(Log::kSetStartLocation, mHikeLog->GetLocLink(true).prev);
			break;
		case eEndLocSelMode:
			QueueLocnIndexPacket(Log::kSetEndLocation, mHikeLog->GetLocLink(false).prev);
			break;
	#endif
		case eBMP280SyncMode:
			if (mSyncState == eBMP280SyncError)
			{
				mSyncState = eBMP280Syncing;
			}
			break;
	}
}

/****************************** LogStateChanged *******************************/
void RemoteLogAction::LogStateChanged(void)
{
	/*
	*	mStartStopMessageTime is the time of the last kStopLog or kStartLog sent.
	*	mStartStopMessageTime is reset here because the packet was sucessfully
	*	sent if the log state changed.
	*/
	mStartStopMessageTime = 0;
}

/***************************** RightButtonPressed *****************************/
void RemoteLogAction::RightButtonPressed(void)
{
	switch (mMode)
	{
		case eLogMode:
		{
			uint8_t	logState = mHikeLog->GetLogState();
			if (logState != RemoteHikeLog::eCantRun)
			{
				/*
				*	Because the packet send can fail, the time of the original
				*	button press is used in case of a failure (mStartStopMessageTime.)
				*
				*	Example: You pressed stop, but the send failed for whatever
				*	reason.  You then press stop again.  You would want the time
				*	of the original button press used, not the current time.
				*
				*	If the message time is invalid THEN
				*	use the current time as the message time.
				*/
				if (mStartStopMessageTime == 0)
				{
					mStartStopMessageTime = UnixTime::Time();
				}
				/*
				*	If the log is running, then stop
				*	else the log is either not running or is stopped.
				*	By sending a kStartLog message, the log is either started or
				*	resumed.
				*/
				QueueTimePacket(logState == RemoteHikeLog::eRunning ? Log::kStopLog : Log::kStartLog,
									mStartStopMessageTime);
			}
			break;
		}
	#ifdef SUPPORT_LOC_SEL_MODES
		case eStartLocSelMode:
			QueueLocnIndexPacket(Log::kSetStartLocation, mHikeLog->GetLocLink(true).next);
			break;
		case eEndLocSelMode:
			QueueLocnIndexPacket(Log::kSetEndLocation, mHikeLog->GetLocLink(false).next);
			break;
	#endif
	}
	
}	

/**************************** CheckRadioForPackets ****************************/
void RemoteLogAction::CheckRadioForPackets(
	bool	inDisplayIsOff)
{
	if (mSyncState != eBMP280Syncing)
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
				*	If the packet received is a BMP280 packet (as expected)
				*/
				if (HandleBMP280PacketRx())
				{
					/*
					*	If the display is on AND
					*	there are no outstanding packets THEN
					*	Queue up a sync request.
					*/
					if (!inDisplayIsOff &&
						!mWaitingForPacket)
					{
						QueueRequestPacket(Log::kGetSync);
						SendPacketIfNotBusy();
					}
				} else
				{
					/*
					*	On the (extremely) off chance that a packet from the
					*	gateway arrived during the period that the BMP280
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
				SyncWithBMP280Remote();
			}
		} else
		{
			/*
			*	If there are outstanding packet requests from the gateway AND
			*	a packet arrived...
			*/
			if (mWaitingForPacket &&
				mRadio->receiveDone())
			{
				HandlePacketRx();
			}
			/*
			*	If the queue is empty THEN
			*	put the radio to sleep
			*/
			if (!SendPacketIfNotBusy())
			{
				mRadio->sleep();
			}
		}
	} else
	{
		SyncWithBMP280Remote();
	}
}

/**************************** SyncWithBMP280Remote ****************************/
/*
*	The BMP280 remote sends a packet containing the current temperature and
*	barometric pressure approximately every 4.5 seconds.  This routine first
*	verifies that the remote is operating and sending valid data, then it
*	measures the time till the next packet.
*/
void RemoteLogAction::SyncWithBMP280Remote(void)
{
	MSPeriod	timeout(8100);
	mBMP280Period.Set(0);	// Set to 0 to show the BMP280 isn't synced
	timeout.Start();
	mMode = eBMP280SyncMode;
	mSyncState = eBMP280Syncing;

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
						mMode = eGatewaySyncMode;
						if (!mHikeLog->Active())
						{
							mHikeLog->UpdateStartingAltitude();
						}
						break;
					}
				}
			} else
			{
				HandlePacketRx();
			}
		}
	}
	if (!mBMP280Period.Get())
	{
		LogTempPres::GetInstance().Set(0, 0);
		mMode = eBMP280SyncMode;
		mSyncState = eBMP280SyncError;
	}
}

/**************************** HandleBMP280PacketRx ****************************/
bool RemoteLogAction::HandleBMP280PacketRx(void)
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

		LogTempPres::GetInstance().Set(packet->temp, packet->pres);
	}
	return(handled);
}

/***************************** HandleSyncPacketRx *****************************/
void RemoteLogAction::HandleSyncPacketRx(void)
{
	Log::SSyncPacket*	packet = (Log::SSyncPacket*)RFM69::DATA;
	mHikeLog->Sync(	packet->startTime,
					packet->endTime,
					packet->startLocIndex,
					packet->endLocIndex,
					packet->logIsFull);
	
	/*
	*	If the sync packet arrives a few milliseconds before the next tick in
	*	the RTC, the next sync delta will be negative, meaning the local time
	*	is a fraction of a second ahead of the gateway.  Negative values can
	*	be ignored.  The test is only for when this remote has been in a deep
	*	sleep for more than some period of time, stopping the RTC.
	*/
	int32_t	delta = (int32_t)packet->time-(int32_t)UnixTime::Time();
	UnixTime::SetTime(packet->time);
	/*
	*	If waking from a deep sleep THEN
	*	reset the sleep time.
	*
	*	If this isn't done the remote will immediately go into a light sleep.
	*/
	if (delta > 60)
	{
		if (mMode == eGatewaySyncMode)
		{
			mMode = eInfoMode;
		}
		UnixTime::ResetSleepTime();
	}
	
#ifdef SUPPORT_LOC_SEL_MODES
	/*
	*	Switch modes as needed based on the sync information.
	*/
	switch (mMode)
	{
		case eStartLocSelMode:
		case eEndLocSelMode:
			if (mHikeLog->Active())
			{
				mMode = eInfoMode;
			}
			break;
	}
#endif
	if (mHikeLog->StartingLocNeedsUpdate())
	{
		QueueLocnIndexPacket(Log::kGetLocation, packet->startLocIndex);
	}
	if (mHikeLog->EndingLocNeedsUpdate())
	{
		QueueLocnIndexPacket(Log::kGetLocation, packet->endLocIndex);
	}

}

/***************************** QueueRequestPacket ******************************/
void RemoteLogAction::QueueRequestPacket(
	uint32_t	inMessage)
{
	Log::SPacket*	packet = (Log::SPacket*)AllocQueuePacketEntry();
	if (packet)
	{
		packet->message = inMessage;
	}
}

/**************************** QueueLocnIndexPacket ****************************/
void RemoteLogAction::QueueLocnIndexPacket(
	uint32_t	inMessage,
	uint16_t	inLocIndex)
{
	Log::SLocnIndexPacket*	packet = (Log::SLocnIndexPacket*)AllocQueuePacketEntry();
	if (packet)
	{
		packet->message = inMessage;
		packet->locIndex = inLocIndex;
	}
}

/****************************** QueueTimePacket *******************************/
/*
*	Called to either start or stop the log.
*/
void RemoteLogAction::QueueTimePacket(
	uint32_t	inMessage,
	time32_t	inTime)
{
	Log::STimePacket*	packet = (Log::STimePacket*)AllocQueuePacketEntry();
	if (packet)
	{
		packet->message = inMessage;
		packet->time = inTime;
	}
}

/*************************** AllocQueuePacketEntry ****************************/
uint8_t* RemoteLogAction::AllocQueuePacketEntry(void)
{
	uint8_t*	queueEntry = NULL;
	if (mPacketsInQueue < 4)
	{
		queueEntry = &mPacketQueue[mPacketQueueTail];
		mPacketQueueTail = (mPacketQueueTail + 8) % 32;
		mPacketsInQueue++;
	/*
	*	Else the queue is full
	*	For now just ignore/trash the packet send request
	*/
	}
	return(queueEntry);
}

/**************************** SendPacketIfNotBusy *****************************/
bool RemoteLogAction::SendPacketIfNotBusy(void)
{
	if (!mBMP280Period.Passed() &&
		mPacketTimoutPeriod.Passed())
	{
		if (mWaitingForPacket)
		{
			mWaitingForPacket--;
			/*
			*	If there are no more retries AND
			*	there are packets in the queue THEN
			*	remove the current packet, the send failed.
			*/
			if (!mWaitingForPacket &&
				mPacketsInQueue)
			{
				mPacketsInQueue--;
				mPacketQueueHead = (mPacketQueueHead + 8) % 32;
				mPacketTimeouts++;
			}
		}
		/*
		*	If not waiting for a packet AND
		*	there are still packets to send THEN
		*	send the current packet
		*/
		if (!mWaitingForPacket &&
			mPacketsInQueue)
		{
			mWaitingForPacket = 2;	// retries
		}
		if (mWaitingForPacket)
		{
			mRadio->send(1, &mPacketQueue[mPacketQueueHead], 8, true);
			mPacketTimoutPeriod.Start();
			/*
			*	Immediately put the radio in receive mode after calling send.
			*	Calling send puts the radio in standby mode.  Calling
			*	receiveDone when the radio is in standby will ONLY turn on the
			*	receiver and always return false.  The receiver must be turned
			*	on here because the gateway could respond before the next
			*	CheckRadioForPackets call.  The delay in calling
			*	CheckRadioForPackets could be significant because the display
			*	may need to be updated.
			*/
			mRadio->receiveDone();
		}
	}
	return(mWaitingForPacket != 0);
}

/******************************* HandlePacketRx *******************************/
void RemoteLogAction::HandlePacketRx(void)
{
	mWaitingForPacket = 0;
	if (mPacketsInQueue)
	{
		// Remove the current packet request
		mPacketsInQueue--;
		mPacketQueueHead = (mPacketQueueHead + 8) % 32;
	}
	switch (((Log::SPacket*)RFM69::DATA)->message)
	{
		case Log::kSync:
			HandleSyncPacketRx();
			break;
		case Log::kHikeLocation:
		{
			Log::SLocnPacket*	packet = (Log::SLocnPacket*)RFM69::DATA;
			mHikeLog->UpdateLoc(packet->locIndex, packet->link);
			break;
		}
	}
}