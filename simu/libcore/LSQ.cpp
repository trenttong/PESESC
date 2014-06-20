/* License & includes {{{1 */
/*
   ESESC: Super ESCalar simulator
   Copyright (C) 2010 University of California, Santa Cruz.

   Contributed by Luis Ceze

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

#include "LSQ.h"
#include "GProcessor.h"
#include "SescConf.h"
/* }}} */

LSQFull::LSQFull(const int32_t id)
  /* constructor {{{1 */
  :LSQ()
  ,stldForwarding("P(%d):stldForwarding", id)
{
}
/* }}} */

void LSQFull::insert(DInst *dinst)
  /* Insert dinst in LSQ (in-order) {{{1 */
{
  I(dinst->getAddr());
  instMap.insert(std::pair<AddrType, DInst *>(calcWord(dinst),dinst));
}
/* }}} */

DInst *LSQFull::executing(DInst *dinst)
  /* dinst got executed (out-of-order) {{{1 */
{
  I(dinst->getAddr());

  AddrType tag = calcWord(dinst);
  AddrDInstQMap::const_iterator instIt = instMap.begin();
  I(instIt != instMap.end());

  I(!dinst->isExecuted());

  const Instruction *inst = dinst->getInst();

  DInst *faulty = 0;
  while(instIt != instMap.end()) {
    if (instIt->first != tag){
      instIt++;
      continue; 
    }
    //inst->dump("Executed");
    DInst *qdinst = instIt->second;
    if(qdinst == dinst) {
      instIt++;
      continue;
    }

    const Instruction *qinst  = qdinst->getInst();

    //bool beforeInst = qdinst->getID() < dinst->getID();
    bool oooExecuted = qdinst->getID() > dinst->getID();
    if(oooExecuted){

      if(qdinst->isExecuted() && qdinst->getPC() != dinst->getPC()) { 

        if(inst->isStore() && qinst->isLoad()) { 
          if (faulty == 0)
            faulty = qdinst; 
          else if (faulty->getID() < qdinst->getID())
            faulty = qdinst;
        }
      }
    }else{
      if (!dinst->isLoadForwarded() && inst->isLoad() && qinst->isStore() && qdinst->isExecuted()) {
        dinst->setLoadForwarded();
        stldForwarding.inc(dinst->getStatsFlag());
      }
    }

    instIt++;
  }


  I(!dinst->isExecuted()); // first clear, then mark executed
  return faulty;
}
/* }}} */

void LSQFull::remove(DInst *dinst)
  /* Remove from the LSQ {{{1 (in-order) */
{
  I(dinst->getAddr());

 //const Instruction *inst = dinst->getInst();

  std::pair<AddrDInstQMap::iterator,AddrDInstQMap::iterator> rangeIt;
  //rangeIt = instMap.equal_range(calcWord(dinst));  
  AddrDInstQMap::iterator instIt = instMap.begin();

  //for(AddrDInstQMap::iterator it = rangeIt.first; it != rangeIt.second ; it++) {
  while(instIt != instMap.end()){
    if(instIt->second == dinst) {
      instMap.erase(instIt);
      return;
    }
    instIt++;
  }
}
/* }}} */

LSQNone::LSQNone(const int32_t id)
  /* constructor {{{1 */
  :LSQ() {
}
/* }}} */

void LSQNone::insert(DInst *dinst)
  /* Insert dinst in LSQ (in-order) {{{1 */
{
}
/* }}} */

DInst *LSQNone::executing(DInst *dinst)
  /* dinst got executed (out-of-order) {{{1 */
{
  return 0;
}
/* }}} */

void LSQNone::remove(DInst *dinst)
  /* Remove from the LSQ {{{1 (in-order) */
{
}
/* }}} */

LSQVPC::LSQVPC()
  /* constructor {{{1 */
  :LSQ() 
  ,LSQVPC_replays("LSQVPC_replays")  
{
}
/* }}} */

void LSQVPC::insert(DInst *dinst)
  /* Insert dinst in LSQ (in-order) {{{1 */
{
  I(dinst->getAddr());
  instMap.insert(std::pair<AddrType, DInst *>(calcWord(dinst),dinst));
}
/* }}} */

DInst *LSQVPC::executing(DInst *dinst)
{
  I(0);
  return 0;
}

AddrType LSQVPC::replayCheck(DInst *dinst)//return non-zero if replay needed
  /* dinst got executed (out-of-order) {{{1 */
{
  AddrType tag = calcWord(dinst);
  std::multimap<AddrType, DInst*>::iterator instIt; 
  //instIt = instMap.begin();
  instIt = instMap.find(tag);
  //AddrType storefound = 0;
  //while(instIt != instMap.end()){
  while(instIt->first==tag){
    if(instIt->first != tag){
      instIt++;
      continue;
    }
    if(instIt->second->getID() < dinst->getID()){
      if(instIt->second->getAddr()==dinst->getAddr()){
        LSQVPC_replays.inc(dinst->getStatsFlag());
        return 1; 
      }
    }
    //storefound = instIt->second->getPC();
    //if(instIt->second->getData()==dinst->getData()){
    //  return 0; //no replay needed
    //}
    instIt++;
  }
  return 0;
  //return storefound;
}
/* }}} */

void LSQVPC::remove(DInst *dinst)
  /* Remove from the LSQ {{{1 (in-order) */
{
  I(dinst->getAddr());
  std::multimap<AddrType, DInst*>::iterator instIt; 
  //instIt = instMap.begin();
  AddrType tag = calcWord(dinst);
  instIt = instMap.find(tag);
  //while(instIt != instMap.end()){
  while(instIt->first==tag){
    if(instIt->second == dinst) {
      instMap.erase(instIt);
      return;
    }
    instIt++;
  }
}
