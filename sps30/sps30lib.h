/**
 * SPS30 I2C Library Header file for Raspberry Pi
 *
 * Copyright (c) April 2019, Paul van Haastrecht
 *
 * All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 **********************************************************************
 * Initial version by paulvha version 12 May 2019
 * 
 * version 1.4  / April 2020
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
 *********************************************************************
*/
#ifndef SPS30_H
#define SPS30_H

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <bcm2835.h>

/**
 * library version levels
 */
#define DRIVER_MAJOR 1
#define DRIVER_MINOR 4

/**
 * ADDED version 1.4
 * New firmware levels have been slipped streamed into the SPS30
 * The datasheet from March 2020 shows added / updated functions on new
 * firmware level. E.g. sleep(), wakeup(), status register are new
 *
 * On serial connection the new functions are accepted and positive
 * acknowledged on lower level firmware, but execution does not seem
 * to happen or should be expected.
 *
 * On I2C reading Status register gives an error on lower level firmware.
 * Sleep and wakeup are accepted and positive acknowledged on lower level
 * firmware, but execution does not seem to happen or should be expected.
 *
 * Starting version 1.4 of this driver a firmware level check has been implemented
 * and in case a function is called that requires a higher level than
 * on the current SPS30, it will return an error.
 * By setting INCLUDE_FWCHECK to 0, this check can be disabled
 */
#define INCLUDE_FWCHECK 1

/* structure to return all values */
struct sps_values
{
    float   MassPM1;        // Mass Concentration PM1.0 [μg/m3]
    float   MassPM2;        // Mass Concentration PM2.5 [μg/m3]
    float   MassPM4;        // Mass Concentration PM4.0 [μg/m3]
    float   MassPM10;       // Mass Concentration PM10 [μg/m3]
    float   NumPM0;         // Number Concentration PM0.5 [#/cm3]
    float   NumPM1;         // Number Concentration PM1.0 [#/cm3]
    float   NumPM2;         // Number Concentration PM2.5 [#/cm3]
    float   NumPM4;         // Number Concentration PM4.0 [#/cm3]
    float   NumPM10;        // Number Concentration PM4.0 [#/cm3]
    float   PartSize;       // Typical Particle Size [μm]
};

/* used to get single value */
#define v_MassPM1 1
#define v_MassPM2 2
#define v_MassPM4 3
#define v_MassPM10 4
#define v_NumPM0 5
#define v_NumPM1 6
#define v_NumPM2 7
#define v_NumPM4 8
#define v_NumPM10 9
#define v_PartSize 10

/* needed for conversion float IEE754 */
typedef union {
    uint8_t array[4];
    float value;
} ByteToFloat;

/* needed for auto interval timing */
typedef union {
    uint8_t array[4];
    uint32_t value;
} ByteToU32;

/*************************************************************/
/* error codes */
#define ERR_OK          0x00
#define ERR_DATALENGTH  0X01
#define ERR_UNKNOWNCMD  0x02
#define ERR_ACCESSRIGHT 0x03
#define ERR_PARAMETER   0x04
#define ERR_OUTOFRANGE  0x28
#define ERR_CMDSTATE    0x43
#define ERR_TIMEOUT     0x50
#define ERR_PROTOCOL    0x51
#define ERR_FIRMWARE    0x88        // added version 1.4

struct Description {
    uint8_t code;
    char    desc[80];
};

/* Receive buffer length*/
#define MAXBUF 100

/* pre-set to retry reading SPS30 */
#define RESET_RETRY 5

/* I2C COMMUNICATION INFORMATION  */
#define I2C_START_MEASUREMENT       0x0010
#define I2C_STOP_MEASUREMENT        0x0104
#define I2C_READ_DATA_RDY_FLAG      0x0202
#define I2C_READ_MEASURED_VALUE     0x0300
#define I2C_SLEEP                   0X1001  // ADDED 1.4
#define I2C_WAKEUP                  0X1002  // ADDED 1.4
#define I2C_START_FAN_CLEANING      0x5607
#define I2C_AUTO_CLEANING_INTERVAL  0x8004
#define I2C_SET_AUTO_CLEANING_INTERVAL      0x8005

#define I2C_READ_PRODUCT_TYPE       0xD002 // CHANGED 1.4
#define I2C_READ_SERIAL_NUMBER      0xD033
#define I2C_READ_VERSION            0xD100 // ADDED 1.4
#define I2C_READ_STATUS_REGISTER    0xD206 // ADDED 1.4
#define I2C_CLEAR_STATUS_REGISTER   0xD206 // ADDED 1.4 (NOT USED)
#define I2C_RESET                   0xD304

/**
 * added version 1.4
 *
 * New call was explained to obtain the version levels
 * datasheet SPS30 March 2020, page 14
 *
 */
struct SPS30_version {
    uint8_t major;                  // Firmware level
    uint8_t minor;
    uint8_t DRV_major;
    uint8_t DRV_minor;
};

/**
 * added version 1.4
 *
 * Status register result
 *
 * REQUIRES FIRMWARE LEVEL 2.2
 */
enum SPS_status {
    STATUS_OK = 0,
    STATUS_SPEED_ERROR = 1,
    STATUS_LASER_ERROR = 2,
    STATUS_FAN_ERROR = 4
};

/**
 * added version 1.4
 *
 * Measurement can be done in FLOAR or unsigned 16bits
 * page 6 datasheet SPS30 page 6.
 *
 * This driver only uses float
 */
#define START_MEASURE_FLOAT           0X03
#define START_MEASURE_UNS16           0X05


/* I2c address */
#define SPS30_ADDRESS 0x69          

class SPS30
{
  public:

    SPS30(void);

   /**
    * @brief  Enable or disable the printing of sent/response HEX values.
    *
    * @param act : level of debug to set
    *  0 : no debug message
    *  1 : sending and receiving data
    *  2 : 1 +  protocol progress
    */
    void EnableDebugging(uint8_t act);

    /**
     * @brief Initialize the communication port
     * 
     * @return
     *  OK = ERR_OK
     *  else error
     */
    uint8_t begin();
 
    /**
     * @brief close the communication port
     */   
    void close();

    /**
     * @brief : Perform SPS-30 instructions
     *
     * @return 
     *  true = ok
     *  false = error
     */
    bool probe();
    bool reset() {return(Instruct(I2C_RESET));}
    bool start() {return(Instruct(I2C_START_MEASUREMENT));}
    bool stop()  {return(Instruct(I2C_STOP_MEASUREMENT));}
    bool clean() {return(Instruct(I2C_START_FAN_CLEANING));}

    /**
     * Added 1.4
     * @brief Set SPS30 to sleep or wakeup
     * Requires Firmwarelevel 2.0
     */
    uint8_t sleep() {return(SetOpMode(I2C_SLEEP));}
    uint8_t wakeup(){return(SetOpMode(I2C_WAKEUP));}
    /**
     * @brief : Set or get Auto Clean interval
     * @param val : value to get (pointer) or set
     * 
     * @return
     *  OK = ERR_OK
     *  else error
     */
    uint8_t GetAutoCleanInt(uint32_t *val);
    uint8_t SetAutoCleanInt(uint32_t val);

    /**
     * @brief : retrieve error message details
     * Maximum length is 80 characters
     * 
     * @param code : error code
     * @param buf  : buffer to store the description
     * @param len  : length of buffer
     */
    void GetErrDescription(uint8_t code, char *buf, uint8_t len);

    /**
     * @brief : obtain device information from the SPS-30
     * maximum length is 32 characters
     * 
     * @param ser     : buffer to hold the read result
     * @param len     : length of the buffer
     * 
     * @return
     *  OK = ERR_OK
     *  else error
     */
    uint8_t GetSerialNumber(char *ser, uint8_t len) {return(Get_Device_info(I2C_READ_SERIAL_NUMBER, ser, len));}
    uint8_t GetProductName(char *ser, uint8_t len)  {return(Get_Device_info(I2C_READ_PRODUCT_TYPE, ser, len));}      // CHANGED 1.4

    /**
     * CHANGED 1.4
     * Depreciated in Datasheet March 2020
     * left for backward compatibility with older sketches
     */
    uint8_t GetArticleCode(char *ser, uint8_t len)  {ser[0] = 0x0; return ERR_OK;}
    
    /** ADDED 1.4
     * @brief : retrieve software/hardware version information from the SPS-30
     *
     */
    uint8_t GetVersion(SPS30_version *v);

    /** ADDED 1.4
     * @brief : Read Device Status from the SPS-30
     *
     * REQUIRES FIRMWARE 2.2
     * The commands are accepted and positive acknowledged on lower level
     * firmware, but do not execute.
     *
     */
    uint8_t GetStatusReg(uint8_t *status);
    
    
    
    /**
     * @brief : check for new data available on SPS-30
     * Return
     *  true  if available
     *  false if not
     */
    bool Check_data_ready();

    /**
     * @brief : obtain all measurement values from SPS-30
     * @param : pointer to structure to store
     * 
     * @return
     *  OK = ERR_OK
     *  else error
     */
    uint8_t GetValues(struct sps_values *v);

    /**
     * @brief : obtain a specific value from the SPS-30
     * 
     *  * Return :
     *  OK value
     *  else -1
     */
    float GetMassPM1()  {return(Get_Single_Value(v_MassPM1));}
    float GetMassPM2()  {return(Get_Single_Value(v_MassPM2));}
    float GetMassPM4()  {return(Get_Single_Value(v_MassPM4));}
    float GetMassPM10() {return(Get_Single_Value(v_MassPM10));}
    float GetNumPM0()   {return(Get_Single_Value(v_NumPM0));}
    float GetNumPM1()   {return(Get_Single_Value(v_NumPM1));}
    float GetNumPM2()   {return(Get_Single_Value(v_NumPM2));}
    float GetNumPM4()   {return(Get_Single_Value(v_NumPM4));}
    float GetNumPM10()  {return(Get_Single_Value(v_NumPM10));}
    float GetPartSize() {return(Get_Single_Value(v_PartSize));}


    /**
     * @brief Check SPS30 Firmware level
     *
     * @param  Major : minimum Major level of firmware
     * @param  Minor : minimum Minor level of firmware
     *
     * return 
     *  true if SPS30 has this mininum level
     *  false is NOT
     */
    bool FWCheck(uint8_t major, uint8_t minor); // added 1.4
    
  private:

    /** shared variables */
    uint8_t _Receive_BUF[MAXBUF];  // buffers
    uint8_t _Send_BUF[10];
    uint8_t _Receive_BUF_Length;
    uint8_t _Send_BUF_Length;
    int _SPS30_Debug;           // program debug level
    bool _started;              // indicate the measurement has started
    bool _sleep;                // indicate that SPS30 is in sleep (added 1.4)
    bool _WasStarted;           // restart if SPS30 was started before setting sleep (added 1.4)
    uint8_t _FW_Major, _FW_Minor;  // holds firmware major (added 1.4)
    uint8_t Reported[11];      // use as cache indicator single value

    /** shared supporting routines */
    uint8_t Get_Device_info(uint32_t type, char *ser, uint8_t len);
    bool Instruct(uint32_t type);
    uint8_t SetOpMode(uint16_t mode);            // added 1.4
    float Get_Single_Value(uint8_t value);
    float byte_to_float(int x);
    uint32_t byte_to_U32(int x);

    /** I2C communication */
    uint8_t I2C_init();
    void I2C_close();
    void I2C_fill_buffer(uint32_t cmd, uint32_t interval = 0);
    uint8_t I2C_ReadToBuffer(uint8_t count, bool chk_zero);
    uint8_t I2C_SetPointer_Read(uint8_t cnt, bool chk_zero = false);
    uint8_t I2C_SetPointer();
    bool I2C_Check_data_ready();
    uint8_t I2C_calc_CRC(uint8_t data[2]);
};

/*! to display in color  */
void p_printf (int level, char *format, ...);

// color display enable
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define WHITE   5

#define REDSTR "\e[1;31m%s\e[00m"
#define GRNSTR "\e[1;92m%s\e[00m"
#define YLWSTR "\e[1;93m%s\e[00m"
#define BLUSTR "\e[1;34m%s\e[00m"

// disable color output
extern bool NoColor;

#endif /* SPS30_H */
