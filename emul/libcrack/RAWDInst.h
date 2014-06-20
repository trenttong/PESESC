/*
   ESESC: Enhanced Super ESCalar simulator
   Copyright (C) 2010 University of California, Santa Cruz.

   Contributed by Jose Renau

This file is part of ESESC.

ESESC is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

ESESC is    distributed in the  hope that  it will  be  useful, but  WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should  have received a copy of  the GNU General  Public License along with
ESESC; see the file COPYING.  If not, write to the  Free Software Foundation, 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef RAWDINST_H
#define RAWDINST_H

#include <vector>

#ifdef SCOORE
#include "scuop.h"
typedef Scuop UOPPredecType;

#else
#include "Instruction.h"

typedef Instruction UOPPredecType;
#endif

enum CUDAMemType{
  RegisterMem = 0,
  GlobalMem,
  ParamMem,
  LocalMem,
  SharedMem,
  ConstantMem,
  TextureMem
};

// ATTA: Added temporarily until we settle on a clearly defined enum for register numbers.
enum ARMReg{
	R0 = 0,
	R1,
	R2,
	R3,
	R4,
	R5,
	R6,
	R7,
	R8,
	R9,
	R10,
	R11,
	R12,
	R13,
	R14,
	R15,
	INVALID_ARM_REG = 16
};

#define ARM_INVALID_MEMADD ((unsigned) -1)

typedef uint64_t DataType;
typedef uint64_t AddrType;
typedef uint32_t FlowID; 

#include "nanassert.h"

typedef uint32_t RAWInstType;

class RAWDInst {
private:
  RAWInstType insn;
  AddrType    pc;
#ifdef SCOORE
  bool isClear;
#else
  AddrType    addr;
  DataType    data;
#endif
#ifdef ENABLE_CUDA
  CUDAMemType memaccess;
#endif

  bool keepStats;
  bool inITBlock;
  float L1clkRatio;
  float L3clkRatio;

protected:
  uint32_t ninst;
  std::vector<UOPPredecType> predec;
public:
#ifdef ENABLE_CUDA
  void setMemaccesstype(CUDAMemType type){
    memaccess = type;
  }

  CUDAMemType getMemaccesstype(void){
    return memaccess;
  }
#endif

  RAWDInst(const RAWDInst &p) {
    insn = p.insn;
    pc   = p.pc;
#ifdef SCOORE
    isClear = p.isClear;
#else
    addr = p.addr;
    data = p.data;
#endif
    ninst = p.ninst;
    predec = p.predec;
    keepStats = false;
  }

  RAWDInst() {
    IS(clear());
  }

#ifdef SCOORE
  void set(RAWInstType _insn, AddrType _pc, bool _keepStats = false) {
    clearInst();
    insn = _insn;
    pc   = _pc;
    isClear = false;
    keepStats = _keepStats;
  }
#else
  void set(RAWInstType _insn, AddrType _pc, AddrType _addr, DataType _data, float _L1clkRatio = 1.0, float _L3clkRatio = 1.0, bool _keepStats = false) {
    clearInst();
    L3clkRatio = _L3clkRatio;
 #ifdef ENABLE_CUDA
    if ((addr >> 61) == 6)
    {
      memaccess   = SharedMem;
    } else {
      memaccess   = GlobalMem;
    }
#endif
    insn = _insn;
    pc   = _pc;
    addr = _addr;
    data = _data;
    keepStats = _keepStats;
    L1clkRatio = _L1clkRatio;
  }

  //ATTA: more information about the instruction is addred to RAWDInst
  ARMReg src1_reg, src2_reg, src3_reg, dst_reg, impl_reg;
  uint32_t src1_val, src2_val, src3_val, dst_val, impl_val;
  bool isBranch, isCond, taken;
  AddrType targetAddr;

  void set(RAWInstType _insn, AddrType _pc, AddrType _addr, DataType _data, float _L1clkRatio, float _L3clkRatio, bool _keepStats,
			uint32_t _src1_reg,   /// RD num (src1_reg)
			uint32_t _src2_reg,   /// RN num (src2_reg)
			uint32_t _src3_reg,   /// RM num (src3_reg)
			uint32_t _dst_reg,   /// RS num (dst_reg)
			uint32_t _impl_reg,   /// default num (impl_reg)
			uint32_t _src1_val,   /// RD value (src1_val)
			uint32_t _src2_val,   /// RN value (src2_val)
			uint32_t _src3_val,   /// RM value (src3_val)
			uint32_t _dst_val,   /// RS value (dst_val)
			uint32_t _impl_val,   /// default value (impl_val)
			uint32_t _branch,/// is branch
			uint32_t _cond,  /// is conditional
			uint32_t _taken, /// is taken
			uint32_t _target	/// target of branch
			) {
     set(_insn, _pc, _addr, _data, _L1clkRatio, _L3clkRatio, _keepStats);
     src1_reg = (ARMReg) _src1_reg;
     src2_reg = (ARMReg) _src2_reg;
     src3_reg = (ARMReg) _src3_reg;
     dst_reg = (ARMReg) _dst_reg;
     impl_reg = (ARMReg) _impl_reg;
     src1_val = _src1_val;
     src2_val = _src2_val;
     src3_val = _src3_val;
     dst_val = _dst_val;
     impl_val = _impl_val;
     isBranch = _branch;
     isCond = _cond;
     taken = _taken;
     targetAddr = _target;
   }
#endif


  void clearInst() {
    ninst = 0;
    // predec.reserve(16);
#ifdef ENABLE_CUDA
    memaccess   = GlobalMem;
#endif


  }

  UOPPredecType *getNewInst() {
    ninst++;
    if (ninst >= predec.size())
      predec.resize(2*ninst);
    return &(predec[ninst-1]);
  }

#ifdef SCOORE
  bool isEqual(uint32_t _pc) { 
    if ((isClear == false) && (pc == _pc))
      return true;

    return false; 
  }
  bool isITBlock() { 
    return (((insn & 0x0000BF00) == 0x0000BF00)? true : false);
  }
#endif

  size_t getNumInst() const { return ninst; }
  const UOPPredecType *getInstRef(size_t id) const { return &predec[id]; }

  void clear() {
    insn = 0;
    pc   = 0;
#ifndef SCOORE
    addr = 0;
    data = 0;
#else
    isClear = true;
#endif
  }

  AddrType    getPC()   const { return pc;   };
  RAWInstType getInsn() const { return insn; };
#ifdef SCOORE
  AddrType    getAddr() const { I(0); return 0; };
  DataType    getData() const { I(0); return 0; };
#else
  AddrType    getAddr() const { return addr; };
  DataType    getData() const { return data; };
#endif
  bool getStatsFlag(){
    return keepStats;
  };
  bool isInITBlock() {
    return inITBlock;
  };
  void setInITBlock(bool _flag) {
    inITBlock = _flag;
  };
  float getL1clkRatio() { return L1clkRatio; };
  float getL3clkRatio() { return L3clkRatio; };
};

#endif

