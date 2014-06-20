/*
 *  i386 emulator asynchronous debug tool 
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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "qemu-stats.h"

const char * ptrc_cmd  = "ptrc";
const char * skip_cmd  = "skip";
const char * dump_cmd  = "dump";
const char * exit_cmd  = "exit";
const char * quit_cmd  = "quit";
const char * entry_cmd = "entry";

async_stats * channel = NULL;

FILE *ptrace = NULL;
FILE *pslice = NULL;

#define MAX_CMD_SIZE 1024 
#define MAX_PC_COUNT 1024 

/* hook up the shared memory */
static async_stats* hook_channel(void)
{
  int shared_id = shmget(SHARED_MEM_KEY, 
                         sizeof(async_stats), 
                         IPC_CREAT | S_IRUSR | S_IWUSR);
  return (async_stats*)shmat(shared_id, 0, 0);
}

/* print help */
static void print_help(void)
{
   printf("ESESC Emulator asynchronous debug tool\n");
   printf("     entry=xxxx  - set entry pc\n");
   printf("     skip=xxxx  - set skip pc\n");
}

/* wait for debug msg to come in and print and clear it */
static void print_debug(async_stats* channel)
{
   while(!channel->debug_pending);
   printf("%s", channel->debug_msg);
   channel->debug_pending = 0;
}

static void reader(char* cmd)
{
    printf("enter qemu-adebug command : ");
    scanf("%s", cmd);
}

static void parser(char *cmd, unsigned size, async_stats* channel)
{
   printf("sending command to emulator : %s\n", cmd);

   /* handle ptrace command */
   if (!strncmp(ptrc_cmd, cmd, strlen(ptrc_cmd)))
   {
      channel->debug_pending = 0;
      channel->ptrc_progmctr = atoi(cmd+strlen(ptrc_cmd)+1);
      channel->update_states = 1;
      print_debug(channel);
      channel->debug_pending = 0;
      channel->update_states = 0;
      return;
   }

   /* handle entry command */
   if (!strncmp(entry_cmd, cmd, strlen(entry_cmd)))
   {
      channel->debug_pending = 0;
      channel->entry_progmctr = atoi(cmd+strlen(entry_cmd)+1);
      channel->update_states = 1;
      print_debug(channel);
      channel->debug_pending = 0;
      channel->update_states = 0;
      return;
   }

   /* handle skip command */
   if (!strncmp(skip_cmd, cmd, strlen(skip_cmd)))
   {
      channel->debug_pending = 0;
      channel->skip_progmctr = atoi(cmd+strlen(skip_cmd)+1);
      channel->update_states = 1;
      print_debug(channel);
      channel->debug_pending = 0;
      channel->update_states = 0;
      return;
   }

   /* handle register print cmd */
   if (!strncmp(dump_cmd, cmd, strlen(dump_cmd)))  
   {
      channel->debug_pending = 0;
      channel->rprint_states = 1;
      print_debug(channel);
      channel->debug_pending = 0;
      channel->update_states = 0;
      return; 
   }

   /* handle exit command */
   if (!strncmp(exit_cmd, cmd, strlen(exit_cmd))) exit(0);
   if (!strncmp(quit_cmd, cmd, strlen(quit_cmd))) exit(0);
}


static void parse_file_to_cmd(FILE *ptrace, FILE* pslice)
{
    /* the first pc is the entry pc */
    int askip_pc[MAX_PC_COUNT];    
    int trace_pc[MAX_PC_COUNT];
    int slice_pc[MAX_PC_COUNT];    
    char cmd[MAX_CMD_SIZE];
    memset(askip_pc, 0, MAX_PC_COUNT*sizeof(int));
    memset(trace_pc, 0, MAX_PC_COUNT*sizeof(int));
    memset(slice_pc, 0, MAX_PC_COUNT*sizeof(int));

    int trace_idx=0;
    int slice_idx=0;
    while (!feof(ptrace)) fscanf(ptrace, "%x", &trace_pc[trace_idx++]);
    while (!feof(pslice)) fscanf(pslice, "%x", &slice_pc[slice_idx++]);

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

    /* handle entry pc */
    sprintf(cmd, "entry=%d", trace_pc[0]);
    parser(cmd, MAX_CMD_SIZE, channel);

    /* handle skip pc */
    for(n=0; n<askip_idx; n++) 
    {
       sprintf(cmd, "skip=%d", askip_pc[n]);
       parser(cmd, MAX_CMD_SIZE, channel);
    }

    /* handle ptrc pc */
    for(n=0; n<trace_idx-1; n++) 
    {
       sprintf(cmd, "ptrc=%d", trace_pc[n]);
       parser(cmd, MAX_CMD_SIZE, channel);
    }
    return;
}

int main(int argc, char **argv)
{
   /* hook up the shared memory */
   channel=hook_channel();
   if (channel) printf("remote channel attached\n");

   /* help ? */
   if (argc==1) print_help();

   int i=0;
   for(;i<argc;i++)
   {
       if (!strcmp("-ptrace", argv[i]))
       {
            ptrace = fopen(argv[++i], "r");
       }
       if (!strcmp("-pslice", argv[i]))
       {
            pslice = fopen(argv[++i], "r");
       }
   }

   if (ptrace && pslice) parse_file_to_cmd(ptrace, pslice);

   /* set registration to done so that the program can start */
   channel->registration = 1;

   char cmd[MAX_CMD_SIZE];
   for(;;)
   {
      reader(cmd);
      parser(cmd, MAX_CMD_SIZE, channel);
   }
   /* process all command lines */
   return 0;
}

