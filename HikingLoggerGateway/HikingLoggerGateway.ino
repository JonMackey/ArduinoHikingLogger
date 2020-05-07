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
#include <EEPROM.h>
#include "TFT_ST7789.h"
#include "RFM69.h"    // https://github.com/LowPowerLab/RFM69

#include "LogDateTime.h"
#include "LogTempPres.h"
#include "LogLayout.h"
#include "LogAction.h"
#include "HikeLocations.h"
#include "HikeLog.h"
#include "MSPeriod.h"
#include <Wire.h>
#include "AT24CDataStream.h"
#include "AT24C.h"
#include "MP3YX5200.h"	// MP3 player

#define USE_EXTERNAL_RTC
#ifdef USE_EXTERNAL_RTC
#include "DS3231SN.h"
#else
#include "CompileTime.h"
#endif

//#define DEBUG_BMP280	1

#define BAUD_RATE	19200
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


/*
*	IMPORTANT RADIO SETTINGS
*/
//Frequency of the RFM69 module:
#define FREQUENCY         RF69_433MHZ
//#define FREQUENCY         RF69_868MHZ
//#define FREQUENCY         RF69_915MHZ

const uint8_t kRadioNSSPin = 4;	// PB4
const uint8_t kRadioIRQPin = 2;	// PB2	INT2
RFM69 radio(kRadioNSSPin, kRadioIRQPin);

const uint8_t kRxPin = 8;			// PD0
const uint8_t kTxPin = 9;			// PD1
const uint8_t kMP3RxPin = 10;		// PD2
const uint8_t kMP3TxPin = 11;		// PD3

const uint8_t kSDDetectPin = 1;		// PB1	PCINT9
const uint8_t kSDSelectPin = 3;		// PB3

const uint8_t kDCPin = 18;			// PC2
const int8_t kResetPin = 30;		// PA6
const int8_t kBacklightPin = 31;	// PA7
const int8_t kCDPin = 19;			// PC3	(Display select)

const int8_t kMP3PowerPin = 12;		// PD4

const uint8_t kUpBtnPin = 13;		// PD5	PCINT29
const uint8_t kLeftBtnPin = 20;		// PC4	PCINT20
const uint8_t kEnterBtnPin = 14;	// PD6	PCINT30
const uint8_t kRightBtnPin = 15;	// PD7	PCINT31
const uint8_t kDownBtnPin = 21;		// PC5	PCINT21
const uint8_t kPINCBtnMask = (_BV(PINC4) | _BV(PINC5));
const uint8_t kPINDBtnMask = (_BV(PIND5) | _BV(PIND6) | _BV(PIND7));

const int8_t kUnusedPinA0 = 24;		// PA0
const int8_t kUnusedPinA1 = 25;		// PA1
const int8_t kUnusedPinA2 = 26;		// PA2
const int8_t kUnusedPinA3 = 27;		// PA3
const int8_t kUnusedPinA4 = 28;		// PA4
const int8_t kUnusedPinA5 = 29;		// PA5
const int8_t kUnusedPinB0 = 0;		// PB0, Marked DS on board

#define DEBOUNCE_DELAY	20	// ms
static bool		sSleeping;	// the board is sleeping, the mcu may be awake.
static uint8_t	sStartPinsState;
static uint8_t	sLastPinsState;
static bool		sButtonPressed = false;
static bool		sSDInsertedOrRemoved = false;
static bool		sUpdateAll;

MSPeriod	debouncePeriod(DEBOUNCE_DELAY);	// For buttons

const uint8_t kAT24CDeviceAddr = 0x50;		// Serial EEPROM
const uint8_t kAT24CDeviceCapacity = 32;	// Value at end of AT24Cxxx xxx/8
AT24C	at24C(kAT24CDeviceAddr, kAT24CDeviceCapacity);
const uint32_t	kHikeLocationsSize = 0x400; // 38 maximum (39 -1, -1 for the root)
const uint32_t	kHikeLogSize = ((uint32_t)kAT24CDeviceCapacity * 1024) - kHikeLocationsSize;	// Rest of space for logs
AT24CDataStream locationsDataStream(&at24C, 0, kHikeLocationsSize);
AT24CDataStream logDataStream(&at24C, (const void*)kHikeLocationsSize, kHikeLogSize);

TFT_ST7789	display(kDCPin, kResetPin, kCDPin, kBacklightPin, 240, 240);
LogLayout	logLayout;
// Define "xFont" to satisfy the auto-generated code with the font files
// This implementation uses logLayout as a subclass of xFont
#define xFont logLayout
#include "MyriadPro-Regular_36.h"
#include "MyriadPro-Regular_18.h"

LogAction	logAction;
HikeLog		hikeLog;
MP3YX5200WithSleep mp3Player(Serial1, kMP3RxPin, kMP3TxPin, kMP3PowerPin);

#ifdef USE_EXTERNAL_RTC
//const uint8_t kDS3231DeviceAddr = 0x68;	// RTC
const uint8_t k32KHzInputPin = 22;		// PC6	PCINT22, 32KHz from RTC
const uint8_t kRTCIntPin = 23;			// PC7	PCINT23, RTC interrupt
DS3231SN	externalRTC;
#endif


/*********************************** setup ************************************/
void setup(void)
{
	Serial.begin(BAUD_RATE);
	//Serial.println(F("Starting..."));
	mp3Player.begin();

	SPI.begin();
	Wire.begin();
#ifdef USE_EXTERNAL_RTC
	externalRTC.begin();
	LogDateTime::RTCInit(0, &externalRTC);
	pinMode(kRTCIntPin, INPUT_PULLUP);
	LogDateTime::SetTimeFromExternalRTC();
#else
	LogDateTime::RTCInit(UNIX_TIMESTAMP + 32);
#endif

	pinMode(kSDDetectPin, INPUT_PULLUP);
	pinMode(kSDSelectPin, OUTPUT);
	digitalWrite(kSDSelectPin, HIGH);	// Deselect the SD card.


	pinMode(kUpBtnPin, INPUT_PULLUP);
	pinMode(kLeftBtnPin, INPUT_PULLUP);
	pinMode(kEnterBtnPin, INPUT_PULLUP);
	pinMode(kRightBtnPin, INPUT_PULLUP);
	pinMode(kDownBtnPin, INPUT_PULLUP);

	// pull-up all unused pins to save power
	pinMode(kUnusedPinA0, INPUT_PULLUP);
	pinMode(kUnusedPinA1, INPUT_PULLUP);
	pinMode(kUnusedPinA2, INPUT_PULLUP);
	pinMode(kUnusedPinA3, INPUT_PULLUP);
	pinMode(kUnusedPinA4, INPUT_PULLUP);
	pinMode(kUnusedPinA5, INPUT_PULLUP);
	pinMode(kUnusedPinB0, INPUT_PULLUP);

	/*if (MCUSR & _BV(BORF))
	{
		Serial.print(F("MCUSR = "));
		Serial.println(MCUSR, HEX);
	}
	MCUSR = 0;*/
	
	cli();
	ADCSRA &= ~_BV(ADEN);		// Turn off ADC to save power.
	PRR0 |= _BV(PRADC);
	
	/*
	*	Other power saving changes verified:
	*	- On-chip Debug System is disabled - OCDEN and JTAGEN H fuses
	*	- Watchdog timer always on is disabled - WDTON of H fuse
	*/
	
	
	/*
	*	To wake from inserting an SD card, setup pin change interrupt for PB1
	*	on PCIE1.
	*/
	PCMSK1 = _BV(PCINT9);

	/*
	*	To wake from sleep and to respond to button presses, setup pin change
	*	interrupts for the button pins. All of the pins aren't on the same port.
	*	PC4 & PC5 are on PCIE2 and the rest are on PCIE3.
	*/
	PCMSK2 = _BV(PCINT20) | _BV(PCINT21);
	PCMSK3 = _BV(PCINT29) | _BV(PCINT30) | _BV(PCINT31);
	PCICR = _BV(PCIE1) | _BV(PCIE2) | _BV(PCIE3);
	
	sei();
#ifdef USE_EXTERNAL_RTC
#endif

	// Read the network and node IDs from EEPROM
	{
		uint8_t	networkID = EEPROM.read(0);
		uint8_t	nodeID = EEPROM.read(1);
		radio.initialize(FREQUENCY, nodeID, networkID);
		radio.sleep();
		/*
		*	Note that the radio may lock up on the first boot after loading
		*	software.  I haven't figured out why.  Reseting the board by
		*	manually pressing reset or toggling DTR on the serial connection
		*	seems to work (has always worked so far.)
		*	The lockup occurs in the RFM69.cpp code not the RFM69 module itself.
		*	 The RFM69::setMode() function has a while statement that never
		*	exits. Removing the sleep statement above removes this immediate
		*	symptom but the lockup is only delayed till the radio is put to
		*	sleep the first time and then an attempt to call setMode it is made.
		*	Putting a timeout in setMode only eliminates the lockup, the radio
		*	still doesn't communicate.
		*/
	}

	xFont.SetDisplay(&display, &MyriadPro_Regular_36::font);
	display.begin(2); // Init TFT
	display.Fill();
	
	/*
	*	The location names and elevations are stored on EEPROM.
	*	Calling begin initialized the hikeLocations instance by counting the
	*	number of locations on the associated stream.
	*/
	HikeLocations::GetInstance().Initialize(&locationsDataStream);
	hikeLog.Initialize(&logDataStream, kSDSelectPin);
	//Serial.print(F("HikeLocations = "));
	//Serial.println(HikeLocations::GetInstance().GetCount(), DEC);

	LogDateTime::ResetSleepTime();
	logLayout.Initialize(&logAction, &hikeLog,
									&display, &MyriadPro_Regular_36::font,
												&MyriadPro_Regular_18::font);
	logAction.Initialize(&radio, &hikeLog);
	sUpdateAll = true;	// Update all data on display
	sSleeping = false;
	// In case booting with the SD card in
	sSDInsertedOrRemoved = digitalRead(kSDDetectPin) == LOW;
}
#if 1
#ifdef __MACH__
#include <string>
#else
#include <Arduino.h>
#endif
void DumpChar(
	uint8_t	inChar)
{
	uint8_t	valueIn = inChar;
	uint8_t	nMask = 0x80;
	uint8_t	binaryStr[32];
	uint8_t*	buffPtr = binaryStr;
	for (; nMask != 0; nMask >>= 1)
	{
		*(buffPtr++) = (valueIn & nMask) ? '1':'0';
		if (nMask & 0xEF)
		{
			continue;
		}
		*(buffPtr++) = ' ';
	}
	*buffPtr = 0;
#ifdef __MACH__
	fprintf(stderr, "0x%02hhX %s\n", valueIn, binaryStr);
#else
	Serial.print("0x");
	Serial.print(valueIn, HEX);
	Serial.print(" ");
	Serial.println((const char*)binaryStr);
#endif
}
#endif

#ifdef DEBUG_BMP280
MSPeriod	period;
#endif

/************************************ loop ************************************/
void loop(void)
{
//	lock up notes:  The old RFM69 library uses SPI within the ISR.  This is
//	very bad because if the ISR is called while some other SPI transfer is
//	in progress, the RFM69 code will lock up and/or currupt data because it
//	can't get control of the SPI bus.
//	The latest RFM69 library only sets a flag that data is available in the ISR.
//	The next call to receiveDone() checks this flag and retrieves the data.
//

	if (!sSleeping)
	{
		logLayout.Update(sUpdateAll);
		sUpdateAll = false;
		/*
		*	Some action states need to be reflected in the display before
		*	performing an action.
		*/
		logAction.Update();
	}
	
	/*
	*	Check to see if any packets have arrived
	*/
	logAction.CheckRadioForPackets(sSleeping);
	
	mp3Player.SleepIfDonePlaying();
	
	/*
	*	If an altitude milestone has been passed THEN
	*	play a notfication mp3 clip.
	*
	*	Note that PassedMilestone() auto increments to the next milestone.
	*	If a unique mp3 is desired for each milestone, PassedMilestone returns
	*	a uint8_t containing the percentage (1-99, default is 25, 50, 75).
	*/
	if (LogTempPres::GetInstance().PassedMilestone())
	{
		mp3Player.Play(1);
	}
	
	hikeLog.LogEntryIfTime();
	if (!sSleeping)
	{
		if (Serial.available())
		{
			switch (Serial.read())
			{
				case '>':	// Set the time.  A hexadecimal ASCII UNIX time follows
					LogDateTime::SetUnixTimeFromSerial();
					break;
				case '.':	// Dump the summaries ring buffer head and tail
				{
					SRingHeader	header;
					EEPROM.get(32, header);
					Serial.print(F("head = 0x"));
					Serial.print(header.head, HEX);
					Serial.print(F(", tail = 0x"));
					Serial.println(header.tail, HEX);
					SHikeSummary	hikeSummary;
					bool	hasSavedHikes = hikeLog.GetSavedHike(header.tail, hikeSummary);
					if (hasSavedHikes)
					{
						Serial.print(F("\tstartTime =\t0x"));
						Serial.println(hikeSummary.startTime, HEX);
						Serial.print(F("\tendTime =\t0x"));
						Serial.println(hikeSummary.endTime, HEX);
					}
					break;
				}
				case 'm':	// Test/play the first mp3.
				{
					mp3Player.Play(1);
					break;
				}
				case 'M':	// Test/play the second mp3
				{
					mp3Player.Play(2);
					break;
				}
				case '-':	// Reset the hike summaries ring buffer
				{
					SRingHeader	header;
					header.head = 0;
					header.tail = 0;
					EEPROM.put(32, header);
					break;
				}
				case 'd':	// Dump the logs currently on the serial eeprom
				{
					AT24CDataStream dumpStream(&at24C, (const void*)kHikeLocationsSize, kHikeLogSize);
					dumpStream.Seek(0, DataStream::eSeekSet);
					SHikeLogHeader	header;
					while(true)
					{
						if (dumpStream.Read(sizeof(SHikeLogHeader), &header) == sizeof(SHikeLogHeader))
						{
							if (header.startTime)
							{
								Serial.println(F("Log:"));
								Serial.print(F("\tstartTime =\t0x"));
								Serial.println(header.startTime, HEX);
								Serial.print(F("\tendTime =\t0x"));
								Serial.println(header.endTime, HEX);
								Serial.print(F("\tinterval =\t0x"));
								Serial.println(header.interval, HEX);
								Serial.print(F("\tstart loc: "));
								Serial.print(header.start.name);
								Serial.print(F(", "));
								Serial.println(header.start.elevation);
								Serial.print(F("\tend loc: "));
								Serial.print(header.end.name);
								Serial.print(F(", "));
								Serial.println(header.end.elevation);
							} else
							{
								Serial.println(F("End of logs"));
								break;
							}
							SHikeLogEntry	entry;
							while (true)
							{
								if (dumpStream.Read(sizeof(SHikeLogEntry), &entry) == sizeof(SHikeLogEntry))
								{
									if (entry.pressure)
									{
										Serial.print(F("\t\t"));
										Serial.print(entry.pressure, HEX);
										Serial.print(F(", "));
										Serial.println(entry.temperature);
									} else
									{
										Serial.println(F("\t\t--End--"));
										// Rewind to the supposed start of the next log header
										dumpStream.Seek(-sizeof(int16_t), DataStream::eSeekCur);
										break;
									}
								}
							}
						} else
						{
							Serial.println(F("Log read failed."));
							break;
						}
					}
				}
			}
		}
		if (sButtonPressed)
		{
			LogDateTime::ResetSleepTime();
			/*
			*	If a debounce period has passed
			*/
			{
				// PINC result shifted right to avoid conflicts with button bit
				// values on PIND.  In the switch below PINC values are offset
				// by 1.
				uint8_t		pinsState = ((~PIND) & kPINDBtnMask) + (((~PINC) & kPINCBtnMask) >> 1);

				/*
				*	If debounced
				*/
				if (sStartPinsState == pinsState)
				{
					if (debouncePeriod.Passed())
					{
						sButtonPressed = false;
						sStartPinsState = 0;
						switch (pinsState)
						{
							case _BV(PIND5):	// Up button pressed
								logAction.IncrementMode(false);
								break;
							case _BV(PIND6):	// Enter button pressed
								logAction.EnterPressed();
								break;
							case _BV(PINC3):	// Left button pressed (actually C4 but it's shifted right above, so it's C3 here)
								logAction.IncrementValue(false);
								break;
							case _BV(PINC4):	// Down button pressed (actually C5 but it's shifted right above, so it's C4 here)
								logAction.IncrementMode(true);
								break;
							case _BV(PIND7):	// Right button pressed
								logAction.IncrementValue(true);
								break;
							default:
								debouncePeriod.Start();
								break;
						}
					}
				} else
				{
					sStartPinsState = pinsState;
					debouncePeriod.Start();
				}
			}
		} else if (LogDateTime::TimeToSleep())
		{
			GoToSleep();
		}
	/*
	*	Else if a button was pressed while sleeping THEN
	*	determine if it's a valid combination to wake the board up from sleep.
	*/
	} else if (sButtonPressed)
	{
		/*
		*	If the left, down, and right buttons aren't pressed AND
		*	the up and enter buttons are THEN
		*	this is the valid button combination to wakup the board.
		*/
		if ((PINC & kPINCBtnMask) == kPINCBtnMask &&
			(PIND & kPINDBtnMask) == _BV(PIND7))
		{
			if (LogDateTime::TimeToSleep())
			{
				LogDateTime::ResetSleepTime();
				debouncePeriod.Start();
			/*
			*	else if the debounce period has passed THEN
			*	wake the board up.
			*/
			} else if (debouncePeriod.Passed())
			{
				sButtonPressed = false;
				WakeUp();
			}
		/*
		*	Else an invalid button combination was entered
		*/
		} else
		{
			sButtonPressed = false;
			debouncePeriod.Start();
			//sleep_mode();	// Go back to sleep
		}
	} /* else
	{
		sleep_mode();	// Go back to sleep
	}*/
	
	if (sSDInsertedOrRemoved)
	{
		WakeUp();
		sSDInsertedOrRemoved = false;
		LogDateTime::ResetSleepTime();
		logAction.SetSDCardPresent(digitalRead(kSDDetectPin) == LOW);
	}
}

/*********************************** WakeUp ***********************************/
/*
*	Wakup the board from sleep.
*/
void WakeUp(void)
{
	if (sSleeping)
	{
		sSleeping = false;
		display.WakeUp();
		LogDateTime::ResetSleepTime();
		LogTempPres::GetInstance().SetChanged();	// To force a redraw
		sUpdateAll = true;	// Update all data on display
		Serial.begin(BAUD_RATE);
	}
}

/********************************* GoToSleep **********************************/
/*
*	Puts the board to sleep
*/
void GoToSleep(void)
{
	//radio.sleep();
	//wdt_enable(inLength);
	//WDTCSR |= _BV(WDIE);
	
	/*
	*	Release the serial pins (otherwise pinMode and digitalWrite have no
	*	effect.)
	*/
	Serial.end();
	// Set both serial pins low so power doesn't backfeed to the serial chip.
	pinMode(kRxPin, INPUT);
	digitalWrite(kRxPin, LOW);
	pinMode(kTxPin, INPUT);
	digitalWrite(kTxPin, LOW);

	display.Fill();
	display.Sleep();
	logAction.GoToLogMode();
	sSleeping = true;
	//sleep_mode();	// Go to sleep
	//wdt_disable();
}

/************************* Pin change interrupt PCI1 **************************/
/*
*
*	Sets a flag to show that an SD card was just inserted or removed.
*/
ISR(PCINT1_vect)
{
	sSDInsertedOrRemoved = true;
}

/************************* Pin change interrupt PCI2 **************************/
/*
*
*	Sets a flag to show that buttons have been pressed.
*	This will also wakeup the mcu if it's sleeping.
*/
ISR(PCINT2_vect)
{
	// We only care when there is a button pressed (or still down), not when it's released.
	// When it's released it will equal the mask value.
	sButtonPressed = sButtonPressed || (PINC & kPINCBtnMask) != kPINCBtnMask;
}

/************************* Pin change interrupt PCI3 **************************/
/*
*
*	Sets a flag to show that buttons have been pressed.
*	This will also wakeup the mcu if it's sleeping.
*/
ISR(PCINT3_vect)
{
	sButtonPressed = sButtonPressed || (PIND & kPINDBtnMask) != kPINDBtnMask;
}

