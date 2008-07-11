/*
* $Id$
*
* OpenSIPS LDAP Module
*
* Copyright (C) 2007 University of North Carolina
*
* Original author: Christian Schlatter, cs@unc.edu
*
*
* This file is part of opensips, a free SIP server.
*
* opensips is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version
*
* opensips is distributed in the hope that it will be useful,
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
* 2007-02-18: Initial version
*/


/*
Based upon libiniparser, by Nicolas Devillard
Hacked into 1 file (m-iniparser) by Freek/2005
Original terms following:

-- -

Copyright (c) 2000 by Nicolas Devillard (ndevilla AT free DOT fr).

Written by Nicolas Devillard. Not derived from licensed software.

Permission is granted to anyone to use this software for any
purpose on any computer system, and to redistribute it freely,
subject to the following restrictions:

1. The author is not responsible for the consequences of use of
this software, no matter how awful, even if they arise
from defects in it.

2. The origin of this software must not be misrepresented, either
by explicit claim or by omission.

3. Altered versions must be plainly marked as such, and must not
be misrepresented as being the original software.

4. This notice may not be removed or altered.

*/


#ifndef _INIPARSER_H_
#define _INIPARSER_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif


	typedef struct _dictionary_ {
		/** Number of entries in dictionary */
		int n;
		/** Storage size */
		int size;
		/** List of string values */
		char **val;
		/** List of string keys */
		char **key ;
		/** List of hash values for keys */
		unsigned *hash;
	} dictionary ;


	/* generated by genproto */

	dictionary * iniparser_new(char *ininame);
	void iniparser_free(dictionary * d);

	int iniparser_getnsec(dictionary * d);
	char * iniparser_getsecname(dictionary * d, int n);
	void iniparser_dump(dictionary * d, FILE * f);
	void iniparser_dump_ini(dictionary * d, FILE * f);
	char * iniparser_getkey(dictionary *d, char *section, char *key);
	char * iniparser_getstr(dictionary * d, char * key);
	char * iniparser_getstring(dictionary * d, char * key, char * def);
	int iniparser_getint(dictionary * d, char * key, int notfound);
	double iniparser_getdouble(dictionary * d, char * key, double notfound);
	int iniparser_getboolean(dictionary * d, char * key, int notfound);
	int iniparser_find_entry(dictionary  *   ini, char        *   entry);
	int iniparser_setstr(dictionary * ini, char * entry, char * val);
	void iniparser_unset(dictionary * ini, char * entry);

#ifdef __cplusplus
}
#endif

#endif

