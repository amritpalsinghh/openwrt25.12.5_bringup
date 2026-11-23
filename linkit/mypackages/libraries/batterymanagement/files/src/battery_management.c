/**************************************************************************************************
 *  @file 	battery_management.c
 *  @brief 	Battery Management API Library
 *
 *  This file contains Battery Management APIs.
 *
**************************************************************************************************/

/* Battery Management interface header */
#include "battery_management.h"
#include "freescale_interface.h"
#include "logMgr.h"
#include "i2c_uC_comm.h"
#include <time.h>
#include <errno.h>
#include <xmfh_err.h>

/* Battery Management I2C init node */
mraa_i2c_context g_p_battery_mgmt_init = NULL;

/* Command sequence ID */
uint8_t g_ui8_sequence_id = 0;
float g_f_prev_percen = 0;

/** Logger File Handler */
int g_i_lgrBatMode = 2;
FILE *g_px_batLgrFileptr = NULL;
uint8_t g_ui8_percen_flag = 0;
struct timespec g_x_prev_ts = {0};

/** Logger File Path */
#define BATTERY_LOG_FILE_PATH   "/var/log/BATTERY.log"
#define RETRY_CNT               5
#define FSL_I2C_SLAVE_ADDRESS   0x21
#define PSEUDO_SLAVE_ADDRESS    0x66

static int g_i_lgrMode = 2;

/**************************************************************************************************
 * @brief   This function initializes bus and context
 * 	    initialize the global i2c context as a structure.
 *	    If the bus cannot be initialized then initialize the context to NULL
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int BatteryAPIInit(void)
{   int l_i_status = BATTERYAPI_SUCCESS;

    /*Setting up log file*/
    setLogOutput(LOG_OUTPUT_FILE);
    if (!g_px_batLgrFileptr)
        setFileLogInfo(&g_px_batLgrFileptr, BATTERY_LOG_FILE_PATH, NULL);
    setLogLevel(LOG_LEVEL_INFO);
    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Entering into %s]", g_ui8_sequence_id,__func__);

    do
    {   /** BatteryAPI Init**/
        g_p_battery_mgmt_init = mraa_i2c_init(I2C_BUS);
        if (!g_p_battery_mgmt_init)
        {   l_i_status = BATTERYAPI_INIT_ERROR;
            break;
        }

        /*Run the device at 100kHz */
        l_i_status = mraa_i2c_frequency(g_p_battery_mgmt_init, MRAA_I2C_STD);
        if (l_i_status != BATTERYAPI_SUCCESS)
            l_i_status = BATTERYAPI_FREQUENCY_ERROR;

        /*Set i2c device address*/
        l_i_status = mraa_i2c_address(g_p_battery_mgmt_init, FSL_I2C_SLAVE_ADDRESS);
        if (l_i_status != BATTERYAPI_SUCCESS)
        {   LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode,"BATTERY: [seqid:<%u> write address set failed]",g_ui8_sequence_id);
            l_i_status = BATTERYAPI_SET_ADDR_ERROR;
            break;
        }

        LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Exiting function: %s: Return Value: %d]", g_ui8_sequence_id,__func__,l_i_status);

    }
    while(0);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function deinitialize the global i2c context
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int BatteryAPIDeInit(void)
{   int l_i_status = BATTERYAPI_SUCCESS;

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Entering into %s]", g_ui8_sequence_id, __func__);

    /** BatteryAPI Deinit*/
    if (g_p_battery_mgmt_init != NULL)
    {   l_i_status = mraa_i2c_stop(g_p_battery_mgmt_init);

        if (l_i_status != BATTERYAPI_SUCCESS)
            l_i_status = BATTERYAPI_DEINIT_ERROR;
    }
    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Exiting function: %s: Return Value: %d]", g_ui8_sequence_id, __func__, l_i_status);

    /*Closing log file*/
    if (g_px_batLgrFileptr)
        logMgrClose(&g_px_batLgrFileptr);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function get charger temperature in C
 *
 * @param   o_f_temp returns charger temperature
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryChargerTemp(float *o_f_temp)
{   int l_i_status = BATTERYAPI_SUCCESS;
    uint16_t l_ui16_data = 0;
    uint8_t l_ui8_deviceAddress = FSL_I2C_SLAVE_ADDRESS;
    unsigned char dataRead[16] = { 0 };
    unsigned char requestFrame[4] = { 0 };

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Entering into %s]", g_ui8_sequence_id,__func__);

    do
    {   /* Battery API init check */
        if(!g_p_battery_mgmt_init)
        {   l_i_status = BATTERYAPI_CHECK_INIT_ERROR;
            break;
        }

        /*Set i2c device address*/
        l_i_status = mraa_i2c_address(g_p_battery_mgmt_init, FSL_I2C_SLAVE_ADDRESS);
        if (l_i_status != BATTERYAPI_SUCCESS)
        {   LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode,"BATTERY: [seqid:<%u> write address set failed]",g_ui8_sequence_id);
            l_i_status = BATTERYAPI_SET_ADDR_ERROR;
            break;
        }

        /** Getting Data from I2C */
        requestFrame[0] = PSEUDO_SLAVE_ADDRESS;
        requestFrame[1] = 0x55;
        requestFrame[2] = 0x14;
        requestFrame[3] = 0x10;

        l_i_status = device_read(g_p_battery_mgmt_init, requestFrame, 4,dataRead, &g_ui8_sequence_id);
        if(!l_i_status)
        {   l_ui16_data = (uint16_t)(dataRead[1]<<8 | dataRead[0]);
            *o_f_temp = (l_ui16_data*0.1f) - 273.15f;
        }
        else
        {   if(l_i_status == DEVICE_I2C_NACK_ERROR)
            {   l_i_status = BATTERYAPI_NACK_ERROR;
                LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
                break;
            }
            LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
            l_i_status = BATTERYAPI_READ_ERROR;
        }
    }
    while(0);

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Exiting function: %s: Temp:%f Return Value: %d]",g_ui8_sequence_id, __func__, *o_f_temp, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function get battery voltage in Volts
 *
 * @param   o_f_voltage returns battery voltage
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryVoltage(float *o_f_voltage)
{   int l_i_status = BATTERYAPI_SUCCESS;
    uint16_t l_ui16_data = 0;
    uint8_t l_ui8_deviceAddress = FSL_I2C_SLAVE_ADDRESS;
    unsigned char dataRead[16] = { 0 };
    unsigned char requestFrame[4] = { 0 };

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Entering into %s]", g_ui8_sequence_id,__func__);

    do
    {   /* Battery API init check */
        if(!g_p_battery_mgmt_init)
        {   l_i_status = BATTERYAPI_CHECK_INIT_ERROR;
            break;
        }

        /*Set i2c device address*/
        l_i_status = mraa_i2c_address(g_p_battery_mgmt_init, FSL_I2C_SLAVE_ADDRESS);
        if (l_i_status != BATTERYAPI_SUCCESS)
        {   LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode,"BATTERY: [seqid:<%u> write address set failed]",g_ui8_sequence_id);
            l_i_status = BATTERYAPI_SET_ADDR_ERROR;
            break;
        }

        /** Getting Data from I2C */
        requestFrame[0] = PSEUDO_SLAVE_ADDRESS;
        requestFrame[1] = 0x55;
        requestFrame[2] = 0x04;
        requestFrame[3] = 0x10;

        l_i_status = device_read(g_p_battery_mgmt_init, requestFrame, 4,dataRead, &g_ui8_sequence_id);
        if(!l_i_status)
        {   l_ui16_data = (uint16_t)(dataRead[1]<<8 | dataRead[0]);
            *o_f_voltage = (l_ui16_data*0.001f);
        }
        else
        {   if(l_i_status == DEVICE_I2C_NACK_ERROR)
            {   l_i_status = BATTERYAPI_NACK_ERROR;
                LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
                break;
            }
            LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
            l_i_status = BATTERYAPI_READ_ERROR;
        }
    }
    while(0);

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Exiting function: %s: Current:%f Return Value: %d]",g_ui8_sequence_id, __func__, *o_f_voltage, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function get battery current in A
 *
 * @param   o_f_current returns battery current
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryCurrent(float *o_f_current)
{   int l_i_status = BATTERYAPI_SUCCESS;
    uint16_t l_ui16_data = 0;
    uint8_t l_ui8_deviceAddress = FSL_I2C_SLAVE_ADDRESS;
    uint8_t l_ui8_registerAddress = 0x0C;
    unsigned char dataRead[2] = { 0 };
    unsigned char requestFrame[4] = { 0 };

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Entering into %s]",g_ui8_sequence_id, __func__);

    do
    {   /* Battery API init check */
        if(!g_p_battery_mgmt_init)
        {   l_i_status = BATTERYAPI_CHECK_INIT_ERROR;
            break;
        }

        /*Set i2c device address*/
        l_i_status = mraa_i2c_address(g_p_battery_mgmt_init, FSL_I2C_SLAVE_ADDRESS);
        if (l_i_status != BATTERYAPI_SUCCESS)
        {   LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode,"BATTERY: [seqid:<%u> write address set failed]",g_ui8_sequence_id);
            l_i_status = BATTERYAPI_SET_ADDR_ERROR;
            break;
        }

        /** Getting Data from I2C */
        requestFrame[0] = 0x55;
        requestFrame[1] = l_ui8_registerAddress;
        requestFrame[2] = 0x02;

        l_i_status = device_read(g_p_battery_mgmt_init, requestFrame, 3,dataRead, &g_ui8_sequence_id);
        if(!l_i_status)
        {   l_ui16_data = (uint16_t)(dataRead[1]<<8 | dataRead[0]);
            *o_f_current = (int16_t)l_ui16_data * 0.001f;
        }
        else
        {   if(l_i_status == DEVICE_I2C_NACK_ERROR)
            {   l_i_status = BATTERYAPI_NACK_ERROR;
                LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
                break;
            }
            LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
            l_i_status = BATTERYAPI_READ_ERROR;
        }
    }
    while(0);

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Exiting function: %s: Current:%f Return Value: %d]", g_ui8_sequence_id,__func__, *o_f_current, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function get the predicted remaining battery capacity as a percentage of FullChargeCapacity
 *
 * @param   o_f_chargePercentage returns battery charge percentage
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryChargePercentage(float *o_f_chargePercentage)
{   int l_i_status = BATTERYAPI_SUCCESS;
    uint16_t l_ui16_data = 0;
    uint8_t l_ui8_deviceAddress = FSL_I2C_SLAVE_ADDRESS;
    uint8_t l_ui8_counter = 0;
    uint8_t l_ui8_pkt_err_cnt = 0;
    struct timespec l_x_ts = {0};
    double l_d_ts = 0;
    unsigned char dataRead[16] = { 0 };
    unsigned char requestFrame[4] = { 0 };

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Entering into %s]", g_ui8_sequence_id,__func__);

    do
    {   /** Taking Latest time stamp */
        clock_gettime(CLOCK_MONOTONIC, &l_x_ts);

        /** Getting time duration between last request and latest request*/
        l_d_ts = (l_x_ts.tv_sec - g_x_prev_ts.tv_sec)+((double)(l_x_ts.tv_nsec - g_x_prev_ts.tv_nsec)/1e9);
        if (g_x_prev_ts.tv_sec != 0)
        {   LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Last Request came before %g S]", g_ui8_sequence_id,l_d_ts);
        }

        /* Battery API init check */
        if(!g_p_battery_mgmt_init)
        {   l_i_status = BATTERYAPI_CHECK_INIT_ERROR;
            break;
        }

        /*Set i2c device address*/
        l_i_status = mraa_i2c_address(g_p_battery_mgmt_init, FSL_I2C_SLAVE_ADDRESS);
        if (l_i_status != BATTERYAPI_SUCCESS)
        {   LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode,"BATTERY: [seqid:<%u> write address set failed]",g_ui8_sequence_id);
            l_i_status = BATTERYAPI_SET_ADDR_ERROR;
            break;
        }

        /** Getting Data from I2C */
        requestFrame[0] = PSEUDO_SLAVE_ADDRESS;
        requestFrame[1] = 0x55;
        requestFrame[2] = 0x16;
        requestFrame[3] = 0x10;


        l_i_status = device_read(g_p_battery_mgmt_init, requestFrame, 4,dataRead, &g_ui8_sequence_id);
        if(!l_i_status)
        {   l_ui16_data = (uint16_t)(dataRead[1]<<8 | dataRead[0]);
            *o_f_chargePercentage = (float)l_ui16_data;
        }
        else
        {   if(l_i_status == DEVICE_I2C_NACK_ERROR)
            {   l_i_status = BATTERYAPI_NACK_ERROR;
                LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
                break;
            }
            LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
            l_i_status = BATTERYAPI_READ_ERROR;
            break;
        }

        if(((*o_f_chargePercentage < 0 || *o_f_chargePercentage > 100) /** percentage beyond '0-100' */ ||
                ((*o_f_chargePercentage - g_f_prev_percen > 5) && ((l_d_ts < 600) && (g_x_prev_ts.tv_sec != 0))) ||
                ((g_f_prev_percen - *o_f_chargePercentage > 5) && ((l_d_ts < 600) && (g_x_prev_ts.tv_sec != 0)))) /** percentage diff more than 5 and time duration of last req. is less than 10 min than retry */
          )

        {   l_i_status = BATTERYAPI_INVALID_VALUE_ERROR;
            LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> ChargePercentage:%f Prev Percent:%f]",g_ui8_sequence_id, *o_f_chargePercentage, g_f_prev_percen);
        }
        else
        {   LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> ChargePercentage:%f Prev Percent:%f]",g_ui8_sequence_id, *o_f_chargePercentage, g_f_prev_percen);
            g_f_prev_percen = *o_f_chargePercentage;
        }
    }
    while(0);

    /** Updating previous time stamp*/
    clock_gettime(CLOCK_MONOTONIC, &g_x_prev_ts);

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Exiting function: %s Return Value: %d]",g_ui8_sequence_id, __func__, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function get desired charging voltage in Volts
 *
 * @param   o_f_chargerVoltage returns battery charging voltage
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryChargerVoltage(float *o_f_chargerVoltage)
{   int l_i_status = BATTERYAPI_SUCCESS;
    uint16_t l_ui16_data = 0;
    uint8_t l_ui8_tx_buf[2] = {0};
    float l_f_Vref = 2.048;
    g_ui8_sequence_id++;

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Entering into %s]", g_ui8_sequence_id,__func__);

    do
    {   /* Battery API init check */
        if(!g_p_battery_mgmt_init)
        {   l_i_status = BATTERYAPI_CHECK_INIT_ERROR;
            break;
        }

        /** Set the slave Address*/
        l_i_status = mraa_i2c_address(g_p_battery_mgmt_init, 0x37);
        if (l_i_status != BATTERYAPI_SUCCESS)
        {   l_i_status = BATTERYAPI_SET_ADDR_ERROR;
            break;
        }

        /** Setting External Reference Voltage */
        l_ui8_tx_buf[0] = 0x0b;
        l_ui8_tx_buf[1] = 0x01;
        l_i_status = mraa_i2c_write(g_p_battery_mgmt_init, l_ui8_tx_buf, 2);
        if(l_i_status != BATTERYAPI_SUCCESS && errno == 6)
        {   l_i_status = BATTERYAPI_NACK_ERROR;
            break;
        }
        else if(l_i_status != BATTERYAPI_SUCCESS)
        {   l_i_status =  BATTERYAPI_WRITE_ERROR;
            break;
        }
        /** Setting Continuous Conversion Mode */
        l_ui8_tx_buf[0] = 0x07;
        l_ui8_tx_buf[1] = 0x01;
        l_i_status = mraa_i2c_write(g_p_battery_mgmt_init, l_ui8_tx_buf, 2);
        if(l_i_status != BATTERYAPI_SUCCESS && errno == 6)
        {   l_i_status = BATTERYAPI_NACK_ERROR;
            break;
        }
        else if(l_i_status != BATTERYAPI_SUCCESS)
        {   l_i_status =  BATTERYAPI_WRITE_ERROR;
            break;
        }

        /** Setting Start bit of the Config Reg */
        l_ui8_tx_buf[0] = 0x00;
        l_ui8_tx_buf[1] = 0x01;
        l_i_status = mraa_i2c_write(g_p_battery_mgmt_init, l_ui8_tx_buf, 2);
        if(l_i_status != BATTERYAPI_SUCCESS && errno == 6)
        {   l_i_status = BATTERYAPI_NACK_ERROR;
            break;
        }
        else if(l_i_status != BATTERYAPI_SUCCESS)
        {   l_i_status =  BATTERYAPI_WRITE_ERROR;
            break;
        }

        usleep(100000);
        /* Read the register */
        l_i_status = mraa_i2c_read_bytes_data(g_p_battery_mgmt_init, 0x24, (uint8_t*)&l_ui16_data, 2);
        if(l_i_status == -1 && errno == 6)
        {   l_i_status = BATTERYAPI_NACK_ERROR;
            break;
        }
        else if(l_i_status == -1)
        {   l_i_status =  BATTERYAPI_READ_ERROR;
            break;
        }

        else
        {   l_ui16_data=(l_ui16_data>>8)+(l_ui16_data&0xFF)*256;
            *o_f_chargerVoltage = 4095.f-l_ui16_data/16.f;
            *o_f_chargerVoltage = (l_f_Vref-(*o_f_chargerVoltage*(l_f_Vref/4096)))/(1.f/11);
        }
        l_i_status = BATTERYAPI_SUCCESS;
    }
    while(0);

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Exiting function: %s: ChargerVoltage:%f Return Value: %d]",g_ui8_sequence_id, __func__, *o_f_chargerVoltage, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function get battery charge cycle
 *
 * @param   o_ui_chrgcycle returns battery charge cycle count
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryChargeCycle(uint16_t *o_ui_chrgcycle)
{   int l_i_status = BATTERYAPI_SUCCESS;
    uint16_t l_ui16_data = 0;
    uint8_t l_ui8_deviceAddress = FSL_I2C_SLAVE_ADDRESS;
    unsigned char dataRead[16] = { 0 };
    unsigned char requestFrame[4] = { 0 };

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Entering into %s]", g_ui8_sequence_id,__func__);

    do
    {   /* Battery API init check */
        if(!g_p_battery_mgmt_init)
        {   l_i_status = BATTERYAPI_CHECK_INIT_ERROR;
            break;
        }

        /*Set i2c device address*/
        l_i_status = mraa_i2c_address(g_p_battery_mgmt_init, FSL_I2C_SLAVE_ADDRESS);
        if (l_i_status != BATTERYAPI_SUCCESS)
        {   LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode,"BATTERY: [seqid:<%u> write address set failed]",g_ui8_sequence_id);
            l_i_status = BATTERYAPI_SET_ADDR_ERROR;
            break;
        }

        /** Getting Data from I2C */
        requestFrame[0] = PSEUDO_SLAVE_ADDRESS;
        requestFrame[1] = 0x55;
        requestFrame[2] = 0x15;
        requestFrame[3] = 0x10;

        l_i_status = device_read(g_p_battery_mgmt_init, requestFrame, 4,dataRead, &g_ui8_sequence_id);
        if(!l_i_status)
        {   *o_ui_chrgcycle = (uint16_t)(dataRead[1]<<8 | dataRead[0]);
        }
        else
        {   if(l_i_status == DEVICE_I2C_NACK_ERROR)
            {   l_i_status = BATTERYAPI_NACK_ERROR;
                LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
                break;
            }
            LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
            l_i_status = BATTERYAPI_READ_ERROR;
        }
    }
    while(0);

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Exiting function: %s: charge cycle:%hu Return Value: %d]",g_ui8_sequence_id, __func__, *o_ui_chrgcycle, l_i_status);

    return l_i_status;
}


/**************************************************************************************************
    * @brief   This function get battery deep sleep configuration
    * @param   o_px_deepSleepConfig returns battery deep sleep configuration
    * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
    *
    *************************************************************************************************   
 */
int GetBatteryDeepSleepConfig(T_DEEP_SLEEP_CONFIG *o_px_deepSleepConfig)
{   int l_i_status = BATTERYAPI_SUCCESS;
    unsigned char dataRead[16] = { 0 };
    unsigned char requestFrame[4] = { 0 };

    LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> Entering into %s]", g_ui8_sequence_id,__func__);

    do
    {   /* Battery API init check */
        if(!g_p_battery_mgmt_init)
        {   l_i_status = BATTERYAPI_CHECK_INIT_ERROR;
            break;
        }
        /* Check for NULL pointer */
        if(o_px_deepSleepConfig == NULL)
        {   l_i_status = BATTERYAPI_INVALID_INPUT;
            break;
        }

        /*Set i2c device address*/
        l_i_status = mraa_i2c_address(g_p_battery_mgmt_init, FSL_I2C_SLAVE_ADDRESS);
        if (l_i_status != BATTERYAPI_SUCCESS)
        {   LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode,"BATTERY: [seqid:<%u> write address set failed]",g_ui8_sequence_id);
            l_i_status = BATTERYAPI_SET_ADDR_ERROR;
            break;
        }

        /** Getting Data from I2C */
        requestFrame[0] = PSEUDO_SLAVE_ADDRESS;
        requestFrame[1] = 0xE0;
        requestFrame[2] = 0x00;
        requestFrame[3] = 0x04;

        l_i_status = device_read(g_p_battery_mgmt_init,
                                requestFrame,
                                4,
                                dataRead,
                                &g_ui8_sequence_id);
        if(!l_i_status)
        {  
            o_px_deepSleepConfig->mode = dataRead[0];
            o_px_deepSleepConfig->days = dataRead[1];
            o_px_deepSleepConfig->percentage = dataRead[2];
        }
        else
        {  
             if(l_i_status == DEVICE_I2C_NACK_ERROR)
            {   l_i_status = BATTERYAPI_NACK_ERROR;
                LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
                break;
            }
            LOG_INFO(&g_px_batLgrFileptr, NULL, g_i_lgrBatMode, "BATTERY: [seqid:<%u> %s : device read failed ERROR : %d]", g_ui8_sequence_id,__func__, l_i_status);
            l_i_status = BATTERYAPI_READ_ERROR;
        }               
    }
    while(0);   
    return l_i_status;
}


/**************************************************************************************************
    * @brief   This function set battery deep sleep configuration
    * @param   i_px_deepSleepConfig set battery deep sleep configuration
    * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
    *
    *************************************************************************************************   
 */
int SetBatteryDeepSleepConfig(T_DEEP_SLEEP_CONFIG *i_px_deepSleepConfig)
{
    int l_i_status = BATTERYAPI_SUCCESS;
    /* Snapshot config once to reduce repeated dereferences and branching. */
    unsigned char l_mode = 0;
    unsigned char l_days = 0;
    unsigned char l_percentage = 0;
    unsigned char requestFrame[8] = {0};
    unsigned char dataRead[2] = {0};    
    /**Check for initialization */
    if (!g_p_battery_mgmt_init)
        return BATTERYAPI_CHECK_INIT_ERROR;
    /**Check for NULL pointer */
    if (i_px_deepSleepConfig == NULL)
        return BATTERYAPI_INVALID_INPUT;

    if(i_px_deepSleepConfig->mode == DEEP_SLEEP_ENABLED)
    {
        l_mode = (unsigned char)i_px_deepSleepConfig->mode;
        l_days = i_px_deepSleepConfig->days;
        l_percentage = i_px_deepSleepConfig->percentage;

        if (l_days < BATTERYAPI_DAYS_MIN_THRESHOLD || 
            l_days > BATTERYAPI_DAYS_MAX_THRESHOLD)
                return BATTERYAPI_INVALID_INPUT;

        if (l_percentage < BATTERYAPI_PERCENTAGE_MIN_THRESHOLD || 
            l_percentage > BATTERYAPI_PERCENTAGE_MAX_THRESHOLD)
                return BATTERYAPI_INVALID_INPUT;
    }
    else if(i_px_deepSleepConfig->mode == DEEP_SLEEP_DISABLED)
    {
        l_mode = (unsigned char)i_px_deepSleepConfig->mode;
        l_days = BATTERYAPI_DAYS_DEFAULT_THRESHOLD;
        l_percentage = BATTERYAPI_PERCENTAGE_DEFAULT_THRESHOLD;
    }
    else
    {
        return BATTERYAPI_INVALID_INPUT;
    }

    l_i_status = mraa_i2c_address(g_p_battery_mgmt_init,
                                FSL_I2C_SLAVE_ADDRESS);
    if (l_i_status != BATTERYAPI_SUCCESS)
        return BATTERYAPI_SET_ADDR_ERROR;

    /*
    byte 1: Pseudo Slave Address
    byte 2: Command 
    byte 3: Sub-command 
    byte 4: Mode (0x00 for disable, 0x01 for enable deep sleep)
    byte 5: days  
    byte 6: Percentage 
    byte 7: Read length 
    */
    requestFrame[0] = PSEUDO_SLAVE_ADDRESS;
    requestFrame[1] = 0xE0;
    requestFrame[2] = 0x01;
    requestFrame[3] = l_mode;
    requestFrame[4] = l_days;    
    requestFrame[5] = l_percentage;
    requestFrame[6] = 0x01;   /* read length */

    l_i_status = device_read(g_p_battery_mgmt_init,
                            requestFrame,
                            7,
                            dataRead,
                            &g_ui8_sequence_id);

    if (l_i_status != BATTERYAPI_SUCCESS)
    {
        if (l_i_status == DEVICE_I2C_NACK_ERROR)
            return BATTERYAPI_NACK_ERROR;

        return BATTERYAPI_WRITE_ERROR;
    }
   
    return BATTERYAPI_SUCCESS;
}
/**************************************************EXTERNAL API SECTION END**********************************************/

