/* 
   ESESC: Super ESCalar simulator
   Copyright (C) 2010 University of California, Santa Cruz.

   Contributed by Jose Renau
                  Smruti Sarangi
                  Milos Prvulovic

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

#ifndef BPRED_H
#define BPRED_H

/*
 * The original Branch predictor code was inspired in simplescalar-3.0, but
 * heavily modified to make it more OOO. Now, it supports more branch predictors
 * that the standard simplescalar distribution.
 *
 * Supported branch predictors models:
 *
 * Oracle, NotTaken, Taken, 2bit, 2Level, 2BCgSkew
 *
 */

#include "nanassert.h"
#include "estl.h"

#include "GStats.h"
#include "DInst.h"
#include "CacheCore.h"
#include "SCTable.h"
#include "iostream"

#define RAP_T_NT_ONLY   1

enum PredType {
  CorrectPrediction = 0,
  NoPrediction,
  NoBTBPrediction,
  MissPrediction
};

class BPred {
public:
  typedef unsigned long long HistoryType;
  class Hash4HistoryType {
  public: 
    size_t operator()(const HistoryType &addr) const {
      return ((addr) ^ (addr>>16));
    }
  };

  typedef boost::dynamic_bitset<> LongHistoryType; 

protected:
  const int32_t id;

  GStatsCntr nHit;  // N.B. predictors should not update these counters directly
  GStatsCntr nMiss; // in their predict() function.

  int32_t bpred4Cycle;
  int32_t bpred4CycleAddrShift;

  HistoryType calcHist(AddrType pc) const {
    HistoryType cid = pc>>2; // psudo-PC works, no need lower 2 bit

    // Remove used bits (restrict predictions per cycle)
    cid = cid>>bpred4CycleAddrShift;
    // randomize it
    cid = (cid >> 17) ^ (cid); 

    return cid;
  }
protected:
public:
  BPred(int32_t i, int32_t fetchWidth, const char *section, const char *name);
  virtual ~BPred();

  virtual PredType predict(DInst *dinst, bool doUpdate) = 0;

  PredType doPredict(DInst *dinst, bool doUpdate) {
    PredType pred = predict(dinst, doUpdate);
    if (!doUpdate || pred == NoPrediction)
      return pred;

    nHit.inc(pred == CorrectPrediction);
    nMiss.inc(pred == MissPrediction);

    return pred;
  }
  
};


class BPRas : public BPred {
private:
  const uint16_t RasSize;

  AddrType *stack;
  int32_t index;
protected:
public:
  BPRas(int32_t i, int32_t fetchWidth, const char *section);
  ~BPRas();
  PredType predict(DInst *dinst, bool doUpdate);

};

class BPBTB : public BPred {
private:

  class BTBState : public StateGeneric<AddrType> {
  public:
    BTBState(int32_t lineSize) {
      inst = 0;
    }

    AddrType inst;

    bool operator==(BTBState s) const {
      return inst == s.inst;
    }
  };

  typedef CacheGeneric<BTBState, AddrType> BTBCache;
  
  BTBCache *data;
  
protected:
public:
  BPBTB(int32_t i, int32_t fetchWidth, const char *section, const char *name=0);
  ~BPBTB();

  PredType predict(DInst *dinst, bool doUpdate);
  void updateOnly(DInst *dinst);

};

class BPOracle : public BPred {
private:
  BPBTB btb;

protected:
public:
  BPOracle(int32_t i, int32_t fetchWidth, const char *section)
    :BPred(i, fetchWidth, section, "Oracle")
    ,btb(i, fetchWidth, section) {
  }

  PredType predict(DInst *dinst, bool doUpdate);

};

class BPNotTaken : public BPred {
private:
  BPBTB btb;

protected:
public:
  BPNotTaken(int32_t i, int32_t fetchWidth, const char *section)
    :BPred(i, fetchWidth, section, "NotTaken") 
    ,btb(  i, fetchWidth, section) {
    // Done
  }

  PredType predict(DInst *dinst, bool doUpdate);

};

class BPNotTakenEnhanced : public BPred {
private:
  BPBTB btb;

protected:
public:
  BPNotTakenEnhanced(int32_t i, int32_t fetchWidth, const char *section)
    :BPred(i, fetchWidth, section, "NotTakenEnhanced")
    ,btb(  i, fetchWidth, section) {
    // Done
  }

  PredType predict(DInst *dinst, bool doUpdate);

};

class BPTaken : public BPred {
private:
  BPBTB btb;

protected:
public:
  BPTaken(int32_t i, int32_t fetchWidth, const char *section)
    :BPred(i, fetchWidth, section, "Taken") 
    ,btb(  i, fetchWidth, section) {
    // Done
  }

  PredType predict(DInst *dinst, bool doUpdate);

};

class BP2bit:public BPred {
private:
  BPBTB btb;

  SCTable table;
protected:
public:
  BP2bit(int32_t i, int32_t fetchWidth, const char *section);

  PredType predict(DInst *dinst, bool doUpdate);

};

class BP2level:public BPred {
private:
  BPBTB btb;

  const uint16_t l1Size;
  const uint32_t  l1SizeMask;

  const uint16_t historySize;
  const HistoryType historyMask;

  SCTable globalTable;

  HistoryType *historyTable; // LHR
protected:
public:
  BP2level(int32_t i, int32_t fetchWidth, const char *section);
  ~BP2level();

  PredType predict(DInst *dinst, bool doUpdate);

};

class BPHybrid:public BPred {
private:
  BPBTB btb;

  const uint16_t historySize;
  const HistoryType historyMask;

  SCTable globalTable;

  HistoryType ghr; // Global History Register

  SCTable localTable;
  SCTable metaTable;

protected:
public:
  BPHybrid(int32_t i, int32_t fetchWidth, const char *section);
  ~BPHybrid();

  PredType predict(DInst *dinst, bool doUpdate);

};

class BP2BcgSkew : public BPred {
private:
  BPBTB btb;

  SCTable BIM;

  SCTable G0;
  const uint16_t       G0HistorySize;
  const HistoryType  G0HistoryMask;

  SCTable G1;
  const uint16_t       G1HistorySize;
  const HistoryType  G1HistoryMask;

  SCTable metaTable;
  const uint16_t       MetaHistorySize;
  const HistoryType  MetaHistoryMask;

  HistoryType history;
protected:
public:
  BP2BcgSkew(int32_t i, int32_t fetchWidth, const char *section);
  ~BP2BcgSkew();
  
  PredType predict(DInst *dinst, bool doUpdate);

};

class BPyags : public BPred {
private:
  BPBTB btb;

  const uint16_t historySize;
  const HistoryType historyMask;

  SCTable table;
  SCTable ctableTaken;
  SCTable ctableNotTaken;

  HistoryType ghr; // Global History Register

  uint8_t    *CacheTaken;
  HistoryType CacheTakenMask;
  HistoryType CacheTakenTagMask;

  uint8_t    *CacheNotTaken;
  HistoryType CacheNotTakenMask;
  HistoryType CacheNotTakenTagMask;

protected:
public:
  BPyags(int32_t i, int32_t fetchWidth, const char *section);
  ~BPyags();
  
  PredType predict(DInst *dinst, bool doUpdate);

};

class BPOgehl : public BPred {
private:
  BPBTB btb;
  
  const int32_t mtables;
  const int32_t glength;
  const int32_t nentry;
  const int32_t addwidth;
  int32_t logpred;

  int32_t THETA;
  int32_t MAXTHETA;
  int32_t THETAUP;
  int32_t PREDUP;

  long long *ghist;
  int32_t *histLength;
  int32_t *usedHistLength;
  
  int32_t *T;
  int32_t AC;  
  int32_t miniTag;
  char *MINITAG;
  
  char **pred;
  int32_t TC;
protected:
  int32_t geoidx(long long Add, long long *histo, int32_t m, int32_t funct);
public:
  BPOgehl(int32_t i, int32_t fetchWidth, const char *section);
  ~BPOgehl();
  
  PredType predict(DInst *dinst, bool doUpdate);

};

#if 1
class BPSOgehl : public BPred {
private:
  BPBTB btb;
  
  const int32_t mtables;
  const int32_t glength;
  const int32_t AddWidth;
  int32_t logtsize;

  int32_t THETA;
  int32_t MAXTHETA;
  int32_t THETAUP;
  int32_t PREDUP;

  LongHistoryType ghr;
  int32_t *histLength;
  int32_t *usedHistLength;
  
  int32_t *T;
  int32_t AC;  
  int32_t miniTag;
  char *MINITAG;
  
  char **pred;
  int32_t TC;
protected:
  uint32_t geoidx2(long long Add, int32_t m);
public:
  BPSOgehl(int32_t i, int32_t fetchWidth, const char *section);
  ~BPSOgehl();
  
  PredType predict(DInst *dinst, bool doUpdate);

};
#endif

class BPredictor {
private:
  const int32_t id;
  const bool SMTcopy;

  BPRas ras;
  BPred *pred;
  
  GStatsCntr nBranches;
  GStatsCntr nTaken;
  GStatsCntr nMiss;           // hits == nBranches - nMiss

  char *section;

protected:
public:
  BPredictor(int32_t i, int32_t fetchWidth, const char *section, BPredictor *bpred=0);
  ~BPredictor();

  static BPred *getBPred(int32_t id, int32_t fetchWidth, const char *sec);

  PredType predict(DInst *dinst, bool doUpdate) {
    I(dinst->getInst()->isControl());

#if 0
    printf("BPRED: pc: %x ", dinst->getPC() );
    printf(" fun call: %d, ", dinst->getInst()->isFuncCall()); printf("fun ret: %d, ", dinst->getInst()->isFuncRet());printf("taken: %d, ", dinst->isTaken());  printf("target addr: 0x%x\n", dinst->getAddr());
#endif

    nBranches.inc(doUpdate);
    nTaken.inc(dinst->isTaken());
  
    PredType p= ras.doPredict(dinst, doUpdate);
    if( p != NoPrediction ) {
      nMiss.inc(p != CorrectPrediction && doUpdate);
      return p;
    }

    p = pred->doPredict(dinst, doUpdate);

    // Overall stats
    nMiss.inc(p != CorrectPrediction && doUpdate);

    return p;
  }

  void dump(const char *str) const;
};

#endif   // BPRED_H
