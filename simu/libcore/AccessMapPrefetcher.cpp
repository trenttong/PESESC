/*
 * AccessMapPrefetcher.cpp
 *
 *  Created on: 2013-09-18
 *      Author: iatta
 */

#include "AccessMapPrefetcher.h"

void AccessMapPrefetcher::IssuePrefetches(Time_t cycle) {

	mshrpf->Housekeeping(cycle, (MemObj*) myCache);
	mshr->Housekeeping(cycle, (MemObj*) myCache);
	pref->Housekeeping(cycle);

	/////////////////////////////////////////////////////
	/////////////////////////////////////////////////////
	// L2 Prefetching (AMPM Prefetching)
	//
	// Following part is pipelined Structure
	// Stage sequence is written in reverse order.
	// (This coding style might be similar to SimpleScalar)
	/////////////////////////////////////////////////////
	/////////////////////////////////////////////////////

	/////////////////////////////////////////////////////
	// Stage 3. Issue Prefetch from MSHR
	/////////////////////////////////////////////////////
	mshrpf->PrefetchHousekeeping(mshr, cycle, (MemObj*)myCache, (MemObj*)myCache, coreID);

	/////////////////////////////////////////////////////
	// Stage 2. Issue Prefetch Request to MSHR
	/////////////////////////////////////////////////////

	// Maximum issue rate to prefetch MSHR...
	static const int MAX_ISSUE = 1;

	// This loop is implemented as a priority encoder
	for (int i = 1, count = 0;
			(NumFwdPref && (count < MAX_ISSUE) && (!mshrpf->Full())
					&& (i < (MAP_Size / 2))); i++) {
		if (FwdPrefMap[i]) {
			AddrType PrefAddress = AddressRegister + (i * BLK_SIZE);
			FwdPrefMap[i] = false;

			// Update Memory Access Map
			pref->UpdateEntry(cycle, PrefAddress);

			//if((GetPrefetchBit(1, PrefAddress) < 0) &&
			// !mshr->Search(PrefAddress)) {
			if (!mshr->Search(PrefAddress)) {
				mshrpf->Issue(cycle, PrefAddress, (MemObj*)myCache);
			}
			NumFwdPref--;
			count++;
		}
	}

	// This loop is implemented as a priority encoder
	for (int i = 1, count = 0;
			(NumBwdPref && (count < MAX_ISSUE) && (!mshrpf->Full())
					&& (i < (MAP_Size / 2))); i++) {
		if (BwdPrefMap[i] && (AddressRegister > (i * BLK_SIZE))) {
			AddrType PrefAddress = AddressRegister - (i * BLK_SIZE);
			BwdPrefMap[i] = false;

			// Update Memory Access Map
			pref->UpdateEntry(cycle, PrefAddress);

			//if((GetPrefetchBit(1, PrefAddress) < 0) &&
			// !mshr->Search(PrefAddress)) {
			if (!mshr->Search(PrefAddress)) {
				mshrpf->Issue(cycle, PrefAddress,(MemObj*) myCache);
			}
			NumBwdPref--;
			count++;
		}
	}

	/////////////////////////////////////////////////////
	// Stage 1. Access to Memory Access Map Table
	/////////////////////////////////////////////////////

	list<PrefetchData_t>::iterator lit;
	while (demandList.size()) {
		lit = demandList.begin();
		PrefetchData_t p = *lit;

		//=====================================================================
		// Insert Here IssuePrefetch Loop of original code

		bool MSHR_hit = mshr->Search(p.DataAddr) || mshrpf->Search(p.DataAddr);

		// Update MSHR
		// This stored data is used for filtering prefetch requests...
		if (!MSHR_hit && !p.hit) {
			mshr->Issue(cycle, p.DataAddr,(MemObj*) myCache);
		}

		// Read prefetch candidate from Memory Access Map Table
		pref->IssuePrefetch(cycle, p.DataAddr, MSHR_hit, FwdPrefMap, BwdPrefMap,
				&NumFwdPref, &NumBwdPref, (MemObj*)myCache);
		AddressRegister = p.DataAddr;
		AddressRegister &= ~(BLK_SIZE - 1);
		//=====================================================================

		demandList.pop_front();
	}
}

AccessMapPrefetcher::AccessMapPrefetcher(MemObj* mycache,
		unsigned int cid, unsigned int blocksize) :
		Prefetcher_Base(mycache, cid, blocksize) {

	mshrpf = new PrefetchMissStatusHandlingRegister();
	mshr = new MissStatusHandlingRegister();
	pref = new MemoryAccessMapTable();
	NumFwdPref = NumBwdPref = 0;
}

AccessMapPrefetcher::~AccessMapPrefetcher() {
	if (mshrpf)
		delete mshrpf;
	if (mshr)
		delete mshr;
	if (pref)
		delete pref;
}

