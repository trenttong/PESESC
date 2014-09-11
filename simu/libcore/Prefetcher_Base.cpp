/*
 * Prefetcher_Base.cpp
 *
 *  Created on: 2013-09-18
 *      Author: iatta
 */

#include "Prefetcher_Base.h"

Prefetcher_Base::Prefetcher_Base(void* cache, unsigned int cid, unsigned int blocksize) {
	myCache = cache;
	coreID = cid;
	cacheBlockSize = blocksize;
	log2BlockSize = log2(cacheBlockSize);
}

void Prefetcher_Base::clearDemandList() {
	demandList.clear();
}

Prefetcher_Base::~Prefetcher_Base() {
}
