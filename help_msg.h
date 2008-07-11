/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of OpenSIPS, a free SIP server.
 *
 * OpenSIPS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * OpenSIPS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 *\file
 * \brief The help message that is shown on the console with 'opensips -h'
 */

/*! \mainpage Developer documentation
 *
 * \section intro_sec Introduction
 * This is the OpenSIPS developer documentation. At the moment most information
 * here is autogenerated from the sources. Additional content will be added
 * over time, any help is of course welcome.
 *
 * This page tries to give some starting points for developers that want to
 * understand the server structure and create their own extensions or modules.
 * 
 * \section db_sec Database interface
 * The server uses a own database interface to hide the differences of the
 * supported db engines. Every module that implementst this API can use all
 * database drivers, as long as they implement the needed capabilities.
 *
 * The file db.h defines the database API.
 *
 * \section start_sec Server start
 * The implementation of the server start process can be found in the
 * main.c file.
 *
 * \section datastruct_sec Important data structures
 * hdr_field - represents a SIP header \n
 * \ref _str "str" - contains text variables \n
 *
 * \todo incomplete
 *
 * \section DeveloperGuide OpenSIPS Developer Guide
 * Please check the OpenSIPS developer guide at http://www.opensips.org/docs/openser-devel-guide/
 */


#ifndef HELP_MSG_H
#define HELP_MSG_H

#include "config.h"

static char help_msg[]= "\
Usage: " NAME " -l address [-l address ...] [options]\n\
Options:\n\
    -f file      Configuration file (default " CFG_FILE ")\n\
    -c           Check configuration file for errors\n\
    -C           Similar to '-c' but in addition checks the flags of exported\n\
                  functions from included route blocks\n\
    -l address   Listen on the specified address/interface (multiple -l\n\
                  mean listening on more addresses).  The address format is\n\
                  [proto:]addr[:port], where proto=udp|tcp and \n\
                  addr= host|ip_address|interface_name. E.g: -l locahost, \n\
                  -l udp:127.0.0.1:5080, -l eth0:5062 The default behavior\n\
                  is to listen on all the interfaces.\n\
    -n processes Number of child processes to fork per interface\n\
                  (default: 8)\n\
    -r           Use dns to check if is necessary to add a \"received=\"\n\
                  field to a via\n\
    -R           Same as `-r` but use reverse dns;\n\
                  (to use both use `-rR`)\n\
    -v           Turn on \"via:\" host checking when forwarding replies\n\
    -d           Debugging mode (multiple -d increase the level)\n\
    -D           Do not fork into daemon mode\n\
    -E           Log to stderr\n"
#ifdef USE_TCP
"    -T           Disable tcp\n\
    -N processes Number of tcp child processes (default: equal to `-n`)\n\
    -W method    poll method\n"
#endif
"    -V           Version number\n\
    -h           This help message\n\
    -b nr        Maximum receive buffer size which will not be exceeded by\n\
                  auto-probing procedure even if  OS allows\n\
    -m nr        Size of shared memory allocated in Megabytes\n\
    -w dir       Change the working directory to \"dir\" (default \"/\")\n\
    -t dir       Chroot to \"dir\"\n\
    -u uid       Change uid \n\
    -g gid       Change gid \n\
    -P file      Create a pid file\n\
    -G file      Create a pgid file\n"
;

#endif
