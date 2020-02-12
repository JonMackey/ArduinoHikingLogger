/*
*	RemoteLogLayout.h, Copyright Jonathan Mackey 2019
*	Manages the remote display layout
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
#ifndef RemoteLogLayout_h
#define RemoteLogLayout_h

#include "XFont.h"
class RemoteLogAction;
class RemoteHikeLog;
class LogTempPres;

//#define DEBUG_RADIO	1
class RemoteLogLayout : public XFont
{
public:
							RemoteLogLayout(void);
	void					Initialize(
								RemoteLogAction*		inLogAction,
								RemoteHikeLog*			inHikeLog,
								DisplayController*		inDisplay,
								Font*					inNormalFont,
								Font*					inSmallFont);
	void					Update(
								bool					inUpdateAll);
protected:
	RemoteLogAction*	mLogAction;
	RemoteHikeLog*		mHikeLog;
	Font*				mNormalFont;
	Font*				mSmallFont;
	uint8_t				mLogState;
	uint8_t				mMode;
	uint16_t			mLocIndex;
	uint8_t				mShowingAMPM;
	uint8_t				mSyncState;
	uint8_t				mBusy;
	uint8_t				mPacketsInQueue;
#ifdef DEBUG_RADIO
	uint16_t			mPacketTimeouts;
	uint8_t				mWaitingForPacket;
#endif

	void					DrawLocation(
								bool					inStart);
	void					ClearLines1to3(void);
};

#endif // RemoteLogLayout_h
