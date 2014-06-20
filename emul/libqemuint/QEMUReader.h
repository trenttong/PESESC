/*
   ESESC: Super ESCalar simulator
   Copyright (C) 2009 University California, Santa Cruz.

   Contributed by Gabriel Southern
                  Jose Renau


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

#ifndef QEMU_READER_H
#define QEMU_READER_H

#include <queue>
#include <unistd.h>

#include "nanassert.h"

#include "Reader.h"
#include "GStats.h"
#include "EmuDInstQueue.h"
#include "DInst.h"
#include "FastQueue.h"

#include "QEMUInterface.h"
#include "ThreadSafeFIFO.h"
#include "CrackBase.h"
#include "ARMCrack.h"
#include "ThumbCrack.h"

#ifdef PSLICE_CONSTRUCT
#include "PsliceConstructor.h"
#endif // PSLICE_CONSTRUCT

class DInst;

class QEMUReader : public Reader {
private:

#ifdef ESESC_QEMU_ISA_ARMEL
  ARMCrack    *crackInstARM;
  ThumbCrack  *crackInstThumb;
  std::vector<CrackBase *>   crackInst;
#endif

  pthread_t         qemu_thread;
  FlowID            numFlows;
  FlowID            numAllFlows;
  bool              started;
  QEMUArgs         *qemuargs;
  EmulInterface    *eint;

#ifdef PSLICE_CONSTRUCT
  PsliceConstructor* psliceConstructor;
#endif // PSLICE_CONSTRUCT

public:
  QEMUReader(QEMUArgs *qargs, const char *section, EmulInterface *eint);
  virtual ~QEMUReader();

  DInst *executeHead(FlowID  fid);
  void  reexecuteTail(FlowID fid);
  void  syncHeadTail(FlowID  fid);

  // Only method called by remote thread
  void queueInstruction(uint32_t insn, AddrType pc, AddrType addr, DataType data, char thumb, FlowID fid, void *env, bool keepStats = false);

  // ATTA: an overloaded version which receives more information about the instruction
  void queueInstruction(uint32_t insn, AddrType pc, AddrType addr, DataType data, char thumb, FlowID fid, void *env, bool keepStats,
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
  	  	  	  	  	  );

  void syscall(uint32_t num, Time_t time, FlowID fid);
  
  void start();

  // Whenever we have a change in statistics (mode), we should drain the queue
  // as much as possible
  void drainFIFO(FlowID fid);
  uint32_t wait_until_FIFO_full(FlowID fid);
};

#endif
