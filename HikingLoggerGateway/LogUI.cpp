/*
*	LogUI.cpp, Copyright Jonathan Mackey 2021
*	Handles the UI.
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
*
*	The 240x240 display supports 5 lines of regualr 36pt text, and up to 7 if
*	the area reserved for the descent is reduced.  The decent is only needed for
*	glyphs that draw below the baseline.  If no lowercase characters are used,
*	the baseline can be manually adjusted to allow for more lines.
*
*	The FontHeader.height is the only font metric field used in the header by
*	XFont, the ascent and descent are ignored.
*	For 6 lines the height is manually set to 40.
*	For 7 lines the height is manually set to 34.
*/
#include <Arduino.h>
#include <EEPROM.h>
#include "LogUI.h"
#include "DisplayController.h"
#include "LogPacket.h"
#include "HikeLog.h"
#include "LogTempPres.h"
#include "UnixTime.h"
#include "BMP280Utils.h"
#include "HikingLoggerConfig.h"

const char kStartStr[] PROGMEM = "START";
const char kResumeStr[] PROGMEM = "RESUME";
const char kStopStr[] PROGMEM = "STOP";
const char kDoneStr[] PROGMEM = "DONE";
const char kSwapLocsStr[] PROGMEM = "SWAP LOCS";

const char kStartLocStr[] PROGMEM = "START LOC";
const char kEndLocStr[] PROGMEM = "END LOC";
const char kLogStartIsEndErrorStr[] PROGMEM = "START == END!";

const char kSavedHikesStr[] PROGMEM = "SAVED HIKES";
const char kNoneFoundStr[] PROGMEM = "(NONE FOUND)";
const char kGainStr[] PROGMEM = "GAIN ";

const char kBMP280ErrorStr[] PROGMEM = "SYNC BMP ERR";
const char kBMP280PressEnterToSyncStr[] PROGMEM = "[ENTER] 2 SYNC";
const char kBMP280SyncStr[] PROGMEM = "SYNCING BMP";
const char kBMP280SyncSuccessStr[] PROGMEM = "BMP SYNCD";

const char kSetTimeStr[] PROGMEM = "SET TIME";
const char kTestMP3Str[] PROGMEM = "TEST MP3";

const LogUI::SString_PDesc kSyncStateDesc[] PROGMEM =
{
	{kBMP280ErrorStr, XFont::eRed},
	{kBMP280SyncStr, XFont::eYellow},
	{kBMP280SyncSuccessStr, XFont::eGreen}
};

const char kSaveToSDStr[] PROGMEM = "SAVE TO SD";
const char kSaveLocsStr[] PROGMEM = "SAVE LOCS";
const char kUpdateLocsStr[] PROGMEM = "UPDATE LOCS";
const char* kSDActionStr[] = {kSaveToSDStr, kSaveLocsStr, kUpdateLocsStr};

const char kSavingStr[] PROGMEM = "SAVING...";
const char kUpdatingStr[] PROGMEM = "UPDATING...";
const char kEjectSDCardStr[] PROGMEM = "EJECT SD CARD";
const char kSDErrorStr[] PROGMEM = "SD ERROR";
const char kSavedStr[] PROGMEM = "SAVED";
const char kUpdatedStr[] PROGMEM = "UPDATED";

const LogUI::SString_PDesc kSDCardStateDesc[] PROGMEM =
{
	{kSavingStr, XFont::eYellow},		// eSavingToSD
	{kUpdatingStr, XFont::eYellow},		// eUpdatingFromSD
	{kEjectSDCardStr, XFont::eRed},		// eEjectSDCardNoReset
	{kSDErrorStr, XFont::eRed},			// eSDError
	{kSavedStr, XFont::eGreen},			// eSDSavedSuccess
	{kUpdatedStr, XFont::eGreen},		// eSDUpdateSuccess
	{kEjectSDCardStr, XFont::eGreen}	// eEjectSDCardAllowReset
};

const char kResetStr[] PROGMEM = "RESET LOG";
const char kResetVerifyYesStr[] PROGMEM = "(YES)";
const char kResetVerifyNoStr[] PROGMEM = "(NO)";
const char kResetSuccessStr[] PROGMEM = "RESET DONE";
const char kResetErrorStr[] PROGMEM = "RESET FAILED";

const LogUI::SString_PDesc kResetLogStateDesc[] PROGMEM =
{
	{kResetVerifyYesStr, XFont::eGreen},
	{kResetVerifyNoStr, XFont::eRed},
	{kResetSuccessStr, XFont::eGreen},
	{kResetErrorStr, XFont::eRed}
};

const uint32_t	k3ButtonRemoteDefaultTime = 100;	// milliseconds

bool	LogUI::sButtonPressed;
bool	LogUI::sSDInsertedOrRemoved;

//#define DEBUG_RADIO	1

#define DEBOUNCE_DELAY	20	// ms

/********************************* LogUI **********************************/
LogUI::LogUI(void)
: mSDCardPresent(false), mRadio(Config::kRadioNSSPin, Config::kRadioIRQPin),
	mMP3Player(Serial1, Config::kMP3RxPin, Config::kMP3TxPin, Config::kMP3PowerPin),
	mDebouncePeriod(DEBOUNCE_DELAY)
{
	pinMode(Config::kSDDetectPin, INPUT_PULLUP);
	pinMode(Config::kSDSelectPin, OUTPUT);
	digitalWrite(Config::kSDSelectPin, HIGH);	// Deselect the SD card.


	pinMode(Config::kUpBtnPin, INPUT_PULLUP);
	pinMode(Config::kLeftBtnPin, INPUT_PULLUP);
	pinMode(Config::kEnterBtnPin, INPUT_PULLUP);
	pinMode(Config::kRightBtnPin, INPUT_PULLUP);
	pinMode(Config::kDownBtnPin, INPUT_PULLUP);
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
		mRadio.initialize(FREQUENCY, nodeID, networkID);
		mRadio.sleep();
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

	mSleeping = false;
	// In case booting with the SD card in
	sSDInsertedOrRemoved = digitalRead(Config::kSDDetectPin) == LOW;
}

/*********************************** begin ************************************/
void LogUI::begin(
	HikeLog*			inHikeLog,
	DisplayController*	inDisplay,
	Font*				inNormalFont,
	Font*				inSmallFont)
{
	mMP3Player.begin();
	mHikeLog = inHikeLog;
	mPrevMode = eReviewHikesMode;
	mMode = eBMP280SyncMode;
	mSyncState = eBMP280Syncing;
	SetDisplay(inDisplay, inNormalFont);
	mNormalFont = inNormalFont;
	mSmallFont = inSmallFont;
	mUnixTimeEditor.Initialize(this);
}

/******************************** GoToLogMode *********************************/
void LogUI::GoToLogMode(void)
{
	UpDownButtonPressed(true);
	if (mMode <= eReviewHikesMode)
	{
		mPrevMode = eReviewHikesMode;
		mMode = eLogMode;
		mLogStateModifier = HikeLog::eModifier;
	}
}

/**************************** UpDownButtonPressed *****************************/
void LogUI::UpDownButtonPressed(
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
			mode = inIncrement ? eTestMP3Mode : eStartLocSelMode;
			break;
		case eReviewHikesMode:
			if (mHikeLog->Active())
			{
				mode = inIncrement ? eLogMode : (mSyncState != eBMP280SyncError ? eLogMode : eBMP280SyncMode);
			} else
			{
				mode = inIncrement ? eLogMode : eSetTimeMode;
			}
			mPrevMode = eReviewHikesMode;
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
		//case eSDCardMode:	This mode is entered and existed based on whether
		//					the SD card is present.
		
		case eSetTimeMode:
			mode = inIncrement ? eReviewHikesMode : eTestMP3Mode;
			break;
		case eTestMP3Mode:
			mode = inIncrement ? eSetTimeMode : eEndLocSelMode;
			break;
		case eEditTimeMode:
			// The only way to get out of eEditTimeMode is to press enter.
			mUnixTimeEditor.UpDownButtonPressed(!inIncrement);
			break;
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
void LogUI::EnterPressed(void)
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
						ClearLines(2,1);
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
			UpDownButtonPressed(true);
			break;
		case eEndLocSelMode:
			mMode = eLogMode;
			mHikeLog->EndingLocIndex() = mLocIndex;
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
					mPrevMode = eReviewHikesMode;
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
					case eSDCardIdle:
						mSDCardState = mSDCardAction == eUpdateLocationsAction ?
										eUpdatingFromSD : eSavingToSD;
						break;
					case eSDError:
						mSDCardState = eEjectSDCardNoReset;
						break;
					case eSDSavedSuccess:
					case eSDUpdateSuccess:
						mSDCardState = eEjectSDCardAllowReset;
						break;
				}
			}
			break;
		case eSetTimeMode:
			if (!mHikeLog->Active())
			{
				mUnixTimeEditor.SetTime(UnixTime::Time());
				mMode = eEditTimeMode;
			}
			break;
		case eTestMP3Mode:
			mMP3Player.Play(1);
			break;
		case eEditTimeMode:
			// If enter was pressed on SET or CANCEL
			if (mUnixTimeEditor.EnterPressed())
			{
				if (!mUnixTimeEditor.CancelIsSelected())
				{
					bool	isFormat24Hour;
					time32_t time = mUnixTimeEditor.GetTime(isFormat24Hour);
					UnixTime::SetTime(time);
					if (UnixTime::Format24Hour() != isFormat24Hour)
					{
						UnixTime::SetFormat24Hour(isFormat24Hour);
						uint8_t	flags;
						EEPROM.get(Config::kFlagsAddr, flags);
						if (isFormat24Hour)
						{
							flags &= ~1;	// 0 = 24 hour
						} else
						{
							flags |= 1;		// 1 = 12 hour (default for new/erased EEPROMs)
						}
						EEPROM.put(Config::kFlagsAddr, flags);
					}
				}
				mPrevMode = eReviewHikesMode;	// Force a redraw
				mMode = eLogMode;
				UnixTime::ResetSleepTime();	// Otherwise it will immediately go to sleep because of the time change
			}
			break;
	}
}

/*********************************** Update ***********************************/
/*
*	Called from loop() just after the layout has updated.  Any states that
*	need time are handled here.
*/
void LogUI::Update(void)
{

//	lock up notes:  The old RFM69 library uses SPI within the ISR.  This is
//	very bad because if the ISR is called while some other SPI transfer is
//	in progress, the RFM69 code will lock up and/or currupt data because it
//	can't get control of the SPI bus.
//	The latest RFM69 library only sets a flag that data is available in the ISR.
//	The next call to receiveDone() checks this flag and retrieves the data.
//

	if (!mSleeping)
	{
		UpdateDisplay();
		/*
		*	Some action states need to be reflected in the display before
		*	performing an action.
		*/
		switch (mMode)
		{
			case eSDCardMode:
				switch (mSDCardState)
				{
					case eSavingToSD:
					{
						mSDCardState = ((mSDCardAction == eSaveHikeLogUI) ?
										mHikeLog->SaveLogToSD() : 
											HikeLocations::GetInstance().SaveToSD()) ?
												eSDSavedSuccess : eSDError;
						break;
					}
					case eUpdatingFromSD:
						mSDCardState = HikeLocations::GetInstance().LoadFromSD() ? eSDUpdateSuccess : eSDError;
						break;
				}
				break;
		}
	}
	
	/*
	*	Check to see if any packets have arrived
	*/
	CheckRadioForPackets(mSleeping);
	
	mMP3Player.SleepIfDonePlaying();
	
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
		mMP3Player.Play(1);
	}
		
	mHikeLog->LogEntryIfTime();
	if (!mSleeping)
	{
		if (Serial.available())
		{
			switch (Serial.read())
			{
				case '>':	// Set the time.  A hexadecimal ASCII UNIX time follows
					UnixTime::SetUnixTimeFromSerial();
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
					bool	hasSavedHikes = mHikeLog->GetSavedHike(header.tail, hikeSummary);
					if (hasSavedHikes)
					{
						Serial.print(F("\tstartTime =\t0x"));
						Serial.println(hikeSummary.startTime, HEX);
						Serial.print(F("\tendTime =\t0x"));
						Serial.println(hikeSummary.endTime, HEX);
					}
					break;
				}
				case 's':
				{
					mHikeLog->SaveLogSummariesToSD();
					break;
				}
				case 'l':
				{
					mHikeLog->LoadLogSummariesFromSD();
					break;
				}
				case 'm':	// Test/play the first mp3.
				{
					mMP3Player.Play(1);
					break;
				}
				case 'M':	// Test/play the second mp3
				{
					mMP3Player.Play(2);
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
			#if 0
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
			#endif
			}
		}
		if (sButtonPressed)
		{
			UnixTime::ResetSleepTime();
			/*
			*	If a debounce period has passed
			*/
			{
				// PINC result shifted right to avoid conflicts with button bit
				// values on PIND.  In the switch below PINC values are offset
				// by 1.
				uint8_t		pinsState = ((~PIND) & Config::kPINDBtnMask) + (((~PINC) & Config::kPINCBtnMask) >> 1);

				/*
				*	If debounced
				*/
				if (mStartPinState == pinsState)
				{
					if (mDebouncePeriod.Passed())
					{
						sButtonPressed = false;
						mStartPinState = 0xFF;
						switch (pinsState)
						{
							case Config::kUpBtn:	// Up button pressed
								UpDownButtonPressed(false);
								break;
							case Config::kEnterBtn:	// Enter button pressed
								EnterPressed();
								break;
							case Config::kLeftBtn:	// Left button pressed
								LeftRightButtonPressed(false);
								break;
							case Config::kDownBtn:	// Down button pressed
								UpDownButtonPressed(true);
								break;
							case Config::kRightBtn:	// Right button pressed
								LeftRightButtonPressed(true);
								break;
							default:
								mDebouncePeriod.Start();
								break;
						}
					}
				} else
				{
					mStartPinState = pinsState;
					mDebouncePeriod.Start();
				}
			}
		} else if (UnixTime::TimeToSleep() &&
			mMode != eEditTimeMode)
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
		if ((PINC & Config::kPINCBtnMask) == Config::kPINCBtnMask &&
			(PIND & Config::kPINDBtnMask) == _BV(PIND7))
		{
			if (UnixTime::TimeToSleep())
			{
				UnixTime::ResetSleepTime();
				mDebouncePeriod.Start();
			/*
			*	else if the debounce period has passed THEN
			*	wake the board up.
			*/
			} else if (mDebouncePeriod.Passed())
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
			mDebouncePeriod.Start();
			//sleep_mode();	// Go back to sleep
		}
	} /* else
	{
		sleep_mode();	// Go back to sleep
	}*/
	
	if (sSDInsertedOrRemoved)
	{
		WakeUp();
		UnixTime::ResetSleepTime();
		uint8_t		pinsState = (~PINB) & _BV(PINB1);
		/*
		*	If debounced
		*/
		if (mStartPinState == pinsState)
		{
			if (mDebouncePeriod.Passed())
			{
				sSDInsertedOrRemoved = false;
				mStartPinState = 0xFF;
				SetSDCardPresent(pinsState != 0);
			}
		} else
		{
			mStartPinState = pinsState;
			mDebouncePeriod.Start();
		}
	}
}

/*********************************** WakeUp ***********************************/
/*
*	Wakup the board from sleep.
*/
void LogUI::WakeUp(void)
{
	if (mSleeping)
	{
		mSleeping = false;
		mDisplay->WakeUp();
	#ifdef USE_EXTERNAL_RTC
		UnixTime::SetTimeFromExternalRTC();
	#endif
		UnixTime::ResetSleepTime();
		LogTempPres::GetInstance().SetChanged();	// To force a redraw
		Serial.begin(BAUD_RATE);
	}
}

/********************************* GoToSleep **********************************/
/*
*	Puts the board to sleep
*/
void LogUI::GoToSleep(void)
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
	pinMode(Config::kRxPin, INPUT);
	digitalWrite(Config::kRxPin, LOW);
	pinMode(Config::kTxPin, INPUT);
	digitalWrite(Config::kTxPin, LOW);

	mDisplay->Fill();
	mDisplay->Sleep();
	GoToLogMode();
	mSleeping = true;
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
	LogUI::SetSDInsertedOrRemoved();
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
	LogUI::SetButtonPressed((PINC & Config::kPINCBtnMask) != Config::kPINCBtnMask);
}

/************************* Pin change interrupt PCI3 **************************/
/*
*
*	Sets a flag to show that buttons have been pressed.
*	This will also wakeup the mcu if it's sleeping.
*/
ISR(PCINT3_vect)
{
	// We only care when there is a button pressed (or still down), not when it's released.
	// When it's released it will equal the mask value.
	LogUI::SetButtonPressed((PIND & Config::kPINDBtnMask) != Config::kPINDBtnMask);
}

/*************************** LeftRightButtonPressed ***************************/
void LogUI::LeftRightButtonPressed(
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
		case eSDCardMode:
			if (mSDCardState == eSDCardIdle)
			{
				if (inIncrement)
				{
					mSDCardAction++;
					if (mSDCardAction >= eNumSDCardActions)
					{
						mSDCardAction = eSaveHikeLogUI;
					}
				} else if (mSDCardAction)
				{
					mSDCardAction--;
				} else
				{
					mSDCardAction = eUpdateLocationsAction;
				}
			}
			break;
		case eEditTimeMode:
			mUnixTimeEditor.LeftRightButtonPressed(inIncrement);
			break;
	}
}

/****************************** SetSDCardPresent ******************************/
void LogUI::SetSDCardPresent(
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
		mSDCardState = mHikeLog->Active() ? eEjectSDCardNoReset : eSDCardIdle;
		mSDCardAction = eSaveHikeLogUI;
	/*
	*	Else the card was just ejected
	*/
	} else
	{
		mMode = (mSDCardAction == eSaveHikeLogUI &&
					mSDCardState == eSDSavedSuccess) ? eResetLogMode : eLogMode;
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
void LogUI::CheckRadioForPackets(
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
			if (mRadio.receiveDone())
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
					mRadio.receiveDone();
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
				mPrevMode = eReviewHikesMode;
				mMode = eBMP280SyncMode;
				mSyncState = eBMP280Syncing;
			}
		/*
		*	Else if a packet has arrived...
		*/
		} else if (!m3ButtonRemotePeriod.Passed() ||
			(RFM69::_mode == RF69_MODE_RX && RFM69::hasData()))
		{
			if (mRadio.receiveDone())
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
			mRadio.sleep();
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
		mRadio.receiveDone();
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
void LogUI::SyncWithBMP280Remote(void)
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
		if (mRadio.receiveDone())
		{
			Log::SBMP280Packet*	packet = (Log::SBMP280Packet*)RFM69::DATA;
			if (packet->message == Log::kBMP280)
			{
				timeout.Start();
				while (!timeout.Passed())
				{
					if (mRadio.receiveDone() &&
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
		/*
		*	Most of the time the reason for the sync error is the BMP280 is out
		*	of range of the gateway.  Play a sound to notify of the sync error.
		*/
		mMP3Player.Play(2);	// Play sound at index 2
	}
}

/**************************** HandleBMP280PacketRx ****************************/
bool LogUI::HandleBMP280PacketRx(void)
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
uint8_t LogUI::InitSyncPacket(
	uint8_t*	inPacket)
{
	Log::SSyncPacket*	packet = (Log::SSyncPacket*)inPacket;
	packet->message = Log::kSync;
	packet->time = UnixTime::Time();
	packet->startTime = mHikeLog->StartTime();
	packet->endTime = mHikeLog->EndTime();
	packet->startLocIndex = mHikeLog->StartingLocIndex();
	packet->endLocIndex = mHikeLog->EndingLocIndex();
	packet->logIsFull = mHikeLog->IsFull();
	return(sizeof(Log::SSyncPacket));			
}

/***************************** InitLocationPacket *****************************/
uint8_t LogUI::InitLocationPacket(
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
void LogUI::HandlePacketRx(void)
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
				mPrevMode = eReviewHikesMode;
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
		mRadio.sendACK(packetBuff, packetSize);
		mRadio.receiveDone();	// Immediately move back into Rx mode
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

/******************************** DrawLocation ********************************/
void LogUI::DrawLocation(
	uint16_t	inLocIndex,
	uint8_t		inFirstLine)
{
	HikeLocations::GetInstance().GoToLocation(inLocIndex);
	const SHikeLocationLink&	link = HikeLocations::GetInstance().GetCurrent();
	MoveTo(inFirstLine);
	SetTextColor(eOrange);
	DrawStr(link.loc.name, true);
	char elevationStr[32];
	int32_t	elevation = link.loc.elevation;
	strcpy(&elevationStr[BMP280Utils::Int32ToIntStr(elevation*100, elevationStr)], LogTempPres::GetInstance().GetAltitudeSuffixStr());
	MoveTo(inFirstLine+1);
	SetTextColor(0xFBC0);
	DrawStr(elevationStr, true);
}

/********************************** DrawTime **********************************/
void LogUI::DrawTime(
	time32_t	inTime,
	bool		inShowingAMPM)
{
	char timeStr[32];
	bool isPM = UnixTime::CreateTimeStr(inTime, timeStr);
	DrawStr(timeStr);
	if (inShowingAMPM)
	{
		SetFont(mSmallFont);
		DrawStr(isPM ? " PM":" AM");
		SetFont(mNormalFont);
	}
}


/***************************** DrawIndexedDescStr *****************************/
void LogUI::DrawIndexedDescStr(
	const SString_PDesc*	inStringList,
	uint8_t					inStrIndex,
	bool					inHasOptions,
	bool					inCentered)
{
	SString_PDesc	stringDesc;
	memcpy_P(&stringDesc, &inStringList[inStrIndex], sizeof(SString_PDesc));
	DrawTextOption(stringDesc.descStr, stringDesc.color, inHasOptions, inCentered);
}

/******************************* DrawTextOption *******************************/
void LogUI::DrawTextOption(
	const char*	inStrRef,
	uint16_t	inColor,
	bool		inHasOptions,
	bool		inCentered)
{
	char descStr[32];
	if (inHasOptions)
	{
		SetTextColor(eWhite);
		DrawStr("<");
		DrawRightJustified(">");
	}
	
	SetTextColor(inColor);
	strcpy_P(descStr, inStrRef);
	if (inCentered)
	{
		DrawCentered(descStr);
	} else
	{
		DrawStr(descStr);
	}
}

/********************************* ClearLines *********************************/
void LogUI::ClearLines(
	uint8_t	inStartLine,
	uint8_t	inNumLines)
{
	MoveTo(inStartLine,0);
	mDisplay->FillBlock(43*inNumLines, 240, eBlack);
}

/******************************* UpdateDisplay ********************************/
/*
*	The display is 240 x 240.  This allows for 5 lines of text at 43px per line.
*	What is drawn is determined by the mode as defined in the LogAction class.
*/
void LogUI::UpdateDisplay(void)
{
	bool	timeChanged = false;
	float	altitude = 0;
	
	/*
	*	For all modes that contain the time, temp and elevation, redraw when
	*	coming from a mode that doesn't contain these fields.
	*/
	bool updateAll = mMode != mPrevMode && mPrevMode == eReviewHikesMode;
	
	if (mMode != eReviewHikesMode &&
		mMode != eEditTimeMode)
	{
		if (updateAll)
		{
			if (mPrevMode == eReviewHikesMode)
			{
				MoveTo(3);
				mDisplay->FillBlock(25, 240, eBlack);
			}
			/*
			*	Draw a 2 pixel horizontal line halfway between line 3 relative to
			*	the top and line 4 relative to the bottom.  This is a 25 pixel gap,
			*	so 13 will place the line in the center.
			*/
			mDisplay->MoveTo(240-(43*2)-13, 0);
			mDisplay->FillBlock(2, 240, eGray);
			// Move to line 4 relative to the display bottom.
			mDisplay->MoveTo(240-(43*2), 0);
			mDisplay->FillBlock(43*2, 240, eBlack);
		}
	
		if (updateAll ||
			UnixTime::TimeChanged())
		{
			timeChanged = true;
			UnixTime::ResetTimeChanged();
			char timeStr[32];
			bool isPM = UnixTime::CreateTimeStr(timeStr);
			mDisplay->MoveTo(240-(43*2), 45);	// Line 4 relative to the display bottom
			SetTextColor(eWhite);
			DrawStr(timeStr);
			uint8_t showingAMPM = UnixTime::Format24Hour() ? 0 : (isPM ? 1 : 2);
			/*
			*	If updating everything OR
			*	the AM/PM suffix state changed (to/from AM/PM or hidden) THEN
			*	draw or erase the suffix.
			*/
			if (updateAll || (mPrevShowingAMPM != showingAMPM))
			{
				mPrevShowingAMPM = showingAMPM;
				if (showingAMPM)
				{
					SetFont(mSmallFont);
					DrawStr(isPM ? " PM":" AM");
					SetFont(mNormalFont);
					// The width of a P is slightly less than an A, so erase any
					// artifacts left over when going from A to P.
					// The width of an 18pt A - the width of a P = 1
					mDisplay->FillBlock(mFontRows, 1, eBlack);
				}
			}
		}
		/*
		*	If everything is being updated OR
		*	the temperature has changed THEN
		*	draw the current temperature
		*/
		if (updateAll ||
			LogTempPres::GetInstance().TemperatureChanged())
		{
			mDisplay->MoveTo(240-43, 0);	// Line 5 relative to the display bottom
			SetTextColor(eMagenta);
			if (LogTempPres::GetInstance().IsValid())
			{
				/*
				*	The Temperature is drawn on the left.
				*/
				char tempStr[32];
				uint8_t strLen = LogTempPres::GetInstance().CreateTempStr(tempStr) + 2;
				strcpy(&tempStr[strLen], LogTempPres::GetInstance().GetTempSuffixStr());
				DrawStr(tempStr);
				EraseTillColumn(86);	// Erase any artifacts when moving to a
										// shorter string.
			}
		}
		/*
		*	If everything is being updated OR the pressure has changed AND
		*	the pressure reading is valid AND
		*	the starting altitude has been set THEN
		*	draw the current altitude
		*/
		if ((updateAll ||
			LogTempPres::GetInstance().PressureChanged()) &&
			LogTempPres::GetInstance().IsValid() &&
			LogTempPres::GetInstance().StartingAltitude())
		{
			char altitudeStr[32];
			altitude = LogTempPres::GetInstance().CalcCurrentAltitude();
			LogTempPres::GetInstance().CreateAltitudeStr(altitude, altitudeStr);
			mDisplay->MoveTo(240-43, 0);	// Line 5 relative to the display bottom
			SetTextColor(eYellow);
			DrawRightJustified(altitudeStr);
		}
	}
	updateAll = updateAll || mMode != mPrevMode;
	mPrevMode = mMode;

	switch (mMode)
	{
		/*
		*	When there are two options the state modifier determines the
		*	option displayed.
		*
		*	Line 1:
		*	if not running the options are "SWAP" or "START"
		*	if running the option is "STOP"
		*	if paused/stopped the options are "DONE" or "RESUME"
		*	If starting == ending loc, "START == END!" is displayed.
		*	Line 2:
		*	If log is running the elapsed time and percentage remaining.
		*/
		case eLogMode:
		{
			uint8_t	logState = mHikeLog->GetLogState() + mLogStateModifier;
			if (updateAll ||
				logState != mPrevLogState)
			{
				if (updateAll ||
					!mHikeLog->Active() ||
					(logState != mPrevLogState && mPrevLogState <= HikeLog::eNotRunning))
				{
					ClearLines();
				} else
				{
					ClearLines(0, 2);
				}
				mPrevLogState = logState;
				MoveTo(0);
				{
					switch(logState)
					{
						case HikeLog::eStopped + HikeLog::eModifier:
							DrawTextOption(kResumeStr, eGreen, true, true);
							break;
						case HikeLog::eNotRunning + HikeLog::eModifier:
							mPrevLocIndex = 0;	// Start and Swap display the start and end location names
							DrawTextOption(kStartStr, eGreen, true, true);
							break;
						case HikeLog::eRunning:
						case HikeLog::eRunning + HikeLog::eModifier:
							DrawTextOption(kStopStr, eRed, false, true);
							break;
						case HikeLog::eStopped:
							DrawTextOption(kDoneStr, eWhite, true, true);
							break;
						case HikeLog::eNotRunning:
							mPrevLocIndex = 0;	// Start and Swap display the start and end location names
							DrawTextOption(kSwapLocsStr, eWhite, true, true);
							break;
						case HikeLog::eCantRun:
						case HikeLog::eCantRun + HikeLog::eModifier:
							DrawTextOption(kLogStartIsEndErrorStr, eYellow, false, true);
							break;
					}
					break;
				}

			}
			if (mHikeLog->Active())
			{
				if (updateAll || timeChanged)
				{
					time32_t	elapsedTime = mHikeLog->ElapsedTime();
					/*
					*	If everything is being updated OR
					*	the timer is active THEN
					*	draw the elapsed time on the 2nd line
					*/
					if (updateAll ||
						elapsedTime)
					{
						MoveTo(2);
						char timeStr[32];
						UnixTime::CreateTimeStr(elapsedTime, timeStr);
						SetTextColor(eYellow);
						DrawStr(timeStr);
					}
				}
				if (altitude != 0)
				{
					char altitudeStr[32];
					MoveTo(2);
					LogTempPres::GetInstance().CreateAltitudePercentageStr(altitude, altitudeStr);
					uint16_t	textWidth = DrawRightJustified(altitudeStr);
					mDisplay->MoveToColumn(122);
					EraseTillColumn(240 - textWidth);
				}
			} else
			{
				uint16_t	locIndex = mHikeLog->StartingLocIndex();
				if (updateAll ||
					locIndex != mPrevLocIndex)
				{
					mPrevLocIndex = locIndex;
					HikeLocations::GetInstance().GoToLocation(locIndex);
					MoveTo(1);
					SetTextColor(eGreen);
					DrawStr(HikeLocations::GetInstance().GetCurrent().loc.name, true);
					HikeLocations::GetInstance().GoToLocation(mHikeLog->EndingLocIndex());
					MoveTo(2);
					SetTextColor(eRed);
					DrawStr(HikeLocations::GetInstance().GetCurrent().loc.name, true);
				}
			}
			break;
		}
		case eStartLocSelMode:
		case eEndLocSelMode:
		{
			bool	isStart = mMode == eStartLocSelMode;
			if (updateAll)
			{
				ClearLines();
				MoveTo(0);
				DrawTextOption(isStart ? kStartLocStr : kEndLocStr, eWhite, true, true);
			}
			
			if (updateAll ||
				mLocIndex != mPrevLocIndex)
			{
				mPrevLocIndex = mLocIndex;
				DrawLocation(mLocIndex);
			}
			break;
		}
		case eBMP280SyncMode:
		{
			if (updateAll ||
				mSyncState != mPrevSyncState)
			{
				ClearLines();
				MoveTo(0);
				mPrevSyncState = mSyncState;
				DrawIndexedDescStr(kSyncStateDesc, mSyncState, false, true);
				MoveTo(2);
				if (mSyncState == eBMP280SyncError)
				{
					DrawTextOption(kBMP280PressEnterToSyncStr, eWhite, false, true);
				}
			}
			break;
		}
		case eResetLogMode:
		{
			if (updateAll ||
				mResetLogState != mPrevResetLogState)
			{
				if (updateAll || mResetLogState >= eResetSuccess)
				{
					ClearLines();
					MoveTo(0);
					DrawTextOption(kResetStr, eWhite, mResetLogState <= eResetVerifyNo, true);
				} else
				{
					ClearLines(1,1);
				}
				mPrevResetLogState = mResetLogState;
				MoveTo(1);
				DrawIndexedDescStr(kResetLogStateDesc, mResetLogState, false, true);
			}
			break;
		}
		case eSDCardMode:
		{			
			if (updateAll ||
				mSDCardState != mPrevSDCardState ||
				mSDCardAction != mPrevSDCardAction)
			{
				mPrevSDCardState = mSDCardState;
				mPrevSDCardAction = mSDCardAction;
				ClearLines();
				if (mSDCardState == eSDCardIdle)
				{
					MoveTo(0);
					DrawTextOption(kSDActionStr[mSDCardAction], eCyan, true, true);
				} else
				{
					MoveTo(1);
					DrawIndexedDescStr(kSDCardStateDesc, mSDCardState, false, true);
				}
			}
			break;
		}
		case eReviewHikesMode:
		{
			SHikeSummary	hikeSummary;
			bool	hasSavedHikes = mHikeLog->GetSavedHike(mHikeRef, hikeSummary);
			updateAll = updateAll || mHikeRef != mPrevHikeRef;
			
			if (updateAll)
			{
				mDisplay->Fill();
				mPrevHikeRef = mHikeRef;
				if (hasSavedHikes)
				{
					char tempStr[32];
					UnixTime::CreateDateStr(hikeSummary.startTime, tempStr);
					MoveTo(0);
					SetTextColor(eWhite);
					DrawStr("<");
					DrawRightJustified(">");
					SetTextColor(eCyan);
					DrawCentered(tempStr);
				} else
				{
					MoveTo(0);
					DrawTextOption(kSavedHikesStr, eWhite, false, true);
					MoveTo(2);
					DrawTextOption(kNoneFoundStr, eYellow, false, true);
				}
			} else if (mPrevReviewState != mReviewState)
			{
				MoveTo(1);
				mDisplay->FillBlock(240-43, 240, eBlack);
				updateAll = true;
			}
			mPrevReviewState = mReviewState;
			if (updateAll &&
				hasSavedHikes)
			{
				char tempStr[32];
				if (mReviewState == eReviewLocs)
				{
					DrawLocation(hikeSummary.startingLocIndex, 1);
					/*
					*	The Temperature is drawn on the right.
					*/

					uint8_t strLen = LogTempPres::GetInstance().CreateTempStr(hikeSummary.startTemp, tempStr) + 2;
					strcpy(&tempStr[strLen], LogTempPres::GetInstance().GetTempSuffixStr());
					SetTextColor(eMagenta);
					DrawRightJustified(tempStr);
					DrawLocation(hikeSummary.endingLocIndex, 3);
					strLen = LogTempPres::GetInstance().CreateTempStr(hikeSummary.endTemp, tempStr) + 2;
					strcpy(&tempStr[strLen], LogTempPres::GetInstance().GetTempSuffixStr());
					SetTextColor(eMagenta);
					DrawRightJustified(tempStr);
				/*
				*	Else draw the elevation gain, and the start, end, and
				*	elapsed time.  The day of week is placed on the bottom line.
				*/
				} else
				{
					HikeLocations::GetInstance().GoToLocation(hikeSummary.endingLocIndex);
					int32_t	elevation = HikeLocations::GetInstance().GetCurrent().loc.elevation;
					HikeLocations::GetInstance().GoToLocation(hikeSummary.startingLocIndex);
					elevation -= HikeLocations::GetInstance().GetCurrent().loc.elevation;
					strcpy(&tempStr[BMP280Utils::Int32ToIntStr(elevation*100, tempStr)], LogTempPres::GetInstance().GetAltitudeSuffixStr());
					MoveTo(1);
					DrawTextOption(kGainStr, eWhite, false, false);
					SetTextColor(0xFBC0);
					DrawStr(tempStr);
					MoveTo(2);
					SetTextColor(eGreen);
					DrawTime(hikeSummary.startTime, true);
					UnixTime::CreateDayOfWeekStr(hikeSummary.startTime, tempStr);
					DrawRightJustified(tempStr);
					MoveTo(3);
					SetTextColor(eRed);
					DrawTime(hikeSummary.endTime, true);
					UnixTime::CreateDayOfWeekStr(hikeSummary.endTime, tempStr);
					DrawRightJustified(tempStr);
					MoveTo(4);
					SetTextColor(eYellow);
					DrawTime(hikeSummary.endTime - hikeSummary.startTime, false);
				}
			}
			break;
		}
		case eSetTimeMode:
			if (updateAll)
			{
				ClearLines(0, 3);
				MoveTo(0);
				DrawTextOption(kSetTimeStr, eMagenta, false, true);
			}
			break;
		case eTestMP3Mode:
			if (updateAll)
			{
				ClearLines(0, 3);
				MoveTo(0);
				DrawTextOption(kTestMP3Str, eMagenta, false, true);
			}
			break;
		case eEditTimeMode:
			mUnixTimeEditor.Update();
			break;

	}
}
