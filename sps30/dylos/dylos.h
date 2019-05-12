/* header file for Dylos access routines
 * 
 * Dylos is registered trademark Dylos Corporation
 * 2900 Adams St#C38, Riverside, CA92504 PH:877-351-2730
 * 
 * Copyright (c) 2017 Paul van Haastrecht <paulvha@hotmail.com>
 *
 * This file is part of Dylos monitor.
 *
 * Dylos routines is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Dylos routines is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Dylos monitor.  If not, see <http://www.gnu.org/licenses/>
 */

// default device 
#define DYLOS_USB	"/dev/ttyUSB0"

/** external routines */
/* open connection to Dylos
 * @param device: the device to use to connect to Dylos
 * if device = NULL, default device will be used */
int open_dylos(char * device);

/* close the Dylos connection correctly */
void close_dylos();

/* read from Dylos
 * @param buf : pointer to buffer allocated by user 
 * @param len : length of allocated buffer 
 * @param wait : max time in seconds before returning
 * 
 * if wait = 0 the read will block untill received data
 * 
 * returns number of characters read into buffer or 0 */
int read_dylos (char * buf, int len, int wait);

/* ask for the device unit information */
int ask_device_name();

/* ask for a dump of the logged data from DC1700 */
int ask_log_data();

/* enable (1), disable (0) debug progress messages */
void debug_dylos(int deb);

/* display debug message
 * @param format : debug message to display and optional arguments
 *                 same as printf
 * @param level : priority : 1 = error, 0 = info */
void dbprintf (int level, char *format, ...);

/** internal used routines */
/* set the serial configuration correct to read Dylos
 * returns Ok(0) or error (-1) */
int serial_configure();

/* write instruction character to Dylos 
 * returns Ok(0) or error (-1) */
int write_dylos(char instruct);
