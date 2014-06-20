/*
 * PsliceConstructor.h
 *
 *  Created on: 2013-11-12
 *      Author: iatta
 *      This class is used to construct p-slices for PCs provided as an input file.
 */

#ifndef PSLICECONSTRUCTOR_H_
#define PSLICECONSTRUCTOR_H_

#include <vector>
#include <map>
#include <list>
#include <stdio.h>
#include <string>
#include <set>
#include <sstream>
#include <iostream>
#include "Pslice.h"
#include "RAWDInst.h"

using namespace std;

#define MAX_TARGET_ADDRESS 10
#define TARGETS_ONLY	// A directive to speed up the execution when we don't need to collect ptraces.
/*
 struct ComparePslicePtr{
 bool operator()(const Pslice* first, const Pslice* second) const{
 list<Pslice::ExecIns>::iterator myit;
 list<Pslice::ExecIns>::iterator otherit;

 myit = first->instructions.begin();
 otherit = second->instructions.begin();

 while (myit != first->instructions.end()) {

 AddrType myPC = (*myit).PC;
 AddrType otherPC = (*otherit).PC;

 if (myPC != otherPC)
 return false;

 myit++;
 otherit++;
 }

 return true;
 }
 };*/

class PsliceConstructor {
public:
	vector<AddrType> delinqPCs; // An array of all delinquent PCs that should be traced.
	vector<int> currentIter; // For each delinquent PC in delPCs, how many times have we seen this PC.
	vector<Pslice*> activePslices; // P-slices that are currently being constructed

	typedef list<AddrType> TARGET_ADDRESSES; // A list of all the target addresses generated by a given PC.
	vector<TARGET_ADDRESSES> targetAddresses; // A target address list per delinquent PC.

	typedef set<Pslice*> PSLICE_SET;
	vector<PSLICE_SET> allPslices; // This vector stores a list of all p-slices extracted and saved per delinquent PC

	PsliceConstructor(string pcs_filename);
	~PsliceConstructor();

	void recieveInstruction(FlowID fid, AddrType PC, uint32_t opcode,
			AddrType addr, AddrType data, ARMReg src1_reg, ARMReg src2_reg,
			ARMReg src3_reg, ARMReg dst_reg, ARMReg impl_reg, uint32_t src1_val,
			uint32_t src2_val, uint32_t src3_val, uint32_t dst_val,
			uint32_t impl_val, bool isBranch, bool isCond, bool taken,
			AddrType targetAddr);

	void delinqPCSeen(int index) {
		currentIter[index]++;
	}

	bool canCreatePslice(int index);

	void dumpAllPslices();
	void dumpTargetAddresses();
	void dumpTargetAddresses(unsigned int delinquentLdID, FlowID fid);

};

#endif /* PSLICECONSTRUCTOR_H_ */
