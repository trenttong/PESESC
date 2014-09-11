/*
 * PrefetchInterface.cpp
 *
 *  Created on: 2013-09-17
 *      Author: iatta
 */
#include <sstream>
#include "PrefetchInterface.h"
#include "AdaptiveStreamPrefetcher.h"
#include "AccessMapPrefetcher.h"

Prefetcher_Base* PrefetchInterface::L1Prefetcher;
Prefetcher_Base* PrefetchInterface::L2Prefetcher;
Prefetcher_Base* PrefetchInterface::L3Prefetcher;

PrefetchInterface::TYPE PrefetchInterface::l1pType;
PrefetchInterface::TYPE PrefetchInterface::l2pType;
PrefetchInterface::TYPE PrefetchInterface::l3pType;

void PrefetchInterface::IssuePrefetches(Time_t cycle) {
	if (L3Prefetcher)
		L3Prefetcher->IssuePrefetches(cycle);
	if (L2Prefetcher)
		L2Prefetcher->IssuePrefetches(cycle);
	if (L1Prefetcher)
		L1Prefetcher->IssuePrefetches(cycle);
}

void PrefetchInterface::insertLevelDemand(Prefetcher_Base* prefetcher,
		AddrType addr, bool hit) {
	if (prefetcher) {
		PrefetchData_t p;
		p.DataAddr = addr;
		p.hit = hit;
		prefetcher->demandList.push_back(p);
	}
}

void PrefetchInterface::insertDemands(
		DInst* dinst/*AddrType addr, string hitLevelString*/) {

	AddrType addr = dinst->getAddr();
	MemObj* obj = dinst->getHitLevel();
	string objName = string(obj->getName());
	LEVEL hitlevel = getLevel(objName);

	if (hitlevel == InvalidLevel && dinst->isLoadForwarded())
		return;

	I(hitlevel != InvalidLevel);

	bool hit = true, miss = false;
	// Identify the miss/hit levels
	switch (hitlevel) {
	case LoadForward:
		// Ignore load forwards.
		break;
	case L1:
		if (!L1Prefetcher) {
			if (l1pType == NONE)
				return;
			else if (l1pType == ASP)
				L1Prefetcher = new AdaptiveStreamPrefetcher_wrapper(obj, 0, 64);
		}
		insertLevelDemand(L1Prefetcher, addr, hit);
		break;
	case L2:
		if (!L2Prefetcher) {
			if (l2pType == NONE)
				return;
			else if (l2pType == ASP)
				L2Prefetcher = new AdaptiveStreamPrefetcher_wrapper(obj, 0, 64);
		}
		insertLevelDemand(L1Prefetcher, addr, miss);
		insertLevelDemand(L2Prefetcher, addr, hit);
		break;
	case L3:
		if (!L3Prefetcher) {
			if (l3pType == NONE)
				return;
			else if (l3pType == ASP)
				L3Prefetcher = new AdaptiveStreamPrefetcher_wrapper(obj, 0, 64);
			else if (l3pType == AMPM)
				L3Prefetcher = new AccessMapPrefetcher(obj, 0, 64);
		}
		insertLevelDemand(L1Prefetcher, addr, miss);
		insertLevelDemand(L2Prefetcher, addr, miss);
		insertLevelDemand(L3Prefetcher, addr, hit);
		break;
	case MEMORY:
		insertLevelDemand(L1Prefetcher, addr, miss);
		insertLevelDemand(L2Prefetcher, addr, miss);
		insertLevelDemand(L3Prefetcher, addr, miss);
		break;
	default:
		cerr << "Message with incorrect hit level: " << hitlevel << endl;
		break;
	}
}

PrefetchInterface::LEVEL PrefetchInterface::getLevel(string objName) {

	if (objName.find("DL1") != string::npos)
		return L1;
	if (objName.find("L2") != string::npos)
		return L2;
	if (objName.find("L3") != string::npos)
		return L3;
	if (objName.find("niceCache") != string::npos)
		return MEMORY;

	return InvalidLevel;
}

void PrefetchInterface::InitialzeTypes(string l1p, string l2p, string l3p) {
	l1pType = l2pType = l3pType = NONE;

	if (l1p.compare("asp") == 0)
		l1pType = ASP;
	if (l2p.compare("asp") == 0)
		l2pType = ASP;
	if (l3p.compare("ampm") == 0)
		l3pType = AMPM;
}

void PrefetchInterface::clearDemandLists() {
	L1Prefetcher->clearDemandList();
	L2Prefetcher->clearDemandList();
	L3Prefetcher->clearDemandList();
}

// The L1 prefetcher can call this function to issue prefetches into L1 cache
bool PrefetchInterface::IssueL1Prefetch(AddrType addr) {
	cout << "L1 Prefetch: " << addr << endl;
	return true;
}

// The L2 prefetcher can call this function to issue prefetches into L2 cache
bool PrefetchInterface::IssueL2Prefetch(AddrType addr) {
	cout << "L2 Prefetch: " << addr << endl;
	return true;
}

// The L3 prefetcher can call this function to issue prefetches into L3 cache
bool PrefetchInterface::IssueL3Prefetch(AddrType addr) {
	cout << "L3 Prefetch: " << addr << endl;
	return true;
}
