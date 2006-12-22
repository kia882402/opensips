/* $Id$ */

#ifndef HA_H
#define HA_H
#include "../../locking.h"/* for get_lock_t define*/
#include <time.h>
struct ping{
   unsigned int id;
   struct timeval sent;
   struct ping *next;
};

struct ha{
   int timed_out_pings;
   int timeout;
   gen_lock_t *mutex;
   struct ping *pings; 
   int begin;
   int end;
   int count;
   int size;
};

extern char *jain_ping_config;
extern int jain_ping_period;
extern int jain_pings_lost;
extern int jain_ping_timeout;
extern struct ping *jain_pings;

extern pid_t pinger_pid;

extern char *servlet_ping_config;
extern int servlet_ping_period;
extern int servlet_pings_lost;
extern int servlet_ping_timeout;
extern struct ping *servlet_pings;

extern int use_ha;

char * create_ping_event(int *evt_len,int flags,unsigned int *seqno);
int prepare_ha();
int spawn_pinger();
int print_pingtable(struct ha *ta,int idx,int lock);
inline int init_pingtable(struct ha *table,int timeout,int maxpings);
inline void destroy_pingtable(struct ha *table);
#endif
