/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * ---------
 */


#include <string.h>
#include <stdio.h>
#include "../../fifo_server.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../qvalue.h"
#include "ul_fifo.h"
#include "dlist.h"
#include "udomain.h"
#include "utime.h"
#include "ul_mod.h"

#define MAX_CONTACT_LEN 128
#define MAX_EXPIRES_LEN 20
#define MAX_Q_LEN 20
#define MAX_REPLICATE_LEN 12
#define MAX_FLAGS_LEN 12


/*
 * Dedicated to Douglas Adams, don't panic !
 */
#define FIFO_CALLID "The-Answer-To-The-Ultimate-Question-Of-Life-Universe-And-Everything"
#define FIFO_CSEQ 42
#define FIFO_UA "OpenSER Server FIFO"

static str fifo_cid = str_init(FIFO_CALLID);
static str fifo_ua  = str_init(FIFO_UA);



int static ul_dump(FILE* pipe, char* response_file)
{
	FILE* reply_file;

	reply_file=open_reply_pipe(response_file);
	if (reply_file==0) {
		LOG(L_ERR, "ERROR: ul_dump: file not opened\n");
		return -1;
	}
	fputs( "200 ok\n", reply_file);
	print_all_udomains(reply_file);
	fclose(reply_file);
	return 1;
}

int static ul_flush(FILE* pipe, char* response_file)
{
	synchronize_all_udomains();
	fifo_reply(response_file, "200 ul_flush completed" );
	return 1;
}


static inline void fifo_find_domain(str* _name, udomain_t** _d)
{
	dlist_t* ptr;

	ptr = root;
	while(ptr) {
		if ((ptr->name.len == _name->len) &&
		    !memcmp(ptr->name.s, _name->s, _name->len)) {
			break;
		}
		ptr = ptr->next;
	}
	
	if (ptr) {
		*_d = ptr->d;
	} else {
		*_d = 0;
	}
}


static inline int add_contact(udomain_t* _d, str* _u, str* _c,
														ucontact_info_t *_ci)
{
	urecord_t* r;
	ucontact_t* c = 0;
	int res;
	
	if (_ci->expires == 0 && !(_ci->flags1 & FL_PERMANENT)) {
		LOG(L_ERR, "fifo_add_contact(): expires == 0 and not "
			"persistent contact, giving up\n");
		return -1;
	}

	get_act_time();

	res = get_urecord(_d, _u, &r);
	if (res >  0) { /* Record not found */
		if (insert_urecord(_d, _u, &r) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while creating "
				"new urecord\n");
			goto error0;
		}
	} else {
		if (get_ucontact(r, _c, &fifo_cid, FIFO_CSEQ+1, &c) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while obtaining ucontact\n");
			goto error0;
		}
	}
	
	_ci->callid = &fifo_cid;
	_ci->user_agent = &fifo_ua;
	_ci->cseq = FIFO_CSEQ;
	_ci->expires += act_time;
	
	if (c) {
		if (update_ucontact(c, _ci) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while updating contact\n");
			goto error1;
		}
	} else {
		if ( insert_ucontact( r, _c, _ci, &c) < 0 ) {
			LOG(L_ERR, "fifo_add_contact(): Error while inserting contact\n");
			goto error1;
		}
	}
	
	release_urecord(r);
	return 0;
error1:
	release_urecord(r);
error0:
	return -1;
}


static int ul_add(FILE* pipe, char* response_file)
{
	ucontact_info_t ci;
	char table_s[MAX_TABLE];
	char user_s[MAX_USER];
	char contact_s[MAX_CONTACT_LEN];
	char expires_s[MAX_EXPIRES_LEN];
	char q_s[MAX_Q_LEN];
	char rep_s[MAX_REPLICATE_LEN];
	char flags_s[MAX_FLAGS_LEN];
	char methods_s[MAX_FLAGS_LEN];
	udomain_t* d;
	char* at;

	str table, user, contact, expires, q, rep, flags, methods;

	if (!read_line(table_s, MAX_TABLE, pipe, &table.len) || table.len == 0) {
		fifo_reply(response_file, "400 ul_add: table name expected\n");
		LOG(L_ERR, "ERROR: ul_add: table name expected\n");
		return 1;
	}
	
	if (!read_line(user_s, MAX_USER, pipe, &user.len) || user.len  == 0) {
		fifo_reply(response_file, "400 ul_add: aor name expected\n");
		LOG(L_ERR, "ERROR: ul_add: aor expected\n");
		return 1;
	}

	at = memchr(user_s, '@', user.len);

	if (use_domain) {
		if (!at) {
			fifo_reply(response_file,"400 ul_add: username@domain expected\n");
			LOG(L_ERR, "ERROR: ul_add: Domain missing\n");
			return 1;
		}
	} else {
		if (at) {
			user.len = at - user_s;
		}
	}

	if (!read_line(contact_s, MAX_CONTACT_LEN, pipe, &contact.len)
	|| contact.len == 0) {
		fifo_reply(response_file, "400 ul_add: contact expected\n");
		LOG(L_ERR, "ERROR: ul_add: contact expected\n");
		return 1;
	}
	
	if (!read_line(expires_s, MAX_EXPIRES_LEN, pipe, &expires.len)
	|| expires.len == 0) {
		fifo_reply(response_file, "400 ul_add: expires expected\n");
		LOG(L_ERR, "ERROR: ul_add: expires expected\n");
		return 1;
	}
	
	if (!read_line(q_s, MAX_Q, pipe, &q.len) || q.len == 0) {
		fifo_reply(response_file, "400 ul_add: q expected\n");
		LOG(L_ERR, "ERROR: ul_add: q expected\n");
		return 1;
	}
	
	/* Kept for backwards compatibility */
	if (!read_line(rep_s, MAX_REPLICATE_LEN, pipe, &rep.len) || rep.len == 0) {
		fifo_reply(response_file, "400 ul_add: replicate expected\n");
		LOG(L_ERR, "ERROR: ul_add: replicate expected\n");
		return 1;
	}
	
	if (!read_line(flags_s, MAX_FLAGS_LEN, pipe, &flags.len)
	|| flags.len == 0) {
		fifo_reply(response_file, "400 ul_add: flags expected\n");
		LOG(L_ERR, "ERROR: ul_add: flags expected\n");
		return 1;
	}

	if (!read_line(methods_s, MAX_FLAGS_LEN, pipe, &methods.len)
	|| methods.len == 0) {
		fifo_reply(response_file,"400 ul_add: methods expected\n");
		LOG(L_ERR, "ERROR: ul_add: methods expected\n");
		return 1;
	}

	table.s = table_s;
	user.s = user_s;
	strlower(&user);

	contact.s = contact_s;
	expires.s = expires_s;
	q.s = q_s;
	flags.s = flags_s;
	methods.s = methods_s;
	
	fifo_find_domain(&table, &d);
	
	if (d) {
		memset( &ci, 0, sizeof(ucontact_info_t));

		if (str2int(&expires, (unsigned int*)&ci.expires) < 0) {
			fifo_reply(response_file, "400 Invalid expires format\n");
			return 1;
		}

		if (str2q(&ci.q, q.s, q.len) < 0) {
			fifo_reply(response_file, "400 Invalid q value\n");
			return 1;
		}

		if (str2int(&flags, (unsigned int*)&ci.flags1) < 0) {
			fifo_reply(response_file, "400 Invalid flags format\n");
			return 1;
		}

		if (str2int(&methods, (unsigned int*)&ci.methods) < 0) {
			fifo_reply(response_file, "400 Invalid methods format\n");
			return 1;
		}

		lock_udomain(d);
		
		if (add_contact(d, &user, &contact, &ci) < 0) {
			unlock_udomain(d);
			LOG(L_ERR, "ul_add(): Error while adding contact ('%.*s','%.*s') "
				"in table '%.*s'\n", user.len, ZSW(user.s), contact.len, 
				ZSW(contact.s), table.len, ZSW(table.s));
			fifo_reply(response_file, "500 Error while adding contact\n"
				" ('%.*s','%.*s') in table '%.*s'\n", user.len, ZSW(user.s), 
				contact.len, ZSW(contact.s), table.len, ZSW(table.s));
			return 1;
		}
		unlock_udomain(d);

		fifo_reply(response_file, "200 Added to table\n"
			"('%.*s','%.*s') to '%.*s'\n", user.len, ZSW(user.s), 
			contact.len, ZSW(contact.s), table.len, ZSW(table.s));
		return 1;
	} else {
		fifo_reply(response_file, "400 Table '%.*s' not found in memory, "
			"use save(\"%.*s\") or lookup(\"%.*s\") in the configuration "
			"script first\n", table.len, ZSW(table.s), table.len, 
			ZSW(table.s), table.len, ZSW(table.s));
		return 1;
	}
}


int static ul_rm( FILE *pipe, char *response_file )
{
	char table[MAX_TABLE];
	char user[MAX_USER];
	udomain_t* d;
	str aor, t;
	char* at;

	if (!read_line(table, MAX_TABLE, pipe, &t.len) || t.len ==0) {
		fifo_reply(response_file, 
			   "400 ul_rm: table name expected\n");
		LOG(L_ERR, "ERROR: ul_rm: table name expected\n");
		return 1;
	}
	if (!read_line(user, MAX_USER, pipe, &aor.len) || aor.len==0) {
		fifo_reply(response_file, 
			   "400 ul_rm: user name expected\n");
		LOG(L_ERR, "ERROR: ul_rm: user name expected\n");
		return 1;
	}

	at = memchr(user, '@', aor.len);

	if (use_domain) {
		if (!at) {
			fifo_reply(response_file,
				   "400 ul_rm: username@domain expected\n");
			LOG(L_ERR, "ERROR: ul_rm: Domain missing\n");
			return 1;
		}
	} else {
		if (at) {
			aor.len = at - user;
		}
	}

	aor.s = user;
	strlower(&aor);

	t.s = table;

	fifo_find_domain(&t, &d);

	LOG(L_INFO, "INFO: deleting user-loc (%s,%s)\n",
	    table, user );
	
	if (d) {
		lock_udomain(d);
		if (delete_urecord(d, &aor, 0) < 0) {
			LOG(L_ERR, "ul_rm(): Error while deleting user %s\n", user);
			unlock_udomain(d);
			fifo_reply(response_file, "500 Error while deleting user %s\n", user);
			return 1;
		}
		unlock_udomain(d);
		fifo_reply(response_file, "200 user (%s, %s) deleted\n", 
			table, user);
		return 1;
	} else {
		fifo_reply(response_file, "400 table (%s) not found\n", table);
		return 1;
	}
}


static int ul_rm_contact(FILE* pipe, char* response_file)
{
	char table[MAX_TABLE];
	char user[MAX_USER];
	char contact[MAX_CONTACT_LEN];
	udomain_t* d;
	urecord_t* r;
	ucontact_t* con;
	str aor, t, c;
	int res;
	char* at;

	if (!read_line(table, MAX_TABLE, pipe, &t.len) || t.len ==0) {
		fifo_reply(response_file, 
			   "400 ul_rm_contact: table name expected\n");
		LOG(L_ERR, "ERROR: ul_rm_contact: table name expected\n");
		return 1;
	}
	if (!read_line(user, MAX_USER, pipe, &aor.len) || aor.len==0) {
		fifo_reply(response_file, 
			   "400 ul_rm_contact: user name expected\n");
		LOG(L_ERR, "ERROR: ul_rm_contact: user name expected\n");
		return 1;
	}

	at = memchr(user, '@', aor.len);

	if (use_domain) {
		if (!at) {
			fifo_reply(response_file,
				   "400 ul_rm_contact: user@domain expected\n");
			LOG(L_ERR, "ERROR: ul_rm_contact: Domain missing\n");
			return 1;
		}
	} else {
		if (at) {
			aor.len = at - user;
		}
	}


	if (!read_line(contact, MAX_CONTACT_LEN, pipe, &c.len) || c.len == 0) {
		fifo_reply(response_file,
			"400 ul_rm_contact: contact expected\n");
		LOG(L_ERR, "ERROR: ul_rm_contact: contact expected\n");
		return 1;
	}

	aor.s = user;
	strlower(&aor);

	t.s = table;
	c.s = contact;

	fifo_find_domain(&t, &d);

	LOG(L_INFO, "INFO: deleting user-loc contact (%s,%s,%s)\n",
	    table, user, contact );


	if (d) {
		lock_udomain(d);

		res = get_urecord(d, &aor, &r);
		if (res < 0) {
			fifo_reply(response_file, "500 Error while looking for "
				"username %s in table %s\n", user, table);
			LOG(L_ERR, "ERROR: ul_rm_contact: Error while looking for "
				"username %s in table %s\n", user, table);
			unlock_udomain(d);
			return 1;
		}
		
		if (res > 0) {
			fifo_reply(response_file, "404 Username %s in table %s "
				"not found\n", user, table);
			unlock_udomain(d);
			return 1;
		}

		res = get_ucontact(r, &c, &fifo_cid, FIFO_CSEQ+1, &con);
		if (res < 0) {
			fifo_reply(response_file, "500 Error while looking for "
				"contact %s\n", contact);
			LOG(L_ERR, "ERROR: ul_rm_contact: Error while looking for "
				"contact %s\n", contact);
			unlock_udomain(d);
			return 1;
		}			

		if (res > 0) {
			fifo_reply(response_file, "404 Contact %s in table %s "
				"not found\n", contact, table);
			unlock_udomain(d);
			return 1;
		}

		if (delete_ucontact(r, con) < 0) {
			fifo_reply(response_file, "500 ul_rm_contact: Error while "
				"deleting contact %s\n", contact);
			unlock_udomain(d);
			return 1;
		}

		release_urecord(r);
		unlock_udomain(d);
		fifo_reply(response_file, "200 Contact (%s, %s) deleted from "
			"table %s\n", user, contact, table);
		return 1;
	} else {
		fifo_reply(response_file, "400 table (%s) not found\n", table);
		return 1;
	}

}


/*
 * Build Contact HF for reply
 */
static inline int print_contacts(FILE* _o, ucontact_t* _c)
{
	int cnt = 0;

	while(_c) {
		if (VALID_CONTACT(_c, act_time)) {
			cnt++;
			if (cnt==1) {
				fputs( "200 OK\n", _o);
			}
			fprintf(_o, "<%.*s>;q=%s;expires=%d;flags=0x%X;socket=<%.*s>"
				";methods=0x%X"
				"%s%.*s%s" /*received*/
				"%s%.*s%s" /*user-agent*/
				"%s%.*s%s\n", /*path*/
				_c->c.len, ZSW(_c->c.s),
				q2str(_c->q, 0), (int)(_c->expires - act_time), _c->flags,
				_c->sock?_c->sock->sock_str.len:3,
					_c->sock?_c->sock->sock_str.s:"NULL",
				_c->methods,
				_c->received.len?";received=<":"",_c->received.len,
					ZSW(_c->received.s), _c->received.len?">":"",
				_c->user_agent.len?";user_agent=<":"",_c->user_agent.len,
					ZSW(_c->user_agent.s), _c->user_agent.len?">":"",
				_c->path.len?";path=<":"",_c->path.len,
					ZSW(_c->path.s), _c->path.len?">":""
				);
		}

		_c = _c->next;
	}

	return cnt;
}


static inline int ul_show_contact(FILE* pipe, char* response_file)
{
	char table[MAX_TABLE];
	char user[MAX_USER];
	FILE* reply_file;
	udomain_t* d;
	urecord_t* r;
	int res;
	str t, aor;
	char* at;

	if (!read_line(table, MAX_TABLE, pipe, &t.len) || t.len ==0) {
		fifo_reply(response_file, 
			   "400 ul_show_contact: table name expected\n");
		LOG(L_ERR, "ERROR: ul_show_contact: table name expected\n");
		return 1;
	}
	if (!read_line(user, MAX_USER, pipe, &aor.len) || aor.len==0) {
		fifo_reply(response_file, 
			   "400 ul_show_contact: user name expected\n");
		LOG(L_ERR, "ERROR: ul_show_contact: user name expected\n");
		return 1;
	}
	
	at = memchr(user, '@', aor.len);

	if (use_domain) {
		if (!at) {
			fifo_reply(response_file,
				   "400 ul_show_contact: user@domain expected\n");
			LOG(L_ERR, "ERROR: ul_show_contact: Domain missing\n");
			return 1;
		}
	} else {
		if (at) {
			aor.len = at - user;
		}
	}

	aor.s = user;
	strlower(&aor);

	t.s = table;
	
	fifo_find_domain(&t, &d);

	if (d) {
		lock_udomain(d);

		res = get_urecord(d, &aor, &r);
		if (res < 0) {
			fifo_reply(response_file, "500 Error while looking for username %s in table %s\n", user, table);
			LOG(L_ERR, "ERROR: ul_show_contact: Error while looking for username %s in table %s\n", user, table);
			unlock_udomain(d);
			return 1;
		}
		
		if (res > 0) {
			fifo_reply(response_file, "404 Username %s in table %s not found\n", user, table);
			unlock_udomain(d);
			return 1;
		}
		
		get_act_time();

		reply_file=open_reply_pipe(response_file);
		if (reply_file==0) {
			LOG(L_ERR, "ERROR: ul_show_contact: file not opened\n");
			unlock_udomain(d);
			return 1;
		}

		if (!print_contacts(reply_file, r->contacts)) {
			unlock_udomain(d);
			fprintf(reply_file, "404 No registered contacts found\n");
			fclose(reply_file);
			return 1;
		}

		fclose(reply_file);
		unlock_udomain(d);
		return 1;
	} else {
		fifo_reply(response_file, "400 table (%s) not found\n", table);
		return 1;
	}
}



int init_ul_fifo( void ) 
{
	if (register_fifo_cmd(ul_rm, UL_RM, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_rm\n");
		return -1;
	}

	if (register_fifo_cmd(ul_rm_contact, UL_RM_CONTACT, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_rm_contact\n");
		return -1;
	}
	

	if (register_fifo_cmd(ul_dump, UL_DUMP, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_dump\n");
		return -1;
	}

	if (register_fifo_cmd(ul_flush, UL_FLUSH, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_flush\n");
		return -1;
	}

	if (register_fifo_cmd(ul_add, UL_ADD, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_add\n");
		return -1;
	}

	if (register_fifo_cmd(ul_show_contact, UL_SHOW_CONTACT, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_show_contact\n");
		return -1;
	}


	return 1;
}
