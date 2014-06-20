/*
 *  i386 emulator main execution loop
 *
 *  Copyright (c) 2003-2005 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include "cpu.h"
#include "disas.h"
#include "tcg.h"
#include "qemu-barrier.h"
#include "qemu-stats.h"

int tb_invalidated_flag;

//#define CONFIG_DEBUG_EXEC
#if defined (CONFIG_ESESC_system) || defined (CONFIG_ESESC_user)
#include "esesc_qemu.h"

uint32_t esesc_iload(uint32_t addr) {
  return ldl_code(addr);
}

uint64_t esesc_ldu64(uint32_t addr) {
  return ldq_raw(addr);
}
uint32_t esesc_ldu32(uint32_t addr) {
  return ldl_raw(addr);
}
uint16_t esesc_ldu16(uint32_t addr) {
  return lduw_raw(addr);
}
int16_t esesc_lds16(uint32_t addr) {
  return ldsw_raw(addr);
}
uint8_t esesc_ldu08(uint32_t addr) {
  return ldub_raw(addr);
}
int8_t esesc_lds08(uint32_t addr) {
  return ldsb_raw(addr);
}
void esesc_st64(uint32_t addr, uint64_t data) {
  stq_raw(addr,data);
}
void esesc_st32(uint32_t addr, uint32_t data) {
  stl_raw(addr,data);
}
void esesc_st16(uint32_t addr, uint16_t data) {
  stw_raw(addr,data);
}
void esesc_st08(uint32_t addr, uint8_t data) {
  stb_raw(addr,data);
}

static int pending_flush=0;
int esesc_allow_large_tb[128] = {[0 ... 127] = 1};
int esesc_single_inst_tb[128] = {[0 ... 127] = 0};
void esesc_set_rabbit(uint32_t fid)
{
  if (esesc_allow_large_tb[fid] == 1 && esesc_single_inst_tb[fid] == 0)
    return;
  esesc_allow_large_tb[fid] = 1;
  esesc_single_inst_tb[fid] = 0;
  //pending_flush = 1;
}
void esesc_set_warmup(uint32_t fid)
{
  if (esesc_allow_large_tb[fid] == 0 && esesc_single_inst_tb[fid] == 0)
    return;
  esesc_allow_large_tb[fid] = 0;
  esesc_single_inst_tb[fid] = 0;
  //pending_flush = 1;
}
void esesc_set_timing(uint32_t fid)
{
  if (esesc_allow_large_tb[fid] == 0 && esesc_single_inst_tb[fid] == 1)
    return;
  esesc_allow_large_tb[fid] = 0;
  esesc_single_inst_tb[fid] = 1;
  //pending_flush = 1;
}
#endif


bool qemu_cpu_has_work(CPUState *env)
{
    return cpu_has_work(env);
}

void cpu_loop_exit(CPUState *env)
{
    env->current_tb = NULL;
    longjmp(env->jmp_env, 1);
}

/* exit the current TB from a signal handler. The host registers are
   restored in a state compatible with the CPU emulator
 */
#if defined(CONFIG_SOFTMMU)
void cpu_resume_from_signal(CPUState *env, void *puc)
{
    /* XXX: restore cpu registers saved in host registers */

    env->exception_index = -1;
    longjmp(env->jmp_env, 1);
}
#endif


/* Execute the code without caching the generated code. An interpreter
   could be used if available. */
static void cpu_exec_nocache(CPUState *env, int max_cycles,
                             TranslationBlock *orig_tb)
{
    unsigned long next_tb;
    TranslationBlock *tb;

    /* Should never happen.
       We only end up here when an existing TB is too long.  */
    if (max_cycles > CF_COUNT_MASK)
        max_cycles = CF_COUNT_MASK;

    tb = tb_gen_code(env, orig_tb->pc, orig_tb->cs_base, orig_tb->flags,
                     max_cycles);
    env->current_tb = tb;
    /* execute the generated code */
    next_tb = tcg_qemu_tb_exec(env, tb->tc_ptr);
    env->current_tb = NULL;

    if ((next_tb & 3) == 2) {
        /* Restore PC.  This may happen if async event occurs before
           the TB starts executing.  */
        cpu_pc_from_tb(env, tb);
    }
    tb_phys_invalidate(tb, -1);
    tb_free(tb);
}

static TranslationBlock *tb_find_slow(CPUState *env,
                                      target_ulong pc,
                                      target_ulong cs_base,
                                      uint64_t flags)
{
    TranslationBlock *tb, **ptb1;
    unsigned int h;
    tb_page_addr_t phys_pc, phys_page1;
    target_ulong virt_page2;

    tb_invalidated_flag = 0;

    /* find translated block using physical mappings */
    phys_pc = get_page_addr_code(env, pc);
    phys_page1 = phys_pc & TARGET_PAGE_MASK;
    h = tb_phys_hash_func(phys_pc);
    ptb1 = &tb_phys_hash[h];
    for(;;) {
        tb = *ptb1;
        if (!tb)
            goto not_found;
        if (tb->pc == pc &&
            tb->page_addr[0] == phys_page1 &&
            tb->cs_base == cs_base &&
            tb->flags == flags) {
            /* check next page if needed */
            if (tb->page_addr[1] != -1) {
                tb_page_addr_t phys_page2;

                virt_page2 = (pc & TARGET_PAGE_MASK) +
                    TARGET_PAGE_SIZE;
                phys_page2 = get_page_addr_code(env, virt_page2);
                if (tb->page_addr[1] == phys_page2)
                    goto found;
            } else {
                goto found;
            }
        }
        ptb1 = &tb->phys_hash_next;
    }
 not_found:
   /* if no translated code available, then translate it now */
    tb = tb_gen_code(env, pc, cs_base, flags, 0);

 found:
    /* Move the last found TB to the head of the list */
    if (likely(*ptb1)) {
        *ptb1 = tb->phys_hash_next;
        tb->phys_hash_next = tb_phys_hash[h];
        tb_phys_hash[h] = tb;
    }
    /* we add the TB in the virtual pc hash table */
    env->tb_jmp_cache[tb_jmp_cache_hash_func(pc)] = tb;
    return tb;
}

static inline TranslationBlock *tb_find_fast(CPUState *env)
{
    TranslationBlock *tb;
    target_ulong cs_base, pc;
    int flags;

    /* we record a subset of the CPU state. It will
       always be the same before a given translated block
       is executed. */
    cpu_get_tb_cpu_state(env, &pc, &cs_base, &flags);
    tb = env->tb_jmp_cache[tb_jmp_cache_hash_func(pc)];
    if (unlikely(!tb || tb->pc != pc || tb->cs_base != cs_base ||
                 tb->flags != flags)) {
        tb = tb_find_slow(env, pc, cs_base, flags);
    }
    return tb;
}

static CPUDebugExcpHandler *debug_excp_handler;

CPUDebugExcpHandler *cpu_set_debug_excp_handler(CPUDebugExcpHandler *handler)
{
    CPUDebugExcpHandler *old_handler = debug_excp_handler;

    debug_excp_handler = handler;
    return old_handler;
}

static void cpu_handle_debug_exception(CPUState *env)
{
    CPUWatchpoint *wp;

    if (!env->watchpoint_hit) {
        QTAILQ_FOREACH(wp, &env->watchpoints, entry) {
            wp->flags &= ~BP_WATCHPOINT_HIT;
        }
    }
    if (debug_excp_handler) {
        debug_excp_handler(env);
    }
}

/* main execution loop */
volatile sig_atomic_t exit_request;

#if defined (CONFIG_ESESC_system) || defined (CONFIG_ESESC_user)

#define HEARTBEAT_FREQ    50000000
///#define HEARTBEAT_FREQ    500
#include <assert.h>

#define IS_INTRACE(env)   (env->processor_state == IN_TRACE) 
#define IS_PRETRACE(env)  (env->processor_state == PRE_TRACE)
#define IS_POSTTRACE(env) (env->processor_state == POST_TRACE)

FILE *ssio = NULL;

/* record the # of instructions executed */
static void qemu_single_step_dump(CPUState *env)
{
///    dump_states(env);
}

/// monitor every executed instruction for ptrace entry.
static bool qemu_entry_pc_monitor(CPUState *env)
{
   int idx;
   int found = false;
   for(idx=0; idx<env->stop_pc_idx; idx++)
   {
      found |= (GET_PC(env) == env->stop_pc[idx]); 
   }
   return found;
}

/// monitor every executed instruction for ptrace exit.
static bool qemu_exit_pc_monitor(CPUState *env)
{
   int idx;
   int found = false;
   for(idx=0; idx < env->ptrc_pc_idx; idx++)
   {
      found |= (GET_PC(env) == env->ptrc_pc[idx]); 
   }
   return !found;
}

/// monitor every executed instruction for ptrace skip.
static TranslationBlock *qemu_skip_pc_monitor(CPUState *env, TranslationBlock *tb)
{
   /* only prefetch core is allowed to skip */
   assert(ISPCORE(env));

   int found = false;
   int idx;
   for(idx=0; idx < env->skip_pc_idx; idx++)
   {
      found |= (GET_PC(env) == env->skip_pc[idx]); 
   }

   if (found) 
   {
     /* this translation block needs to be skipped */
     env->regs[15] = tb->next_pc;
     return tb_find_fast(env);
   }

   /* return the original tb */
   return tb; 
}

/// xtrace_init_cpu_tp - initialize a CPU tracepoint strucutre.
static void xtrace_reset_trace_point(CPUTracePoint *tp)
{
   int i          = 0;
   tp->pc         = 0;
   tp->opcode     = 0;
   tp->is_branch  = 0;
   tp->is_taken   = 0;
   tp->is_cond    = 0;
   tp->has_imm    = 0;
   tp->is_lsm     = 0;
   tp->memadd_cnt = 0;
   tp->memval_cnt = 0;
   tp->rn_regn    = INVALID_REG;
   tp->rm_regn    = INVALID_REG;
   tp->rs_regn    = INVALID_REG;
   tp->rd_regn    = INVALID_REG;
   tp->df_regn    = INVALID_REG;
   for(;i<MAX_REG_PER_INSN;++i) tp->lsm_regn[i] = INVALID_REG;
}

/// xtrace_reset_trace_points - instrumentation data has been read out. 
/// reset the structure.
static void xtrace_reset_trace_points(CPUState *env)
{
   int i, cnt = env->xtrace_op_cnt;
   for(i=0; i<=cnt; ++i) xtrace_reset_trace_point(&env->xtrace_op_tp[i]); 
   env->xtrace_op_cnt = 0;
}

/// xtrm_trace_sanity_check - This function performs some sanity
/// check on the collected data through instrumentation.
static void xtrm_trace_sanity_check(CPUState *env)
{
   static unsigned long long total_cnt = 0;
   int i, cnt = env->xtrace_op_cnt;
   total_cnt += cnt;

   /// heartbeat.
   if (total_cnt % HEARTBEAT_FREQ == 0) 
   {
     //fprintf(stdout, "heartbeat: %lld instructions traced\n", total_cnt);
   }

   /// -------------------------------------- ///
   /// verify pc.                             /// 
   /// the difference between 2 consecutive   ///
   /// PC has to be 2 or 4.                   ///
   /// -------------------------------------- ///
   unsigned last_pc = 0;
   unsigned curr_pc = 0;
   for(i=1; i<=cnt; ++i) {
      last_pc = curr_pc;
      curr_pc = env->xtrace_op_tp[i].pc;
      unsigned diff = curr_pc - last_pc;
      if (diff < 5 && last_pc) assert(diff==4 || diff==2);
   }

   /// -------------------------------------- ///
   /// verify register number. the register   ///
   /// number has to be a value of less than  /// 
   /// the max GPR number.                    /// 
   /// -------------------------------------- ///
   for(i=0; i<=cnt; ++i) {
      assert(IS_VALID_OR_NULL_ARCH_REG(env->xtrace_op_tp[i].rn_regn));
      assert(IS_VALID_OR_NULL_ARCH_REG(env->xtrace_op_tp[i].rm_regn));
      assert(IS_VALID_OR_NULL_ARCH_REG(env->xtrace_op_tp[i].rs_regn));
      assert(IS_VALID_OR_NULL_ARCH_REG(env->xtrace_op_tp[i].rd_regn));
   }

   /// -------------------------------------- ///
   /// verify load store multiple register    ///
   /// number. if an operation is load/store  ///
   /// multiple, the number of valid register ///
   /// number must be the same as the number  ///
   /// of memory operands.                    ///
   /// -------------------------------------- ///
   for(i=0; i<=cnt; ++i) {
      CPUTracePoint *tp = &env->xtrace_op_tp[i];
      if (tp->is_lsm)
      {
         unsigned k=0;
         unsigned lctr = 0;
         for(;k<MAX_REG_PER_INSN;++k) {
            if (IS_VALID_ARCH_REG(tp->lsm_regn[k])) 
            {
                /// printf("memadd 0x%x\n", tp->lsm_memadd[k]);
                lctr ++;
            }
         }
         /// total number of lsm registers must be no bigger than 
         /// MAX_REG_PER_INSN
         assert(lctr<=MAX_REG_PER_INSN);
      }
   }
 

   /// -------------------------------------- ///
   /// verify memory.                         /// 
   /// need a way to verify memory.
   /// -------------------------------------- ///
   for(i=0; i<=cnt; ++i) {
#if 0
      unsigned ctr  = env->xtrace_op_tp[i].memadd_cnt;
      unsigned addr = env->xtrace_op_tp[i].memadd[0];
      ///if (ctr) printf("memaddr is 0x%d 0x%x\n", ctr, addr);
#endif
   }

   /// -------------------------------------- ///
   /// verify branch.                         /// 
   /// 1. detect instruction is not branch,but///
   /// conditional set ....                   ///
   /// -------------------------------------- ///
   for(i=0; i<=cnt; ++i) {
      /// if (!env->xtrace_op_tp[i].is_branch && env->xtrace_op_tp[i].is_cond) assert(0);      
   }
}

/// xtrm_trace_expand_single_nomem - expand the trace record with no memory.
static void xtrm_trace_expand_single_nomem(CPUState *env, int idx)
{
   CPUTracePoint *curr = &env->xtrace_op_tp[idx];
   ExpandedCPUTracePoint *curr_x = &env->xtrace_xp_op_tp[env->op_cnt];
   /// one more record.
   env->op_cnt ++;
   /// copy the fields.
   curr_x->pc = curr->pc;
   curr_x->opcode = curr->opcode;
   curr_x->has_imm = curr->has_imm;
   curr_x->src_imm = curr->src_imm;
   curr_x->is_branch = curr->is_branch;
   curr_x->is_cond = curr->is_cond;
   curr_x->is_taken = curr->is_taken;
   curr_x->target = curr->target;
   curr_x->rn_regn = curr->rn_regn;  
   curr_x->rm_regn = curr->rm_regn; 
   curr_x->rs_regn = curr->rs_regn;   
   curr_x->rd_regn = curr->rd_regn;  
   curr_x->df_regn = curr->df_regn;
   curr_x->rn_regv = curr->rn_regv; 
   curr_x->rm_regv = curr->rm_regv;   
   curr_x->rs_regv = curr->rs_regv;  
   curr_x->rd_regv = curr->rd_regv;
   curr_x->df_regv = curr->df_regv;
   curr_x->memadd  = INVALID_MEMADD; 
   curr_x->hasmem  = 0;
}

/// xtrm_trace_expand_single_mem - expand the trace record with memory trace.
static void xtrm_trace_expand_single_mem(CPUState *env, int idx, int midx)
{
   CPUTracePoint *curr = &env->xtrace_op_tp[idx];
   ExpandedCPUTracePoint *curr_x = &env->xtrace_xp_op_tp[env->op_cnt];
   /// one more record.
   env->op_cnt ++;
   /// copy the fields.
   curr_x->pc = curr->pc;
   curr_x->opcode = curr->opcode;
   curr_x->has_imm = curr->has_imm;
   curr_x->src_imm = curr->src_imm;
   curr_x->is_branch = curr->is_branch;
   curr_x->is_cond = curr->is_cond;
   curr_x->is_taken= curr->is_taken;
   curr_x->target  = curr->target;
   curr_x->rn_regn = curr->rn_regn;  
   curr_x->rm_regn = curr->rm_regn; 
   curr_x->rs_regn = curr->rs_regn;   
   curr_x->rd_regn = curr->rd_regn;  
   curr_x->df_regn = curr->df_regn;
   curr_x->rn_regv = curr->rn_regv; 
   curr_x->rm_regv = curr->rm_regv;   
   curr_x->rs_regv = curr->rs_regv;  
   curr_x->rd_regv = curr->rd_regv;
   curr_x->df_regv = curr->df_regv;
   curr_x->memadd  = curr->memadd[midx];
   curr_x->memval  = curr->memval[midx];
   curr_x->hasmem  = 1;
}

/// xtrm_trace_expand - exapand the per instruction CPUTracePoint into structure
/// understanable to the simulation interface.
static void xtrm_trace_expand(CPUState *env)
{
   int i, cnt = env->xtrace_op_cnt;
   for(i=1; i<=cnt;++i)
   {
      unsigned ctr = env->xtrace_op_tp[i].memadd_cnt;
      if (!ctr){
        xtrm_trace_expand_single_nomem(env, i);
      } else {
        int k=0;
        for(;k<ctr;++k) xtrm_trace_expand_single_mem(env, i, k);
     }
   }
}

static void xtrm_trace_print(CPUState *env)
{
#if TRACE_PRINT_VERIFY
   int i, cnt = env->op_cnt;
   for(i=0;i<cnt;++i)
   {
      ExpandedCPUTracePoint *curr_x = &env->xtrace_xp_op_tp[i];
#ifdef ENABLE_INSTRUCTION_TRACING
      printf("pc is 0x%x bytes is 0x%x\n", 
              curr_x->pc,  
              ldl_code(curr_x->pc));
#endif 

#ifdef ENABLE_BRANCH_TRACING
      if (curr_x->is_branch)
         printf("branchinst pc is 0x%x is_cond %d is_taken %d target 0x%x\n",
                 curr_x->pc,
                 curr_x->is_cond,
                 curr_x->is_taken,
                 curr_x->target);
#endif

#ifdef ENABLE_MEMORY_TRACING
      if (curr_x->hasmem)
         printf("meminst pc is 0x%x memaddr is 0x%x memval is 0x%x\n",
                curr_x->pc,
                curr_x->memadd,
                curr_x->memval);
#endif

#if ENABLE_REGISTER_TRACING
     printf("reginst pc is 0x%x rdn is %d rdv is 0x%x rnn is %d rnv is 0x%x rmn is %d rmv is 0x%x rsn is %d rsv is 0x%x\n",
             curr_x->pc,
             curr_x->rd_regn,
             curr_x->rd_regv,
             curr_x->rn_regn,
             curr_x->rn_regv,
             curr_x->rm_regn,
             curr_x->rm_regv,
             curr_x->rs_regn,
             curr_x->rs_regv);
#endif
   }
#endif
}

#undef HEARTBEAT_FREQ
#endif

/* create a temporary prefetch core */
static void qemu_cpu_create_pcore(CPUState *env) 
{
   const char *dbgm = "[PREFETCH-DEBUG]";
   /* pcore can not create pcore */
   assert(!ISPCORE(env));
   assert(ISPCORE(env->PCore));

   /* copy over the entire register states of the main core */
   memcpy(env->PCore, env, sizeof(CPUState));

   /* some states of the PCORE is different from MCORE */
   env->PCore->IsPCore = 1;
   env->PCore->PCore   = 0;
   env->PCore->MCore   = env;
   env->PCore->processor_state = IN_TRACE;
   env->PCore->prefetchDistance = 0;
   //env->PCore->fid = 1;
   printf("%s PCore created\n", dbgm);
}

static void qemu_cpu_destroy_pcore(CPUState *env) 
{
   const char *dbgm = "[PREFETCH-DEBUG]";
   /* make sure we are destroying a PCORE */
   assert(ISPCORE(env));
   env->processor_state = PRE_TRACE;
   memset(env->sbytes, 0, sizeof(MemoryByte*)*MEMBYTE_BUCKET_NUM);
   printf("%s PCore destroyed\n", dbgm);
}

#define MAX_PC_COUNT 4096 
static FILE *ptrace = NULL;
static FILE *pslice = NULL;
extern char *ptrace_name;
extern char *pslice_name;
extern bool disable_pcore;
static void parse_file_to_pcs(CPUState *env)
{
    /* do not do anything, pcore is disabled */
    if (disable_pcore) {
    	//printf("PCore disabled.\n");
    	return ;
    }

    printf("Opening ptrace %s and pslice file %s\n", ptrace_name, pslice_name);
    ptrace = fopen(ptrace_name, "r");
    pslice = fopen(pslice_name, "r");

    assert(ptrace && pslice && "ptrace or pslice not provided properly");

    /* the first pc is the entry pc */
    int askip_pc[MAX_PC_COUNT];
    int trace_pc[MAX_PC_COUNT];
    int slice_pc[MAX_PC_COUNT];
    memset(askip_pc, 0, MAX_PC_COUNT*sizeof(int));
    memset(trace_pc, 0, MAX_PC_COUNT*sizeof(int));
    memset(slice_pc, 0, MAX_PC_COUNT*sizeof(int));

    int trace_idx=0;
    int slice_idx=0;
    while (!feof(ptrace)) fscanf(ptrace, "%x", &trace_pc[trace_idx++]);
    while (!feof(pslice)) fscanf(pslice, "%x", &slice_pc[slice_idx++]);

    if (trace_idx >= MAX_PC_COUNT || slice_idx >= MAX_PC_COUNT) 
    {
       perror("more ptrace/pslice pc provided than the emulator can handle\n");
       exit(0);
    }

    /* these 2 pcs should be equal */
    assert(trace_pc[0] == slice_pc[0]);

    /* find pcs in trace_pc but not in slice_pc */
    int n,m;
    int askip_idx=0;
    for(n=0; n < MAX_PC_COUNT; n++)
    {
       int found = 0;
       for(m=0;m<MAX_PC_COUNT;m++) found |= (trace_pc[n] == slice_pc[m]);
       if (!found) askip_pc[askip_idx++] = trace_pc[n];
    }

    const char* dbgm = "[PREFETCH-DEBUG]";
    /* handle entry pc */
    env->stop_pc[env->stop_pc_idx++] = trace_pc[0];
    printf("%s entry pc is 0x%lx\n", dbgm, (long int) trace_pc[0]);
    /* handle ptrc pc */
    for(n=0; n<trace_idx-1; n++) 
    {
      env->ptrc_pc[env->ptrc_pc_idx++] = trace_pc[n];
      printf("%s trace_pc has 0x%lx\n", dbgm, (long int) trace_pc[n]);
    }
    /* handle skip pc */
    for(n=0; n<askip_idx; n++)   
    {
      env->skip_pc[env->skip_pc_idx++] = askip_pc[n];
      printf("%s pskip_pc has 0x%lx\n", dbgm, (long int) askip_pc[n]);
    }

    if (env->skip_pc_idx == env->ptrc_pc_idx)
    {
       perror("you are skipping everything. prefetch core will stuck\n");
       exit(0);
    }
    /* done */
    return;
}

int cpu_exec(CPUState *env, CPUState **next_env)
{
    int ret, interrupt_request;
    TranslationBlock *tb;
    uint8_t *tc_ptr;
    unsigned long next_tb;

    //const char *dbgm = "[PREFETCH-DEBUG]";
    //const char *logname = "single_step.log";

    /* reset the # of instructions to execute */
    env->InstQuatum = ROUNDROBIN_EXECUTION_SLICE; 

    if (!ptrace && !pslice) parse_file_to_pcs(env);
    ///if (0) parse_file_to_pcs(env);

    if (env->halted) {
        if (!cpu_has_work(env)) {
            return EXCP_HALTED;
        }
        env->halted = 0;
    }

    cpu_single_env = env;

#if defined (CONFIG_ESESC_system) || defined (CONFIG_ESESC_user)
    env->op_cnt         =0;
    env->xtrace_op_cnt  =0;
#endif

    if (unlikely(exit_request)) {
        env->exit_request = 1;
    }

#if defined(TARGET_I386)
    /* put eflags in CPU temporary format */
    CC_SRC = env->eflags & (CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
    DF = 1 - (2 * ((env->eflags >> 10) & 1));
    CC_OP = CC_OP_EFLAGS;
    env->eflags &= ~(DF_MASK | CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
#elif defined(TARGET_SPARC)
#elif defined(TARGET_M68K)
    env->cc_op = CC_OP_FLAGS;
    env->cc_dest = env->sr & 0xf;
    env->cc_x = (env->sr >> 4) & 1;
#elif defined(TARGET_ALPHA)
#elif defined(TARGET_ARM)
#elif defined(TARGET_UNICORE32)
#elif defined(TARGET_PPC)
    env->reserve_addr = -1;
#elif defined(TARGET_LM32)
#elif defined(TARGET_MICROBLAZE)
#elif defined(TARGET_MIPS)
#elif defined(TARGET_SH4)
#elif defined(TARGET_CRIS)
#elif defined(TARGET_S390X)
#elif defined(TARGET_XTENSA)
    /* XXXXX */
#else
#error unsupported target CPU
#endif
    env->exception_index = -1;

    /* prepare setjmp context for exception handling */
    for(;;) {
        if (setjmp(env->jmp_env) == 0) {
            /* if an exception is pending, we execute it here */
            if (env->exception_index >= 0) {
                if (env->exception_index >= EXCP_INTERRUPT) {
                    /* exit request from the cpu execution loop */
                    ret = env->exception_index;
                    if (ret == EXCP_DEBUG) {
                        cpu_handle_debug_exception(env);
                    }
                    break;
                } else {
#if defined(CONFIG_USER_ONLY)
                    /* if user mode only, we simulate a fake exception
                       which will be handled outside the cpu execution
                       loop */
#if defined(TARGET_I386)
                    do_interrupt(env);
#endif
                    ret = env->exception_index;
                    break;
#else
                    do_interrupt(env);
                    env->exception_index = -1;
#endif
                }
            }

            next_tb = 0; /* force lookup of first TB */
            for(;;) {
                if (ISPCORE(env))  assert(env->MCore);
                if (!ISPCORE(env)) assert(env->PCore);
                if (!ISPCORE(env)) assert(ISPCORE(env->PCore));

                interrupt_request = env->interrupt_request;
                if (unlikely(interrupt_request)) {
                    if (unlikely(env->singlestep_enabled & SSTEP_NOIRQ)) {
                        /* Mask out external interrupts for this step. */
                        interrupt_request &= ~CPU_INTERRUPT_SSTEP_MASK;
                    }
                    if (interrupt_request & CPU_INTERRUPT_DEBUG) {
                        env->interrupt_request &= ~CPU_INTERRUPT_DEBUG;
                        env->exception_index = EXCP_DEBUG;
                        cpu_loop_exit(env);
                    }
#if defined(TARGET_ARM) || defined(TARGET_SPARC) || defined(TARGET_MIPS) || \
    defined(TARGET_PPC) || defined(TARGET_ALPHA) || defined(TARGET_CRIS) || \
    defined(TARGET_MICROBLAZE) || defined(TARGET_LM32) || defined(TARGET_UNICORE32)
                    if (interrupt_request & CPU_INTERRUPT_HALT) {
                        env->interrupt_request &= ~CPU_INTERRUPT_HALT;
                        env->halted = 1;
                        env->exception_index = EXCP_HLT;
                        cpu_loop_exit(env);
                    }
#endif
#if defined(TARGET_I386)
                    if (interrupt_request & CPU_INTERRUPT_INIT) {
                            svm_check_intercept(env, SVM_EXIT_INIT);
                            do_cpu_init(env);
                            env->exception_index = EXCP_HALTED;
                            cpu_loop_exit(env);
                    } else if (interrupt_request & CPU_INTERRUPT_SIPI) {
                            do_cpu_sipi(env);
                    } else if (env->hflags2 & HF2_GIF_MASK) {
                        if ((interrupt_request & CPU_INTERRUPT_SMI) &&
                            !(env->hflags & HF_SMM_MASK)) {
                            svm_check_intercept(env, SVM_EXIT_SMI);
                            env->interrupt_request &= ~CPU_INTERRUPT_SMI;
                            do_smm_enter(env);
                            next_tb = 0;
                        } else if ((interrupt_request & CPU_INTERRUPT_NMI) &&
                                   !(env->hflags2 & HF2_NMI_MASK)) {
                            env->interrupt_request &= ~CPU_INTERRUPT_NMI;
                            env->hflags2 |= HF2_NMI_MASK;
                            do_interrupt_x86_hardirq(env, EXCP02_NMI, 1);
                            next_tb = 0;
			} else if (interrupt_request & CPU_INTERRUPT_MCE) {
                            env->interrupt_request &= ~CPU_INTERRUPT_MCE;
                            do_interrupt_x86_hardirq(env, EXCP12_MCHK, 0);
                            next_tb = 0;
                        } else if ((interrupt_request & CPU_INTERRUPT_HARD) &&
                                   (((env->hflags2 & HF2_VINTR_MASK) && 
                                     (env->hflags2 & HF2_HIF_MASK)) ||
                                    (!(env->hflags2 & HF2_VINTR_MASK) && 
                                     (env->eflags & IF_MASK && 
                                      !(env->hflags & HF_INHIBIT_IRQ_MASK))))) {
                            int intno;
                            svm_check_intercept(env, SVM_EXIT_INTR);
                            env->interrupt_request &= ~(CPU_INTERRUPT_HARD | CPU_INTERRUPT_VIRQ);
                            intno = cpu_get_pic_interrupt(env);
                            qemu_log_mask(CPU_LOG_TB_IN_ASM, "Servicing hardware INT=0x%02x\n", intno);
                            do_interrupt_x86_hardirq(env, intno, 1);
                            /* ensure that no TB jump will be modified as
                               the program flow was changed */
                            next_tb = 0;
#if !defined(CONFIG_USER_ONLY)
                        } else if ((interrupt_request & CPU_INTERRUPT_VIRQ) &&
                                   (env->eflags & IF_MASK) && 
                                   !(env->hflags & HF_INHIBIT_IRQ_MASK)) {
                            int intno;
                            /* FIXME: this should respect TPR */
                            svm_check_intercept(env, SVM_EXIT_VINTR);
                            intno = ldl_phys(env->vm_vmcb + offsetof(struct vmcb, control.int_vector));
                            qemu_log_mask(CPU_LOG_TB_IN_ASM, "Servicing virtual hardware INT=0x%02x\n", intno);
                            do_interrupt_x86_hardirq(env, intno, 1);
                            env->interrupt_request &= ~CPU_INTERRUPT_VIRQ;
                            next_tb = 0;
#endif
                        }
                    }
#elif defined(TARGET_PPC)
#if 0
                    if ((interrupt_request & CPU_INTERRUPT_RESET)) {
                        cpu_reset(env);
                    }
#endif
                    if (interrupt_request & CPU_INTERRUPT_HARD) {
                        ppc_hw_interrupt(env);
                        if (env->pending_interrupts == 0)
                            env->interrupt_request &= ~CPU_INTERRUPT_HARD;
                        next_tb = 0;
                    }
#elif defined(TARGET_LM32)
                    if ((interrupt_request & CPU_INTERRUPT_HARD)
                        && (env->ie & IE_IE)) {
                        env->exception_index = EXCP_IRQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_MICROBLAZE)
                    if ((interrupt_request & CPU_INTERRUPT_HARD)
                        && (env->sregs[SR_MSR] & MSR_IE)
                        && !(env->sregs[SR_MSR] & (MSR_EIP | MSR_BIP))
                        && !(env->iflags & (D_FLAG | IMM_FLAG))) {
                        env->exception_index = EXCP_IRQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_MIPS)
                    if ((interrupt_request & CPU_INTERRUPT_HARD) &&
                        cpu_mips_hw_interrupts_pending(env)) {
                        /* Raise it */
                        env->exception_index = EXCP_EXT_INTERRUPT;
                        env->error_code = 0;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_SPARC)
                    if (interrupt_request & CPU_INTERRUPT_HARD) {
                        if (cpu_interrupts_enabled(env) &&
                            env->interrupt_index > 0) {
                            int pil = env->interrupt_index & 0xf;
                            int type = env->interrupt_index & 0xf0;

                            if (((type == TT_EXTINT) &&
                                  cpu_pil_allowed(env, pil)) ||
                                  type != TT_EXTINT) {
                                env->exception_index = env->interrupt_index;
                                do_interrupt(env);
                                next_tb = 0;
                            }
                        }
		    }
#elif defined(TARGET_ARM)
                    if (interrupt_request & CPU_INTERRUPT_FIQ
                        && !(env->uncached_cpsr & CPSR_F)) {
                        env->exception_index = EXCP_FIQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
                    /* ARMv7-M interrupt return works by loading a magic value
                       into the PC.  On real hardware the load causes the
                       return to occur.  The qemu implementation performs the
                       jump normally, then does the exception return when the
                       CPU tries to execute code at the magic address.
                       This will cause the magic PC value to be pushed to
                       the stack if an interrupt occurred at the wrong time.
                       We avoid this by disabling interrupts when
                       pc contains a magic address.  */
                    if (interrupt_request & CPU_INTERRUPT_HARD
                        && ((IS_M(env) && env->regs[15] < 0xfffffff0)
                            || !(env->uncached_cpsr & CPSR_I))) {
                        env->exception_index = EXCP_IRQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_UNICORE32)
                    if (interrupt_request & CPU_INTERRUPT_HARD
                        && !(env->uncached_asr & ASR_I)) {
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_SH4)
                    if (interrupt_request & CPU_INTERRUPT_HARD) {
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_ALPHA)
                    {
                        int idx = -1;
                        /* ??? This hard-codes the OSF/1 interrupt levels.  */
		        switch (env->pal_mode ? 7 : env->ps & PS_INT_MASK) {
                        case 0 ... 3:
                            if (interrupt_request & CPU_INTERRUPT_HARD) {
                                idx = EXCP_DEV_INTERRUPT;
                            }
                            /* FALLTHRU */
                        case 4:
                            if (interrupt_request & CPU_INTERRUPT_TIMER) {
                                idx = EXCP_CLK_INTERRUPT;
                            }
                            /* FALLTHRU */
                        case 5:
                            if (interrupt_request & CPU_INTERRUPT_SMP) {
                                idx = EXCP_SMP_INTERRUPT;
                            }
                            /* FALLTHRU */
                        case 6:
                            if (interrupt_request & CPU_INTERRUPT_MCHK) {
                                idx = EXCP_MCHK;
                            }
                        }
                        if (idx >= 0) {
                            env->exception_index = idx;
                            env->error_code = 0;
                            do_interrupt(env);
                            next_tb = 0;
                        }
                    }
#elif defined(TARGET_CRIS)
                    if (interrupt_request & CPU_INTERRUPT_HARD
                        && (env->pregs[PR_CCS] & I_FLAG)
                        && !env->locked_irq) {
                        env->exception_index = EXCP_IRQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
                    if (interrupt_request & CPU_INTERRUPT_NMI
                        && (env->pregs[PR_CCS] & M_FLAG)) {
                        env->exception_index = EXCP_NMI;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_M68K)
                    if (interrupt_request & CPU_INTERRUPT_HARD
                        && ((env->sr & SR_I) >> SR_I_SHIFT)
                            < env->pending_level) {
                        /* Real hardware gets the interrupt vector via an
                           IACK cycle at this point.  Current emulated
                           hardware doesn't rely on this, so we
                           provide/save the vector when the interrupt is
                           first signalled.  */
                        env->exception_index = env->pending_vector;
                        do_interrupt_m68k_hardirq(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_S390X) && !defined(CONFIG_USER_ONLY)
                    if ((interrupt_request & CPU_INTERRUPT_HARD) &&
                        (env->psw.mask & PSW_MASK_EXT)) {
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_XTENSA)
                    if (interrupt_request & CPU_INTERRUPT_HARD) {
                        env->exception_index = EXC_IRQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#endif
                   /* Don't use the cached interrupt_request value,
                      do_interrupt may have updated the EXITTB flag. */
                    if (env->interrupt_request & CPU_INTERRUPT_EXITTB) {
                        env->interrupt_request &= ~CPU_INTERRUPT_EXITTB;
                        /* ensure that no TB jump will be modified as
                           the program flow was changed */
                        next_tb = 0;
                    }
                }
                if (unlikely(env->exit_request)) {
                    env->exit_request = 0;
                    env->exception_index = EXCP_INTERRUPT;
                    cpu_loop_exit(env);
                }
#if defined(DEBUG_DISAS) || defined(CONFIG_DEBUG_EXEC)
                if (qemu_loglevel_mask(CPU_LOG_TB_CPU)) {
                    /* restore flags in standard format */
#if defined(TARGET_I386)
                    env->eflags = env->eflags | cpu_cc_compute_all(env, CC_OP)
                        | (DF & DF_MASK);
                    log_cpu_state(env, X86_DUMP_CCOP);
                    env->eflags &= ~(DF_MASK | CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
#elif defined(TARGET_M68K)
                    cpu_m68k_flush_flags(env, env->cc_op);
                    env->cc_op = CC_OP_FLAGS;
                    env->sr = (env->sr & 0xffe0)
                              | env->cc_dest | (env->cc_x << 4);
                    log_cpu_state(env, 0);
#else
                    log_cpu_state(env, 0);
#endif
                }
#endif /* DEBUG_DISAS || CONFIG_DEBUG_EXEC */
                spin_lock(&tb_lock);
#if defined (CONFIG_ESESC_system) || defined (CONFIG_ESESC_user)
                if (pending_flush) {
                  tb_flush(env);
                  pending_flush = 0;
                }
#endif
                tb = tb_find_fast(env);
                /* Note: we do it here to avoid a gcc bug on Mac OS X when
                   doing it in tb_find_slow */
                if (tb_invalidated_flag) {
                    /* as some TB could have been invalidated because
                       of memory exceptions while generating the code, we
                       must recompute the hash index here */
                    next_tb = 0;
                    tb_invalidated_flag = 0;
                }
#ifdef CONFIG_DEBUG_EXEC
                qemu_log_mask(CPU_LOG_EXEC, "Trace 0x%08lx [" TARGET_FMT_lx "] %s\n",
                             (long)tb->tc_ptr, tb->pc,
                             lookup_symbol(tb->pc));
#endif
                /* see if we can patch the calling TB. When the TB
                   spans two pages, we cannot safely do a direct
                   jump. */
                if (next_tb != 0 && tb->page_addr[1] == -1) {
#if defined (CONFIG_ESESC_system) || defined (CONFIG_ESESC_user)
                    /* use single step in ESESC mode */
#else
                    tb_add_jump((TranslationBlock *)(next_tb & ~3), next_tb & 3, tb);
#endif
                }
                spin_unlock(&tb_lock);

                /* cpu_interrupt might be called while translating the
                   TB, but before it is linked into a potentially
                   infinite loop and becomes env->current_tb. Avoid
                   starting executionk if there is a pending interrupt. */
                env->current_tb = tb;
                barrier();
                if (likely(!env->exit_request)) 
                {
                    /// --------------------------------------------------- ///
                    ///
                    /* monitor for entry pc */
                    ///
                    /// --------------------------------------------------- ///
                    if (!ISPCORE(env) && IS_PRETRACE(env) && qemu_entry_pc_monitor(env)) 
                    {
                       /* Check whether MCore has consumed enough of the Prefetches or not before creating a new Pcore */
                       if(env->PCore->prefetchDistance <= 0)
                       {
                    	  //printf("%s PrefetchDistance: Max %lld, current %lld \n", dbgm, env->maxPrefetchDistance, env->PCore->prefetchDistance);
                          /* create the prefetch core and hope it can run ahead and do something useful*/
                          qemu_cpu_create_pcore(env);
                          /* the system is in trace now */
                          env->processor_state = IN_TRACE;
                          /* print  the trace enter */
                          //printf("%s stop point hit at 0x%lx\n", dbgm, (long int)GET_PC(env));
                          //printf("%s entered in-trace mode. singlestep log in %s\n", dbgm, logname);
                       }
                    }

                    /// --------------------------------------------------- ///
                    /* monitor for exit pc */
                    /// --------------------------------------------------- ///
                    if(ISPCORE(env) && IS_INTRACE(env) && qemu_exit_pc_monitor(env)) 
                    {
                       //printf("%s PrefetchDistance: Max %lld, current %lld \n", dbgm, env->MCore->maxPrefetchDistance, env->prefetchDistance);
                       /* the system is post (off) trace now */
                       env->processor_state = PRE_TRACE;
                       env->InstQuatum = 0;
                       env->MCore->processor_state = PRE_TRACE;
                       /* shutdown the prefetch core */
                       qemu_cpu_destroy_pcore(env);
                       /* log it */
                       //printf("%s exit point hit at 0x%lx\n", dbgm, (long int)GET_PC(env));
                       //printf("%s entered post-trace mode\n", dbgm);
                       goto load_main_core;
                    }


                    /// --------------------------------------------------- ///
                    /* skip this pc, and find  the next translation block ? */
                    /// --------------------------------------------------- ///
                    TranslationBlock *new_tb = NULL;
                    while (ISPCORE(env) && IS_INTRACE(env))
                    {
                       new_tb = qemu_skip_pc_monitor(env, tb);
                       if (new_tb == tb) break;
                       tb = new_tb;
                    }

                    /// --------------------------------------------------- ///
                    /* track the number of prefetches */
                    /// --------------------------------------------------- ///
                    if(ISPCORE(env) && IS_INTRACE(env) && qemu_entry_pc_monitor(env))
                    {
                       env->prefetchDistance++;
                       //fprintf(stderr,"\rprefetchDist: %lld", env->prefetchDistance);
                       if(env->MCore->maxPrefetchDistance < env->prefetchDistance)
                       {
                    	   env->MCore->maxPrefetchDistance = env->prefetchDistance;
                       }
                    }
                    else if(!ISPCORE(env) && qemu_entry_pc_monitor(env))
                    {
                    	env->PCore->prefetchDistance--;
                    	//fprintf(stderr,"\rprefetchDist: %lld", env->PCore->prefetchDistance);
                    }

                    /// --------------------------------------------------- ///
                    /* dump every instruction when in trace */
                    /// --------------------------------------------------- ///
                    if (ISPCORE(env) && IS_INTRACE(env)) qemu_single_step_dump(env);

                    printf("number of shadow bytes is %d\n", env->sbytes_size);
                    tc_ptr = tb->tc_ptr;
                    /* execute the generated code */
                    next_tb = tcg_qemu_tb_exec(env, tc_ptr);
                    /* one more instructions executed */
                    if (IS_INTRACE(env)) env->InstQuatum --;

#if defined (CONFIG_ESESC_system) || defined (CONFIG_ESESC_user)
                    /// do a sanity check to make sure the tracing is done correctly.
                    xtrm_trace_sanity_check(env);
                    /// expand the CPUTracePoint.
                    xtrm_trace_expand(env);
                    /// do some printing for verification.
                    xtrm_trace_print(env);
                    if (env) {
                      int i;
                      //for(i=0;i<env->op_cnt;i++) 
                      //  printf("%d pc:0x%x, insn:0x%x\n",i, env->op_pc[i], env->op_insn[i]);

                      if (esesc_allow_large_tb[env->fid]) { // RABBIT
                        uint32_t ninst = env->op_cnt;
                        if (ninst == 0)  ninst = tb->icount;

                        QEMUReader_queue_inst(0xdeaddead, 
                                              env->op_pc[ninst-1], 
                                              0, 
                                              0, 
                                              env->fid,
                                              env->op_insn[ninst-1], 
                                              ninst, (void *) env, env->IsPCore, 0, 0, 0, 0 , 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

                      } else if (esesc_allow_large_tb[env->fid]==0 && esesc_single_inst_tb[env->fid] == 0) { // WARMUP
                          for(i=0;i<env->op_cnt;i++) {
                            QEMUReader_queue_inst(0xdeadbeaf, 
                                                  env->xtrace_xp_op_tp[i].pc, 
                                                  env->xtrace_xp_op_tp[i].memadd, 
                                                  env->xtrace_xp_op_tp[i].memval, 
                                                  env->fid, 
                                                  env->xtrace_xp_op_tp[i].opcode, 
                                                  1, 
                                                  (void *) env,
                                                  env->IsPCore,
                                                  env->xtrace_xp_op_tp[i].rd_regn, 
                                                  env->xtrace_xp_op_tp[i].rn_regn, 
                                                  env->xtrace_xp_op_tp[i].rm_regn, 
                                                  env->xtrace_xp_op_tp[i].rs_regn, 
                                                  env->xtrace_xp_op_tp[i].df_regn, 
                                                  env->xtrace_xp_op_tp[i].rd_regv, 
                                                  env->xtrace_xp_op_tp[i].rn_regv, 
                                                  env->xtrace_xp_op_tp[i].rm_regv, 
                                                  env->xtrace_xp_op_tp[i].rs_regv, 
                                                  env->xtrace_xp_op_tp[i].df_regv, 
                                                  env->xtrace_xp_op_tp[i].is_branch, 
                                                  env->xtrace_xp_op_tp[i].is_cond, 
                                                  env->xtrace_xp_op_tp[i].is_taken, 
                                                  env->xtrace_xp_op_tp[i].target);

                          }
                      } else {
                        // FIXME: what if env->op_cnt == 0
                        for(i=0;i<env->op_cnt;i++) {
                          // the msb of op_inst indicates thumb mode for ARM
                          QEMUReader_queue_inst(ldl_code(env->xtrace_xp_op_tp[i].pc), 
                                                env->xtrace_xp_op_tp[i].pc, 
                                                env->xtrace_xp_op_tp[i].memadd, 
                                                env->xtrace_xp_op_tp[i].memval, 
                                                env->fid, 
                                                env->xtrace_xp_op_tp[i].opcode, 
                                                1, 
                                                (void *) env,
                                                env->IsPCore,
                                                env->xtrace_xp_op_tp[i].rd_regn, 
                                                env->xtrace_xp_op_tp[i].rn_regn, 
                                                env->xtrace_xp_op_tp[i].rm_regn, 
                                                env->xtrace_xp_op_tp[i].rs_regn, 
                                                env->xtrace_xp_op_tp[i].df_regn, 
                                                env->xtrace_xp_op_tp[i].rd_regv, 
                                                env->xtrace_xp_op_tp[i].rn_regv, 
                                                env->xtrace_xp_op_tp[i].rm_regv, 
                                                env->xtrace_xp_op_tp[i].rs_regv, 
                                                env->xtrace_xp_op_tp[i].df_regv, 
                                                env->xtrace_xp_op_tp[i].is_branch, 
                                                env->xtrace_xp_op_tp[i].is_cond, 
                                                env->xtrace_xp_op_tp[i].is_taken, 
                                                env->xtrace_xp_op_tp[i].target);
                        }
                      }
                      xtrace_reset_trace_points(env);
                      env->op_cnt   =0;
                      env->xtrace_op_cnt   =0;
                    }
#endif
                    if ((next_tb & 3) == 2) {
                        /* Instruction counter expired.  */
                        int insns_left;
                        tb = (TranslationBlock *)(long)(next_tb & ~3);
                        /* Restore PC.  */
                        cpu_pc_from_tb(env, tb);
                        insns_left = env->icount_decr.u32;
                        if (env->icount_extra && insns_left >= 0) {
                            /* Refill decrementer and continue execution.  */
                            env->icount_extra += insns_left;
                            if (env->icount_extra > 0xffff) {
                                insns_left = 0xffff;
                            } else {
                                insns_left = env->icount_extra;
                            }
                            env->icount_extra -= insns_left;
                            env->icount_decr.u16.low = insns_left;
                        } else {
                            if (insns_left > 0) {
                                /* Execute remaining instructions.  */
                                cpu_exec_nocache(env, insns_left, tb);
#if defined (CONFIG_ESESC_system) || defined (CONFIG_ESESC_user)
                                if (env) {
			  	   int i;
     			           if (esesc_allow_large_tb[env->fid]) { // RABBIT
                                      uint32_t ninst = env->op_cnt;
		                      if (ninst == 0) ninst = tb->icount;
                                      QEMUReader_queue_inst(0xdeaddead, 
                                                            env->op_pc[ninst-1], 
                                                            0, 
                                                            0, 
                                                            env->fid, 
                                                            0, 
                                                            ninst, (void *) env, env->IsPCore, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                                   } else if (esesc_allow_large_tb[env->fid]==0 && esesc_single_inst_tb[env->fid] == 0) { // WARMUP
                                      for(i=0;i<env->op_cnt;i++) {
                                         QEMUReader_queue_inst(ldl_code(env->xtrace_xp_op_tp[i].pc), 
                                                env->xtrace_xp_op_tp[i].pc, 
                                                env->xtrace_xp_op_tp[i].memadd, 
                                                env->xtrace_xp_op_tp[i].memval, 
                                                env->fid, 
                                                env->xtrace_xp_op_tp[i].opcode, 
                                                1, 
                                                (void *) env,
                                                env->IsPCore,
                                                env->xtrace_xp_op_tp[i].rd_regn, 
                                                env->xtrace_xp_op_tp[i].rn_regn, 
                                                env->xtrace_xp_op_tp[i].rm_regn, 
                                                env->xtrace_xp_op_tp[i].rs_regn, 
                                                env->xtrace_xp_op_tp[i].df_regn, 
                                                env->xtrace_xp_op_tp[i].rd_regv, 
                                                env->xtrace_xp_op_tp[i].rn_regv, 
                                                env->xtrace_xp_op_tp[i].rm_regv, 
                                                env->xtrace_xp_op_tp[i].rs_regv, 
                                                env->xtrace_xp_op_tp[i].df_regv, 
                                                env->xtrace_xp_op_tp[i].is_branch, 
                                                env->xtrace_xp_op_tp[i].is_cond, 
                                                env->xtrace_xp_op_tp[i].is_taken, 
                                                env->xtrace_xp_op_tp[i].target);
                                      }
                                   } else {
                                     // FIXME: what if env->op_cnt == 0
                                     for(i=0;i<env->op_cnt;i++) {
                                        // the msb of op_inst indicates thumb mode for ARM
                                        QEMUReader_queue_inst(ldl_code(env->xtrace_xp_op_tp[i].pc), 
                                                env->xtrace_xp_op_tp[i].pc, 
                                                env->xtrace_xp_op_tp[i].memadd, 
                                                env->xtrace_xp_op_tp[i].memval, 
                                                env->fid, 
                                                env->xtrace_xp_op_tp[i].opcode, 
                                                1, 
                                                (void *) env,
                                                env->IsPCore,
                                                env->xtrace_xp_op_tp[i].rd_regn, 
                                                env->xtrace_xp_op_tp[i].rn_regn, 
                                                env->xtrace_xp_op_tp[i].rm_regn, 
                                                env->xtrace_xp_op_tp[i].rs_regn, 
                                                env->xtrace_xp_op_tp[i].df_regn, 
                                                env->xtrace_xp_op_tp[i].rd_regv, 
                                                env->xtrace_xp_op_tp[i].rn_regv, 
                                                env->xtrace_xp_op_tp[i].rm_regv, 
                                                env->xtrace_xp_op_tp[i].rs_regv, 
                                                env->xtrace_xp_op_tp[i].df_regv, 
                                                env->xtrace_xp_op_tp[i].is_branch, 
                                                env->xtrace_xp_op_tp[i].is_cond, 
                                                env->xtrace_xp_op_tp[i].is_taken, 
                                                env->xtrace_xp_op_tp[i].target);
                                    }
                                  }
                                  xtrace_reset_trace_points(env);
                                  env->op_cnt   =0;
                                  env->xtrace_op_cnt   =0;
                                }
#endif
                            }
                            env->exception_index = EXCP_INTERRUPT;
                            next_tb = 0;
                            cpu_loop_exit(env);
                            }
                        }
                    }
                    env->current_tb = NULL;

        /// --------------------------------------------------- ///
        /* in trace means we need to run a few instructions from each core in a */ 
        /* roundrobin fashion.this core has no instructions remaining */
        /// --------------------------------------------------- ///
        if (IS_INTRACE(env) && !env->InstQuatum) 
        {
           if (ISPCORE(env))
              env = env->MCore;
           else   // ATTA: will switch from MCore to PCore only if run-ahead distance is below threshold.
           {
              if(env->PCore->prefetchDistance < 1000)
        	     env = env->PCore;
           }
           env->InstQuatum = ROUNDROBIN_EXECUTION_SLICE; 
           //printf("switch to %s with %d instruction quata\n", ISPCORE(env) ? "PCORE" : "MCORE", env->InstQuatum);
        }

        if (0)
        {
load_main_core:
          env = env->MCore;
          env->InstQuatum = ROUNDROBIN_EXECUTION_SLICE; 
        }


                    /* reset soft MMU for next block (it can currently
                       only be set by a memory fault) */
                } /* for(;;) */
        } else {
            /* Reload env after longjmp - the compiler may have smashed all
             * local variables as longjmp is marked 'noreturn'. */
            env = cpu_single_env;
        }

    } /* for(;;) */

#if defined(TARGET_I386)
    /* restore flags in standard format */
    env->eflags = env->eflags | cpu_cc_compute_all(env, CC_OP)
        | (DF & DF_MASK);
#elif defined(TARGET_ARM)
    /* XXX: Save/restore host fpu exception state?.  */
#elif defined(TARGET_UNICORE32)
#elif defined(TARGET_SPARC)
#elif defined(TARGET_PPC)
#elif defined(TARGET_LM32)
#elif defined(TARGET_M68K)
    cpu_m68k_flush_flags(env, env->cc_op);
    env->cc_op = CC_OP_FLAGS;
    env->sr = (env->sr & 0xffe0)
              | env->cc_dest | (env->cc_x << 4);
#elif defined(TARGET_MICROBLAZE)
#elif defined(TARGET_MIPS)
#elif defined(TARGET_SH4)
#elif defined(TARGET_ALPHA)
#elif defined(TARGET_CRIS)
#elif defined(TARGET_S390X)
#elif defined(TARGET_XTENSA)
    /* XXXXX */
#else
#error unsupported target CPU
#endif

    /* fail safe : never use cpu_single_env outside cpu_exec() */
    cpu_single_env = NULL;
    return ret;
}
