/*
*	LogDateTime.h, Copyright Jonathan Mackey 2019
*	Class to manage the date and time.
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
#ifndef LogDateTime_h
#define LogDateTime_h

#include <inttypes.h>
/*
*	Rather than use time_t, which can be 32 or 64 bit depending on the target,
*	an explicite 32 bit type is used instead.
*/
typedef uint32_t time32_t;

class LogDateTime
{
public:
	static void				SetTime(
								const char*				inDateStr,
								const char*				inTimeStr);
	static void				SetTime(
								time32_t				inTime);
	static void				TimeComponents(
								time32_t				inTime,
								uint8_t&				outHour,
								uint8_t&				outMinute,
								uint8_t&				outSecond);
	static time32_t			DateComponents(
								time32_t				inTime,
								uint16_t&				outYear,
								uint8_t&				outMonth,
								uint8_t&				outDay);
	static bool				CreateTimeStr(
								char*					outTimeStr)
								{return(CreateTimeStr(sTime, outTimeStr));}
	static bool				CreateTimeStr(
								time32_t				inTime,
								char*					outTimeStr);
	static void				CreateDateStr(
								char*					outDateStr)
								{return(CreateDateStr(sTime, outDateStr));}
	static void				CreateDateStr(
								time32_t				inTime,
								char*					outDateStr);
	static inline uint8_t	DayOfWeek(
								time32_t				inTime)
								{return(((inTime/kOneDay)+4)%7);}
	static void				CreateDayOfWeekStr(
								time32_t				inTime,
								char*					outDayStr);
#ifndef __MACH__
	static void				RTCInit(
								time32_t				inTime = 0);
	static void				RTCEnable(void);
	static void				RTCDisable(void);
#endif
	static inline void		Tick(void)
								{
									sTime++;
									sTimeChanged = true;
								}
	static inline time32_t	Time(void)
								{return(sTime);}
	static inline bool		TimeChanged(void)
								{return(sTimeChanged);}
	static inline void		ResetTimeChanged(void)
								{sTimeChanged = false;}
	static inline bool		Format24Hour(void)
								{return(sFormat24Hour);}
	static inline void		SetFormat24Hour(
								bool					inFormat24Hour)
								{sFormat24Hour = inFormat24Hour;}
	static void				SetUnixTimeFromSerial(void);													
	static void				ResetSleepTime(void);
	static inline bool		TimeToSleep(void)
								{return(sSleepTime < Time());}
	static void				SDFatDateTimeCB(
								uint16_t*				outDate,
								uint16_t*				outTime);
	static void				SDFatDateTime(
								time32_t				inTime,
								uint16_t*				outDate,
								uint16_t*				outTime);
protected:
	static time32_t			sSleepTime;
	static time32_t			sTime;
	static bool				sTimeChanged;
	static bool				sFormat24Hour;	// false = 12, true = 24
	static const uint8_t	kOneMinute;
	static const uint16_t	kOneHour;
	static const uint32_t	kOneDay;
	static const uint32_t	kOneYear;
	static const time32_t	kYear2000;
	static const uint16_t	kDaysTo[];
	static const uint16_t	kDaysToLY[];
	static const char		kMonth3LetterAbbr[];
	static const char		kDay3LetterAbbr[];

	static uint8_t			StrDecValue(
								const char*				in2ByteStr);
	static void				DecStrValue(
								uint8_t					inDecVal,
								char*					outByteStr);
	static void				Uint16ToDecStr(
								uint16_t				inNum,
								char*					inBuffer);
};

#endif // LogDateTime_h
