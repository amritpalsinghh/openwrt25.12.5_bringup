/**
 *  @file 	occ_interface.c
 *  @brief 	OCC API Library
 *
 *  This source file contains OCC command APIs definitions
 *
 */

/* standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include "xmfh_err.h"


#include "occ_interface.h"
#include "occ_interface_internal.h"

/* i2c bus */
#define OCC_I2C_BUS 0

#define HARDWARE_REVISION_1_0 "/tmp/hwrev1.0"

/** MRAA i2c init node */
mraa_i2c_context g_p_adc_init, g_p_adg_init, g_p_eeprom_init;

int 			g_i_lastGainUsed[] 	= 	{3, 3, 1, 1, 0};

int 			g_i_lastPdUsed 	= 	0;

float g_f_PDslope[] = {1.0, 1.0, 1.0,1.0,1.0};  // responsivity in mA/mW, to replace this with EEPROM readings



/**
 * @brief This function gets the PD voltage in ADC counts
 *
 * @param v : input ADC counts [0, 0xF0FF], where 0xF0FF = 0xFFF0 with LSB & MSB swapped.
 *  The ADC pads the values of the conversion with 4 zeros at the LSB to allow for interoperability with a 16 bit ADC.
 * @return converted ADC counts [0x0FFF, 0], 0xFFF counts = 0V = Vref
*/
float ConvertV(uint16_t i_ui16_Cur)  // input [0, 0xF0FF] -> output [0x0FFF, 0]
{   /** swap MSB and LSB + keep higher 12 bits as + bit inversion */
    i_ui16_Cur=(i_ui16_Cur>>8)+(i_ui16_Cur&0xFF)*256;
    return (4095-(i_ui16_Cur/16));
}

/*
ADC 0 to 4095
ADC 0 to 65520
*/

/*
 * @brief This function gets the Max Power value for passed pd
 *
 * @param i_i_pdNum          Pd Number
 * @param o_f_pwrVal     	 returns Max Power value for passed pd number
 * @return On success, returns EXIT_SUCCESS, error code otherwise
*/
int OCCReadPWR(int i_i_pdNum, float *o_f_pwrVal)
{   unsigned char 	l_uc_pDSW[] = { 1, 2, 4, 8, 0};
    unsigned char 	l_uc_rfbSW[] = {0x00, 0x80, 0x40, 0x20, 0x10};
    int l_i_status = OCC_SUCCESS;
    int l_i_currGain = 0;
    int l_i_done = 0 ;
    int l_i_direction = 0;
    int l_i_cnt = 0;
    float l_f_current = 0;
    float l_f_calVal = 0;
    float l_f_Rfb[5] = {0};
    float l_f_Vref = 2.048;
    uint8_t l_ui8_tx_buf[2] = {0};

    /**Check the Hw Rev file*/
    if( access(HARDWARE_REVISION_1_0, F_OK ) == 0 )
    {   //Changed on 20210127, TIA gain in Ohm, if Beta is 1.0
        l_f_Rfb[0] = 10.001e6;
        l_f_Rfb[1] = 1.001e6;
        l_f_Rfb[2] = 1.01e5;
        l_f_Rfb[3] = 11000.;
        l_f_Rfb[4] = 2000;
    }
    else
    {   //Changed on 20210127, TIA gain in Ohm, if Beta is 1.1
        l_f_Rfb[0] = 10e6;
        l_f_Rfb[1] = 1e6;
        l_f_Rfb[2] = 1e5;
        l_f_Rfb[3] = 10000.;
        l_f_Rfb[4] = 1000;
    }

    do
    {   if(o_f_pwrVal == NULL)
        {   l_i_status = OCC_NULL_POINTER;
            break;
        }

        if((i_i_pdNum > PD_NONE) || (i_i_pdNum < PD_1577))
        {   l_i_status = OCC_PD_OUTOF_RANGE_ERROR;
            break;
        }

        if(NULL == g_p_adc_init)
        {   l_i_status = ADC_INIT_ERROR;
            break;
        }

        if(NULL == g_p_adg_init)
        {   l_i_status = ADG_INIT_ERROR;
            break;
        }

        if(NULL == g_p_eeprom_init)
        {   l_i_status = EEPROM_INIT_ERROR;
            break;
        }

        /**start with the last gain that was used */
        l_i_currGain = g_i_lastGainUsed[i_i_pdNum];

        /** blead off capacitor */
        /** Only do this if the pd is different from the last call */
        if (g_i_lastPdUsed != i_i_pdNum)
        {   if(OCC_DEBUG)
            {   printf("DEBUG: PD Number From Last Call: <%d> Curr pd : <%d>\n", g_i_lastPdUsed, i_i_pdNum);
            }
            l_ui8_tx_buf[0] = ADG_ADDRESS;
            l_ui8_tx_buf[1] = l_uc_rfbSW[4];
            l_i_status = mraa_i2c_write(g_p_adg_init,l_ui8_tx_buf,2);
            if(l_i_status != OCC_SUCCESS && errno == 6)
            {   l_i_status = OCC_NACK_ERROR;
                break;
            }
            else if(l_i_status != OCC_SUCCESS)
            {   l_i_status =  OCC_WRITE_ERROR;
                break;
            }
            usleep(100000);
        }
        while(l_i_done == 0 && l_i_cnt < 6)
        {   l_i_cnt += 1; 					//brute force prevention of infinite loop (this should be redundant)

            /** measure dark current by turning off all the switches */
            unsigned char l_uc_readGain = l_uc_rfbSW[l_i_currGain];
            l_ui8_tx_buf[0] = ADG_ADDRESS;
            l_ui8_tx_buf[1] =  l_uc_rfbSW[l_i_currGain];
            l_i_status = mraa_i2c_write(g_p_adg_init,l_ui8_tx_buf,2);
            if(l_i_status != OCC_SUCCESS && errno == 6)
            {   l_i_status = OCC_NACK_ERROR;
                break;
            }
            else if(l_i_status != OCC_SUCCESS)
            {   l_i_status =  OCC_WRITE_ERROR;
                break;
            }
            usleep(100000);

            uint16_t l_ui16_darkCur = 0x0000;

            l_i_status = mraa_i2c_read_bytes_data(g_p_adc_init, 0x20, (uint8_t*) &l_ui16_darkCur, 2); //because we this is a 12 bit ADC and the largesst value is 0xFFF0
            if(l_i_status == -1 && errno == 6)
            {   l_i_status = OCC_NACK_ERROR;
                break;
            }
            else if(l_i_status == -1)
            {   l_i_status =  OCC_READ_ERROR;
                break;
            }
            l_i_status = OCC_SUCCESS;

            /** measure photocurrent */

            l_uc_readGain = l_uc_pDSW[i_i_pdNum]+ l_uc_rfbSW[l_i_currGain];
            l_ui8_tx_buf[0] = ADG_ADDRESS;
            l_ui8_tx_buf[1] = l_uc_readGain;
            l_i_status = mraa_i2c_write(g_p_adg_init,l_ui8_tx_buf,2);
            if(l_i_status != OCC_SUCCESS && errno == 6)
            {   l_i_status = OCC_NACK_ERROR;
                break;
            }
            else if(l_i_status != OCC_SUCCESS)
            {   l_i_status =  OCC_WRITE_ERROR;
                break;
            }
            usleep(100000);
            uint16_t l_ui16_photoCur = 0x0000;
            l_i_status = mraa_i2c_read_bytes_data(g_p_adc_init, 0x20, (uint8_t*) &l_ui16_photoCur, 2); //because we this is a 12 bit ADC and the largesst value is 0xFFF0
            if(l_i_status == -1 && errno == 6)
            {   l_i_status = OCC_NACK_ERROR;
                break;
            }
            else if(l_i_status == -1)
            {   l_i_status =  OCC_READ_ERROR;
                break;
            }
            l_i_status = OCC_SUCCESS;

            /** subtract dark current from photocurrent */
            l_f_current = ConvertV(l_ui16_photoCur) - ConvertV(l_ui16_darkCur); // PD voltage in ADC counts with range [4095, 0]


            /** ADC counts to amp based on current TZA gain */
            l_f_calVal = (l_f_current*l_f_Vref/l_f_Rfb[l_i_currGain]/4096); // counts * 2.048V/4096 counts / TZA Ohm = A;
            if(OCC_DEBUG)
            {   printf("DEBUG : PD%d , l_f_current : %f,Gain:%d : <%x>     <%e>mA\n", i_i_pdNum,
                       l_f_current,l_i_currGain,l_uc_readGain,(l_f_calVal*1e5));
            }

            if ((l_f_current<380) && (l_i_currGain > 0) && l_i_direction !=-1 )
            {   /** if the reading is too low then increase the gain but only if it was not previously decreased */
                l_i_currGain= l_i_currGain - 1;
                l_i_done = 0;
                l_i_direction =1;
            }
            else if ((l_f_current>4000) && (l_i_currGain < 4) && l_i_direction !=1)
            {   /** if the reading is too high then decrease the gain but only if it was not previously increased */
                l_i_currGain= l_i_currGain + 1;
                l_i_done = 0;
                l_i_direction = -1;
            }
            else
            {   /** gain was good. We are done */
                l_i_done = 1;
            }
        }


        /** Check the status */
        if(l_i_status != OCC_SUCCESS)
        {   break;
        }

        /** save the optimum gain as the initial guess for the next API call for this pd */
        g_i_lastGainUsed[i_i_pdNum] = l_i_currGain ;
        g_i_lastPdUsed = i_i_pdNum;
        /** convert to photocurrent */
        l_f_calVal = (l_f_current*l_f_Vref/l_f_Rfb[l_i_currGain]/4095); // counts * 2.048V/4095 counts / TZA Ohm = A; V_ref = 2.048V = 4095 counts

        *o_f_pwrVal = ((l_f_calVal) * (1e+5)); // A * 1000 mA/A * 100
        if(FP_NORMAL == fpclassify(g_f_PDslope[i_i_pdNum]))
        {   /** devided by the responsivity. Result is in mW. */
            *o_f_pwrVal /= g_f_PDslope[i_i_pdNum]; // responsivity_mA/mW = mW
        }
        else
        {   l_i_status = OCC_CAL_FACTOR_ERROR;
        }
    }
    while(0);

    return l_i_status;
}

/**
 * @brief This function gets the Max Current value for passed pd. API is for internal use only.
 *
 * @param i_i_pdNum      Pd Number
 * @param o_px_pwr     	 returns structure containing Max Power value and current for passed pd number
 * @return On success, returns EXIT_SUCCESS, error code otherwise
*/
int OCCReadCurrPWR(int i_i_pdNum, T_POWER *o_px_pwr)
{   unsigned char l_uc_pDSW[] = { 1, 2, 4, 8, 0};
    unsigned char l_uc_rfbSW[] = {0x00, 0x80, 0x40, 0x20, 0x10};
    unsigned char l_uc_readGain = 0;
    int l_i_status = OCC_SUCCESS;
    int l_i_currGain = 0;
    int l_i_done = 0 ;
    int l_i_direction = 0;
    int l_i_cnt = 0;
    float l_f_current = 0;
    float l_f_calVal = 0;
    float l_f_Rfb[5] = {0};
    float l_f_Vref = 2.048;
    uint8_t l_ui8_tx_buf[2] = {0};

    /**Check the Hw Rev file*/
    if( access(HARDWARE_REVISION_1_0, F_OK ) == 0 )
    {   //Changed on 20210127, TIA gain in Ohm, if Beta is 1.0
        l_f_Rfb[0] = 10.001e6;
        l_f_Rfb[1] = 1.001e6;
        l_f_Rfb[2] = 1.01e5;
        l_f_Rfb[3] = 11000.;
        l_f_Rfb[4] = 2000;
    }
    else
    {   //Changed on 20210127, TIA gain in Ohm, if Beta is 1.1
        l_f_Rfb[0] = 10e6;
        l_f_Rfb[1] = 1e6;
        l_f_Rfb[2] = 1e5;
        l_f_Rfb[3] = 10000.;
        l_f_Rfb[4] = 1000;
    }

    do
    {   if(o_px_pwr == NULL)
        {   l_i_status = OCC_NULL_POINTER;
            break;
        }

        if((i_i_pdNum > PD_NONE) || (i_i_pdNum < PD_1577))
        {   l_i_status = OCC_PD_OUTOF_RANGE_ERROR;
            break;
        }

        if(NULL == g_p_adc_init)
        {   l_i_status = ADC_INIT_ERROR;
            break;
        }

        if(NULL == g_p_adg_init)
        {   l_i_status = ADG_INIT_ERROR;
            break;
        }

        if(NULL == g_p_eeprom_init)
        {   l_i_status = EEPROM_INIT_ERROR;
            break;
        }

        /**start with the last gain that was used */
        l_i_currGain = g_i_lastGainUsed[i_i_pdNum];

        /** blead off capacitor */
        /** Only do this if the pd is different from the last call */
        if (g_i_lastPdUsed != i_i_pdNum)
        {   if(OCC_DEBUG)
            {   printf("DEBUG: PD Number From Last Call: <%d> Curr pd : <%d>\n", g_i_lastPdUsed, i_i_pdNum);
            }
            l_ui8_tx_buf[0] = ADG_ADDRESS;
            l_ui8_tx_buf[1] = l_uc_rfbSW[4];
            l_i_status = mraa_i2c_write(g_p_adg_init,l_ui8_tx_buf,2);
            if(l_i_status != OCC_SUCCESS && errno == 6)
            {   l_i_status = OCC_NACK_ERROR;
                break;
            }
            else if(l_i_status != OCC_SUCCESS)
            {   l_i_status =  OCC_WRITE_ERROR;
                break;
            }
            usleep(100000);
        }
        while(l_i_done == 0 && l_i_cnt < 6)
        {   l_i_cnt += 1; 					//brute force prevention of infinite loop (this should be redundant)

            /** measure dark current by turning off all the switches */
            l_uc_readGain = l_uc_rfbSW[l_i_currGain];
            l_ui8_tx_buf[0] = ADG_ADDRESS;
            l_ui8_tx_buf[1] = l_uc_readGain;
            l_i_status = mraa_i2c_write(g_p_adg_init,l_ui8_tx_buf,2);
            if(l_i_status != OCC_SUCCESS && errno == 6)
            {   l_i_status = OCC_NACK_ERROR;
                break;
            }
            else if(l_i_status != OCC_SUCCESS)
            {   l_i_status =  OCC_WRITE_ERROR;
                break;
            }
            usleep(100000);

            uint16_t l_ui16_darkCur = 0x0000;
            l_i_status = mraa_i2c_read_bytes_data(g_p_adc_init, 0x20, (uint8_t*) &l_ui16_darkCur, 2); //because we this is a 12 bit ADC and the largesst value is 0xFFF0
            if(l_i_status == -1 && errno == 6)
            {   l_i_status = OCC_NACK_ERROR;
                break;
            }
            else if(l_i_status == -1)
            {   l_i_status =  OCC_READ_ERROR;
                break;
            }
            l_i_status = OCC_SUCCESS;

            /** measure photocurrent */
            l_uc_readGain = l_uc_pDSW[i_i_pdNum]+ l_uc_rfbSW[l_i_currGain];
            l_ui8_tx_buf[0] = ADG_ADDRESS;
            l_ui8_tx_buf[1] = l_uc_readGain;
            l_i_status = mraa_i2c_write(g_p_adg_init,l_ui8_tx_buf,2);
            if(l_i_status != OCC_SUCCESS && errno == 6)
            {   l_i_status = OCC_NACK_ERROR;
                break;
            }
            else if(l_i_status != OCC_SUCCESS)
            {   l_i_status =  OCC_WRITE_ERROR;
                break;
            }
            usleep(100000);

            uint16_t l_ui16_photoCur = 0x0000;
            l_i_status = mraa_i2c_read_bytes_data(g_p_adc_init, 0x20, (uint8_t*) &l_ui16_photoCur, 2);
            if(l_i_status == -1 && errno == 6)
            {   l_i_status = OCC_NACK_ERROR;
                break;
            }
            else if(l_i_status == -1)
            {   l_i_status =  OCC_READ_ERROR;
                break;
            }
            l_i_status = OCC_SUCCESS;

            /** subtract dark current from photocurrent */
            l_f_current = ConvertV(l_ui16_photoCur) - ConvertV(l_ui16_darkCur); // PD voltage in ADC counts with range [4095, 0]
            o_px_pwr->f_current = l_f_current;

            /** ADC counts to amp based on current TZA gain */
            l_f_calVal = (l_f_current*l_f_Vref/l_f_Rfb[l_i_currGain]/4096); // counts * 2.048V/4096 counts / TZA Ohm = A;
            if(OCC_DEBUG)
            {   printf("DEBUG : PD%d , l_f_current : %f,Gain:%d : <%x>     <%e>mA\n", i_i_pdNum,
                       l_f_current,l_i_currGain,l_uc_readGain,(l_f_calVal*1e5));
            }

            if ((l_f_current<380) && (l_i_currGain > 0) && l_i_direction !=-1 )
            {   /** if the reading is too low then increase the gain but only if it was not previously decreased */
                l_i_currGain= l_i_currGain - 1;
                l_i_done = 0;
                l_i_direction =1;
            }
            else if ((l_f_current>4000) && (l_i_currGain < 4) && l_i_direction !=1)
            {   /** if the reading is too high then decrease the gain but only if it was not previously increased */
                l_i_currGain= l_i_currGain + 1;
                l_i_done = 0;
                l_i_direction = -1;
            }
            else
            {   /** gain was good. We are done */
                l_i_done = 1;
            }
        }

        /** Check the status */
        if(l_i_status != OCC_SUCCESS)
        {   break;
        }

        /** save the optimum gain as the initial guess for the next API call for this pd */
        g_i_lastGainUsed[i_i_pdNum] = l_i_currGain ;
        g_i_lastPdUsed = i_i_pdNum;
        /** convert to photocurrent */
        l_f_calVal = (l_f_current*l_f_Vref/l_f_Rfb[l_i_currGain]/4095); // counts * 2.048V/4095 counts / TZA Ohm = A; V_ref = 2.048V = 4095 counts

        o_px_pwr->f_pwrVal = ((l_f_calVal) * (1e+5)); // A * 1000 mA/A * 100
        if(FP_NORMAL == fpclassify(g_f_PDslope[i_i_pdNum]))
        {   /** devided by the responsivity. Result is in mW. */
            o_px_pwr->f_pwrVal /= g_f_PDslope[i_i_pdNum]; // responsivity_mA/mW = mW
        }
        else
        {   l_i_status = OCC_CAL_FACTOR_ERROR;
        }
    }
    while(0);

    return l_i_status;
}

/**
 *
 * @brief This function read a single page from designated register
 *
 * @param i_ui8_deviceAddress input device address
 * @param i_ui8_registerAddress input register address
 * @param i_ui16_n input length of bytes (max limit is 63)
 * @param o_ui8_data returns bytes of given length
 *
 * @return On success, returns OCC_SUCCESS, error code otherwise
 *
 */
int OCCPageRead(uint8_t i_ui8_registerAddress, uint16_t i_ui16_n, uint8_t *o_ui8_data)
{   int l_i_status = OCC_SUCCESS;
    int l_i_read_bytes_len = 0;

    do
    {   if(NULL == g_p_adc_init)
        {   l_i_status = ADC_INIT_ERROR;
            break;
        }

        if(NULL == g_p_adg_init)
        {   l_i_status = ADG_INIT_ERROR;
            break;
        }

        if(NULL == g_p_eeprom_init)
        {   l_i_status = EEPROM_INIT_ERROR;
            break;
        }

        l_i_read_bytes_len = mraa_i2c_read_bytes_data(g_p_eeprom_init, i_ui8_registerAddress, o_ui8_data, i_ui16_n);
        if(l_i_read_bytes_len == -1 && errno == 6)
        {   l_i_status = OCC_NACK_ERROR;
        }
        else if(l_i_read_bytes_len == -1)
        {   l_i_status =  OCC_READ_ERROR;
        }
    }
    while(0);

    return l_i_status;
}


/**
 * @brief This function Reads OCC Calibration data
 * @return On success, returns OCC_SUCCESS, error code otherwise
 */
int OCCGetCalibration(void)
{   int l_i_status = OCC_SUCCESS;
    uint8_t page_write_read_pd1[4] = {0};
    uint8_t page_write_read_pd2[4] = {0};
    uint8_t page_write_read_pd3[4] = {0};
    uint8_t page_write_read_pd4[4] = {0};
    uint8_t l_ui8_sw_byte = 0;
    uint8_t l_ui8_sw_byte_curnt = 0;
    uint8_t l_ui8_tx_buf[2] = {0};

    do
    {   /* writing 0x00 turns all the switches off. 0xFF would turn them all on	*/
        l_i_status = mraa_i2c_read(g_p_adg_init,&l_ui8_sw_byte,1);
        if(l_i_status == 0 && errno == 6)
        {   l_i_status = OCC_NACK_ERROR;
            break;
        }
        else if(l_i_status == 0)
        {   l_i_status =  OCC_READ_ERROR;
            break;
        }

        if (l_ui8_sw_byte == 0)
            l_ui8_sw_byte = 1;

        l_ui8_tx_buf[0] = ADG_ADDRESS;
        l_ui8_tx_buf[1] = l_ui8_sw_byte;

        l_i_status = mraa_i2c_write(g_p_adg_init,l_ui8_tx_buf,2);
        if(l_i_status != OCC_SUCCESS && errno == 6)
        {   l_i_status = OCC_NACK_ERROR;
            break;
        }
        else if(l_i_status != OCC_SUCCESS)
        {   l_i_status =  OCC_WRITE_ERROR;
            break;
        }
        usleep(10000);

        l_i_status = mraa_i2c_read(g_p_adg_init,&l_ui8_sw_byte_curnt,1);
        {   if(l_i_status == 0 && errno == 6)
            {   l_i_status = OCC_NACK_ERROR;
            }
            else if(l_i_status == 0 || l_ui8_sw_byte != l_ui8_sw_byte_curnt)
            {   l_i_status =  OCC_READ_ERROR;
            }

            if(OCC_DEBUG)
                printf("Error communicating with the gain and PD switches\n");
        }

        /* Reading Calibration Data from EPROM 0x56 */
        l_i_status = OCCPageRead(0x84, 4, page_write_read_pd1);
        if (l_i_status != OCC_SUCCESS)
        {   l_i_status = OCC_READ_ERROR;
            break;
        }

        g_f_PDslope[0] = *(float*)(page_write_read_pd1);

        usleep(10000);

        l_i_status = OCCPageRead(0x88, 4, page_write_read_pd2);
        if (l_i_status != OCC_SUCCESS)
        {   l_i_status = OCC_READ_ERROR;
            break;
        }

        g_f_PDslope[1] = *(float*)(page_write_read_pd2);

        usleep(10000);

        l_i_status = OCCPageRead(0x8C, 4, page_write_read_pd3);
        if (l_i_status != OCC_SUCCESS)
        {   l_i_status = OCC_READ_ERROR;
            break;
        }

        g_f_PDslope[2] = *(float*)(page_write_read_pd3);

        usleep(10000);

        l_i_status = OCCPageRead(0x90, 4, page_write_read_pd4);
        if (l_i_status != OCC_SUCCESS)
        {   l_i_status = OCC_READ_ERROR;
            break;
        }

        g_f_PDslope[3] = *(float*)(page_write_read_pd4);

        usleep(10000);
        if(OCC_DEBUG)
        {   printf("Responsivities:[%f, %f, %f, %f, %f]\n", g_f_PDslope[0], g_f_PDslope[1], g_f_PDslope[2], g_f_PDslope[3], g_f_PDslope[4]);
        }
    }
    while(0);

    return l_i_status;
}

/**
 * @brief This function Initialize  OCC
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int OCCInterfaceInit(void)
{   int status = OCC_SUCCESS;
    uint8_t l_ui8_tx_buf[2] = {0};
    do
    {   /** ADC Init*/
        g_p_adc_init = mraa_i2c_init(OCC_I2C_BUS);
        if (g_p_adc_init == NULL)
        {   status = ADC_INIT_ERROR;
            break;
        }

        /** Set the slave Address*/
        status = mraa_i2c_address(g_p_adc_init, ADC_ADDRESS);
        if (status != OCC_SUCCESS)
        {   status = ADC_SET_ADDR_ERROR;
            break;
        }

        /*Run the device at 100kHz */
        status = mraa_i2c_frequency(g_p_adc_init, MRAA_I2C_STD);
        if (status != OCC_SUCCESS)
        {   status = OCC_FREQUENCY_ERROR;
            break;
        }

        /** ADG init*/
        g_p_adg_init = mraa_i2c_init(OCC_I2C_BUS);
        if (g_p_adg_init == NULL)
        {   status = ADG_INIT_ERROR;
            break;
        }

        /** Set the slave Address*/
        status = mraa_i2c_address(g_p_adg_init, ADG_ADDRESS);
        if (status != OCC_SUCCESS)
        {   status = ADG_SET_ADDR_ERROR;
            break;
        }

        /*Run the device at 100kHz */
        status = mraa_i2c_frequency(g_p_adg_init, MRAA_I2C_STD);
        if (status != OCC_SUCCESS)
        {   status = OCC_FREQUENCY_ERROR;
            break;
        }

        /** EEPROM init*/
        g_p_eeprom_init = mraa_i2c_init(OCC_I2C_BUS);
        if (g_p_eeprom_init == NULL)
        {   status = EEPROM_INIT_ERROR;
            break;
        }

        /** Set the EEPROM Address*/
        status = mraa_i2c_address(g_p_eeprom_init, EEPROM_ADDRESS);
        if (status != OCC_SUCCESS)
        {   status = EEPROM_SET_ADDR_ERROR;
            break;
        }

        /*Run the device at 100kHz */
        status = mraa_i2c_frequency(g_p_eeprom_init, MRAA_I2C_STD);
        if (status != OCC_SUCCESS)
        {   status = OCC_FREQUENCY_ERROR;
            break;
        }

        status = OCCGetCalibration();
        if (status != OCC_SUCCESS)
        {   status = OCC_CAL_ERROR;
            break;
        }
    }
    while(0);

    /** TO check Both  Initialization */
    if(status != OCC_SUCCESS)
    {   if(g_p_adc_init)
        {   mraa_i2c_stop(g_p_adc_init);
            g_p_adc_init = NULL;
        }

        if(g_p_adg_init)
        {   mraa_i2c_stop(g_p_adg_init);
            g_p_adg_init = NULL;
        }

        if(g_p_eeprom_init)
        {   mraa_i2c_stop(g_p_eeprom_init);
            g_p_eeprom_init = NULL;
        }
    }
    else
    {   /** Setting External Reference Voltage */
        l_ui8_tx_buf[0] = ADC_ADV_CONFIG;
        l_ui8_tx_buf[1] = 0x01;
        mraa_i2c_write(g_p_adc_init,l_ui8_tx_buf,2);

        /** Setting Continuous Conversion Mode */
        l_ui8_tx_buf[0] = ADC_CONV_RATE;
        l_ui8_tx_buf[1] = 0x01;
        mraa_i2c_write(g_p_adc_init,l_ui8_tx_buf,2);

        /** Setting Start bit of the Config Reg */
        l_ui8_tx_buf[0] = ADC_CONFIG;
        l_ui8_tx_buf[1] = 0x01;
        mraa_i2c_write(g_p_adc_init,l_ui8_tx_buf,2);
    }

    return status;
}

/**
 * @brief This function DeInitialize  OCC
 * @return no returns
 */
void OCCInterfaceDeInit()
{   if(g_p_adc_init)
    {   mraa_i2c_stop(g_p_adc_init);
        g_p_adc_init = NULL;
    }

    if(g_p_adg_init)
    {   mraa_i2c_stop(g_p_adg_init);
        g_p_adg_init = NULL;
    }

    if(g_p_eeprom_init)
    {   mraa_i2c_stop(g_p_eeprom_init);
        g_p_eeprom_init = NULL;
    }

    return;
}

