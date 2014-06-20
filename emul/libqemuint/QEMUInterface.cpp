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

#include <pthread.h>

#include "InstOpcode.h"
#include "Instruction.h"
#include "Snippets.h"

#include "QEMUInterface.h"
#include "QEMUReader.h"
#include "EmuSampler.h"

#include "logger.h"

#define QPRINT_FORMAT_STR "inst fid=%d pc=0x%x insn=0x%x addr=0x%x data=0x%x op=%d icount=%d branch=%d cond=%d taken=%d target=0x%x rdn=%d rnn=%d rmn=%d rsn=%d dfn=%d rdv=0x%x rnv=0x%x rmv=0x%x rsv=0x%x dfv=0x%x" 

EmuSampler *qsamplerlist[128];
//EmuSampler *qsampler = 0;
bool* globalFlowStatus = 0;
XLog *qinterface_logger;

extern "C" void QEMUReader_goto_sleep(void *env);
extern "C" void QEMUReader_wakeup_from_sleep(void *env);
extern "C" int qemuesesc_main(int argc, char **argv, char **envp);

#define MAX_QEMU_CORE_COUNT 2

extern "C" void *qemuesesc_main_bootstrap(void *threadargs) {

  static unsigned qemuCount = 0;
  //static bool qemuStarted = false;
  if (qemuCount < MAX_QEMU_CORE_COUNT)
  {
    qemuCount ++;
    QEMUArgs *qdata = (struct QEMUArgs *) threadargs;

    int    qargc = qdata->qargc;
    char **qargv = qdata->qargv;

    MSG("Starting qemu with");
    for(int i = 0; i < qargc; i++)
      MSG("arg[%d] is: %s",i,qargv[i]);

    qemuesesc_main(qargc,qargv,NULL);

    MSG("qemu done");

    exit(0);
  }else{
    MSG("QEMU already started! Ignoring the new start.");
  }

  return 0;
}

extern "C" uint32_t QEMUReader_getFid(FlowID last_fid)
{
  return qsamplerlist[last_fid]->getFid(last_fid);
}

extern "C" uint64_t QEMUReader_get_time() 
{
  //FIXME: fid?
  return qsamplerlist[0]->getTime();
}

extern "C" void QEMUReader_queue_inst(uint32_t insn,
                             uint32_t pc,
                             uint32_t addr,
                             uint32_t data,
                             uint32_t fid,
                             char op,       // opcode 0 other, 1 LD, 2 ST, 3 BR
                             uint64_t icount,   // #inst >1 while in rabbit or warmup
                             void *env,    // CPU env
                             // ATTA: additional arguments required to reexecute instructions.
                             // Instruction broken down into its components
                             uint32_t pcore,
                             uint32_t dst_reg,   /// RD num
                             uint32_t src1_reg,   /// RN num
                             uint32_t src2_reg,   /// RM num
                             uint32_t src3_reg,   /// RS num
                             uint32_t impl_reg,   /// default num
                             uint32_t dst_val,   /// RD value
                             uint32_t src1_val,   /// RN value
                             uint32_t src2_val,   /// RM value
                             uint32_t src3_val,   /// RS value
                             uint32_t impl_val,   /// default value (impl_val)
                             // Branch information
                             uint32_t branch,/// is branch
                             uint32_t cond,  /// is conditional
                             uint32_t taken, /// is taken 
                             uint32_t target)/// target of branch
{
/*
  if(unlikely(!qinterface_logger)) {
     qinterface_logger = new XLog(XLog::SUPERVERBOSE, /// log level.
                                  "insn_trace.log",   /// log file name. 
                                  "qinterface_log",   /// name of this logger.
                                  1,                  /// log to file ?
                                  0,                  /// log to stdout ?
                                  1,                  /// enable sampling.
                                  1);                 /// sampling frequency.
  }

  /// log this instruction.
  qinterface_logger->logme(XLog::SUPERVERBOSE, 
                           QPRINT_FORMAT_STR,
                           fid,pc,insn,addr,data,op,icount, 
                           branch,cond,taken,target,dst_reg,src1_reg,
                           src2_reg,src3_reg,impl_reg,dst_val,src1_val,src2_val,src3_val,impl_val);
*/
  /// push this instruction.
  
  // ATTA: Original function call
  //qsamplerlist[fid]->queue(insn,pc,addr,data,fid,op,icount,env);
  // ATTA: The following is the modified version
	// ATTA: the queue call has been fixed to fid=0 because we only use one
	// thread and the fid of the pcore won't be 0 causing a segmentation fault
	// (cheap workaround).

	FlowID tempFid = (pcore) ? fid+1 : fid;
	qsamplerlist[fid]->queue(insn,pc,addr,data,tempFid,op,icount,env
							, src1_reg, src2_reg, src3_reg, dst_reg, impl_reg
							, src1_val, src2_val, src3_val, dst_val, impl_val
							, branch, cond, taken, target
  	  	  	  	  	  	  );
}

extern "C" void QEMUReader_finish(uint32_t fid)
{
  qsamplerlist[fid]->stop();
  qsamplerlist[fid]->pauseThread(fid);
  qsamplerlist[fid]->terminate();
//  qsamplerlist[fid]->freeFid(fid);
}

extern "C" void QEMUReader_finish_thread(uint32_t fid)
{
  qsamplerlist[fid]->stop();
  qsamplerlist[fid]->pauseThread(fid);
}

extern "C" void QEMUReader_syscall(uint32_t num, uint64_t usecs, uint32_t fid)
{
  qsamplerlist[fid]->syscall(num, usecs, fid);
}

#if 1
extern "C" FlowID QEMUReader_resumeThreadGPU(FlowID uid) {
  return(qsamplerlist[uid]->resumeThread(uid));
}

extern "C" FlowID QEMUReader_resumeThread(FlowID uid, FlowID last_fid) {

  uint32_t fid = qsamplerlist[0]->getFid(last_fid);
  return(qsamplerlist[fid]->resumeThread(uid, fid));
}
extern "C" void QEMUReader_pauseThread(FlowID fid) {
  qsamplerlist[fid]->pauseThread(fid);
}

extern "C" void QEMUReader_setFlowCmd(bool* flowStatus) {

}
#endif

extern "C" int QEMUReader_is_sampler_done(FlowID fid) {
  return qsamplerlist[fid]->isSamplerDone();
}

extern "C" int32_t QEMUReader_setnoStats(FlowID fid){
  qsamplerlist[fid]->dumpThreadProgressedTime(fid);
  qsamplerlist[fid]->setnoStats(fid);
  return 0;
}


