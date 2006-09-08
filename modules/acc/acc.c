/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
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
 * 2003-04-04  grand acc cleanup (jiri)
 * 2003-11-04  multidomain support for mysql introduced (jiri)
 * 2004-06-06  updated to the new DB api, cleanup: acc_db_{bind, init,close)
 *              added (andrei)
 * 2005-05-30  acc_extra patch commited (ramona)
 * 2005-06-28  multi leg call support added (bogdan)
 * 2006-01-13  detect_direction (for sequential requests) added (bogdan)
 * 2006-09-08  flexible multi leg accounting support added,
 *             code cleanup for low level functions (bogdan)
 */


#include <stdio.h>
#include <time.h>

#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"      /* q_memchr */
#include "../../mem/mem.h"
#include "../../usr_avp.h"
#include "../../parser/hf.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/digest/digest.h"
#include "../tm/t_funcs.h"
#include "acc_mod.h"
#include "acc.h"
#include "acc_extra.h"
#include "dict.h"
#ifdef RAD_ACC
#include <radiusclient-ng.h>
#endif

#ifdef DIAM_ACC
#include "diam_dict.h"
#include "diam_message.h"
#include "diam_tcp.h"

#define AA_REQUEST 265
#define AA_ANSWER  265

#define ACCOUNTING_REQUEST 271
#define ACCOUNTING_ANSWER  271

#endif

#define ATR(atr)  atr_arr[cnt].s=A_##atr;\
				atr_arr[cnt].len=A_##atr##_LEN;

static str na={NA, NA_LEN};

extern struct acc_extra *log_extra;
extern struct acc_extra *leg_info;

#ifdef RAD_ACC
/* caution: keep these aligned to RAD_ACC_FMT !! */
static int rad_attr[] = { A_CALLING_STATION_ID, A_CALLED_STATION_ID,
	A_SIP_TRANSLATED_REQUEST_URI, A_ACCT_SESSION_ID, A_SIP_TO_TAG,
	A_SIP_FROM_TAG, A_SIP_CSEQ };
extern struct acc_extra *rad_extra;
#endif

#ifdef DIAM_ACC
extern char *diameter_client_host;
extern int diameter_client_port;
extern struct acc_extra *dia_extra;

/* caution: keep these aligned to DIAM_ACC_FMT !! */
static int diam_attr[] = { AVP_SIP_FROM_URI, AVP_SIP_TO_URI, AVP_SIP_OURI, 
	AVP_SIP_CALLID, AVP_SIP_TO_TAG, AVP_SIP_FROM_TAG, AVP_SIP_CSEQ };
#endif

#ifdef SQL_ACC
static db_func_t acc_dbf;
static db_con_t* db_handle=0;
extern struct acc_extra *db_extra;
#endif

/* arrays used to collect the attributes and their values before being
 * pushed to the storage backend (whatever used) */
static str atr_arr[ALL_LOG_FMT_LEN+MAX_ACC_EXTRA+MAX_ACC_LEG];
static str val_arr[ALL_LOG_FMT_LEN+MAX_ACC_EXTRA+MAX_ACC_LEG];



static inline struct hdr_field *valid_to( struct cell *t, 
				struct sip_msg *reply)
{
	if (reply==FAKED_REPLY || !reply || !reply->to) 
		return t->uas.request->to;
	return reply->to;
}

static inline str *cred_user(struct sip_msg *rq)
{
	struct hdr_field* h;
	auth_body_t* cred;

	get_authorized_cred(rq->proxy_auth, &h);
	if (!h) get_authorized_cred(rq->authorization, &h);
	if (!h) return 0;
	cred=(auth_body_t*)(h->parsed);
	if (!cred || !cred->digest.username.user.len) 
			return 0;
	return &cred->digest.username.user;
}

static inline str *cred_realm(struct sip_msg *rq)
{
	str* realm;
	struct hdr_field* h;
	auth_body_t* cred;

	get_authorized_cred(rq->proxy_auth, &h);
	if (!h) get_authorized_cred(rq->authorization, &h);
	if (!h) return 0;
	cred=(auth_body_t*)(h->parsed);
	if (!cred) return 0;
	realm = GET_REALM(&cred->digest);
	if (!realm->len || !realm->s) {
		return 0;
	}
	return realm;
}


/* create an array of str's for accounting using a formatting string;
 * this is the heart of the accounting module -- it prints whatever
 * requested in a way, that can be used for syslog, radius, 
 * sql, whatsoever */
static int fmt2strar( char *fmt, /* what would you like to account ? */
		struct sip_msg *rq, /* accounted message */
		struct hdr_field *to_hdr,
		str *phrase,
		str *val_arr, /* that's the output -- must have MAX_ACC_COLUMNS */
		str *atr_arr)
{
#define get_from_to( _msg, _from, _to) \
	do{ \
		if (!from_to_set) {\
			if(_msg->msg_flags&FL_REQ_UPSTREAM){\
				DBG("DBUG:acc:fmt2strar: UPSTREAM flag set -> swap F/T\n"); \
				/* swap from and to */ \
				_from = to_hdr; \
				_to = _msg->from; \
			} else { \
				_from = _msg->from; \
				_to = to_hdr; \
			} \
			from_to_set = 1; \
		} \
	}while(0)

#define get_ft_body( _ft_hdr) ((struct to_body*)_ft_hdr->parsed)

	int cnt;
	struct to_body *ft_body;
	static struct sip_uri f_puri;
	static struct sip_uri t_puri;
	static str mycode;
	str *cr;
	struct cseq_body *cseq;
	int from_to_set;
	struct hdr_field *from , *to;

	cnt=0;
	from_to_set = 0;
	from = to = 0;

	/* we don't care about parsing here; either the function
	 * was called from script, in which case the wrapping function
	 * is supposed to parse, or from reply processing in which case
	 * TM should have preparsed from REQUEST_IN callback; what's not
	 * here is replaced with NA
	 */

	while(*fmt) {
		if (cnt==ALL_LOG_FMT_LEN) {
			LOG(L_ERR, "ERROR:acc:fmt2strar: too long formatting string\n");
			return 0;
		}
		switch(*fmt) {
			case 'n': /* CSeq number */
				if (rq->cseq && (cseq=get_cseq(rq)) && cseq->number.len) 
					val_arr[cnt]=cseq->number;
				else val_arr[cnt]=na;
				ATR(CSEQ);
				break;
			case 'c':	/* Callid */
				val_arr[cnt]=rq->callid && rq->callid->body.len
						? rq->callid->body : na;
				ATR(CALLID);
				break;
			case 'i': /* incoming uri */
				val_arr[cnt]=rq->first_line.u.request.uri;
				ATR(IURI);
				break;
			case 'm': /* method */
				val_arr[cnt]=rq->first_line.u.request.method;
				ATR(METHOD);
				break;
			case 'o': /* outgoing uri */
				if (rq->new_uri.len) val_arr[cnt]=rq->new_uri;
				else val_arr[cnt]=rq->first_line.u.request.uri;
				ATR(OURI);
				break;
			case 'f': /* from-body */
				get_from_to( rq, from, to);
				val_arr[cnt]=(from && from->body.len) 
					? from->body : na;
				ATR(FROM);
				break;
			case 'r': /* from-tag */
				get_from_to( rq, from, to);
				if (from && (ft_body=get_ft_body(from))
							&& ft_body->tag_value.len) {
						val_arr[cnt]=ft_body->tag_value;
				} else val_arr[cnt]=na;
				ATR(FROMTAG);
				break;
			case 'U': /* digest, from-uri otherwise */
				cr=cred_user(rq);
				if (cr) {
					ATR(UID);
					val_arr[cnt]=*cr;
					break;
				}
				/* fallback to from-uri if digest unavailable ... */
			case 'F': /* from-uri */
				get_from_to( rq, from, to);
				if (from && (ft_body=get_ft_body(from)) && ft_body->uri.len) {
						val_arr[cnt]=ft_body->uri;
				} else val_arr[cnt]=na;
				ATR(FROMURI);
				break;
			case '0': /* from-user */
				get_from_to( rq, from, to);
				val_arr[cnt]=na;
				if (from && (ft_body=get_ft_body(from)) && ft_body->uri.len) {
					parse_uri(ft_body->uri.s, ft_body->uri.len, &f_puri);
					if (f_puri.user.len) 
							val_arr[cnt]=f_puri.user;
				} 
				ATR(FROMUSER);
				break;
			case 'X': /* from-domain */
				get_from_to( rq, from, to);
				val_arr[cnt]=na;
				if (from && (ft_body=get_ft_body(from)) && ft_body->uri.len) {
					parse_uri(ft_body->uri.s, ft_body->uri.len, &f_puri);
					if (f_puri.host.len) 
							val_arr[cnt]=f_puri.host;
				} 
				ATR(FROMDOMAIN);
				break;
			case 't': /* to-body */
				get_from_to( rq, from, to);
				val_arr[cnt]=(to && to->body.len) ? to->body : na;
				ATR(TO);
				break;
			case 'd': /* to-tag */
				get_from_to( rq, from, to);
				val_arr[cnt]=(to && (ft_body=get_ft_body(to))
					&& ft_body->tag_value.len) ? ft_body->tag_value : na;
				ATR(TOTAG);
				break;
			case 'T': /* to-uri */
				get_from_to( rq, from, to);
				if (to && (ft_body=get_ft_body(to))
							&& ft_body->uri.len) {
						val_arr[cnt]=ft_body->uri;
				} else val_arr[cnt]=na;
				ATR(TOURI);
				break;
			case '1': /* to user */ 
				get_from_to( rq, from, to);
				val_arr[cnt]=na;
				if (to && (ft_body=get_ft_body(to)) && ft_body->uri.len) {
					parse_uri(ft_body->uri.s, ft_body->uri.len, &t_puri);
					if (t_puri.user.len)
						val_arr[cnt]=t_puri.user;
				} 
				ATR(TOUSER);
				break;
			case 'S':
				if (phrase->len>=3) {
					mycode.s=phrase->s;mycode.len=3;
					val_arr[cnt]=mycode;
				} else val_arr[cnt]=na;
				ATR(CODE);
				break;
			case 's':
				val_arr[cnt]=*phrase;
				ATR(STATUS);
				break;
			case 'u':
				cr=cred_user(rq);
				val_arr[cnt]=cr?*cr:na;
				ATR(UID);
				break;
			case 'p': /* user part of request-uri */
				val_arr[cnt]=rq->parsed_orig_ruri.user.len ?
					rq->parsed_orig_ruri.user : na;
				ATR(UP_IURI);
				break;
			case 'D': /* domain part of request-uri */
				val_arr[cnt]=rq->parsed_orig_ruri.host.len ?
					rq->parsed_orig_ruri.host : na;
				ATR(RURI_DOMAIN);
				break;
			default:
				LOG(L_CRIT, "BUG:acc:acc_log_request: unknown char: %c\n",
					*fmt);
				return 0;
		} /* switch (*fmt) */
		fmt++;
		cnt++;
	} /* while (*fmt) */
	return cnt;
}


/********************************************
 *        acc_request
 ********************************************/
int acc_log_request( struct sip_msg *rq, struct hdr_field *to, 
				str *txt, str *phrase)
{
	static char log_msg[MAX_SYSLOG_SIZE];
	static char *log_msg_end=log_msg+MAX_SYSLOG_SIZE;
	char *p;
	int attr_cnt;
	int i;

	/* get default values */
	attr_cnt = fmt2strar( log_fmt, rq, to, phrase, val_arr, atr_arr);
	if (!attr_cnt) {
		LOG(L_ERR, "ERROR:acc:acc_log_request: fmt2strar failed\n");
		return -1;
	}

	/* get extra values */
	attr_cnt+=extra2strar( log_extra, rq, atr_arr+attr_cnt, val_arr+attr_cnt);

	/* skip leading text and begin with first item's
	 * separator ", " which will be overwritten by the
	 * leading text later */
	p=log_msg + txt->len - 1;
	for (i=0; i<attr_cnt; i++) {
		if (p+1+atr_arr[i].len+1+val_arr[i].len >= log_msg_end) {
			LOG(L_WARN,"WARNING:acc:acc_log_request: acc message too long,"
				" truncating..\n");
			p = log_msg_end;
			break;
		}
		*(p++) = A_SEPARATOR_CHR;
		memcpy(p, atr_arr[i].s, atr_arr[i].len);
		p+=atr_arr[i].len;
		*(p++) = A_EQ_CHR;
		memcpy(p, val_arr[i].s, val_arr[i].len);
		p+=val_arr[i].len;
	}

	/* get per leg attributes */
	if ( leg_info ) {
		while ( p!=log_msg_end &&
		(attr_cnt=legs2strar(leg_info,rq,atr_arr, val_arr))!=0 ) {
			for (i=0; i<attr_cnt; i++) {
				if (p+1+atr_arr[i].len+1+val_arr[i].len >= log_msg_end) {
					LOG(L_WARN,"WARNING:acc:acc_log_request: acc message too "
						"long, truncating..\n");
					p = log_msg_end;
					break;
				}
				*(p++) = A_SEPARATOR_CHR;
				memcpy(p, atr_arr[i].s, atr_arr[i].len);
				p+=atr_arr[i].len;
				*(p++) = A_EQ_CHR;
				memcpy(p, val_arr[i].s, val_arr[i].len);
				p+=val_arr[i].len;
			}
		}
	}

	/* terminating line */
	*(p++) = '\n';
	*(p++) = 0;

	/* leading text */
	memcpy(log_msg, txt->s, txt->len);

	LOG(log_level, "%s", log_msg );

	return 1;
}



/********************************************
 *        acc_missed_report
 ********************************************/


void acc_log_missed( struct cell* t, struct sip_msg *req,
								struct sip_msg *reply, unsigned int code )
{
	str acc_text;
	static str leading_text={ACC_MISSED, ACC_MISSED_LEN};

	get_reply_status(&acc_text, reply, code);
	if (acc_text.s==0) {
		LOG(L_ERR, "ERROR:acc:acc_missed_report: "
						"get_reply_status failed\n" );
		return;
	}

	acc_log_request( req, valid_to(t, reply), &leading_text, &acc_text);
	pkg_free(acc_text.s);
}


/********************************************
 *        acc_reply_report
 ********************************************/

void acc_log_reply( struct cell* t, struct sip_msg *req,
									struct sip_msg *reply, unsigned int code )
{
	static str lead={ACC_ANSWERED, ACC_ANSWERED_LEN};
	str code_str;

	code_str.s=int2str(code, &code_str.len);
	acc_log_request( req, valid_to(t,reply), &lead, &code_str );
}


/********************************************
 *        reports for e2e ACKs
 ********************************************/
void acc_log_ack( struct cell* t, struct sip_msg *req, struct sip_msg *ack )
{

	struct hdr_field *to;
	static str lead={ACC_ACKED, ACC_ACKED_LEN};
	str code_str;

	if (ack->to) to=ack->to; else to=req->to;
	code_str.s=int2str(t->uas.status, &code_str.len);
	acc_log_request(ack, to, &lead, &code_str );
}


/**************** SQL Support *************************/

#ifdef SQL_ACC

/* caution: keys need to be aligned to formatting strings */
static db_key_t db_keys[ALL_LOG_FMT_LEN+3+MAX_ACC_EXTRA];
static db_val_t db_vals[ALL_LOG_FMT_LEN+3+MAX_ACC_EXTRA];


/* binds to the corresponding database module
 * returns 0 on success, -1 on error */
int acc_db_bind(char* db_url)
{
	if (bind_dbmod(db_url, &acc_dbf)<0){
		LOG(L_ERR, "ERROR:acc:acc_db_init: bind_db failed\n");
		return -1;
	}

	/* Check database capabilities */
	if (!DB_CAPABILITY(acc_dbf, DB_CAP_INSERT)) {
		LOG(L_ERR, "ERROR:acc:acc_db_init: Database module does not "
			"implement insert function\n");
		return -1;
	}
	return 0;
}


void acc_db_init_keys()
{
	struct acc_extra *extra;
	int i;
	int n;

	/* init the static db keys */
	n = 0;
	/* caution: keys need to be aligned to formatting strings */
	db_keys[n++] = acc_from_uri;
	db_keys[n++] = acc_to_uri;
	db_keys[n++] = acc_sip_method_col;
	db_keys[n++] = acc_i_uri_col;
	db_keys[n++] = acc_o_uri_col;
	db_keys[n++] = acc_sip_from_col;
	db_keys[n++] = acc_sip_callid_col;
	db_keys[n++] = acc_sip_to_col;
	db_keys[n++] = acc_sip_status_col;
	db_keys[n++] = acc_user_col;
	db_keys[n++] = acc_totag_col;
	db_keys[n++] = acc_fromtag_col;
	db_keys[n++] = acc_domain_col;

	/* init the extra db keys */
	for(i=0,extra=db_extra; extra && i<MAX_ACC_EXTRA ; i++,extra=extra->next)
		db_keys[n++] = extra->name.s;

	/* time column */
	db_keys[n++] = acc_time_col;

	/* multi leg call columns */
	for( extra=leg_info ; extra ; extra=extra->next)
		db_keys[n++] = extra->name.s;

	/* init the values */
	for(i=0; i<n; i++) {
		VAL_TYPE(db_vals+i)=DB_STR;
		VAL_NULL(db_vals+i)=0;
	}
}


/* initialize the database connection
 * returns 0 on success, -1 on error */
int acc_db_init(char *db_url)
{
	db_handle=acc_dbf.init(db_url);
	if (db_handle==0){
		LOG(L_ERR, "ERROR:acc:acc_db_init: unable to connect to the "
				"database\n");
		return -1;
	}
	acc_db_init_keys();
	return 0;
}


/* close a db connection */
void acc_db_close()
{
	if (db_handle && acc_dbf.close)
		acc_dbf.close(db_handle);
}


int acc_db_request( struct sip_msg *rq, struct hdr_field *to, 
				str *phrase, char *table, char *fmt)
{
	static char time_buf[20];
	struct tm *tm;
	time_t timep;
	str  time_str;
	int attr_cnt;
	int i;
	int n;

	/* formated database columns */
	attr_cnt=fmt2strar( fmt, rq, to, phrase, val_arr, atr_arr);
	if (!attr_cnt) {
		LOG(L_ERR, "ERROR:acc:acc_db_request: fmt2strar failed\n");
		return -1;
	}

	/* extra columns */
	attr_cnt += extra2strar( db_extra, rq, atr_arr+attr_cnt, val_arr+attr_cnt);

	for(i=0; i<attr_cnt; i++)
		VAL_STR(db_vals+i)=val_arr[i];

	/* time column  FIXME */
	timep = time(NULL);
	tm = db_localtime ? localtime(&timep) : gmtime(&timep);
	time_str.len = strftime(time_buf, 20, "%Y-%m-%d %H:%M:%S", tm);
	time_str.s = time_buf;
	VAL_STR( db_vals + (attr_cnt++) ) = time_str;

	if (acc_dbf.use_table(db_handle, table) < 0) {
		LOG(L_ERR, "ERROR:acc:acc_db_request: Error in use_table\n");
		return -1;
	}

	if ( !leg_info ) {
		if (acc_dbf.insert(db_handle, db_keys, db_vals, attr_cnt) < 0) {
			LOG(L_ERR, "ERROR:acc:acc_db_request: "
					"Error while inserting to database\n");
			return -1;
		}
	} else {
		while ( (n=legs2strar(leg_info,rq,atr_arr+attr_cnt,
		val_arr+attr_cnt))!=0 ) {
			for (i=attr_cnt; i<attr_cnt+n; i++)
				VAL_STR(db_vals+i)=val_arr[i];
			if (acc_dbf.insert(db_handle, db_keys, db_vals, attr_cnt+n) < 0) {
				LOG(L_ERR, "ERROR:acc:acc_db_request: "
					"Error while inserting to database\n");
				return -1;
			}
		}
	}

	return 1;
}


void acc_db_missed( struct cell* t, struct sip_msg *req, struct sip_msg *reply,
	unsigned int code )
{
	str acc_text;

	get_reply_status(&acc_text, reply, code);
	if (acc_text.s==0) {
		LOG(L_ERR, "ERROR:acc:acc_db_missed_report: "
						"get_reply_status failed\n" );
		return;
	}
	acc_db_request(req, valid_to(t,reply), &acc_text, db_table_mc, SQL_MC_FMT);
	pkg_free(acc_text.s);
}


void acc_db_ack( struct cell* t, struct sip_msg *req, struct sip_msg *ack )
{
	str code_str;

	code_str.s=int2str(t->uas.status, &code_str.len);
	acc_db_request(ack, ack->to ? ack->to : req->to,
			&code_str, db_table_acc, SQL_ACC_FMT);
}


void acc_db_reply( struct cell* t, struct sip_msg *req, struct sip_msg *reply,
	unsigned int code )
{
	str code_str;

	code_str.s=int2str(code, &code_str.len);
	acc_db_request(req, valid_to(t,reply), &code_str,
			db_table_acc, SQL_ACC_FMT);
}
#endif

/************ RADIUS & DIAMETER helper functions **************/
#if defined(RAD_ACC) || defined (DIAM_ACC)
inline static UINT4 phrase2code(str *phrase)
{
	UINT4 code;
	int i;

	if (phrase->len<3) return 0;
	code=0;
	for (i=0;i<3;i++) {
		if (!(phrase->s[i]>='0' && phrase->s[i]<'9'))
				return 0;
		code=code*10+phrase->s[i]-'0';
	}
	return code;
}

inline static str* get_rd_username(struct sip_msg *rq)
{
	static char buf[MAX_URI_SIZE];
	static str user_name;
	str* user;
	str* realm;
	struct sip_uri puri;
	struct to_body* from;

	/* try to take it from credentials */
	user = cred_user(rq);
	if (user) {
		realm = cred_realm(rq);
		if (realm) {
			user_name.len = user->len+1+realm->len;
			if (user_name.len>MAX_URI_SIZE) {
				LOG(L_ERR, "ERROR:acc:get_rd_username: URI too long\n");
				return 0;
			}
			user_name.s = buf;
			memcpy(user_name.s, user->s, user->len);
			user_name.s[user->len] = '@';
			memcpy(user_name.s+user->len+1, realm->s, realm->len);
		} else {
			user_name.len = user->len;
			user_name.s = user->s;
		}
	} else {
		/* from from uri */
		if (rq->from && (from=get_from(rq)) && from->uri.len) {
			if (parse_uri(from->uri.s, from->uri.len, &puri) < 0 ) {
				LOG(L_ERR, "ERROR:acc:get_rd_username: Bad From URI\n");
				return 0;
			}
			user_name.len = puri.user.len+1+puri.host.len;
			if (user_name.len>MAX_URI_SIZE) {
				LOG(L_ERR, "ERROR:acc:get_rd_username: URI too long\n");
				return 0;
			}
			user_name.s = buf;
			memcpy(user_name.s, puri.user.s, puri.user.len);
			user_name.s[puri.user.len] = '@';
			memcpy(user_name.s+puri.user.len+1, puri.host.s, puri.host.len);
		} else {
			user_name.len = na.len;
			user_name.s = na.s;
		}
	}
	return &user_name;
}
#endif


/**************** RADIUS Support *************************/
#ifdef RAD_ACC
inline UINT4 rad_status(struct sip_msg *rq, str *phrase)
{
	int code;

	code=phrase2code(phrase);
	if (code==0)
		return vals[V_STATUS_FAILED].v;
	if ((rq->REQ_METHOD==METHOD_INVITE || rq->REQ_METHOD==METHOD_ACK)
				&& code>=200 && code<300) 
		return vals[V_STATUS_START].v;
	if ((rq->REQ_METHOD==METHOD_BYE 
					|| rq->REQ_METHOD==METHOD_CANCEL)) 
		return vals[V_STATUS_STOP].v;
	return vals[V_STATUS_FAILED].v;
}


int acc_rad_request( struct sip_msg *rq, struct hdr_field *to, str *phrase )
{
	int attr_cnt;
	VALUE_PAIR *send;
	UINT4 av_type;
	str* user;
	int i;

	send=NULL;

	attr_cnt = fmt2strar( RAD_ACC_FMT, rq, to, phrase, val_arr, atr_arr);
	if (attr_cnt!=RAD_ACC_FMT_LEN) {
		LOG(L_ERR, "ERROR:acc:acc_rad_request: fmt2strar failed\n");
		goto error;
	}

	av_type=rad_status(rq, phrase); /* status */
	if (!rc_avpair_add(rh, &send, attrs[A_ACCT_STATUS_TYPE].v,&av_type,-1,0)){
		LOG(L_ERR, "ERROR:acc:acc_rad_request: add STATUS_TYPE\n");
		goto error;
	}

	av_type=vals[V_SIP_SESSION].v; /* session*/
	if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &av_type, -1, 0)) {
		LOG(L_ERR, "ERROR:acc:acc_rad_request: add STATUS_TYPE\n");
		goto error;
	}

	av_type=phrase2code(phrase); /* status=integer */
	if (!rc_avpair_add(rh, &send, attrs[A_SIP_RESPONSE_CODE].v,&av_type,-1,0)){
		LOG(L_ERR, "ERROR:acc:acc_rad_request: add RESPONSE_CODE\n");
		goto error;
	}

	av_type=rq->REQ_METHOD; /* method */
	if (!rc_avpair_add(rh, &send, attrs[A_SIP_METHOD].v, &av_type, -1, 0)) {
		LOG(L_ERR, "ERROR:acc:acc_rad_request: add SIP_METHOD\n");
		goto error;
	}

	user = get_rd_username(rq);
	if (user==0)
		goto error;
	if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, user->s, user->len,0)){
		LOG(L_ERR, "ERROR:acc:acc_rad_request: add USER_NAME\n" );
		goto error;
	}

	/* unix time */
	av_type=(UINT4)time(0);
	if (!rc_avpair_add(rh, &send, attrs[A_TIME_STAMP].v, &av_type, -1, 0)) {
		LOG(L_ERR, "ERROR:acc:acc_rad_request: add TIME_STAMP\n");
		goto error;
	}

	/* Remaining attributes from rad_attr vector */
	for(i=0; i<attr_cnt; i++) {
		if (!rc_avpair_add(rh, &send, attrs[rad_attr[i]].v, 
				val_arr[i]->s,val_arr[i]->len, 0)) {
			LOG(L_ERR, "ERROR:acc:acc_rad_request: rc_avpaid_add "
				"failed for FMT %s\n", attrs[rad_attr[i]].n );
			goto error;
		}
	}

	/* add extra also */
	attr_cnt = extra2strar( rad_extra, rq, atr_arr, val_arr);
	for(i=0; i<attr_cnt; i++) {
		if (!rc_avpair_add(rh, &send, attrs[atr_arr[i].len].v,
				val_arr[i]->s,val_arr[i]->len, 0)) {
			LOG(L_ERR, "ERROR:acc:acc_rad_request: rc_avpaid_add "
				"failed for extra %s {%d,%d,%d} val {%p,%d}\n", atr_arr[i].s,
				i, atr_arr[i].len, attrs[atr_arr[i].len].v,
				val_arr[i]->s, val_arr[i]->len);
			goto error;
		}
	}

	/* call-legs attributes also get inserted */
	if ( leg_info ) {
		while ( (attr_cnt=legs2strar(leg_info,rq,atr_arr,val_arr))!=0 ) {
			for (i=0; i<attr_cnt; i++) {
				if (!rc_avpair_add(rh, &send, attrs[atr_arr[i].len].v,
				val_arr[i]->s,val_arr[i]->len, 0)) {
					LOG(L_ERR, "ERROR:acc:acc_rad_request: rc_avpaid_add "
						"failed for leg %s {%d,%d,%d} val {%p,%d}\n",
						atr_arr[i].s, i, atr_arr[i].len,
						attrs[atr_arr[i].len].v,
						val_arr[i]->s, val_arr[i]->len);
					goto error;
				}
			}
		}
	}

	if (rc_acct(rh, SIP_PORT, send)!=OK_RC) {
		LOG(L_ERR, "ERROR:acc:acc_rad_request: radius-ing failed\n");
		goto error;
	}
	rc_avpair_free(send);
	return 1;

error:
	rc_avpair_free(send);
	return -1;
}


void acc_rad_missed( struct cell* t, struct sip_msg *req,
									struct sip_msg *reply, unsigned int code )
{
	str acc_text;

	get_reply_status(&acc_text, reply, code);
	if (acc_text.s==0) {
		LOG(L_ERR, "ERROR:acc:acc_rad_missed_report: "
			"get_reply_status failed\n" );
		return;
	}
	acc_rad_request( req, valid_to(t,reply), &acc_text);
	pkg_free(acc_text.s);
}


void acc_rad_ack( struct cell* t, struct sip_msg *req, struct sip_msg *ack )
{
	str code_str;

	code_str.s=int2str(t->uas.status, &code_str.len);
	acc_rad_request(ack, ack->to ? ack->to : req->to,
			&code_str);
}


void acc_rad_reply( struct cell* t, struct sip_msg *req, struct sip_msg *reply,
	unsigned int code )
{
	str code_str;

	code_str.s=int2str(code, &code_str.len);
	acc_rad_request( req, valid_to(t,reply), &code_str);
}
#endif



/**************** DIAMETER Support *************************/
#ifdef DIAM_ACC
inline unsigned long diam_status(struct sip_msg *rq, str *phrase)
{
	int code;

	code=phrase2code(phrase);
	if (code==0)
		return -1;

	if ((rq->REQ_METHOD==METHOD_INVITE || rq->REQ_METHOD==METHOD_ACK)
				&& code>=200 && code<300) 
		return AAA_ACCT_START;
	
	if ((rq->REQ_METHOD==METHOD_BYE 
					|| rq->REQ_METHOD==METHOD_CANCEL)) 
		return AAA_ACCT_STOP;
	
	if (code>=200 && code <=300)  
		return AAA_ACCT_EVENT;
	
	return -1;
}


int acc_diam_request( struct sip_msg *rq, struct hdr_field *to, str *phrase )
{
	int attr_cnt;
	AAAMessage *send = NULL;
	AAA_AVP *avp;
	struct sip_uri puri;
	str* user;
	str value;
	str *uri;
	int ret;
	int i;
	int status;
	char tmp[2];
	unsigned int mid;


	if (skip_cancel(rq)) return 1;

	attr_cnt = fmt2strar( DIAM_ACC_FMT, rq, to, phrase, val_arr, atr_arr);
	if (attr_cnt!=DIAM_ACC_FMT_LEN) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: fmt2strar failed\n");
		return -1;
	}

	if ( (send=AAAInMessage(ACCOUNTING_REQUEST, AAA_APP_NASREQ))==NULL) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: failed to create new "
			"AAA request\n");
		return -1;
	}

	/* AVP_ACCOUNTIG_RECORD_TYPE */
	if( (status = diam_status(rq, phrase))<0) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: status unknown\n");
		goto error;
	}
	tmp[0] = status+'0';
	tmp[1] = 0;
	if( (avp=AAACreateAVP(AVP_Accounting_Record_Type, 0, 0, tmp,
	1, AVP_DUPLICATE_DATA)) == 0) {
		LOG(L_ERR,"ERROR:acc:acc_diam_request: no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}
	/* SIP_MSGID AVP */
	mid = rq->id;
	if( (avp=AAACreateAVP(AVP_SIP_MSGID, 0, 0, (char*)(&mid), 
	sizeof(mid), AVP_DUPLICATE_DATA)) == 0) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	/* SIP Service AVP */
	if( (avp=AAACreateAVP(AVP_Service_Type, 0, 0, SIP_ACCOUNTING, 
	SERVICE_LEN, AVP_DUPLICATE_DATA)) == 0) {
		LOG(L_ERR,"ERROR:acc:acc_diam_request: no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	/* SIP_STATUS avp */
	if( (avp=AAACreateAVP(AVP_SIP_STATUS, 0, 0, phrase->s,
	phrase->len, AVP_DUPLICATE_DATA)) == 0) {
		LOG(L_ERR,"ERROR:acc:acc_diam_request: no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	/* SIP_METHOD avp */
	value = rq->first_line.u.request.method;
	if( (avp=AAACreateAVP(AVP_SIP_METHOD, 0, 0, value.s, 
	value.len, AVP_DUPLICATE_DATA)) == 0) {
		LOG(L_ERR,"ERROR:acc:acc_diam_request: no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	user = get_rd_username(rq);
	if (user==0)
		goto error;
	if( (avp=AAACreateAVP(AVP_User_Name, 0, 0, user->s, user->len,
	AVP_DUPLICATE_DATA)) == 0) {
		LOG(L_ERR,"ERROR:acc:acc_diam_request: no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	/* Remaining attributes from diam_attr vector */
	for(i=0; i<attr_cnt; i++) {
		if((avp=AAACreateAVP(diam_attr[i], 0,0, val_arr[i]->s, val_arr[i]->len,
		AVP_DUPLICATE_DATA)) == 0) {
			LOG(L_ERR,"ERROR:acc:acc_diam_request: no more free memory!\n");
			goto error;
		}
		if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
			LOG(L_ERR, "ERROR:acc:acc_diam_request: avp not added \n");
			AAAFreeAVP(&avp);
			goto error;
		}
	}

	/* also the extra attributes */
	attr_cnt = extra2strar( dia_extra, rq, atr_arr, val_arr);
	for(i=0; i<attr_cnt; i++) {
		if((avp=AAACreateAVP(atr_arr[i].len/*AVP code*/, 0, 0,
		val_arr[i]->s, val_arr[i]->len, AVP_DUPLICATE_DATA)) == 0) {
			LOG(L_ERR,"ERROR:acc:acc_diam_request: no more free memory!\n");
			goto error;
		}
		if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
			LOG(L_ERR, "ERROR:acc:acc_diam_request: avp not added \n");
			AAAFreeAVP(&avp);
			goto error;
		}
	}

	/* and the leg attributes */
	if ( leg_info ) {
		while ( (attr_cnt=legs2strar(leg_info,rq,atr_arr,val_arr))!=0 ) {
			for (i=0; i<attr_cnt; i++) {
				if((avp=AAACreateAVP(atr_arr[i].len/*AVP code*/, 0, 0,
				val_arr[i]->s, val_arr[i]->len, AVP_DUPLICATE_DATA)) == 0) {
					LOG(L_ERR,"ERROR:acc:acc_diam_request: no more "
						"free memory!\n");
					goto error;
				}
				if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
					LOG(L_ERR, "ERROR:acc:acc_diam_request: avp not added \n");
					AAAFreeAVP(&avp);
					goto error;
				}
			}
		}
	}

	if (get_uri(rq, &uri) < 0) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: From/To URI not found\n");
		goto error;
	}

	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: Error parsing From/To URI\n");
		goto error;
	}

	/* Destination-Realm AVP */
	if( (avp=AAACreateAVP(AVP_Destination_Realm, 0, 0, puri.host.s,
	puri.host.len, AVP_DUPLICATE_DATA)) == 0) {
		LOG(L_ERR,"ERROR:acc:acc_diam_request: no more free memory!\n");
		goto error;
	}

	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}


	/* prepare the message to be sent over the network */
	if(AAABuildMsgBuffer(send) != AAA_ERR_SUCCESS) {
		LOG(L_ERR, "ERROR:acc:acc_diam_request: message buffer not created\n");
		goto error;
	}

	if(sockfd==AAA_NO_CONNECTION) {
		sockfd = init_mytcp(diameter_client_host, diameter_client_port);
		if(sockfd==AAA_NO_CONNECTION) {
			LOG(L_ERR, "ERROR:acc:acc_diam_request: failed to reconnect"
								" to Diameter client\n");
			goto error;
		}
	}

	/* send the message to the DIAMETER client */
	ret = tcp_send_recv(sockfd, send->buf.s, send->buf.len, rb, rq->id);
	if(ret == AAA_CONN_CLOSED) {
		LOG(L_NOTICE, "NOTICE:acc:acc_diam_request: connection to Diameter"
			" client closed.It will be reopened by the next request\n");
		close(sockfd);
		sockfd = AAA_NO_CONNECTION;
		goto error;
	}

	if(ret != ACC_SUCCESS) {
		/* a transmission error occurred */
		LOG(L_ERR, "ERROR:acc:acc_diam_request: message sending to the" 
			" DIAMETER backend authorization server failed\n");
		goto error;
	}

	AAAFreeMessage(&send);
	return 1;

error:
	AAAFreeMessage(&send);
	return -1;
}


void acc_diam_missed( struct cell* t, struct sip_msg *req,
		struct sip_msg *reply, unsigned int code )
{
	str acc_text;

	get_reply_status(&acc_text, reply, code);
	acc_diam_request( req, valid_to(t,reply), &acc_text);
}


void acc_diam_ack( struct cell* t, struct sip_msg *req, struct sip_msg *ack )
{
	str code_str;

	code_str.s=int2str(t->uas.status, &code_str.len);
	acc_diam_request(ack, ack->to ? ack->to : req->to,
			&code_str);
}


void acc_diam_reply( struct cell* t , struct sip_msg *req,
		struct sip_msg *reply, unsigned int code )
{
	str code_str;

	code_str.s=int2str(code, &code_str.len);
	acc_diam_request(req, valid_to(t, reply), &code_str);
}

#endif
