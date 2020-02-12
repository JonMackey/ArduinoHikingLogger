/*
*	LogTempPres.cpp, Copyright Jonathan Mackey 2019
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

#include "LogTempPres.h"
#include "BMP280Utils.h"
#include "LogDateTime.h"

LogTempPres		LogTempPres::sInstance;
const char LogTempPres::kCTempSuffixStr[] = "°C"; // Degree glyph, U00B0 UTF-8 C2 B0
const char LogTempPres::kFTempSuffixStr[] = "°F";
const char LogTempPres::kFTAltitudeSuffixStr[] = "\'";
const char LogTempPres::kMAltitudeSuffixStr[] = "m";

/******************************** LogTempPres *********************************/
LogTempPres::LogTempPres(void)
	: mTemperature(0), mPressure(0), mSeaLevelPa(0),
		mStartingAltitude(0), mEndingAltitude(0),
		mTemperatureChanged(false), mPressureChanged(false),
		mTempIOAsC(false), mAltitudeIOAsM(false),
		mIsValid(false), mTimePressureChanged(0), mMilestonePercent(100)
{

}

/************************************ Set *************************************/
/*
*	Returns the seconds since the last pressure change (see comment below.)
*/
time32_t LogTempPres::Set(
	int32_t		inTemperature,
	uint32_t	inPressure)
{
	if (!mTemperatureChanged)
	{
		mTemperatureChanged = mTemperature != inTemperature;
	}
	mTemperature = inTemperature;
	bool	pressureChanged = mPressure != inPressure;
	if (!mPressureChanged && pressureChanged)
	{
		mPressureChanged = true;
	}
 	mPressure = inPressure;
 	mIsValid = inPressure != 0;
	/*
	*	mTimePressureChanged is used to determine if the hiker has stopped and
	*	forgot to stop the logger.  When the pressure doesn't change for more
	*	than the timeout, the logger will automaticaly be stopped
	*	and mTimePressureChanged will become the end time.
	*/
	if (pressureChanged)
	{
		mTimePressureChanged = LogDateTime::Time();
	}
 	return(pressureChanged ? 0 : (LogDateTime::Time() - mTimePressureChanged));
}

/**************************** SetStartingAltitude *****************************/
void LogTempPres::SetStartingAltitude(
	float	inAltitude)
{
	mStartingAltitude = mAltitudeIOAsM ? inAltitude : (inAltitude * 0.3084);
	SetSeaLevelPressure();
}

/***************************** SetEndingAltitude ******************************/
void LogTempPres::SetEndingAltitude(
	float	inAltitude)
{
	mEndingAltitude = mAltitudeIOAsM ? inAltitude : (inAltitude * 0.3084);
}

/**************************** SetSeaLevelPressure *****************************/
/*
*	The sea level calculation is based on the starting altitude.
*/
void LogTempPres::SetSeaLevelPressure(void)
{
	mSeaLevelPa = BMP280Utils::CalcSeaLevelForAltitude(mStartingAltitude,
								((float)mPressure)/100.0);
}

/**************************** CalcCurrentAltitude *****************************/
float LogTempPres::CalcCurrentAltitude(void)
{
	return(BMP280Utils::CalcAltitude(mSeaLevelPa, ((float)Pressure())/100.0));
}

/******************************* CreateTempStr ********************************/
uint8_t LogTempPres::CreateTempStr(
	char*	outTempStr)
{
	return(CreateTempStr(Temperature(), outTempStr));
}

/******************************* CreateTempStr ********************************/
uint8_t LogTempPres::CreateTempStr(
	int32_t	inTemp,
	char*	outTempStr)
{
	return(BMP280Utils::Int32ToDec21Str((mTempIOAsC ? inTemp : ((inTemp * 9) / 5) + 3200), outTempStr));
}

/********************************** Altitude **********************************/
uint32_t LogTempPres::Altitude(void) const
{
	float altitude = BMP280Utils::CalcAltitude(mSeaLevelPa, ((float)PeekPressure())/100.0);
	if (!mAltitudeIOAsM)
	{
		altitude /= 0.3084;
	}
	return((int32_t)(altitude * 100));
}

/***************************** CreateAltitudeStr ******************************/
uint8_t LogTempPres::CreateAltitudeStr(
	float	inAltitudeInMeters,
	char*	outAltitudeStr)
{
	uint8_t	strLen;
	char suffixChar;
	if (mAltitudeIOAsM)
	{
		strLen = BMP280Utils::Int32ToDec21Str((int32_t)(inAltitudeInMeters * 100), outAltitudeStr);
		suffixChar = 'm';
	} else
	{
		strLen = BMP280Utils::Int32ToIntStr((int32_t)((inAltitudeInMeters/0.3084) * 100), outAltitudeStr);
		suffixChar = '\'';
	}
	outAltitudeStr[strLen] = suffixChar;
	strLen++;
	outAltitudeStr[strLen] = 0;
	return(strLen);
}

/************************ CreateAltitudePercentageStr *************************/
uint8_t LogTempPres::CreateAltitudePercentageStr(
	float	inAltitude,	// in meters
	char*	outAltitudePercentageStr)
{
	float	elevPercent = 0;
	if (inAltitude)
	{
		float	elevGain = mEndingAltitude - mStartingAltitude;
		if (elevGain)
		{
			elevPercent = (inAltitude - mStartingAltitude)/elevGain;
		}
	}
	uint8_t	strLen = BMP280Utils::Int32ToIntStr((int32_t)(elevPercent * 10000), outAltitudePercentageStr);
	outAltitudePercentageStr[strLen] = '%';
	strLen++;
	outAltitudePercentageStr[strLen] = 0;
	return(strLen);
}

/**************************** SetMilestonePressure ****************************/
void LogTempPres::SetMilestonePressure(void)
{
	if (mMilestonePercent < 100)
	{
		float	elevGain = mEndingAltitude - mStartingAltitude;
		float	milestoneAltitude = mStartingAltitude + (elevGain * ((float)mMilestonePercent/100));
		mMilestonePressure = BMP280Utils::CalcPressureForAltitude(milestoneAltitude, mSeaLevelPa) * 100;
	}
}

/****************************** PassedMilestone *******************************/
uint8_t LogTempPres::PassedMilestone(void)
{
	uint8_t	passedMilestone = 0;
	/*
	*	If not at 100% AND
	*	a milestone has passed THEN
	*	return the milestone passed and set the next milestone.
	*/
	if (mMilestonePercent < 100 &&
		(Ascending() ? (mPressure < mMilestonePressure) :
							(mPressure > mMilestonePressure)))
	{
		passedMilestone = mMilestonePercent;
		mMilestonePercent += mMilestoneIncrement;
		SetMilestonePressure();
	}
	return(passedMilestone);
}

/******************************* ResetMilestone *******************************/
void LogTempPres::ResetMilestone(
	uint8_t	inIncrementPercent)
{
	mMilestonePercent = mMilestoneIncrement = inIncrementPercent;
	SetMilestonePressure();
}
