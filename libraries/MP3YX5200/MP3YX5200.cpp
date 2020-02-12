/*
*	MP3YX5200.cpp
*	Copyright (c) 2018 Jonathan Mackey
*
*	Minimal class to control the MP3YX5200 module / YX5200-24SS chip.
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
#include <Arduino.h>
#include "MP3YX5200.h"

enum EYX5200PacketOffsets
{
	ePacketStart,
	ePacketVersion,
	ePacketLength,
	ePacketCommand,
	ePacketWantsReply,
	ePacketParam,
	ePacketParamLow,
	ePacketChecksum,
	ePacketChecksumLow,
	ePacketEnd,
	ePacketSize
};

// MP3YX5200 packet
uint8_t MP3YX5200::sPacket[] = {0x7E, 0xFF, 06, 00, 00, 00, 00, 00, 00, 0xEF};
const uint32_t	MP3YX5200::kBaudRate = 9600;

/********************************* MP3YX5200 ***********************************/
MP3YX5200::MP3YX5200(
	HardwareSerial&	inSerial)
	: mSerial(inSerial), mRingIndex(0), mReplyCommand(0)
{

}

/*********************************** begin ************************************/
void MP3YX5200::begin(void)
{
	mSerial.begin(kBaudRate);
}

/****************************** CommandCompleted ******************************/
/*
*	Returns true when the last issued command completes.
*/
bool MP3YX5200::CommandCompleted(void)
{
	if (mSerial.available())
	{
		uint8_t		thisByte = mSerial.read();
		uint16_t	param;
		mRingBuffer[mRingIndex++] = thisByte;
		mRingIndex %= sizeof(mRingBuffer);
		if (thisByte == 0xEF)
		{
			mReplyCommand = GetParamsFromRingBuffer(mParam);
		}
	}
	return(mReplyCommand != 0);
}

/****************************** CalculateChecksum *****************************/
uint16_t MP3YX5200::CalculateChecksum(
	uint8_t*	inBuffer)
{
	return(-(0x105 + inBuffer[ePacketCommand]
				 + inBuffer[ePacketWantsReply]
				 + inBuffer[ePacketParam]
				 + inBuffer[ePacketParamLow]));
}

/************************** GetParamsFromRingBuffer ***************************/
uint8_t MP3YX5200::GetParamsFromRingBuffer(
	uint16_t&	outParam)
{
	uint8_t	command = 0;
	// Quick sanity check to see if 9 bytes earlier there is a 7E
	if (mRingBuffer[mRingIndex] == 0x7E)
	{
		command = mRingBuffer[(mRingIndex+ePacketCommand)%sizeof(mRingBuffer)];
		uint8_t wantsReply = mRingBuffer[(mRingIndex+ePacketWantsReply)%sizeof(mRingBuffer)];
		uint8_t paramH = mRingBuffer[(mRingIndex+ePacketParam)%sizeof(mRingBuffer)];
		uint8_t paramL = mRingBuffer[(mRingIndex+ePacketParamLow)%sizeof(mRingBuffer)];
		uint16_t expectedChecksum = -(0x105 + command  + wantsReply + paramH + paramL);
		uint16_t actualChecksum = (mRingBuffer[(mRingIndex+ePacketChecksum)%sizeof(mRingBuffer)] << 8) + mRingBuffer[(mRingIndex+ePacketChecksumLow)%sizeof(mRingBuffer)];
		if (expectedChecksum == actualChecksum)
		{
			outParam = (paramH << 8) + paramL;
		} else
		{
			command = 0;
		}
		mRingBuffer[mRingIndex] = 0;	// We've already looked at this response
	}
	return(command);
}

/******************************* SerializeUInt16 ******************************/
void MP3YX5200::SerializeUInt16(
	uint16_t	inUint16,
	uint8_t*	outArray)
{
	outArray[0] = (uint8_t)(inUint16 >> 8);
	outArray[1] = (uint8_t)inUint16;
}

/******************************** SendCommand *********************************/
void MP3YX5200::SendCommand(
	uint8_t inCommand,
	uint16_t inParam,
	bool	inWantsReply)
{
	while (mSerial.available())
	{
		mSerial.read();
	}
	mReplyCommand = 0;	// Used to detect when the command is completed.
	sPacket[ePacketCommand] = inCommand;
	sPacket[ePacketWantsReply] = inWantsReply;
	SerializeUInt16(inParam, &sPacket[ePacketParam]);
	SerializeUInt16(CalculateChecksum(sPacket), &sPacket[ePacketChecksum]);
	mSerial.write(sPacket, sizeof(sPacket));
}

/*************************** WaitForCommandCompeted ***************************/
/*
*	Block until command completes or timeout whichever occurs first.
*/
bool MP3YX5200::WaitForCommandCompeted(
	uint32_t	inTimeout)
{
	MSPeriod	timeoutPeriod(inTimeout);
	timeoutPeriod.Start();
	while (CommandCompleted() == false)
	{
		if (!timeoutPeriod.Passed())
		{
			continue;
		}
		// timeout occured
		return(false);
	}
	return(true);
}

/********************************* SetVolume **********************************/
void MP3YX5200::SetVolume(
	uint8_t	inVolume)
{
	SendCommand(eSetVolumeCmd, inVolume, true);
	WaitForCommandCompeted(100);
}

#pragma mark -
/****************************** MP3YX5200WithSleep *****************************/
MP3YX5200WithSleep::MP3YX5200WithSleep(
	HardwareSerial&	inSerial,
	uint8_t			inRxPin,
	uint8_t			inTxPin,
	uint8_t			inPowerPin)
	: MP3YX5200(inSerial), mRxPin(inRxPin), mTxPin(inTxPin), mPowerPin(inPowerPin)
{
}

/*********************************** begin ************************************/
void MP3YX5200WithSleep::begin(void)
{
	MP3YX5200::begin();
	pinMode(mPowerPin, OUTPUT);
	Sleep();
}

/*********************************** Sleep ************************************/
void MP3YX5200WithSleep::Sleep(void)
{
	/*
	*	Release the serial pins (otherwise pinMode and digitalWrite have no
	*	effect.)
	*/
	mSerial.end();
	// Set both serial pins low so power doesn't backfeed to the MP3 section.
	pinMode(mRxPin, INPUT);
	digitalWrite(mRxPin, LOW);
	pinMode(mTxPin, INPUT);
	digitalWrite(mTxPin, LOW);
	digitalWrite(mPowerPin, HIGH);	// Turn off power to MP3 section
	mAwake = false;
}

/***************************** SleepIfDonePlaying *****************************/
void MP3YX5200WithSleep::SleepIfDonePlaying(void)
{
	if (DonePlaying())
	{
		Sleep();
	}
}

/******************************** DonePlaying *********************************/
/*
*	DonePlaying is actually CommandCompleted with a timeout and a flag.  It's
*	named DonePlaying because of the context in which it's generally used.
*/
bool MP3YX5200WithSleep::DonePlaying(void)
{
	return(mAwake && (CommandCompleted() || mTimeout.Passed()));
}

/*********************************** WakeUp ***********************************/
void MP3YX5200WithSleep::WakeUp(void)
{
	MP3YX5200::begin();
	/*
	*	Serial pins are high when idling.  Insert a small delay between
	*	enabling the serial pins and powering the MP3 section.  This small delay
	*	allows the serial pins (now high) to backfeed enough power to the MP3
	*	section capacitors so that when the MP3 section is powered on it doesn't
	*	cause a brown out reset.
	*/
	delay(2);
	digitalWrite(mPowerPin, LOW);	// Turn on MP3 section
	ClearReplyCommand();
	//SendCommand(eStopPlayCmd, 0, true);	// For debugging, so the startup time can be measured
												// using the MP3YX5200Debug sketch.  MP3YX5200Debug will
												// report the time between command/responses.

	/*
	*	Wait up to 2 seconds for the player to initialize
	*/
	WaitForCommandCompeted(2000);
	 
	/*
	*	When power is applied the MP3 chip will try to start playing.
	*	Immediately send a stop command once initialization completes.
	*/
	SendCommand(eStopPlayCmd, 0, true);
	WaitForCommandCompeted(50);
	mAwake = true;
}

/************************************ Play ************************************/
void MP3YX5200WithSleep::Play(
	uint8_t		inMP3Index,
	uint32_t	inTimeout)
{
	if (!mAwake)
	{
		WakeUp();
	}
	PlayNthRootFile(inMP3Index);
	mTimeout.Set(inTimeout);
	mTimeout.Start();
}


