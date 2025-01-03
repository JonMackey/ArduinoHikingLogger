/*
*	UnixTime.cpp, Copyright Jonathan Mackey 2021
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
#include "UnixTime.h"
#ifndef __MACH__
#include <Arduino.h>
//#include <avr/pgmspace.h>
#include "SerialUtils.h"
#include "DS3231SN.h"
#else
#include <iostream>
#include <CoreFoundation/CFTimeZone.h>
#define PROGMEM
#define pgm_read_word(xx) *(xx)
#define pgm_read_byte(xx) *(xx)
#define memcpy_P memcpy
#define cli()
#define sei()
#endif

#ifndef __MACH__
DS3231SN*		UnixTime::sExternalRTC;
#endif
time32_t		UnixTime::sTime;
bool			UnixTime::sTimeChanged;

// SLEEP_DELAY : If no activity after SLEEP_DELAY seconds, go to sleep
// This can also be set via UnixTime::SetSleepDelay()
#define SLEEP_DELAY	90
time32_t			UnixTime::sSleepTime;
uint32_t			UnixTime::sSleepDelay = SLEEP_DELAY;
// date +%s		<< Unix command to get the local time

// https://www.epochconverter.com
// Unix time 0 is January 1, 1970 12:00:00 AM, a Thursday

const uint8_t	UnixTime::kOneMinute = 60;
const uint16_t	UnixTime::kOneHour = 3600;
const uint32_t	UnixTime::kOneDay = 86400;
const uint32_t	kDaysInFourYears = 1461;
const uint32_t	UnixTime::kOneYear = 31557600;
const time32_t	UnixTime::kYear2000 = 946684800;	// Seconds from 1970 to 2000
const uint8_t	UnixTime::kDaysInMonth[] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
const uint16_t	UnixTime::kDaysTo[] PROGMEM = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
const uint16_t	UnixTime::kDaysToLY[] PROGMEM = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
bool			UnixTime::sFormat24Hour;	// false = 12, true = 24
const char		UnixTime::kMonth3LetterAbbr[] PROGMEM = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
const char		UnixTime::kDay3LetterAbbr[] PROGMEM = "SUNMONTUEWEDTHUFRISAT";


#ifndef __MACH__
/**************************** DSDateTimeToUnixTime ****************************/
time32_t UnixTime::DSDateTimeToUnixTime(
	const DSDateTime&	inDSDateTime)
{
	time32_t time = kYear2000;
	time += (inDSDateTime.dt.year * 31536000);
	{
		uint16_t	days = pgm_read_word(&kDaysTo[inDSDateTime.dt.month-1])
								+ inDSDateTime.dt.date;
		/*
		*	If past February AND
		*	year is a leap year THEN
		*	add a day
		*/
		if (inDSDateTime.dt.month > 2 &&
			(inDSDateTime.dt.year % 4) == 0)
		{
			days++;
		}
		// Account for leap year days since 2000
		days += (((inDSDateTime.dt.year+3)/4)-1);
		time += (days * kOneDay);
	}
	time += (((uint32_t)inDSDateTime.dt.hour) * kOneHour);
	time += (inDSDateTime.dt.minute * kOneMinute);
	time += inDSDateTime.dt.second;
	return(time);
}

/**************************** UnixTimeToDSDateTime ****************************/
void UnixTime::UnixTimeToDSDateTime(
	time32_t	inTime,
	DSDateTime&	outDSDateTime)
{
	uint16_t	year;
	TimeComponents(DateComponents(inTime, year,
									outDSDateTime.dt.month,
									outDSDateTime.dt.date),
									outDSDateTime.dt.hour,
									outDSDateTime.dt.minute,
									outDSDateTime.dt.second);
	outDSDateTime.dt.day = DayOfWeek(inTime)+1;
	outDSDateTime.dt.year = year - 2000;
}

/********************************** SetTime ***********************************/
void UnixTime::SetTime(
	time32_t	inTime)
{
	#if defined ESP_H
		sTime = inTime;
	#elif  defined _STM32_DEF_
		sTime = inTime;
	#else
		cli();
		sTime = inTime;
		sei();
	#endif
	if (sExternalRTC)
	{
		DSDateTime	dateAndTime;
		UnixTimeToDSDateTime(inTime, dateAndTime);
		sExternalRTC->SetTime(dateAndTime);
	}
}
#endif

#ifdef __MACH__
/*************************** SetTimeFromExternalRTC ***************************/
void UnixTime::SetTimeFromExternalRTC(void)
{
	CFTimeZoneRef tz = CFTimeZoneCopySystem();
	time32_t localTime = (time32_t)(time(nullptr) + CFTimeZoneGetSecondsFromGMT(tz, 0));
	CFRelease(tz);
#if defined ESP_H
	sTime = localTime;
#elif  defined _STM32_DEF_
	sTime = localTime;
#else
	cli();
	sTime = localTime;
	sei();
#endif
}
#else
/*************************** SetTimeFromExternalRTC ***************************/
void UnixTime::SetTimeFromExternalRTC(void)
{
	if (sExternalRTC)
	{
		DSDateTime	dateAndTime;
		sExternalRTC->GetTime(dateAndTime);

		time32_t time = DSDateTimeToUnixTime(dateAndTime);
		
	#if defined ESP_H
		sTime = time;
	#elif  defined _STM32_DEF_
		sTime = time;
	#else
		cli();
		sTime = time;
		sei();
	#endif
	}
}
#endif

/****************************** StringToUnixTime ******************************/
/*
*	Converts a cell modem time string to Unix time.
*
*	This conversion is for values returned by cell modem command AT+CCLK and the
*	unsolicited *PSUTTZ automatic time update.  For CCLK inDateTimeStr is local
*	time of the form YY/MM/DD,hh:mm:ss+-uu", where +-uu is the deviation from
*	GMT in quarter hours for the local timezone.  The timezone is ignored for
*	CCLK because only the local time is needed. For *PSUTTZ, inDateTimeStr is
*	UTC of the form YY/MM/DD,hh:mm:ss","+-uu",DST, where +-uu is the deviation
*	from GMT in quarter hours for the local timezone, and DST is 0/1 signifying
*	the whether daylight savings time is active for this timezone.
*	DST is ignored.
*	
*	The timezone is in quarter hours off of GMT.  e.g. -18 = -04:30:0.  
*
*	When parsing for timezone, quotes and commas are ignored.
*	No format validation is performed (shit in, shit out.)
*
*	Ex 21/07/26,13:25:53-16" = 26-JUL-2021 1:25PM
*	If inAdjustForTimezone
*		21/07/26,17:25:53","-16",1 = 26-JUL-2021 1:25PM EST, DST is active
*/
time32_t UnixTime::StringToUnixTime(
	const char*	inDateTimeStr,
	bool		inAdjustForTimezone)
{
	time32_t time = kYear2000;
	uint8_t	year = StrDecValue(&inDateTimeStr[0]);
	if (year < 80)
	{
		time += (year * 31536000);
		{
			uint8_t		month = StrDecValue(&inDateTimeStr[3]) -1;
			uint16_t	days = pgm_read_word(&kDaysTo[month]) + StrDecValue(&inDateTimeStr[6]);
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
		time += ((uint32_t)StrDecValue(&inDateTimeStr[9]) * kOneHour);
		time += (StrDecValue(&inDateTimeStr[12]) * kOneMinute);
		time += StrDecValue(&inDateTimeStr[15]);
		if (inAdjustForTimezone)
		{
			const char*	tzPtr = &inDateTimeStr[17];
			char thisChar = *(tzPtr++);
			for (; thisChar; thisChar = *(tzPtr++))
			{
				// Ignore quotes and commas
				if (thisChar == '\"' ||
					thisChar == ',')
				{
					continue;
				}
				uint16_t tzAdjustment = StrDecValue(tzPtr) * kOneMinute * 15;
				if (thisChar == '-')
				{
					time -= tzAdjustment;
				} else
				{
					time += tzAdjustment;
				}
				break;
			}
		}
	} else
	{
		// Only happens after the battery has been removed and there has been
		// no connection to a cell tower or automatic time updates have not been
		// enabled by sending AT+CLTS=1 to the modem.
		time = 0;
	}
	return(time);
}

/****************************** StringToUnixTime ******************************/
/*
*	Converts a pair of strings to Unix time.
*	inDateStr is of the form Mmm-DD-YYYY, where the month is a 3 letter English
*	abbreviation.  The last two letters must be lowercase.
*/
time32_t UnixTime::StringToUnixTime(
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
	return(time);
}

/******************************** StrDecValue *********************************/
uint8_t UnixTime::StrDecValue(
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

/******************************** ToComponents ********************************/
void UnixTime::ToComponents(
	time32_t		inTime,
	SComponents&	outComponents)
{
	TimeComponents(DateComponents(inTime,
									outComponents.year,
									outComponents.month,
									outComponents.day),
									outComponents.hour,
									outComponents.minute,
									outComponents.second);
}

/******************************* FromComponents *******************************/
time32_t UnixTime::FromComponents(
	const SComponents&	inComponents)
{
	time32_t time = kYear2000;
	uint16_t	year = inComponents.year % 2000;
	time += (year * 31536000);
	{
		uint16_t	days = pgm_read_word(&kDaysTo[inComponents.month-1])
								+ inComponents.day;
		/*
		*	If past February AND
		*	year is a leap year THEN
		*	add a day
		*/
		if (inComponents.month > 2 &&
			(year % 4) == 0)
		{
			days++;
		}
		// Account for leap year days since 2000
		days += (((year+3)/4)-1);
		time += (days * kOneDay);
	}
	time += (((uint32_t)inComponents.hour) * kOneHour);
	time += (inComponents.minute * kOneMinute);
	time += inComponents.second;
	return(time);
}

/******************************* TimeComponents *******************************/
void UnixTime::TimeComponents(
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
time32_t UnixTime::DateComponents(
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
void UnixTime::CreateDateStr(
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

/******************************* CreateMonthStr *******************************/
/*
*	Returns a month string as a 3 letter uppercase English abbreviation.
*	No validity checking on inMonth is performed.  inMonth should be a number
*	from 1 to 12.
*/
void UnixTime::CreateMonthStr(
	uint8_t		inMonth,
	char*		outMonthStr)
{
	memcpy_P(outMonthStr, &kMonth3LetterAbbr[(inMonth-1)*3], 3);
	outMonthStr[3] = 0;
}

/***************************** CreateDayOfWeekStr *****************************/
/*
*	Creates a day of week string as a 3 letter uppercase English abbreviation.
*/
void UnixTime::CreateDayOfWeekStr(
	time32_t	inTime,
	char*		outDayStr)
{
	memcpy_P(outDayStr, &kDay3LetterAbbr[DayOfWeek(inTime)*3], 3);
	outDayStr[3] = 0;
}

/***************************** DaysInMonthForYear *****************************/
/*
*	Returns the number of days in inMonth for inYear.
*	No validation of inMonth or inYear is performed.
*/
uint8_t UnixTime::DaysInMonthForYear(
	uint8_t		inMonth,
	uint16_t	inYear)
{
	uint8_t	daysInMonth;
	if (inMonth != 2 ||
		(inYear % 4) != 0)
	{
		daysInMonth = pgm_read_byte(&kDaysInMonth[inMonth-1]);
	} else
	{
		daysInMonth = 29;
	}
	return(daysInMonth);
}

/******************************* CreateTimeStr ********************************/
bool UnixTime::CreateTimeStr(
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

/******************************** DecStrValue *********************************/
void UnixTime::DecStrValue(
	uint8_t		inDecVal,
	char*		outByteStr)
{
	outByteStr[0] = (inDecVal / 10) + '0';
	outByteStr[1] = (inDecVal % 10) + '0';
}

/******************************* Uint16ToDecStr *******************************/
void UnixTime::Uint16ToDecStr(
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

/********************************** SetTime ***********************************/
void UnixTime::SetTime(
	const char*	inDateStr,
	const char*	inTimeStr)
{
	sTime = StringToUnixTime(inDateStr, inTimeStr);
}

/******************************* SDFatDateTime ********************************/
/*
*	SDFat date time callback helper
*/
void UnixTime::SDFatDateTime(
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

/****************************** SDFatDateTimeCB *******************************/
/*
*	SDFat date time callback.
*/
void UnixTime::SDFatDateTimeCB(
	uint16_t*	outDate,
	uint16_t*	outTime)
{
	SDFatDateTime(Time(), outDate, outTime);
}

#ifndef __MACH__
/*************************** SetUnixTimeFromSerial ****************************/
void UnixTime::SetUnixTimeFromSerial(void)
{
	uint32_t	unixTime = SerialUtils::GetUInt32FromSerial();

	if (unixTime)
	{
		SetTime(unixTime);
	}
	ResetSleepTime();
}
#endif

/******************************* SetSleepDelay ********************************/
void UnixTime::SetSleepDelay(
	uint32_t	inDelaySeconds)
{
	sSleepDelay = inDelaySeconds;
}

/******************************* ResetSleepTime *******************************/
void UnixTime::ResetSleepTime(void)
{
	sSleepTime = sTime + sSleepDelay;
}

