/*
*	HikingLoggerRemote.ino, Copyright Jonathan Mackey 2019
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
*/
#include <avr/sleep.h>
#include <SPI.h>
#include <EEPROM.h>
#include "TFT_ST7735S.h"
#include "RFM69.h"    // https://github.com/LowPowerLab/RFM69

#include "UnixTime.h"
#include "LogTempPres.h"
#include "RemoteLogLayout.h"
#include "RemoteLogAction.h"
#include "RemoteHikeLog.h"
#include "MSPeriod.h"


//#define BAUD_RATE	19200
//#define DEBUG_TIMING	1

/*
*	IMPORTANT RADIO SETTINGS
*/
//Frequency of the RFM69 module:
#define FREQUENCY         RF69_433MHZ
//#define FREQUENCY         RF69_868MHZ
//#define FREQUENCY         RF69_915MHZ

const uint8_t kRadioNSSPin = 12;	// PD4
const uint8_t kRadioIRQPin = 10;	// PD2	INT0
RFM69 radio(kRadioNSSPin, kRadioIRQPin);

const uint8_t kRxPin = 8;			// PD0
const uint8_t kTxPin = 9;			// PD1

const uint8_t kDCPin = 4;			// PB4
const int8_t kResetPin = 0;			// PB0
const int8_t kBacklightPin = 24;	// PA0
const int8_t kCDPin = 3;			// PB3

const uint8_t kLeftBtnPin = 15;		// PD7	PCINT31
const uint8_t kModeBtnPin = 2;		// PB2	PCINT10
const uint8_t kRightBtnPin = 11;	// PD3	PCINT27

const uint8_t kUnusedPinA1 = 25;	// PA1
const uint8_t kUnusedPinA2 = 26;	// PA2
const uint8_t kUnusedPinA3 = 27;	// PA3
const uint8_t kUnusedPinA4 = 28;	// PA4
const uint8_t kUnusedPinA5 = 29;	// PA5
const uint8_t kUnusedPinA6 = 30;	// PA6
const uint8_t kUnusedPinA7 = 31;	// PA7
const uint8_t kUnusedPinB1 = 1;		// PB1
const uint8_t kUnusedPinC0 = 16;	// PC0
const uint8_t kUnusedPinC1 = 17;	// PC1
const uint8_t kUnusedPinC2 = 18;	// PC2
const uint8_t kUnusedPinC3 = 19;	// PC3
const uint8_t kUnusedPinC4 = 20;	// PC4
const uint8_t kUnusedPinC5 = 21;	// PC5
const uint8_t kUnusedPinD5 = 13;	// PD5
const uint8_t kUnusedPinD6 = 14;	// PD6

const uint8_t kPINBBtnMask = _BV(PINB2);
const uint8_t kPINDBtnMask = (_BV(PIND3) | _BV(PIND7));

#define DEBOUNCE_DELAY	20	// ms
#define DEEP_SLEEP_DELAY	2000	// ms
enum ESleepMode
{
	eAwake,
	eLightSleep,	// Radio and Display turned off/sleeping
	eDeepSleep		// eLightSleep + MCU sleeping with BOD turned off.
};
static uint8_t	sSleepMode;	// One of ESleepMode
static uint8_t	sStartPinsState;
static uint8_t	sLastPinsState;
static bool		sButtonPressed = false;
static bool		sUpdateAll;

MSPeriod	debouncePeriod(DEBOUNCE_DELAY);	// for buttons

TFT_ST7735S	display(kDCPin, kResetPin, kCDPin, kBacklightPin);
RemoteLogLayout	remoteLogLayout;
// Define "xFont" to satisfy the auto-generated code with the font files
// This implementation uses remoteLogLayout as a subclass of xFont
#define xFont remoteLogLayout
#include "MyriadPro-Regular_24.h"
#include "MyriadPro-Regular_14.h"

RemoteLogAction	remoteLogAction;
RemoteHikeLog	remoteHikeLog;

#ifdef DEBUG_TIMING
uint8_t		sTimingDebug[254];
uint8_t		sTimingDebugIndex;
uint32_t	sLastMS;
#endif

/*********************************** setup ************************************/
void setup(void)
{
	UnixTime::RTCInit();
	set_sleep_mode(SLEEP_MODE_IDLE);

#ifdef BAUD_RATE
	Serial.begin(BAUD_RATE);
#else
	// Set both serial pins low so power doesn't backfeed to the serial board.
	pinMode(kRxPin, INPUT);
	digitalWrite(kRxPin, LOW);
	pinMode(kTxPin, INPUT);
	digitalWrite(kTxPin, LOW);
#endif
	SPI.begin();
	
	pinMode(kLeftBtnPin, INPUT_PULLUP);		// PD7
	pinMode(kModeBtnPin, INPUT_PULLUP);		// PB2
	pinMode(kRightBtnPin, INPUT_PULLUP);	// PD3
	
	// pull-up all unused pins to save power
	pinMode(kUnusedPinA1, INPUT_PULLUP);
	pinMode(kUnusedPinA2, INPUT_PULLUP);
	pinMode(kUnusedPinA3, INPUT_PULLUP);
	pinMode(kUnusedPinA4, INPUT_PULLUP);
	pinMode(kUnusedPinA5, INPUT_PULLUP);
	pinMode(kUnusedPinA6, INPUT_PULLUP);
	pinMode(kUnusedPinA7, INPUT_PULLUP);
	pinMode(kUnusedPinB1, INPUT_PULLUP);
	pinMode(kUnusedPinC0, INPUT_PULLUP);
	pinMode(kUnusedPinC1, INPUT_PULLUP);
	pinMode(kUnusedPinC2, INPUT_PULLUP);
	pinMode(kUnusedPinC3, INPUT_PULLUP);
	pinMode(kUnusedPinC4, INPUT_PULLUP);
	pinMode(kUnusedPinC5, INPUT_PULLUP);
	pinMode(kUnusedPinD5, INPUT_PULLUP);
	pinMode(kUnusedPinD6, INPUT_PULLUP);
	
	cli();
	ADCSRA &= ~_BV(ADEN);		// Turn off ADC to save power.
	PRR0 |= _BV(PRADC);
	/*
	*	To wake from sleep and to respond to button presses, setup pin change
	*	interrupts for the button pins. All of the pins aren't on the same port.
	*	PB2 is on PCIE1 and PD3 & PD7 are on PCIE3.
	*/
	PCMSK1 = _BV(PCINT10);
	PCMSK3 = _BV(PCINT27) | _BV(PCINT31);
	PCICR = _BV(PCIE1) | _BV(PCIE3);
	sei();
	

	// Read the network and node IDs from EEPROM
	{
		uint8_t	networkID = EEPROM.read(0);
		uint8_t	nodeID = EEPROM.read(1);

		radio.initialize(FREQUENCY, nodeID, networkID);
	}
	radio.sleep();

	display.begin(3); // Init TFT
	display.Fill();
	
	UnixTime::ResetSleepTime();
	remoteLogLayout.Initialize(&remoteLogAction, &remoteHikeLog,
									&display, &MyriadPro_Regular_24::font,
										&MyriadPro_Regular_14::font);
	remoteLogAction.Initialize(&radio, &remoteHikeLog);
	sUpdateAll = true;	// Update all data on display
	sSleepMode = eAwake;
	
#ifdef DEBUG_TIMING
	sTimingDebugIndex = 0;
	sLastMS = millis();
#endif
}

/************************************ loop ************************************/
void loop(void)
{
	if (sSleepMode == eAwake)
	{
		remoteLogLayout.Update(sUpdateAll);
		sUpdateAll = false;
	}
	
#ifdef DEBUG_TIMING
	if (sTimingDebugIndex < 253)
	{
		sTimingDebug[sTimingDebugIndex] = millis() - sLastMS;
		sLastMS = millis();
		sTimingDebugIndex++;
	}
#endif
	
	/*
	*	Check to see if any packets have arrived
	*/
	if (sSleepMode < eDeepSleep)
	{
		remoteLogAction.CheckRadioForPackets(sSleepMode >= eLightSleep);
	}
		
	if (sSleepMode == eAwake)
	{
	#ifdef DEBUG_TIMING
		if (Serial.available())
		{
			switch (Serial.read())
			{
				case '>':
					UnixTime::SetUnixTimeFromSerial();
					break;
				case 'D':
				case 'd':
					for (uint8_t j = 0; j < sTimingDebugIndex; j++)
					{
						Serial.println(sTimingDebug[sTimingDebugIndex]);
					}
					sTimingDebugIndex = 0;
					sLastMS = millis();
					break;
			}
		}
	#endif
		if (sButtonPressed)
		{
			UnixTime::ResetSleepTime();
			/*
			*	If a debounce period has passed
			*/
			{
				uint8_t		pinsState = ((~PIND) & kPINDBtnMask) + ((~PINB) & kPINBBtnMask);

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
							case _BV(PINB2):	// Mode button pressed
								remoteLogAction.ModeButtonPressed();
								break;
							case _BV(PIND7):	// Left button pressed
								remoteLogAction.LeftButtonPressed();
								break;
							case _BV(PIND3):	// Right button pressed
								remoteLogAction.RightButtonPressed();
								break;
							case (_BV(PIND3) | _BV(PIND7)): // Left & Right buttons held down together
								/*
								*	Deep sleep mode is only entered when not in
								*	log mode to avoid accidentally starting or
								*	stopping a log session, or changing the
								*	start/end log location.
								*/
								if (remoteLogAction.Mode() != RemoteLogAction::eLogMode)
								{
									if (debouncePeriod.Get() == DEBOUNCE_DELAY)
									{
										debouncePeriod.Set(DEEP_SLEEP_DELAY);
										debouncePeriod.Start();
										sButtonPressed = true;
										sStartPinsState = (_BV(PIND3) | _BV(PIND7));
									} else
									{
										GoToDeepSleep();
										debouncePeriod.Start();
									}
								}
								break;
							default:
								debouncePeriod.Start();
								break;
						}
					}
				} else
				{
					sStartPinsState = pinsState;
					/*
					*	The debouncePeriod is set to DEBOUNCE_DELAY in case it
					*	was set to DEEP_SLEEP_DELAY without actually going into
					*	deep sleep.  This can happen when the left and right
					*	buttons held down together were released before the
					*	DEEP_SLEEP_DELAY period passed.
					*/
					debouncePeriod.Set(DEBOUNCE_DELAY);
					debouncePeriod.Start();
				}
			}
		} else if (UnixTime::TimeToSleep())
		{
			GoToSleep();
		}
	/*
	*	Else if a button was pressed while sleeping THEN
	*	determine if it's the mode/enter button to wake the board up from sleep.
	*/
	} else if (sButtonPressed)
	{
		/*
		*	If the left and right buttons aren't pressed AND
		*	the enter button is THEN
		*	wakup the board
		*/
		if ((((~PIND) & kPINDBtnMask) + ((~PINB) & kPINBBtnMask)) == _BV(PINB2))
		{
			if (UnixTime::TimeToSleep())
			{
				UnixTime::ResetSleepTime();
				/*
				*	When pressed after eLightSleep the period is DEBOUNCE_DELAY.
				*	When pressed after eDeepSleep the period is DEEP_SLEEP_DELAY.
				*	This means that in order to wakeup from deep sleep the mode
				*	button must be held down for DEEP_SLEEP_DELAY ms.
				*/
				debouncePeriod.Start();
			/*
			*	else if the debounce period has passed THEN
			*	wake up.
			*/
			} else if (debouncePeriod.Passed())
			{
				sButtonPressed = false;
				if (sSleepMode == eDeepSleep)
				{
					/*
					*	Initialize causes the remoteLogAction to resync
					*	with the BMP280.
					*/
					remoteLogAction.Initialize();
				}
				WakeUp();
			}
		/*
		*	Else an invalid button combination was entered, go back to sleep.
		*	(light sleep or deep sleep)
		*/
		} else
		{
			sButtonPressed = false;
			debouncePeriod.Start();
			if (sSleepMode == eDeepSleep)
			{
				DeepSleep();
			}
		}
	} else if (sSleepMode == eDeepSleep)
	{
		DeepSleep();
	}
}

/*********************************** WakeUp ***********************************/
/*
*	Wakup the display.
*/
void WakeUp(void)
{
	sSleepMode = eAwake;
	display.WakeUp();
	UnixTime::ResetSleepTime();
	LogTempPres::GetInstance().SetChanged();	// To force a redraw
	sUpdateAll = true;	// Update all data on display
#ifdef BAUD_RATE
	Serial.begin(BAUD_RATE);
#endif
}

/********************************* GoToSleep **********************************/
/*
*	Puts the display to sleep
*	Note that the MCU does not go into a sleep mode.  If it does, this would
*	complicate the timing in monitoring the BMP280 remote.
*/
void GoToSleep(void)
{
	display.Fill();
	display.Sleep();
	remoteLogAction.GoToInfoMode();
	sSleepMode = eLightSleep;
}

/******************************* GoToDeepSleep ********************************/
/*
*	Puts the MCU, radio, and display to sleep.  In this mode nothing is working
*	including the RTC.  This is OK because when it wakes from deep sleep it
*	syncs with the gateway which sets the time.
*/
void GoToDeepSleep(void)
{
	radio.sleep();	
	display.Fill();
	display.Sleep();
	remoteLogAction.GoToInfoMode();
	sSleepMode = eDeepSleep;
	
#ifdef BAUD_RATE
	/*
	*	Release the serial pins (otherwise pinMode and digitalWrite have no
	*	effect.)
	*/
	Serial.end();
	// Set both serial pins low so power doesn't backfeed to the serial board.
	pinMode(kRxPin, INPUT);
	digitalWrite(kRxPin, LOW);
	pinMode(kTxPin, INPUT);
	digitalWrite(kTxPin, LOW);
#endif
	DeepSleep();
}

/********************************* DeepSleep **********************************/
/*
*	Puts the MCU, radio, and display to sleep.  In this mode nothing is working
*	including the RTC.  This is OK because when it wakes from deep sleep it
*	syncs with the gateway which sets the time.
*/
void DeepSleep(void)
{
	UnixTime::RTCDisable();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	cli();
	sleep_enable();
	sleep_bod_disable();
	sei();
	sleep_cpu();
	sleep_disable();
	sei();
	set_sleep_mode(SLEEP_MODE_IDLE);
	UnixTime::SetTime(0);
	UnixTime::RTCEnable();
}

/************************* Pin change interrupt PCI1 **************************/
/*
*
*	Sets a flag to show that pins have been pressed.
*	This will also wakeup the mcu if it's sleeping.
*/
ISR(PCINT1_vect)
{
	// We only care when there is a button pressed (or still down), not when it's released.
	// When it's released it will equal the mask value.
	sButtonPressed = sButtonPressed || (PINB & kPINBBtnMask) != kPINBBtnMask;
}

/************************* Pin change interrupt PCI3 **************************/
/*
*
*	Sets a flag to show that pins have been pressed.
*	This will also wakeup the mcu if it's sleeping.
*/
ISR(PCINT3_vect)
{
	sButtonPressed = sButtonPressed || (PIND & kPINDBtnMask) != kPINDBtnMask;
}
