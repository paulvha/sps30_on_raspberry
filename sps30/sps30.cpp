/*******************************************************************
 * 
 * Resources / dependencies:
 * 
 * BCM2835 library (http://www.airspayce.com/mikem/bcm2835/)
 * 
 * The SPS30 monitor can be extended with a DYLOS 1700 monitor.
 * For this "DYLOS" needs to be set. The best way is to use the makefile
 * 
 * To create a build with only the SPS30 monitor type: 
 *      make
 *
 * To create a build with the SPS30 and DYLOS monitor type:
 *      make BUILD=DYLOS  (requires the DYLOS sub-directory)
 * 
 *  To create a build with the SPS30 and SDS011 monitor type:
 *      make BUILD=SDS011 (requires the sds011 sub-directory)
 *
 * To create a build with the SPS30  BOTH DYLOS and SDS011 monitor type:
 *       make BUILD=BOTH
 * 
 * Hardware connection
 * SPS30 pin    Rasberry Pi
 * 1 VCC        +5V
 * 2 SDA        SDA pin 3 / GPIO 2
 * 3 SCL        SCL pin 5 / GPIO 3
 * 4 SELECT     GND (I2C communication)
 * 5 GND        GND
 * 
 * No need for external resistors as pin 3 and 5 have onboard 1k8 
 * pull-up resistors on the Raspberry Pi.
 * 
 * *****************************************************************
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation, either version 3 of the 
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  version 1.4  / April 2020
 *  - Based on the new SPS30 datasheet (March 2020) a number of functions
 *    are added or updated. Some are depending on the new firmware.
 *  - Added sleep() and wakeup(). Requires firmware 2.0
 *  - Added GetVersion() to obtain the current firmware / hardware info
 *  - Added GetStatusReg() to obtain SPS30 status information. Requires firmware 2.2
 *  - Added structure SPS30_version for GetVersion
 *  - Added FWcheck function to check on correct firmware level
 *  - Added INCLUDE_FWCHECK in SPS30lib.h to enable /disable check.
 *  - Changed probe() to obtain firmware levels instead of serial number.
 *  - Changed on how to obtaining product-type
 *  - Depreciated GetArticleCode(). Still supporting backward compatibility
 *  - Update to documentation
 **********************************************************************/

# include "sps30lib.h"
# include <getopt.h>
# include <signal.h>
# include <stdint.h>
# include <stdarg.h>
# include <time.h>
# include <stdlib.h>


#ifdef DYLOS        // DYLOS monitor option

/* indicate these are C-programs and not to be linked */
extern "C" {
    void close_dylos();
    int read_dylos (char * buf, int len, int wait, int verbose);
    int open_dylos(char * device, int verbose);
};

typedef struct dylos
{
    char     port[MAXBUF];   // connected port (like /dev/ttyUSB0)
    bool     include;        // true = include
    uint16_t value_pm10;      // measured value PM10 DC1700
    uint16_t value_pm1;       // measured value PM1  DC1700
} dylos;

#endif //DYLOS

#ifdef SDS011
#include "sds011/sdsmon.h"
SDSmon SDSm;

typedef struct sds
{
    char    port[MAXBUF];   // connected port (like /dev/ttyUSB0)
    bool    include;        // true = include
    float   value_pm25;     // measured value sds
    float   value_pm10;     // measured value sds
} sds;

#endif //SDS011

typedef struct sps_par
{
    /* option SPS30 parameters */
    uint32_t interval;          // set Auto clean interval
    bool    fanclean;          // perform fan clean now
    bool    dev_info_only;     // only display device info.
    
    /* option program variables */
    uint16_t loop_count;        // number of measurement
    uint16_t loop_delay;        // loop delay in between measurements
    bool timestamp;             // add timestamp in output
    int verbose;                // verbose level
    bool   mass;                // display mass
    bool   num;                 // display numbers
    bool   partsize;            // display partsize
    bool   relation;            // include correlation calc (SDS or Dylos)
    bool   DevStatus;            // display device status 
    bool   OptMode ;            //  perform sleep /wake up during wait-time

    /* to store the SPS30 values */
    struct sps_values v;
        
#ifdef DYLOS                    // DYLOS monitor option
    /* include Dylos info */
    struct dylos dylos;
#endif

#ifdef SDS011                      // SDS monitor option
    /* include SDS info */
    struct sds sds;
#endif
} sps_par;

/* used as part of p_printf() */
bool NoColor=false;

/* global constructor */ 
SPS30 MySensor;

char progname[20];

/*********************************************************************
 * @brief Display in color
 * @param format : Message to display and optional arguments
 *                 same as printf
 * @param level :  1 = RED, 2 = GREEN, 3 = YELLOW 4 = BLUE 5 = WHITE
 * 
 * if NoColor was set, output is always WHITE.
 *********************************************************************/
void p_printf(int level, char *format, ...) {
    
    char    *col;
    int     coll=level;
    va_list arg;
    
    //allocate memory
    col = (char *) malloc(strlen(format) + 20);
    
    if (NoColor) coll = WHITE;
                
    switch(coll)  {
        case RED:
            sprintf(col,REDSTR, format);
            break;
        case GREEN:
            sprintf(col,GRNSTR, format);
            break;      
        case YELLOW:
            sprintf(col,YLWSTR, format);
            break;      
        case BLUE:
            sprintf(col,BLUSTR, format);
            break;
        default:
            sprintf(col,"%s",format);
    }

    va_start (arg, format);
    vfprintf (stdout, col, arg);
    va_end (arg);

    fflush(stdout);

    // release memory
    free(col);
}
 
/*********************************************************************
*  @brief close hardware and program correctly
**********************************************************************/
void closeout()
{
   /* reset pins in Raspberry Pi */
   MySensor.close();
   
#ifdef DYLOS        // DYLOS monitor option
   /* close dylos */
   close_dylos();
#endif

#ifdef SDS011       // SDS011 monitor
    SDSm.close_sds();
#endif

   exit(EXIT_SUCCESS);
}

/*********************************************************************
* @brief catch signals to close out correctly 
* @param  sig_num : signal that was raised
* 
**********************************************************************/
void signal_handler(int sig_num)
{
    switch(sig_num)
    {
        case SIGINT:
        case SIGKILL:
        case SIGABRT:
        case SIGTERM:
        default:
#ifdef DYLOS                        // DYLOS monitor option
            printf("\nStopping SPS30 & Dylos monitor\n");
#else
            printf("\nStopping SPS30 monitor\n");
#endif
            closeout();
            break;
    }
}

/*****************************************
 * @brief setup signals 
 *****************************************/
void set_signals()
{
    struct sigaction act;
    
    memset(&act, 0x0,sizeof(act));
    act.sa_handler = &signal_handler;
    sigemptyset(&act.sa_mask);
    
    sigaction(SIGTERM,&act, NULL);
    sigaction(SIGINT,&act, NULL);
    sigaction(SIGABRT,&act, NULL);
    sigaction(SIGSEGV,&act, NULL);
    sigaction(SIGKILL,&act, NULL);
}

/*********************************************
 * @brief generate timestamp
 * 
 * @param buf : returned the timestamp
 *********************************************/  
void get_time_stamp(char * buf)
{
    time_t ltime;
    struct tm *tm ;
    
    ltime = time(NULL);
    tm = localtime(&ltime);
    
    static const char wday_name[][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    
    static const char mon_name[][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    sprintf(buf, "%.3s %.3s%3d %.2d:%.2d:%.2d %d",
    wday_name[tm->tm_wday],  mon_name[tm->tm_mon],
    tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
    1900 + tm->tm_year);
}

/************************************************
 * @brief  initialise the variables 
 * @param sps : pointer to SPS30 parameters
 ************************************************/
void init_variables(struct sps_par *sps)
{
    if (MySensor.begin() != ERR_OK) {
        p_printf(RED,(char *)"Error during setting I2C\n");
        exit(EXIT_FAILURE);
    }
    
    /* option SPS30 parameters */
    sps->interval = 604800;         // default value for autoclean
    sps->fanclean = false;
    sps->dev_info_only = false;
    
    /* option program variables */
    sps->loop_count = 10;           // number of measurement
    sps->loop_delay = 5;            // loop delay in between measurements
    sps->timestamp = false;         // NOT include timestamp in output
    sps->verbose = 0;               // No verbose level
    sps->mass = true;               // display mass by default
    sps->num = true;                // display num by default
    sps->partsize = false;          // display partsize by default
    sps->relation = false;          // display correlation Dylos/SDS
    sps->DevStatus = false;         // display device status 
    sps->OptMode = false;           //  perform sleep /wake up during wait-time

#ifdef DYLOS                        // DYLOS monitor option
    /* Dylos values */
    sps->dylos.include = false;
    sps->dylos.value_pm1 = 0;
    sps->dylos.value_pm10 = 0;
#endif

#ifdef SDS011
    /* SDS values */
    sps->sds.include = false;
    sps->sds.value_pm25 = 0;
    sps->sds.value_pm10 = 0;
#endif
}

/**********************************************************
 * @brief initialise the Raspberry PI and SPS30 / Dylos hardware 
 * @param sps : pointer to SPS30 parameters
 *********************************************************/
void init_hw(struct sps_par *sps)
{
    uint32_t val;
    
    /* progress & debug messages tell driver */
    MySensor.EnableDebugging(sps->verbose);
  
    /* check for auto clean interval update */
    if (MySensor.GetAutoCleanInt(&val) != ERR_OK) {
        p_printf(RED,(char *)"Could not obtain the Auto Clean interval\n");
        closeout();
    }
    
    if (val != sps->interval) {
        if (MySensor.SetAutoCleanInt(sps->interval) != ERR_OK) {
            p_printf(RED,(char *)"Could not set the Auto Clean interval\n");
            closeout();
        }
        else {
            p_printf(GREEN,(char *)"Auto Clean interval has been changed from %d to %d seconds\n",
                val, sps->interval);
        }
    }  

  
#ifdef DYLOS    // DYLOS monitor option

    /* init Dylos DC1700 port */ 
    if (sps->dylos.include)  {
        if(sps->verbose) p_printf (YELLOW, (char *) "initialize Dylos\n");
        
        if (open_dylos(sps->dylos.port, sps->verbose) != 0)   closeout();
    }
#endif // DYLOS

#ifdef SDS011  // SDS011 monitor
    if (sps->sds.include) {
    
        if (sps->verbose) p_printf (YELLOW, (char *) "initialize SDS011\n");
        
        if (SDSm.open_sds(sps->sds.port, sps->verbose) != 0) closeout();
        
        if (sps->verbose) p_printf (YELLOW, (char *) "connected to SDS011\n");
    }
#endif // SDS011
}

#ifdef DYLOS        // DYLOS monitor option
/*****************************************************************
 * @brief Try to read from Dylos DC1700 monitor
 * 
 * @param sps : pointer to SPS30 parameters and Dylos values
 ****************************************************************/
bool dylos_read(struct sps_par *sps)
{
    char    buf[MAXBUF], t_buf[MAXBUF];
    int     ret, i, offset =0 ;
    
    /* if no Dylos device specified */
    if ( ! sps->dylos.include) return(false);
    
    if(sps->verbose > 0 ) printf("\nReading Dylos data ");
    
    /* reset values */
    sps->dylos.value_pm1 = sps->dylos.value_pm10 = 0;
    
    /* try to read from Dylos and wait max 2 seconds */
    ret = read_dylos(buf, MAXBUF, 2, sps->verbose);

    /* if data received : parse it */
    for(i = 0; i < ret; i++)  {
        /* if last byte on line */
        if (buf[i] == 0xa)    {
            /* terminate & get PM10 */
            t_buf[offset] = 0x0;
            sps->dylos.value_pm10 = (uint16_t)strtod(t_buf, NULL);
            
            // break
            i = ret;        
        }
        
        /* skip carriage return and any carbage below 'space' */
        else if (buf[i] != 0xd && buf[i] > 0x1f) {
            t_buf[offset] = buf[i];
        
            /* get PM1 */
            if (t_buf[offset] == ',') {
                t_buf[offset] = 0x0;
                sps->dylos.value_pm1 = (uint16_t)strtod(t_buf, NULL);
                offset=0;
            }
            else
                offset++;
        }
    }
    
    return(true);
}

bool dylos_output(struct sps_par *sps)
{
    /* each minute the DC1700 is providing an average of the particles
     * over the past minute. So we capture during that minute the values 
     * measured bythe SPS30 and once we get a new value from the DC1700 
     * we apply the average of those values for the relation calculation
     */
    static float prevPM10 = 0x0;
    static float prevPM1 = 0x0;
    static float sps30_PM5 = 0;
    static float sps30_PM25 = 0;
    static float sps30_PM10 = 0;
    static float cnt=0;
    
    float temp, temp2,temp3;
    
    if (dylos_read(sps)) {
    
        // capture the current  SPS30 values
        if (sps->dylos.value_pm10 == prevPM10 || sps->dylos.value_pm1 == prevPM1)  {
            sps30_PM5 += sps->v.NumPM2 - sps->v.NumPM0;
            sps30_PM25 += sps->v.NumPM10 - sps->v.NumPM2;
            sps30_PM10 += sps->v.NumPM10 - sps->v.NumPM0;
            cnt++;
            
            p_printf(GREEN, (char *)"DYLOS\t\t\t      waiting new sample within 1 minute\n");
        }
        else {  // new values received from Dylos
            
            p_printf(GREEN, (char *)"DYLOS\t\t\t      PM1: %8d PM10:%8d PPM   (update every minute)\n"
            ,sps->dylos.value_pm1, sps->dylos.value_pm10 );

            if(sps->relation && sps->dylos.value_pm10  > 0) {
                // DYLOS / 283.1685 to convert  cubic feet to cubic centimeter
                temp = (sps->dylos.value_pm1 - sps->dylos.value_pm10) /283.1685;
                temp2 = sps30_PM5 / cnt;
                temp3 = temp/temp2 -1 ;
                p_printf(YELLOW, (char *)"\tCorrelation\t      PM2.5: DYLOS %f\t(avg)SPS30 %f part/cm3 (%3.2f%%)\n",
                temp, temp2, temp3 *100);
                
                temp = sps->dylos.value_pm10 /283.1685;
                temp2 = sps30_PM25 /cnt;
                temp3 = temp/temp2 -1;
                p_printf(YELLOW, (char *)"\t\t\t     >PM2.5: DYLOS %f\t(avg)SPS30 %f part/cm3 (%3.2f%%)\n",
                temp, temp2, temp3 *100);
                
                
                temp = sps->dylos.value_pm1 /283.1685;
                temp2 = sps30_PM10 /cnt;
                temp3 = temp/temp2 -1;
                p_printf(YELLOW, (char *)"\t\t\t      PM10 : DYLOS %f\t(avg)SPS30 %f part/cm3 (%3.2f%%)\n",
                temp, temp2, temp3 *100);
            }
            
            // reset values for next round
            //prevPM10 = sps->dylos.value_pm10;
            //prevPM1 = sps->dylos.value_pm1;
            prevPM1 = prevPM10 = 0;
            sps30_PM5 = sps30_PM25 = sps30_PM10 = cnt=0;
            
                        
            return(true);
        }
    }
    return(false);
}

#endif

#ifdef SDS011
/**
 * @brief read and display SDS information
 * @param sps : stored values
 * 
 * @return
 * false : no display done
 * true  ; display done
 */
bool sds_output(struct sps_par *sps)
{
    float temp;
    
    /* if no SDS device specified */
    if ( ! sps->sds.include) return(false);
    
    // read values from SDS
    if (SDSm.read_sds(&sps->sds.value_pm25, &sps->sds.value_pm10) != 0)
    {
        p_printf(RED, (char*) "error during reading sds\n");
        return(false);
    }

    p_printf(GREEN, (char *)"SDS\t\t\t\t\t    PM2.5: %8.4f\t\t  PM10: %8.4f\n",
    sps->sds.value_pm25, sps->sds.value_pm10 );

    // if relation is requested
    if(sps->relation && sps->sds.value_pm10  > 0) {

        temp = sps->sds.value_pm25/ sps->v.MassPM2 -1 ;
        p_printf(YELLOW, (char *)"\tCorrelation\t\t\t    PM2.5:   %3.2f%%",
        temp *100);
        
        temp = sps->sds.value_pm10/ sps->v.MassPM10 -1 ;
        p_printf(YELLOW, (char *)"\t\t  PM10:   %3.2f%%\n", temp *100);
    }

    return(true);
}
#endif

/*****************************************************************
 * @brief : output the results
 * 
 * @param sps : pointer to SPS30 parameters
 ****************************************************************/
void do_output(struct sps_par *sps)
{
    char buf[30];
    bool output = false;
    uint8_t status;
    
    /* obtain the data */
    if (MySensor.GetValues(&sps->v) != ERR_OK)  {
        p_printf(RED,(char*) "Error during reading data\n");
        closeout();
    }

    if (sps->timestamp)  {
        get_time_stamp(buf);
        p_printf(YELLOW, (char *) "%s\n",buf);
    }
       
    // format output of the data
    if (sps->mass) {
        p_printf(GREEN,(char *) "MASS\t\t\t      PM1: %8.4f PM2.5: %8.4f PM4: %8.4f PM10: %8.4f\n"
        ,sps->v.MassPM1, sps->v.MassPM2, sps->v.MassPM4, sps->v.MassPM10);
        
        output = true;
    }
    
    if (sps->num) {
        p_printf(GREEN,(char *) "NUM\t\tPM0: %8.4F PM1: %8.4f PM2.5: %8.4f PM4: %8.4f PM10: %8.4f\n"
        ,sps->v.NumPM0, sps->v.NumPM1, sps->v.NumPM2, sps->v.NumPM4, sps->v.NumPM10);
        
        output = true;
    }
    
    if (sps->partsize) {
        p_printf(GREEN,(char *) "Partsize\t     %8.4f\n",sps->v.PartSize); 
        
        output = true;
    }
    
    if (sps->DevStatus) {
        if (MySensor.GetStatusReg(&status) == ERR_OK) {
            if (status == 0)
                p_printf(GREEN,(char *) "Device Status\t     No Errors.\n");
            
            else {
                
                if (status & STATUS_SPEED_ERROR)
                    p_printf(RED,(char *) "Device Status\t      WARNING: Fan is turning too fast or too slow\n");
                if (status & STATUS_LASER_ERROR)
                    p_printf(RED,(char *) "Device Status\t      ERROR  : Laser failure\n");
                if (status & STATUS_FAN_ERROR)
                    p_printf(RED,(char *) "Device Status\t      ERROR  : Fan failure : fan is mechanically blocked or broken\n");
            }
        }
        else {
            p_printf(RED,(char *) "Device Status\t     Could Not obtain\n");
        }
        
        output = true;
    }
    
#ifdef DYLOS
    if(dylos_output(sps)) output = true;
#endif

#ifdef SDS011
    if (sds_output(sps)) output = true;
#endif

    if (output)    p_printf(WHITE, (char *) "\n");
    else p_printf(RED, (char *) "Nothing selected to display \n");
}

/*****************************************************************
 * @brief : Display the device information
 * @param sps : pointer to SPS30 parameters
 ****************************************************************/
uint8_t disp_dev(struct sps_par *sps)
{
    char    buf[35];
    SPS30_version gv;
        
    /* get the serial number (check that communication works) */
    if(MySensor.GetSerialNumber(buf, 35) != ERR_OK) {
       p_printf (RED, (char *) "Error during getting serial number\n");
       return(ERR_PROTOCOL);
    }
    
    if (strlen(buf) == 0) 
        p_printf(YELLOW, (char *) "NO serialnumber available\n");
    else
        p_printf(YELLOW, (char *) "Serialnumber   %s\n", buf);

    /* get the article code */
    if(MySensor.GetProductName(buf, 35) != ERR_OK) {
       p_printf (RED, (char *) "Error during getting product type\n");
       return(ERR_PROTOCOL);
    }

    if (strlen(buf) == 0) 
        p_printf(YELLOW, (char *) "NO product type available\n");
    else
        p_printf(YELLOW, (char *) "Article code   %s\n", buf);
    
    if(MySensor.GetVersion(&gv) != ERR_OK) {
       p_printf (RED, (char *) "Error during getting firmware level\n");
       return(ERR_PROTOCOL);
    }

    p_printf(YELLOW, (char *) "SPS30 Firmware %d.%d\n",gv.major,gv.minor);   
    
    return(ERR_OK);
}

/*****************************************************************
 * @brief Here the main of the program 
 * @param sps : pointer to SPS30 parameters
 ****************************************************************/
void main_loop(struct sps_par *sps)
{
    int     loop_set, reset_retry = RESET_RETRY;
    bool    first=true;
   
    if (disp_dev(sps) != ERR_OK) return;
    
    /* if only device info was requested */
    if (sps->dev_info_only) return;

    /* instruct to start reading */
    if (MySensor.start() == false) {
        p_printf(RED,(char *)  "Can not start measurement:\n");
        return;
    }
    
    p_printf(GREEN,(char *)  "Starting SPS30 measurement:\n");

    /* check for manual fan clean (can only be done after start) */
    if(sps->fanclean) {
        
        if (MySensor.clean()) 
            p_printf(BLUE,(char *)"A manual fan clean instruction has been sent\n");
        else
            p_printf(RED,(char *)"Could not force a manual fan clean\n");
    }
                    
    /*  check for endless loop */
    if (sps->loop_count > 0 ) loop_set = sps->loop_count;
    else loop_set = 1;
    
    /* loop requested */
    while (loop_set > 0)  {
        
        if(MySensor.Check_data_ready()) {
            reset_retry = RESET_RETRY;
            do_output(sps);
        }
        else  {
            if (reset_retry-- == 0) {
                
                p_printf (RED, (char *) "Retry count exceeded. perform softreset\n");
                MySensor.reset();
                reset_retry = RESET_RETRY;
                first = true;
            }
            else  {
                /* Prevent message when previous mode of the SPS30 was 
                 * STOP continuous measurement. It needs 4 seconds 
                 * at least for the first results in that case */
                 
                if (first)  first = false;
                else printf("no data available\n");
            }
        }
        
        // if sleep was requisted during wait
        if (sps->OptMode) MySensor.sleep();
        
        /* delay for seconds */
        sleep(sps->loop_delay);

        // if sleep was requisted during wait
        if (sps->OptMode) MySensor.wakeup();        
        
        /* check for endless loop */
        if (sps->loop_count > 0) loop_set--;
    }
    
    printf("Reached the loopcount of %d.\nclosing down\n", sps->loop_count);
}       

/*********************************************************************
* @brief usage information  
* @param sps : pointer to SPS30 parameters
**********************************************************************/
void usage(struct sps_par *sps)
{
    printf(    "%s [options]  (program version %d.%d) \n\n"
    
    "SPS30 settings: \n"
    "-a #   set Auto clean interval in seconds\n"
    "-A     set Auto clean interval to factory setting (604800 seconds)\n"
    "-m     perform a manual clean\n"
    "-d     display serial-number, product type and firmware level only\n"
    
    "\nprogram settings\n"
    "-B     do not display output in color\n"
    "-l #   number of measurements (0 = endless)      (default %d)\n"
    "-w #   wait-time (seconds) between measurements  (default %d)\n"
    "-v #   verbose / debug level (0 - 2)             (default %d)\n"
    "-T     add / remove timestamp to output          (default %s)\n"
    "-E     add / remove display device error   (*1)  (default %s)\n"
    "-F     add / remove sleep during wait-time (*2)  (default %s)\n"
    "-M     add / remove MASS info to output          (default %s)\n"
    "-N     add / remove NUMBERS info to output       (default %s)\n"
    "-P     add / remove Partsize info to output      (default %s)\n"
    "\n\t*1 : requires SPS30 firmware level 2.2 or higher\n"
    "\t*2 : requires SPS30 firmware level 2.0 or higher\n"
    
#ifdef DYLOS 
    "\nDylos DC1700: \n"
    "-D port    Enable Dylos input from port          (No default)\n"
    "-C     add correlation calculation               (default %s)\n"
#endif    

#ifdef SDS011
    "\nSDS011: \n"
    "-S port    Enable SDS011 input from port         (No default)\n"
    "-C     add correlation calculation               (default %s)\n"
#endif    
   , progname, DRIVER_MAJOR, DRIVER_MINOR, sps->loop_count, sps->loop_delay, 
   sps->verbose,
   sps->timestamp?"added":"removed",  
   sps->DevStatus?"added":"removed", 
   sps->OptMode?"added":"removed", 
   sps->mass?"added":"removed", 
   sps->num?"added":"removed",
   sps->partsize?"added":"removed"
#ifdef DYLOS 
   ,
   sps->relation?"added":"removed"
#endif

#ifdef SDS011
   ,
   sps->relation?"added":"removed");
#else
    );
#endif
}

/*********************************************************************
 * Parse parameter input 
 * @param sps : pointer to SPS30 parameters
 * @param opt: option character
 * @param option : option argunment
 *********************************************************************/ 

void parse_cmdline(int opt, char *option, struct sps_par *sps)
{
    switch (opt) {

    case 'h':   // display help
    case 'H':
        usage(sps);
        exit(EXIT_SUCCESS);
        break;
                
    case 'm':   // set Automatic manual fanclean
        sps->fanclean = true;
        break;

    case 'a':   // SPS30 automatic clean interval 
        sps->interval = (uint32_t) strtod(option, NULL);
        
        // must be valid
        if (sps->interval < 0 )
        {
            p_printf (RED, (char *) "Incorrect interval %d. Must be positive\n", sps->interval);
            exit(EXIT_FAILURE);
        }
        break;
                     
    case 'A':   // SPS30 reset automatic clean interval 
        sps->interval = 604800;
        break;
         
    case 'd':   // display device info only
        sps->dev_info_only = true;
        break;

    case 'M':   // toggle mass display
        sps->mass = ! sps->mass;
        break;

    case 'N':   // toggle num display
        sps->num = ! sps->num;
        break;
        
     case 'P':   // toggle partsize display
        sps->partsize = ! sps->partsize;
        break;       
                
    case 'B':   // set for no color output
        NoColor = true;
        break; 
              
    case 'l':   // loop count
        sps->loop_count = (uint16_t) strtod(option, NULL);
        break;
          
    case 'w':   // loop delay in between measurements
        sps->loop_delay = (uint16_t) strtod(option, NULL);
        break;
    
    case 'T':  // toggle timestamp to output
        sps->timestamp = ! sps->timestamp;
        break;

    case 'E':  // toggle display device errors
        if (MySensor.FWCheck(2,2))  
            sps->DevStatus = ! sps->DevStatus;
        else {
            p_printf (RED, (char *) "Can enable display device error status\n");
            p_printf (RED, (char *) "SPS30 firmware does not have minimum level of 2.2\n");
        }
        break;
        
    case 'F':  // toggle force sleep during wait-time
        if (MySensor.FWCheck(2,0))  
            sps->OptMode = ! sps->OptMode;
        else {
            p_printf (RED, (char *) "Can set sleep during wait-time\n");
            p_printf (RED, (char *) "SPS30 firmware does not have minimum level of 2.0\n");
        }
        break;               

    case 'v':   // set verbose / debug level
        sps->verbose = (int) strtod(option, NULL);

        // must be between 0 and 2
        if (sps->verbose < 0 || sps->verbose > 2)
        {
            p_printf (RED, (char *) "Incorrect verbose/debug. Must be  0,1, 2 \n");
            exit(EXIT_FAILURE);
        }
        break;
    case 'C':   // toggle correlation calculation
        sps->relation = ! sps->relation;
        break;
                
    case 'D':   // include Dylos read
#ifdef DYLOS
        strncpy(sps->dylos.port, option, MAXBUF);
        sps->dylos.include = true;
#else
        p_printf(RED, (char *) "Dylos is not supported in this build\n");
#endif
        break;
        
    case 'S':   // include SDS011 read
#ifdef SDS011        
        strncpy(sps->sds.port, option, MAXBUF);
        sps->sds.include = true;
#else
        p_printf(RED, (char *) "SDS011 is not supported in this build\n");
#endif
        break;    
    
    default: /* '?' */
        usage(sps);
        exit(EXIT_FAILURE);
    }
}

/***********************
 *  program starts here
 **********************/
int main(int argc, char *argv[])
{
    int opt;
    struct sps_par sps; // parameters

    if (geteuid() != 0)  {
        p_printf(RED,(char *) "You must be super user\n");
        exit(EXIT_FAILURE);
    }
    
    /* set signals */
    set_signals(); 
 
    /* save name for (potential) usage display */
    strncpy(progname,argv[0],20);
    
    /* set the initial values */
    init_variables(&sps);

    /* parse commandline */
    while ((opt = getopt(argc, argv, "CAa:mdBl:v:w:EFTHhMNPD:S:")) != -1) {
        parse_cmdline(opt, optarg, &sps);
    }

    /* initialise hardware */
    init_hw(&sps);

    /* main loop to read SPS30 results */
    main_loop(&sps);

    closeout();
}
