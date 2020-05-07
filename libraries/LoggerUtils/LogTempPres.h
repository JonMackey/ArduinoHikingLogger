/*
*	LogTempPres.h, Copyright Jonathan Mackey 2019
*	Handles the data returned by the BMP280 sensor
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
#ifndef LogTempPres_h
#define LogTempPres_h

#include <inttypes.h>
typedef uint32_t time32_t;

class LogTempPres
{
public:
							LogTempPres(void);
	time32_t				Set(
								int32_t					inTemperature,
								uint32_t				inPressure);
	void					SetTempUnit(
								bool					inAsC)	// false = F
								{mTempIOAsC = inAsC;}
	inline bool				TempIOAsC(void) const
								{return(mTempIOAsC);}
	void					SetAltitudeUnit(
								bool					inAsM)	// false = Feet
								{mAltitudeIOAsM = inAsM;}
	inline bool				AltitudeIOAsM(void) const
								{return(mAltitudeIOAsM);}
	inline bool				TemperatureChanged(void) const
								{return(mTemperatureChanged);}
	inline bool				PressureChanged(void) const
								{return(mPressureChanged);}
	inline bool				IsValid(void) const
								{return(mIsValid);}
							/*
							*	MakeInvalid is called when the BMP280 remote
							*	fails to send.  mIsValid will be set to true
							*	by Set() once the BMP280 starts sending again.
							*/
	inline void				MakeInvalid(void)
								{mIsValid = false;}
	inline void				SetChanged(void)
								{mTemperatureChanged = true; mPressureChanged = true;}
	inline int32_t			Temperature(void)
								{mTemperatureChanged = false;
								 return(mTemperature);}
	inline int32_t			PeekTemperature(void) const
								{return(mTemperature);}
	inline uint32_t			Pressure(void)
								{mPressureChanged = false; return(mPressure);}
	inline uint32_t			PeekPressure(void) const
								{return(mPressure);}
	inline time32_t			TimePressureChanged(void) const
								{return(mTimePressureChanged);}
	void					SetStartingAltitude(
								float					inAltitude);
	float					StartingAltitude(void) const
								{return(mStartingAltitude);}
	void					SetEndingAltitude(
								float					inAltitude);
	float					EndingAltitude(void) const
								{return(mEndingAltitude);}
	inline bool				Ascending(void) const
								{return(mEndingAltitude > mStartingAltitude);}
	void					SetSeaLevelPressure(void);
	float					CalcCurrentAltitude(void);
	uint32_t				Altitude(void) const;	// In current units determined by mAltitudeIOAsM
	uint8_t					CreateTempStr(
								char*					outTempStr);
	uint8_t					CreateTempStr(
								int32_t					inTemp,
								char*					outTempStr);
	inline const char*		GetTempSuffixStr(void) const
								{return(mTempIOAsC ? kCTempSuffixStr : kFTempSuffixStr);}
	uint8_t					CreateAltitudeStr(
								float					inAltitudeInMeters,
								char*					outAltitudeStr);
	inline const char*		GetAltitudeSuffixStr(void) const
								{return(mAltitudeIOAsM ? kMAltitudeSuffixStr : kFTAltitudeSuffixStr);}
	uint8_t					CreateAltitudePercentageStr(
								float					inAltitudeInMeters,
								char*					outAltitudePercentageStr);
	uint8_t					PassedMilestone(void);
							// ResetMilestone should be called after starting
							// and ending altitude is set.
	void					ResetMilestone(
								uint8_t					inIncrementPercent = 25);
	bool					PassedAllMilestones(void)
								{return(mMilestonePercent >= 100);}
	static inline LogTempPres&	GetInstance(void)
								{return(sInstance);}
protected:
	static LogTempPres	sInstance;
	float		mSeaLevelPa;
	float		mStartingAltitude;	// in meters
	float		mEndingAltitude;	// in meters
	int32_t		mTemperature;		// current temperature
	uint32_t	mPressure;			// current pressure
	uint32_t	mMilestonePressure;
	time32_t	mTimePressureChanged;
	uint8_t		mMilestonePercent;		// Current
	uint8_t		mMilestoneIncrement;	// As a percent, e.g. 25 = 25%
	bool		mTemperatureChanged;
	bool		mPressureChanged;
	bool		mTempIOAsC;
	bool		mAltitudeIOAsM;
	bool		mIsValid;
	static const char kCTempSuffixStr[];
	static const char kFTempSuffixStr[];
	static const char kFTAltitudeSuffixStr[];
	static const char kMAltitudeSuffixStr[];

	void					SetMilestonePressure(void);
};

#endif // LogTempPres_h
