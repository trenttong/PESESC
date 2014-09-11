/*
 * Prefetcher_Base.h
 *
 *  Created on: 2013-09-18
 *      Author: iatta
 */

#ifndef PREFETCHER_BASE_H_
#define PREFETCHER_BASE_H_

#include <list>
#include <math.h>
#include <stdint.h>
#include "RAWDInst.h"
#include "Snippets.h"

//typedef uint64_t AddrType;
//typedef uint32_t FlowID;

using namespace std;

typedef struct {
	AddrType LastRequestAddr; // Program Counter of the instruction that made the request
	int32_t AccessType; // Request type (Load:1, Store:2)
	AddrType DataAddr; // Virtual address for the data being requested
	bool hit; // Did the request hit in the cache?
	bool LastRequestPrefetch; // Was this a prefetch request?
} PrefetchData_t;

class Prefetcher_Base {
public:
	void* myCache;
	unsigned int coreID;
	list<PrefetchData_t> demandList;
	unsigned int cacheBlockSize;
	unsigned int log2BlockSize;

	Prefetcher_Base(void* cache, unsigned int cid, unsigned int blocksize);
	virtual ~Prefetcher_Base();

	virtual void IssuePrefetches(Time_t) = 0;

	void clearDemandList();

};

#endif /* PREFETCHER_BASE_H_ */
