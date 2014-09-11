/*
 * Pslice.h
 *
 *  Created on: 2013-11-12
 *      Author: iatta
 *      This class stores all the information relevant to a p-slice.
 */

#ifndef PSLICE_H_
#define PSLICE_H_

#include <list>
#include <stdio.h>
#include <string>
#include <map>
#include "RAWDInst.h"

using namespace std;

#define PSLICE_MAX_SIZE 300
//#define CONSTRUCT_PSLICE

class Pslice {
public:
	struct ExecIns {
		AddrType PC;
		uint32_t opcode;
		AddrType addr;
		AddrType data;
		ARMReg src1_reg, src2_reg, src3_reg, dst_reg, impl_reg;
		uint32_t src1_val, src2_val, src3_val, dst_val, impl_val;
		bool isBranch, isCond, taken;
		AddrType targetAddr;

		void operator+(ExecIns other) {
			PC = other.PC;
			opcode = other.opcode;
			addr = other.addr;
			data = other.data;
			src1_reg = other.src1_reg;
			src2_reg = other.src2_reg;
			src3_reg = other.src3_reg;
			dst_reg = other.dst_reg;
			impl_reg = other.impl_reg;
			src1_val = other.src1_val;
			src2_val = other.src2_val;
			src3_val = other.src3_val;
			dst_val = other.dst_val;
			impl_val = other.impl_val;
			isBranch = other.isBranch;
			isCond = other.isCond;
			taken = other.taken;
			targetAddr = other.targetAddr;
		}
	};

	Pslice(AddrType pc, int iter);

	const AddrType delPC;
	int iterationNumber;
	unsigned int frequency;
	list<ExecIns> ptrace;	// A list of all instructions that are part of the original program execution
#ifdef CONSTRUCT_PSLICE
	list<ExecIns> pslice;	// A slimmed down version of the main thread which includes only instructions which contribute to the generation of the prefetch address.
#endif // CONSTRUCT_PSLICE
	unsigned int loadStores;	// A counter for the instructions included in the pslice only because of being a load/store
	list<AddrType> signature;

	uint32_t RegisterFile[INVALID_ARM_REG+1]; // A snapshot of the register file
	bool liveIns[INVALID_ARM_REG+1];
	uint32_t numberOfLiveIns;

	void insertInst(AddrType pc, uint32_t insn, AddrType addr, AddrType data,
			ARMReg src1_reg, ARMReg src2_reg, ARMReg src3_reg, ARMReg impl_reg,
			ARMReg dst_reg, uint32_t src1_val, uint32_t src2_val,
			uint32_t src3_val, uint32_t dst_val, uint32_t impl_val,
			bool isBranch, bool isCond, bool taken, AddrType targetAddr);

	size_t size() {
		return ptrace.size();
	}

	void clear() {
		ptrace.clear();
	}

	bool dumpPtrace();
#ifdef CONSTRUCT_PSLICE
	bool dumpPslice();
#endif // CONSTRUCT_PSLICE

	void dumpToFile();

	void identifyPtraceLiveIns();
#ifdef CONSTRUCT_PSLICE
	void identifyPsliceLiveIns();
#endif // CONSTRUCT_PSLICE

	bool isEqual(Pslice* other);
	bool operator==(Pslice& other);
	bool operator<(Pslice& other);

	//bool compare(const Pslice* other);

	void generatePtraceSignature(); // Computes the signature of the ptrace
#ifdef CONSTRUCT_PSLICE
	void generatePsliceSignature(); // Computes the signature of this p-slice
	void generatePslice();	// Constructs a shrinked version of the p-slice which includes only instructions that affect the prefetch address.
#endif // CONSTRUCT_PSLICE
};

// For a specific delinquent load, all p-slices are examined to extract a global memory map.
// This map contains all the address/data pairs captured by all p-slices. Each valus is also
// tagged by the iteration number at which that value was captured.
class PsliceMemoryMap {
	vector<map<AddrType, DataType> > memap;

	PsliceMemoryMap(size_t iterationCount);
	void insertValue(unsigned int iter, uint32_t addr, uint32_t data);
	uint32_t getData(unsigned int iter, uint32_t addr);
	void resize(size_t newSize);
	unsigned int getIterationCount();
};

#endif /* PSLICE_H_ */
