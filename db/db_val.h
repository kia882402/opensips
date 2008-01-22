/* 
 * $Id$ 
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007-2008 1&1 Internet AG
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

/**
 * \file db/db_val.h
 * \brief Data structures that represents values in the database.
 *
 * This file defines data structures that represents values in the database.
 * Several datatypes are recognized and converted by the database API.
 * Available types: DB_INT, DB_DOUBLE, DB_STRING, DB_STR, DB_DATETIME, DB_BLOB and DB_BITMAP
 * It also provides some macros for convenient access to this values.
 */


#ifndef DB_VAL_H
#define DB_VAL_H

#include <time.h>
#include "../str.h"


/**
 * Each cell in a database table can be of a different type. To distinguish
 * among these types, the db_type_t enumeration is used. Every value of the
 * enumeration represents one datatype that is recognized by the database
 * API.
 */
typedef enum {
	DB_INT,        /**< represents an 32 bit integer number      */
	DB_DOUBLE,     /**< represents a floating point number       */
	DB_STRING,     /**< represents a zero terminated const char* */
	DB_STR,        /**< represents a string of 'str' type        */
	DB_DATETIME,   /**< represents date and time                 */
	DB_BLOB,       /**< represents a large binary object         */
	DB_BITMAP      /**< an one-dimensional array of 32 flags     */
} db_type_t;


/**
 * This structure represents a value in the database. Several datatypes are
 * recognized and converted by the database API. These datatypes are automaticaly
 * recognized, converted from internal database representation and stored in the
 * variable of corresponding type.
 */
typedef struct {
	db_type_t type; /** Type of the value                              */
	int nul;        /** Means that the column in database has no value */
	/** Column value structure that holds the actual data in a union.  */
	union {
		int           int_val;    /**< integer value              */
		double        double_val; /**< double value               */
		time_t        time_val;   /**< unix time_t value          */
		const char*   string_val; /**< zero terminated string     */
		str           str_val;    /**< str type string value      */
		str           blob_val;   /**< binary object data         */
		unsigned int  bitmap_val; /**< Bitmap data type           */
	} val;
} db_val_t;


/**
 * Useful macros for accessing attributes of db_val structure.
 * All macros expect a reference to a db_val_t variable as parameter.
 */

/**
 * Use this macro if you need to set/get the type of the value.
 */
#define VAL_TYPE(dv)   ((dv)->type)


/**
 * Use this macro if you need to set/get the null flag. A non-zero flag means that
 * the corresponding cell in the database contains no data (a NULL value in MySQL
 * terminology).
 */
#define VAL_NULL(dv)   ((dv)->nul)


/**
 * Use this macro if you need to access the integer value in the db_val_t structure.
 */
#define VAL_INT(dv)    ((dv)->val.int_val)


/**
 * Use this macro if you need to access the double value in the db_val_t structure.
 */
#define VAL_DOUBLE(dv) ((dv)->val.double_val)


/**
 * Use this macro if you need to access the time_t value in the db_val_t structure.
 */
#define VAL_TIME(dv)   ((dv)->val.time_val)


/**
 * Use this macro if you need to access the string value in the db_val_t structure.
 */
#define VAL_STRING(dv) ((dv)->val.string_val)


/**
 * Use this macro if you need to access the str structure in the db_val_t structure.
 */
#define VAL_STR(dv)    ((dv)->val.str_val)


/**
 * Use this macro if you need to access the blob value in the db_val_t structure.
 */
#define VAL_BLOB(dv)   ((dv)->val.blob_val)


/**
 * Use this macro if you need to access the bitmap value in the db_val_t structure.
 */
#define VAL_BITMAP(dv) ((dv)->val.bitmap_val)


#endif /* DB_VAL_H */
