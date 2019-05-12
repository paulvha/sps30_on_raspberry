/**
 * Copyright (c) 2018 Paulvha.      version 1.0
 *
 * This program will set and get information from an SDS-011 sensor
 *
 * A starting point and still small part of this code is based on the work from
 * karl, found on:  github https://github.com/karlchen86/SDS011
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * version 1.0 paulvha, May 2019
 *  -initial version background monitor
 */

#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include "sdsmon.h"

/* indicate these serial calls are C-programs and not to be linked */
extern "C" {
    void configure_interface(int fd, int speed);
    void set_blocking(int fd, int should_block);
    void restore_ser(int fd);
}

// indicate connected & initialised 
int sdsconnected = 0;

// global variables
int  sdsfd = 0xff;                   // file pointer

// verbose messages
int sdsverbose = 0;

/**
 * constructor
 **/
SDSmon::SDSmon(void) {}

/**
 *  @brief close program correctly
 **/
void SDSmon::close_sds()
{
    if (!sdsconnected) return;
    
    if (sdsfd != 0xff) {
        // restore serial/USB to orginal setting
        restore_ser(sdsfd);
        close(sdsfd);
    }

    sdsconnected = 0;
    
    if (sdsverbose) printf("SDS monitor: Connection has been closed.\n");
}

/**
 * @brief read from SDS
 * @param pm25 : store value PM2.5
 * @param pm10 : store value PM10
 * 
 * @return  
 * 0 success
 * -1 error
 */

int SDSmon::read_sds(float *pm25, float *pm10)
{
    // check that init was done
    if (!sdsconnected) return(-1);

    if (Query_data(pm25, pm10) == SDS011_ERROR) {
        printf("SDS monitor: error during query data\n");
        return(-1);
    }

    return(0);
}

/** 
 * @brief open connection to SDS
 * @param device: the device to use to connect to SDS
 * @param verbose : if > 0 progress messsages are displayed
 * 
 * @return  
 * 0 success
 * -1 error
 */
int SDSmon::open_sds(char * device, int verbose)
{
    if (geteuid() != 0) {
        printf("SDS monitor: You do not have root permission. Start with: sudo  ...\n");
        return(-1);
    }
    
    sdsverbose = verbose;
 
    if (sdsverbose)
        printf("SDS monitor: trying to open USB port %s\n", device);
        
    /* you need the driver to be loaded before opening /dev/ttyUSBx
     * otherwise it will hang. The SDS-011 has an HL-341 chip, checked
     * with lsusb. The name of the driver is ch341.
     * One can change the system setup so this is done automatically
     * when reboot and then you can remove the call below. */
    system("modprobe usbserial");
    system("modprobe ch341");

    sdsfd = open(device, O_RDWR | O_NOCTTY | O_SYNC);

    if (sdsfd < 0) {
        printf("SDS monitor: could not open %s\n", device);
        return(-1);
    }

    configure_interface(sdsfd, B9600);
    set_blocking(sdsfd, 0);

   /* There is a problem with flushing buffers on a serial USB that can
    * not be solved. The only thing one can try is to flush any buffers
    * after some delay:
    *
    * https://bugzilla.kernel.org/show_bug.cgi?id=5730
    * https://stackoverflow.com/questions/13013387/clearing-the-serial-ports-buffer
    */
    usleep(10000);                      // required to make flush work, for some reason
    tcflush(sdsfd,TCIOFLUSH);
    
    if (sdsverbose) printf("SDS monitor: trying to connecting to SDS-011\n");
    
    /* try overcome connection problems before real actions (see document)
     * this will also inform the driver about the file description to use for writting
     * and reading */
    if (begin(sdsfd) == SDS011_ERROR)
    {
        printf("SDS monitor: Error during trying to connect\n");
        return(-1);
    }
    
    if (sdsverbose) printf("SDS monitor: Connected\n");
    
    // set reporting in query mode
    if (Set_data_reporting_mode(REPORT_QUERY) == SDS011_ERROR) {
        printf("SDS monitor: error during setting reading mode\n");
        close_sds();
        return(-1);
    }
    
    // set as opened / initialised
    sdsconnected = 1;

    return(0);
}
