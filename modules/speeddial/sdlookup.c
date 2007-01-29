/*
 * $Id$
 *
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router (openser).
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * History:
 * ---------
 * 
 */


#include <string.h>

#include "../../dprint.h"
#include "../../action.h"
#include "../../config.h"
#include "../../ut.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../db/db.h"

#include "speeddial.h"
#include "sdlookup.h"

#define MAX_USERURI_SIZE	256


char useruri_buf[MAX_USERURI_SIZE];

/**
 * Rewrite Request-URI
 */
static inline int rewrite_ruri(struct sip_msg* _m, char* _s)
{
	struct action act;

	act.type = SET_URI_T;
	act.elem[0].type = STRING_ST;
	act.elem[0].u.string = _s;
	act.next = 0;
	
	if (do_action(&act, _m) < 0)
	{
		LOG(L_ERR, "sd:rewrite_ruri: Error in do_action\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
int sd_lookup(struct sip_msg* _msg, char* _table, char* _str2)
{
	str user_s;
	int nr_keys;
	struct sip_uri *puri;
	db_key_t db_keys[4];
	db_val_t db_vals[4];
	db_key_t db_cols[1];
	db_res_t* db_res = NULL;

	/* init */
	nr_keys = 0;
	db_cols[0]=new_uri_column;
	
	/* take username@domain from From header */
	if ( (puri = parse_from_uri(_msg ))==NULL )
	{
		LOG(L_ERR, "sd_lookup: ERROR cannot parse FROM header\n");
		goto err_server;
	}
		
	db_keys[nr_keys]=user_column;
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = puri->user.s;
	db_vals[nr_keys].val.str_val.len = puri->user.len;
	nr_keys++;

	if(use_domain>=1)
	{
		db_keys[nr_keys]=domain_column;
		db_vals[nr_keys].type = DB_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val.s = puri->host.s;
		db_vals[nr_keys].val.str_val.len = puri->host.len;
		nr_keys++;
		
		if (dstrip_s.s!=NULL && dstrip_s.len>0
			&& dstrip_s.len<puri->host.len
			&& strncasecmp(puri->host.s,dstrip_s.s,dstrip_s.len)==0)
		{
			db_vals[nr_keys].val.str_val.s   += dstrip_s.len;
			db_vals[nr_keys].val.str_val.len -= dstrip_s.len;
		}
	}
	/* take sd from r-uri */
	if (parse_sip_msg_uri(_msg) < 0)
	{
		LOG(L_ERR, "sd_lookup: Error while parsing Request-URI\n");
		goto err_server;
	}
	
	db_keys[nr_keys]=sd_user_column;
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = _msg->parsed_uri.user.s;
	db_vals[nr_keys].val.str_val.len = _msg->parsed_uri.user.len;
	nr_keys++;
	
	if(use_domain>=2)
	{
		db_keys[nr_keys]=sd_domain_column;
		db_vals[nr_keys].type = DB_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val.s = _msg->parsed_uri.host.s;
		db_vals[nr_keys].val.str_val.len = _msg->parsed_uri.host.len;
		nr_keys++;
		
		if (dstrip_s.s!=NULL && dstrip_s.len>0
			&& dstrip_s.len<_msg->parsed_uri.host.len
			&& strncasecmp(_msg->parsed_uri.host.s,dstrip_s.s,dstrip_s.len)==0)
		{
			db_vals[nr_keys].val.str_val.s   += dstrip_s.len;
			db_vals[nr_keys].val.str_val.len -= dstrip_s.len;
		}
	}
	
	db_funcs.use_table(db_handle, _table);
	if(db_funcs.query(db_handle, db_keys, NULL, db_vals, db_cols,
		nr_keys /*no keys*/, 1 /*no cols*/, NULL, &db_res)!=0)
	{
		LOG(L_ERR, "sd_lookup: error querying database\n");
		goto err_server;
	}

	if (RES_ROW_N(db_res)<=0 || RES_ROWS(db_res)[0].values[0].nul != 0)
	{
		DBG("sd_lookup: no sip addres found for R-URI\n");
		if (db_res!=NULL && db_funcs.free_result(db_handle, db_res) < 0)
			DBG("sd_lookup: Error while freeing result of query\n");
		return -1;
	}

	user_s.s = useruri_buf+4;
	switch(RES_ROWS(db_res)[0].values[0].type)
	{ 
		case DB_STRING:
			strcpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.string_val);
			user_s.len = strlen(user_s.s);
		break;
		case DB_STR:
			strncpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.str_val.s,
				RES_ROWS(db_res)[0].values[0].val.str_val.len);
			user_s.len = RES_ROWS(db_res)[0].values[0].val.str_val.len;
			user_s.s[user_s.len] = '\0';
		break;
		case DB_BLOB:
			strncpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.blob_val.s,
				RES_ROWS(db_res)[0].values[0].val.blob_val.len);
			user_s.len = RES_ROWS(db_res)[0].values[0].val.blob_val.len;
			user_s.s[user_s.len] = '\0';
		default:
			LOG(L_ERR, "sd_lookup: Unknown type of DB new_uri column\n");
			if (db_res != NULL && db_funcs.free_result(db_handle, db_res) < 0)
			{
				DBG("sd_lookup: Error while freeing result of query\n");
			}
			goto err_server;
	}
	
	/* check 'sip:' */
	if(user_s.len<4 || strncmp(user_s.s, "sip:", 4))
	{
		memcpy(useruri_buf, "sip:", 4);
		user_s.s -= 4;
		user_s.len += 4;
	}

	/**
	 * Free the result because we don't need it anymore
	 */
	if (db_res!=NULL && db_funcs.free_result(db_handle, db_res) < 0)
		DBG("sd_lookup: Error while freeing result of query\n");

	/* set the URI */
	DBG("sd_lookup: URI of sd from R-URI [%s]\n", user_s.s);
	if(rewrite_ruri(_msg, user_s.s)<0)
	{
		LOG(L_ERR, "sd_lookup: Cannot replace the R-URI\n");
		goto err_server;
	}

	return 1;

err_server:
	return -1;
}

