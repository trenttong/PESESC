#ifndef INTERFACE_H
#define INTERFACE_H

#include <iostream>
#include <string>
#include "DInst.h"
#include "Prefetcher_Base.h"

using namespace std;

class DInst;

class PrefetchInterface {
public:

	enum LEVEL {
		L1 = 0, L2, L3, MEMORY, InvalidLevel, LoadForward
	};

	enum TYPE {
		NONE = 0, ASP, AMPM
	};

	static TYPE l1pType;
	static TYPE l2pType;
	static TYPE l3pType;

	static Prefetcher_Base* L1Prefetcher;
	static Prefetcher_Base* L2Prefetcher;
	static Prefetcher_Base* L3Prefetcher;

	static void Initialize(Prefetcher_Base* l1pref, Prefetcher_Base* l2pref,
			Prefetcher_Base* l3pref) {
		L1Prefetcher = l1pref;
		L2Prefetcher = l2pref;
		L3Prefetcher = l3pref;
	}

	static void InitialzeTypes(string l1p, string l2p, string l3p);

	// The processor calls this function every cycle to issue prefetches should the prefetcher want to
	static void IssuePrefetches(Time_t cycle);

	// The L1 prefetcher can call this function to issue prefetches into L1 cache
	static bool IssueL1Prefetch(AddrType addr);

	// The L2 prefetcher can call this function to issue prefetches into L2 cache
	static bool IssueL2Prefetch(AddrType addr);

	// The L2 prefetcher can call this function to issue prefetches into L2 cache
	static bool IssueL3Prefetch(AddrType addr);

	static void insertDemands(DInst* dinst);

	static void insertLevelDemand(Prefetcher_Base* prefetcher, AddrType addr,
			bool hit);

	static void clearDemandLists();
	static LEVEL getLevel(string objName);
	static void initLevelNames(FlowID myFlowId);

};

#endif

