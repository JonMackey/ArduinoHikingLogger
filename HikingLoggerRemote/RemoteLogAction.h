/*
*	RemoteLogAction.h, Copyright Jonathan Mackey 2019
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
#ifndef RemoteLogAction_h
#define RemoteLogAction_h

#include <inttypes.h>
#include "MSPeriod.h"
typedef uint32_t time32_t;
class RFM69;
class RemoteHikeLog;
//#define SUPPORT_LOC_SEL_MODES 1

class RemoteLogAction
{
public:
							RemoteLogAction(void);
	enum EMode
	{
		eInfoMode,
		eLogMode,
	#ifdef SUPPORT_LOC_SEL_MODES
		eStartLocSelMode,
		eEndLocSelMode,
	#endif
		eBMP280SyncMode,
		eGatewaySyncMode
	};
	
	enum ESyncState
	{
		eBMP280SyncError,
		eBMP280Syncing,
		eBMP280SyncSuccess
	};
	void					Initialize(
								RFM69*					inRadio,
								RemoteHikeLog*			inHikeLog);
	void					Initialize(void);

	void					CheckRadioForPackets(
								bool					inDisplayIsOff);
	void					SyncWithBMP280Remote(void);
		
	uint8_t					Mode(void) const
								{return(mMode);}
	void					GoToInfoMode(void);
			
	uint8_t					SyncState(void) const
								{return(mSyncState);}
							/*
							*	Busy returns true when the BMP280 packet is expected soon OR
							*	a BMP280 sync is taking place.
							*/
	bool					Busy(void) const
								{return(mBMP280Period.Passed() ||
									mSyncState == eBMP280Syncing);}
	uint8_t					PacketsInQueue(void) const
								{return(mPacketsInQueue);}

	void					LeftButtonPressed(void);
	void					RightButtonPressed(void);
	void					ModeButtonPressed(void);
	
	uint16_t				PacketTimeouts(void) const
								{return(mPacketTimeouts);}
	uint8_t					WaitingForPacket(void) const
								{return(mWaitingForPacket);}
protected:
	MSPeriod		mBMP280Period;
	MSPeriod		mPacketTimoutPeriod;
	RFM69*			mRadio;
	RemoteHikeLog*	mHikeLog;
	uint8_t			mMode;
	uint8_t			mSyncState;
	/*
	*	FIFO queue of packets to be sent.
	*/
	uint8_t		mWaitingForPacket;	// this is also the retry countdown
	uint8_t		mPacketQueue[4*8];	// 4 element queue, 8 bytes per element.
	uint8_t		mPacketsInQueue;
	uint8_t		mPacketQueueHead;
	uint8_t		mPacketQueueTail;
	uint16_t	mPacketTimeouts;
	
	bool					HandleBMP280PacketRx(void);
	void					HandlePacketRx(void);
	void					HandleSyncPacketRx(void);
	void					QueueRequestPacket(
								uint32_t				inMessage);
	void					QueueLocnIndexPacket(
								uint32_t				inMessage,
								uint16_t				inLocIndex);
	void					QueueTimePacket(
								uint32_t				inMessage,
								time32_t				inTime);
	uint8_t*				AllocQueuePacketEntry(void);
	bool					SendPacketIfNotBusy(void);	// (if any)
};

#endif // RemoteLogAction_h
