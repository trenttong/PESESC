/*
 * Pslice.cpp
 *
 *  Created on: 2013-11-12
 *      Author: iatta
 */

#include "nanassert.h"
#include "Pslice.h"
#include <sstream>
#include <iostream>

using namespace std;

Pslice::Pslice(AddrType pc, int iter) :
		delPC(pc), iterationNumber(iter) {
	frequency = 1; // Initially, a pslice represnts at least one occurrence
}

void Pslice::insertInst(AddrType pc, uint32_t insn, AddrType addr,
		AddrType data, ARMReg src1_reg, ARMReg src2_reg, ARMReg src3_reg,
		ARMReg dst_reg, ARMReg impl_reg, uint32_t src1_val, uint32_t src2_val,
		uint32_t src3_val, uint32_t dst_val, uint32_t impl_val, bool isBranch,
		bool isCond, bool taken, AddrType targetAddr) {

	ExecIns ins;
	ins.PC = pc;
	ins.opcode = insn;
	ins.addr = addr;
	ins.data = data;
	ins.src1_reg = src1_reg;
	ins.src2_reg = src2_reg;
	ins.src3_reg = src3_reg;
	ins.dst_reg = dst_reg;
	ins.impl_reg = impl_reg;
	ins.src1_val = src1_val;
	ins.src2_val = src2_val;
	ins.src3_val = src3_val;
	ins.dst_val = dst_val;
	ins.impl_val = impl_val;
	ins.isBranch = isBranch;
	ins.isCond = isCond;
	ins.taken = taken;
	ins.targetAddr = targetAddr;

	ptrace.push_back(ins);
	ptrace_set.insert(pc);
}

void Pslice::dumpToFile() {
	dumpPtrace();
#ifdef CONSTRUCT_PSLICE
	dumpPslice();
#endif //CONSTRUCT_PSLICE
}
#ifdef CONSTRUCT_PSLICE
bool Pslice::dumpPslice() {
	string fileName = "pslices/";
	ostringstream delPCstr;
	delPCstr << std::hex << delPC << std::dec;
	fileName += delPCstr.str();
	fileName += "-";
	ostringstream iterStr;
	iterStr << iterationNumber;
	fileName += iterStr.str();
	fileName += "-";
	ostringstream freqStr;
	freqStr << frequency;
	fileName += freqStr.str();
	fileName += ".sl.csv";
	FILE* csv = fopen(fileName.c_str(), "w");

	if (!csv) {
		fprintf(stderr, "OOPS: pslice file could not be created!\n");
		return false;
	}

	// Generate and print the signature
	generatePsliceSignature();
	fprintf(csv, "%u", (unsigned int) signature.size());
	list<AddrType>::iterator sigit;
	for (sigit = signature.begin(); sigit != signature.end(); sigit++) {
		fprintf(csv, ",%u", (unsigned int) (*sigit));
	}
	fprintf(csv, "\n");

	// Print the frequency of this pslice
	fprintf(csv, "%u\n", frequency);

	// Print input live Ins
	identifyPsliceLiveIns();
	fprintf(csv, "%u\n", numberOfLiveIns);
	for (unsigned int i = 0; i < INVALID_ARM_REG; i++) {
		if (liveIns[i])
			fprintf(csv, "%u,%u\n", i, RegisterFile[i]);
	}

	list<ExecIns>::iterator it;

	for (it = pslice.begin(); it != pslice.end(); it++) {
		ExecIns ins = (ExecIns) (*it);
		fprintf(csv, "%u,0x%x,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%u\n",
				(uint32_t) ins.PC, (uint32_t) ins.opcode, (uint32_t) ins.addr,
				(uint32_t) ins.data, ins.src1_reg, ins.src2_reg, ins.src3_reg,
				ins.dst_reg, ins.impl_reg, ins.src1_val, ins.src2_val,
				ins.src3_val, ins.dst_val, ins.impl_val, ins.isBranch,
				ins.isCond, ins.taken, (uint32_t) ins.targetAddr);
	}

	// Print the loadStores counter as a ratio of ptrace size.
	fprintf(csv, "\nDebugging Statistics\n");
	fprintf(csv, "Ptrace size,%u\n", (unsigned int) ptrace.size());
	fprintf(csv, "Pslice size,%u\n", (unsigned int) pslice.size());
	fprintf(csv, "Load/Store instructions,%u\n", loadStores);

	fclose(csv);
	return true;
}
#endif // CONSTRUCT_PSLICE

bool Pslice::dumpPtrace() {
	string fileName = "pslices/";
	ostringstream delPCstr;
	delPCstr << std::hex << delPC << std::dec;
	fileName += delPCstr.str();
	fileName += "-";
	ostringstream iterStr;
	iterStr << iterationNumber;
	fileName += iterStr.str();
	fileName += "-";
	ostringstream freqStr;
	freqStr << frequency;
	fileName += freqStr.str();
	fileName += ".tr.csv";
	FILE* csv = fopen(fileName.c_str(), "w");

	if (!csv) {
		fprintf(stderr, "OOPS: ptrace file could not be created!\n");
		return false;
	}

	// Print input live Ins
	identifyPtraceLiveIns();
	fprintf(csv, "%u\n", numberOfLiveIns);
	for (unsigned int i = 0; i < INVALID_ARM_REG; i++) {
		if (liveIns[i])
			fprintf(csv, "%u,%u\n", i, RegisterFile[i]);
	}

	list<ExecIns>::iterator it;

	for (it = ptrace.begin(); it != ptrace.end(); it++) {
		ExecIns ins = (ExecIns) (*it);
		fprintf(csv, "%x,%x,%x,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%u\n",
				(uint32_t) ins.PC, (uint32_t) ins.opcode, (uint32_t) ins.addr,
				(uint32_t) ins.data, ins.src1_reg, ins.src2_reg, ins.src3_reg,
				ins.dst_reg, ins.impl_reg, ins.src1_val, ins.src2_val,
				ins.src3_val, ins.dst_val, ins.impl_val, ins.isBranch,
				ins.isCond, ins.taken, (uint32_t) ins.targetAddr);
	}

	fclose(csv);
	return true;
}

void Pslice::identifyPtraceLiveIns() {
	// Algorithm: a live-in register is a SRC register that has not been written to (i.e. was not a DST) by a prior instruction
	bool dstRegs[INVALID_ARM_REG + 1];
	for (unsigned int i = 0; i <= INVALID_ARM_REG; i++) {
		dstRegs[i] = false;
		liveIns[i] = false;
	}

	list<ExecIns>::iterator it;
	for (it = ptrace.begin(); it != ptrace.end(); it++) {
		ExecIns ins = (ExecIns) (*it);

		// NOTE: The current interface assumes that implicit registers are always sources

		if (!dstRegs[ins.impl_reg] && !liveIns[ins.impl_reg]) {
			liveIns[ins.impl_reg] = true;
			RegisterFile[ins.impl_reg] = ins.impl_val;
		}

		if (!dstRegs[ins.src1_reg] && !liveIns[ins.src1_reg]) {
			liveIns[ins.src1_reg] = true;
			RegisterFile[ins.src1_reg] = ins.src1_val;
		}
		if (!dstRegs[ins.src2_reg] && !liveIns[ins.src2_reg]) {
			liveIns[ins.src2_reg] = true;
			RegisterFile[ins.src2_reg] = ins.src2_val;
		}
		if (!dstRegs[ins.src3_reg] && !liveIns[ins.src3_reg]) {
			liveIns[ins.src3_reg] = true;
			RegisterFile[ins.src3_reg] = ins.src3_val;
		}

		dstRegs[ins.dst_reg] = true;
	}

	// These are dummy registers and should not be used as live-ins.
	liveIns[INVALID_ARM_REG] = false;

	numberOfLiveIns = 0;
	for (int i = 0; i < INVALID_ARM_REG; i++)
		numberOfLiveIns += (liveIns[i]) ? 1 : 0;
}
#ifdef CONSTRUCT_PSLICE
void Pslice::identifyPsliceLiveIns() {
	// Algorithm: a live-in register is a SRC register that has not been written to (i.e. was not a DST) by a prior instruction
	bool dstRegs[INVALID_ARM_REG + 1];
	for (unsigned int i = 0; i <= INVALID_ARM_REG; i++) {
		dstRegs[i] = false;
		liveIns[i] = false;
	}

	list<ExecIns>::iterator it;
	for (it = pslice.begin(); it != pslice.end(); it++) {
		ExecIns ins = (ExecIns) (*it);

		// NOTE: The current interface assumes that implicit registers are always sources

		if (!dstRegs[ins.impl_reg] && !liveIns[ins.impl_reg]) {
			liveIns[ins.impl_reg] = true;
			RegisterFile[ins.impl_reg] = ins.impl_val;
		}

		if (!dstRegs[ins.src1_reg] && !liveIns[ins.src1_reg]) {
			liveIns[ins.src1_reg] = true;
			RegisterFile[ins.src1_reg] = ins.src1_val;
		}
		if (!dstRegs[ins.src2_reg] && !liveIns[ins.src2_reg]) {
			liveIns[ins.src2_reg] = true;
			RegisterFile[ins.src2_reg] = ins.src2_val;
		}
		if (!dstRegs[ins.src3_reg] && !liveIns[ins.src3_reg]) {
			liveIns[ins.src3_reg] = true;
			RegisterFile[ins.src3_reg] = ins.src3_val;
		}

		dstRegs[ins.dst_reg] = true;
	}

	// These are dummy registers and should not be used as live-ins.
	liveIns[INVALID_ARM_REG] = false;

	numberOfLiveIns = 0;
	for (int i = 0; i < INVALID_ARM_REG; i++)
		numberOfLiveIns += (liveIns[i]) ? 1 : 0;
}
#endif // CONSTRUCT_PSLICE

// Compares two Pslices and determines if they are exactly the same based on the sequence of PCs
// Note that the basic assumption is that both Pslices belong to the same delinquent PC.
bool Pslice::isEqual(Pslice* other) {

	// Quick test
	if(ptrace_set.size() != other->ptrace_set.size())
		return false;

	set<AddrType>::iterator mySetIt;
	set<AddrType>::iterator otherSetIt;

	// Now compare both sets together.
	mySetIt = ptrace_set.begin();
	otherSetIt = other->ptrace_set.begin();

	while (mySetIt != ptrace_set.end()) {

		if ((*mySetIt) != (*otherSetIt))
			return false;

		mySetIt++;
		otherSetIt++;
	}

	return true;
}

bool Pslice::operator==(Pslice& other) {
	list<ExecIns>::iterator myit;
	list<ExecIns>::iterator otherit;

	myit = ptrace.begin();
	otherit = other.ptrace.begin();

	while (myit != ptrace.end()) {

		AddrType myPC = (*myit).PC;
		AddrType otherPC = (*otherit).PC;

		if (myPC != otherPC)
			return false;

		myit++;
		otherit++;
	}

	return true;
}

/*
 bool Pslice::compare(const Pslice* other) {
 list<ExecIns>::iterator myit;
 list<ExecIns>::iterator otherit;

 myit = instructions.begin();
 otherit = other->instructions.begin();

 while (myit != instructions.end()) {

 AddrType myPC = (*myit).PC;
 AddrType otherPC = (*otherit).PC;

 if (myPC != otherPC)
 return false;

 myit++;
 otherit++;
 }

 return true;
 }
 */

// This function is the opposite of operator==. The goal is use this to identify unique Pslices
bool Pslice::operator <(Pslice& other) {
	return !(this->operator ==(other));
}

void Pslice::generatePtraceSignature() {

	signature.clear();
	list<ExecIns>::iterator it;
	it = ptrace.begin();

	// Insert the first instruction's PC
	signature.push_back((*it).PC);

	for (; it != ptrace.end(); it++) {

		// Currently, we do not have a valid interface. The instructions are generated randomly, thus
		// we may end up with a final p-slice load instruction appears as a branch.
		// TODO: REMOVE THIS AFTER THE QMEU/SIMULATOR INTERFACE BECOMES READY
		if ((*it).PC == delPC)
			continue;

		if ((*it).isBranch) {
			if (((*it).isCond)) {
				if ((*it).taken) // Conditional branches have to be taken for the target address to be valid.
					signature.push_back((*it).targetAddr);
				else { // Otherwise, we should use the PC of the next instruction as the signature entry.
					list<ExecIns>::iterator nextInstIt = it;
					nextInstIt++;
					if (nextInstIt == ptrace.end()) {
						fprintf(stderr,
								"OOPS: something went wrong with the signature generation!\n");
					}
					signature.push_back((*nextInstIt).PC);
				}
			} else { // Unconditional branches should have an associated valid target address
				signature.push_back((*it).targetAddr);
			}
		}
	}

	//Insert the delinquent load PC
	signature.push_back(delPC);
}
#ifdef CONSTRUCT_PSLICE
void Pslice::generatePsliceSignature() {

	signature.clear();
	list<ExecIns>::iterator it;
	it = pslice.begin();

	// Insert the first instruction's PC
	signature.push_back((*it).PC);

	AddrType lastPC = (*it).PC;

	for (; it != pslice.end(); it++) {

		// We use a simple address jump detector; if an instructions PC is more than 4 bytes more than
		// the prior instruction, then it's a jump.
		if ((*it).PC > lastPC + 4)
		signature.push_back((*it).PC);
		else {
			if (((*it).isCond)) {
				list<ExecIns>::iterator nextInstIt = it;
				nextInstIt++;
				if (nextInstIt == pslice.end()) {
					fprintf(stderr,
							"OOPS: something went wrong with the signature generation!\n");
				} else {
					signature.push_back((*nextInstIt).PC);
					it = nextInstIt;
				}
			}
		}

		lastPC = (*it).PC;
	}

	// Since some instructions are branches or predicated, we include the subsequent instructions
	// too even if they are 2/4 bytes away.

	/*
	 // Currently, we do not have a valid interface. The instructions are generated randomly, thus
	 // we may end up with a final p-slice load instruction appears as a branch.
	 // TODO: REMOVE THIS AFTER THE QMEU/SIMULATOR INTERFACE BECOMES READY
	 if ((*it).PC == delPC)
	 continue;

	 if ((*it).isBranch) {
	 if (((*it).isCond)) {
	 if ((*it).taken) // Conditional branches have to be taken for the target address to be valid.
	 signature.push_back((*it).targetAddr);
	 else { // Otherwise, we should use the PC of the next instruction as the signature entry.
	 list<ExecIns>::iterator nextInstIt = it;
	 nextInstIt++;
	 if (nextInstIt == pslice.end()) {
	 fprintf(stderr,
	 "OOPS: something went wrong with the signature generation!\n");
	 }
	 signature.push_back((*nextInstIt).PC);
	 }
	 } else { // Unconditional branches should have an associated valid target address
	 signature.push_back((*it).targetAddr);
	 }
	 }
	 */

	//Insert the delinquent load PC
	signature.push_back(delPC);
}

void Pslice::generatePslice() {

// Clear pslice
	pslice.clear();
	loadStores = 0;

	bool depRegisters[INVALID_ARM_REG + 1];
	for (int i = 0; i < INVALID_ARM_REG; i++)
	depRegisters[i] = false;

	list<ExecIns>::reverse_iterator rit;
	rit = ptrace.rbegin();

// For the first instruction (the delinquent load), the source registers are part of
// the dependency set and the instruction is inserted into the shrinked pslice version.
	depRegisters[(*rit).src1_reg] = true;
	depRegisters[(*rit).src2_reg] = true;
	depRegisters[(*rit).src3_reg] = true;
	depRegisters[(*rit).impl_reg] = true;
	ExecIns temp = (*rit);
	pslice.push_back(temp);

	rit++;
	for (; rit != ptrace.rend(); rit++) {
		// For each instruction, if the destination registers are part of the dependency list,
		// then this instruction produces registers which contribute to the prefetch address.
		// Hence, we add these instructions to the pslice, and add any source registers to the
		// dependency list. Otherwise, we can ignore the instruction.
		// We also "conservatively" add all memory instructions.
		if ((depRegisters[(*rit).dst_reg] && (*rit).dst_reg != INVALID_ARM_REG)
				/*|| (*rit).addr != ARM_INVALID_MEMADD*/) {

			if (!(depRegisters[(*rit).dst_reg]
							&& (*rit).dst_reg != INVALID_ARM_REG))
			loadStores++;

			depRegisters[(*rit).dst_reg] = false; // We can now remove the destination register from the dependency list.
			depRegisters[(*rit).src1_reg] = true;// Add all source registers to the dependency list.
			depRegisters[(*rit).src2_reg] = true;
			depRegisters[(*rit).src3_reg] = true;
			depRegisters[(*rit).impl_reg] = true;

			ExecIns temp = (*rit);
			pslice.push_front(temp);

		}
	}
}
#endif // CONSTRUCT_PSLICE

void PsliceMemoryMap::resize(size_t newSize) {
	memap.resize(newSize);
}

void PsliceMemoryMap::insertValue(unsigned int iter, uint32_t addr,
		uint32_t data) {
	if (memap.size() < iter) {
		fprintf(stderr,
				"OOPS: trying INSERT IN a memory of size %u and iteration count %u!\n",
				(unsigned int) memap.size(), iter);
		exit(0);
	}

	memap[iter - 1][addr] = data;
}

// Returns the most recent data value found in the data map at or prior to the given iteration
// number. If no value was found, we return a random value.
uint32_t PsliceMemoryMap::getData(unsigned int iter, uint32_t addr) {
	if (memap.size() < iter) {
		fprintf(stderr,
				"OOPS: trying READ FROM a memory of size %u and iteration count %u!\n",
				(unsigned int) memap.size(), iter);
		exit(0);
	}

// Let's get the most recent value for that address.
	for (int i = iter - 1; i >= 0; i++) {
		if (memap[i].find(addr) != memap[i].end()) {
			return memap[i][addr];
		}
	}

// No value found. We return a random value and display an error.
	fprintf(stderr,
			"OOPS: no VALID value found for addr %u at or before iteration %u. Returning RANDOM value.\n",
			addr, iter);
	return (uint32_t) rand();
}

PsliceMemoryMap::PsliceMemoryMap(size_t iterationCount) {
	resize(iterationCount);
}

unsigned int PsliceMemoryMap::getIterationCount() {
	return (unsigned int) memap.size();
}

