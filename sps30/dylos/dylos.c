/* Supporting routines to access and read Dylos information and measured
 * values 
 * 
 * Dylos is registered trademark Dylos Corporation
 * 2900 Adams St#C38, Riverside, CA92504 PH:877-351-2730
 *
 * This file is part of Dylos monitor.
 * 
 * Copyright (c) 2017 Paul van Haastrecht <paulvha@hotmail.com>
 *
 * Dylosmon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Dylos monitor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Dylos monitor. If not, see <http://www.gnu.org/licenses/>.  
 * 
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "dylos.h"
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <stdarg.h>

// file descriptor to device
int fd;

// set DEBUG progress level
int dylos_DEBUG = 0; 

// indicate connected & initialised 
int	connected = 0;

// save & restore current port setting
struct termios old_options;

/* display debug message
 * @param format : debug message to display and optional arguments
 *                 same as printf
 * @param level : priority : 1 = error, 0 = info 
 * info is only displayed if debug_dylos had set for it*/

void dbprintf (int level, char *format, ...)
{
    va_list arg;

    if (dylos_DEBUG || level)
    {
		va_start (arg, format);
		vfprintf (stdout, format, arg);
		va_end (arg);
    }
}

/* open connection to Dylos
 * @param device: the device to use to connect to Dylos
 * if device = NULL, default device will be used */
 
int open_dylos(char * device)
{
    // if already initialised
    if (connected)	return(0);
       
    // open device
    if (device == NULL) device = DYLOS_USB;
    
    fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
    
    if (fd < 0)
    {
		dbprintf(1,"Unable to open device %s.\n",device);
		
		if (geteuid() != 0)
			dbprintf(1,"You do not have root permission. Start with: sudo  ...\n");
		
		return(-1);
    }
        
    dbprintf(0, "Device %s has been opened.\n",device);
    
    // flush any pending input or output data
    tcflush(fd,TCIOFLUSH);
    
    // configure
    if (serial_configure() < 0)	return(-1);
    
    // set as opened / initialised
    connected = 1;

    return(0);
}

/* close the Dylos connection correctly */
void close_dylos()
{
    if (!connected)	return;
    
    if (fd)
    {
		// restore old port settings
		if (tcsetattr(fd, TCSANOW, &old_options) < 0)
			dbprintf(1,"Unable to restore serial setting on device.\n");
	
		close(fd);
		
		fd=0;
    }
    
    connected = 0;
    
    dbprintf(0, "Dylos Connection has been closed.\n");
}

/* enable (1), disable (0) debug messages */
void debug_dylos(int deb)
{
    dylos_DEBUG = deb;
}

/* set the serial configuration correct to read Dylos
 * returns Ok(0) or error (-1) */
 
int serial_configure()
{
    struct termios options;
    
    tcgetattr(fd, &options);
    tcgetattr(fd, &old_options);	// restore later
    
    cfsetispeed(&options, B9600);	// set input speed
    cfsetospeed(&options, B9600);	// set output speed
    
    options.c_cflag &=  ~CSIZE;
    options.c_cflag |= CS8;			// 8 bit
    options.c_cflag &= ~CSTOPB;		// 0 stopbit
    options.c_cflag &= ~CRTSCTS; 	// no flow control
    options.c_cflag &= ~PARENB;		// no parity
    
    options.c_iflag &= ~(IXON|IXOFF);	// no flow control
    
    options.c_cc[VMIN] = 1;			// need at least 1 character
    options.c_cc[VTIME] = 0;		// no timeout
    
    options.c_cflag |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(ISTRIP | IGNCR | INLCR | ICRNL);
    
    options.c_oflag &= ~(OPOST);
    
    if (tcsetattr(fd, TCSANOW, &options) < 0) 
    {
		dbprintf(1,"Unable to configure Dylos port.\n");
		return(-1);
    }
    
    dbprintf(0,"Serial parameters have been set.\n");
	
    return(0);
}

/* write instruction character to Dylos 
 * returns Ok(0) or error (-1) */
int write_dylos(char instruct)
{
    char	buf[2];
	
    // check that init was done
    if (!connected)	return(-1);
		
    dbprintf(0,"Now sending instruction.\n");
	    	
    buf[0] = instruct;
    buf[1] = 0xd;

    if (write(fd, buf, 2) < 0)
    {
		dbprintf(1, "Error during writing to device.\n");   
		return(-1);
    }
    
    return(0);
}


/* read from Dylos
 * @param buf : pointer to buffer allocated by user 
 * @param len : length of allocated buffer 
 * @param wait : max time in seconds before returning
 * 
 * if wait = 0 the read will block untill received data
 * 
 * returns number of characters read into buffer or 0 */
int read_dylos (char * buf, int len, int wait)
{
    int		num;
    time_t 	time_start;
	
    // check that init was done
    if (!connected) return(-1);
	
    // clear buffer
    memset(buf, 0x0, len);
    
    // get start time
    time_start=time(NULL);
	
    // wait for data
    while(1)
    {
		// try to read
		num = read(fd, buf, len);
	
		// if nothing, sleep second
		if (num == -1)	sleep(1);
		else return(num);
			
		// if wait time was requested, check whether elapsed		
		if (wait > 0)
		{
		    if (wait < time(NULL) - time_start)
			return(0);
		}
    } 
}

/* ask for the device information */
int ask_device_name()
{
    return(write_dylos('Y'));
}

/* ask for a dump of the logged data from DC1700*/
int ask_log_data()
{
    return(write_dylos('D'));
}
