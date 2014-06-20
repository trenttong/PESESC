/*
   ESESC: Super ESCalar simulator
   Copyright (C) 2006 University California, Santa Cruz.

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

#ifndef QEMU_INTERFACE_H
#define QEMU_INTERFACE_H

#include <stdint.h>

#include "nanassert.h"

class EmuSampler;

extern EmuSampler *qsamplerlist[];
extern EmuSampler *qsampler;

extern "C" {
  typedef struct QEMUArgs {
    int qargc;
    char **qargv;
  } QEMUArgs;

  void *qemuesesc_main_bootstrap(void *threadargs);
  void QEMUReader_goto_sleep(void *env);
  void QEMUReader_wakeup_from_sleep(void *env);

  void QEMUReader_queue_inst(uint32_t insn,
                             uint32_t pc,
                             uint32_t addr,
                             uint32_t data,
                             uint32_t fid,
                             char op,       // opcode 0 other, 1 LD, 2 ST, 3 BR
                             uint64_t icount,   // #inst >1 while in rabbit or warmup
                             void *env,    // CPU env
                             uint32_t  pcore,	// determines if instruction is received from Pcore or Mcore
                             // ATTA: additional arguments required to reexecute instructions.
                             // Instruction broken down into its components
                             uint32_t rdn,   /// RD num (src1_reg)
                             uint32_t rnn,   /// RN num (src2_reg)
                             uint32_t rmn,   /// RM num (src3_reg)
                             uint32_t rsn,   /// RS num (dst_reg)
                             uint32_t dfn,   /// default num (impl_reg)
                             uint32_t rdv,   /// RD value (src1_val)
                             uint32_t rnv,   /// RN value (src2_val)
                             uint32_t rmv,   /// RM value (src3_val)
                             uint32_t rsv,   /// RS value (dst_val)
                             uint32_t dfv,   /// default value (impl_val)
                             // Branch information
                             uint32_t branch,/// is branch
                             uint32_t cond,  /// is conditional
                             uint32_t taken, /// is taken 
                             uint32_t target);/// target of branch


  void QEMUReader_finish(uint32_t fid);
  void QEMUReader_finish_thread(uint32_t fid);

  void esesc_set_rabbit(uint32_t fid);
  void esesc_set_warmup(uint32_t fid);
  void esesc_set_timing(uint32_t fid);
  void loadSampler(uint32_t fid);
  int QEMUReader_is_sampler_done(uint32_t fid);
}

#endif
