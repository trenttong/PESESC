/*
   ESESC: Enhanced Super ESCalar simulator
   Copyright (C) 2009 University of California, Santa Cruz.

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
#ifndef QEMUEMULINTERFACE_H
#define QEMUEMULINTERFACE_H

#include <map>

#include "nanassert.h"
#include "EmulInterface.h"
#include "QEMUReader.h"


#define FID_NOT_AVAILABLE 0
#define FID_FREE     1
#define FID_FREED    3
#define FID_TAKEN         2
#define FID_NULL          std::numeric_limits<uint32_t>::max ()


class QEMUEmulInterface : public EmulInterface {
 private:
  FlowID      nFlows;
  FlowID      nEmuls;
  QEMUReader *reader;

 protected:
  static std::vector<FlowID> fidFreePool; 
  static std::map<FlowID, FlowID> fidMap; 
 public:
  QEMUEmulInterface(const char *section);
  ~QEMUEmulInterface();

  DInst  *executeHead(FlowID   fid);
  void    reexecuteTail(FlowID fid);
  void    syncHeadTail(FlowID  fid);

  FlowID  getNumFlows(void) const;
  FlowID  getNumEmuls(void) const;

  FlowID  mapGlobalID(FlowID gid) const{
    return gid;
  }

  void start() {
    reader->start();
  }

  void queueInstruction(uint32_t insn, AddrType pc, AddrType addr, DataType data, char thumb, FlowID fid, void *env, bool inEmuTiming) {
    reader->queueInstruction(insn,pc,addr,data, thumb, fid, env, inEmuTiming);
  }

  // ATTA: an overloaded version which propagates more information about the instruction
  void queueInstruction(uint32_t insn, AddrType pc, AddrType addr, DataType data, char thumb, FlowID fid, void *env, bool inEmuTiming,
						uint32_t src1_reg,   /// RD num (src1_reg)
						uint32_t src2_reg,   /// RN num (src2_reg)
						uint32_t src3_reg,   /// RM num (src3_reg)
						uint32_t dst_reg,   /// RS num (dst_reg)
						uint32_t impl_reg,   /// default num (impl_reg)
						uint32_t src1_val,   /// RD value (src1_val)
						uint32_t src2_val,   /// RN value (src2_val)
						uint32_t src3_val,   /// RM value (src3_val)
						uint32_t dst_val,   /// RS value (dst_val)
						uint32_t impl_val,   /// default value (impl_val)
						uint32_t branch,/// is branch
						uint32_t cond,  /// is conditional
						uint32_t taken, /// is taken
						uint32_t target	/// target of branch
		  				) {
      reader->queueInstruction(insn,pc,addr,data, thumb, fid, env, inEmuTiming
							, src1_reg, src2_reg, src3_reg, dst_reg, impl_reg
							, src1_val, src2_val, src3_val, dst_val, impl_val
							, branch, cond, taken, target);
  }

#ifdef ENABLE_CUDA
  uint32_t getKernelId() { I(0); };
#endif

  void syscall(uint32_t num, Time_t time, FlowID fid) {
    reader->syscall(num, time, fid);
  }
  
  void startRabbit(FlowID fid);
  void startWarmup(FlowID fid);
  void startDetail(FlowID fid);
  void startTiming(FlowID fid);

  FlowID getFirstFlow() const; 
  FlowID getFid(FlowID last_fid);
  void freeFid(FlowID fid);
  void setFirstFlow(FlowID fid);
  
  void setSampler(EmuSampler *a_sampler, FlowID fid=0);
  void drainFIFO();
  FlowID mapLid(FlowID fid);

};

#endif // QEMUEMULINTERFACE_H
