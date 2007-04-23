/*
 * $Id
 *
 * presence - presence server implementation
 * 
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2006-10-09  first version (anca)
 */

#ifndef PA_MOD_H
#define PA_MOD_H

#include "../../parser/msg_parser.h"
#include "../tm/tm_load.h"
#include "../sl/sl_api.h"
#include "../../db/db.h"
#include "../../parser/parse_from.h"
#include "event_list.h"

/* TM bind */
extern struct tm_binds tmb;
/* DB module bind */
extern db_func_t pa_dbf;
extern db_con_t* pa_db;

/* PRESENCE database */
extern int use_db;
extern str db_url;
extern char *presentity_table;
extern char *active_watchers_table;
extern char *watchers_table; 

extern int counter;
extern int pid;
extern int startup_time;
extern int reply_tag_avp_id;
extern char *to_tag_pref;
extern int expires_offset;
extern int default_expires;
extern int lock_set_size;
extern int max_expires;
extern struct sl_binds slb;
extern str server_address;

#endif /* PA_MOD_H */
