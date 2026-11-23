/**************************************************************************************************
 *  @file   osa_calibration.c
 *  @brief  OSA (CTM) Self calibration API
 *
 *  This file contains OSA (CTM) Self calibration API implementation.
 *
**************************************************************************************************/
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "common_i2c_operation.h"
#include "otdr_interface.h"
#include "ocm_interface.h"
#include "sfp_interface.h"
#include "osa_calibration.h"
/* mraa header */
#include "mraa/i2c.h"


#define DAC_GAIN       0x04
#define DAC_DAC_DATA   0x08

// Legit addresses for ADC128D818
#define DAC_ADDRESS    0x48

/** For logger */
#define OSACAL_LOGGER_FILE_PATH   "/var/log/OSACAL.log"
#define OSACAL_REPORT_FILE_PATH   "/root/CalReport.csv"

void setLogOutput(int output);
void setFileLogInfo(FILE **fp, const char *path, const char *mode);
void logMgrClose(FILE **fp);
int CTMFindNotchPosition(float i_f_freq[], float* i_f_slicePwr, int i_i_smoothData, int i_i_method, float* i_f_Data);

mraa_i2c_context sld_init = NULL;
FILE * g_px_OsaCallgrFileptr = NULL;


float byte2Float(uint8_t*bytes)
{   float num = 0;
    uint n = 0;
    memcpy(&num, bytes, sizeof(float));
    return num;
}

void float2Byte(float f, uint8_t*bytes)
{   memcpy(bytes, &f, sizeof(float));
}

float Freq2Bin(float Freq, float StartFreq)
{   return (StartFreq+0.075-Freq)/0.00625;
}
/* set the OSW to calibration mode */
int OSWcal()
{   int l_i_status = 0;
    do
    {   /* set the OSW to calibration mode */
        if(l_i_status = OPSWGPIOWrite(37, 0))
        {
#ifdef DEBUG
            printf("Write 0 to GPIO 37 failed. Returned:%d\n", l_i_status);
#endif
            break;
        }
        if(l_i_status = OPSWGPIOWrite(3, 1))
        {
#ifdef DEBUG
            printf("Write 1 to GPIO 3 failed. Returned:%d\n", l_i_status);
#endif
            break;
        }
    }
    while(0);
    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function set the OSA to measaure mode.
 *
 * @return  returns 0 in case success, error code otherwise.
 *
 **************************************************************************************************/
int OSWmeasure()
{   int l_i_status = 0;
    do
    {   /* set the OSW to calibration mode */
        if(l_i_status = OPSWGPIOWrite(37, 1))
        {
#ifdef DEBUG
            printf("Write 1 to GPIO 37 failed. Returned:%d\n", l_i_status);
#endif
            break;
        }
        if(l_i_status = OPSWGPIOWrite(3, 0))
        {
#ifdef DEBUG
            printf("Write 0 to GPIO 3 failed. Returned:%d\n", l_i_status);
#endif
            break;
        }
    }
    while(0);
    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function reads a uint16 register from the DAC.
 *
 * @param   i_i_value input bits
 *
 * @return  returns 0 in case success, error code otherwise.
 *
 **************************************************************************************************/
int SLDGetData(uint16_t *o_ui16_data)
{   int l_i_status = 0;

    do
    {   /* DAC Init*/
        sld_init = mraa_i2c_init(0);

        if (NULL == sld_init)
        {   l_i_status = DAC_INIT_ERROR;
            break;
        }
        if (mraa_i2c_address(sld_init, DAC_ADDRESS))
        {   l_i_status = DAC_SET_ADDR_ERROR;
            break;
        }
        if(mraa_i2c_frequency(sld_init, MRAA_I2C_STD))
        {   l_i_status = DAC_FREQUENCY_ERROR;
            break;
        }
        if (mraa_i2c_read_bytes_data(sld_init, DAC_DAC_DATA, o_ui16_data, 2) == -1)
        {   l_i_status = DAC_READ_ERROR;
            break;
        }
        *o_ui16_data = *o_ui16_data << 4;
    }
    while(0);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function sets the current to be used in the on state. Will become active next
 *          time the SLD is turned on.
 *
 * @param   i_i_value input current state
 *
 * @return  returns 0 in case success, error code otherwise.
 *
 **************************************************************************************************/
int SetCurrent(uint16_t i_i_value)
{   int l_i_status = 0;
    uint8_t l_ui8_msb = 0;
    uint8_t l_ui8_lsb = 0;

    do
    {   l_ui8_msb = i_i_value >> 4;
        l_ui8_lsb = i_i_value & 0xF;

        uint8_t l_ui8_tx_buf[3] = {0};
        l_ui8_tx_buf[0] = DAC_GAIN;
        l_ui8_tx_buf[1] = 0x01;
        l_ui8_tx_buf[2] = 0x01;

        /* Write the register */
        l_i_status = mraa_i2c_write(sld_init, l_ui8_tx_buf, 3);
        if (l_i_status != 0)
            l_i_status = DAC_WRITE_ERROR;

        l_ui8_tx_buf[0] = 0x08;
        l_ui8_tx_buf[1] = l_ui8_msb;
        l_ui8_tx_buf[2] = l_ui8_lsb;

        /* Write the register */
        l_i_status = mraa_i2c_write(sld_init, l_ui8_tx_buf, 3);
        if (l_i_status != 0)
            l_i_status = DAC_WRITE_ERROR;
    }
    while(0);

    return l_i_status;
}

#if 0
int ReadSLDDataFromCTM()
{   int l_i_status = 0;
    int l_i_N = 10;
    int l_i_clmHeader[10] = {0};

    do
    {   for (int l_i_cnt = 0; l_i_cnt<10; l_i_cnt++)
        {   usleep(600000); /* wait for CTM to complete another scan */
            printf("Scan: %d" , l_i_cnt + 1);
            l_i_clmHeader[l_i_cnt] = l_i_cnt + 1;
        }
    }
    while(0);

    return l_i_status;
}
#endif

/**************************************************************************************************
 *
 * @brief   This function check whether a number is in a range of [min, max]
 *
 * @param   i_f_num input number as a float
 * @param   l_f_min_val input minimun range of number to be checked
 * @param   l_f_max_val input maximum range of number to be checked
 *
 * @return  returns 0 in case number is in a range, 1 otherwise.
 *
 **************************************************************************************************/
int ValidateRange(float i_f_num, float l_f_min_val, float l_f_max_val)
{   if ((i_f_num < l_f_min_val) || (i_f_num > l_f_max_val))
    {   return 1;
    }
    return 0;
}

/**************************************************************************************************
 *
 * @brief   This function check and/or perform osa calibration
 *
 * @param   i_b_commit calibration option to commit calibrated data
 *
 * @param   N Number of SLD Scans
 *
 * @return  returns 0 on success Error Code otherwise.
 *
 **************************************************************************************************/
int osaCalibration(bool i_b_commit, float* new_sboc, float* new_sbtc)
{   int l_i_status = 0;
    uint16_t l_ui16_sldADC = 0;
    T_CDGT1 cdgt1[10] = {0};
    float sum[785] = {0};
    int N = 10;

    int PerformCal = 1;
    int PerformCalCheck = 2;

    int CalNeeded = 10;
    int CalSucessful = 0;
    int CalNotNeeded = 11;
    uint8_t l_ui8_sbocMfgBaseLine[4] = {0xff,0xff,0xff,0xff};
    uint8_t l_ui8_sbtcMfgBaseLine[4] = {0xff,0xff,0xff,0xff};
    uint8_t l_ui8_infinite[4] = {0xff,0xff,0xff,0xff};

    float l_f_sbocMfgBaseLine = 0;
    float l_f_sbtcMfgBaseLine = 0;
    float l_f_infinite = byte2Float(l_ui8_infinite);

    FILE *osaCalReport = fopen(OSACAL_REPORT_FILE_PATH,"w");

    do
    {   /** Initialize Logger */
        setLogOutput(2);
        if(!g_px_OsaCallgrFileptr)
            setFileLogInfo(&g_px_OsaCallgrFileptr, OSACAL_LOGGER_FILE_PATH, NULL);
        setLogLevel(3);

        T_EEPROMIFO eeprominfo = {0};
        getEEPROMInfo(&eeprominfo);
        fprintf(osaCalReport,"\nPartNumber, %s\n",eeprominfo.m_uc_partNumber);
        fprintf(osaCalReport,"SerialNumber, %s\n",eeprominfo.m_uc_serialnumber);
        fprintf(osaCalReport,"Revision, %s\n",eeprominfo.m_uc_hardwareRivision);

        char revision[1024] = {0};
        if(!OcmGetDREV(revision))
            fprintf(osaCalReport,"\nDREV, %s\n",revision);

        T_CSTA0 l_x_csta0 = {0};
        if(!OcmGetCSTA0(&l_x_csta0))
            fprintf(osaCalReport,"CSTA0,  %f\n", l_x_csta0.m_f_deviceTemperature);

        T_DIAG diag = {0};
        char * channel_des[8] = {"V(TIA-OUT), ", "V(ref), ", "I(Supply), ", "Vcc, ", "V(EPS3), ", "V(Batt), ", "V(OSW), "};
        if(!GetDiagnosticsData(&diag))
        {   fprintf(osaCalReport,"\nTemp, %f, C\n", diag.temp);
            for (int loop = 0; loop < 7; loop++)
            {   fprintf(osaCalReport,"%d. %s%f\n", loop, channel_des[loop], diag.channel[loop]);
            }
        }

        switch(getOTDRStatus())
        {   case POWERMGT_DEVICE_ON:
                fprintf(osaCalReport,"PWR(OTDR), True\n");
                break;
            case POWERMGT_DEVICE_OFF:
                fprintf(osaCalReport,"PWR(OTDR), False\n");
                break;
            case POWERMGT_DEVICE_POWERING_UP:
                fprintf(osaCalReport,"PWR(OTDR), POWERRING UP\n");
                break;
            default :
                fprintf(osaCalReport,"PWR(OTDR), ERROR\n");
        }


        switch(getOCMStatus())
        {   case POWERMGT_DEVICE_ON:
                fprintf(osaCalReport,"PWR(OCM), True\n");
                break;
            case POWERMGT_DEVICE_OFF:
                fprintf(osaCalReport,"PWR(OCM), False\n");
                break;
            case POWERMGT_DEVICE_POWERING_UP:
                fprintf(osaCalReport,"PWR(OCM), POWERRING UP\n");
                break;
            default :
                fprintf(osaCalReport,"PWR(OCM), ERROR\n");
        }

        switch(getSFPStatus())
        {   case POWERMGT_DEVICE_ON:
                fprintf(osaCalReport,"PWR(SFP), True\n");
                break;
            case POWERMGT_DEVICE_OFF:
                fprintf(osaCalReport,"PWR(SFP), False\n");
                break;
            case POWERMGT_DEVICE_POWERING_UP:
                fprintf(osaCalReport,"PWR(SFP), POWERRING UP\n");
                break;
            default :
                fprintf(osaCalReport,"PWR(SFP), ERROR\n");
        }

        /* set the OSW to calibration mode */
        if (l_i_status = OSWcal())
        {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: Calibration Failed: %d",l_i_status);
            break;
        }

        T_CBDI l_x_cbdi = {0};
        l_i_status = OcmGetCBDI(&l_x_cbdi);
        if (!l_i_status)
        {

            l_i_status = EEPROMPageRead(218, 4, l_ui8_sbocMfgBaseLine);
            l_i_status = EEPROMPageRead(214, 4, l_ui8_sbtcMfgBaseLine);
            if(!l_i_status)
            {   l_f_sbocMfgBaseLine = byte2Float(l_ui8_sbocMfgBaseLine);
                l_f_sbtcMfgBaseLine = byte2Float(l_ui8_sbtcMfgBaseLine);
                if(isnan(l_f_sbocMfgBaseLine))
                {   float2Byte(l_x_cbdi.m_f_tofAppliedSBOC,l_ui8_sbocMfgBaseLine);
                    l_i_status = EEPROMPageWrite(218,4,l_ui8_sbocMfgBaseLine);
                    if(l_i_status)
                        break;
                }
                if(isnan(l_f_sbtcMfgBaseLine))
                {   float2Byte(l_x_cbdi.m_f_tofAppliedSBTC,l_ui8_sbtcMfgBaseLine);
                    l_i_status = EEPROMPageWrite(214,4,l_ui8_sbtcMfgBaseLine);
                    if(l_i_status)
                        break;
                }
            }
            else
            {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: OcmGetCBDI Failed: %d",l_i_status);
                break;
            }
        }
        else
            break;

        /* check the SLD status at start of calibration check (wait for SLD to cool if needed) */
        if (l_i_status = SLDGetData(&l_ui16_sldADC))
        {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: SLDGetData Failed: %d",l_i_status);
            break;
        }
        if (l_ui16_sldADC != 0)
        {   /* SLD OFF */
            if (l_i_status = SetCurrent(0x00))
            {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: SLD OFF Failed: %d",l_i_status);
                LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: Warning: SLD was on at start of calibration");
                break;
            }
        }

        // Load CTM mfg cal info from EPROM
        // 	register 188	Bytes 4		Center bin notch filter 1
        // 	register 192	Bytes 4		Width notch filter 1
        // 	register 196	Bytes 4		Center bin notch filter 2
        // 	register 200	Bytes 4		Width notch filter 2
        // 	register 204	Bytes 1		TLS Ch 1
        // 	register 205	Bytes 4		Center bin TL Ch1 (ITU 27)
        // 	register 209	Bytes 1		TLS Ch 2
        // 	register 210	Bytes 4		Center bin TL Ch2 (ITU 55)
        // 	all data above in units of bins (except the ITU channel)
        // 	register 214	Bytes 4		CTM Slope
        // 	register 218	Bytes 4		CTM Offset

        uint8_t l_ui8_CntrBinNotchFltr[4] = {0};
        float NotchFilterCntrMfg[2] = {0};
        float NotchFilterWidthMfg[2] = {0};
        uint8_t TLSItuChMfg[2] = {0};
        float TLSCntrMfg[2] = {0};

        uint8_t l_ui8_NotchFilterCntrMfg[4] = {0};
        uint8_t l_ui8_NotchFilterWidthMfg[4] = {0};
        uint8_t l_ui8_TLSCntrMfg[4] = {0};
        uint8_t l_ui8_TLSItuChMfg[2] = {0};

        l_i_status = EEPROMPageRead(188, 4, l_ui8_CntrBinNotchFltr);
        if(!l_i_status)
        {   float l_f_CntrBinNotchFltr = byte2Float(l_ui8_CntrBinNotchFltr);
            if(isnan(l_f_CntrBinNotchFltr))
            {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: Calibration is not possible");
                l_i_status = CAL_NOT_POSSIBLE;
                break;
            }
        }
        else
            break;

        for(uint8_t i = 0; i<2; i++)
        {   l_i_status = EEPROMPageRead(188+i*8, 4, l_ui8_NotchFilterCntrMfg);
            l_i_status = EEPROMPageRead(192+i*8, 4, l_ui8_NotchFilterWidthMfg);
            l_i_status = EEPROMPageRead(204+i*5, 1, l_ui8_TLSItuChMfg);
            l_i_status = EEPROMPageRead(205+i*5, 4, l_ui8_TLSCntrMfg);
            if(!l_i_status)
            {   NotchFilterCntrMfg[i] = byte2Float(l_ui8_NotchFilterCntrMfg);
                NotchFilterWidthMfg[i] = byte2Float(l_ui8_NotchFilterWidthMfg);
                TLSItuChMfg[1-i] = l_ui8_TLSItuChMfg[0];
                TLSCntrMfg[1-i] = byte2Float(l_ui8_TLSCntrMfg);
            }
            else
                break;
        }
        float TLSCntrBin[2] = {0};
        TLSCntrBin[0] = Freq2Bin(TLSItuChMfg[0]*.1+190, l_x_cbdi.m_f_tofStartFrequency);
        TLSCntrBin[1] = Freq2Bin(TLSItuChMfg[1]*.1+190, l_x_cbdi.m_f_tofStartFrequency);


        if (l_i_status = SLDGetData(&l_ui16_sldADC))
        {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: SLDGetData Failed: %d",l_i_status);
            break;
        }
        fprintf(osaCalReport,"sldADC %x at start of calibration\n",l_ui16_sldADC);

        /* SLD ON */
        if (l_i_status = SetCurrent(0x800))
        {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: SLD ON Failed : %d",l_i_status);
            //sleep(2);
            break;
        }

        usleep(500000);

        if (l_i_status = SLDGetData(&l_ui16_sldADC))
        {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: SLDGetData Failed: %d",l_i_status);
            break;
        }
        fprintf(osaCalReport,"sldADC %x during calibration\n",l_ui16_sldADC);

        int n=785; // total number of bins expected
        //int clmHeader[10] = {0};
        for (int i=0; i<N; i++)
        {   usleep(100000); // wait for CTM to complete another scan
            //clmHeader[i] = i+1;
            if (l_i_status = OcmGetCDGT1(&cdgt1[i]))
            {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: OcmGetCDGT1 Failed : %d",l_i_status);
                break;
            }
            if (n != cdgt1[i].m_frequency)
            {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: Wrong nuber of bins recieved from CTM %d recieved but %d  expected\n", cdgt1[i].m_frequency, n);
            }
        }

        /* SLD OFF */
        if (l_i_status = SetCurrent(0x00))
        {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: SLD OFF Failed : %d",l_i_status);
            break;
        }

        /* calcualting sum of scan */
        for (int i=0; i<n; i++)
        {   for(int j=0; j<N; j++)
            {   sum[i] += cdgt1[j].m_power[i];
            }
        }

        fprintf(osaCalReport,"\n\nBins,%d\nbinID",n);
        for(uint32_t i = 0; i < N; i++)
            fprintf(osaCalReport,", SLD Scan#%d",i);
        fprintf(osaCalReport,"sum\n");

        for (int i=0; i<n; i++)
        {   fprintf(osaCalReport,"%d, ",i);
            for(int j=0; j<N; j++)
            {   fprintf(osaCalReport,"%f, ",cdgt1[j].m_power[i]);
            }
            fprintf(osaCalReport,"%f\n",sum[i]);
        }


        /* set the OSW back to measure mode */
        if (l_i_status = OSWmeasure())
        {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: OSWmeasure Failed : %d",l_i_status);
            break;
        }

        // SF: this may be 'wrong' but it does not matter as the result is not used. Could be any equally spaced array of values. TODO: Eliminate
        float freq[785] = {0};
        for (int i=0; i<n; i++)
        {   freq[i] = (n*.00625+191.9375);
        }


        // Process the SLD Scan (find centers and width)
        float l_f_npData[4] = {0};
        char * res_key[] = {"notch_center_bin0", "notch_width0", "notch_center_bin1", "notch_width1"};
        l_i_status = CTMFindNotchPosition(freq, sum, 1, 1, l_f_npData);
        if (l_i_status)
        {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: CTMFindNotchPosition Failed : %d",l_i_status);
            break;
        }


        // validate that the notch filter locations and width are approximately correct
        if (ValidateRange(l_f_npData[0], 113, 135) && ValidateRange(l_f_npData[1], 560, 580) &&
                ValidateRange(l_f_npData[2], 20, 34) && ValidateRange(l_f_npData[3], 20, 34))
        {   l_i_status = CAL_NOTCHFLTR_CAL_FAILED;
            LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: Notch Filter Calibration Failed : %d",l_i_status);
            break;
        }


        float D0 = l_f_npData[0] - NotchFilterCntrMfg[0] - TLSCntrBin[0] + TLSCntrMfg[0];
        float D1 = l_f_npData[2] - NotchFilterCntrMfg[1] - TLSCntrBin[1] + TLSCntrMfg[1];
        float sbocDelta = 6.25*0.5*(D0 + D1);

        float sbtcDelta = -6.25*( D0 - D1)*6.25*(n-1)/((TLSItuChMfg[0]-TLSItuChMfg[1])*100);

        // calculate new cal seetings
        float l_f_new_sboc = 0;
        float l_f_new_sbtc = 0;
        l_f_new_sboc = l_x_cbdi.m_f_tofAppliedSBOC + sbocDelta;
        l_f_new_sbtc = l_x_cbdi.m_f_tofAppliedSBTC + sbtcDelta; // # this will change
        *new_sboc = l_f_new_sboc;
        *new_sbtc = l_f_new_sbtc;
        LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: sboc:  Mfg: %f   Current: %f  New: %f", l_f_sbocMfgBaseLine, l_x_cbdi.m_f_tofAppliedSBOC, l_f_new_sboc);
        LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: sbtc:  Mfg: %f   Current: %f  New: %f", l_f_sbtcMfgBaseLine, l_x_cbdi.m_f_tofAppliedSBTC, l_f_new_sbtc);
        if (i_b_commit)
        {   // write the new cal parameters to the CTM

            l_i_status = OcmSetSSTC1(l_f_new_sbtc);
            if (l_i_status)
            {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: OcmSetSSTC1 failed", __func__);
                break;
            }
            l_i_status = OcmSetSSBC1(l_f_new_sboc);
            if (l_i_status)
            {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: OcmSetSSBC1 failed", __func__);
                break;
            }
            T_CBDI l_x_cbdi = {0};
            l_i_status = OcmGetCBDI(&l_x_cbdi);
            if (!l_i_status)
            {   if ((abs(l_f_new_sboc - l_x_cbdi.m_f_tofAppliedSBOC)>0.01) || (abs(l_f_new_sbtc - l_x_cbdi.m_f_tofAppliedSBTC)>0.00001))
                {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: Readback of calibration values writen did not match");
                    break;
                }
            }
            LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: Calibration data is commited");
        }
        else
        {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: Calibration data is not commited");
        }

    }
    while(0);

    if(osaCalReport)
        fclose(osaCalReport);
    /** Deinit Logger */
    if(g_px_OsaCallgrFileptr)
        logMgrClose(&g_px_OsaCallgrFileptr);

    return l_i_status;
}
/**************************************************************************************************
 *
 * @brief   This function set calibration data as per input
 *
 * @param   sboc input SBOC Calibration data
 * @param   sbtc input SBOC Calibration data
 *
 * @return  returns 0 in case success, error code otherwise.
 *
 **************************************************************************************************/
int SetCTMCalibration(float sboc, float sbtc)
{   int l_i_status = 0;
    do
    {   if(!g_px_OsaCallgrFileptr)
        {   setLogOutput(2);
            setFileLogInfo(&g_px_OsaCallgrFileptr, OSACAL_LOGGER_FILE_PATH, NULL);
            setLogLevel(3);
        }

        l_i_status = OcmSetSSTC1(sbtc);
        if (l_i_status)
        {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: OcmSetSSTC1 Failed\n");
            break;
        }

        l_i_status = OcmSetSSBC1(sboc);
        if (l_i_status)
        {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: OcmSetSSBC1 Failed\n");
            break;
        }
        usleep(100000);
        T_CBDI l_x_cbdi = {0};
        l_i_status = OcmGetCBDI(&l_x_cbdi);
        if (!l_i_status)
        {   if ((abs(sboc - l_x_cbdi.m_f_tofAppliedSBOC)> 0.01f) || (abs(sbtc - l_x_cbdi.m_f_tofAppliedSBTC)> 0.00001f))
            {   LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: ERROR: Readback of calibration values writen did not match SBOC: %f SBTC: %f\n",l_x_cbdi.m_f_tofAppliedSBOC,l_x_cbdi.m_f_tofAppliedSBTC);
                break;
            }
        }
        LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: [%s] new sboc: %f ", __func__,sboc);
        LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: [%s] new sbtc: %f ", __func__,sbtc);
    }
    while(0);

    /** Deinit Logger */
    if(g_px_OsaCallgrFileptr)
        logMgrClose(&g_px_OsaCallgrFileptr);

    return l_i_status;
}
/**************************************************************************************************
 *
 * @brief   This function reset calibration data to MFG Baseline
 *
 *
 * @return  returns 0 in case success, error code otherwise.
 *
 **************************************************************************************************/
int ResetCTMCalibrationToMFGBaseline()
{   int l_i_status = 0;
    uint8_t l_ui8_sbocMfgBaseLine[4] = {0};
    uint8_t l_ui8_sbtcMfgBaseLine[4] = {0};
    uint8_t l_ui8_infinite[4] = {0xff,0xff,0xff,0xff};

    float l_f_sbocMfgBaseLine = 0;
    float l_f_sbtcMfgBaseLine = 0;
    float l_f_infinite = byte2Float(l_ui8_infinite);
    setLogOutput(2);
    if(!g_px_OsaCallgrFileptr)
        setFileLogInfo(&g_px_OsaCallgrFileptr, OSACAL_LOGGER_FILE_PATH, NULL);
    setLogLevel(3);
    do
    {   l_i_status = EEPROMPageRead(218, 4, l_ui8_sbocMfgBaseLine);
        l_i_status = EEPROMPageRead(214, 4, l_ui8_sbtcMfgBaseLine);
        if(!l_i_status)
        {   l_f_sbocMfgBaseLine = (float)byte2Float(l_ui8_sbocMfgBaseLine);
            l_f_sbtcMfgBaseLine = (float)byte2Float(l_ui8_sbtcMfgBaseLine);
            if(l_f_sbocMfgBaseLine == l_f_infinite || l_f_sbtcMfgBaseLine == l_f_infinite)
            {   l_i_status = CAL_MFG_BL_NOT_FOUND;
                break;
            }
            LOG_INFO(&g_px_OsaCallgrFileptr,NULL, 0, "OSACAL: resetting Calibration");
            l_i_status = SetCTMCalibration(l_f_sbocMfgBaseLine, l_f_sbtcMfgBaseLine);
        }
    }
    while(0);
    /** Deinit Logger */
    if(g_px_OsaCallgrFileptr)
        logMgrClose(&g_px_OsaCallgrFileptr);
    return l_i_status;
}
