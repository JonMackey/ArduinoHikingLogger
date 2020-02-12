/*
*	LogSerialUtils.cpp, Copyright Jonathan Mackey 2019
*	Handles serial commands.
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
#include "LogSerialUtils.h"
#include <Arduino.h>

/**************************** GetUInt32FromSerial *****************************/
uint32_t LogSerialUtils::GetUInt32FromSerial(void)
{
	char 		numStr[9];
	uint32_t	retVal = 0;
	if (LoadLine(9, numStr))
	{
		for (uint8_t i = 0; i < 8; i++)
		{
			retVal = (retVal*16) + HexAsciiToBin(numStr[i]);
		}
	}
	return(retVal);
}

/********************************* GetChar ************************************/
uint8_t LogSerialUtils::GetChar(void)
{
	uint32_t	timeout = millis() + 1000;
	while (!Serial.available())
	{
		if (millis() < timeout)continue;
		return('T');
	}
	return(Serial.read());
}

/********************************** LoadLine **********************************/
bool LogSerialUtils::LoadLine(
	uint8_t	inMaxLen,
	char* 	outLine)
{
	char	thisChar = GetChar();
	char*	linePtr = outLine;
	char*	maxLinePtr = &outLine[inMaxLen];
	
	do
	{
		*(linePtr++) = thisChar;
		thisChar = GetChar();
		if (thisChar != '\n')
		{
			if (thisChar != 'T')
			{
				continue;
			}
			outLine[0] = 'T';
		}
		break;
	} while(linePtr < maxLinePtr);
	return(thisChar == '\n');
}
