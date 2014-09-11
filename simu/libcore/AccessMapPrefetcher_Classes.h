/*
 * AMP_Classes.h
 *
 *  Created on: 2013-09-18
 *      Author: iatta
 */

#ifndef AMP_CLASSES_H_
#define AMP_CLASSES_H_

//IATTA
//#define DEBUG_ASP
//#define DEBUG_AMPM
// -*- mode: C++ -*-

// This header file is devided 5 files originally.
// So, the author wants that the reader divide this file appropriately.
// The concatination point include Emacs Header like " -*- mode: C++ -*- "

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
//#include "Snippets.h"
#include "RAWDInst.h"
#include "MemObj.h"
class MemObj;

using namespace std;

#ifndef __BASE_H__
#define __BASE_H__

#define ASSERT(X)
//#define ASSERT(X) assert(X)

static const int BLK_WIDTH = 6;
static const int IDX_WIDTH = 8;
static const int TAG_WIDTH = 18;

static const int BLK_BITS = BLK_WIDTH;
static const int IDX_BITS = IDX_WIDTH + BLK_BITS;
static const int TAG_BITS = TAG_WIDTH + IDX_BITS;

static const AddrType BLK_SIZE = ((AddrType) 1) << BLK_BITS;
static const AddrType IDX_SIZE = ((AddrType) 1) << IDX_BITS;
static const AddrType TAG_SIZE = ((AddrType) 1) << TAG_BITS;

static const AddrType BLK_MASK = (((AddrType) 1) << BLK_WIDTH) - 1;
static const AddrType IDX_MASK = (((AddrType) 1) << IDX_WIDTH) - 1;
static const AddrType TAG_MASK = (((AddrType) 1) << TAG_WIDTH) - 1;

static const int PREF_MSHR_SIZE = 32;

#endif

///////////////////////////////////////////////////////////////
// Adaptive Stream Prefetcher
//
// One of novel stream prefetcher
// This prefetcher is used as a L1 prefetcher in our work.
//
// About detail of this prefetcher refer to following paper.
// "Memory Prefetching Using Adaptive Stream Detection (Micro2006)"
// Ibrahim Hur, Calvin Lin
///////////////////////////////////////////////////////////////

#ifndef __ADAPTIVE_H__
#define __ADAPTIVE_H__

static const int LIFETIME = 1023;
static const int LHT_SIZE = 16;
static const int SLOTSIZE = 16;
static const Time_t EPOCH_MASK = ((Time_t) 1 << 16) - 1;

class AdaptiveStreamFilterSlot {
private:
	// Budget Size
	// Address   : 26 bit
	// Lifetime  : 10 bit
	// Length    :  4 bit
	// Direction :  1 bit
	// Valid     :  1 bit

public:
	AddrType Address;
	int Lifetime;
	int Length;
	bool Direction; // T->Positive, F->Negative
	bool Valid;

	AdaptiveStreamFilterSlot() {
		Valid = false;
	}

	void Init() {
		Valid = false;
	}

	void Assign(AddrType addr) {
		ASSERT(!(addr & BLK_MASK));

		Address = addr;
		Lifetime = LIFETIME;
		Length = 0;
		Valid = true;
		Direction = true;
	}

	//
	// Hit / Miss judge
	//
	// When the stream was hit,
	// the address was already accessed position or
	// the address was neighbor last accessed address.
	//
	bool Hit(AddrType addr) {
		ASSERT(!(addr & BLK_MASK));

		if (!Valid)
			return false;

		if (Length == 0) {
			return (addr == Address) || ((addr + BLK_SIZE) == Address)
					|| ((addr - BLK_SIZE) == Address);
		} else {
			if (Direction) {
				AddrType BaseAddress = Address - Length * BLK_SIZE;
				return (BaseAddress <= addr && (Address + BLK_SIZE) >= addr);
			} else {
				AddrType BaseAddress = Address + Length * BLK_SIZE;
				return (BaseAddress >= addr && (Address - BLK_SIZE) <= addr);
			}
		}
	}

	//
	// Update Slot
	//
	// Return Value
	//
	// 0  : Missed
	// 1-7: 1st-7th stream address was detected
	// -1 : Already accessed position was detected.
	//
	int Update(AddrType addr) {
		ASSERT(!(addr & BLK_MASK));

		if (!Hit(addr))
			return 0;

		if (Length == 0) {
			if ((Address - BLK_SIZE) == addr) {
				Direction = false;
			}
		}

		AddrType StreamAddr = Address + (Direction ? 1 : -1) * BLK_SIZE;
		if (StreamAddr == addr) {
			Lifetime = LIFETIME;
			Address = addr;
			Length = Length + 1;
			return Length;
		}

		return -1;
	}

	// Housekeeping Slot Info.
	void Housekeeping() {
		if (Valid) {
			// Too long stream is released since
			// prefetcher cannot handling such stream.
			if (Length >= LHT_SIZE) {
				Valid = false;
				return;
			}

			// When lifetime became 0,
			// The entry is released.
			if (Lifetime == 0) {
				Valid = false;
				return;
			}

			Lifetime--;
			ASSERT(Lifetime >= 0);
		}
	}
};

class AdaptiveStreamPrefetcher {
	// IATTA
	MemObj* myCache;
	unsigned int coreId;
	unsigned int log2BlockSize;

//private:
	// Budget Count
	// Total Count = 672 + 1024 = 1696 bit

	// 16 bit * 2 direction * 16 entry * 2 series = 1024 bit
	int LHTnext[LHT_SIZE][2]; // Each counter holds 16 bit data
	int LHTcurr[LHT_SIZE][2]; // Each counter holds 16 bit data

	// 42 bit * 16 = 672 bit
	AdaptiveStreamFilterSlot slot[SLOTSIZE];

public:
	AdaptiveStreamPrefetcher(MemObj* cache, unsigned int cid,
			unsigned int blockSize) {
		myCache = cache; //IATTA
		coreId = cid;
		log2BlockSize = log2(blockSize);

		for (int i = 0; i < SLOTSIZE; i++) {
			slot[i].Init();
		}

		for (int i = 0; i < LHT_SIZE; i++) {
			LHTcurr[i][0] = 0;
			LHTcurr[i][1] = 0;
			LHTnext[i][0] = 0;
			LHTnext[i][1] = 0;
		}
	}

	void IssuePrefetchSub(Time_t cycle, AddrType addr) {
		//IssueL1Prefetch(cycle, addr);
#ifdef DEBUG_ASP
		cout << "ASP-Prefetch: @ " << std::dec << cycle << " cid-" << coreId << " L-"
		<< myCache->myLevel << " 0x" << std::hex << addr
		<< endl;
#endif // DEBUG_ASP
		myCache->prefetch(addr); //IATTA
	}

	void IssuePrefetch(Time_t cycle, AddrType addr, bool hit) {
		addr &= ~BLK_MASK;

		// Searching entry.
		for (int i = 0; i < SLOTSIZE; i++) {
			int idx = slot[i].Update(addr);
			int dir = slot[i].Direction ? 0 : 1;
			if (idx != 0) {
				if (idx < 0) {
					return;
				} else if (idx > 0) {

					if (idx <= LHT_SIZE) {
						LHTnext[idx][dir] += 1;

						for (int j = idx; j < LHT_SIZE; j++) {
							if ((LHTcurr[idx - 1][dir] >= 2 * LHTcurr[j][dir])
									&& (LHTcurr[idx - 0][dir]
											< 2 * LHTcurr[j][dir])) {
								IssuePrefetchSub(cycle,
										addr
												+ (j - idx) * (dir ? -1 : 1)
														* BLK_SIZE);
							}
						}
					}
				}
				return;
			}
		}

		// When the searching was failed,
		// prefetcher try to assign new slot for this request.
		if (!hit) {
			for (int i = 0; i < SLOTSIZE; i++) {

				// The slot was assigned only when the target slot is empty.
				if (!slot[i].Valid) {

					// Initializing slot...
					slot[i].Assign(addr);

					LHTnext[0][0] += 1;
					LHTnext[0][1] += 1;

					// First Prefetch Issue
					for (int j = 1; j < LHT_SIZE; j++) {
						if (LHTcurr[0][0] < 2 * LHTcurr[j][0]) {
							IssuePrefetchSub(cycle, addr + j * BLK_SIZE);
						}
						if (LHTcurr[0][1] < 2 * LHTcurr[j][1]) {
							IssuePrefetchSub(cycle, addr - j * BLK_SIZE);
						}
					}
					return;
				}
			}
		}
	}

	void Housekeeping(Time_t cycle) {
		for (int i = 0; i < SLOTSIZE; i++) {
			slot[i].Housekeeping();
		}

		// When the global clock counter does not exist,
		// The prefetcher employ similar components.
		// Howerver it does not require huge budget size.
		if ((cycle % EPOCH_MASK) == 0) {
			for (int i = 0; i < LHT_SIZE; i++) {
				LHTcurr[i][0] = LHTnext[i][0];
				LHTcurr[i][1] = LHTnext[i][1];
				LHTnext[i][0] = 0;
				LHTnext[i][1] = 0;
			}
		}
	}
};

#endif

// -*- mode: C++ -*-

//
// Default MSHR (16 entries)
// We assume that this MSHR is implemente as a part of Toolkit.
// So, we do not count this module as a 32K bit prefetch storage.
//

#ifndef __MSHR_H__
#define __MSHR_H__

static const int MSHR_SIZE = 16;

class MSHR_Entry {
private:
public:
	// Budget Count

	// Valid bit  :  1 bit
	// Address bit: 26 bit
	// Total Count: 27 bit
	bool Valid;AddrType Addr;

public:
	// Budget() returns an amount of storage for
	// implementing prefetch algorithm.
	int Budget() {
		return 0;
	}

	// Initialize MSHR entry
	void Init() {
		Valid = false;
		Addr = (AddrType) 0;
	}

	// Returns hit or miss with MSHR entry.
	bool Hit(AddrType addr) {
		// NULL pointer handling
		if (addr == 0)
			return true;

		// Check Address
		addr ^= Addr;
		addr &= ~BLK_MASK;
		return Valid && (addr == (AddrType) 0);
	}

	// Assign Entry
	void Entry(AddrType addr) {
		Valid = true;
		Addr = addr;
	}

	// Housekeeping for MSHR entry
	void Housekeeping(Time_t cycle, MemObj* uppercache) {
		// When V bit is not assigned, this entry is free.
		ASSERT(Valid);

		// When GetPrefetchBit() returns a not negative value,
		// the fill of the entry will be finished.
		// In realistic processor, this judge should not based on prefetch bit.
		// (This might be done with the setting of the VALID bit of a tag array)
		// So, it is not so high complexity.
		//if (GetPrefetchBit(1, Addr) >= 0) { // IATTA
		if (uppercache->getExtraBit(Addr) >= 0)
		{
			// Release MSHR entry
			Valid = false;
		}
	}
};

class MissStatusHandlingRegister {
private:
	// MSHR Entry Size is 16

	int ptr; // 4  bits (Added control register)
	MSHR_Entry mshr[MSHR_SIZE];
	// 27 bits x 16 (But only 16 bits are counted as a budget)

	// Total Budget Count: 436
	// Total Budget Count for Contest Storage: 20 bits

public:
	MissStatusHandlingRegister() {
		ptr = 0;
		for (int i = 0; i < MSHR_SIZE; i++) {
			mshr[i].Init();
		}
	}

	bool Full() {
		for (int i = 0; i < MSHR_SIZE; i++) {
			if (!mshr[i].Valid)
				return false;
		}
		return true;
	}

	bool Search(AddrType addr) {
		// Search Previous Memory Access
		for (int i = 0; i < MSHR_SIZE; i++) {
			// When the appropriate access is found, the access is merged.
			if (mshr[i].Hit(addr))
				return true;
		}
		return false;
	}

	void Issue(Time_t cycle, AddrType addr, MemObj* uppercache) {
		// Check In-flight memory access
		if (Search(addr))
			return;

		// When the appropriate access cannot be found,
		// the access will issue for main memory.
		mshr[ptr].Entry(addr);

		mshr[ptr].Housekeeping(cycle, uppercache);
		ptr = (ptr + 1) % MSHR_SIZE;
	}

	void Housekeeping(Time_t cycle, MemObj* uppercache) {
		// housekeeping for MSHR...
		for (int i = 0; i < MSHR_SIZE; i++) {

			if (mshr[i].Valid) {
				// This Housekeeping() can invoke
				// only when the mshr entry is already assigned.
				mshr[i].Housekeeping(cycle, uppercache);
			}
		}
	}
};

#endif
// -*- mode: C++ -*-

//
// Prefetch MSHR
// This mshr is provided for handling status of prefetch requests.
// Entries of prefetch MSHR are counted for 32K bit budget count.
//

#ifndef __PREF_MSHR_H__
#define __PREF_MSHR_H__

class PrefetchMissStatusHandlingRegisterEntry {
private:
public:
	// Budget Count

	// Issued bit :  1 bit
	// Valid bit  :  1 bit
	// Address bit: 26 bit
	// Total Count: 28 bit
	bool Valid;
	bool Issue;AddrType Addr;

public:
	// Budget() returns an amount of storage for
	// implementing prefetch algorithm.
	int Budget() {
		return 0;
	}

	// Initialize MSHR entry
	void Init() {
		Valid = false;
		Issue = false;
		Addr = (AddrType) 0;
	}

	// Returns hit or miss with MSHR entry.
	bool Hit(AddrType addr) {
		// NULL pointer handling
		if (addr == 0)
			return true;

		// Check Address
		addr ^= Addr;
		addr &= ~BLK_MASK;
		return Valid && (addr == (AddrType) 0);
	}

	// Assign Entry
	void Entry(AddrType addr) {
		Valid = true;
		Issue = false;
		Addr = addr;
	}

	// Assign Entry
	void IssuePrefetch(Time_t cycle, MemObj* mycache, MemObj* uppercache,
			unsigned int coreid) {
		ASSERT(Valid && !Issue);
		Issue = true;

		if (uppercache->getExtraBit(Addr) < 0) { //GetPrefetchBit(1, Addr) < 0) { // IATTA
#ifdef DEBUG_AMPM
			cout << "AMPM-Prefetch: @ " << std::dec << cycle << " cid-"
					<< coreid << " L-" << mycache->myLevel << " 0x" << std::hex
					<< Addr << endl;
#endif // DEBUG_AMPM
			mycache->prefetch(Addr);
		}
	}

	// Housekeeping for MSHR entry
	void Housekeeping(Time_t cycle, MemObj* uppercache) {
		// When V bit is not assigned, this entry is free.
		ASSERT(Valid);

		// When GetPrefetchBit() returns a not negative value,
		// the fill of the entry will be finished.
		// In realistic processor, this judge should not based on prefetch bit.
		// (This might be done with the setting of the VALID bit of a tag array)
		// So, it is not so high complexity.
		//if (GetPrefetchBit(1, Addr) >= 0) { // IATTA: see note at end of file
		if (uppercache->getExtraBit(Addr) >= 0) {
			// Release MSHR entry
			Valid = false;
		}
	}
};

class PrefetchMissStatusHandlingRegister {
private:
	// Total Budget Size : 901 bits

	// MSHR Entry Size is 32
	int ptr; // 5 bits
	PrefetchMissStatusHandlingRegisterEntry mshr[PREF_MSHR_SIZE]; // 28 bits x 32

public:
	PrefetchMissStatusHandlingRegister() {
		ptr = 0;
		for (int i = 0; i < PREF_MSHR_SIZE; i++) {
			mshr[i].Init();
		}
	}

	bool Full() {
		return mshr[ptr].Valid;
	}

	bool Search(AddrType addr) {
		// Search Previous Memory Access
		for (int i = 0; i < PREF_MSHR_SIZE; i++) {
			// When the appropriate access is found, the access is merged.
			if (mshr[i].Hit(addr))
				return true;
		}
		return false;
	}

	void Issue(Time_t cycle, AddrType addr, MemObj* uppercache) {
		// Check In-flight memory access
		if (Search(addr))
			return;

		// When the appropriate access cannot be found,
		// the access will issue for main memory.
		mshr[ptr].Entry(addr);

		mshr[ptr].Housekeeping(cycle, uppercache);
		ptr = (ptr + 1) % PREF_MSHR_SIZE;
	}

	void Housekeeping(Time_t cycle, MemObj* uppercache) {
		// housekeeping for MSHR...
		for (int i = 0; i < PREF_MSHR_SIZE; i++) {
			if (mshr[i].Valid) {
				// This Housekeeping() can invoke
				// only when the mshr entry is already assigned.
				mshr[i].Housekeeping(cycle, uppercache);
			}
		}
	}

	void PrefetchHousekeeping(MissStatusHandlingRegister *MSHR, Time_t cycle,
			MemObj* mycache, MemObj* uppercache, unsigned int coreid) {
		for (int i = 0; i < PREF_MSHR_SIZE; i++) {
			int p = (ptr + i + 1) % PREF_MSHR_SIZE;
			if ((mshr[p].Valid) && (!mshr[p].Issue)) {
				// When the entry does not issued,
				// prefetch MSHR issues request to Memory Unit.
				if (MSHR->Search(mshr[p].Addr)) {
					mshr[p].Init();
				} else {
					mshr[p].IssuePrefetch(cycle, mycache, uppercache, coreid);
				}
				return;
			}
		}
	}
};

#endif
// -*- mode:c++ -*-

// -*- mode:c++ -*-

#ifndef __MEMORY_ACCESS_MAP_H__
#define __MEMORY_ACCESS_MAP_H__

////////////////////////////////////////////////////
// Definition of Constants.
////////////////////////////////////////////////////

// Access Map Size
static const int MAP_Size = 1 << IDX_WIDTH;

// Memory Access Map Table Size
#define REALISTIC
// Our submission is realistic configuration

#ifdef REALISTIC
// Realistic Configuration
// 13 way set associative table
static const int NUM_SET = 16;
static const int NUM_WAY = 13;

#else
// Idealistic Configuration
// Full associative table
static const int NUM_SET = 1;
static const int NUM_WAY = 52;

#endif

// TABLE SIZE is 52 entries because of the storage limitation.
// If  we have more budget, we will emply 64 entries.
// And we have less budget, we will emply 32 entries.
static const int TABLE_SIZE = NUM_SET * NUM_WAY;

// Threshold
// They are only the constansts.
static const int AP_THRESHOLD = 8;
static const int SE_THRESHOLD = 128;
static const int CA_THRESHOLD = 256;

////////////////////////////////////////////////////
// Definition of Constants.
////////////////////////////////////////////////////

// Memory access map status is implemented as 2bit state machine
enum MemoryAccessMapState {
	INIT, // INIT     Status (00)
	ACCESS, // ACCESS   Status (01)
	PREFETCH, // PREFETCH Status (10)
	SUCCESS, // SUCCESS  Status (11)
};

class MemoryAccessMap {
public:
	int LRU; // 6 bits LRU information

	Time_t LastAccess; // Timer(16 bits)
	Time_t NumAccess; // Access counter(4 bits)
	AddrType Tag; // 18 bit address tag(18 bits)

	// 2 bit state machine x 256 = 512 bit Map
	enum MemoryAccessMapState AccessMap[MAP_Size];

	// Total Budget Count : 556 bits

	// Generate Index and Tag
	AddrType GenTag(AddrType addr) {
		return (addr >> IDX_BITS) & TAG_MASK;
	}
	AddrType GenIdx(AddrType addr) {
		return (addr >> BLK_BITS) & IDX_MASK;
	}

public:
	/////////////////////////////////////////////////////
	// Constructer & Copy Constructer
	/////////////////////////////////////////////////////
	MemoryAccessMap(AddrType t = 0) :
			Tag(t) {
	}
	MemoryAccessMap(const MemoryAccessMap& ent) :
			Tag(ent.Tag) {
	}
	~MemoryAccessMap() {
	}

	/////////////////////////////////////////////////////
	// Read prefetch infomation
	/////////////////////////////////////////////////////
	bool Hit(AddrType addr) {
		return Tag == GenTag(addr);
	}

	bool AlreadyAccess(AddrType addr) {
		return (Hit(addr) && (AccessMap[GenIdx(addr)] != INIT));
	}

	bool PrefetchHit(AddrType addr) {
		return (Hit(addr) && (AccessMap[GenIdx(addr)] == INIT));
	}

	void Read(enum MemoryAccessMapState *ReadMap) {
		for (int i = 0; i < MAP_Size; i++) {
			ReadMap[i] = AccessMap[i];
		}
	}

	/////////////////////////////////////////////////////
	// Updating methods...
	/////////////////////////////////////////////////////

	// Assign new Czone to this map.
	void Entry(AddrType addr) {
		// Assign & Initializing Entry
		Tag = GenTag(addr);
		for (int i = 0; i < MAP_Size; i++) {
			AccessMap[i] = INIT;
		}
		LastAccess = 0;
		NumAccess = 0;
	}

	// Issuing prefetch request
	bool IssuePrefetchSub(Time_t cycle, AddrType addr) {
		if (!Hit(addr))
			return false;

		// L2 Access Control
		ASSERT(AccessMap[GenIdx(addr)] == INIT);
		AccessMap[GenIdx(addr)] = PREFETCH;
		return true;
	}

	// Updating state machine
	void Update(Time_t cycle, AddrType addr) {
		ASSERT(Hit(addr));

		switch (AccessMap[GenIdx(addr)]) {
		case INIT:
			AccessMap[GenIdx(addr)] = ACCESS;
			break;
		case ACCESS:
			return;
		case PREFETCH:
			AccessMap[GenIdx(addr)] = SUCCESS;
			break;
		case SUCCESS:
			return;
		default:
			ASSERT(false);
			break;
		}

		if (NumAccess >= 15) {
			NumAccess = 8;
			LastAccess = LastAccess >> 1;
		} else {
			NumAccess++;
		}

		return;
	}

	/////////////////////////////////////////////////////
	// Profiling methods
	/////////////////////////////////////////////////////
	int NumInit() {
		int Succ = 0;
		for (int i = 0; i < MAP_Size; i++) {
			Succ += (AccessMap[i] == INIT ? 1 : 0);
		}
		return Succ;
	}
	int NumSuccPref() {
		int Succ = 0;
		for (int i = 0; i < MAP_Size; i++) {
			Succ += (AccessMap[i] == SUCCESS ? 1 : 0);
		}
		return Succ;
	}
	int NumAccesses() {
		int Succ = 0;
		for (int i = 0; i < MAP_Size; i++) {
			Succ += (AccessMap[i] == ACCESS ? 1 : 0);
		}
		return Succ;
	}
	int NumFailPref() {
		int Succ = 0;
		for (int i = 0; i < MAP_Size; i++) {
			Succ += (AccessMap[i] == PREFETCH ? 1 : 0);
		}
		return Succ;
	}

	// Returns stream length of the prefetch
	// * The stream length is decided from access history
	int MaxAccess() {
		int req = (NumAccess * 256) / (1 + LastAccess);
		int max = 7;
		return max > req ? req : max;
	}

	/////////////////////////////////////////////////////
	// Housekeeping
	/////////////////////////////////////////////////////

	// Updates some counters.
	void Housekeeping(Time_t cycle) {
		LastAccess++;
		if (LastAccess > 65536) {
			LastAccess = 65536;
		}
	}
};

class MemoryAccessMapTable {
public:
	// These flags uses 3 bits
	bool Aggressive;
	bool SaveEntry;
	bool ConflictAvoid;

	// These counter uses 128 bits
	int NeedEntry;
	int Conflict;
	int PrefFail;
	int PrefSucc;

	// 54 entries table (556 x 52)
	MemoryAccessMap Table[NUM_SET][NUM_WAY];

	// Total Budget Count = 29147 bits

	/////////////////////////////////////////////
	// Constructor & Copy Constructor
	/////////////////////////////////////////////

	MemoryAccessMapTable() {
		NeedEntry = 64;
		Conflict = 16;
		SaveEntry = false;
		ConflictAvoid = false;
		Aggressive = false;

		for (int Idx = 0; Idx < NUM_SET; Idx++) {
			for (int Way = 0; Way < NUM_WAY; Way++) {
				Table[Idx][Way].LRU = Way;
				Table[Idx][Way].Entry(0);
			}
		}
	}

	~MemoryAccessMapTable() {
	}

	/////////////////////////////////////////////
	// Basic Functions.
	/////////////////////////////////////////////

	// Status Handling Function
	bool IsAccessed(MemoryAccessMapState state) {
		switch (state) {
		case INIT:
			return false;
		case ACCESS:
			return true;
		case PREFETCH:
			return Aggressive;
		case SUCCESS:
			return true;
		default:
			ASSERT(false);
			return false;
		}
	}

	bool IsCandidate(MemoryAccessMapState state, int index, int passivemode) {
		return ((state == INIT)
				&& ((index >= MAP_Size && index < (2 * MAP_Size))
						|| !passivemode));
	}

	/////////////////////////////////////////////
	// Read & Update memory access map
	/////////////////////////////////////////////

	// Generate Index and Tag
	AddrType GenTag(AddrType addr) {
		return (addr >> IDX_BITS) & TAG_MASK;
	}
	AddrType GenIdx(AddrType addr) {
		return (addr >> BLK_BITS) & IDX_MASK;
	}

	// Read Memory Access Map from Table
	MemoryAccessMap* AccessEntry(Time_t cycle, AddrType addr) {
		int Oldest = 0;

		int Idx = GenTag(addr) % NUM_SET;
		ASSERT(Idx >= 0 && Idx < NUM_SET);
		for (int Way = 0; Way < NUM_WAY; Way++) {
			if (Table[Idx][Way].Hit(addr)) {
				// Find Entry
				for (int i = 0; i < NUM_WAY; i++) {
					if (Table[Idx][i].LRU < Table[Idx][Way].LRU) {
						Table[Idx][i].LRU++;
						ASSERT(Table[Idx][i].LRU < NUM_WAY);
					}
				}
				Table[Idx][Way].LRU = 0;
				return (&(Table[Idx][Way]));
			}

			if (Table[Idx][Way].LRU > Table[Idx][Oldest].LRU) {
				Oldest = Way;
			}
		}

		ASSERT(Table[Idx][Oldest].LRU == (NUM_WAY-1));

		// Find Entry
		for (int i = 0; i < NUM_WAY; i++) {
			if (Table[Idx][i].LRU < Table[Idx][Oldest].LRU) {
				Table[Idx][i].LRU++;
				ASSERT(Table[Idx][i].LRU < NUM_WAY);
			}
		}
		PrefFail += Table[Idx][Oldest].NumFailPref();
		PrefSucc += Table[Idx][Oldest].NumSuccPref();
		Table[Idx][Oldest].Entry(addr);

		Table[Idx][Oldest].LRU = 0;
		return (&(Table[Idx][Oldest]));
	}

	// Update Memory Access Map when the entry is on the table.
	void UpdateEntry(Time_t cycle, AddrType addr) {
		// Issueing Prefetch
		int Idx = GenTag(addr) % NUM_SET;
		ASSERT(Idx >= 0 && Idx < NUM_SET);
		for (int Way = 0; Way < NUM_WAY; Way++) {
			if (Table[Idx][Way].Hit(addr)) {
				Table[Idx][Way].IssuePrefetchSub(cycle, addr);
				return;
			}
		}
	}

	///////////////////////////////////////////////////
	// Read access map & generate prefetch candidates.
	///////////////////////////////////////////////////

	// Read Prefetch Candidates from Memory Access Map
	// This function does not have any memories.
	void IssuePrefetch(Time_t cycle, AddrType addr, bool MSHR_hit,
			bool *FwdPrefMap, bool *BwdPrefMap, int *NumFwdPref,
			int *NumBwdPref, MemObj* myCache) {
		//////////////////////////////////////////////////////////////////
		// Definition of Temporary Values
		//////////////////////////////////////////////////////////////////

		// These variables are not counted as a storage
		// Since these are used as a wire (not register)
		MemoryAccessMap *ent_l, *ent_m, *ent_h;
		static enum MemoryAccessMapState ReadMap[MAP_Size * 3];
		static enum MemoryAccessMapState FwdHistMap[MAP_Size];
		static enum MemoryAccessMapState BwdHistMap[MAP_Size];
		static bool FwdCandMap[MAP_Size];
		static bool BwdCandMap[MAP_Size];
		static bool FwdPrefMapTmp[MAP_Size / 2];
		static bool BwdPrefMapTmp[MAP_Size / 2];
		static int NumFwdPrefTmp;
		static int NumBwdPrefTmp;

		bool PassiveMode = SaveEntry || ConflictAvoid; // Mode

		//////////////////////////////////////////////////////////////////
		// Read & LRU update for access table
		//////////////////////////////////////////////////////////////////
		if (PassiveMode) {
			ent_l = NULL;
			ent_h = NULL;
		} else {
			ent_l = AccessEntry(cycle, addr - IDX_SIZE);
			ent_h = AccessEntry(cycle, addr + IDX_SIZE);
		}
		ent_m = AccessEntry(cycle, addr);

		//////////////////////////////////////////////////////////////////
		// Following Cases are not prefered.
		//
		// 1. AccessMap==0 && PrefetchBit==1 (Prefetched)
		//      -> There are not enough Entry Size
		// 2. L2 Miss && Already Accessed && MSHR Miss (Finished filling)
		//      -> L2 Cache conflict miss is detected.
		//
		// Our prefetcher detect these situation and profiling these case.
		// When the frequenty exceed some threshold,
		// the profiler changes the prefetch mode.
		//////////////////////////////////////////////////////////////////
		// IATTA: for the following three lines, see note at end of file.
		assert(myCache);
		// An upperCache has to be defined. AMPM cannot be implemented for L1 caches.
		bool PF_succ = myCache->getExtraBit(addr) == 1; //GetPrefetchBit(1, addr) == 1; // Prefetch successed
		bool L2_miss = myCache->getExtraBit(addr) < 0; //GetPrefetchBit(1, addr)  < 0; // L2 miss detection
		myCache->resetExtraBit(addr); // UnSetPrefetchBit(1, addr); // Clear Prefetch Bit

		bool NE_INC = PF_succ && ent_m->PrefetchHit(addr);
		bool CF_INC = L2_miss && ent_m->AlreadyAccess(addr) && !MSHR_hit;
		NeedEntry += NE_INC ? 1 : 0;
		Conflict += CF_INC ? 1 : 0;

		//////////////////////////////////////////////////////////////////
		// Prefetching Part...
		//////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////
		// Read Entry (Read & Concatenate Accessed Maps)
		//////////////////////////////////////////////////////////////////
		for (int i = 0; i < MAP_Size; i++) {
			ReadMap[i] = INIT;
		}

		if (ent_l)
			ent_l->Read(&ReadMap[0 * MAP_Size]);
		if (ent_m)
			ent_m->Read(&ReadMap[1 * MAP_Size]);
		if (ent_h)
			ent_h->Read(&ReadMap[2 * MAP_Size]);

		//////////////////////////////////////////////////////////////////
		// Generating Forwarding Prefetch
		//////////////////////////////////////////////////////////////////
		if (ConflictAvoid) {
			NumFwdPrefTmp = 2;
		} else {
			NumFwdPrefTmp = 2 + (ent_m ? ent_m->MaxAccess() : 0);
			// +(PassiveMode && ent_l ? 0 : ent_l->MaxAccess());
		}

		// This block should be implemented in simple shifter
		for (int i = 0; i < MAP_Size; i++) {
			int index_p = MAP_Size + GenIdx(addr) + i;
			int index_n = MAP_Size + GenIdx(addr) - i;
			FwdHistMap[i] = ReadMap[index_n];
			FwdCandMap[i] = IsCandidate(ReadMap[index_p], index_p, PassiveMode);
		}

		// This for statement can be done in parallel
		for (int i = 0; i < MAP_Size / 2; i++) {
			FwdPrefMapTmp[i] = FwdCandMap[i] && IsAccessed(FwdHistMap[i])
					&& (IsAccessed(FwdHistMap[2 * i + 0])
							|| IsAccessed(FwdHistMap[2 * i + 1]));
		}

		//////////////////////////////////////////////////////////////////
		// Generating Backward Prefetch
		//////////////////////////////////////////////////////////////////

		if (ConflictAvoid) {
			NumBwdPrefTmp = 2;
		} else {
			NumBwdPrefTmp = 2 + (ent_m ? ent_m->MaxAccess() : 0);
			// +(PassiveMode && ent_h ? 0 : ent_h->MaxAccess());
		}

		for (int i = 0; i < MAP_Size; i++) {
			int index_p = MAP_Size + GenIdx(addr) + i;
			int index_n = MAP_Size + GenIdx(addr) - i;
			BwdHistMap[i] = ReadMap[index_p];
			BwdCandMap[i] = IsCandidate(ReadMap[index_n], index_n, PassiveMode);
		}

		// This for statement can be done in parallel
		for (int i = 0; i < MAP_Size / 2; i++) {
			BwdPrefMapTmp[i] = BwdCandMap[i] && IsAccessed(BwdHistMap[i])
					&& (IsAccessed(BwdHistMap[2 * i + 0])
							|| IsAccessed(BwdHistMap[2 * i + 1]));
		}

		//////////////////////////////////////////////////////////////////
		// Update Entry
		//////////////////////////////////////////////////////////////////

		ent_m->Update(cycle, addr);

		//////////////////////////////////////////////////////////////////
		// Setting Output Values (Copy to Arguments)
		//////////////////////////////////////////////////////////////////

		*NumFwdPref = NumFwdPrefTmp;
		*NumBwdPref = NumBwdPrefTmp;
		for (int i = 0; i < MAP_Size / 2; i++) {
			BwdPrefMap[i] = BwdPrefMapTmp[i];
			FwdPrefMap[i] = FwdPrefMapTmp[i];
		}
	}

	///////////////////////////////////////////////////
	// Housekeeping Function
	///////////////////////////////////////////////////

	void Housekeeping(Time_t cycle) {
		// Housekeeping for each entry.
		for (int Idx = 0; Idx < NUM_SET; Idx++) {
			for (int Way = 0; Way < NUM_WAY; Way++) {
				Table[Idx][Way].Housekeeping(cycle);
			}
		}

		// Aggressive Prefetching Mode
		// This mode is enabled when the
		// (prefetch success count > 8 * prefetch fail count) is detected.
		// If this mode is enabled,
		// the prefetcher assume PREFETCH state as the ACCESS state.
		if ((cycle & (((Time_t) 1 << 18) - 1)) == 0) {
			if (PrefSucc > AP_THRESHOLD * PrefFail) {
				// printf("Aggressive Prefetch Mode\n");
				Aggressive = true;
			} else if (PrefSucc < (AP_THRESHOLD / 2) * PrefFail) {
				// printf("Normal Prefetch Mode\n");
				Aggressive = false;
			}
			PrefSucc >>= 1;
			PrefFail >>= 1;
		}

		// Profiling Execution Status
		// This mode is enabled when the replacement of an access table entry
		// occurs frequently. When this mode is enabled,
		// the prefetcher stops reading neighbor entries.
		// This feature reduces the unprefered access entry replacements.
		if ((cycle & (((Time_t) 1 << 18) - 1)) == 0) {
			if (NeedEntry > SE_THRESHOLD) {
				// printf("Save Entry Mode\n");
				SaveEntry = true;
			} else if (NeedEntry > (SE_THRESHOLD / 4)) {
				// printf("Normal Entry Mode\n");
				SaveEntry = false;
			}
			NeedEntry >>= 1;
		}

		// Profiling Execution Status
		// This mode is enabled when the L2 conflict misses are detected frequentry.
		// When the conflict avoidance mode is enabled,
		// the prefetcher reduces the number of prefetch requests.
		if ((cycle & (((Time_t) 1 << 18) - 1)) == 0) {
			if (Conflict > CA_THRESHOLD) {
				// printf("Conflict Avoidance Mode\n");
				ConflictAvoid = true;
			} else if (Conflict > (CA_THRESHOLD / 4)) {
				// printf("Normal Conflict Mode\n");
				ConflictAvoid = false;
			}
			Conflict >>= 1;
		}
	}
};

#endif

#endif /* AMP_CLASSES_H_ */
