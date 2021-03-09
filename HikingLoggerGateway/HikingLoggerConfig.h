/*
*	HikingLoggerConfig.h, Copyright Jonathan Mackey 2021
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
#ifndef HikingLoggerConfig_h
#define HikingLoggerConfig_h

#include <inttypes.h>

#define BAUD_RATE	19200
#define LOGGER_VER	12	// v1.2
#define USE_EXTERNAL_RTC

/*
*	IMPORTANT RADIO SETTINGS
*/
//Frequency of the RFM69 module:
#define FREQUENCY         RF69_433MHZ
//#define FREQUENCY         RF69_868MHZ
//#define FREQUENCY         RF69_915MHZ

namespace Config
{
	const uint8_t	kUnusedPinB0		= 0;	// PB0
	const uint8_t	kSDDetectPin		= 1;	// PB1
	const uint8_t	kRadioIRQPin		= 2;	// PB2
	const uint8_t	kSDSelectPin		= 3;	// PB3	t
	const uint8_t	kRadioNSSPin		= 4;	// PB4	t
	const uint8_t	kMOSI				= 5;	// PB5
	const uint8_t	kMISO				= 6;	// PB6
	const uint8_t	kSCK				= 7;	// PB7
	
	const uint8_t	kRxPin				= 8;	// PD0
	const uint8_t	kTxPin				= 9;	// PD1
	const uint8_t	kMP3RxPin			= 10;	// PD2
	const uint8_t	kMP3TxPin			= 11;	// PD3
	const uint8_t	kMP3PowerPin		= 12;	// PD4	t
	const uint8_t	kUpBtnPin			= 13;	// PD5	PCINT29	t
	const int8_t	kEnterBtnPin		= 14;	// PD6	PCINT30 t
	const uint8_t	kRightBtnPin		= 15;	// PD7	PCINT31 t

	const uint8_t	kSCL				= 16;	// PC0
	const uint8_t	kSDA				= 17;	// PC1
	const uint8_t	kDCPin				= 18;	// PC2
	const uint8_t	kCDPin				= 19;	// PC3	(Display select)
	const uint8_t	kLeftBtnPin			= 20;	// PC4	PCINT20
	const uint8_t	kDownBtnPin			= 21;	// PC5	PCINT21
#ifdef USE_EXTERNAL_RTC
	const uint8_t	k32KHzPin			= 22;	// PC6	PCINT22, 32KHz from RTC
	const uint8_t	kRTCIntPin			= 23;	// PC7	PCINT23, RTC interrupt
#else
	const uint8_t	kRTCTimer0Pin		= 22;
	const uint8_t	kRTCTimer1Pin		= 23;
#endif
	const int8_t	kUnusedPinA0		= 24;	// PA0
	const uint8_t	kUnusedPinA1		= 25;	// PA1
	const uint8_t	kUnusedPinA2		= 26;	// PA2
	const uint8_t	kUnusedPinA3		= 27;	// PA3	PCINT3
	const uint8_t	kUnusedPinA4 		= 28;	// PA4	PCINT4
	const uint8_t	kUnusedPinA5		= 29;	// PA5	PCINT5
	const uint8_t	kResetPin			= 30;	// PA6	PCINT6
	const uint8_t	kBacklightPin		= 31;	// PA7	PCINT7

	const uint8_t	kDisplayRotation	= 2;	// 180
	const uint8_t	kEnterBtn			= _BV(PIND6);
	const uint8_t	kRightBtn			= _BV(PIND7);
	const uint8_t	kDownBtn			= _BV(PINC4);	// offset by 1, see note in Update code
	const uint8_t	kUpBtn				= _BV(PIND5);
	const uint8_t	kLeftBtn			= _BV(PINC3);	// offset by 1, see note in Update code
	const uint8_t	kPINCBtnMask = (_BV(PINC4) | _BV(PINC5));
	const uint8_t	kPINDBtnMask = (_BV(PIND5) | _BV(PIND6) | _BV(PIND7));

	/*
	*	EEPROM usage, 2K bytes (assumes ATmega644PA)
	*
	*	[0]		uint8_t		networkID;
	*	[1]		uint8_t		nodeID;
	*	[2]		uint8_t		unassigned[2];
	*
	*	Rest used by HikeLog
	*	[4]		uint16_t	lastStartingLocIndex;
	*	[6]		uint16_t	lastEndingLocIndex;
	*	[8]		uint8_t		logInitialized;
	*	[9]		uint8_t		unassigned2[23];
	*
	*	Storage of the last n hikes, circular storage, 16 byte struct
	*	[32]	uint16_t	lastHikesHead
	*	[34]	uint16_t	lastHikesTail
	*	[36]	uint8_t		unassigned3[2];
	*	[38]	SHikeSummary hikes[125];
	*	[2038]	uint8_t		unassigned4[10];
	*/

	const uint16_t	kFlagsAddr			= 0;
	const uint8_t	k12HourClockBit		= 0;
	const uint8_t	kEnableSleepBit		= 1;
	const uint8_t	kOnlyUseISPBit		= 2;
	const uint16_t	kISP_SPIClockAddr	= 1;


	const uint8_t	kTextInset			= 3; // Makes room for drawing the selection frame
	const uint8_t	kTextVOffset		= 6; // Makes room for drawing the selection frame
	// To make room for the selection frame the actual font height in the font
	// file is reduced.  The actual height is kFontHeight.
	const uint8_t	kFontHeight			= 43;
	const uint8_t	kDisplayWidth		= 240;
}

#endif // HikingLoggerConfig_h

