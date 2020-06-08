/*
*	LogLayout.h, Copyright Jonathan Mackey 2019
*	Manages the display layout
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
#ifndef LogLayout_h
#define LogLayout_h

#include "XFont.h"

typedef uint32_t time32_t;
class LogAction;
class HikeLog;

class LogLayout : public XFont
{
public:
	struct SString_PDesc
	{
		const char*	descStr;
		uint16_t	color;
	};
							LogLayout(void);
	void					Initialize(
								LogAction*				inLogAction,
								HikeLog*				inHikeLog,
								DisplayController*		inDisplay,
								Font*					inNormalFont,
								Font*					inSmallFont);
							/*
							*	When inUpdateAll is false, only whatever is
							*	dirty is updated.  When  inUpdateAll is true
							*	the entire display is redrawn.
							*/
	void					Update(
								bool					inUpdateAll = false);
protected:
	LogAction*	mLogAction;
	HikeLog*	mHikeLog;
	Font*		mNormalFont;
	Font*		mSmallFont;
	uint8_t		mLogState;
	uint8_t		mPrevMode;
	uint16_t	mLocIndex;
	uint16_t	mHikeRef;
	uint8_t		mShowingAMPM;
	uint8_t		mSyncState;
	uint8_t		mResetLogState;
	uint8_t		mSDCardState;
	uint8_t		mSDCardAction;
	uint8_t		mReviewState;

	void					DrawLocation(
								uint16_t				inLocIndex,
								uint8_t					inFirstLine = 1);
	void					DrawTime(
								time32_t				inTime,
								bool					inShowingAMPM);
	void					DrawIndexedDescStr(
								const SString_PDesc*	inStringList,
								uint8_t					inStrIndex,
								bool					inHasOptions,
								bool					inCentered);
	void					DrawTextOption(
								const char*				inStrRef,
								uint16_t				inColor,
								bool					inHasOptions,
								bool					inCentered);
	void					ClearLines(
								uint8_t					inStartLine = 0,
								uint8_t					inNumLines = 3);

};

#endif // LogLayout_h
