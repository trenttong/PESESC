/* License & includes {{{1 */
/* 
   ESESC: Enhanced Super ESCalar simulator
   Copyright (C) 2009 University of California, Santa Cruz.

   Contributed by Gabriel Southern
                  Jose Renau
                  Ehsan K.Ardestani

This file is part of ESESC.

ESESC is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

ESESC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
ESESC; see the file COPYING. If not, write to the Free Software Foundation, 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <cstring>
#include "PowerModel.h"
#include "SescConf.h"
#include "Report.h"
#include "GMemorySystem.h"
#include "GProcessor.h"

#ifdef ENABLE_PEQ
#include "PeqParser.h"
#include "SRAM.h"
#endif
/* }}} */

PowerModel::PowerModel():
  /* constructor {{{1 */
  powerTime("powerTime")
{

  activity            = new std::vector<uint32_t>();
  updateInterval      = 0;
  instCountCurrent    = 0;
  instCountPrev       = 0;
  bPredHitCurrent     = 0;
  bPredMissCurrent    = 0;
  bPredHitPrev        = 0;
  bPredMissPrev       = 0;
  logfile             = 0;
  nSamples            = 0;
  totalPow            = 0;
  meanPow             = 0;
  stdevPow            = 0;
  headPtr             = 10030102;                                   // very large number to force initialization to zero
  downSample          = 1;

  mcpatWrapper        = new Wrapper();
  sescThermWrapper    = new SescThermWrapper();

  stats               = new std::vector<PowerStats *>();
  sysConn             = new std::vector<std::string>();
  coreConn            = new std::vector<std::string>();
  pwrCntrRepo         = new std::map<float, std::vector<float> >();
  ipcRepo             = new std::map<float, float>();
  avgEnergyCntrValues = new std::vector<float>();

  temperatures        = new std::vector<float>();

  energyBundle       = new ChipEnergyBundle(); 


}
/* }}} */

void PowerModel::plug(const char* section) 
  /* plug {{{1 */
{
  I(sescThermWrapper);
  I(pwrCntrRepo);
  I(ipcRepo);
  I(temperatures);
  I(mcpatWrapper);
  I(energyBundle);

/*test code start */
#ifdef ENABLE_PEQ
  PeqParser peq_parser;
  peq_parser.getGeneralParams();
  peq_parser.getCoreParams();
  peq_parser.getMemParams();
  peq_parser.testParser();
  SRAM my_sram;
  my_sram.testSRAM();
#endif
/*test code end */

  avgTimingIPC  = 1;

  powerGlue.plug(section, energyBundle);

  logfile = 0;
  if (SescConf->checkCharPtr(section, "logfile")) {
    filename = SescConf->getCharPtr(section, "logfile");
    logfile = fopen(filename, "w");
    GMSG(logfile == 0, "ERROR: could not open logfile \"%s\" (ignoring it)", filename);
  }

 


  powerGlue.createStatCounters();
  //checkStatCounters(section);
  //

  //Parse conf file to get stats information
  //Format in conf file is: stats[i] = pwrstat +gstat1 -gstat2 ...
  readStatsFromConfFile(section);
  
  genIndeces();


  readPowerConfig(section);
  initPowerModel(section);
  readTempConfig(section);
  initTempModel(section);


  //plugStackingValidator();

  

#ifdef ENABLE_CUDA
  initGPUCfgSearch();
#endif

//Initialize logfile
  if (logfile == NULL) return;

  avgPowerValid = false;
  fprintf(logfile, "#IPC\t");
  for (size_t i = 0; i < stats->size(); i++) {
    fprintf(logfile, "%s\t", stats->at(i)->getName());
  }
  fprintf(logfile, "Total access all\n");




}
/* }}} */

void PowerModel::unplug() 
  /* unplug {{{1 */
{
  if (logfile != NULL)
    fclose(logfile);
  stopDump();
}
/* }}} */

void PowerModel::updateActivity(uint64_t timeinterval, FlowID fid)
  /* updateActivity {{{1 */
{
  // Update stats that are passed to McPat
  timeInterval = timeinterval;
  PowerStats *pwrstat = 0;
  uint32_t zcnt = 0;
  for(size_t i        = 0; i < stats->size(); i++) {
    pwrstat             = (*stats)[i];
    (*activity)[i]     = pwrstat->getActivity();
    if (!(*activity)[i])
      zcnt++;
  }
  dumpActivity();

  static int pcnt = 0;
  if ((2*zcnt > stats->size())) {
    if (pcnt++ < 20)
      printf("WARNING: Too many blocks have zero activity. Check the eSESC to McPAT mapping.\n");
  }

  // Update internal performance counters
  GStats *gref = 0;
  char str[128];

  for (size_t i=0; i<ncores; i++) {

    sprintf(str,"S(%lu):globalClock_Timing", i);
    gref = GStats::getRef(str); 
    I(gref);
    uint64_t cticks = gref->getSamples(); // clockTicks
    if (cticks - clockPrev[i] == 0) {
      I(0);
      clockInterval[i]  = 0;
    }else{
      clockInterval[i]    =  cticks - clockPrev[i]; // + 1000;
    }
    clockPrev[i]        =  cticks;
  }

  // Just use the total stats, not average
  avgTimingIPC = static_cast<float>(1.0)/static_cast<float>(getEmul(fid)->getSampler()->getMeaCPI());
  ipc = avgTimingIPC;
}
/* }}} */

void PowerModel::printStatus()
  /* printStatus {{{1 */
{
  I(logfile);
  fprintf(logfile, "%2.3f\t", ipc);
  uint32_t value = 0;
  uint32_t valueSum = 0;
  for (size_t i = 0; i < stats->size(); i++) {
    value = activity->at(i);
    valueSum+=value;
    fprintf(logfile, "%u\t", value);
  }
  fprintf(logfile, "%d\n", valueSum);      
}
/* }}} */


int PowerModel::calcStats(uint64_t timeinterval, bool keepPower, FlowID fid)
/* calcStats {{{1 */
{
  // This is called through sampler. So the power/thermal
  // simulator are called explicitly rather than periodically by
  // a timer.
  //
  //need to sync stats first

#ifdef ENABLE_CUDA
  loadPerKernelCfg();
#endif

  int return_signal = 0; // check for 90s simulated time to finish
 
  energyBundle->setFreq(getFreq());



  if (!keepPower) {     // Calculate new Power 
    updateActivity(timeinterval, fid);
    // Dump eSESC performance counters to file
    if (logfile)
      printStatus();
    
    mcpatWrapper->calcPower(activity, energyBundle, &clockInterval);

    //Turbo
    if (enableTurbo)
      updateTurboState();
    //FIXME: else i should copy lkg to scaled lkg

    //dumpTotalPower("totalp");


    if (doPowPred) {
      updatePowerHist();
      loadPredPower();
    }
  }

  //dumpTotalPower("totalpf");




  uint32_t throttleLength = 0;
  if (doTherm) {
    if (energyBundle->cntrs.size()>0){
      return_signal = sescThermWrapper->calcTemp(energyBundle, temperatures, timeInterval, throttleLength);
    }
  }

  updatePowerGStats();
  dumpTotalPower("totalpTh");
  double tp = getLastTotalPower();
#ifdef ENABLE_CUDA
  double ed = getCurrentED();
  printf("total Power TH:%f, ED:%f, Vol:%f  timeinterval=%lu\n", tp, ed, volNTC, (long unsigned int) timeinterval);
#else
  printf("total Power TH:%f, Vol:%f  timeinterval=%lu\n", tp, volNTC, (long unsigned int) timeinterval);
#endif

  // Thermal Throttling
  throttle(fid, throttleLength);
  updatePowerTime(timeInterval);


  //stackingValidator();


#ifdef ENABLE_CUDA
  adapt();
#endif

  if (dumppwth && logpwrdyn/* && tc > 0*/) {
    dumpDynamic();
    if (logprf)
      dumpPerf(false);
  }
  return(return_signal);
}
/* }}} */

void PowerModel::testWrapper()
  /* testWrapper {{{1 */
{
  LOG("Calling mcpatWrapper->calcPower");
  mcpatWrapper->calcPower(activity, energyBundle, &clockInterval);
  
  LOG("Calling sescThermWrapper->calcTemp");
  uint32_t throttleLength;
  sescThermWrapper->calcTemp(energyBundle, temperatures, static_cast<uint64_t>(updateInterval), throttleLength);
}
/* }}} */

void PowerModel::dumpPerf(bool pwrReuse)
  /* dumpPerf {{{1 */
{
  I(logprf);
  if (totalPowerSamples < 2)  
    fprintf(logprf, "Time\t IPC\t Cycles\t bPredHit\t bPredMiss\t avgMemLat\n");
  // dump some performance statistics as well for verification
  //fprintf(logprf, "%e\t", sescThermWrapper->sesctherm.temp_model.get_time());
    if (doTherm)
      fprintf(logprf, "%e\t", sescThermWrapper->sesctherm.temp_model.get_time());
    else
      fprintf(logprf, "%e\t", getPowerTime());
  fprintf(logprf, "%e\t", ipc);
  fprintf(logprf, "%e\t", static_cast<double>(clockInterval[0]));
  fprintf(logprf, "%e\t", static_cast<double>(bPredHitCurrent - bPredHitPrev));
  fprintf(logprf, "%e\t", static_cast<double>(bPredMissCurrent - bPredMissPrev));
  if (pwrReuse) 
    fprintf(logprf, "%e  PR \n", avgMemLat);
  else
    fprintf(logprf, "%e \n", avgMemLat);

  fflush(logprf);
}
/* }}} */

void PowerModel::dumpDynamic()
  /* dumpDynamic {{{1 */
{
  // eka, to dump unmapped power values
  totalPowerSamples++;
  if (totalPowerSamples == 1) { // also write the header
    for (size_t ii = 0; ii < energyBundle->cntrs.size(); ii++) {
      fprintf(logpwrdyn, "%s\t", energyBundle->cntrs[ii].getName());
    }
    fprintf(logpwrdyn, "\n");
  }
  for (size_t j = 0; j < energyBundle->cntrs.size(); j++) {
    fprintf(logpwrdyn, "%e\t", energyBundle->cntrs[j].getDyn());
  }
  fprintf(logpwrdyn, "\n");
  fflush(logpwrdyn);
}
/* }}} */

void PowerModel::dumpLeakage()
  /* dumpLeakage {{{1 */
{
  // eka, For each interval, cal this function if you want 
  // have the power dumped into a different file

  // elnaz, separated dump leakage from dynamic power

  if (dumppwth) {
    char *fname_pwr_lkg = static_cast<char *>(malloc(1023));
    sprintf(fname_pwr_lkg, "power_lkg_%s", Report::getNameID());
    logpwrlkg = fopen(fname_pwr_lkg, "w");  
    GMSG(logpwrlkg == 0, "ERROR: could not open logpwrlkg file \"%s\" (ignoring it)", fname_pwr_lkg);
    free(fname_pwr_lkg);
    for (size_t i = 0; i < energyBundle->cntrs.size(); i++) {
      fprintf(logpwrlkg, "%e\t", energyBundle->cntrs[i].getLkg());
    }
    fprintf(logpwrlkg, "\n");
    if (logpwrlkg) {
      fclose(logpwrlkg);
      logpwrlkg = NULL;
    }
  }
}
/* }}} */

void PowerModel::dumpDeviceTypes()
  /* dumpDeviceTypes {{{1 */
{
  // elnaz, dump device types as specified in .xml file of mcpat 
  if (dumppwth) {
    char *fname_devTypes = static_cast<char *>(malloc(1023));
    sprintf(fname_devTypes, "deviceTypes_%s", Report::getNameID());
    logDeviceTypes = fopen(fname_devTypes, "w");  
    GMSG(logDeviceTypes == 0, "ERROR: could not open logDeviceTypes file \"%s\" (ignoring it)", fname_devTypes);
    free(fname_devTypes);


    for (size_t i = 0; i < energyBundle->cntrs.size(); i++) {
      fprintf(logDeviceTypes, "%i\t", energyBundle->cntrs[i].getDevType());
    }
    fprintf(logDeviceTypes, "\n");
    if (logDeviceTypes) {
      fclose(logDeviceTypes);
      logDeviceTypes = NULL;
    }
  }
}
/* }}} */

void PowerModel::getPowerDumpFile()
  /* getPowerDumpFile {{{1 */
{
  dumppwthSplit = true;
  totalPowerSamples = 0;
  nPowerCall++;
  if (dumppwth && dumppwthSplit) {
    char *fname_prf           = static_cast<char *>(malloc(1023));
    char *fname_pwr_dyn       = static_cast<char *>(malloc(1023));
    sprintf(fname_prf,     "perf_%d_%s",      nPowerCall, Report::getNameID());
    sprintf(fname_pwr_dyn, "power_dyn_%d_%s", nPowerCall, Report::getNameID());
    logprf         = fopen(fname_prf, "w");
    logpwrdyn      = fopen(fname_pwr_dyn, "w");
    GMSG(logprf    == 0, "ERROR: could not open logprf file \"%s\" (ignoring it)",    fname_prf);
    GMSG(logpwrdyn == 0, "ERROR: could not open logpwrdyn file \"%s\" (ignoring it)", fname_pwr_dyn);
    free(fname_pwr_dyn);
    free(fname_prf);
  }
}
/* }}} */

void PowerModel::closePowerDumpFile()
  /* closePowerDumpFile {{{1 */
{
  // close last opened file if it exists
  if (logprf) {
    fclose(logprf);
    logprf = NULL;
  }
  if (logpwrdyn) {
    fclose(logpwrdyn);
    logpwrdyn = NULL;
  }
  dumppwthSplit = false;


  if (dpowerf)
    fclose(dpowerf);
}
/* }}} */

void PowerModel::startDump()
  /* startDump {{{1 */
{
  getPowerDumpFile();
}
/* }}} */

void PowerModel::stopDump()
  /* stopDump {{{1 */
{
  closePowerDumpFile();
}
/* }}} */


/* }}} */
float PowerModel::getLastTotalPower(){
  float totalpower = 0;
  for (size_t j = 0; j < energyBundle->cntrs.size(); j++) {
    totalpower += energyBundle->cntrs[j].getDyn();
    totalpower += energyBundle->cntrs[j].getScaledLkg();
  }
  return totalpower;
}
float PowerModel::getLastDynPower(){
  float totalpower = 0;
  for (size_t j = 0; j < energyBundle->cntrs.size(); j++) {
    //printf("Name:%s, Power:%e\n", energyBundle->cntrs[j].getName(), energyBundle->cntrs[j].dyn);
    totalpower += energyBundle->cntrs[j].getDyn();
  }
  return totalpower;
}
PowerModel::~PowerModel()
  /* destructor {{{1 */
{
  closePowerDumpFile()     ;

/*
  if (activity != NULL)
    free(activity);

  if (mcpatWrapper != NULL)
    delete (mcpatWrapper      );  

  if (sescThermWrapper != NULL)
    delete (sescThermWrapper  );  

  if (stats != NULL)
    delete (stats             );  

  if (energyCntrNames != NULL)
    delete (energyCntrNames   );  

  if (pwrCntrRepo != NULL)
    delete pwrCntrRepo       ); 

  if (ipcRepo != NULL)
    delete (ipcRepo           ); 
  if (leakageCntrValues != NULL)
    delete (leakageCntrValues );   

  if (avgEnergyCntrValues != NULL)
    delete (avgEnergyCntrValues); 

  if (deviceTypes != NULL)
    delete (deviceTypes       );

  if (temperatures != NULL)
    free(temperatures      );
*/
}
/* }}} */

void PowerModel::updatePowerHist(){

  static size_t validP = 0;

  //reset buffer
  for (size_t j = 0; j<powerHist[0].size();j++)
    predPow[j] = 0;

  // circular head pointer handling 
  if(headPtr >= PowerHistSize-1)
    headPtr = 0;
  else
    headPtr++;

  //copy last power and advance head pointer, and update current totalPow
  totalPow    = 0.0;
  for (size_t j  = 0; j<powerHist[0].size();j++){
    powerHist[headPtr][j] = energyBundle->cntrs[j].getDyn();   // assuming this contain the average of last timing mode
    totalPow             += energyBundle->cntrs[j].getDyn();
  }
 
  //save total Power in History
  totalPowHist[headPtr] = totalPow;

  // sum  history powers up
  for(size_t i=0; i< PowerHistSize;i++){
    for (size_t j = 0; j<powerHist[0].size();j++){
      predPow[j]           += powerHist[i][j];
    }
  }


  float d = PowerHistSize;
  if (validP<PowerHistSize){
    validP++;
    d = static_cast<float>(validP);
  }
  //printf("# of samples:%d, d:%f, heatPtr:%d", validP, d, headPtr);
  //predicted power for skip mode (R+W), average of the history window
  for (size_t j = 0; j<powerHist[0].size();j++)
    predPow[j] /= d;

  //update meanPow for total power
  meanPow    = 0.0;
  for (size_t j = 0; j<powerHist[0].size();j++)
    meanPow   += predPow[j];

  //update stdevPow
  stdevPow   = 0.0;
  for (size_t j = 0; j<PowerHistSize;j++)
    stdevPow   = stdevPow + (meanPow - totalPowHist[j])*(meanPow - totalPowHist[j]);
  stdevPow /= (PowerHistSize-1);
  stdevPow = sqrt(stdevPow);

/*
  //float adj = 2*1.25*((*temperatures)[6]/307-1);
  //printf("stdv: %e\t", stdevPow);
  float adj  = 0.70*stdevPow;
  //if (stdevPow > 0.4)
    adj = 0;
  
  for (size_t j = 0; j<powerHist[0].size();j++)
    predPow[j] = predPow[j] * 1.010 + adj;
    */
}

void PowerModel::loadPredPower() {
  I(doPowPred);

  if (!usePrediction)
    for (size_t j = 0; j<powerHist[0].size();j++)
      energyBundle->cntrs[j].setDyn( powerHist[headPtr][j] );
  else
    for (size_t j = 0; j<powerHist[0].size();j++)
      energyBundle->cntrs[j].setDyn(predPow[j]);
}

bool PowerModel::isDeviationHigh(){
  if ((abs(totalPow - meanPow) > 1.5 * stdevPow) && 
      abs(meanPow - totalPow)> (0.1 * meanPow)){
    //printf("!!!!!!!!!!!!!!!!!!!!!!!!High Variation, mean=%f, std:%f, pow=%f\n", meanPow, stdevPow, totalPow);
    return true;
  }else
    return false;
}

void PowerModel::adjustSamplingRate(){
  // Adapt sampling rate
  static short change_conta = 0;
  if (isDeviationHigh()){ // More samples
    clearUsePrediction(); // Do not use the predicted power, last value instead
    downSample = 4;
    change_conta = PowerHistSize; 
  }else if (change_conta > 0){ // wait until history is fully updated
    clearUsePrediction(); // Do not use the predicted power, last value instead
    downSample = 4;
    change_conta --;
  }else{
    downSample = 1; // back to normal sampling rate
    setUsePrediction(); // Use predicted power
    change_conta = 0;
  }
}
/*void PowerModel::stdevOffst(bool subtract, float c){
  I(doPowPred);

  float sd = stdevPow;
  if (subtract)
    sd = -1*sd;

  for (int j = 0; j<powerHist[0].size();j++)
    energyBundle->cntrs[j].dyn  += (sd*c);
}*/



void PowerModel::dumpTotalPower(const char * str){
    char *fname_p           = static_cast<char *>(malloc(1023));
    sprintf(fname_p,     "%s_%s", str, Report::getNameID());
    FILE * tp = fopen(fname_p, "a");
    if (doTherm)
      fprintf(tp, "%2.7lf\t", sescThermWrapper->sesctherm.temp_model.get_time());
    else
      fprintf(tp, "%2.7lf\t", getPowerTime());

    fprintf(tp, "%f\t", getLastTotalPower());
    for(size_t i=0; i<ncores;i++)
      fprintf(tp, "%ld\t", (unsigned long) clockInterval[i]);
    fprintf(tp, "%ld\t", (unsigned long) timeInterval);
    fprintf(tp, "%ld\t", (unsigned long) getFreq());
    fprintf(tp,"\n");
    fclose(tp);
    free(fname_p);
}

void PowerModel::dumpActivity() {
    char *fname_p           = static_cast<char *>(malloc(1023));
    sprintf(fname_p,     "activity_%s", Report::getNameID());
    FILE * tp = fopen(fname_p, "a");

    static bool first = true;
    if (first) {
      for(size_t i = 0; i< activity->size();i++){
        fprintf(tp, "%s\t", stats->at(i)->getName());
      }
      fprintf(tp, " total  clockInterval\n");
      first = false;
    }

    if (doTherm)
      fprintf(tp, "%2.7lf\t", sescThermWrapper->sesctherm.temp_model.get_time());
    else
      fprintf(tp, "%2.7lf\t", getPowerTime());

    uint32_t tmp   = 0;
    for(size_t i = 0; i< activity->size();i++){
      tmp         += activity->at(i);
      fprintf(tp, "%d\t", activity->at(i));
    }

    fprintf(tp, "%d\t", tmp);
    fprintf(tp, "%ld \n", (unsigned long) clockInterval[0]);
    fclose(tp);
    free(fname_p);
}

void PowerModel::energy2power() {
  for (size_t k=0;k<energyBundle->cntrs.size();k++) {
    double adjusted = energyBundle->cntrs[k].getDyn();
    adjusted        = getFreq() * adjusted /clockInterval[0];
    energyBundle->cntrs[k].setDyn( adjusted );     
  }
}


bool PowerModel::throttle(FlowID fid, uint32_t throttleLength) {

  static uint32_t throttleLengthPrev = 0;

  uint32_t tc = throttleLength - throttleLengthPrev;
  uint32_t throttleCycles = tCycQuanta*tc * samplingRatio;
  if (tc > 0) {
    MSG("Thermal throttling cpu ?? (0) @%lld, %f %d", static_cast<long long>(globalClock), throttleCycles, throttleLength);
    freeze(fid, static_cast<Time_t>(throttleCycles));
  }
  throttleLengthPrev = throttleLength;

  timeInterval +=  tCycQuanta*tc;

  return tc > 0? true:false;
}


void PowerModel::initPowerGStats() {
  char str[256];
  for(size_t i=0; i<energyBundle->cntrs.size();i++) {
    sprintf(str, "pwrDyn%s",energyBundle->cntrs[i].getName()); 
    powerDynCntr.push_back(new GStatsAvg(str));
    sprintf(str, "pwrLkg%s",energyBundle->cntrs[i].getName()); 
    powerLkgCntr.push_back(new GStatsAvg(str));
    sprintf(str, "energy%s",energyBundle->cntrs[i].getName()); 
    energyCntr.push_back(new GStatsAvg(str));
    sprintf(str, "engDelay%s",energyBundle->cntrs[i].getName()); 
    engDelayCntr.push_back(new GStatsAvg(str));
    sprintf(str, "currentEngDelay%s",energyBundle->cntrs[i].getName()); 
    currentEngDelayCntr.push_back(new GStatsAvg(str));
    
    
    sprintf(str, "area%s",energyBundle->cntrs[i].getName()); 
    areaCntr.push_back(new GStatsCntr(str));
    areaCntr[i]->add(energyBundle->cntrs[i].getArea()*10e9);

  }
}

void PowerModel::updatePowerGStats() {

  static uint32_t currentWin = 0;

  for(size_t i=0; i< energyBundle->cntrs.size(); i++) {
    if (clockInterval[ncores-1] > 1 || !energyBundle->cntrs[i].isGPU()){
        powerDynCntr[i]->sample(1000*energyBundle->cntrs[i].getDyn());  
        powerLkgCntr[i]->sample(1000*energyBundle->cntrs[i].getScaledLkg());  
        energyCntr[i]->sample((energyBundle->cntrs[i].getScaledLkg() + energyBundle->cntrs[i].getDyn())*energyBundle->cntrs[i].getCyc()/1e3);  
        engDelayCntr[i]->sample((energyBundle->cntrs[i].getScaledLkg() + energyBundle->cntrs[i].getDyn())*pow(energyBundle->cntrs[i].getCyc(),2)/1e6);  
        if (currentWin){
          currentEngDelayCntr[i]->reset();
        }
        currentEngDelayCntr[i]->sample((energyBundle->cntrs[i].getScaledLkg() + energyBundle->cntrs[i].getDyn())*pow(energyBundle->cntrs[i].getCyc(),2)/1e6);  
    }
  }

  if (currentWin)
    currentWin = 0;

  currentWin++;
}


void PowerModel::updatePowerTime(uint64_t timeinterval) {
  powerTime.add(timeinterval);
}

float PowerModel::getMaxT() {
  float  maxT = 0.0;
  for (size_t i=0 ; i< temperatures->size(); i++) {
    if (maxT < (*temperatures)[i])
      maxT = (*temperatures)[i];
  }
  return maxT;
}

void PowerModel::readStatsFromConfFile(const char *section) {
  int nstats = SescConf->getRecordSize(section, "stats");
  activity->resize(nstats); 
  for(int i = 0; i < nstats; i++) {
    const char *stats_string = SescConf->getCharPtr(section, "stats", i);
    const char *start        = stats_string;
    const char *end          = stats_string;
    char *new_str = 0;
    int  len      = 0;
    int  scalar   = 0;

    // 1st, skip spaces
    while (std::isspace(*end)) end++;
    start = end;

    // 2nd, get 1st word
    while (!std::isspace(*end) && *end) {
      end++;
      len++;
    }

    if (new_str != 0)
      delete new_str;

    new_str = static_cast<char*>(alloca(len + 1));
    std::strncpy(new_str, start, len);
    new_str[len] = '\0';

    PowerStats *pwrstat = new PowerStats(new_str);

    while (*end) {
      len = 0;
      while (std::isspace(*end))
        end++;

      if (*end == '-')
        scalar = -1;
      else if (*end == '+')
        scalar = 1;
      else {
        MSG("ERROR: cannot parse powerstat string: '%s' (expecting + or -)", stats_string);
        exit(-1);
      }
      end++;

      while (std::isspace(*end))
        end++;

      start = end;     
      while (!std::isspace(*end) && *end) {
        len++;
        end++;
      }
      new_str = static_cast<char *>(std::malloc(len + 1));
      new_str[len] = '\0';
      std::strncpy(new_str, start, len);
      pwrstat->addGStat(new_str, scalar);
      std::free(new_str);
    }
    stats->push_back(pwrstat);
  }
}

void PowerModel::genIndeces() {
  coreIndex = new std::vector<uint32_t>();
  gpuIndex = new std::vector<uint32_t>();
  for (size_t i = 0; i < getFlowIDEmulMapping()->size(); i++){
    if ((*getFlowIDEmulMapping())[i])
      gpuIndex->push_back(i);
    else
      coreIndex->push_back(i);
  }
}


void PowerModel::initPowerModel(const char *section) {
  MSG("Initialize McPAT/CACTI");
  //mcpatWrapper->plug(section, activity, energyCntrNames, leakageCntrValues, deviceTypes, energyCntrAreas, energyCntrTDPs, sysConn, coreConn, &coreEIdx, &ncores, &nL2, &nL3, coreIndex, gpuIndex, true);
  mcpatWrapper->plug(section, activity, energyBundle, &coreEIdx, &ncores, &nL2, &nL3, coreIndex, gpuIndex, false);
  I(ncores == (uint32_t)SescConf->getRecordSize("","cpusimu"));
  clockPrev.resize(ncores);
  clockInterval.resize(ncores);
  temperatures->resize(energyBundle->cntrs.size(), INITIAL_TEMP);

  powerGlue.dumpFlpDescr(coreEIdx);
  initPowerGStats();
  dumppwth   = 0; 
  dumppwth   = SescConf->getBool(section, "dumpPower", 0);
  dumppwthSplit = dumppwth;
  dumpLeakage();
  dumpDeviceTypes();
  getPowerDumpFile();
}


void PowerModel::initTempModel(const char* section) {
  if (doTherm) {
    MSG("Initialize sesctherm");
    //sescThermWrapper->plug(section, energyCntrNames, leakageCntrValues, deviceTypes);
    sescThermWrapper->plug(section, energyBundle);
  }
}

void PowerModel::readPowerConfig(const char *section) {

  initTurboState(); 

  updateInterval = static_cast<uint32_t>(SescConf->getDouble(section, "updateInterval"));
  printf("update Interval = %d",updateInterval);
  freq      = SescConf->getDouble("technology","frequency");
  const char *sectionEmu   = SescConf->getCharPtr("", "cpuemul", 0);
  const char *sampler_sec  = SescConf->getCharPtr(sectionEmu, "sampler");
  const char *sampler_type = SescConf->getCharPtr(sampler_sec, "type");
  if (strcasecmp(sampler_type,"Adaptive") == 0){
    doPowPred  = SescConf->getBool(sampler_sec, "doPowPrediction"); 
    usePrediction = doPowPred;
    if (doPowPred){ 
      PowerHistSize = SescConf->getInt(sampler_sec, "PowPredictionHist"); 
      powerHist.resize(PowerHistSize);
      for (size_t i=0;i<PowerHistSize;i++)
        powerHist[i].resize(energyBundle->cntrs.size());
      predPow.resize(energyBundle->cntrs.size());
      std::cout<<"Power Prediction enabled with History size "<<PowerHistSize<<"\n";
    }
      samplerType = Adaptive;
  }  else if (strcasecmp(sampler_type,"Periodic") == 0){
    doPowPred  = SescConf->getBool(sampler_sec, "doPowPrediction"); 
    if (doPowPred){ 
      usePrediction = doPowPred;
      PowerHistSize = SescConf->getInt(sampler_sec, "PowPredictionHist"); 
      powerHist.resize(PowerHistSize);
      totalPowHist.resize(PowerHistSize);
      for (size_t i=0;i<PowerHistSize;i++)
        powerHist[i].resize(energyBundle->cntrs.size());
      predPow.resize(energyBundle->cntrs.size());
      std::cout<<"Power Prediction enabled with History size "<<PowerHistSize<<"\n";
    }
    samplerType = Periodic;
  } else if (strcasecmp(sampler_type,"SMARTS") == 0 ){
    doPowPred  = false; 
    usePrediction = doPowPred;
    samplerType = SMARTS;
  } else if (strcasecmp(sampler_type,"SkipSim") == 0 ){
    doPowPred  = false; 
    usePrediction = doPowPred;
    samplerType = SkipSim;
  } else if (strcasecmp(sampler_type,"SPoint") == 0 ){
    doPowPred  = false; 
    usePrediction = doPowPred;
    samplerType = SPoint;
  }else{
    samplerType = Others;
  }
  volNTC = 1.0;
}


void PowerModel::readTempConfig(const char *section) {
  doTherm = SescConf->getBool(section, "doTherm");
  if (!doTherm) {
    volNTC = 1.0;
    setTurboRatio(1.0);
    return;
  }

  thermalThrottle = SescConf->getDouble("pwrmodel", "thermalThrottle"); 
  tCycQuanta = SescConf->getDouble("pwrmodel", "updateInterval") *
                   SescConf->getDouble("pwrmodel", "throttleCycleRatio"); 
  enableTurbo  = SescConf->getBool("pwrmodel", "enableTurbo"); 
  if (enableTurbo){
   const char *mode  = SescConf->getCharPtr("pwrmodel", "turboMode"); 
   if (strcmp(mode, "turbo") == 0)
     turboMode = 0;
   else if (strcmp(mode, "dvfs_t") == 0)
     turboMode = 1;
   else if (strcmp(mode, "ntc") == 0) {
     turboMode = 2;
     volNTC  = SescConf->getDouble("pwrmodel", "volNTC"); 
   } else if (strcmp(mode, "ntc_adaptive") == 0) {
     turboMode = 3;
     volNTC  = SescConf->getDouble("pwrmodel", "volNTC"); 
   } else {
     printf("Error determining the mode for DTM mechanism! Check turboMode in therm.conf\n");
     exit(-1);
   }
  } else {
    volNTC = 1.0;
    setTurboRatio(1.0);
  }
  updateTurboState(); 
}
#define K(t)  t + 273.15

int PowerModel::updateFreqTurbo() {


  float turboFreq;
  int state = 0;

  float maxT       = getMaxT();
  FlowID actvCores = getNumActiveCores();

  // Get the maximum frequency based on the number of active cores
  float maxF       = getFreq() + (ncores - actvCores + 1) * 500*1e6; //Mhz , FIXME: the step (e.g. 133) should be defined in the conf file

  // Decide on the actual turbo frequency based on temperature
  if (maxT > K(100)) {
    turboFreq = getFreq();
    state = 4;
  } else if (maxT > K(90)) {
    turboFreq = maxF - 3*(maxF - getFreq())/4;
    state = 3;
  } else if (maxT > K(80)) {
    turboFreq = maxF - 2*(maxF - getFreq())/4;
    state = 2;
  } else if (maxT > K(70)) {
    turboFreq = maxF - (maxF - getFreq())/4;
    state = 1;
  } else {
    turboFreq = maxF;
    state = 0;
  }

  float freqCoef = turboFreq/getFreq();
  // Save the turbo frequency ratio in TaskHandler, other modules will read it from there.
  setTurboRatio(freqCoef);


  return (state);

}

int PowerModel::updateFreqDVFS_T() {


  float dvfsFreq;
  int state = 0;

  float maxT       = getMaxT();

  // Decide on the actual turbo frequency based on temperature
  if (maxT > K(90)) {
    // Throttling should have been engaged!
    dvfsFreq = 0.7*getFreq();
    state    = 4;
  } else if (maxT > K(88.5)) {
    dvfsFreq = 0.7*getFreq();
    state    = 4;
  } else if (maxT > K(87.5)) {
    dvfsFreq = 0.8*getFreq();
    state    = 3;
  } else if (maxT > K(86)) {
    dvfsFreq = 0.9*getFreq();
    state    = 2;
  } else {
    dvfsFreq = getFreq();
    state    = 0;
  }

  float freqCoef = dvfsFreq/getFreq();
  // Save the turbo frequency ratio in TaskHandler, other modules will read it from there.
  setTurboRatio(freqCoef);


  return (state);

}



float PowerModel::getVolRatio(int state) {
  float volCoef = 1.0;
  switch (state) {
    case 0: 
    case 1: 
      volCoef = 1.0;
      break;
    case 2: 
      volCoef = 0.95;
      break;
    case 3: 
      volCoef = 0.90;
      break;
    case 4: 
      volCoef = 0.85;
      break;
    default:
      volCoef = 1.0;
  }
  return volCoef;
}

void PowerModel::updatePowerTurbo(int freqState) {

  float volCoef = getVolRatio(freqState);

  float dynCoef = volCoef * volCoef * volCoef; // dynP ~ V^3
  float lkgCoef = volCoef * volCoef; // Lkg ~ V^2
  for (size_t k=0;k<energyBundle->cntrs.size();k++) {
      energyBundle->cntrs[k].setDyn(energyBundle->cntrs[k].getDyn() * dynCoef);
      energyBundle->cntrs[k].setLkg(energyBundle->cntrs[k].getLkg() * lkgCoef);
      energyBundle->cntrs[k].setCyc(energyBundle->cntrs[k].getCyc() / volCoef);
  }
}

void PowerModel::initTurboState() {

#if ENABLE_CUDA
     setTurboRatioGPU(1.0);
#endif
     setTurboRatio(1.0);
}

void PowerModel::updateTurboState() {

  if (!enableTurbo)
    return;

  int freqState = 0;
  switch (turboMode) {
    case 0:
      freqState = updateFreqTurbo();
      updatePowerTurbo(freqState);
      break;
    case 1:
     freqState =  updateFreqDVFS_T();
     updatePowerTurbo(freqState);
     break;
#if ENABLE_CUDA
    case 2:
    case 3:
     freqState =  updateFreqNTC();
     updatePowerNTC();
     break;
#endif
    default:
     printf("\nWarning! Unknown Turbo state.\n");
#if ENABLE_CUDA
     setTurboRatioGPU(1.0);
#endif
     setTurboRatio(1.0);
  }

  timeInterval /= getTurboRatio();
}

#if ENABLE_CUDA

float PowerModel::getCurrentED() {
  float ed     = 0.0;
  for(size_t i = 0; i< energyBundle->cntrs.size(); i++) {
    ed +=  currentEngDelayCntr[i]->getDouble();
  }
  return ed;
}

void PowerModel::adapt() {

  if (turboMode == 3) {
  // It will notify GPUThreadManager of the new numSM
  // also will return the new volNTC
    kernelsVolNTC[kId] = cfgSearch[kId]->nextCfg(getCurrentED());
    MSG("new volNTC:%f\n", kernelsVolNTC[kId]);
    }
}

int PowerModel::updateFreqNTC() {

  //get the volNTC
  //calculate volRatio
  //calculate freq

  float ntcFreq = freqTrendNTC_var(volNTC);
  float freqCoef = ntcFreq/getFreq();
  setTurboRatioGPU(freqCoef);


  return 0;

}

void PowerModel::updatePowerNTC() {

  float volCoef = volNTC/1.0;

  float dynCoef = volCoef * volCoef; // dynP ~ V^2
  float lkgCoef = volCoef; // Lkg ~ V
  for (size_t k=0;k<energyBundle->cntrs.size();k++) {
    if (strstr(energyBundle->cntrs[k].getName(), "G")) {
      energyBundle->cntrs[k].setDyn(energyBundle->cntrs[k].getDyn() * dynCoef);
      energyBundle->cntrs[k].setScaledLkg(energyBundle->cntrs[k].getLkg() * lkgCoef);
    }
  }
}


float PowerModel::freqTrendNTC(float volNTC) {
  // extracted from the model presented in 
  // "Practical Strategies for Power-Efficient Computing Technologies"
  return (3.667*volNTC - 1.276);
}

float PowerModel::freqTrendNTC_var(float volNTC) {
  // extracted from the model presented in 
  // "Practical Strategies for Power-Efficient Computing Technologies"
  float vfreq    = (3.667*volNTC - 1.276)*freq/(3.667*1.0 - 1.276);
  float penalty = (5.0528/(volNTC-0.3787))/100 - 0.081;
  float newfreq = 1.0/((1.0/vfreq) * (1+penalty));

  return newfreq; 
}

void initGPUCfgSearch() {
  FlowID numSMs     = gpuTM->ret_numSM(); // We might dynamically changes this to a lower number
  FlowID numKernels = gpuTM->getNumKernels();
  for (int i=0; i<numKernels; i++) {
    cfgSearch.push_back(new CfgSearch(numSMs, 1.0, 0.5, volNTC, i) );
    kernelsVolNTC.push_back(volNTC);
  }
}
#endif

void PowerModel::plugStackingValidator(){
  // read the solution
  if (SescConf->checkCharPtr("", "stackingSol")) {
    const char * solution = SescConf->getCharPtr("", "stackingSol");
    uint32_t ii = 0;
     // mark each solution in the Bundle
     while (solution[ii] != '\0') {
       energyBundle->cntrs[ii].setSPart(solution[ii]);
       ii++;
     }
     I(ii == energyBundle->cntrs.size());
  }

  char *fname_dpower = static_cast<char *>(malloc(1023));
  sprintf(fname_dpower, "dpower_%s", Report::getNameID());
  dpowerf = fopen(fname_dpower, "w");  
  GMSG(dpowerf == 0, "ERROR: could not open dpower file \"%s\".", fname_dpower);
  free(fname_dpower);
}

void PowerModel::stackingValidator(){

  float hPower = 0.0;
  float fPower = 0.0;
  float power  = 0.0;
 for(size_t i = 0; i<energyBundle->cntrs.size(); i++) {
 //for(size_t i = 0; i<energyBundle->coreEIdx; i++) {
    float pow = energyBundle->cntrs[i].getDyn() + energyBundle->cntrs[i].getScaledLkg();
    power      += pow;
    if (energyBundle->cntrs[i].isInHeader())
      hPower += pow;
    else
      fPower += pow;
    //fprintf(dpowerf, "%d    %s    %e\n", energyBundle->cntrs[i].isInHeader(), energyBundle->cntrs[i].getName(), pow);
  }

  float dpower = 100*(hPower - fPower)/power;
  fprintf(dpowerf, "%e\n", dpower);
  static int cnt =0;
  if (cnt++ > 1000) {
    fflush(dpowerf);
    cnt = 0;
  }
      printf("%f\n", dpower);

}
