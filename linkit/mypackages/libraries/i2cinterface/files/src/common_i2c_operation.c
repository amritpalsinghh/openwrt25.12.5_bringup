/**************************************************************************************************
 *  @file   common_i2c_operation.c
 *  @brief  Common i2c operation API Library
 *
 *  This file contains common i2c APIs implementations.
 *
**************************************************************************************************/
#include <string.h>
#include <json.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
/* Common i2c operation header */
#include "common_i2c_operation.h"
#include "otdr_interface.h"
#include "ocm_interface.h"
#include "product_info.h"
#include "freescale_interface.h"
#include "led_control.h"
#include "xmfh_err.h"
#define I2C_BUS 0

#define EEPROM_ADDRESS 0x56
#define ADC_ADDRESS    0x37
#define ADG_ADDRESS    0x49
#define DAC_DAC_DATA   0x08
#define DAC_GAIN       0x04
// Legit addresses for ADC128D818
#define DAC_ADDRESS    0x48

#define HEALTH_CHECK_PASS "Pass"
#define HEALTH_CHECK_FAIL "Fail"

/** File locations */
#define HEALTH_CHECK_FILE_PATH          "/root/HealthCheck.csv"
#define PROD_INFO_FILE_PATH         	"/tmp/product_info.json"
#define BUILD_FILE_PATH             	"/etc/banner"
#define BUF                         	2048

#define MRAA_INIT_ERROR           2100
#define EEPROM_SET_ADDR_ERROR     2101
#define EEPROM_FREQUENCY_ERROR    2102
#define EEPROM_READ_ERROR         2103
#define EEPROM_NACK_ERROR         2104
#define EEPROM_WRITE_ERROR        2105


#define I2C_READ_TIMEOUT_CONFIG  "/tmp/i2cReadTimeoutConfig"
#define I2C_INPROGRESS_TIMEOUT_CONFIG  "/tmp/i2cInProgressTimeoutConfig"

#define OP_SW_SUCCESS         0
#define OP_SW_INIT_ERROR      1720
#define OP_SW_SET_ADDR_ERROR  1721
#define OP_SW_FREQUENCY_ERROR 1722
#define OP_SW_READ_ERROR      1723
#define OP_SW_WRITE_ERROR     1724
#define GPIO_INIT_ERROR       1725
#define GPIO_DIR_ERROR        1726
#define GPIO_WRITE_ERROR      1727
#define GPIO_DEINIT_ERROR     1728

#define ADC_SUCCESS           0
#define ADC_INIT_ERROR        1740
#define ADC_FREQUENCY_ERROR   1741
#define ADC_SET_ADDR_ERROR    1742
#define ADC_READ_WORD_ERROR   1743
#define ADC_OUT_OF_RANGE      1744
#define ADC_FAIL              1745

/* I2C ERRORS */
#define WRITE_ERROR     1771
#define READ_ERROR      1772
#define FREQUENCY_ERROR 1773
#define SET_ADDR_ERROR  1774
#define INIT_ERROR      1775

#define DAC_INIT_ERROR      1781
#define DAC_SET_ADDR_ERROR  1782
#define DAC_FREQUENCY_ERROR 1783
#define DAC_READ_ERROR      1784
#define DAC_WRITE_ERROR     1785

#define OP_NACK_ERROR     1786

#define I2C_IO_EXPANDER1  0x27
#define I2C_IO_EXPANDER2  0x26


#define PWR_OTDR 0x8000
#define PWR_OSA 0x4000
#define PWR_OSW 0x0100
#define PWR_SFP 0x2000
#define SFP_TX_DIS 0x0008
#define CTM_Status 0x0020
#define CTM_DATA_RDY 0x0400
#define CTM_MR 0x0002
#define PWR_ALL PWR_OTDR + PWR_OSA + PWR_OSW + PWR_SFP
#define PWR_ALL_EXCEPT_SFP PWR_OTDR + PWR_OSA + PWR_OSW
#define HARDWARE_REVISION_1_1 "/tmp/hwrev1.1"


int FslVerCmd(char *o_c_ver);
int FslPackage(char *o_c_package);
int initialProductInfo(char *info);
int power_off_sfp();
int BatteryAPIInit(void);
int GetBatteryChargerTemp(float *o_f_temp);
int GetBatteryChargeCycle(uint16_t *o_ui_chrgcycle);
int BatteryAPIDeInit(void);

/**
 * @brief  Remove trailing spaces from input string
 *         This function remove consecutive leading & trailing spaces from input string
 *
 * @param  str  pointer to a string
 */

void trimleadingandTrailing(char *s)
{   int  i,j;

    for(i=0; s[i]==' '||s[i]=='\t' || s[i] == '\r' || s[i] == '\n'; i++);

    for(j=0; s[i]; i++)
    {   s[j++]=s[i];
    }
    s[j]='\0';
    for(i=0; s[i]!='\0'; i++)
    {   if(s[i]!=' '&& s[i]!='\t' && s[i]!='\r' && s[i]!='\n')
            j=i;
    }
    s[j+1]='\0';
}

/**************************************************************************************************
 *
 * @brief   This function converts temperature from input data
 *
 * @param   i_ui16_input is the input data to convert the data
 * @param   o_f_output is the coverted data into float.
 *
 * @return  returns DIAG_SUCCESS
 **************************************************************************************************/

int ConvertT(uint16_t i_ui16_input, float *o_f_output)
{   uint16_t l_ui16_tmp;
    l_ui16_tmp = (i_ui16_input & 0x7f)<<8 | (i_ui16_input & 0xff00)>>8;
    l_ui16_tmp = l_ui16_tmp >> 7;
    if ((i_ui16_input&0x80) != DIAG_SUCCESS)
    {   l_ui16_tmp = (i_ui16_input & 0xff)<<8 | (i_ui16_input & 0xff00)>>8;
        l_ui16_tmp = l_ui16_tmp >> 7;
        l_ui16_tmp = l_ui16_tmp * 1.0 - 512.;
    }
    *o_f_output=l_ui16_tmp*0.5;
    return DIAG_SUCCESS;
}

/**************************************************************************************************
 *
 * @brief   This function converts voltage from input data
 *
 * @param   i_ui16_input is the input data to convert the data
 * @param   o_f_output is the coverted data into float.
 *
 * @return  returns DIAG_SUCCESS
 **************************************************************************************************/
int ConvertVolt(uint16_t i_ui16_input, float *o_f_output)
{   i_ui16_input=(i_ui16_input>>8) + (i_ui16_input & 0xFF) * 256;
    *o_f_output = 4095.- i_ui16_input / 16.;
    return DIAG_SUCCESS;
}

/**************************************************************************************************
 *
 * @brief   This function reads diagnostics data
 *
 * @param   diag returns diagnostics data as a structure T_DIAG
 *
 * @return  On success, returns DIAG_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetADCVcc(float *o_f_vcc)
{   int l_i_status = ADC_SUCCESS;
    mraa_i2c_context adc_init = NULL;
    float l_f_voltage_divider = 10/(10+10.);
    int l_i_scale_factor = 1;
    float l_f_Vref = 2.048;

    do
    {   /* ADC Init*/
        adc_init = mraa_i2c_init(I2C_BUS);
        if (NULL == adc_init)
        {   l_i_status = ADC_INIT_ERROR;
            break;
        }

        /* Run the device at 100kHz */
        if(mraa_i2c_frequency(adc_init, MRAA_I2C_STD))
        {   l_i_status = ADC_FREQUENCY_ERROR;
            break;
        }

        /** ADC I2C address */
        if (mraa_i2c_address(adc_init, ADC_ADDRESS))
        {   l_i_status = ADC_SET_ADDR_ERROR;
            break;
        }

        uint16_t o_ui16_channel = {0};

        /* Read the register */
        if (mraa_i2c_read_bytes_data(adc_init, 0x20 + 3, (uint8_t*)&o_ui16_channel, 2) == -1)
        {   return  ADC_READ_WORD_ERROR;
        }

        /* Read channel data */
        float l_f_tmp;
        if(l_i_status = ConvertVolt(o_ui16_channel, &l_f_tmp))
        {   l_i_status = ADC_FAIL;
            break;
        }

        *o_f_vcc = l_i_scale_factor*(l_f_Vref-l_f_tmp*l_f_Vref/4096)/l_f_voltage_divider;
        if ((*o_f_vcc < 3.05) && (*o_f_vcc > 3.55))
        {   l_i_status = ADC_OUT_OF_RANGE;
        }
    }
    while(0);

    if (adc_init)
    {   mraa_i2c_stop(adc_init);
        adc_init = NULL;
    }

    return l_i_status;
}

int VerifyAnalogSwitch()
{   int l_i_status = 0;
    mraa_i2c_context l_p_adg_init = NULL;
    uint8_t l_ui8_sw_byte = 0;
    uint8_t l_ui8_sw_byte1 = 0;
    uint8_t l_ui8_sw_byte2 = 0;
    uint8_t l_ui8_tx_buf[2] = {0};
    do
    {   /** ADG init*/
        l_p_adg_init = mraa_i2c_init(0);
        if (l_p_adg_init == NULL)
        {   l_i_status = INIT_ERROR;
            break;
        }

        /** Set the slave Address*/
        l_i_status = mraa_i2c_address(l_p_adg_init, ADG_ADDRESS);
        if (l_i_status != 0)
        {   l_i_status = SET_ADDR_ERROR;
            break;
        }

        /*Run the device at 100kHz */
        l_i_status = mraa_i2c_frequency(l_p_adg_init, MRAA_I2C_STD);
        if (l_i_status != 0)
        {   l_i_status = FREQUENCY_ERROR;
            break;
        }
        /*Verify that the communictaion with the gain and PD switches works
          writing 0x00 turns all the switches off. 0xFF would turn them all on */
        if(mraa_i2c_read(l_p_adg_init, &l_ui8_sw_byte, 1) == -1)
        {   l_i_status = READ_ERROR;
            break;
        }

        l_ui8_sw_byte1 = 0xFF - l_ui8_sw_byte;

        /* write two complement to verify that we can modify the switch */
        l_ui8_tx_buf[0] = ADG_ADDRESS;
        l_ui8_tx_buf[1] = l_ui8_sw_byte1;
        l_i_status = mraa_i2c_write(l_p_adg_init,l_ui8_tx_buf,2);
        if (l_i_status != 0)
        {   l_i_status = WRITE_ERROR;
        }

        if(mraa_i2c_read(l_p_adg_init, &l_ui8_sw_byte2, 1) == -1)
        {   l_i_status = READ_ERROR;
            break;
        }

        l_ui8_tx_buf[0] = ADG_ADDRESS;
        l_ui8_tx_buf[1] = l_ui8_sw_byte;
        l_i_status = mraa_i2c_write(l_p_adg_init,l_ui8_tx_buf,2);
        if (l_i_status != 0)
        {   l_i_status = WRITE_ERROR;
        }

        /* verify that d2 is indeed qual d (indicating success) and return an error if not */
        if (l_ui8_sw_byte2 != l_ui8_sw_byte1)
        {   printf("ERROR: failed to communicate with the gain and PD switches\n");
            l_i_status = WRITE_ERROR;
            break;
        }
    }
    while(0);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function reads diagnostics data
 *
 * @param   diag returns diagnostics data as a structure T_DIAG
 *
 * @return  On success, returns DIAG_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetDiagnosticsData(T_DIAG *diag)
{   int l_i_status = DIAG_SUCCESS;
    mraa_i2c_context diag_init = NULL;
    uint8_t l_ui8_data;
    uint16_t l_ui16_data;
    //voltage divider
    float l_f_voltage_divider[8] = {1., 4.10/(10+4.99), 4, 10/(10+10.), 1./(10+1.), 4.99/(10+4.99), 4.99/(10+4.99), 1.69/(10+1.69)};
    //scale factor for conversion to other units
    int l_i_scale_factor[8] = {1, 1, 4, 1, 1, 1, 1, 1};
    float l_f_V[8] = {0};
    float l_f_Vref = 2.048;

    do
    {   /* Diag Init*/
        diag_init = mraa_i2c_init(I2C_BUS);
        if (NULL == diag_init)
        {   l_i_status = DIAG_INIT_ERROR;
            break;
        }
        usleep(20000);

        /* Switch I2C address */
        if (mraa_i2c_address(diag_init, 0x49))
        {   l_i_status = DIAG_SET_ADDR_ERROR;
            break;
        }
        usleep(20000);

        /* Run the device at 100kHz */
        if(mraa_i2c_frequency(diag_init, MRAA_I2C_STD))
        {   l_i_status = DIAG_FREQUENCY_ERROR;
            break;
        }
        usleep(20000);

        /* writing 0x00 turns all the switches off. 0xFF would turn them all on */
        l_i_status = mraa_i2c_write_byte(diag_init, 0x00);
        if(l_i_status != DIAG_SUCCESS && errno == 6)
        {   l_i_status = DIAG_I2C_NACK_ERROR;
            break;
        }
        else if(l_i_status != DIAG_SUCCESS)
        {   l_i_status =  DIAG_WRITE_ERROR;
            break;
        }
        usleep(100000);

        /* ADC I2C */
        diag_init = mraa_i2c_init(I2C_BUS);
        if (NULL == diag_init)
        {   l_i_status = DIAG_INIT_ERROR;
            break;
        }
        usleep(20000);

        /* Run the device at 100kHz */
        if(mraa_i2c_frequency(diag_init, MRAA_I2C_STD))
        {   l_i_status = DIAG_FREQUENCY_ERROR;
            break;
        }
        usleep(20000);

        /** ADC I2C address */
        if (mraa_i2c_address(diag_init, 0x37))
        {   l_i_status = DIAG_SET_ADDR_ERROR;
            break;
        }
        usleep(20000);

        /* Read Configuration register */
        l_i_status = mraa_i2c_read_bytes_data(diag_init, 0x00, &diag->conf_reg, 1);
        if(l_i_status == -1 && errno == 6)
        {   l_i_status = DIAG_I2C_NACK_ERROR;
            break;
        }
        else if(l_i_status == -1)
        {   l_i_status =  DIAG_READ_ERROR;
            break;
        }
        usleep(100000);

        /* Read Manufacturing ID register */
        l_i_status = mraa_i2c_read_bytes_data(diag_init, 0x3e, &diag->mfg_id, 1);
        if(l_i_status == -1 && errno == 6)
        {   l_i_status = DIAG_I2C_NACK_ERROR;
            break;
        }
        else if(l_i_status == -1)
        {   l_i_status =  DIAG_READ_ERROR;
            break;
        }
        usleep(100000);

        /* Read Revision ID register */
        l_i_status = mraa_i2c_read_bytes_data(diag_init, 0x3f, &diag->rev_id, 1);
        if(l_i_status == -1 && errno == 6)
        {   l_i_status = DIAG_I2C_NACK_ERROR;
            break;
        }
        else if(l_i_status == -1)
        {   l_i_status =  DIAG_READ_ERROR;
            break;
        }
        usleep(100000);

        /* Setting External Reference Voltage */
        l_i_status = mraa_i2c_write_byte_data(diag_init, 0x01, 0x0b);
        if(l_i_status != DIAG_SUCCESS && errno == 6)
        {   l_i_status = DIAG_I2C_NACK_ERROR;
            break;
        }
        else if(l_i_status != DIAG_SUCCESS)
        {   l_i_status =  DIAG_WRITE_ERROR;
            break;
        }
        usleep(100000);

        /* Setting Continuous Conversion Mode */
        l_i_status = mraa_i2c_write_byte_data(diag_init, 0x01, 0x07);
        if(l_i_status != DIAG_SUCCESS && errno == 6)
        {   l_i_status = DIAG_I2C_NACK_ERROR;
            break;
        }
        else if(l_i_status != DIAG_SUCCESS)
        {   l_i_status =  DIAG_WRITE_ERROR;
            break;
        }
        usleep(100000);

        /* Read Conversion Rate register */
        l_i_status = mraa_i2c_read_bytes_data(diag_init, 0x07, &diag->conv_rate, 1);
        if(l_i_status == -1 && errno == 6)
        {   l_i_status = DIAG_I2C_NACK_ERROR;
            break;
        }
        else if(l_i_status == -1)
        {   l_i_status =  DIAG_READ_ERROR;
            break;
        }
        usleep(100000);

        /* Read Channel Disable register */
        l_i_status = mraa_i2c_read_bytes_data(diag_init, 0x08, &diag->ch_disable_reg, 1);
        if(l_i_status == -1 && errno == 6)
        {   l_i_status = DIAG_I2C_NACK_ERROR;
            break;
        }
        else if(l_i_status == -1)
        {   l_i_status =  DIAG_READ_ERROR;
            break;
        }
        usleep(100000);

        /* Read Status register */
        l_i_status = mraa_i2c_read_bytes_data(diag_init, 0x0c, &diag->status_reg, 1);
        if(l_i_status == -1 && errno == 6)
        {   l_i_status = DIAG_I2C_NACK_ERROR;
            break;
        }
        else if(l_i_status == -1)
        {   l_i_status =  DIAG_READ_ERROR;
            break;
        }
        usleep(100000);

        /* Setting Start bit of the Config Register */
        l_i_status = mraa_i2c_write_byte_data(diag_init, 0x01, 0x00);
        if(l_i_status != DIAG_SUCCESS && errno == 6)
        {   l_i_status = DIAG_I2C_NACK_ERROR;
            break;
        }
        else if(l_i_status != DIAG_SUCCESS)
        {   l_i_status =  DIAG_WRITE_ERROR;
            break;
        }
        usleep(100000);

        uint16_t o_ui16_channel[8] = {0};
        uint8_t l_i_loop=0;
        for (l_i_loop = 0; l_i_loop < 8; l_i_loop++)
        {   /* Read the register */
            l_i_status = mraa_i2c_read_bytes_data(diag_init, 0x20 + l_i_loop, (uint8_t*)&o_ui16_channel[l_i_loop], 2);
            if(l_i_status == -1 && errno == 6)
            {   return DIAG_I2C_NACK_ERROR;
            }
            else if(l_i_status == -1)
            {   return DIAG_READ_WORD_ERROR;
            }
            usleep(100000);
        }

        /* Read the register */
        l_i_status = mraa_i2c_read_bytes_data(diag_init, 0x27, (uint8_t*)&l_ui16_data, 2);
        if(l_i_status == -1 && errno == 6)
        {   return DIAG_I2C_NACK_ERROR;
        }
        else if(l_i_status == -1)
        {   return DIAG_READ_WORD_ERROR;
        }
        usleep(100000);

        /* Read the Temperature */
        if(l_i_status = ConvertT(l_ui16_data, &diag->temp))
        {   l_i_status = DIAG_FAIL;
            break;
        }

        /* Read channels data */
        for(l_i_loop = 0; l_i_loop < 8; l_i_loop++)
        {   float l_f_tmp;
            if(l_i_status = ConvertVolt(o_ui16_channel[l_i_loop], &l_f_tmp))
            {   l_i_status = DIAG_FAIL;
                break;
            }
            diag->channel[l_i_loop] = l_i_scale_factor[l_i_loop]*(l_f_Vref-l_f_tmp*l_f_Vref/4096)/l_f_voltage_divider[l_i_loop];
        }
    }
    while(0);

    if (diag_init)
    {   mraa_i2c_stop(diag_init);
        diag_init = NULL;
    }

    return l_i_status;
}

/**************************************************************************************************
 * @brief This function read a single page from designated register
 *
 * @param i_ui8_registerAddress input register address
 * @param i_ui16_n input length of bytes (max limit is 63)
 * @param o_ui8_data returns bytes of given length
 *
 * @return On success, returns 0, error code otherwise
 *
 **************************************************************************************************/
int EEPROMPageRead(uint8_t i_ui8_registerAddress, uint16_t i_ui16_n, uint8_t *o_ui8_data)
{   int l_i_status = 0;
    int l_i_read_bytes_len = 0;
    mraa_i2c_context eeprom_init = NULL;

    do
    {   /** EEPROM Init*/
        eeprom_init = mraa_i2c_init(0);
        if (eeprom_init == NULL)
        {   l_i_status = MRAA_INIT_ERROR;
            break;
        }
        /** Set the EEPROM Address*/
        l_i_status = mraa_i2c_address(eeprom_init, EEPROM_ADDRESS);
        if (l_i_status != 0)
        {   l_i_status = EEPROM_SET_ADDR_ERROR;
            break;
        }
        /** Set standard Frequency */
        l_i_status = mraa_i2c_frequency(eeprom_init, MRAA_I2C_STD);
        if (l_i_status != 0)
        {   l_i_status = EEPROM_FREQUENCY_ERROR;
            break;
        }

        /** Read n bytes from EEPROM */
        l_i_read_bytes_len = mraa_i2c_read_bytes_data(eeprom_init, i_ui8_registerAddress, o_ui8_data, i_ui16_n);
        if(l_i_read_bytes_len == -1 && errno == 6)
        {   l_i_status = EEPROM_NACK_ERROR;
            break;
        }
        else if(l_i_read_bytes_len == -1)
        {   l_i_status = EEPROM_READ_ERROR;
            break;
        }
    }
    while(0);

    if(eeprom_init)
    {   mraa_i2c_stop(eeprom_init);
        eeprom_init = NULL;
    }

    return l_i_status;
}

/**************************************************************************************************
 * @brief This function write number of bytes to designated register
 *
 * @param i_ui8_registerAddress input register address
 * @param i_ui16_n input length of bytes (max limit is 63)
 * @param i_ui8_data input bytes of given length
 *
 * @return On success, returns 0, error code otherwise
 *
 **************************************************************************************************/
int EEPROMPageWrite(uint8_t i_ui8_registerAddress, uint16_t i_ui16_n, uint8_t *i_ui8_data)
{   int l_i_status = 0;
    int l_i_write_bytes_len = 0;
    mraa_i2c_context eeprom_init = NULL;

    do
    {   /** EEPROM Init*/
        eeprom_init = mraa_i2c_init(0);
        if (eeprom_init == NULL)
        {   l_i_status = MRAA_INIT_ERROR;
            break;
        }
        /** Set the EEPROM Address*/
        l_i_status = mraa_i2c_address(eeprom_init, EEPROM_ADDRESS);
        if (l_i_status != 0)
        {   l_i_status = EEPROM_SET_ADDR_ERROR;
            break;
        }
        /** Set standard Frequency */
        l_i_status = mraa_i2c_frequency(eeprom_init, MRAA_I2C_STD);
        if (l_i_status != 0)
        {   l_i_status = EEPROM_FREQUENCY_ERROR;
            break;
        }

        /** write n bytes to EEPROM */

        for(uint32_t i = 0; i < i_ui16_n; i++)
        {   l_i_status = mraa_i2c_write_byte_data(eeprom_init, i_ui8_data[i], i_ui8_registerAddress+i);
            if(l_i_status == -1 && errno == 6)
            {   l_i_status = EEPROM_NACK_ERROR;
                break;
            }
            else if(l_i_status == -1 || l_i_status)
            {   l_i_status = EEPROM_WRITE_ERROR;
                break;
            }
            usleep(10000);

        }
    }
    while(0);

    if(eeprom_init)
    {   mraa_i2c_stop(eeprom_init);
        eeprom_init = NULL;
    }

    return l_i_status;
}

/**************************************************************************************************
 * @brief This function reads hardware revision information from EEPROM
 *
 * @param Get EEPROM information as a structure (T_EEPROMIFO) containing string elements.
 *
 * @return On success, returns 0, error code otherwise
 *
 **************************************************************************************************/
int getEEPROMHardRev(uint8_t *o_c_harRev)
{   int l_i_status = 0;

    /** Read Hardware Revision from EEPROM */
    l_i_status = EEPROMPageRead(0x38, 4, o_c_harRev);
    if (l_i_status != 0)
    {   l_i_status = EEPROM_READ_ERROR;
    }

    return l_i_status;
}
/**************************************************************************************************
 * @brief This function reads APD Vendor information from EEPROM
 *
 * @param Get EEPROM information as a structure (T_EEPROMIFO) containing string elements.
 *
 * @return On success, returns 0, error code otherwise
 *
 **************************************************************************************************/
int getEEPROMApdVendor(uint8_t *o_c_ApdVen)
{   int l_i_status = 0;

    /** Read Hardware Revision from EEPROM */
    l_i_status = EEPROMPageRead(0x3F, 1, o_c_ApdVen);
    if (l_i_status != 0)
    {   l_i_status = EEPROM_READ_ERROR;
    }

    return l_i_status;
}
/**************************************************************************************************
 * @brief This function reads data from EEPROM
 *
 * @param Get EEPROM information as a structure (T_EEPROMIFO) containing string elements.
 *
 * @return On success, returns 0, error code otherwise
 *
 **************************************************************************************************/
int getEEPROMInfo(T_EEPROMIFO *o_x_eeprominfo)
{   int l_i_status = 0;
    char l_c_tmp_sn[17] = {0};
    int l_i_cnt = 0;

    do
    {   /** Read Hardware Revision from EEPROM */
        l_i_status = EEPROMPageRead(0x28, 16, o_x_eeprominfo->m_uc_partNumber);
        if (l_i_status != 0)
        {   l_i_status = EEPROM_READ_ERROR;
            break;
        }
        trimleadingandTrailing(o_x_eeprominfo->m_uc_partNumber);

        /** Read Hardware Revision from EEPROM */
        l_i_status = EEPROMPageRead(0x38, 4, o_x_eeprominfo->m_uc_hardwareRivision);
        if (l_i_status != 0)
        {   l_i_status = EEPROM_READ_ERROR;
            break;
        }
        /** Read Serial Number from EEPROM */
        l_i_status = EEPROMPageRead(0x44, 16, l_c_tmp_sn);
        if (l_i_status != 0)
        {   l_i_status = EEPROM_READ_ERROR;
            break;
        }

        for(int l_i_loop = 0; (l_c_tmp_sn[l_i_loop] != '\0') && (l_i_loop < (sizeof(l_c_tmp_sn))) ; ++l_i_loop)
        {   if (isalnum(l_c_tmp_sn[l_i_loop]) || l_c_tmp_sn[l_i_loop] == '.' || l_c_tmp_sn[l_i_loop] == ' ')
            {   o_x_eeprominfo->m_uc_serialnumber[l_i_cnt] = l_c_tmp_sn[l_i_loop];
                l_i_cnt++;
            }
        }
        trimleadingandTrailing(o_x_eeprominfo->m_uc_serialnumber);
    }
    while(0);

    return l_i_status;
}

/***************************************************************************************************
 * @brief  Enable / disable retry and configure timeout in the instances where MCU read operation indicates
 *         In Progress status.
 *
 * @param  config pass true or false to enable/disable Inprogress timeout
 * @param  InProgressTimeout value of timeout for Inprogress error from i2c
 ***************************************************************************************************/
void configure_mcu_inprogress_timeout(bool config, unsigned int InProgressTimeout)
{   FILE *fp = fopen(I2C_INPROGRESS_TIMEOUT_CONFIG, "w");
    if(fp)
    {   fprintf(fp,"%d %d", InProgressTimeout, (int)config);
        fclose(fp);
    }
}

/***************************************************************************************************
 * @brief  Configure Read timeout in the instances where MCU read operation indicates
 *         In Progress status.
 *
 * @param  readTimeout  value of timeout for overall read timeout
 ***************************************************************************************************/
void configure_mcu_read_timeout(unsigned int readTimeout)
{   FILE *fp = fopen(I2C_READ_TIMEOUT_CONFIG, "w");
    if(fp)
    {   fprintf(fp,"%d",readTimeout);
        fclose(fp);
    }
}

void setHealthBit(uint16_t *number, int *n)
{   *number |= 1UL << *n;
    (*n)++;
}

void printBits(unsigned int num, int n)
{   printf("Health Check Bit Sign: ");
    for(int bit=0; bit<n; bit++)
    {   printf("%i ", num & 0x01);
        num = num >> 1;
    }
    printf("\n");
}

void substring(char s[], char sub[], int p, int l)
{   int c = 0;

    while (c < l)
    {   sub[c] = s[p+c-1];
        c++;
    }
    sub[c] = '\0';
}

/**************************************************************************************************
 * @brief This function returns product information string containing json data
 *
 * @param info string containing json format data of firmware version and hardware rivision.
 *
 * @return  On success returns 0 and error code otherwise.
**************************************************************************************************/
int initialProductInfo(char *info)
{   FILE *fp = NULL;
    FILE *l_px_filePtr = NULL;
    FILE* l_px_healthCheckFp = NULL;

    int  l_i_status = 0;
    int  ret_fail_api = 0;
    int  ret_init_deint_status = 0;
    char buffer[BUF] = {0};
    char *buf = NULL;
    char *ret_str = NULL;
    char ver[64] = {0};
    char l_c_fsl_ver[64] = {0};
    char l_c_fsl_package[64] = {0};
    char l_ac_bff[512] = {0};
    char *l_pc_subStr = NULL;
    int  ret_api = 0;
    char l_c_otdr_ver[64] = {0};
    char l_c_otdr_FPGAver[64] = {0};
    T_VER_FULL otdr_ver = {0};
    T_VER_FULL otdr_emu_ver = {0};
    char l_c_emu_ver[32] = {0};
    char l_c_CTM_FWVer[128] = {0};
    int l_i_checkOTDRCorruption = 0;

    json_bool ret_json_val = 0;
    struct json_object *parsed_json = NULL;
    struct json_object *parsed_json1 = NULL;
    struct json_object *parsed_json2 = NULL;
    struct json_object *root = NULL;
    struct json_object *subroot = NULL;
    struct json_object *subroot1 = NULL;
    struct json_object *leaf = NULL;
    char   str[BUF] = "{\"XMFH\":{\"Firmware Ver\":\"\",\"Hardware Rev\":\"\",\"Freescale Ver\":\"\",\"Freescale Package\":\"\",\"Serial Number\":\"\",\"OTDR\":{\"Firmware Ver\":\"\",\"Emulator Ver\": \"\",\"FPGA Ver\": \"\"},\"OCM\":{\"Firmware Ver\":\"\"}}}";
    T_EEPROMIFO eeprom = {0};
    /*mraa i2c context*/
    mraa_i2c_context l_px_pcfRead = NULL;
    uint16_t result = 0;
    int peripheral_n = 0;
    int l_i_isCorrectValue = 0;
    char l_c_subStr[28] = {0};

    do
    {   /* opening file & parsing string to json object*/
        /*Getting json object from the buffer*/
        root = json_tokener_parse(str);
        if(root == NULL)
        {   strncpy(info, "{\"ERROR\":\"Invalid JSON FORMAT Detected\"}",
                    strlen("{\"ERROR\":\"Invalid JSON FORMAT Detected\"}"));
            break;
        }

        l_px_healthCheckFp = fopen(HEALTH_CHECK_FILE_PATH, "w+");
        if(!l_px_healthCheckFp)
        {   strncpy(info, "{\"ERROR\":\" Failed to open HealthCheck File\"}",
                    strlen("{\"ERROR\":\" Failed to open HealthCheck File\"}"));
            break;
        }

        do
        {   /************ Update version for XMFH (Release Version) ************/
            memset(ver, 0, sizeof(ver));

            ret_json_val = json_object_object_get_ex(root, "XMFH", &subroot);
            if(!ret_json_val)
            {   strncpy(info, "{\"ERROR\":\"ver node not found.\"}",
                        strlen("{\"ERROR\":\"ver node not found.\"}"));
                break;
            }

            buf = strdup(json_object_get_string(subroot));

            /* Parsing subobject */
            parsed_json = json_tokener_parse(buf);
            if (buf != NULL)
            {   free(buf);
                buf = NULL;
            }
            /* Get version details from instrument */
            l_px_filePtr = fopen(BUILD_FILE_PATH, "r");

            if(l_px_filePtr != NULL)
            {   if(fread(l_ac_bff, 1, sizeof(l_ac_bff)-1, l_px_filePtr) > 1)
                {   if((l_pc_subStr = strstr(l_ac_bff, ": v"))!=NULL)
                    {   ret_str = strtok((l_pc_subStr+3),"\n");
                        if(ret_str != NULL)
                            sprintf(ver, "%s",ret_str);
                    }
                }
                fclose(l_px_filePtr);
                l_px_filePtr = NULL;
            }

            /* Add version details in leaf */
            leaf=json_object_new_string (ver);
            /* Adding change in respective subobject */
            json_object_object_add(parsed_json, "Firmware Ver", leaf);

            if (!getEEPROMInfo(&eeprom))
            {   /* Add version details in leaf */
                leaf=json_object_new_string (eeprom.m_uc_hardwareRivision);
                /* Adding change in respective subobject */
                json_object_object_add(parsed_json, "Hardware Rev", leaf);

                /* Add version details in leaf */
                leaf=json_object_new_string (eeprom.m_uc_serialnumber);
                /* Adding change in respective subobject */
                json_object_object_add(parsed_json, "Serial Number", leaf);

                setHealthBit(&result, &peripheral_n);
            }

            if (eeprom.m_uc_partNumber[0] == '6' && eeprom.m_uc_partNumber[1] == '5' && eeprom.m_uc_partNumber[2] == 'H')
            {   for (int i = 3; eeprom.m_uc_partNumber[i] != '\0'; i++)
                {   // check for decimal digits
                    if (isdigit(eeprom.m_uc_partNumber[i]) == 0)
                    {   l_i_isCorrectValue = 2;
                    }
                }
            }

            if (eeprom.m_uc_serialnumber[0] == 'H' && l_i_isCorrectValue != 2)
            {   substring(eeprom.m_uc_serialnumber, l_c_subStr, 2, 8);
                int l_i_num = atoi(l_c_subStr);
                if((l_i_num % 10 == 0) && (l_i_num >= 2050010) && (l_i_num <= 2999990))
                {   l_i_isCorrectValue = 1;
                }
            }

            if (l_i_isCorrectValue == 1)
            {   fprintf(l_px_healthCheckFp, "EEPROM,%s\n", HEALTH_CHECK_PASS);
            }
            else
            {   fprintf(l_px_healthCheckFp, "EEPROM,%s\n", HEALTH_CHECK_FAIL);

            }

            /************ Freescale ver check ************/
            do
            {   /*Getting Freescale version*/
                ret_api = FslVerCmd(l_c_fsl_ver);
                if(ret_api)
                {   fprintf(l_px_healthCheckFp, "PwrMgtMCU,%s\n", HEALTH_CHECK_FAIL);
                    peripheral_n++;
                    break;
                }

                /**Write fslver status the CSV file*/
                fprintf(l_px_healthCheckFp, "PwrMgtMCU,%s\n", HEALTH_CHECK_PASS);
                setHealthBit(&result, &peripheral_n);

                /* Add version details in leaf */
                leaf=json_object_new_string (l_c_fsl_ver);
                /* Adding change in respective subobject */
                json_object_object_add(parsed_json, "Freescale Ver", leaf);

            }
            while(0);

              /************ Freescale package check ************/
            do
            {   /*Getting Freescale version*/
                ret_api = FslPackage(l_c_fsl_package);
                if(ret_api)
                {   fprintf(l_px_healthCheckFp, "PwrMgtMCU,%s\n", HEALTH_CHECK_FAIL);
                    peripheral_n++;
                    break;
                }

                /**Write fslver status the CSV file*/
                fprintf(l_px_healthCheckFp, "PwrMgtMCU,%s\n", HEALTH_CHECK_PASS);
                setHealthBit(&result, &peripheral_n);

                /* Add version details in leaf */
                leaf=json_object_new_string (l_c_fsl_package);
                /* Adding change in respective subobject */
                json_object_object_add(parsed_json, "Freescale Package", leaf);

            }
            while(0);

            /************ Freescale Slave LED Check  ************/
            do
            {   ret_api = LEDInit();
                if(ret_api != LED_SUCCESS)
                {   printf("Failed to Initialize Error <%d>\n", ret_api);
                    break;
                }
                /** LED Controller*/
                ret_api = LEDEnableController(1);
                if(ret_api != LED_SUCCESS)
                {   printf("Failed to Enable the LED Controller Error <%d>\n", ret_api);
                    break;
                }
                else
                {   //Turn on all LEDs white
                    printf("LED :Turn on all LEDs white \n");
                    ret_api = LEDBrightness(LED_ID_ALL, 255, 255, 255);
                    if(ret_api != LED_SUCCESS)
                    {   printf("Failed to set LED Brightness Error <%d>\n", ret_api);
                        break;
                    }
                }
            }
            while (0);

            /** Deinit the LED*/
            LEDDeInit();

            if (!ret_api)
            {   fprintf(l_px_healthCheckFp, "LED,%s\n", HEALTH_CHECK_PASS);
                setHealthBit(&result, &peripheral_n);
            }
            else
            {   fprintf(l_px_healthCheckFp, "LED,%s\n", HEALTH_CHECK_FAIL);
                peripheral_n++;
            }

            /************ Freescale Slave Battery Check  ************/
            do
            {   ret_api = BatteryAPIInit();
                if (ret_api != 0)
                {   printf("ERROR: BatteryAPIInit Error no: %d\n", ret_api);
                    break;
                }

                /**********Read Fuel Gauge*********/
                uint16_t l_ui16_tmp = 0;
                l_i_status = GetBatteryChargeCycle(&l_ui16_tmp);
                if (l_i_status != 0)
                {   printf("ERROR: GetBatteryChargeCycle Error no: %d\n", l_i_status);
                    /**Write Fuel Gauge check pass */
                    fprintf(l_px_healthCheckFp, "Fuel Gauge,%s\n", HEALTH_CHECK_FAIL);
                    peripheral_n++;
                }
                else
                {   printf("BatteryAPI GetBatteryChargeCycle: <%hu>\n", l_ui16_tmp);
                    /**Write Fuel Gauge check pass */
                    fprintf(l_px_healthCheckFp, "Fuel Gauge,%s\n", HEALTH_CHECK_PASS);
                    setHealthBit(&result, &peripheral_n);
                }

                /**********Read Charger**********/
                float l_f_tmp = 0;
                l_i_status = GetBatteryChargerTemp(&l_f_tmp);
                if (l_i_status != 0)
                {   printf("ERROR: GetBatteryChargerTemp Error no: %d\n", l_i_status);
                    /**Write Charger check pass */
                    fprintf(l_px_healthCheckFp, "Charger,%s\n", HEALTH_CHECK_FAIL);
                    peripheral_n++;
                }
                else
                {   printf("BatteryAPI GetBatteryChargerTemp: <%.2fC>\n", l_f_tmp);

                    fprintf(l_px_healthCheckFp, "Charger,%s\n", HEALTH_CHECK_PASS);
                    setHealthBit(&result, &peripheral_n);
                }
            }
            while (0);

            /** Deinit the Battery*/
            BatteryAPIDeInit();

            /************ Update version for OTDR ************/
            ret_api = json_object_object_get_ex(subroot, "OTDR", &subroot1);
            if(!ret_api)
            {   printf("Failed to parse json File for OTDR\n");
                break;
            }
            buf = strdup(json_object_get_string(subroot1));

            /* Parsing subobject */
            parsed_json1 = json_tokener_parse(buf);
            if (buf != NULL)
            {   free(buf);
                buf = NULL;
            }

            do
            {   /**** Initialize the serial port to communicate over uart ******/
                if (access("/tmp/lock/ttyS0.LCK", F_OK) == 0)  
                    remove("/tmp/lock/ttyS0.LCK");
				/* Get version details from instrument */
                ret_api = OtdrInterfaceInit(OTDR_NORMAL_MODE);
                if(ret_api)
                {   break;
                }

                /*Getting Firmware version from the OTDR*/
                ret_api = OtdrGetVerFull(&otdr_ver);
                if(ret_api)
                {   if (OTDR_CORRUPTION_ERROR == ret_api)
                    {   l_i_checkOTDRCorruption = 1;
                    }
                    break;
                }

                if(strlen(otdr_ver.version) < sizeof(l_c_otdr_ver) && strlen(otdr_ver.designversion) < sizeof(l_c_otdr_FPGAver))
                {   strncpy(l_c_otdr_ver, otdr_ver.version, strlen(otdr_ver.version));
                    otdr_ver.version[strlen(otdr_ver.version)] = 0;
                    strncpy(l_c_otdr_FPGAver, otdr_ver.designversion, strlen(otdr_ver.designversion));
                    otdr_ver.designversion[strlen(otdr_ver.designversion)] = 0;
                }

                /* Add version details in leaf */
                leaf=json_object_new_string (l_c_otdr_ver);
                /* Adding change in respective subobject */
                json_object_object_add(parsed_json1, "Firmware Ver",leaf);

                /* Add version details in leaf */
                leaf=json_object_new_string (l_c_otdr_FPGAver);
                /* Adding change in respective subobject */
                json_object_object_add(parsed_json1, "FPGA Ver",leaf);
            }
            while(0);

            if(!l_i_checkOTDRCorruption && !ret_api)
            {   fprintf(l_px_healthCheckFp, "OTDR Serial,%s\n", HEALTH_CHECK_PASS);
                setHealthBit(&result, &peripheral_n);
            }
            else
            {   fprintf(l_px_healthCheckFp, "OTDR Serial,%s\n", HEALTH_CHECK_FAIL);
                peripheral_n++;
            }

            /*DeInitialization of OtdrInterface*/
            ret_api = OtdrInterfaceDeInit();

            if (!l_i_checkOTDRCorruption)
            {   do
                {   /* Get version details from OTDR EMU */
                    ret_api = OtdrInterfaceInit(OTDR_SPEED_MODE);
                    if(ret_api)
                    {   break;
                    }

                    /*Getting Firmware version from the OTDR EMU*/
                    ret_api = OtdrGetVerFull(&otdr_emu_ver);
                    if(ret_api)
                    {   break;
                    }

                    if(strlen(otdr_emu_ver.emuversion) < sizeof(l_c_emu_ver))
                    {   strncpy(l_c_emu_ver, otdr_emu_ver.emuversion, strlen(otdr_emu_ver.emuversion));
                        otdr_emu_ver.emuversion[strlen(otdr_emu_ver.emuversion)] = 0;
                    }
                    /* Add version details in leaf */
                    leaf=json_object_new_string (l_c_emu_ver);
                    /* Adding change in respective subobject */
                    json_object_object_add(parsed_json1, "Emulator Ver",leaf);
                }
                while(0);

                if(ret_api)
                {   fprintf(l_px_healthCheckFp, "OTDR Socket,%s\n", HEALTH_CHECK_FAIL);
                    peripheral_n++;
                }
                else
                {   fprintf(l_px_healthCheckFp, "OTDR Socket,%s\n", HEALTH_CHECK_PASS);
                    setHealthBit(&result, &peripheral_n);
                }

                /*DeInitialization of OtdrInterface*/
                ret_api = OtdrInterfaceDeInit();
            }
            else
            {   fprintf(l_px_healthCheckFp, "OTDR Socket,%s\n", HEALTH_CHECK_FAIL);
                peripheral_n++;
            }
            /* Adding change in root object */
            json_object_object_add(parsed_json, "OTDR", parsed_json1);

            /************ Update version for OCM ************/
            ret_api = json_object_object_get_ex(subroot, "OCM", &subroot1);
            if(!ret_api)
            {   printf("Failed to parse json File for OCM\n");
                break;
            }
            buf = strdup(json_object_get_string(subroot1));

            /* Parsing subobject */
            parsed_json1 = json_tokener_parse(buf);
            if (buf != NULL)
            {   free(buf);
                buf = NULL;
            }

            /* Read OSA (CTM) Information */
            do
            {   /* Initialization of OcmInterface*/
                ret_api = OcmInterfaceInit();
                if(ret_api)
                {   break;
                }

                /*Getting Firmware Revision from the CTM*/
                ret_api = OcmGetDREV((char *)l_c_CTM_FWVer);
                if(ret_api)
                {   break;
                }

                if (strstr((char *) l_c_CTM_FWVer, (char *)"Compiled") == NULL)
                {   l_c_CTM_FWVer[0] = '\0';
                }

                /* Add version details in leaf */
                leaf=json_object_new_string (l_c_CTM_FWVer);
                /* Adding change in respective subobject */
                json_object_object_add(parsed_json1, "Firmware Ver",leaf);
            }
            while(0);

            if (!ret_api)
            {   fprintf(l_px_healthCheckFp, "OSA (CTM),%s\n", HEALTH_CHECK_PASS);
                setHealthBit(&result, &peripheral_n);
            }
            else
            {   fprintf(l_px_healthCheckFp, "OSA (CTM),%s\n", HEALTH_CHECK_FAIL);
                peripheral_n++;
            }

            /*Deinitialization of ocminterface*/
            ret_api = OcmInterfaceDeInit();

            /* Adding change in root object */
            json_object_object_add(parsed_json, "OCM", parsed_json1);
            int l_i_length = 0;

            /* Read IO Expander */
            do
            {   ret_api = 0;
                uint8_t l_ui8_data[2] = {0};

                l_px_pcfRead = mraa_i2c_init(I2C_BUS);
                if (!l_px_pcfRead)
                {   printf("ERROR: Failed to set the Address\n");
                    break;
                }

                ret_api = mraa_i2c_address(l_px_pcfRead, 0x27);
                if(ret_api != 0)
                {   printf("ERROR: Failed to set the Address\n");
                    break;
                }

                l_i_length = mraa_i2c_read(l_px_pcfRead, l_ui8_data, 2);
                if (l_i_length < 2)
                {   printf("ERROR: Failed to get the DATA from IO Expander");
                    break;
                }
                fprintf(l_px_healthCheckFp, "IO Expander,%s\n", HEALTH_CHECK_PASS);
                setHealthBit(&result, &peripheral_n);
            }
            while(0);

            if((ret_api != 0) && (l_i_length < 2))
            {   fprintf(l_px_healthCheckFp, "IO Expander,%s\n", HEALTH_CHECK_FAIL);
                peripheral_n++;
            }
            if(l_px_pcfRead)
            {   mraa_i2c_stop(l_px_pcfRead);
                l_px_pcfRead = NULL;
            }

            /* Read ADC */
            do
            {   float l_f_vcc = 0;
                ret_api = GetADCVcc(&l_f_vcc);

                if (!ret_api)
                {   printf("vcc:%f\n", l_f_vcc);
                    fprintf(l_px_healthCheckFp, "ADC,%s\n", HEALTH_CHECK_PASS);
                    setHealthBit(&result, &peripheral_n);
                }
                else
                {   fprintf(l_px_healthCheckFp, "ADC,%s\n", HEALTH_CHECK_FAIL);
                    peripheral_n++;
                }
            }
            while(0);

            /* Read Analog Switch */
            if (VerifyAnalogSwitch())
            {   fprintf(l_px_healthCheckFp, "Analog Switch,%s\n", HEALTH_CHECK_FAIL);
                peripheral_n++;
            }
            else
            {   fprintf(l_px_healthCheckFp, "Analog Switch,%s\n", HEALTH_CHECK_PASS);
                setHealthBit(&result, &peripheral_n);
            }

        }
        while(0);

        /* Adding change in root object */
        json_object_object_add(root, "XMFH", parsed_json);

        strcpy(buffer, json_object_get_string(root)); //json object convert into string and copy to buffer

        fp = fopen(PROD_INFO_FILE_PATH,"w+");
        if(fp == NULL)
        {   ret_api = errno;
            break;
        }
        fwrite(buffer, 1, strlen(buffer), fp); //write in file  with new changes

        strncpy(info, buffer, strlen(buffer));
    }
    while(0);

    /* Print health check in bit sign */
    printBits(result, peripheral_n);

    if (fp != NULL)
        fclose(fp);
    if (buf != NULL)
        free(buf);
    if(parsed_json!= NULL)
        json_object_put(parsed_json);
    if(parsed_json1!= NULL)
        json_object_put(parsed_json1);
    if(root!= NULL)
        json_object_put(root);
    if(subroot!= NULL)
        json_object_put(subroot);
    if(subroot1!= NULL)
        json_object_put(subroot1);
    if(leaf!= NULL)
        json_object_put(leaf);
    if (l_px_healthCheckFp)
    {   fclose(l_px_healthCheckFp);
        l_px_healthCheckFp = NULL;
    }

    return ret_api;
}

/*******************************************************************************************************
 * @brief This function returns product information
 *
 * @param i_c_info A string containing data in JSON format of hardware revision and firmware version.
 *
 * @return  On success returns 0 and error code otherwise.
******************************************************************************************************/
int ProductInfo(char *o_c_info)
{   FILE *l_px_readFile = NULL;
    int l_i_status = PRODUCT_INFO_SUCCESS;

    do
    {   if(o_c_info == NULL)
        {   break;
        }

        l_px_readFile = fopen(PROD_INFO_FILE_PATH,"r");
        if(l_px_readFile == NULL)
        {   l_i_status = PRODUCT_INFO_FAIL;
            break;
        }

        fread(o_c_info, BUF, 1, l_px_readFile);
        fclose(l_px_readFile);
        l_px_readFile = NULL;

    }
    while(0);

    return l_i_status;
}

/*******************************************************************************************************
 * @brief This function use to perform power operation to OTDR, OCM and SFP devices.
 *
 * @param i_i16_mask A Hexadecimal mask of devices to operate particular device ON/OFF.
 *
 * @return  On success returns 0 and error code otherwise.
*******************************************************************************************************/
int set_bits(int i_i16_mask, uint8_t i_ui8_slaveAddr)
{   int l_i_status = OP_SW_SUCCESS;
    int l_i_clr = 0;
    int l_i_m0 = 0;
    int l_i_m1 = 0;
    uint8_t l_ui8_ar_data[2] = {0};
    mraa_i2c_context op_sw_init = NULL;

    if (i_i16_mask < 0)
    {   l_i_clr = 1;
        i_i16_mask =-i_i16_mask;
    }

    l_i_m0 = i_i16_mask % 256;
    l_i_m1 = (i_i16_mask - l_i_m0) / 256;
#ifdef DEBUG
    printf("l_i_m0:%d\n", l_i_m0);
    printf("l_i_m1:%d\n", l_i_m1);
    printf ("Mask:%d\n",i_i16_mask);
#endif

    do
    {   /* Optical Switch Init*/
        op_sw_init = mraa_i2c_init(I2C_BUS);
        if (NULL == op_sw_init)
        {   l_i_status = OP_SW_INIT_ERROR;
            break;
        }
        /* Switch I2C address */
        if (mraa_i2c_address(op_sw_init, i_ui8_slaveAddr))
        {   l_i_status = OP_SW_SET_ADDR_ERROR;
            break;
        }
        /* Run the device at 100kHz */
        if(mraa_i2c_frequency(op_sw_init, MRAA_I2C_STD))
        {   l_i_status = OP_SW_FREQUENCY_ERROR;
            break;
        }
        /* Read the current IO settings. Only the TX_DIS bit will be modified */
        l_i_status = mraa_i2c_read_bytes_data(op_sw_init, i_ui8_slaveAddr, l_ui8_ar_data, 2);
        if(l_i_status == -1 && errno == 6)
        {   l_i_status = OP_NACK_ERROR;
            break;
        }
        else if(l_i_status == -1)
        {   l_i_status =  OP_SW_READ_ERROR;
            break;
        }
        if(i_ui8_slaveAddr == 0x26)
        {   l_ui8_ar_data[0] = 0xff;
            l_ui8_ar_data[1] = 0x3F;
        }
        else
        {   if (l_i_clr==1)
            {   /* set bits low */
                l_ui8_ar_data[0] = l_ui8_ar_data[0] & (0xFF-l_i_m0);
                l_ui8_ar_data[1] = l_ui8_ar_data[1] & (0xFF-l_i_m1);
            }
            else
            {   /* set bit high */
                l_ui8_ar_data[0] = l_ui8_ar_data[0] | l_i_m0;
                l_ui8_ar_data[1] = l_ui8_ar_data[1] | l_i_m1;
            }

            /* make sure that inputs are set high independent of current status */
            l_ui8_ar_data[0] = l_ui8_ar_data[0] | 0xD1;   // changed on 20201020
            l_ui8_ar_data[1] = l_ui8_ar_data[1] | 0x1A;
        }
        l_i_status = mraa_i2c_write(op_sw_init, l_ui8_ar_data, 2);
        if(l_i_status != OP_SW_SUCCESS && errno == 6)
        {   l_i_status = OP_NACK_ERROR;
            break;
        }
        else if(l_i_status != OP_SW_SUCCESS)
        {   l_i_status =  OP_SW_WRITE_ERROR;
            break;
        }

        mraa_i2c_stop(op_sw_init);
        op_sw_init = NULL;


    }
    while(0);

    return l_i_status;
}

/*******************************************************************************************************
 * @brief This function to toggle GPIO for front connector.
 *
 * @param i_i_gpio A GPIO number
 *
 * @param i_i_value A GPIO value
 *
 * @return  On success returns 0 and error code otherwise.
*******************************************************************************************************/
int OPSWGPIOWrite(int i_i_gpio, int i_i_value)
{   int l_i_status = OP_SW_SUCCESS;
    mraa_gpio_context l_gpio = NULL;

#ifdef DEBUG
    printf("Entering into %s GPIO:%d, value:%d\n", __func__, i_i_gpio, i_i_value);
#endif
    do
    {   /* initialize mraa for the platform (not needed most of the times) */
        mraa_init();
        /* initialize GPIO pin */
        l_gpio = mraa_gpio_init(i_i_gpio);
        if (l_gpio == NULL)
        {
#ifdef DEBUG
            printf("Failed to initialize GPIO: %d\n", i_i_gpio);
#endif
            mraa_deinit();
            l_i_status = GPIO_INIT_ERROR;
            break;
        }

        /* set GPIO to output */
        l_i_status = mraa_gpio_dir(l_gpio, MRAA_GPIO_OUT);
        if (l_i_status != OP_SW_SUCCESS)
        {
#ifdef DEBUG
            printf("Failed to set GPIO direction\n");
#endif
            l_i_status = GPIO_DIR_ERROR;
            break;
        }

        l_i_status = mraa_gpio_write(l_gpio, i_i_value);
        if (l_i_status != OP_SW_SUCCESS)
        {
#ifdef DEBUG
            printf("Failed to set GPIO value:%d\n", i_i_value);
#endif
            l_i_status = GPIO_WRITE_ERROR;
            break;
        }
        usleep(250000);
    }
    while(0);

#ifdef DEBUG
    printf("Exiting from %s GPIO Returned:%d\n", __func__, l_i_status);
#endif
    return l_i_status;
}

/*******************************************************************************************************
 * @brief This function enables optical switch.
 *
 * @return  On success returns 0 and error code otherwise.
*******************************************************************************************************/
int enable_op_sw()
{   int l_i_status = 0;

    do
    {   /* Pulling CTM MR low */
        if (l_i_status = set_bits(-CTM_MR, I2C_IO_EXPANDER1))
        {
#ifdef DEBUG
            printf("Pulling CTM MR low failed. Returned:%d\n", l_i_status);
#endif
            break;
        }
        usleep(500000);
        /* Enabling CTM PWR */
        if (l_i_status = set_bits(-PWR_OSA,I2C_IO_EXPANDER1))
        {
#ifdef DEBUG
            printf("Enabling CTM PWR failed. Returned:%d\n", l_i_status);
#endif
            break;
        }
        /* Pulling CTM MR high */
        if (l_i_status = set_bits(CTM_MR, I2C_IO_EXPANDER1))
        {
#ifdef DEBUG
            printf("Pulling CTM MR high failed. Returned:%d\n", l_i_status);
#endif
            break;
        }
        /* for(int second=0; second<25;second++)
        {
        	sleep(1);
        	printf("Waiting for CTM to boot %d/25 seconds", second);
        } */
        if (l_i_status = set_bits(-PWR_OSA-PWR_OSW, I2C_IO_EXPANDER1)) // Turn on the OSA and OSW
        {
#ifdef DEBUG
            printf("Turn on the OSA and OSW failed. Returned:%d\n", l_i_status);
#endif
            break;
        }
        if(l_i_status = set_bits(-CTM_DATA_RDY-CTM_Status, I2C_IO_EXPANDER1)) // Turn off the CTM interrupt
        {
#ifdef DEBUG
            printf("Turn off the CTM interrupt failed. Returned:%d\n", l_i_status);
#endif
            break;
        }

        /* Switch the CTM to front connector */
        if(l_i_status = OPSWGPIOWrite(37, 1))
        {
#ifdef DEBUG
            printf("Write to GPIO 37 failed. Returned:%d\n", l_i_status);
#endif
            break;
        }
        if(l_i_status = OPSWGPIOWrite(3, 0))
        {
#ifdef DEBUG
            printf("Write to GPIO 3 failed. Returned:%d\n", l_i_status);
#endif
            break;
        }

        if(l_i_status = power_off_sfp())
        {
#ifdef DEBUG
            printf("Turn off the SFP failed. Returned:%d\n", l_i_status);
#endif
            break;
        }

        if(l_i_status = set_bits(-PWR_OTDR-SFP_TX_DIS, I2C_IO_EXPANDER1)) // Turn on the OTDR and negate Tx-Dis
        {
#ifdef DEBUG
            printf("Turn on the OTDR and negate Tx-Dis failed. Returned:%d\n", l_i_status);
#endif
            break;
        }
        sleep(3);

        /**Check the Hw Rev file*/
        if( access(HARDWARE_REVISION_1_1, F_OK ) == 0 )
        {   if(l_i_status = set_bits(0xff, I2C_IO_EXPANDER2)) // Set IO_Expander2 value to the 0xff
            {
#ifdef DEBUG
                printf("IOExpander2 failed to initialize:%d\n", l_i_status);
#endif
                break;
            }
        }
    }
    while(0);

    return l_i_status;
}

/*******************************************************************************************************
 * @brief This function powers off OTDR.
 *
 * @return  On success returns 0 and error code otherwise.
*******************************************************************************************************/
int power_off_otdr(bool flag)
{   int l_i_status = -1;

    if (flag)
    {   l_i_status = set_bits(PWR_OTDR,I2C_IO_EXPANDER1); // Turn off the OTDR
    }
    else
    {   do
        {   if (l_i_status = OtdrInterfaceInit(OTDR_NORMAL_MODE))
            {   break;
            }

            if (l_i_status = OTDRPower(OFF))
            {   break;
            }

            if (l_i_status = OtdrInterfaceDeInit())
            {   break;
            }
        }
        while(0);
    }

    if(l_i_status)
    {
#ifdef DEBUG
        printf("Turn off the OTDR failed. Returned:%d\n", l_i_status);
#endif
    }
    else
    {
#ifdef DEBUG
        printf("Turned off the OTDR. Returned:%d\n", l_i_status);
#endif
    }

    return l_i_status;
}

/*******************************************************************************************************
 * @brief This function powers off SFP.
 *
 * @return  On success returns 0 and error code otherwise.
*******************************************************************************************************/
int power_off_sfp()
{   int l_i_status = -1;

    if(l_i_status = set_bits(PWR_SFP,I2C_IO_EXPANDER1)) // Turn off the SFP
    {
#ifdef DEBUG
        printf("Turn off the SFP failed. Returned:%d\n", l_i_status);
#endif
    }
    else
    {
#ifdef DEBUG
        printf("Turned off the SFP. Returned:%d\n", l_i_status);
#endif
    }

    return l_i_status;
}


/*******************************************************************************************************
 * @brief This function powers off Instruments.
 *
 * @return  On success returns 0 and error code otherwise.
*******************************************************************************************************/
int power_off_all_Ins()
{   int l_i_status = -1;

    if(l_i_status = set_bits(PWR_ALL,I2C_IO_EXPANDER1)) // Turn off all Instrument
    {
#ifdef DEBUG
        printf("Turn off all instruments failed. Returned:%d\n", l_i_status);
#endif
    }
    else
    {
#ifdef DEBUG
        printf("Turned off all instruments Returned:%d\n", l_i_status);
#endif
    }

    return l_i_status;
}

