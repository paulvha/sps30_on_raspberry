/**
 * SPS30 I2C Library file for Raspberry Pi
 *
 * Copyright (c) May 2019, Paul van Haastrecht
 *
 * All rights reserved.
 *
 * ================ Disclaimer ===================================
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
 */

#include "sps30lib.h"

/* error descripton */
struct Description ERR_desc[10] =
{
  {ERR_OK, "All good"},
  {ERR_DATALENGTH, "Wrong data length for this command (too much or little data)"},
  {ERR_UNKNOWNCMD, "Unknown command"},
  {ERR_ACCESSRIGHT, "No access right for command"},
  {ERR_PARAMETER, "Illegal command parameter or parameter out of allowed range"},
  {ERR_OUTOFRANGE, "Internal function argument out of range"},
  {ERR_CMDSTATE, "Command not allowed in current state"},
  {ERR_TIMEOUT, "No response received within timeout period"},
  {ERR_PROTOCOL, "Protocol error"},
  {0xff, "Unknown Error"}
};


/**
 * @brief constructor and initialize variables
 */
SPS30::SPS30(void)
{
  _Send_BUF_Length = 0;
  _Receive_BUF_Length = 0;
  _SPS30_Debug = 0;
  _started = false;
  memset(Reported,0x1,sizeof(Reported));     // Trigger reading single value cache
}

/**
 * @brief Initialize the I2C communication port
 */
uint8_t SPS30::begin()
{
    return(I2C_init());
}

/**
 * @brief Close the communication port
 */
void SPS30::close()
{
    return(I2C_close());
}

/**
 * @brief  Enable or disable the printing of sent/response HEX values.
 *
 * @param act : level of debug to set
 *  0 : no debug message
 *  1 : sending and receiving data
 *  2 : 1 +  protocol progress
 */
void SPS30::EnableDebugging(uint8_t act)
{
    _SPS30_Debug = act;
}

/**
 * @brief check if SPS30 sensor is available (read serial number)
 *
 * Return:
 *   true on success else false
 */
bool SPS30::probe() 
{
    char buf[32];

    if (GetSerialNumber(buf, 32) == ERR_OK) return(true);
    return(false);
}

/**
 * @brief Instruct SPS30 sensor
 * @param type
 *  I2C_START_MEASUREMENT   Start measurement
 *  I2C_STOP_MEASUREMENT    Stop measurement
 *  I2C_RESET               Perform reset
 *  I2C_START_FAN_CLEANING  start cleaning
 *
 * @return 
 *  true = ok
 *  false = error
 */
bool SPS30::Instruct(uint32_t type)
{
    if (type == I2C_START_FAN_CLEANING)
    {
        if(_started == false)
        {
            if (_SPS30_Debug){
                printf("ERROR : Sensor is not in measurement mode\n");
            }
            return(false);
        }
    }
    
    I2C_fill_buffer(type);

    if (I2C_SetPointer() == ERR_OK){

        if (type == I2C_START_MEASUREMENT) {
            _started = true;
            delay(1000);
        }
        else if (type == I2C_STOP_MEASUREMENT)
            _started = false;

        else if (type == I2C_RESET){
            _started = false;
            delay(2000);
        }

        return(true);
    }

    return(false);
}

/**
 * @brief : General Read device info
 *
 * @param type:
 *  Article Code  : I2C_READ_ARTICLE_CODE
 *  Serial Number : I2C_READ_SERIAL_NUMBER
 *
 * @param ser     : buffer to hold the read result
 * @param len     : length of the buffer
 *
 * return
 *  ERR_OK = ok
 *  else error
 */
uint8_t SPS30::Get_Device_info(uint32_t type, char *ser, uint8_t len)
{
    uint8_t i, ret;

    I2C_fill_buffer(type);
    
    ret = I2C_SetPointer_Read(len,true);

    if (ret != ERR_OK) return (ret);

    // get data
    for (i = 0; i < len ; i++) {
        ser[i] = _Receive_BUF[i];
        if (ser[i] == 0x0) break;
    }

    return(ERR_OK);
}

/**
 * @brief : read the auto clean interval
 * @param val : pointer to return the interval value
 *
 * The default cleaning interval is set to 604’800 seconds (i.e. 168 hours or 1 week).
 *
 * Return:
 *  OK = ERR_OK
 *  else error
 */
uint8_t SPS30::GetAutoCleanInt(uint32_t *val)
{
    uint8_t ret;

    I2C_fill_buffer(I2C_AUTO_CLEANING_INTERVAL);

    ret = I2C_SetPointer_Read(4);

    // get data
    *val = byte_to_U32(0);

    return(ret);
}

/**
 * @brief : SET the auto clean interval
 * @param val : pointer for the interval value
 *
 *
 * @return
 *  OK = ERR_OK
 *  else error
 */
uint8_t SPS30::SetAutoCleanInt(uint32_t val)
{
    bool save_started, r;

    I2C_fill_buffer(I2C_SET_AUTO_CLEANING_INTERVAL, val);

    if (I2C_SetPointer() == ERR_OK)
    {
        /* Datasheet page 15: Note that after writing a new interval, this will be activated immediately.
         * However, if the interval register is read out after setting the new value, the previous value
         * is returned until the next start/reset of the sensor module.
         *
         * A reset() alone will NOT do the job. It will continue to show the old value. The only way is to perform
         * a low level I2C line reset first and then perform a reset()*/
        save_started = _started;

        // flush and release lines
        I2C_close();
        delay(1000);
        I2C_init();

        r = reset();

        // do we need to restart ?
        if (r) {if (save_started) r = start();}

        if (r) return(ERR_OK);
    }

    return(ERR_PROTOCOL);
}

/**
 * @brief : get single sensor value
 * @param value : the single value to get
 *
 * This routine has a cache indicator and will provide a specific value
 * only once. A second request will trigger a re-read of all information.
 *
 * This will reduce overhead and allow the user to collect invidual data
 * that has been obtained at the same time.
 *
 * Return :
 *  OK value
 *  else -1
 */
float SPS30::Get_Single_Value(uint8_t value)
{
    static struct sps_values v;

    if (value > v_PartSize) return(-1);

    // if already reported this value
    if (Reported[value]) {
        // do a reload
        if (GetValues(&v) != ERR_OK) return(-1);
        memset(Reported,0x0,sizeof(Reported));
    }

    Reported[value] = 1;

    switch(value){
        case v_MassPM1:  return(v.MassPM1);
        case v_MassPM2:  return(v.MassPM2);
        case v_MassPM4:  return(v.MassPM4);
        case v_MassPM10: return(v.MassPM10);
        case v_NumPM0:   return(v.NumPM0);
        case v_NumPM1:   return(v.NumPM1);
        case v_NumPM2:   return(v.NumPM2);
        case v_NumPM4:   return(v.NumPM4);
        case v_NumPM10:  return(v.NumPM10);
        case v_PartSize: return(v.PartSize);
    }
    
    // stop WALL complaining
    return(0);
}

/**
 * @brief : get error description
 * @param code : error code
 * @param buf  : buffer to store the description
 * @param len  : length of buffer
 */
void SPS30::GetErrDescription(uint8_t code, char *buf, uint8_t len)
{
    int i=0;

    while (ERR_desc[i].code != 0xff) {
        if(ERR_desc[i].code == code)  break;
        i++;
    }

    strncpy(buf, ERR_desc[i].desc, len);
}

/**
 * @brief : read all values from the sensor and store in structure
 * @param : pointer to structure to store
 *
 * return
 *  ERR_OK = ok
 *  else error
 */
uint8_t SPS30::GetValues(struct sps_values *v)
{
    uint8_t ret, loop = 0;

    // measurement started already?
    if (_started == false) {
        if (start() == false) return(ERR_CMDSTATE);
    }

    do {
        // if new data available
        if (Check_data_ready())
        {
            I2C_fill_buffer(I2C_READ_MEASURED_VALUE);
            
            ret = I2C_SetPointer_Read(40);
            break;
        }
        else
        {
            delay(1000);
        }
    } while(loop++ < 3);

    if (loop == 3) return(ERR_TIMEOUT);
    if (ret != ERR_OK) return(ERR_PROTOCOL);
    
    memset(v,0x0,sizeof(struct sps_values));

    // get data
    v->MassPM1 = byte_to_float(0);
    v->MassPM2 = byte_to_float(4);
    v->MassPM4 = byte_to_float(8);
    v->MassPM10 = byte_to_float(12);
    v->NumPM0 = byte_to_float(16);
    v->NumPM1 = byte_to_float(20);
    v->NumPM2 = byte_to_float(24);
    v->NumPM4 = byte_to_float(28);
    v->NumPM10 = byte_to_float(32);
    v->PartSize = byte_to_float(36);

    return(ERR_OK);
}

/**
 * @brief : translate 4 bytes to float IEEE754
 * @param x : offset in _Receive_BUF
 *
 * return : float number
 */
float SPS30::byte_to_float(int x)
{
    ByteToFloat conv;

    for (uint8_t i = 0; i < 4; i++){
        conv.array[3-i] = _Receive_BUF[x+i]; //or conv.array[i] = _Receive_BUF[x+i]; depending on endianness
    }

    return conv.value;
}

/**
 * @brief : translate 4 bytes to Uint32
 * @param x : offset in _Receive_BUF
 *
 * Used for Get Auto Clean interval
 * return : uint32_t number
 */
uint32_t SPS30::byte_to_U32(int x)
{
    ByteToU32 conv;

    for (uint8_t i = 0; i < 4; i++){
        conv.array[3-i] = _Receive_BUF[x+i]; //or conv.array[i] = _Receive_BUF[x+i]; depending on endianness
    }

    return conv.value;
}

/**
 * @brief : Start I2C communication
 * 
 * @return
 * All good : ERR_OK
 * else error
 * 
 */
uint8_t SPS30::I2C_init()
{
     if (!bcm2835_init()) {
        printf("Can't init bcm2835!\n");
        return(ERR_PROTOCOL);
    }

    // will select I2C channel 0 or 1 depending on board version.
    if (!bcm2835_i2c_begin()) {
        printf("Can't setup I2c pin!\n");
        
        // release BCM2835 library
        bcm2835_close();
        return(ERR_PROTOCOL);
    }
    
    /* set BSC speed to 100Khz*/
    bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500);
    
    /* set BCM2835 slaveaddress */
    bcm2835_i2c_setSlaveAddress(SPS30_ADDRESS);
    
    return(ERR_OK);
}

/**
 * @brief : close library and reset pins.
 */
void SPS30::I2C_close()
{
    // reset pins
    bcm2835_i2c_end();  
    
    // release BCM2835 library
    bcm2835_close();
}

/**
 * @brief : Fill buffer to send over I2C communication
 * @param cmd: I2C commmand
 * @param interval : value to set for interval
 */
void SPS30::I2C_fill_buffer(uint32_t cmd, uint32_t interval)
{
    memset(_Send_BUF,0x0,sizeof(_Send_BUF));
    _Send_BUF_Length = 0;

    int i = 0 ;

    // add command
    _Send_BUF[i++] = cmd >> 8 & 0xff;   //0 MSB
    _Send_BUF[i++] = cmd & 0xff;        //1 LSB

    // add / change information if needed
    switch(cmd) {

        case I2C_START_MEASUREMENT:
            _Send_BUF[i++] = 0x3;       //2 Measurement-Mode, this value must be set to 0x03
            _Send_BUF[i++] = 0x00;      //3 dummy byte
            _Send_BUF[i++] = I2C_calc_CRC(&_Send_BUF[2]);
            break;

        case I2C_SET_AUTO_CLEANING_INTERVAL: // when SETTING interval
            i=0;
            _Send_BUF[i++] = I2C_AUTO_CLEANING_INTERVAL >> 8 & 0xff;   //0 MSB
            _Send_BUF[i++] = I2C_AUTO_CLEANING_INTERVAL & 0xff;        //1 LSB
            _Send_BUF[i++] = interval >> 24 & 0xff;       //2 MSB
            _Send_BUF[i++] = interval >> 16 & 0xff;       //3 MSB
            _Send_BUF[i++] = I2C_calc_CRC(&_Send_BUF[2]); //4 CRC
            _Send_BUF[i++] = interval >>8 & 0xff;         //5 LSB
            _Send_BUF[i++] = interval & 0xff;             //6 LSB
            _Send_BUF[i++] = I2C_calc_CRC(&_Send_BUF[5]); //7 CRC
            break;
        }

     _Send_BUF_Length = i;
}

/**
 * @brief : SetPointer (and write if included) with I2C communication
 *
 * return:
 * Ok ERR_OK
 * else error
 */
uint8_t SPS30::I2C_SetPointer()
{
    if (_Send_BUF_Length == 0) return(ERR_DATALENGTH);

    if (_SPS30_Debug == 1){
        printf("I2C Sending: ");
        for(uint8_t i = 0; i < _Send_BUF_Length; i++)
            printf(" 0x%02X", _Send_BUF[i]);
        printf("\n");
    }

    switch(bcm2835_i2c_write( (char*) _Send_BUF, _Send_BUF_Length))
    {
        case BCM2835_I2C_REASON_ERROR_NACK :
            if(_SPS30_Debug == 2) printf(REDSTR,"DEBUG: Write NACK error\n");
            return(ERR_PROTOCOL);
            break;

        case BCM2835_I2C_REASON_ERROR_CLKT :
            if(_SPS30_Debug == 2) printf(REDSTR,"DEBUG: Write Clock stretch error\n");
            return(ERR_PROTOCOL);
            break;

        case BCM2835_I2C_REASON_ERROR_DATA :
            if(_SPS30_Debug ==2) printf(REDSTR,"DEBUG: not all data has been written\n");
            return(ERR_PROTOCOL);
            break;
    }

    //give time to settle
    usleep(500);

    return(ERR_OK);
}

/**
 * @brief : read with I2C communication
 * @param cnt: number of data bytes to get
 * @param chk_zero : needed for read info buffer
 *  false : expect all the bytes
 *  true  : expect NULL termination and cnt is MAXIMUM byte
 *
 */
uint8_t SPS30::I2C_SetPointer_Read(uint8_t cnt, bool chk_zero)
{
    uint8_t ret;

    // set pointer
    ret = I2C_SetPointer();
    if (ret != ERR_OK) {
        if (_SPS30_Debug == 2) printf(REDSTR,"Can not set pointer\n");
        return(ret);
    }

    // read from Sensor
    ret = I2C_ReadToBuffer(cnt, chk_zero);

    if (_SPS30_Debug){
       printf("I2C Received: ");
       for(uint8_t i = 0; i < _Receive_BUF_Length; i++) printf("0x%02X ",_Receive_BUF[i]);
       printf("length: %d\n\n",_Receive_BUF_Length);
    }

    if (ret != ERR_OK) {
        if (_SPS30_Debug == 2){
            printf("Error during reading from I2C: 0x%02X\n", ret);
        }
    }
    return(ret);
}

/**
 * @brief       : receive from Sensor with I2C communication
 * @param count : number of data bytes to expect
 * @param chk_zero :  check for zero termination (Serial and product code)
 *  false : expect and read all the data bytes
 *  true  : expect NULL termination and count is MAXIMUM data bytes
 * 
 * return :
 * OK   ERR_OK
 * else error
 */
uint8_t SPS30::I2C_ReadToBuffer(uint8_t count, bool chk_zero)
{
    uint8_t data[3];
    uint8_t tmp_buf[MAXBUF];
    uint8_t i, j, x, y;

    // every 2 bytes have a CRC
    x = count / 2 * 3;
    if (x > MAXBUF) x = MAXBUF;
    
    switch(bcm2835_i2c_read((char *) tmp_buf, x))
    {
        case BCM2835_I2C_REASON_ERROR_NACK :
            if(_SPS30_Debug == 2) printf(REDSTR,"DEBUG: Read NACK error\n");
            return(ERR_PROTOCOL);
            break;

        case BCM2835_I2C_REASON_ERROR_CLKT :
            if(_SPS30_Debug == 2) printf(REDSTR,"DEBUG: Read Clock stretch error\n");
            return(ERR_PROTOCOL);
            break;

        case BCM2835_I2C_REASON_ERROR_DATA :
            if(_SPS30_Debug ==2) printf(REDSTR,"DEBUG: not all data has been read\n");
            return(ERR_PROTOCOL);
            break;
    }

    j = i = _Receive_BUF_Length = 0;
    
    /* parse the response */
    for (y = 0; y < x; y++)
    {
        data[i++] = tmp_buf[y];

        // 2 bytes RH, 1 CRC
        if( i == 3) {

            if (data[2] != I2C_calc_CRC(&data[0])){
                
                if (_SPS30_Debug == 2)
                    printf("I2C CRC error: Expected 0x%02X, calculated 0x%02X\n",data[2] & 0xff,I2C_calc_CRC(&data[0]) &0xff);
                
                return(ERR_PROTOCOL);
            }

            _Receive_BUF[_Receive_BUF_Length++] = data[0];
            _Receive_BUF[_Receive_BUF_Length++] = data[1];

            i = 0;

            // check for zero termination (Serial and product code)
            if (chk_zero) {
                if (data[0] == 0 && data[1] == 0) return(ERR_OK);
            }
        }
      }

    if (i != 0) {
        if (_SPS30_Debug == 2) printf("Error: Data counter %d\n",i);
        while (j < i) _Receive_BUF[_Receive_BUF_Length++] = data[j++];
    }

    if (_Receive_BUF_Length == 0) {
        if (_SPS30_Debug == 2)  printf(REDSTR,"Error: Received NO bytes\n");
        return(ERR_PROTOCOL);
    }

    if (_Receive_BUF_Length == count) return(ERR_OK);

    if (_SPS30_Debug == 2)
        printf("Error: Expected bytes : %d, Received bytes %d\n", count,_Receive_BUF_Length);

    return(ERR_DATALENGTH);
}

/**
 * @brief :check for data ready
 *
 * Return
 *  true  if available
 *  false if not
 */
bool SPS30::Check_data_ready()
{
   I2C_fill_buffer(I2C_READ_DATA_RDY_FLAG);

   if (I2C_SetPointer_Read(2) != ERR_OK) return(false);

   if (_Receive_BUF[1] == 1) return(true);
   
   return(false);
}

/**
 * @brief : calculate CRC for I2c comms
 * @param data : 2 databytes to calculate the CRC from
 *
 * Source : datasheet SPS30
 *
 * return CRC
 */
uint8_t SPS30::I2C_calc_CRC(uint8_t data[2])
{
    uint8_t crc = 0xFF;
    for(int i = 0; i < 2; i++) {
        crc ^= data[i];
        for(uint8_t bit = 8; bit > 0; --bit) {
            if(crc & 0x80) {
                crc = (crc << 1) ^ 0x31u;
            } else {
                crc = (crc << 1);
            }
        }
    }

    return crc;
}
