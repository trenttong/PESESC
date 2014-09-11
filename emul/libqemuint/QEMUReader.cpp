/* License & includes {{{1 */
/*
  ESESC: Super ESCalar simulator
  Copyright (C) 2005 University California, Santa Cruz.

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

#include <sys/types.h>
#include <dirent.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

#include "EmuSampler.h"
#include "SescConf.h"
#include "QEMUReader.h"
//#include "SPARCInstruction.h"
#include "Snippets.h"
#include "callback.h"
#include "DInst.h"
#include "QEMUInterface.h"

/* }}} */

#if 0
void *QEMUReader::getSharedMemory(size_t size)
/* Allocate a shared memory region {{{1 */
{
  int shmid= shmget(0,size,IPC_CREAT|0666);
  if (shmid<0) {
    MSG("ERROR: Could not allocate shared memory for fifo (run ipcs and iprm if necessary)");
    exit(-3);
  }

  void *shared_mem_ptr = shmat(shmid,NULL,0);

  shmctl(shmid, IPC_RMID, 0);

  return shared_mem_ptr;
}
/* }}} */
#endif

QEMUReader::QEMUReader(QEMUArgs *qargs, const char *section, EmulInterface *eint_)
  /* constructor {{{1 */
  : Reader(section),
    qemuargs(qargs),
    eint(eint_) {


  numFlows = 0;
  numAllFlows = 0;
  FlowID nemul = SescConf->getRecordSize("","cpuemul");
  for(FlowID i=0;i<nemul;i++) {
    const char *section = SescConf->getCharPtr("","cpuemul",i);
    const char *type    = SescConf->getCharPtr(section,"type");
    if(strcasecmp(type,"QEMU") == 0 ) {
      numFlows++;
    }
    numAllFlows++;
  }

  MSG ("CPU NumFlows = %d",numFlows);

#ifdef ESESC_QEMU_ISA_ARMEL
  crackInstARM = new ARMCrack[numAllFlows];
  crackInstThumb = new ThumbCrack[numAllFlows];
  crackInst.resize(numAllFlows);
#endif
  qemu_thread = -1;
  started = false;

#ifdef PSLICE_CONSTRUCT
  string pcs_filename = (string) SescConf->getCharPtr("Delinquency", "PC_list_fname");
  psliceConstructor = new PsliceConstructor(pcs_filename);
#endif // PSLICE_CONSTRUCT

    }
/* }}} */

void QEMUReader::start()
/* Start QEMU Thread (wait until sampler is ready {{{1 */
{

  if (started)
    return;

  started = true;

#if 1
  MSG ("STARTING QEMU ......");
  pthread_attr_t attr;
  pthread_attr_init(&attr);

  size_t stacksize = 1024*1024;
  pthread_attr_setstacksize(&attr, stacksize);

#if 0
  sigset_t mysigset;

  sigemptyset (&mysigset);
  sigaddset(&mysigset, SIGALRM);
  sigaddset(&mysigset, SIGHUP);
  sigaddset(&mysigset, SIGINT);
  sigaddset(&mysigset, SIGPIPE);

  pthread_sigmask (SIG_UNBLOCK, &mysigset, NULL);
#endif

  if (pthread_create(&qemu_thread, &attr, qemuesesc_main_bootstrap, (void *)qemuargs) != 0) {
    MSG("ERROR: pthread create failed");
    exit(-2);
  }
#else
  // Useful for debugging qemu (no threads)
  qemuesesc_main_bootstrap((void *)qemuargs);
#endif
  MSG("QEMUReader: Initializing qemu...");
}
/* }}} */

QEMUReader::~QEMUReader() {
  /* destructor {{{1 */
#if 0
  MSG("Killing group %d",getpgid(0));
  kill(-getpgid(0), SIGKILL);
#endif

#ifdef PSLICE_CONSTRUCT
  if(psliceConstructor) delete psliceConstructor;
#endif // PSLICE_CONSTRUCT

}
/* }}} */

// ATTA: an overloaded version which receives more information about the instruction and initiates instruction trace extraction
void QEMUReader::queueInstruction(uint32_t insn, AddrType pc, AddrType addr, DataType data, char thumb, FlowID fid, void *env, bool keepStats,
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

	  uint64_t conta=0;

	  if (tsfifo[fid].full()) {

	    //release lock
	    QEMUReader_goto_sleep(env);
	    // MSG("tsfifo full, goto sleep fid %d", fid);

	    while(tsfifo[fid].full()) {
	      // Good for 65K buffer struct timespec ts = {0,10000};
	      //pthread_yield();
	      conta++;
	      if (conta> 100000) {
	        struct timespec ts = {0,100000};
	        nanosleep(&ts, 0);
	        conta = 0;
	      }
	    }

	    // accuire lock again
	    QEMUReader_wakeup_from_sleep(env);
	    // MSG("tsfifo not full anymore, wakeup from sleep fid %d", fid);
	  }

	  RAWDInst *rinst = tsfifo[fid].getTailRef();

	  I(rinst);
	  I(insn);

	  float L1clkRatio = EmuSampler::getTurboRatio();
	  float L3clkRatio = 1.0;

	  rinst->set(insn,pc,addr,data,L1clkRatio,L3clkRatio,keepStats
			, src1_reg, src2_reg, src3_reg, dst_reg, impl_reg
			, src1_val, src2_val, src3_val, dst_val, impl_val
			, branch, cond, taken, target);

	#ifdef ESESC_QEMU_ISA_ARMEL
	  // ATTA: similar to what has been done in QEMUInterface::QEMUReader_queue_inst,
	  // here we also replace the fid with 0. This hard codes the crackInst to only
	  // one thread.
	  if (thumb){
	    crackInst[0/*fid*/] = &crackInstThumb[0/*fid*/];
	  }else{
	    crackInst[0/*fid*/] = &crackInstARM[0/*fid*/];
	  }

	#if 0
	  if (crackInst[fid]->isInIT()) {
	    AddrType epc = crackInst[fid]->getCurrentDecodePC();
	    AddrType cpc = rinst->getPC();

	    I((epc+16)<=cpc); // At most 16 instructions

	    while (epc<cpc) {
	      crackInst[fid]->popIT();
	      epc+=4; // FIXME: or 2
	    }

	    crackInst[fid]->setCurrentDecodePC(cpc+2); // FIXME: it could be +4
	  }
	#endif

	  crackInst[0/*fid*/]->expand(rinst);


	#if 0
	  static int sample=0;
	  sample++;
	  if (sample > 1021) { // prime number
	    if (thumb){
	      printf("THUMB pc:0x%x insn:0x%x size:%d\n", pc, insn, rinst->getNumInst());
	    }else{
	      printf("ARM pc:0x%x insn:0x%x size:%d\n", pc, insn, rinst->getNumInst());
	    }
	    sample = 0;
	  }
	#endif

	#else
	  //esesc_disas_sparc_inst(rinst);
	  I(0); //
	#endif

	#if 0
	  for(size_t j=0;j<rinst->getNumInst();j++) {
	    const Instruction *inst = rinst->getInstRef(j);
	    MSG("*x%d* pc=0x%llx addr=0x%llx data=0x%x (%d:%d)", j, rinst->getPC(), rinst->getAddr(), rinst->getData(), inst->isStore(), inst->isLoad() );
	  }
	#endif

	#if 0
	if ((rinst->getPC() == 0xdeaddead) || (rinst->getPC() == 0xdeaddeb1)){
	  //Added by Alamelu to test cudamemcpy
	  for(size_t j=0;j<rinst->getNumInst();j++) {
	    const Instruction *inst = rinst->getInstRef(j);
	    inst->dump("dead ");
	    MSG("*x%d* pc=0x%llx addr=0x%llx data=0x%x (%d:%d)", j, rinst->getPC(), rinst->getAddr(), rinst->getData(), inst->isStore(), inst->isLoad() );
	  }
	}
	#endif

	  if(rinst->getNumInst()==0) {
	    // Legal for IT blocks
	#if 0
	    if (crackInst[fid]->isInIT()) {
	      crackInst[fid]->setCurrentDecodePC(rinst->getPC()+2); // Always thumb2
	    }
	#endif

	    return;
	  }

	#ifdef DEBUG
	  for(size_t j=0;j<rinst->getNumInst();j++) {
	    const Instruction *inst = rinst->getInstRef(j);
	    I(inst->getOpcode() != iOpInvalid);
	    //GI(inst->isMemory(), (rinst->getAddr() & 0x3) == 0);
	  }
	#endif

	  I(rawInst.size() > fid);
	  //rawInst[fid]->add(1);
	  rawInst[fid]->inc();

	  tsfifo[fid].push();
}

void QEMUReader::queueInstruction(uint32_t insn, AddrType pc, AddrType addr, DataType data, char thumb, FlowID fid, void * env, bool keepStats)
/* queue instruction (called by QEMU) {{{1 */
{
  uint64_t conta=0;

  if (tsfifo[fid].full()) {

    //release lock
    QEMUReader_goto_sleep(env);
    // MSG("tsfifo full, goto sleep fid %d", fid);

    while(tsfifo[fid].full()) {
      // Good for 65K buffer struct timespec ts = {0,10000};
      //pthread_yield();
      conta++;
      if (conta> 100000) {
        struct timespec ts = {0,100000};
        nanosleep(&ts, 0);
        conta = 0;
      }
    }

    // accuire lock again
    QEMUReader_wakeup_from_sleep(env);
    // MSG("tsfifo not full anymore, wakeup from sleep fid %d", fid);
  }

  RAWDInst *rinst = tsfifo[fid].getTailRef();

  I(rinst);
  I(insn);

  float L1clkRatio = EmuSampler::getTurboRatio();
  float L3clkRatio = 1.0;

  rinst->set(insn,pc,addr,data,L1clkRatio,L3clkRatio,keepStats);
#ifdef ESESC_QEMU_ISA_ARMEL
  if (thumb){
    crackInst[fid] = &crackInstThumb[fid];
  }else{
    crackInst[fid] = &crackInstARM[fid];
  }

#if 0
  if (crackInst[fid]->isInIT()) {
    AddrType epc = crackInst[fid]->getCurrentDecodePC();
    AddrType cpc = rinst->getPC();

    I((epc+16)<=cpc); // At most 16 instructions

    while (epc<cpc) {
      crackInst[fid]->popIT();
      epc+=4; // FIXME: or 2
    }

    crackInst[fid]->setCurrentDecodePC(cpc+2); // FIXME: it could be +4
  }
#endif

  crackInst[fid]->expand(rinst);


#if 0
  static int sample=0;
  sample++;
  if (sample > 1021) { // prime number
    if (thumb){
      printf("THUMB pc:0x%x insn:0x%x size:%d\n", pc, insn, rinst->getNumInst());
    }else{
      printf("ARM pc:0x%x insn:0x%x size:%d\n", pc, insn, rinst->getNumInst());
    }
    sample = 0;
  }
#endif

#else
  //esesc_disas_sparc_inst(rinst);
  I(0); //
#endif

#if 0
  for(size_t j=0;j<rinst->getNumInst();j++) {
    const Instruction *inst = rinst->getInstRef(j);
    MSG("*x%d* pc=0x%llx addr=0x%llx data=0x%x (%d:%d)", j, rinst->getPC(), rinst->getAddr(), rinst->getData(), inst->isStore(), inst->isLoad() );
  }
#endif

#if 0
if ((rinst->getPC() == 0xdeaddead) || (rinst->getPC() == 0xdeaddeb1)){
  //Added by Alamelu to test cudamemcpy
  for(size_t j=0;j<rinst->getNumInst();j++) {
    const Instruction *inst = rinst->getInstRef(j);
    inst->dump("dead ");
    MSG("*x%d* pc=0x%llx addr=0x%llx data=0x%x (%d:%d)", j, rinst->getPC(), rinst->getAddr(), rinst->getData(), inst->isStore(), inst->isLoad() );
  }
}
#endif

  if(rinst->getNumInst()==0) {
    // Legal for IT blocks
#if 0
    if (crackInst[fid]->isInIT()) {
      crackInst[fid]->setCurrentDecodePC(rinst->getPC()+2); // Always thumb2
    }
#endif

    return;
  }

#ifdef DEBUG
  for(size_t j=0;j<rinst->getNumInst();j++) {
    const Instruction *inst = rinst->getInstRef(j);
    I(inst->getOpcode() != iOpInvalid);
    //GI(inst->isMemory(), (rinst->getAddr() & 0x3) == 0);
  }
#endif

  I(rawInst.size() > fid);
  //rawInst[fid]->add(1);
  rawInst[fid]->inc();

  tsfifo[fid].push();
 }
/* }}} */

void QEMUReader::syscall(uint32_t num, Time_t time, FlowID fid)
/* Create an syscall instruction and inject in the pipeline {{{1 */
{
#if 0
  // FIXME: Maybe the wait function should be a method by itself
  uint64_t conta=0;
  while(tsfifo[fid].full()) {
    // Good for 65K buffer struct timespec ts = {0,10000};
    pthread_yield();
    conta++;
    if (conta> 100000) {
      struct timespec ts = {0,100000};
      nanosleep(&ts, 0);
      conta = 0;
    }
  }
#endif
  RAWDInst *rinst = tsfifo[fid].getTailRef();

  rinst->set(0,0xdeaddead,num,static_cast<DataType>(time));

  rinst->clearInst();
  Instruction *inst = rinst->getNewInst();
  inst->set(iRALU, NoDependence, NoDependence, LREG_InvalidOutput, LREG_InvalidOutput, false);
  tsfifo[fid].push();
}

uint32_t QEMUReader::wait_until_FIFO_full(FlowID fid)
{
  if(!tsfifo[fid].full()) {
    pthread_yield();
    if (qsamplerlist[0/*fid*/]->isActive(0/*fid*/) == false)	// ATTA: again hard coded fid to 0 because there is no sampler for the Pcore thread.
      return 0;
    if (!tsfifo[fid].full()) {
      pthread_yield();
      //qsampler->pauseThread(fid); // Too slow, get it out for until the FIFO is full
      // Very infrequent situation unless it is time to power down
      if (tsfifo[fid].empty()){
        return 0;
      }
    }
  }
  return 1;
}

DInst *QEMUReader::executeHead(FlowID fid)
/* speculative advance of execution {{{1 */
{
  //DInst *dinst = pickNextDInst(fid);

  //if (dinst)
  //  return dinst;

  //I(fid<numFlows); //Don't need this... because fid can change at runtime

  if (ruffer[fid].empty()) {

	  if(!tsfifo[fid].full()) {
		  pthread_yield();
		  if (qsamplerlist[0]->isActive(0) == false)
			  return 0;
		  if(!tsfifo[fid].full()) {
			  //pthread_yield();
			  //if(!tsfifo[fid].full())
				  return 0;
		  }
	  }
	  /*
	  while(!tsfifo[fid].full()) {
      pthread_yield();
      // ATTA: changed the fid below to be fixed to 0 since we only have one sampler
      // but we have two simulators (mcore and pcore).
      if (qsamplerlist[0]->isActive(0) == false)
        return 0;
      if (tsfifo[fid].empty()) {
        pthread_yield();
        //qsampler->pauseThread(fid); // Too slow, get it out for until the FIFO is full
        // Very infrequent situation unless it is time to power down
        if (tsfifo[fid].empty()){
          return 0;		// ATTA: uncommented this line to avoid deadlock between Mcore and Pcore.
        }
      }
    } */

    //I(tsfifo[fid].full());
    // To minimize false sharing (start by 7, enough for a long cache line)
    bool first = true;
    AddrType last_PC = 0;
    for(int i=32;i<tsfifo[fid].size();i++) {

      RAWDInst  *rinst = tsfifo[fid].getHeadRef();

#ifdef PSLICE_CONSTRUCT
      psliceConstructor->recieveInstruction(fid, rinst->getPC(), rinst->getInsn(), rinst->getAddr(), rinst->getData(), rinst->src1_reg, rinst->src2_reg,
    		  rinst->src3_reg, rinst->dst_reg, rinst->impl_reg, rinst->src1_val, rinst->src2_val, rinst->src3_val, rinst->dst_val, rinst->impl_val,
    		  rinst->isBranch, rinst->isCond, rinst->taken, rinst->targetAddr);
#endif // PSLICE_CONSTRUCT

      I(rinst->getNumInst()!=0);

#if 0
      static uint64_t crackinst = 0;
      static uint64_t cnt       = 0;
      crackinst                += rinst->getNumInst();
      cnt++;
      if (!(cnt % 10000002))
        MSG("\naverage crack rate:%f\n", (float (crackinst)/float (cnt)));
#endif

      for(size_t j=0;j<rinst->getNumInst();j++) {
        DInst **dinsth = ruffer[fid].getInsertPointRef();

        if (rinst->getInstRef(j)->isMemory() && rinst->getAddr() == 0) {
          // This happens with several syscalls from crack
          continue;
        }

        if ((rinst->getInstRef(j)->isMemory() || rinst->getInstRef(j)->isFuncRet()) && !first) {
          /*
           * Catching load or store multiples already cracked by QEMU
           *  Load to PC will be treated as FuncRet
          */
          if (tsfifo[fid].realsize() < 16) {
            if(wait_until_FIFO_full(fid)==0) {
              //I(0);
              return 0;
            }
          }
          //I(tsfifo[fid].realsize()>=16);
          RAWDInst  *rinst2 = tsfifo[fid].getNextHeadRef();
          if (rinst2->getPC() == rinst->getPC()) {
            // either the same or 3 more because of the ITT block from ARM
            //I(rinst->getNumInst() == rinst2->getNumInst() || (rinst2->getNumInst()+3) == rinst->getNumInst());
            tsfifo[fid].pop();

            //printf("pc=0x%llx addr=0x%llx : addr2=0x%llx\n",rinst->getPC(), rinst->getAddr(), rinst2->getAddr());
            I(rinst->getNumInst() == rinst2->getNumInst());
            rinst = rinst2;

#ifdef PSLICE_CONSTRUCT
            psliceConstructor->recieveInstruction(fid, rinst->getPC(), rinst->getInsn(), rinst->getAddr(), rinst->getData(), rinst->src1_reg, rinst->src2_reg,
            		rinst->src3_reg, rinst->dst_reg, rinst->impl_reg, rinst->src1_val, rinst->src2_val, rinst->src3_val, rinst->dst_val, rinst->impl_val,
            		rinst->isBranch, rinst->isCond, rinst->taken, rinst->targetAddr);
#endif // PSLICE_CONSTRUCT

            if (rinst2->getNumInst()<=j)
              j = rinst2->getNumInst()-1; // HACK HACK, We still have an IT block problem

            I(rinst == tsfifo[fid].getHeadRef());
          }
          /* End of catching load or store multiples already cracked by QEMU */
        }
        if (rinst->getPC() != last_PC)
          first = true;
        if (rinst->getInstRef(j)->isMemory() ||  rinst->getInstRef(j)->isFuncRet())
          first = false;

        last_PC = rinst->getPC();
        //printf("create pc=0x%llx addr=0x%llx %d %d\n",rinst->getPC(), rinst->getAddr(),j,rinst->getInstRef(j)->getOpcode());

        if (rinst->getInstRef(j)->isControl() && (j != (rinst->getNumInst()-1))) {
          // Control instruction in the middle (some conditional instruction executed)
          if (rinst->getAddr()==0) {
            // taken crack exit branch
            *dinsth = DInst::create(rinst->getInstRef(j), rinst, rinst->getPC()+4, fid);

#ifdef ENABLE_CUDA
            (*dinsth)->setPE(0);
#endif
            //(*dinsth)->dump("create");
            ruffer[fid].add();
            break;
          }else{
            // conditional instruction in the middle, and not taken crack exit branch
            *dinsth = DInst::create(rinst->getInstRef(j), rinst, 0, fid);
          }
        }else{
          //I(!(rinst->getInstRef(j)->isMemory() && rinst->getAddr()==0));
          *dinsth = DInst::create(rinst->getInstRef(j), rinst, rinst->getAddr(), fid);
        }
        I((*dinsth)->getInst()->getOpcode() != iOpInvalid);
#ifdef ENABLE_CUDA
        (*dinsth)->setPE(0);
#endif
        //(*dinsth)->dump("create");
        ruffer[fid].add();
      }
#if DEBUG_LDM
      if(rinst->getPC()==0x10110)
        printf("----------------\n");
#endif
      tsfifo[fid].pop();
    }
  }

  DInst *dinst = ruffer[fid].getHead();
#ifdef ENABLE_CUDA
  (dinst)->setPE(0);
#endif

  ruffer[fid].popHead();

  I(dinst); // We just added, there should be one or more

  return dinst;
}
/* }}} */

void QEMUReader::reexecuteTail(FlowID fid) {
  /* safe/retire advance of execution {{{1 */

  ruffer[fid].advanceTail();
}
/* }}} */

void QEMUReader::syncHeadTail(FlowID fid){
  /* replay triggers a syncHeadTail {{{1 */

  ruffer[fid].moveHead2Tail();
  //ruffer[fid].advanceTail(); // Make sure that the same inst is not re-exec again

}
/* }}} */

void QEMUReader::drainFIFO(FlowID fid)
/* Drain the tsfifo as much as possible due to a mode change {{{1 */
{
  I(0); // not needed for the moment
  if (pthread_equal(qemu_thread,pthread_self()))
    return;

  while(!tsfifo[fid].empty() && !ruffer[fid].empty()) {
    pthread_yield();
  }
}
/* }}} */

