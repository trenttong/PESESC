/*
   ESESC: Super ESCalar simulator
   Copyright (C) 2010 University California, Santa Cruz.

   Contributed by Jose Renau
                  Ehsan K.Ardestani


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


#include "SamplerSkipSim.h"
#include "EmulInterface.h"
#include "BootLoader.h"
#include "SescConf.h"
#include <set>

SamplerSkipSim::SamplerSkipSim(const char *iname, const char *section, EmulInterface *emu, FlowID fid)
  : SamplerBase(iname, section, emu)
  /* SamplerSkipSim constructor {{{1 */
{
  nInstSkip = static_cast<uint64_t>(SescConf->getDouble(section,"nInstSkip"));
  nInstMax  = static_cast<uint64_t>(SescConf->getDouble(section,"nInstMax"));
  
  if (fid != 0) // first thread might need a different skip
    nInstSkip   = static_cast<uint64_t>(SescConf->getDouble(section,"nInstSkipThreads"));
  
  if (nInstSkip)
    startRabbit(fid);
  else
    startTiming(fid);

}
/* }}} */

SamplerSkipSim::~SamplerSkipSim() 
  /* DestructorRabbit {{{1 */
{
  // Free name, but who cares 
}
/* }}} */

// ATTA: an overloaded version which propagates more information about the instruction
void SamplerSkipSim::queue(uint32_t insn, uint64_t pc, uint64_t addr, uint64_t data, uint32_t fid, char op, uint64_t icount, void *env,
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
  	  	  	 )
{
  if (TaskHandler::isTerminated())
	return;

  //ATTA: added the fid==0 condition to avoid counting Pcore instructions.
  if(fid == 0)
	  if(!execute(fid,icount))
	     return; // QEMU can still send a few additional instructions (emul should stop soon)

  I(mode!=EmuInit);
  I(insn!=0);
  I(icount!=0);

  if (nInstMax <= totalnInst || endSimSiged) {
	markDone();
	return;
  }

  if (nInstSkip>totalnInst) {		// ATTA: changed >= to > to avoid inappropriate assert when nInstSkip = 0
	I(mode==EmuRabbit);
	//pthread_mutex_unlock (&mode_lock);
	return;
  }

  static bool first = true;
  if (first) {
	lastTime = getLocalTime();
	first    = false;
  }

  I(nInstSkip<=totalnInst);
  if (mode == EmuRabbit) {
	stop();
	startTiming(fid);
  }

  emul->queueInstruction(insn,pc,addr,data, op&0xc0 /*thumb*/, fid, env, getStatsFlag()
		  	  , src1_reg, src2_reg, src3_reg, dst_reg, impl_reg
		  	  , src1_val, src2_val, src3_val, dst_val, impl_val
		  	  , branch, cond, taken, target);

  if (!doPower)
	return;

  pthread_mutex_lock (&mode_lock); //*************** BEGIN ATOMIC

  uint64_t ti = 0;
  bool callpwr = callPowerModel(ti, fid);

  if (callpwr) {
	I(ti > 0);

	int32_t simt = 0;
	//setMode(EmuTiming, fid);  //Spikes?
	TaskHandler::syncStats();
	simt =  BootLoader::getPowerModelPtr()->calcStats(ti, !(mode == EmuTiming), fid);

	endSimSiged = (simt==90)?1:0;
  }

  pthread_mutex_unlock (&mode_lock); //*************** END ATOMIC
}

void SamplerSkipSim::queue(uint32_t insn, uint64_t pc, uint64_t addr, uint64_t data, uint32_t fid, char op, uint64_t icount, void *env)
  /* main qemu/gpu/tracer/... entry point {{{1 */
{
  if (TaskHandler::isTerminated())
    return;

  if(!execute(fid,icount))
    return; // QEMU can still send a few additional instructions (emul should stop soon)
  
  I(mode!=EmuInit);
  I(insn!=0);
  I(icount!=0);
  
  if (nInstMax <= totalnInst || endSimSiged) {
    markDone();
    return;
  }

  if (nInstSkip>totalnInst) {		// ATTA: changed >= to > to avoid inappropriate assert when nInstSkip = 0
    I(mode==EmuRabbit);
    //pthread_mutex_unlock (&mode_lock);
    return;
  }

  static bool first = true;
  if (first) {
    lastTime = getLocalTime();
    first    = false;
  }

  I(nInstSkip<=totalnInst);
  if (mode == EmuRabbit) {
    stop();
    startTiming(fid);
  }

  emul->queueInstruction(insn,pc,addr,data, op&0xc0 /*thumb*/, fid, env, getStatsFlag());

  if (!doPower) 
    return;
 
  pthread_mutex_lock (&mode_lock); //*************** BEGIN ATOMIC

  uint64_t ti = 0;
  bool callpwr = callPowerModel(ti, fid);

  if (callpwr) { 
    I(ti > 0);

    int32_t simt = 0;
    //setMode(EmuTiming, fid);  //Spikes?
    TaskHandler::syncStats();
    simt =  BootLoader::getPowerModelPtr()->calcStats(ti, !(mode == EmuTiming), fid);

    endSimSiged = (simt==90)?1:0;
  }

  pthread_mutex_unlock (&mode_lock); //*************** END ATOMIC
}
/* }}} */


