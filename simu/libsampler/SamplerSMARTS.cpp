/*
   ESESC: Super ESCalar simulator
   Copyright (C) 2010 University of California, Santa Cruz.

   Contributed by Jose Renau
                  Ehsan K.Ardestani


This file is part of ESESC.

ESESC is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

ESESC is distributed in the  hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
ESESC; see the file COPYING. If not, write to the Free Software Foundation, 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#include "SamplerSMARTS.h"
#include "EmulInterface.h"
#include "SescConf.h"
#include "BootLoader.h"
#include "TaskHandler.h"
#include "MemObj.h"
#include "GProcessor.h"
#include "GMemorySystem.h"
uint64_t cuda_inst_skip = 0;


SamplerSMARTS::SamplerSMARTS(const char *iname, const char *section, EmulInterface *emu, FlowID fid)
  : SamplerBase(iname, section, emu, fid)
  /* SamplerSMARTS constructor {{{1 */
{
  nInstRabbit = static_cast<uint64_t>(SescConf->getDouble(section,"nInstRabbit"));
  nInstWarmup = static_cast<uint64_t>(SescConf->getDouble(section,"nInstWarmup"));
  nInstDetail = static_cast<uint64_t>(SescConf->getDouble(section,"nInstDetail"));
  nInstTiming = static_cast<uint64_t>(SescConf->getDouble(section,"nInstTiming"));

  if (fid != 0){ // first thread might need a different skip
    nInstSkip   = static_cast<uint64_t>(SescConf->getDouble(section,"nInstSkipThreads"));
    cuda_inst_skip = nInstSkip;
  }
  nInstMax    = static_cast<uint64_t>(SescConf->getDouble(section,"nInstMax"));

  finished[fid] = true; // will be set to false in resumeThread
  finished[0] = false;

  if (nInstWarmup>0) {
    sequence_mode.push_back(EmuWarmup);
    sequence_size.push_back(nInstWarmup);
    next2EmuTiming = EmuWarmup;
  }
  if (nInstDetail>0) {
    sequence_mode.push_back(EmuDetail);
    sequence_size.push_back(nInstDetail);
    next2EmuTiming = EmuDetail;
  }
  if (nInstTiming>0) {
    sequence_mode.push_back(EmuTiming);
    sequence_size.push_back(nInstTiming);
    next2EmuTiming = EmuTiming;
  }

  // Rabbit last because we start with nInstSkip
  if (nInstRabbit>0) {
    sequence_mode.push_back(EmuRabbit);
    sequence_size.push_back(nInstRabbit);
    next2EmuTiming = EmuRabbit;
  }

  if (sequence_mode.empty()) {
    MSG("ERROR: SamplerSMARTS needs at least one valid interval");
    exit(-2);
  }

  sequence_pos = 0;

  nInstForcedDetail = nInstDetail==0? nInstTiming/2:nInstDetail;

  if (emul->cputype != GPU) {
    GProcessor *gproc = TaskHandler::getSimu(fid);
    MemObj *mobj      =  gproc->getMemorySystem()->getDL1();
    DL1 = mobj;
  }

  nextSwitch = nInstSkip;
  if (nInstSkip)
    startRabbit(fid);

  cpiHistSize = static_cast<uint32_t>(SescConf->getDouble(section, "PowPredictionHist")); 
  cpiHist.resize(cpiHistSize);

  lastMode = EmuInit;
  printf("Sampler: SMARTS, R:%lu, W:%lu, D:%lu, T:%lu\n", nInstRabbit, nInstWarmup, nInstDetail, nInstTiming);
  printf("Sampler: SMARTS, nInstMax:%lu, nInstSkip:%lu\n", nInstMax, nInstSkip);

}
/* }}} */

SamplerSMARTS::~SamplerSMARTS() 
  /* DestructorRabbit {{{1 */
{
  // Free name, but who cares 
}
/* }}} */

void SamplerSMARTS::queue(uint32_t insn, uint64_t pc, uint64_t addr, uint64_t data, FlowID fid, char op, uint64_t icount, void *env)
  /* main qemu/gpu/tracer/... entry point {{{1 */
{

  I(fid < emul->getNumEmuls());
  if(likely(!execute(fid, icount)))
    return; // QEMU can still send a few additional instructions (emul should stop soon)
  I(mode!=EmuInit);

  I(insn);

  // process the current sample mode
  if (nextSwitch>totalnInst) {

    if (mode == EmuRabbit || mode == EmuInit)
      return;

    if (mode == EmuDetail || mode == EmuTiming) {
      emul->queueInstruction(insn,pc,addr,data, (op&0xc0) /* thumb */ ,fid, env, getStatsFlag());
      return;
    }

    I(mode == EmuWarmup);
    if(addr) {
      I (emul->cputype != GPU);
      if ( (op&0x3F) == 1)
        DL1->ffread(addr,data);
      else if ( (op&0x3F) == 2)
        DL1->ffwrite(addr,data);
    }
    return;
  }


  // We did enough
  if (totalnInst >= nInstMax || endSimSiged) {
    markThisDone(fid);
    
    if (allDone())
      markDone();
    else {
      keepStats = false;
    }
    return;
  }

  // Look for the new mode
  I(nextSwitch <= totalnInst);


 // I(mode != next_mode);
  pthread_mutex_lock (&mode_lock);
  //

  if (nextSwitch > totalnInst){//another thread just changed the mode
    pthread_mutex_unlock (&mode_lock);
    return;
  }

  lastMode = mode;
  nextMode(ROTATE, fid);
  if (doPower && fid == winnerFid) {
    if (lastMode == EmuTiming) { // timing is going to be over
      uint64_t mytime          = getLocalTime();
      int64_t ti = mytime - lastTime;
      I(ti > 0);
      ti = freq*ti/1e9;

      std::cout<<"mode "<<lastMode<<" Timeinterval "<<ti<<" mytime "<<mytime<<" last time "<<lastTime<<"\n";  
      BootLoader::getPowerModelPtr()->setSamplingRatio(getSamplingRatio()); 
      int32_t simt = BootLoader::getPowerModelPtr()->calcStats(ti, !(lastMode == EmuTiming), fid); 
      lastTime = mytime;
      updateCPI(fid); 
      endSimSiged = (simt==90)?1:0;
      if (doTherm)
        BootLoader::getPowerModelPtr()->sescThermWrapper->sesctherm.updateMetrics(ti);  
    }
  }
  pthread_mutex_unlock (&mode_lock);

}
/* }}} */



void SamplerSMARTS::updateCPI(FlowID fid){
  //extract cpi of last sample interval 
 
  localTicksUptodate = false;
  estCPI = getMeaCPI();
  return; 

}


void SamplerSMARTS::nextMode(bool rotate, FlowID fid, EmuMode mod){

  if (rotate){

    fetchNextMode();
    I(next_mode != EmuInit);

    setMode(next_mode, fid);
    I(mode == next_mode);
    if (next_mode == EmuRabbit){
      setModeNativeRabbit();
    }
    nextSwitch       = nextSwitch + sequence_size[sequence_pos];
  }else{
    I(0);
  }
}
  

bool SamplerSMARTS::allDone() {
  for (size_t i=0; i< emul->getNumFlows(); i++) {
    if (!finished[i])
      return false;
  }
  return true;
}

void SamplerSMARTS::markThisDone(FlowID fid) {
  if (!finished[fid]) {
    finished[fid] = true;
    printf("fid %d finished, waiting for the rest...\n", fid);
  }
}
