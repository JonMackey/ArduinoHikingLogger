/*
*	MP3YX5200.h
*	Copyright (c) 2019 Jonathan Mackey
*
*	Minimal class to control the YX5200-24SS chip (aka DFPlayer module.)
*	These two classes can only be used with HardwareSerial.  My earlier DFPlayer
*	class, which MP3YX5200 is based on, can be used with either hardware or
*	software serial.  The reason for the split is that HardwareSerial needs to
*	be disconnected from its pins in order to pull them low when going to sleep.
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
*/
#ifndef MP3YX5200_H
#define MP3YX5200_H

#include <HardwareSerial.h>
#include "MSPeriod.h"

class MP3YX5200
{
public:
	enum EDFPlayerCommands
	{
		ePlayTrackCmd			= 3,
		eSetVolumeCmd			= 6,
		eStopPlayCmd			= 0x16,
		eQueryNumFlashFilesCmd	= 0x49
	};
							MP3YX5200(
								HardwareSerial&			inSerial);
	virtual void			begin(void);

	void					SendCommand(
								uint8_t					inCommand,
								uint16_t				inParam,
								bool					inWantsReply = false);
							/*
							*	Contrary to the documentation, the index is
							*	1 to N, not 0 to N
							*/
	inline void				PlayNthRootFile(
								uint16_t				inIndex)
								{SendCommand(ePlayTrackCmd, inIndex);}
	bool					CommandCompleted(void);
	uint8_t					GetCommand(void) const
								{return(mReplyCommand);}
	uint16_t				GetParam(void) const
								{return(mParam);}
	void					ClearReplyCommand(void)
								{mReplyCommand = 0;}
	bool					WaitForCommandCompeted(
								uint32_t				inTimeout);
	void					SetVolume(
								uint8_t					inVolume);	// 0 to 30
protected:
	static uint8_t	sPacket[];
	uint8_t			mRingBuffer[10];
	uint8_t			mRingIndex;
	uint8_t			mReplyCommand;
	uint16_t		mParam;
	HardwareSerial&	mSerial;
	static const uint32_t	kBaudRate;
	
	static uint16_t			CalculateChecksum(
								uint8_t*				inBuffer);
	static void				SerializeUInt16(
								uint16_t				inUint16,
								uint8_t*				outArray);
	uint8_t					GetParamsFromRingBuffer(
								uint16_t&				outParam);
};

class MP3YX5200WithSleep : public MP3YX5200
{
public:
							MP3YX5200WithSleep(
								HardwareSerial&			inSerial,
								uint8_t					inRxPin,
								uint8_t					inTxPin,
								uint8_t					inPowerPin);
	virtual void			begin(void);
	void					Sleep(void);
	void					SleepIfDonePlaying(void);
	bool					DonePlaying(void);
	void					WakeUp(void);
							// Asynchronous, returns immediately.
	void					Play(
								uint8_t					inMP3Index,
								uint32_t				inTimeout = 5000);
	bool					Awake(void) const
								{return(mAwake);}
protected:
	uint8_t		mRxPin;
	uint8_t		mTxPin;
	uint8_t		mPowerPin;
	bool		mAwake;
	MSPeriod	mTimeout;
};

#endif