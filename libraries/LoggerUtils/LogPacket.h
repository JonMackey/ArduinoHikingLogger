/*
*	LogPacket.h, Copyright Jonathan Mackey 2019
*	RFM69 Packets used by the HikingLoggerGateway, HikingLoggerRemote, and
*	BMP280Remote sketches
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
#ifndef LogPacket_h
#define LogPacket_h

#include <inttypes.h>
#include "HikeLocations.h"
typedef uint32_t time32_t;

/*
	There are three buttons on the top of the logger remote: Left, Mode, Right
	or L, M, and R.
00:00:00 80%
3901' -40.3°F
00:00:00PM
	Info Mode:	If log is running the elapsed time is displayed and percentage
				remaining on line 1.
				The time on line 2.
				The altitude and temperature on line 3.
				Pressing M moves to log mode
	Log Mode:	if not running options are L "SWAP", R "START" on line 1
					Displays starting location name on line 2
					Displays starting location altitude on line 3
				if running options are L none, R "STOP"
				if paused/stopped, options are L "DONE", R "RESUME"
					Displays ending location name on line 2
					Displays ending location altitude on line 3
				If starting == ending loc, "START == END!" is displayed on
				line 1, and the LR buttons are disabled
				Pressing M when not running changes mode to to starting location
				selection mode.
				Pressing M when running changes the mode to info mode.
				
	Starting Location Selection Mode: Displays "START LOC" centered on line 1
				Displays current starting loc name on line 2
				Displays current starting loc altitude on line 3
				Pressing L moves to and selects previous location
				Pressing R moves to and selects next location
				Pressing M changes the mode to ending location selection mode
	Ending Location Selection Mode: Displays "END LOC" centered on line 1
				Displays current ending loc name on line 2
				Displays current ending loc altitude on line 3
				Pressing L moves to and selects previous location
				Pressing R moves to and selects next location
				Pressing M changes the mode to info mode
	Sync Mode:	You can't enter or leave sync mode by pressing mode when
				syncing.  Once the sync is successful pressing mode will move
				to info mode.
				
				When not synced and not syncing, "SYNC BMP" is displayed on
				line 1. "[LEFT] 2 SYNC" is displayed on line 3.
				
				When sync starts line 3 displays "SYNCING BMP"
				If successful line 2 displays "BMP SYNCD" and
				line 3 displays "[MODE] 2 EXIT"
				
	Handshaking with gateway: The gateway goes into receive mode immediately
				after receiving a packet from the BMP280.  The gateway stays in
				receive mode for up to 1/2 second.  If a remote packet arrives,
				meaning the remote is awake, the gateway will continuously stay
				in receive mode for 30 seconds after each packet is received.
				
				The remote receives a BMP280 packet.  If the remote is awake it
				will request synchronization data from the gatway.
				
*/
namespace Log
{
	struct SPacket
	{
		uint32_t	message;
	};
	
	/*
	*	Broadcast by the BMP280 remote approximately every 4 seconds.
	*	Broadcast = no specific target ID.  Any receiver with the
	*	same network ID can receive this packet.  This packet is used
	*	to synchronize communication between the gateway and the
	*	3 button remote.
	*/
	const uint32_t	kBMP280 = 0x424D5032;	// 'BMP2';
	const int8_t 	kBMP280AcquisitionTime = 10; // in ms, the time before the period ends to start listening for the next packet.
	struct SBMP280Packet : SPacket
	{
		int32_t		temp;		// Degrees Celcius * 100
		uint32_t	pres;		// hPa
	};
	
	/*
	*	Sent to the gateway to request the BMP to sync.
	*	Will be ignored if in the process of syncing or the gateway is
	*	already synced.
	*/
	const uint32_t	kSyncBMP280 = 0x424D5053;	// 'BMPS';

	/*
	*	Sent to the gateway to request synchronization data.
	*	The reply is a SSyncPacket.
	*/
	const uint32_t	kGetSync = 0x4753594E;	// 'GSYN';

	/*
	*	Sent by the gateway in response to a get sync request.
	*	All location indexes are the unsorted physical record index.
	*/
	const uint32_t	kSync = 0x53594E43;	// 'SYNC';
	struct SSyncPacket : SPacket
	{
		time32_t	time;		// Unix time
		time32_t	startTime;	// 0 if no active log session
		time32_t	endTime;	// 0 if running, else time stopped/paused
		uint16_t	startLocIndex; // Unsorted physical record index
		uint16_t	endLocIndex; // Unsorted physical record index
		bool		logIsFull;
	};

	/*
	*	Sent to the gateway to request location name and elevation.
	*/
	const uint32_t	kGetLocation = 0x474C4F43;	// 'GLOC';
	struct SLocnIndexPacket : SPacket
	{
		uint16_t		locIndex;	// Unsorted physical record index
	};

	/*
	*	Sent by gateway in response to a get location or set start/end location
	*	index request. The indexes are unsorted physical record indexes.
	*/
	const uint32_t	kHikeLocation = 0x484C4F43;	// 'HLOC';
	struct SLocnPacket : SPacket
	{
		uint16_t			locIndex;
		SHikeLocationLink	link;	// See HikeLocations.h
	};

	/*
	*	Sent to gateway to set the start or end unsorted physical record index.
	*	The gateway responds with a location packet OR a sync packet if the
	*	log is active.  Both use SLocnIndexPacket
	*/
	const uint32_t	kSetStartLocation = 0x53455453;	// 'SETS';
	const uint32_t	kSetEndLocation = 0x53455445;	// 'SETE';

	/*
	*	Sent to gateway to start or resume the current log session.
	*/
	const uint32_t	kStartLog = 0x53545254;	// 'STRT';
	struct STimePacket : SPacket
	{
		time32_t		time;
	};

	/*
	*	Sent to the gateway to stop the current log session.
	*/
	const uint32_t	kStopLog = 0x53544F50;	// 'STOP';
	// uses STimePacket

	/*
	*	Sent to the gateway to end the currently stopped log session.
	*/
	const uint32_t	kEndLog = 0x454E444C;	// 'ENDL';

	/*
	*	Sent to the gateway to swap the start and end locations.
	*/
	const uint32_t	kSwapLocIndexes = 0x53574150;	// 'SWAP';
}				


#endif // LogPacket_h
