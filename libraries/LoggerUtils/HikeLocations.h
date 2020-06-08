/*
*	HikeLocations.h, Copyright Jonathan Mackey 2019
*	Class to manage hike locations.
*
*	The requirement I have is that it not be tied to any specific storage AND
*	when adding/removing locations, the existing locations will not move.  This
*	is implemented using a generic stream, which means it can be stored anywhere.
*	It's also a simple linked list, so minimal changes are needed to add and
*	remove locations.
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
#ifndef HikeLocations_h
#define HikeLocations_h

#include <inttypes.h>

class DataStream;

typedef struct
{
	uint16_t	elevation;	// Location elevation in feet.
	char		name[20];	// Location name
} SHikeLocation;

/*
*	next and prev below, when multiplied by the size of SHikeLocationLink, is a
*	physical offset from the start of the data stream.
*	The first location (index 0) is the root.  See SHikeLocationRoot within
*	HikeLocations.cpp for a description of its fields.
*
*	The location names are all uppercase.  strcmp is used for making comparisons.
*	The "MT " prefix is skipped for comparison purposes (e.g. "MT ADAMS" == "ADAMS")
*/
typedef struct
{
	uint16_t		prev;		// Index of the previous location.  0 if head.
	uint16_t		next;		// Index of the next location.  0 if tail.
	SHikeLocation	loc;
} SHikeLocationLink;

typedef struct
{
	uint16_t	tail;		// Index of the last location.
	uint16_t	head;		// Index of the first location.  0xFFFF if none.
	/*
	*	freeHead is the location of the first free location.  This will only be
	*	non-zero when a location is removed within the set of defined locations.
	*	The length of the data stream determines the capacity.  freeHead only
	*	indicates that there is fragmentation within the set of locations.
	*/
	uint16_t	freeHead;
	// The root is the same size as a SHikeLocationLink
	char		unused[20];
} SHikeLocationRoot;

class HikeLocations
{
public:
							HikeLocations(void);
	void					Initialize(
								DataStream*				inLocations,
								uint8_t					inSDSelectPin);
	uint16_t				GetCount(void) const
								{return(mCount);}
	const SHikeLocationLink& GetCurrent(void) const			// Loaded location
								{return(mCurrent);}
	uint16_t				GetCurrentIndex(void) const		// Unsorted physical record index
								{return(mCurrentIndex);}
	int16_t					GetLogicalIndex(void) const;	// Sorted logical index
	bool					IsValidIndex(
								uint16_t				inRecIndex); // Unsorted physical record index
							/*
							*	Load the next sorted location
							*/
	bool					Next(
								bool					inWrap = true);
							/*
							*	Gets the next sorted physical index without loading
							*/
	uint16_t				GetNextIndex(
								bool					inWrap = true) const;
							/*
							*	Load the previous sorted location
							*/
	bool					Previous(
								bool					inWrap = true);
							/*
							*	Gets the previous sorted physical index without loading
							*/
	uint16_t				GetPreviousIndex(
								bool					inWrap = true) const;
	void					GoToLocation(
								uint16_t				inRecIndex);	// Unsorted physical record index
	bool					GoToNthLocation(
								uint16_t				inLogIndex);	// Sorted logical index
	uint16_t				Add(
								SHikeLocationLink&		inLocation);
	bool					RemoveCurrent(void);
#ifndef __MACH__
	bool					LoadFromSD(void);
	bool					SaveToSD(void);
#endif
	static inline HikeLocations&	GetInstance(void)
								{return(sInstance);}
protected:
	static HikeLocations	sInstance;
	DataStream*			mLocations;
	SHikeLocationLink	mCurrent;
	uint16_t			mCurrentIndex;
	uint16_t			mCount;
	uint8_t				mSDSelectPin;
	
	void					ReadLocation(
								uint16_t				inIndex,	// Physical record index
								void*					inLocation) const;
	void					WriteLocation(
								uint16_t				inIndex,	// Physical record index
								const void*				inLocation) const;
	bool					GoToRelativeLocation(
								int16_t					inRelLogIndex);	// Relative sorted logical index
	static const char*		SkipMTPrefix(
								const char*				inName);
#ifndef __MACH__
							/*
							*	Used by SaveToSD to set the file creation date
							*	and time to something reasonable.  The board
							*	doesn't have an RTC so the time set the static
							*	compile date/time.
							*/
	static void				SDFatDateTimeCB(
								uint16_t*				outDate,
								uint16_t*				outTime);
#endif
};

#endif // HikeLocations_h
