/**
 * Copyright (c) 2019 Paulvha.  version 1.0
 *
 * Header file for monitor to get information from an SDS-011
 *
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
 */
 
#include "sds011_lib.h"
 
class SDSmon : public SDS
{
  public:
    
    SDSmon(void);

    /** 
     * @brief open connection to SDS
     * @param device: the device to use to connect to SDS
     * @param verbose : if > 0 progress messsages are displayed
     * 
     * @return  
     * 0 success
     * -1 error
     */
    int open_sds(char * device, int verbose);
    
    
    /**
     * @brief close the SDS connection correctly 
     */
    void close_sds();
    
    /**
     * @brief read from SDS
     * @param pm25 : return value PM2.5
     * @param pm10 : return value PM10
     * 
     * @return  
     * 0 success
     * -1 error
     */
    int read_sds(float *pm25, float *pm10);
   
   private:
};
