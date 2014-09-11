/*
 * AdaptiveStreamPrefetcher.h
 *
 *  Created on: 2013-09-18
 *      Author: iatta
 */

#ifndef ADAPTIVESTREAMPREFETCHER_H_
#define ADAPTIVESTREAMPREFETCHER_H_

#include "Prefetcher_Base.h"
#include "AccessMapPrefetcher_Classes.h"
#include <iostream>


using namespace std;

class AdaptiveStreamPrefetcher_wrapper: public Prefetcher_Base {
public:
	AdaptiveStreamPrefetcher_wrapper(MemObj* cache, unsigned int cid, unsigned int blocksize);
	~AdaptiveStreamPrefetcher_wrapper();

	virtual void IssuePrefetches(Time_t cycle);

	// Adaptive Stream Prefetcher(1696 bits)
	AdaptiveStreamPrefetcher *asp;
};



#endif /* ADAPTIVESTREAMPREFETCHER_H_ */
