/*
 * $Id: event_list.h 1953 2007-04-04 08:50:33Z anca_vamanu $
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2007-04-05  initial version (anca)
 */

#ifndef _PRES_EV_LST_H
#define  _PRES_EV_LST_H

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "subscribe.h"

#define WINFO_TYPE			1<< 0
#define PUBL_TYPE		    1<< 1

struct subscription;

typedef int (apply_auth_t)(str* , struct subscription*, str* );

typedef int (publ_handling_t)(struct sip_msg*);

typedef int (subs_handling_t)(struct sip_msg*);

typedef str* (agg_nbody_t)(str** body_array, int n, int off_index);
/* params for agg_body_t 
 *	body_array= an array with all the bodies stored for that resource
 *	n= the number of bodies
 *	off_index= the index of the registration(etag) for which a Publish
 *				with Expires: 0 has just been received
 *	*/
typedef int (is_allowed_t)(struct subscription* subs);
/* return code rules for is_allowed_t
 *	< 0  if error occured
 *	=0	 if no change in status(if no xcap document exists)
 *	>0   if change in status
 *	*/

struct ev
{
	str name;
	str* param;         // required param 
	/* to do: transform it in a list ( for multimple param)*/
	str stored_name;
	str content_type;
	int type;

	/* fileds that deal with authorization rules*/
	/*
	 *  req_auth -> flag 0  - if not require 
	 *  is_watcher_allowed  - get subscription state from xcap rules
	 *  apply_auth_nbody    - alter the body according to authorization rules
	 */
	int req_auth;
	apply_auth_t*  apply_auth_nbody;
	is_allowed_t*  is_watcher_allowed;
	
	/* an agg_body_t function should be registered if the event permits having
	 * multiple published states and requires an aggregation of the information
	 * otherwise, this field should be NULL and the last published state is taken 
	 * when constructing Notify msg 
	 * */
	agg_nbody_t* agg_nbody;
	publ_handling_t  * evs_publ_handl;
	subs_handling_t  * evs_subs_handl;

	struct ev* wipeer;			
	struct ev* next;
	
};
typedef struct ev ev_t;

typedef struct evlist
{
	int ev_count;
	ev_t* events;
}evlist_t;	

evlist_t* init_evlist();

int add_event(ev_t* event);

typedef int (*add_event_t)(ev_t* event);

ev_t* contains_event(str* name, str* param);

void destroy_evlist();

extern evlist_t* EvList;

#endif
