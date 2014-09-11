/*
 * AdaptiveStreamPrefetcher.cpp
 *
 *  Created on: 2013-09-18
 *      Author: iatta
 */

#include "AdaptiveStreamPrefetcher.h"

void AdaptiveStreamPrefetcher_wrapper::IssuePrefetches(Time_t cycle) {
	asp->Housekeeping(cycle);

	list<PrefetchData_t>::iterator lit;
	while (demandList.size()) {
		lit = demandList.begin();
		PrefetchData_t p = *lit;

		//=====================================================================
		// Insert Here IssuePrefetch Loop of original code

		// This prefetcher is based on "Adaptive Stream Prefetcher"
		asp->IssuePrefetch(cycle, p.DataAddr, p.hit);
		//=====================================================================

		demandList.pop_front();
	}
}

AdaptiveStreamPrefetcher_wrapper::AdaptiveStreamPrefetcher_wrapper(MemObj* cache,
		unsigned int cid, unsigned int blocksize) :
		Prefetcher_Base(cache, cid, blocksize) {
	asp = new AdaptiveStreamPrefetcher(cache, cid, blocksize);
}

AdaptiveStreamPrefetcher_wrapper::~AdaptiveStreamPrefetcher_wrapper() {
	if (asp)
		delete asp;
}

