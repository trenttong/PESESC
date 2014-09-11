/*
 * AccessMapPrefetcher.h
 *
 *  Created on: 2013-09-18
 *      Author: iatta
 */

#include "Prefetcher_Base.h"
#include "AccessMapPrefetcher_Classes.h"
#include <iostream>

using namespace std;

#define DEBUG_AMP

class AccessMapPrefetcher: public Prefetcher_Base {
public:
	AccessMapPrefetcher(MemObj* mycache, unsigned int cid, unsigned int blocksize);
	~AccessMapPrefetcher();

	virtual void IssuePrefetches(Time_t cycle);

	// MSHR for demand requests(0 bits)
	MissStatusHandlingRegister *mshr;

	// MSHR for prefetch requests(901 bits)
	PrefetchMissStatusHandlingRegister *mshrpf;

	// Memory Access Map Table(29147 bits)
	MemoryAccessMapTable *pref;

	// Adaptive Stream Prefetcher(1696 bits)
	AdaptiveStreamPrefetcher *asp;

	// Prefetch requests for generating prefetches.
	// * These registers are overwritten
	// * only when the new demand requests are reached to the prefetcher.
	int NumFwdPref; // 5 bit counter
	int NumBwdPref; // 5 bit counter
	bool FwdPrefMap[MAP_Size / 2]; // 128 bit register
	bool BwdPrefMap[MAP_Size / 2]; // 128 bit register
	AddrType AddressRegister; // 26 bit register
	// Pipeline registers total budget : 292 bit
};

