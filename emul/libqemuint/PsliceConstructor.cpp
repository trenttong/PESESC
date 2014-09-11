/*
 * PsliceConstructor.cpp
 *
 *  Created on: 2013-11-12
 *      Author: iatta
 */

#include "PsliceConstructor.h"

PsliceConstructor::PsliceConstructor(string pcs_filename) {
	FILE* f = fopen(pcs_filename.c_str(), "r");

	if (!f) {
		fprintf(stderr, "OOPS: error while opening PC list file: %s\n",
				pcs_filename.c_str());
		exit(-1);
	}

	set<AddrType> pcSet; // Using a set to avoid redundant PCs

	while (!feof(f)) {
		uint32_t t;
		fscanf(f, "%x", &t);
		pcSet.insert((AddrType) t);
	}

	if (pcSet.empty())
		fprintf(stderr, "No Delinquent PCs read from %s", pcs_filename.c_str());

	delinqPCs.resize(pcSet.size());
	currentIter.resize(pcSet.size());
	activePslices.resize(pcSet.size());
	allPslices.resize(pcSet.size());
#ifdef TARGETS
	targetAddresses.resize(pcSet.size() * 2); // ATTA: we're creating two lists: Mcore and Pcore
#endif // TARGETS

	set<AddrType>::iterator it;
	int i = 0;
	for (it = pcSet.begin(); it != pcSet.end(); it++, i++) {
		delinqPCs[i] = (AddrType) (*it);
		currentIter[i] = 0;
	}
}

void PsliceConstructor::recieveInstruction(FlowID fid, AddrType PC,
		uint32_t opcode, AddrType addr, AddrType data, ARMReg src1_reg,
		ARMReg src2_reg, ARMReg src3_reg, ARMReg dst_reg, ARMReg impl_reg,
		uint32_t src1_val, uint32_t src2_val, uint32_t src3_val,
		uint32_t dst_val, uint32_t impl_val, bool isBranch, bool isCond,
		bool taken, AddrType targetAddr) {

#ifdef TARGETS
	// Let's insert the loaded address into the corresponding target address list
	for (int i = 0; i < delinqPCs.size(); i++) {
		if (delinqPCs[i] == PC) {
			int k = i + (fid * delinqPCs.size()); // k is the corresponding index in the targetAddresses
			if (targetAddresses[k].size() > MAX_TARGET_ADDRESS)
				dumpTargetAddresses(i, fid);
			targetAddresses[k].push_back(addr);
		}
	}
#endif // TARGETS

#ifndef TARGETS_ONLY
	// Are we constructing any Pslices now?
	// If yes, then let's inject this instruction.
	for (unsigned int i = 0; i < activePslices.size(); i++) {
		if (activePslices[i]) {
			activePslices[i]->insertInst(PC, opcode, addr, data, src1_reg,
					src2_reg, src3_reg, dst_reg, impl_reg, src1_val, src2_val,
					src3_val, dst_val, impl_val, isBranch, isCond, taken,
					targetAddr);
		}
	}

	// // Let's check whether the p-slice is larger than the maximum, if yes, we should discard it
	// (it's probable that execution went out of the loop bounds).
	for (unsigned int i = 0; i < activePslices.size(); i++) {
		if (activePslices[i]) {
			if (activePslices[i]->size() > PSLICE_MAX_SIZE) {
				Pslice* t = activePslices[i];
				activePslices[i] = 0;
				delete t;
			}
		}
	}

	// Now let's find out whether this PC matches a delinquent load.
	// If it maches:
	//      If the corresponding p-slice is active, then this is the target load, and
	//			we should stop collecting instructions in this p-slice and open a new one.
	//      Else, this delinquent loads triggers the creation of a new p-slice.

	for (unsigned int i = 0; i < activePslices.size(); i++) {
		if (delinqPCs[i] == PC) {
			delinqPCSeen(i);

			if (activePslices[i]) {
				// We are currently collecting a p-slice for this PC. Then let's wrap up this
				// p-slice, add it to the corresponding p-slice list (if there's no other identical p-slice)
				PSLICE_SET::iterator sit;
				for (sit = allPslices[i].begin(); sit != allPslices[i].end();
						sit++) {
					// Compare p-slices. If they are similar, then discard the newly generated instance
					// and increment the frequency of the previous.
					if ((*sit)->isEqual(activePslices[i])) {
						(*sit)->frequency++;
						delete activePslices[i];
						activePslices[i] = 0;
						break;
					}
				}

				// If none turn out to be similar, then let's insert this p-slice in the set.
				if (activePslices[i]) {
#ifdef CONSTRUCT_PSLICE
					activePslices[i]->generatePslice();
#endif // CONSTRUCT_PSLICE
					allPslices[i].insert(activePslices[i]);
					activePslices[i] = 0;
				}
			}

			if (!activePslices[i] && canCreatePslice(i)) {
				// Now let's create a new p-slice
				activePslices[i] = new Pslice(PC, currentIter[i]);
			}
		}
	}
#endif // TARGETS_ONLY
}

// This function determines if we can or cannot create a new p-slice for this delinquent PC.
bool PsliceConstructor::canCreatePslice(int index) {
	return true;
}

void PsliceConstructor::dumpAllPslices() {
	for (unsigned int i = 0; i < activePslices.size(); i++) {
		if (allPslices[i].size()) {
			PSLICE_SET::iterator it;
			for (it = allPslices[i].begin(); it != allPslices[i].end(); it++) {
				(*it)->dumpToFile();
			}
		}
	}
}

PsliceConstructor::~PsliceConstructor() {
	dumpAllPslices();

	// Clear all pslices
	for (unsigned int i = 0; i < activePslices.size(); i++) {
		if (allPslices[i].size()) {
			PSLICE_SET::iterator it;
			for (it = allPslices[i].begin(); it != allPslices[i].end(); it++) {
				Pslice* t = (Pslice*) (*it);
				t->clear();
				delete t;
			}
		}
	}

#ifdef TARGETS
	// Dump target addresses
	dumpTargetAddresses();
#endif // TARGETS
}

#ifdef TARGETS
void PsliceConstructor::dumpTargetAddresses(unsigned int delinquentLdID,
		FlowID fid) {

	// Verify the delinquent load ID is valid.
	if (delinquentLdID >= delinqPCs.size()) {
		cerr << "Invalid delinquent Load ID in dumpTragetAddresses ("
				<< delinquentLdID << endl;
		return;
	}

	string fileName = "pslices/targets-";
	ostringstream delPCstr;
	delPCstr << std::hex << delinqPCs[delinquentLdID] << std::dec << "-" << fid
			<< ".out";
	fileName += delPCstr.str();
	FILE* outFile = fopen(fileName.c_str(), "a");

	if (!outFile) {
		fprintf(stderr,
				"OOPS: target addresses file (%s) could not be created!\n",
				fileName.c_str());
	}

	TARGET_ADDRESSES::iterator it;

	// k is the targetAddresses index corresponding to the delinquent ID and fid
	int k = delinquentLdID + (fid * delinqPCs.size());

	for (it = targetAddresses[k].begin();
			it != targetAddresses[k].end(); it++) {
		AddrType temp = (AddrType) (*it);
		fprintf(outFile, "%x\n", temp);
	}

	targetAddresses[k].clear();

	fclose(outFile);

	return;
}

void PsliceConstructor::dumpTargetAddresses() {
	for (int i = 0; i < delinqPCs.size(); i++) {
		dumpTargetAddresses(i, 0);	// Dump for Mcore
		dumpTargetAddresses(i, 1);	// Dump for Pcore
	}
	return;
}
#endif // TARGETS
