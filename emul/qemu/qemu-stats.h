/*
 *  emulator asynchronous dump tool 
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

/* Common header file that is included by all of qemu.  */
#ifndef QEMU_STATS_H
#define QEMU_STATS_H 

#define SHARED_MEM_KEY 123321

#define _NOCACHE_ 
#define MAX_DEBUG_MSG_SIZE 4096

#define PRE_TRACE   0 
#define IN_TRACE    1 
#define POST_TRACE  0 

typedef struct async_stats
{
   /* update states ? */
   _NOCACHE_ unsigned update_states;

   /* print register states */
   _NOCACHE_ unsigned rprint_states;

   /* ptrace pc */
   _NOCACHE_ unsigned ptrc_progmctr;

   /* entry pc */
   _NOCACHE_ unsigned entry_progmctr;

   /* skip pc */
   _NOCACHE_ unsigned skip_progmctr;

   /* processor currently in single step */
   _NOCACHE_ unsigned execute_mode; 

   /* interested pc registration done */
   _NOCACHE_ unsigned registration;

   /* async_msg */
   _NOCACHE_ unsigned debug_pending;
   _NOCACHE_ char     debug_msg[MAX_DEBUG_MSG_SIZE];
} async_stats;
   
#endif // INSTRUMENT_DATA_H
