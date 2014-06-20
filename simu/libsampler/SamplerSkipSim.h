/*
   ESESC: Super ESCalar simulator
   Copyright (C) 2010 University California, Santa Cruz.

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

#ifndef SAMPLER_SKIP_SIM_H
#define SAMPLER_SKIP_SIM_H

#include "SamplerBase.h"

class SamplerSkipSim : public SamplerBase {
private:
protected:

public:
  SamplerSkipSim(const char *name, const char *section, EmulInterface *emul, FlowID fid);
  virtual ~SamplerSkipSim();

  void queue(uint32_t insn, uint64_t pc, uint64_t addr, uint64_t data, uint32_t fid, char op, uint64_t icount, void *env);

  // ATTA: an overloaded version which propagates more information about the instruction
  void queue(uint32_t insn, uint64_t pc, uint64_t addr, uint64_t data, uint32_t fid, char op, uint64_t icount, void *env,
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
				uint32_t branch,/// is branch
				uint32_t cond,  /// is conditional
				uint32_t taken, /// is taken
				uint32_t target	/// target of branch
  	  	  	 );

  float getSamplingRatio() {return 1.0;};

};
#endif
