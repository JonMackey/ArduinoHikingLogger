/*
*	HikeLocations.cpp, Copyright Jonathan Mackey 2019
*	Class to manage hike locations.
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
#include "HikeLocations.h"
#include "DataStream.h"
#include <string.h>

#ifndef __MACH__
#include <Arduino.h>
#include "SdFat.h"
#include "sdios.h"
#include "CompileTime.h"
#else
#include <stdio.h>
#endif
#include "CSVUtils.h"


HikeLocations	HikeLocations::sInstance;

/******************************** HikeLocations ********************************/
HikeLocations::HikeLocations(void)
	: mLocations(0), mCurrentIndex(0), mCount(0)
{
}

/********************************* Initialize *********************************/
void HikeLocations::Initialize(
	DataStream*	inLocations,
	uint8_t		inSDSelectPin)
{
	mSDSelectPin = inSDSelectPin;
	mLocations = inLocations;
	if (mLocations)
	{
		SHikeLocationRoot	root;
		ReadLocation(0, &root);
		if (root.tail)
		{
			uint16_t	count = 1;
			ReadLocation(root.tail, &mCurrent);
			while (mCurrent.prev)
			{
				count++;
				ReadLocation(mCurrent.prev, &mCurrent);
			}
			mCurrentIndex = root.head;
			mCount = count;
		}
	}
}

/******************************** GetNextIndex ********************************/
/*
*	Gets the next sorted physical index without loading
*/
uint16_t HikeLocations::GetNextIndex(
	bool	inWrap) const
{
	uint16_t	index = 0;
	if (mCurrentIndex != 0)
	{
		if (mCurrent.next)
		{
			index = mCurrent.next;
		} else if (inWrap &&
			mCurrent.prev)
		{
			SHikeLocationRoot	root;
			ReadLocation(0, &root);
			index = root.head;
		}
	}
	return(index);
}

/************************************ Next ************************************/
bool HikeLocations::Next(
	bool	inWrap)
{
	uint16_t	nextindex = GetNextIndex(inWrap);
	bool	success = nextindex != 0;
	if (success)
	{
		GoToLocation(nextindex);
	}
	return(success);
}

/****************************** GetPreviousIndex ******************************/
uint16_t HikeLocations::GetPreviousIndex(
	bool	inWrap) const
{
	uint16_t	index = 0;
	if (mCurrentIndex != 0)
	{
		if (mCurrent.prev)
		{
			index = mCurrent.prev;
		} else if (inWrap &&
			mCurrent.next)
		{
			SHikeLocationRoot	root;
			ReadLocation(0, &root);
			index = root.tail;
		}
	}
	return(index);
}

/********************************** Previous **********************************/
bool HikeLocations::Previous(
	bool	inWrap)
{
	uint16_t	prevIndex = GetPreviousIndex(inWrap);
	bool	success = prevIndex != 0;
	if (success)
	{
		GoToLocation(prevIndex);
	}
	return(success);
}

/******************************** GoToLocation ********************************/
/*
*	Does no bounds checking.
*/
void HikeLocations::GoToLocation(
	uint16_t	inRecIndex)
{
	if (mCurrentIndex != inRecIndex)
	{
		mCurrentIndex = inRecIndex;
		ReadLocation(inRecIndex, &mCurrent);
	}
}

/****************************** GoToNthLocation *******************************/
/*
*	Returns true if there is an Nth record.
*/
bool HikeLocations::GoToNthLocation(
	uint16_t	inLogIndex)
{
	SHikeLocationRoot	root;
	ReadLocation(0, &root);
	bool	success = root.head != 0;
	if (success)
	{
		GoToLocation(root.head);
		
		for (uint16_t i = 0; i < inLogIndex; i++)
		{
			if (mCurrent.next)
			{
				GoToLocation(mCurrent.next);
			} else
			{
				success = false;
				break;
			}
		}
	}
	return(success);
}

/**************************** GoToRelativeLocation ****************************/
/*
*	Starting from the current location, go to the logical position relative to
*	the current location by inRelLogIndex. 
*/
bool HikeLocations::GoToRelativeLocation(
	int16_t	inRelLogIndex)
{
	bool success = true;
	if (inRelLogIndex > 0)
	{
		do
		{
			success = Next(false);
			inRelLogIndex--;
		} while (success && inRelLogIndex);
	} else if (inRelLogIndex < 0)
	{
		do
		{
			success = Previous(false);
			inRelLogIndex++;
		} while (success && inRelLogIndex);
	}
	return(success);
}

/****************************** GetLogicalIndex *******************************/
/*
*	Starting from the root head, count indexes till the current location is hit.
*	-1 is returned if there is no current.  This should only happen when there
*	are no locations.
*/
int16_t HikeLocations::GetLogicalIndex(void) const
{
	int16_t	logIndex = 0;
	if (mCurrentIndex)
	{
		SHikeLocationLink	thisLocation;
		uint16_t	next = 0;
		uint16_t	currentPrev = mCurrent.prev;
		while (next != currentPrev)
		{
			logIndex++;
			ReadLocation(next, &thisLocation);
			next = thisLocation.next;
		}
	} else
	{
		logIndex = -1;
	}
	return(logIndex);
}

/******************************** SkipMTPrefix ********************************/
const char*	HikeLocations::SkipMTPrefix(
	const char*	inName)
{
	return (strncmp(inName, "MT ", 3) != 0 ? inName : &inName[3]);
}

/************************************ Add *************************************/
/*
*	The added location becomes the current location.
*/
uint16_t HikeLocations::Add(
	SHikeLocationLink&	inLocation)
{
	int16_t leftIndex = 0;
	int16_t	currLogIndex = GetLogicalIndex();
	if (currLogIndex >= 0)
	{
		int16_t current = 0;
		int16_t rightIndex = mCount -1;
		const char*	locationName = SkipMTPrefix(inLocation.loc.name);
		while (leftIndex <= rightIndex)
		{
			current = (leftIndex + rightIndex) / 2;
			GoToRelativeLocation(current - currLogIndex);
			currLogIndex = current;
			
			int	cmpResult = strcmp(SkipMTPrefix(mCurrent.loc.name), locationName);
			if (cmpResult == 0)
			{
				leftIndex = current;
				break;
			} else if (cmpResult > 0)
			{
				rightIndex = current - 1;
			} else
			{
				leftIndex = current + 1;
			}
		}
	}
	SHikeLocationRoot	root;
	ReadLocation(0, &root);
	uint16_t	newIndex = root.freeHead;
	if (newIndex)
	{
		SHikeLocationLink	freeLocation;
		ReadLocation(newIndex, &freeLocation);
		root.freeHead = freeLocation.next;
		WriteLocation(0, &root);
		mCount++;
	} else
	{
		mLocations->Seek(0, DataStream::eSeekEnd);
		uint16_t	maxLocations = mLocations->GetPos()/sizeof(SHikeLocationLink)-1;
		if (maxLocations > mCount)
		{
			mCount++;
			newIndex = mCount;
		}
	}

	if (newIndex)
	{
		if (mCurrentIndex)
		{
			leftIndex--;
			/*
			*	If the new location it to be inserted after an existing location...
			*/
			if (leftIndex >= 0)
			{
				GoToRelativeLocation(leftIndex - currLogIndex);
				inLocation.prev = mCurrentIndex;
				inLocation.next = mCurrent.next;
				mCurrent.next = newIndex;
				WriteLocation(mCurrentIndex, &mCurrent);
				if (inLocation.next != 0)
				{
					GoToLocation(inLocation.next);
					mCurrent.prev = newIndex;
					WriteLocation(inLocation.next, &mCurrent);
				} else
				{
					root.tail = newIndex;
					WriteLocation(0, &root);
				}
			/*
			*	Else, this is the new head.
			*	Load and update the root and the previous head location.
			*/
			} else
			{
				GoToLocation(root.head);
				inLocation.prev = 0;
				inLocation.next = root.head;
				root.head = newIndex;
				mCurrent.prev = newIndex;
				WriteLocation(mCurrentIndex, &mCurrent);
				WriteLocation(0, &root);
			}
		/*
		*	Else the list is empty.
		*	Initialize a new list.
		*/
		} else
		{
			inLocation.prev = 0;
			inLocation.next = 0;
			root.head = newIndex;
			root.tail = newIndex;
			WriteLocation(0, &root);
		}
		WriteLocation(newIndex, &inLocation);
		GoToLocation(newIndex);
	}
	return(newIndex);
}

/******************************* RemoveCurrent ********************************/
/*
*	Removes the current location by adding it to the free linked list.
*	Returns true if the current location was removed.
*	If there is a next location, the next location becomes the current else
*	the previous becomes the current, if one exists.
*	If no current then the current index is 0 (root).
*/
bool HikeLocations::RemoveCurrent(void)
{
	bool	success = mCurrentIndex != 0;
	if (success)
	{
		// Save the current prev and next indexes
		uint16_t	prev = mCurrent.prev;
		uint16_t	next = mCurrent.next;
		// Load the root
		SHikeLocationRoot	root;
		ReadLocation(0, &root);
	
		// Add current to the free list as the new head.
		mCurrent.next = root.freeHead;
		mCurrent.prev = 0;	// The free list is one way.
		WriteLocation(mCurrentIndex, &mCurrent);
		root.freeHead = mCurrentIndex;

		/*
		*	If the previous location isn't the root THEN
		*	update the previous location's next field.
		*/
		if (prev != 0)
		{
			ReadLocation(prev, &mCurrent);
			mCurrent.next = next;
			WriteLocation(prev, &mCurrent);
		/*
		*	Else, update the root's head.
		*/
		} else
		{
			root.head = next;
		}
		/*
		*	If there is a next location THEN
		*	update the next location's prev field.
		*	The next location becomes the new current location.
		*/
		if (next != 0)
		{
			ReadLocation(next, &mCurrent);
			mCurrent.prev = prev;
			WriteLocation(next, &mCurrent);
			mCurrentIndex = next;
		/*
		*	Else update the root tail to point to the prev location.
		*	The prev location becomes the new current location.
		*/
		} else
		{
			root.tail = prev;
			mCurrentIndex = prev;
		}
		
		// Write the updated root.
		WriteLocation(0, &root);
		mCount--;
	}
	return(success);
}

/******************************** IsValidIndex ********************************/
/*
*	Returns true if the passed physical record index is valid.  In order to
*	determine this, the link list needs to be walked till the index is found.
*	You can't simply check to see if it's less than the logical count because
*	physical indexes don't move when a location is removed, the index is only added
*	to the freeHead chain.
*/
bool HikeLocations::IsValidIndex(
	uint16_t	inRecIndex)
{
	bool isValid = false;
	if (inRecIndex > 0)
	{
		SHikeLocationRoot	root;
		ReadLocation(0, &root);
		SHikeLocationLink	link;
		uint16_t	next = root.head;
		while (next)
		{
			if (next != inRecIndex)
			{
				ReadLocation(next, &link);
				next = link.next;
				continue;
			}
			isValid = true;
			break;
		}
	}
	return(isValid);
}

/******************************** ReadLocation ********************************/
void HikeLocations::ReadLocation(
	uint16_t		inIndex,
	void*			inLocation) const
{
	mLocations->Seek(inIndex*sizeof(SHikeLocationLink), DataStream::eSeekSet);
	mLocations->Read(sizeof(SHikeLocationLink), inLocation);
}

/******************************** WriteLocation *******************************/
void HikeLocations::WriteLocation(
	uint16_t		inIndex,
	const void*		inLocation) const
{
	mLocations->Seek(inIndex*sizeof(SHikeLocationLink), DataStream::eSeekSet);
	mLocations->Write(sizeof(SHikeLocationLink), inLocation);
}

#ifndef __MACH__

const char kCSVFilename[] = "HikeLocations.csv";

/********************************* LoadFromSD *********************************/
/*
	This routine attemps to open the CSV file named HikeLocations.csv.  The
	original file should be created using SaveToSD().  The csv contains three
	fields: ID, Name and Elevation.  The ID field should not be modified.  The
	ID field is used to associate the edited name with the location. Only the
	character set defined for the font used should be used in the name field.
*/
bool HikeLocations::LoadFromSD(void)
{
	SdFat sd;
	bool	success = sd.begin(mSDSelectPin);
	if (success)
	{
		SdFile file;
		success = file.open(kCSVFilename, O_RDONLY);
		if (success)
		{
			uint16_t			id;
			SHikeLocationLink	link;
			CSVUtils			csv(&file);
			
			char thisChar = csv.SkipLine();	// Skip the csv header line.
			
			while (thisChar != 0)
			{
				if ((thisChar = csv.ReadUint16(&id)) == ',' &&
					((thisChar = csv.ReadStr(sizeof(link.loc.name), link.loc.name)) == ',') &&
					((thisChar = csv.ReadUint16(&link.loc.elevation)) == '\n' || thisChar == 0))
				{
					if (id == 999 ||
						IsValidIndex(id))
					{
						/*
						*	If this isn't a new location THEN
						*	update or delete it.
						*/
						if (id != 999)
						{
							/*
							*	Remove and add this location to update the sort by name.
							*	The physical index is the location ID.
							*	Load the location with this id
							*/
							GoToLocation(id);
						
							/*
							*	Update only if the data has changed
							*/
							if (GetCurrent().loc.elevation != link.loc.elevation ||
								strcmp(GetCurrent().loc.name, link.loc.name))
							{
								/*
								*	RemoveCurrent places the location link as the freeHead.
								*	Add will reuse the freeHead.  This means that the
								*	physical index (i.e. id) will remain the same.
								*/
								RemoveCurrent();
							
								/*
								*	If the elevation is zero THEN
								*	this is the flag to remove the location.
								*	WARNING: The location will be reused for the
								*	next new location added.  This affects any
								*	summaries saved for the location being removed. 
								*	The summaries only record the physical index and
								*	blindly display whatever record is at the
								*	physical index, even when it's in the free list.
								*/
								if (link.loc.elevation == 0)
								{
									continue;
								}
							} else
							{
								continue;
							}
						}
						Add(link);
					}
				}
			}
		}
		file.close();
	} else
	{
		sd.initErrorHalt();
	}
	return(success);
}

/********************************** SaveToSD **********************************/
/*
	This routine overwrites or creates the CSV file named HikeLocations.csv. The
	csv contains three fields: ID, Name and Elevation.  Only the name and
	elevation fields should be edited by the user.  Only the character set
	defined for the font used should be used in the name field.
*/
bool HikeLocations::SaveToSD(void)
{
	bool	success = mCount > 0;
	if (success)
	{
		SdFat sd;
		bool	success = sd.begin(mSDSelectPin);
		if (success)
		{
			SdFile::dateTimeCallback(SDFatDateTimeCB);
			SdFile file;
			success = file.open(kCSVFilename, O_WRONLY | O_TRUNC | O_CREAT);
			if (success)
			{
				CSVUtils	csv(&file);
				file.println(F("ID,Name,Elevation"));
				uint16_t	savedCurrent = mCurrentIndex;
				GoToNthLocation(0);
				char	quotedStr[50];
				do	
				{
					file.print(mCurrentIndex);
					file.write(',');
					file.print(csv.QuoteForCSV(mCurrent.loc.name, quotedStr));
					file.write(',');
					file.println(mCurrent.loc.elevation);
				} while(Next(false));
				GoToNthLocation(savedCurrent);
				file.close();
			}
		} else
		{
			sd.initErrorHalt();
		}
	}
	return(success);
}

/****************************** SDFatDateTimeCB *******************************/
/*
*	SDFat date time callback.
*/
void HikeLocations::SDFatDateTimeCB(
	uint16_t*	outDate,
	uint16_t*	outTime)
{
	// The time macros come from CompileTime.h
	*outDate = ((uint16_t)(__TIME_YEARS__ - 1980) << 9 | (uint16_t)__TIME_MONTH__ << 5 | __TIME_DAYS__);
	*outTime = ((uint16_t)__TIME_HOURS__ << 11 | (uint16_t)__TIME_MINUTES__ << 5 | __TIME_SECONDS__ >> 1);
}
#endif
