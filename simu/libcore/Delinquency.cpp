/*
 * Delinquency.cpp
 *
 *  Created on: 2013-11-04
 *      Author: iatta
 */

#include "Delinquency.h"
#include "SescConf.h"
#include "MemObj.h"
#include "DInst.h"
#include <sstream>

#ifdef DELINQUENT_LOAD

vector<DelinquencyAnalyzer::ADDR_MAP> DelinquencyAnalyzer::instMaps;
vector<DelinquencyAnalyzer::ADDR_MAP> DelinquencyAnalyzer::dataMaps;
vector<DelinquencyAnalyzer::stringVector> DelinquencyAnalyzer::levelNames;

void DelinquencyAnalyzer::addMap(FlowID fid) {
	FlowID nsimu = SescConf->getRecordSize("", "cpusimu");

	if (instMaps.empty()) {
		instMaps.resize(nsimu);
	}I(!instMaps.empty());

	if (dataMaps.empty()) {
		dataMaps.resize(nsimu);
	}I(!dataMaps.empty());

	if (levelNames.empty()) {
		levelNames.resize(nsimu);
		for (uint32_t i = 0; i < nsimu; i++)
			levelNames[i].resize(4);
	}I(!levelNames.empty());

	instMaps[fid].clear();
	dataMaps[fid].clear();
	initLevelNames(fid);
}

string privatizeDeviceName(string given_name, int32_t num) {
	string temp = given_name;
	temp += '(';
	ostringstream convert;
	convert << num;
	temp += convert.str();
	temp += ')';
	return temp;
}

DelinquencyAnalyzer::LEVEL DelinquencyAnalyzer::getLevel(FlowID fid,
		MemObj* obj) {
	if (obj) {
		string objName = string(obj->getName());
		for (LEVEL l = L1; l < InvalidLevel; l = (LEVEL) ((int) l + 1)) {
			//if (levelNames[fid][l].compare(objName) == 0)
			if(objName.find(levelNames[fid][l]) != string::npos)
				return l;
		}
	}
	return InvalidLevel;
}

/*
 char *privatizeDeviceName(char *given_name, int32_t num) {
 char *ret = new char[strlen(given_name) + 8 + (int) log10((float) num + 10)];
 sprintf(ret, "%s(%d)", given_name, num);
 delete[] given_name;
 return ret;
 }
 */
/*
 DelinquencyAnalyzer::LEVEL DelinquencyAnalyzer::getLevel(MemObj* obj) {
 if (obj) {
 const char* objName = obj->getName();
 for (LEVEL l = L1; l < InvalidLevel; l = (LEVEL) ((int) l + 1)) {
 if (!strcmp(levelNames[l], objName))
 return l;
 }
 }
 return InvalidLevel;
 }*/

void DelinquencyAnalyzer::initLevelNames(FlowID myFlowId) {
	// Figure out the different names of all memory levels.
	// TODO: the naming conventions here assume either private or globaly shared memory objects
	//		e.g., a semi-shared object (L2 shared by 4 cores from a total of 16 cores) would not work properly.
	string dl1conf = string(SescConf->getCharPtr("Delinquency", "L1_name"));
	const bool dl1_shared = SescConf->getBool("Delinquency", "L1_shared");
	if (!dl1_shared) {
		levelNames[myFlowId][L1] = privatizeDeviceName(dl1conf,
				(int32_t) myFlowId);
	} else {
		levelNames[myFlowId][L1] = dl1conf;
	}

	string l2conf = string(SescConf->getCharPtr("Delinquency", "L2_name"));
	const bool l2_shared = SescConf->getBool("Delinquency", "L2_shared");
	if (!l2_shared) {
		levelNames[myFlowId][L2] = privatizeDeviceName(l2conf,
				(int32_t) myFlowId);
	} else {
		levelNames[myFlowId][L2] = l2conf;
	}

	const char* l3conf = SescConf->getCharPtr("Delinquency", "L3_name");
	const bool l3_shared = SescConf->getBool("Delinquency", "L3_shared");
	if (!l3_shared) {
		levelNames[myFlowId][L3] = privatizeDeviceName(l3conf,
				(int32_t) myFlowId);
	} else {
		levelNames[myFlowId][L3] = l3conf;
	}

	levelNames[myFlowId][MEMORY] = SescConf->getCharPtr("Delinquency",
			"Memory_name");
	levelNames[myFlowId][MEMORY] += "(0)";
}

void DelinquencyAnalyzer::sortAndReportTopDelinquentPCs(unsigned int count) {

	//Dump top delinquent loads in a CSV file.
	for (uint32_t i = 0; i < instMaps.size(); i++) {
		string filename = "topdelinquent_";
		ostringstream convert;
		convert << i;
		filename += convert.str();
		filename += ".csv";
		ofstream csv(filename.c_str(), std::fstream::out);

		if (csv.good()) {
			set<perInstStat> highestAverageLatencyPCs;

			csv
					<< "PC,Load Forward,L1_hits,L2_hits,L3_hits,Offchip,Sum of Distances, Sum of Delays,L1_missRate,L2_missRate,L3_missRate,Avg Access Distance,Max Distance,Min Distance,Avg Access Latency\n";

			map<AddrType, perInstStat>::iterator mapit;
			for (mapit = instMaps[i].begin(); mapit != instMaps[i].end();
					mapit++) {
				perInstStat t = (*mapit).second;
				highestAverageLatencyPCs.insert(t);
				while (highestAverageLatencyPCs.size() > count) {
					set<perInstStat>::iterator first = highestAverageLatencyPCs.begin();
					highestAverageLatencyPCs.erase(first);
				}
			}

			set<perInstStat>::reverse_iterator rit;
			for (rit = highestAverageLatencyPCs.rbegin();
					rit != highestAverageLatencyPCs.rend(); rit++) {
				perInstStat p = *rit;
				double L1_missRate = (double) (p.hitL2 + p.hitL3 + p.hitMem)
						/ (double) (p.hitL1 + p.hitL2 + p.hitL3 + p.hitMem);
				double L2_missRate = (double) (p.hitL3 + p.hitMem)
						/ (double) (p.hitL2 + p.hitL3 + p.hitMem);
				double L3_missRate = (double) (p.hitMem)
						/ (double) (p.hitL3 + p.hitMem);
				double avgAccessDistance = (double) p.sumOfDifferences
						/ (double) (p.hitL1 + p.hitL2 + p.hitL3 + p.hitMem
								+ p.loadForward - 1);
				double avgLatency = (double) p.sumOfLoadDelays
						/ (double) (p.hitL1 + p.hitL2 + p.hitL3 + p.hitMem
								+ p.loadForward);

				csv << std::hex << p.addr << std::dec << "," << p.loadForward << "," << p.hitL1 << ","
						<< p.hitL2 << "," << p.hitL3 << "," << p.hitMem << ","
						<< p.sumOfDifferences << "," << p.sumOfLoadDelays << ","
						<< L1_missRate << "," << L2_missRate << ","
						<< L3_missRate << "," << avgAccessDistance << ","
						<< p.maxReuse << "," << p.minReuse << "," << avgLatency
						<< endl;
			}
			csv.close();
		} else
			cerr << "ERROR: cannot open CSV file: " << filename << "!\n";
	}
}

void DelinquencyAnalyzer::reset() {
	instMaps.clear();
	dataMaps.clear();
}

void DelinquencyAnalyzer::insertStat(FlowID fid, DInst* dinst) {

	LEVEL level = getLevel(fid, dinst->getHitLevel());
	if (level == InvalidLevel && dinst->isLoadForwarded())
		level = LoadForward;
	I(level != InvalidLevel);
	Time_t reuseID = dinst->getID();
	AddrType instAddr = dinst->getPC();
	AddrType dataAddr = dinst->getAddr();
	Time_t delay = globalClock - dinst->getFetchTime();

	insertInstMap(fid, instAddr, dataAddr, level, reuseID, delay);
//insertDataMap(dataAddr, level, reuseID, delay);
}

void DelinquencyAnalyzer::insertInstMap(FlowID fid, AddrType instAddr,
		AddrType dataAddr, LEVEL level, Time_t reuseID, Time_t delay) {
	if (instMaps[fid].find(instAddr) == instMaps[fid].end()) {
		perInstStat statElement;
		statElement.hitL1 = statElement.hitL2 = statElement.hitL3 = statElement.hitMem = statElement.loadForward = statElement.sumOfLoadDelays = statElement.sumOfDifferences = 0;
		statElement.lastReuse = reuseID;
		statElement.maxReuse = 0;
		statElement.minReuse = MaxTime;
		statElement.addr = instAddr;
		instMaps[fid][instAddr] = statElement;
	}
	switch (level) {
	case L1:
		instMaps[fid][instAddr].hitL1++;
		break;
	case L2:
		instMaps[fid][instAddr].hitL2++;
		break;
	case L3:
		instMaps[fid][instAddr].hitL3++;
		break;
	case MEMORY:
		instMaps[fid][instAddr].hitMem++;
		break;
	case LoadForward:
		instMaps[fid][instAddr].loadForward++;
		break;
	default:
		I(0);
		break;
	}
	// TODO: Add max and min reuse distance
	Time_t reuseDistance = reuseID - instMaps[fid][instAddr].lastReuse;
	instMaps[fid][instAddr].sumOfDifferences += reuseDistance;
	instMaps[fid][instAddr].maxReuse = MAX(instMaps[fid][instAddr].maxReuse, reuseDistance);
	if (reuseDistance)
		instMaps[fid][instAddr].minReuse = MIN(instMaps[fid][instAddr].minReuse, reuseDistance);
	instMaps[fid][instAddr].lastReuse = reuseID;
	instMaps[fid][instAddr].sumOfLoadDelays += delay;
}

void DelinquencyAnalyzer::insertDataMap(FlowID fid, AddrType dataAddr,
		LEVEL level, Time_t reuseID, Time_t delay) {
	if (dataMaps[fid].find(dataAddr) == dataMaps[fid].end()) {
		perInstStat statElement;
		statElement.hitL1 = statElement.hitL2 = statElement.hitL3 = statElement.hitMem = statElement.loadForward = statElement.sumOfLoadDelays = statElement.sumOfDifferences = 0;
		statElement.lastReuse = reuseID;
		statElement.addr = dataAddr;
		dataMaps[fid][dataAddr] = statElement;
	}
	switch (level) {
	case L1:
		dataMaps[fid][dataAddr].hitL1++;
		break;
	case L2:
		dataMaps[fid][dataAddr].hitL2++;
		break;
	case L3:
		dataMaps[fid][dataAddr].hitL3++;
		break;
	case MEMORY:
		dataMaps[fid][dataAddr].hitMem++;
		break;
	case LoadForward:
		dataMaps[fid][dataAddr].loadForward++;
		break;
	default:
		I(0);
		break;
	}

	dataMaps[fid][dataAddr].sumOfDifferences += reuseID
			- dataMaps[fid][dataAddr].lastReuse;
	dataMaps[fid][dataAddr].lastReuse = reuseID;
	dataMaps[fid][dataAddr].sumOfLoadDelays += delay;
}

#endif // DELINQUENT_LOAD
