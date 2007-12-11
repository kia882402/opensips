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
 */


#ifndef error_h
#define error_h

#define E_UNSPEC            -1
#define E_OUT_OF_MEM        -2
#define E_BAD_RE            -3
/* #define E_BAD_ADDRESS -4 */
#define E_BUG               -5
#define E_CFG               -6
#define E_NO_SOCKET	        -7
/* unresolvable topmost Via */
#define E_BAD_VIA           -8
/* incomplete transaction tuple */
#define E_BAD_TUPEL         -9
/* script programming error */
#define E_SCRIPT            -10
/* error in execution of external tools */
#define E_EXEC              -11
/* too many branches demanded */
#define E_TOO_MANY_BRANCHES -12
#define E_BAD_TO            -13
/* invalid params */
#define E_INVALID_PARAMS    -14
/* Invalid character in q */
#define E_Q_INV_CHAR        -15
/* Empty q */
#define E_Q_EMPTY           -16
/* q too big (> 1) */
#define E_Q_TOO_BIG         -17
/* Decimal part missing */
#define E_Q_DEC_MISSING     -18
/* Decimal part missing */
#define E_NO_DESTINATION    -19

/* openser specific error codes */
/* destination filtered */
#define E_IP_BLOCKED      -473
/* bad protocol, like */
#define E_BAD_PROTO       -474
/* unparseable URI */
#define E_BAD_URI         -475
/* unresolvable next-hop address */
#define E_BAD_ADDRESS     -476
/* generic send error */
#define E_SEND            -477

/* generic malformed request */
#define E_BAD_REQ	  -400

/* error in server */
#define E_BAD_SERVER	  -500


#define MAX_REASON_LEN	128

#include "str.h"

/* processing status of the last command */
extern int ser_error;
extern int prev_ser_error;

struct sip_msg;

/* ser error -> SIP error */
int err2reason_phrase( int ser_error, int *sip_error, 
                char *phrase, int etl, char *signature );

/* SIP error core -> SIP text */
char *error_text( int code );

/* return pkg_malloc-ed reply status in status->s */
void get_reply_status( str *status, struct sip_msg *reply, int code );

#endif
