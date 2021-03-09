/*
*	HikingLoggerGateway.ino, Copyright Jonathan Mackey 2019
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
*	This code uses Felix Rusu's RFM69 library.
*	Copyright Felix Rusu 2016, http://www.LowPowerLab.com/contact
*
*	This code uses Bill Greiman's SdFat library.
*	Copyright Bill Greiman 2011-2018
*/
#include <avr/sleep.h>
#include <SPI.h>
#include "TFT_ST7789.h"
#include "RFM69.h"    // https://github.com/LowPowerLab/RFM69

#include "ATmega644RTC.h"
#include "LogTempPres.h"
#include "LogUI.h"
#include "HikeLocations.h"
#include "HikeLog.h"
#include <Wire.h>
#include "AT24CDataStream.h"
#include "AT24C.h"
#include "HikingLoggerConfig.h"

#ifdef USE_EXTERNAL_RTC
#include "DS3231SN.h"
#else
#include "CompileTime.h"
#endif

const uint8_t kAT24CDeviceAddr = 0x50;		// Serial EEPROM
const uint8_t kAT24CDeviceCapacity = 32;	// Value at end of AT24Cxxx xxx/8
AT24C	at24C(kAT24CDeviceAddr, kAT24CDeviceCapacity);
const uint32_t	kHikeLocationsSize = 0x400; // 38 maximum (39 -1, -1 for the root)
const uint32_t	kHikeLogSize = ((uint32_t)kAT24CDeviceCapacity * 1024) - kHikeLocationsSize;	// Rest of space for logs
AT24CDataStream locationsDataStream(&at24C, 0, kHikeLocationsSize);
AT24CDataStream logDataStream(&at24C, (const void*)kHikeLocationsSize, kHikeLogSize);

TFT_ST7789	display(Config::kDCPin, Config::kResetPin, Config::kCDPin, Config::kBacklightPin, 240, 240);
LogUI		logUI;
// Define "xFont" to satisfy the auto-generated code with the font files
// This implementation uses logUI as a subclass of xFont
#define xFont logUI
#include "MyriadPro-Regular_18.h"
#include "MyriadPro-Regular_36_1b.h"

HikeLog		hikeLog;

#ifdef USE_EXTERNAL_RTC
DS3231SN	externalRTC;
#endif


/*********************************** setup ************************************/
void setup(void)
{
	Serial.begin(BAUD_RATE);
	//Serial.println(F("Starting..."));
	SPI.begin();
	Wire.begin();
#ifdef USE_EXTERNAL_RTC
	externalRTC.begin();
	ATmega644RTC::RTCInit(0, &externalRTC);
	pinMode(Config::kRTCIntPin, INPUT_PULLUP);	// Not used.
	UnixTime::SetTimeFromExternalRTC();
#else
	ATmega644RTC::RTCInit(UNIX_TIMESTAMP + 32);
#endif

	// pull-up all unused pins to save power
	pinMode(Config::kUnusedPinA0, INPUT_PULLUP);
	pinMode(Config::kUnusedPinA1, INPUT_PULLUP);
	pinMode(Config::kUnusedPinA2, INPUT_PULLUP);
	pinMode(Config::kUnusedPinA3, INPUT_PULLUP);
	pinMode(Config::kUnusedPinA4, INPUT_PULLUP);
	pinMode(Config::kUnusedPinA5, INPUT_PULLUP);
	pinMode(Config::kUnusedPinB0, INPUT_PULLUP);
	
	display.begin(Config::kDisplayRotation); // Init TFT
	display.Fill();
	/*
	*	The location names and elevations are stored on EEPROM.
	*	Calling begin initialized the hikeLocations instance by counting the
	*	number of locations on the associated stream.
	*/
	HikeLocations::GetInstance().Initialize(&locationsDataStream, Config::kSDSelectPin);
	hikeLog.Initialize(&logDataStream, Config::kSDSelectPin);

	UnixTime::ResetSleepTime();
	logUI.begin(&hikeLog, &display, &MyriadPro_Regular_36_1b::font,
												&MyriadPro_Regular_18::font);
}

/************************************ loop ************************************/
void loop(void)
{
	logUI.Update();
}

