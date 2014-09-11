/*
 * Delinquency.h
 *
 *  Created on: 2013-11-04
 *      Author: iatta
 */

#ifndef DELINQUENCY_H_
#define DELINQUENCY_H_

#include <map>
#include <stdlib.h>
#include <vector>
#include <set>
#include <list>
#include <queue>
#include <math.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include "RAWDInst.h"
#include "callback.h"
#include "DInst.h"

using namespace std;

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) > (b)) ? (b) : (a))

class MemObj;
class DInst;

#ifdef DELINQUENT_LOAD

#define DELINQUENT_LOAD_COUNT 100000

class DelinquencyAnalyzer {

protected:
	//enum HIT_LEVEL {
	//	L1 = 0, L2, L3, MEMORY, InvalidLevel, LoadForward
	//};

	struct perInstStat {
		ulong hitL1;
		ulong hitL2;
		ulong hitL3;
		ulong hitMem;
		ulong loadForward;

		// Measure the average distance between two successive loads in number of instructions.
		Time_t sumOfDifferences;
		Time_t lastReuse;
		Time_t maxReuse;
		Time_t minReuse;

		Time_t sumOfLoadDelays;

		AddrType addr;

		bool operator<(perInstStat other) const {
			double myAvgDelay = (double) sumOfLoadDelays
			/ (double) (hitL1 + hitL2 + hitL3 + hitMem + loadForward);
			double otherAvgDelay = (double) other.sumOfLoadDelays
			/ (double) (other.hitL1 + other.hitL2 + other.hitL3
					+ other.hitMem + other.loadForward);
			return myAvgDelay < otherAvgDelay;
		}

		void operator=(perInstStat other) {
			hitL1 = other.hitL1;
			hitL2 = other.hitL2;
			hitL3 = other.hitL3;
			hitMem = other.hitMem;
			loadForward = other.loadForward;
			addr = other.addr;
			sumOfDifferences = other.sumOfDifferences;
			lastReuse = other.lastReuse;
			maxReuse = other.maxReuse;
			minReuse = other.minReuse;
			sumOfLoadDelays = other.sumOfLoadDelays;
		}
	};

	static HIT_LEVEL getLevel(FlowID fid, MemObj* obj);

	typedef map<AddrType, perInstStat> ADDR_MAP;
	static vector<ADDR_MAP> instMaps;
	static vector<ADDR_MAP> dataMaps;

	typedef vector<string> stringVector;
	static vector<stringVector> levelNames;

public:
	static void initLevelNames(FlowID myFlowId);
	static void sortAndReportTopDelinquentPCs(unsigned int count =
			DELINQUENT_LOAD_COUNT);

	static void reset();
	static void insertStat(FlowID fid, DInst* dinst);
	static void insertInstMap(FlowID fid, AddrType instAddr,
			HIT_LEVEL level, Time_t reuseID, Time_t delay);
	static void insertDataMap(FlowID fid, AddrType dataAddr, HIT_LEVEL level,
			Time_t reuseID, Time_t delay);

	static void addMap(FlowID fid);
};

#endif // DELINQUENT_LOAD
#endif /* DELINQUENCY_H_ */
