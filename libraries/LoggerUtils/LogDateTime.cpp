/*
*	LogDateTime.cpp, Copyright Jonathan Mackey 2019
*	Class to manage the date and time.  This does not handle leap seconds.
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
#include "LogDateTime.h"
#ifndef __MACH__
#include "LogSerialUtils.h"
#include <Arduino.h>
#include <avr/sleep.h>
#else
#include <iostream>
#define PROGMEM
#define pgm_read_word(xx) *(xx)
#define memcpy_P memcpy
#define cli()
#define sei()
#endif

// https://www.epochconverter.com
// Unix time 0 is January 1, 1970 12:00:00 AM, a Thursday

const uint8_t	LogDateTime::kOneMinute = 60;
const uint16_t	LogDateTime::kOneHour = 3600;
const uint32_t	LogDateTime::kOneDay = 86400;
const uint32_t	kDaysInFourYears = 1461;
const uint32_t	LogDateTime::kOneYear = 31557600;
const time32_t	LogDateTime::kYear2000 = 946684800;	// Seconds from 1970 to 2000
const uint16_t	LogDateTime::kDaysTo[] PROGMEM = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
const uint16_t	LogDateTime::kDaysToLY[] PROGMEM = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
time32_t		LogDateTime::sTime;
bool			LogDateTime::sTimeChanged;
bool			LogDateTime::sFormat24Hour;	// false = 12, true = 24
const char		LogDateTime::kMonth3LetterAbbr[] PROGMEM = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
const char		LogDateTime::kDay3LetterAbbr[] PROGMEM = "SUNMONTUEWEDTHUFRISAT";

// SLEEP_DELAY : If no activity after SLEEP_DELAY seconds, go to sleep
#define SLEEP_DELAY	120
//#define SLEEP_DELAY	10
time32_t			LogDateTime::sSleepTime;
// date +%s		<< Unix command to get the local time

#ifndef __MACH__
#if 0
/*
	Sleep Mode notes - Idle is the only mode that seems to keep good time.
		power save speeds up
		standby slows down.
		ADC speeds up.

	This alternate method of implementing the RTC interrupt allows for fine
	tuning of the time.  It's not helpful so it's not being used. It was used to
	try to figure out how to keep the sleep modes from influencing the time.
	Some cause the time to drift significantly faster or slower.  It may be
	related to the DC to DC converter changing voltage when going into one of
	the power save modes.  I finally gave up and decided not to put the MCU to
	sleep. This makes other timing issues easier (e.g. BMP280 period), but it's
	still annoying not to know why the time drift happens.
*/
/********************************** RTCInit ***********************************/
/*
*	This is the RTCInit for a 644 or 1284, and the rest of this family with
*	a Timer2 section.
*/
void LogDateTime::RTCInit(
	time32_t	inTime)
{
	cli();					// Disable interrupts
	TIMSK2 &= ~((1<<TOIE2)|(1<<OCIE2A)|(1<<OCIE2B));		// Make sure all TC2 interrupts are disabled

	ASSR = (1<<AS2);		// set Timer/counter0 to be asynchronous from the CPU clock
							// with a second external clock (32,768kHz)driving it.
	TCNT2 =0;				// Reset timer counter register
	/*
	*	Use CTC mode to fine tune the time by adjusting OCR2A.
	*	On match with OCR2A the counter TCNT2 is reset to zero.
	*	I could not get CTC mode 2 to work, on either the 644 or the 1284.
	*	The symptom was/is that the reset never occured and it appeared to be
	*	racing (interrupt called way too often.)
	*	Mode 7, although not named CTC, has the same effect, and more
	*	importantly, it works (i.e. it resets the counter on match.)
	*/
	TCCR2A = (1<<WGM20) | (1<<WGM21);	// Clear timer on compare match.							

//	TCCR2B = ((1<<CS22)|(1<<CS21)|(0<<CS20)|(1<<WGM22));
//	OCR2A  = 127;

	TCCR2B = ((1<<CS22)|(0<<CS21)|(1<<CS20)|(1<<WGM22));
	OCR2A  = 255;
															
	while ((ASSR & ((1 << OCR2AUB) | (1 << OCR2BUB) | (1 << TCR2AUB)
        	| (1 << TCR2BUB) | (1<< TCN2UB))));	//Wait until TC2 is updated
	
	TIFR2  = (1 << TOV2) | (1 << OCF2A) | (1 << OCF2B);		// Clear pending interrupts
	TIMSK2 = (1 << OCIE2A);					// Enable Timer 2 Output Compare Match Interrupt
	sTime = inTime;
	sei();									// Set the Global Interrupt Enable Bit
}

/********************************* RTCDisable *********************************/
void LogDateTime::RTCDisable(void)
{
	// Power-Down mode is currently used to put the MCU to sleep so just
	// disabling the interrupt should be enough.
	cli();						// Disable interrupts
	TIMSK2 &= ~(1<<OCIE2A);		// Disable all TC2 interrupts are disabled
	sei();						// Set the Global Interrupt Enable Bit
}


/********************************* RTCEnable **********************************/
void LogDateTime::RTCEnable(void)
{
	cli();						// Disable interrupts
	TIMSK2 = (1 << OCIE2A);		// Enable Timer 2 Output Compare Match Interrupt
	sei();						// Set the Global Interrupt Enable Bit
}

/******************* Timer 2 Output Compare Match Interrupt *******************/
ISR(TIMER2_COMPA_vect)
{
	LogDateTime::Tick();
}
#else
/********************************** RTCInit ***********************************/
void LogDateTime::RTCInit(
	time32_t	inTime)
{
	cli();					// Disable interrupts
	TIMSK2 &= ~((1<<TOIE2)|(1<<OCIE2A)|(1<<OCIE2B));		// Make sure all TC2 interrupts are disabled

	ASSR = (1<<AS2);										// set Timer/counter0 to be asynchronous from the CPU clock
															// with a second external clock (32,768kHz)driving it.								
	TCNT2 =0;												// Reset timer counter register

	/*
	*	Prescale to 128 (32768/128 = 256Hz with the 8 bit TCNT2 overflowing at 255 results in 1 tick/second)
	*/	
	TCCR2A = (0<<WGM20) | (0<<WGM21);		// Overflow		
	TCCR2B = ((1<<CS22)|(0<<CS21)|(1<<CS20)|(0<<WGM22));	// Prescale the timer to be clock source/128 to make it
															// exactly 1 second for every overflow to occur
															
	while (ASSR & ((1<<TCN2UB)|(1<<OCR2AUB)|(1<<OCR2BUB)|(1<<TCR2AUB)|(1<<TCR2BUB))){}	//Wait until TC2 is updated
	
	TIMSK2 |= (1<<TOIE2);									// Set 8-bit Timer/Counter0 Overflow Interrupt Enable
	sTime = inTime;
	sei();													// Set the Global Interrupt Enable Bit
	
}

/********************************* RTCDisable *********************************/
void LogDateTime::RTCDisable(void)
{
	// Power-Down mode is currently used to put the MCU to sleep so just
	// disabling the interrupt should be enough.
	cli();						// Disable interrupts
	TIMSK2 &= ~(1<<TOIE2);		// Disable all TC2 interrupts are disabled
	sei();						// Set the Global Interrupt Enable Bit
}

/********************************* RTCEnable **********************************/
void LogDateTime::RTCEnable(void)
{
	cli();						// Disable interrupts
	TIMSK2 |= (1<<TOIE2);		// Set 8-bit Timer/Counter0 Overflow Interrupt Enable
	sei();						// Set the Global Interrupt Enable Bit
}

/************************** Timer/Counter2 Overflow ***************************/
ISR(TIMER2_OVF_vect)
{
	LogDateTime::Tick();
}
#endif
#endif

/********************************** SetTime ***********************************/
void LogDateTime::SetTime(
	time32_t	inTime)
{
	cli();
	sTime = inTime;
	sei();
}

/********************************** SetTime ***********************************/
void LogDateTime::SetTime(
	const char*	inDateStr,
	const char*	inTimeStr)
{
	time32_t time = kYear2000;
	uint8_t	year = StrDecValue(&inDateStr[9]);
	time += (year * 31536000);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    {
		uint8_t	month;
		switch (inDateStr[1]+inDateStr[2])
		{
		case 199:
			month = 1;
			break;
		case 200:
			month = 11;
			break;
		case 207:
			month = 0;
			break;
		case 211:
			month = 2;
			break;
		case 213:
			month = 8;
			break;
		case 215:
			month = 9;
			break;
		case 218:
			month = 4;
			break;
		case 220:
			month = 7;
			break;
		case 225:
			month = 6;
			break;
		case 226:
			month = 3;
			break;
		case 227:
			month = 5;
			break;
		default:
		case 229:
			month = 10;
			break;
		}
		uint16_t	days = pgm_read_word(&kDaysTo[month]) + StrDecValue(&inDateStr[4]);
		/*
		*	If past February AND
		*	year is a leap year THEN
		*	add a day
		*/
		if (month > 2 &&
			(year % 4) == 0)
		{
			days++;
		}
		// Account for leap year days since 2000
		days += (((year+3)/4)-1);
		time += (days * kOneDay);
	}
	time += ((uint32_t)StrDecValue(&inTimeStr[0]) * kOneHour);
	time += (StrDecValue(&inTimeStr[3]) * kOneMinute);
	time += StrDecValue(&inTimeStr[6]);
	sTime = time;
}

/******************************** StrDecValue *********************************/
uint8_t LogDateTime::StrDecValue(
	const char* in2ByteStr)
{
	uint8_t value = 0;
	uint8_t	ch = in2ByteStr[0];
	if (ch >= '0' && ch <= '9')
	{
		value = ch - '0';
	}
	return((value*10) + in2ByteStr[1] - '0');
}

/******************************* TimeComponents *******************************/
void LogDateTime::TimeComponents(
	time32_t	inTime,
	uint8_t&	outHour,
	uint8_t&	outMinute,
	uint8_t&	outSecond)
{
	outSecond = inTime % 60;
	inTime /= 60;
	outMinute = inTime % 60;
	inTime /= 60;
	outHour = inTime % 24;
}

/******************************* DateComponents *******************************/
time32_t LogDateTime::DateComponents(
	time32_t	inTime,
	uint16_t&	outYear,
	uint8_t&	outMonth,
	uint8_t&	outDay)
{
	inTime -= (365*2*kOneDay);	// Start from 1972, the first leap year after 1970
	time32_t	timeComp = inTime%kOneDay;
	inTime -= timeComp;
	outYear = (inTime/kOneYear) + 1972;
	uint16_t	day = ((inTime % kOneYear) / kOneDay) + 1;
	const uint16_t*	daysTo = ((inTime/kOneDay) % kDaysInFourYears) <= 365 ? kDaysToLY : kDaysTo;
	uint8_t month = 1;
	for (; month < 12; month++)
	{
		if (day > pgm_read_word(&daysTo[month]))
		{
			continue;
		}
		break;
	}
	outMonth = month;
	outDay = day - pgm_read_word(&daysTo[month-1]);
	return(timeComp);
}

/******************************* CreateDateStr ********************************/
/*
*	Creates a date string of the form dd-MON-yyyy (12 bytes including nul)
*/
void LogDateTime::CreateDateStr(
	time32_t	inTime,
	char*		outDateStr)
{
	uint16_t	year;
	uint8_t		month;
	uint8_t		day;
	DateComponents(inTime, year, month, day);
	DecStrValue(day, outDateStr);
	outDateStr[2] = '-';
	memcpy_P(&outDateStr[3], &kMonth3LetterAbbr[(month-1)*3], 3);
	outDateStr[6] = '-';
	Uint16ToDecStr(year, &outDateStr[7]);
}

/***************************** CreateDayOfWeekStr *****************************/
/*
*	Creates a day of week string as a 3 letter abbreviation.
*/
void LogDateTime::CreateDayOfWeekStr(
	time32_t	inTime,
	char*		outDayStr)
{
	memcpy_P(outDayStr, &kDay3LetterAbbr[DayOfWeek(inTime)*3], 3);
	outDayStr[3] = 0;
}

/******************************* CreateTimeStr ********************************/
bool LogDateTime::CreateTimeStr(
	time32_t	inTime,
	char*		outTimeStr)
{
	bool notElapsedTime = inTime > kOneYear;
	uint8_t	hour, minute, second;
	TimeComponents(inTime, hour, minute, second);
	bool isPM = hour >= 12;
	/*
	*	If using a 12 hour format AND
	*	this isn't an elapsed time AND
	*	the hour is past noon THEN
	*	adjust the hour.
	*/
	if (!sFormat24Hour &&
		notElapsedTime &&
		hour > 12)
	{
		hour -= 12;
	}
	DecStrValue(hour, outTimeStr);
	outTimeStr[2] = ':';
	DecStrValue(minute, &outTimeStr[3]);
	outTimeStr[5] = ':';
	DecStrValue(second, &outTimeStr[6]);
	outTimeStr[8] = 0;
	return(isPM);
}

/****************************** SDFatDateTimeCB *******************************/
/*
*	SDFat date time callback.
*/
void LogDateTime::SDFatDateTimeCB(
	uint16_t*	outDate,
	uint16_t*	outTime)
{
	SDFatDateTime(Time(), outDate, outTime);
}

/******************************* SDFatDateTime ********************************/
/*
*	SDFat date time callback helper
*/
void LogDateTime::SDFatDateTime(
	time32_t	inTime,
	uint16_t*	outDate,
	uint16_t*	outTime)
{
	uint16_t	year;
	uint8_t		month, day, hour, minute, second;
	TimeComponents(DateComponents(inTime, year, month, day), hour, minute, second);
	*outDate = ((uint16_t)(year - 1980) << 9 | (uint16_t)month << 5 | day);
	*outTime = ((uint16_t)hour << 11 | (uint16_t)minute << 5 | second >> 1);
}

/******************************** DecStrValue *********************************/
void LogDateTime::DecStrValue(
	uint8_t		inDecVal,
	char*		outByteStr)
{
	outByteStr[0] = (inDecVal / 10) + '0';
	outByteStr[1] = (inDecVal % 10) + '0';
}

/******************************* Uint16ToDecStr *******************************/
void LogDateTime::Uint16ToDecStr(
	uint16_t	inNum,
	char*		inBuffer)
{
	for (uint16_t num = inNum; num/=10; inBuffer++);
	inBuffer[1] = 0;
	do
	{
		*(inBuffer--) = (inNum % 10) + '0';
		inNum /= 10;
	} while (inNum);
}

#ifndef __MACH__
/*************************** SetUnixTimeFromSerial ****************************/
void LogDateTime::SetUnixTimeFromSerial(void)
{
	uint32_t	unixTime = LogSerialUtils::GetUInt32FromSerial();

	if (unixTime)
	{
		cli();		// Disable interrupts
		sTime = unixTime;
		sei();		// Enable interrupts
	}
	ResetSleepTime();
}
#endif
/******************************* ResetSleepTime *******************************/
void LogDateTime::ResetSleepTime(void)
{
	sSleepTime = sTime + SLEEP_DELAY;
}
