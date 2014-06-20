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

#include <stdlib.h>
#include <stdio.h>

#include "EmulInterface.h"
#include "EmuSampler.h"
#include "SescConf.h"

bool cuda_go_ahead = false;
std        :: vector<bool> EmuSampler :: done;
int32_t  EmuSampler :: inTiming[];
uint64_t EmuSampler :: nSamples[];
uint64_t EmuSampler :: totalnSamples                = 0;
bool     EmuSampler :: justUpdatedtotalnSamples     = false;
uint64_t *EmuSampler :: instPrev;
uint64_t *EmuSampler :: clockPrev;
uint64_t *EmuSampler :: fticksPrev;
uint64_t __thread EmuSampler::local_icount=0;

float EmuSampler::turboRatio = 1.0;
#ifdef ENABLE_CUDA
float EmuSampler::turboRatioGPU = 1.0;
#endif
 
EmuSampler::EmuSampler(const char *iname, EmulInterface *emu, FlowID fid)
  /* EmuSampler constructor {{{1 */
  : name(strdup(iname))
    ,sFid(fid)
    ,emul(emu)
{
  clock_gettime(CLOCK_REALTIME,&startTime);

  const char *sys_sec = SescConf->getCharPtr(emu->getSection(),"syscall");
  syscall_enable      = SescConf->getBool(sys_sec,"enable");
  syscall_generate    = SescConf->getBool(sys_sec,"generate");
  syscall_runtime     = SescConf->getBool(sys_sec,"runtime");
  if (!syscall_runtime && syscall_enable) {
    const char *fname = SescConf->getCharPtr(sys_sec,"file");
    if (syscall_generate)
      syscall_file      = fopen(fname,"w");
    else
      syscall_file      = fopen(fname,"r");
    if (syscall_file == 0) {
      MSG("ERROR: syscall could not open file %s",fname);
      exit(-3);
    }
  }else
    syscall_file = 0;

  mode  = EmuInit; 

  phasenInst = 0;
  totalnInst = 0;
  meaCPI    = 1.0;

  globalClock_Timing_prev = 0;

  static int sampler_count = 0;

  tusage[EmuInit  ] = new GStatsCntr("S(%d):InitTime"  ,sampler_count);
  tusage[EmuRabbit] = new GStatsCntr("S(%d):RabbitTime",sampler_count);
  tusage[EmuWarmup] = new GStatsCntr("S(%d):WarmupTime",sampler_count);
  tusage[EmuDetail] = new GStatsCntr("S(%d):DetailTime",sampler_count);
  tusage[EmuTiming] = new GStatsCntr("S(%d):TimingTime",sampler_count);

  iusage[EmuInit  ] = new GStatsCntr("S(%d):InitInst"  ,sampler_count);
  iusage[EmuRabbit] = new GStatsCntr("S(%d):RabbitInst",sampler_count);
  iusage[EmuWarmup] = new GStatsCntr("S(%d):WarmupInst",sampler_count);
  iusage[EmuDetail] = new GStatsCntr("S(%d):DetailInst",sampler_count);
  iusage[EmuTiming] = new GStatsCntr("S(%d):TimingInst",sampler_count);

  globalClock_Timing = new GStatsCntr("S(%d):globalClock_Timing", sampler_count);

  ipc  = new GStatsAvg("P(%d)_ipc", sampler_count);
  uipc = new GStatsAvg("P(%d)_uipc", sampler_count);
  char str[100];
  sprintf(str,"P(%d):nCommitted",fid);
  nCommitted = GStats::getRef(str);
  I(nCommitted);
  prevnCommitted = 0;

  sampler_count++;

  for(int i=0;i<static_cast<int>(EmuMax);i++) {
    
    tusage[i]->setIgnoreSampler();
    iusage[i]->setIgnoreSampler();
  }

  freq  = SescConf->getDouble("technology","frequency");

  stopJustCalled = true; // implicit stop at the beginning
  calcCPIJustCalled = false;

  local_icount = 0;

  next = 1024*1024+7;

  pthread_mutex_init(&stop_lock, NULL);
  numFlow = SescConf->getRecordSize("","cpuemul");
  if (done.size() != numFlow)
    done.resize(numFlow);
  for(size_t i=1; i< done.size();i++) {
    //done[i] = true;
    clearInTiming(i);
  }

  keepStats = true;
  //keepStats = false;

  nSamples[fid] = totalnSamples; // Local sample counter get the value of global counter in the beginning

  instPrev = new uint64_t[emul->getNumEmuls()]; 
  clockPrev = new uint64_t[emul->getNumEmuls()]; 
  fticksPrev = new uint64_t[emul->getNumEmuls()]; 
  for (size_t i=0; i<emul->getNumEmuls();i++) {
   instPrev[i] = 0;  
   clockPrev[i] = 0;
   fticksPrev[i] = 0;
  }
}
/* }}} */

EmuSampler::~EmuSampler() 
  /* Destructor {{{1 */
{
  if (syscall_file) {
    fclose(syscall_file);
    syscall_file = 0;
  }
}
/* }}} */

void EmuSampler::setMode(EmuMode mod, FlowID fid)
  /* Stop and start statistics for a given mode {{{1 */
{
  //printf("Thread: %u, set to mode: %u\n", fid, mod);
  stop();
  switch(mod) {
    case EmuRabbit:
      startRabbit(fid);
      break;
    case EmuWarmup:
      startWarmup(fid);
      break;
    case EmuDetail:
      startDetail(fid);
      break;
    case EmuTiming:
      startTiming(fid);
      break;
    default:
      I(0);
  }
}
/* }}} */

  FlowID EmuSampler::getNumFlows(){
    return emul->getNumFlows();
  }

//  FlowID EmuSampler::getFirstFlow(){
//    return emul->getFirstFlow();
//  }

void EmuSampler::beginTiming(EmuMode mod)
  /* Start sampling a new mode {{{1 */
{
  I(stopJustCalled);
  stopJustCalled = false;
  phasenInst         = 0;
  mode = mod;

  clock_gettime(CLOCK_REALTIME,&startTime);

  const char *mode2name[] = { "Init","Rabbit","Warmup","Detail","Timing","InvalidValue"};
  GMSG(mode!=mod,"Sampler %s: starting %s mode", name, mode2name[mod]);
}
/* }}} */

/* }}} */
void EmuSampler::stop()
  /* stop a given mode, and assign statistics {{{1 */
{ 
  if (totalnInst >= cuda_inst_skip)
  {
    cuda_go_ahead = true;
  }

  //MSG("Sampler:STOP");
  pthread_mutex_lock (&stop_lock);
  if(stopJustCalled){
    pthread_mutex_unlock (&stop_lock);
    return;
  }
  struct timespec endTime;
  clock_gettime(CLOCK_REALTIME,&endTime);
  uint64_t usecs = endTime.tv_sec - startTime.tv_sec;
  usecs *= 1000000;
  usecs += (endTime.tv_nsec - startTime.tv_nsec)/1000;
  tusage[mode]->add(usecs);
#ifdef ENABLE_CUDA
  AtomicAdd(&phasenInst, icount);
#endif
  iusage[mode]->add(phasenInst);

  //get the globalClock_Timing, it includes idle clock as well
  if (mode == EmuTiming){ 
    // A bit complex swap because it is multithreaded
    Time_t gcct = globalClock;

    uint64_t cticks = gcct - globalClock_Timing_prev;
    globalClock_Timing->add(cticks);

    globalClock_Timing_prev = gcct;
  }else{
    globalClock_Timing_prev = globalClock; 
  }

  calcCPI();

  I(!stopJustCalled);
  I(phasenInst); // There should be something executed (more likely)
  lastPhasenInst = phasenInst;
  phasenInst = 0;
  stopJustCalled = true;
  calcCPIJustCalled = false;
  pthread_mutex_unlock (&stop_lock);
}
/* }}} */

void EmuSampler::startInit(FlowID fid)
  /* Start Init Mode : No timing or warmup, go as fast as possible {{{1 */
{
  //MSG("Sampler:STARTRABBIT");
  I(stopJustCalled);
  if (mode!=EmuInit)
    emul->startRabbit(fid);
  beginTiming(EmuInit);
}


void EmuSampler::startRabbit(FlowID fid)
  /* Start Rabbit Mode : No timing or warmup, go as fast as possible {{{1 */
{
  //MSG("Sampler:STARTRABBIT");
  I(stopJustCalled);
  if (mode!=EmuRabbit)
    emul->startRabbit(fid);
  beginTiming(EmuRabbit);
}
/* }}} */

void EmuSampler::startWarmup(FlowID fid)
  /* Start Rabbit Mode : No timing but it has cache/bpred warmup {{{1 */
{
  I(stopJustCalled);
  if (mode!=EmuWarmup)
    emul->startWarmup(fid);
  beginTiming(EmuWarmup);
}
/* }}} */

void EmuSampler::startDetail(FlowID fid)
  /* Start Rabbit Mode : Detailing modeling without no statistics gathering {{{1 */
{
  I(stopJustCalled);
  if (mode!=EmuDetail)
    emul->startDetail(fid);
  syncRunning();
  beginTiming(EmuDetail);
}
/* }}} */

void EmuSampler::startTiming(FlowID fid)
  /* Start Timing Mode : full timing model {{{1 */
{
  //MSG("Sampler:STARTTIMING");
  I(stopJustCalled);
  globalClock_Timing_prev = globalClock; 

  setInTiming(fid);
  //if (mode!=EmuTiming)
  emul->startTiming(fid);

  syncRunning();
  beginTiming(EmuTiming);
}
/* }}} */

bool EmuSampler::execute(FlowID fid, uint64_t icount)
  /* called for every instruction that qemu/gpu executes {{{1 */
{

	  if(fid!=0)
	  {
		  int j;
		  j=fid;
	  }

  GI(mode==EmuTiming, icount==1);

  local_icount+=icount; // There can be several samplers, but each has its own thread
  if ( likely(local_icount < 100))
    return !done[fid];

  AtomicAdd(&phasenInst, local_icount);
  AtomicAdd(&totalnInst, local_icount);

  local_icount = 0;

  if( likely(totalnInst <= next) )  // This is a likely taken branch, pass the info to gcc
    return !done[fid];

  next += 1024*1024; // Note, this is racy code. We can miss a rwdt from time to time, but who cares?

  if ( done[fid] ) {
    fprintf(stderr,"X" ); 
    fflush(stderr);
    {
      // We can not really, hold the thread, because it can hold locks inside qemu
      pthread_yield();
      sleep(1);
      fprintf(stderr,"X[%d]",fid); 
    }
  }else{
#ifdef PRINT_TOTALINST
	  // ATTA: augmented printfs with the totoalnInst
	  if(fid==0/*(totalnInst >> 20) % 100 == 0*/){	// Print every 100 million (100 * 2^20) cycles.
		fprintf(stderr,"\r");
		if (mode==EmuRabbit)
		  fprintf(stderr,"r");
		else if (mode==EmuWarmup)
		  fprintf(stderr,"w");
		else if (mode==EmuDetail)
		  fprintf(stderr,"d");
		else if (mode==EmuTiming)
		  fprintf(stderr,"t");
		else if (mode==EmuInit)
		  fprintf(stderr,">");
		else
		  fprintf(stderr,"?");

    	fprintf(stderr,"~%4.3f B\t", (float) (((double)totalnInst) / 1000000000.0));
		//fprintf(stderr,"%u\t", totalnInst);
	  }
#endif // PRINT_TOTALINST
  }

  return !done[fid];
}
/* }}} */

void EmuSampler::markDone()
/* indicate the sampler that a flow is done for good {{{1 */
{
  uint32_t endfid = emul->getNumEmuls() - 1;
  stop();
  I(!stopJustCalled || endfid==0); 

  I(done.size() > endfid);
  for(size_t i=0;i<endfid;i++) {
    done[i] = true;
  }

  phasenInst = 0;
//  mode       = EmuInit;
  terminate();
}
/* }}} */

void EmuSampler::syscall(uint32_t num, uint64_t usecs, FlowID fid)
  /* Create an syscall instruction and inject in the pipeline {{{1 */
{
  if (!syscall_enable)
    return;

  Time_t nsticks  = static_cast<uint64_t>(((double)usecs/freq)*1.0e12);

  if (syscall_generate) {
    // Generate
    uint32_t cycles = static_cast<uint32_t>(nsticks);
    I(syscall_file);
    fprintf(syscall_file, "%u %u\n",num,cycles);
  }else if(!syscall_runtime) {
    // use the file
    I(syscall_file);
    uint32_t num2;
    uint32_t cycles2;
    int ret = fscanf(syscall_file, "%u %u\n", &num2, &cycles2);
    if (num2!=num && ret!=2) {
      MSG("Error: the Syscall trace file does not seem consistent with the execution\n");
    }else{
      nsticks = cycles2;
    }
  }

  if (mode!=EmuTiming)
    return;

  emul->syscall(num,nsticks,fid);
}
/* }}} */

void EmuSampler::calcCPI()
  /* calculates cpi for the last EmuTiming mode {{{1 */
{
  if (mode != EmuTiming)
    return;
  I(calcCPIJustCalled == false); 
  I(!stopJustCalled);
  calcCPIJustCalled = true;

  //get instruction count
  uint64_t timingInst      = iusage[EmuTiming]->getSamples();
  uint64_t instCount       = timingInst - instPrev[sFid];
  I(instCount>0);
  instPrev[sFid]                 = timingInst;

  char str[255];
  sprintf(str, "P(%d):clockTicks", sFid); //FIXME: needs mapping
  GStats *gref = 0;
  gref = GStats::getRef(str);
  I(gref);
  uint64_t cticks = gref->getSamples();
  uint64_t clockInterval    =  cticks - clockPrev[sFid] + 1;
  clockPrev[sFid]        =  cticks;
  
  //Get freezed cycles due to thermal throttling 
  sprintf(str, "P(%d):nFreeze", sFid); //FIXME: needs mapping
  gref = 0;
  gref = GStats::getRef(str);
  I(gref);
  uint64_t fticks = gref->getSamples();
  uint64_t fticksInterval    =  fticks - fticksPrev[sFid] + 1;
  fticksPrev[sFid]        =  fticks;

  uint64_t uInstCount = nCommitted->getSamples() - prevnCommitted;
  prevnCommitted = nCommitted->getSamples();

  uint64_t adjustedClock = clockInterval - fticksInterval;

  //printf("InstCnt:%ld, prevInst:%ld, clock:%ld, ipc:%f\n", instCount, instPrev, clockInterval, static_cast<float>(instCount)/static_cast<float>(clockInterval));
  float newCPI  =  static_cast<float>(adjustedClock)/static_cast<float>(instCount);
  // newuCPI should be used, but as the sampler, we can only scale Inst, not uInst.
  //float newCPI  =  static_cast<float>(clockInterval)/static_cast<float>(uInstCount);
  I(newCPI>0);



  float newipc = static_cast<float>(instCount)/static_cast<float>(adjustedClock);
  float newuipc = static_cast<float>(uInstCount)/static_cast<float>(adjustedClock);
  uipc->sample(100*newuipc);
  ipc->sample(100*newipc);


  //I(newCPI<=4);
  if (newCPI > 5) {
    //newCPI = 5.0;
    //return;
  }
  //meaCPI = newCPI;
  meaCPI = newCPI;
  meauCPI = 1.0/newuipc;
}
/* }}} */

bool EmuSampler::othersStillInTiming(FlowID fid){
  return false;
  pthread_mutex_lock (&mode_lock);
  for (uint32_t i = 0 ; i < getNumFlows() ; i++) {
    if (isActive(mapLid(i))){
      if ( (inTiming[mapLid(i)] && mapLid(i) != fid) || 
          (nSamples[mapLid(i)] < nSamples[fid]) ) {
        pthread_mutex_unlock (&mode_lock);
        return true;
      }
    }
  }

  pthread_mutex_unlock (&mode_lock);
  return false;
}

void EmuSampler::updatenSamples() {
  if (justUpdatedtotalnSamples)
    return;

  totalnSamples ++ ;
  I(totalnSamples == nSamples[sFid]);
  syncTimes(sFid);
  justUpdatedtotalnSamples = true;
  //MSG(" %lu ", totalnSamples);
}

void EmuSampler::clearInTiming(FlowID fid){ 
  inTiming[fid] = 0; 
}

void EmuSampler::setInTiming(FlowID fid)  { 
  inTiming[fid] = 1; 
  justUpdatedtotalnSamples = false;
  nSamples[fid]++;
}

