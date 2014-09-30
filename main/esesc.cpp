/* 
   ESESC: Super ESCalar simulator
   Copyright (C) 2009 University of California, Santa Cruz.

   Contributed by Jose Renau
                  Basilio Fraguela
                  Milos Prvulovic
                  Smruti Sarangi

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

/*
 * This launches the ESESC simulator environment with an ideal memory
 */

#include <sys/types.h>
#include <signal.h>

#include "nanassert.h"

#include "BootLoader.h"

char *pslice_list[10];
int pslice_count = 0;

char *ptrace_list[10];
int ptrace_count = 0;

char *sppslice_list[10];
int sppslice_count = 0;

char *ptrace_name = NULL;
char *pslice_name = NULL;
char *sppslice_name = NULL;
bool disable_pcore = false;
char *globalReportFile = NULL;
unsigned int runAheadThreshold = 256;
long int max_sbytes_size = 0;

int main(int argc, const char **argv) { 

   /* parse the ptrace and pslice file names */
  int idx;
  for(idx=0; idx<argc; idx++) 
  {
     if (!strncmp(argv[idx], "-pslice", strlen("-pslice"))) {
    	 pslice_name = (char*)argv[++idx];
    	 pslice_list[pslice_count++] = pslice_name;
     }
     if (!strncmp(argv[idx], "-ptrace", strlen("-ptrace"))) {
    	 ptrace_name = (char*)argv[++idx];
    	 ptrace_list[ptrace_count++] = ptrace_name;
     }
     if (!strncmp(argv[idx], "-sppslice", strlen("-spplice"))) {
    	 sppslice_name = (char*)argv[++idx];
    	 sppslice_list[sppslice_count++] == sppslice_name;
     }
     if (!strncmp(argv[idx], "-disable_pcore", strlen("-disable_pcore"))) disable_pcore = true;
     if (!strncmp(argv[idx], "-rh", strlen("-rh"))) runAheadThreshold = (unsigned int) atoi(argv[++idx]);
  }

  BootLoader::plug(argc, argv);
  globalReportFile = (char*) BootLoader::reportFile;
  BootLoader::boot();
  BootLoader::report("done");
  BootLoader::unboot();
  BootLoader::unplug();

  return 0;
}
