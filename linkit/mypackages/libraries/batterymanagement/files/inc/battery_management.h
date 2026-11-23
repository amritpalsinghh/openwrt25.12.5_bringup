/**************************************************************************************************
 *  @file 	battery_management.h
 *  @brief 	Battery Management API Library
 *
 *  This file contains Battery Management APIs.
 *
**************************************************************************************************/

#ifndef BATTERY_MANAGEMENT_H
#define BATTERY_MANAGEMENT_H

/* standard header */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

/* mraa header */
#include "mraa/i2c.h"
#include "calc_crc8.h"
/* i2c bus */
#define I2C_BUS 0

/* Response Codes */
#define BATTERYAPI_SUCCESS                  0
#define BATTERYAPI_ERROR                    1
#define BATTERYAPI_INIT_ERROR               1701
#define BATTERYAPI_DEINIT_ERROR             1702
#define BATTERYAPI_SET_ADDR_ERROR           1703
#define BATTERYAPI_INVALID_INPUT            1704
#define BATTERYAPI_INVALID_DEV_ADDR         1705
#define BATTERYAPI_INVALID_REG_ADDR         1706
#define BATTERYAPI_FREQUENCY_ERROR          1707
#define BATTERYAPI_CHECK_INIT_ERROR         1708
#define BATTERYAPI_READ_ERROR               1709
#define BATTERYAPI_WRITE_ERROR              1710
#define BATTERYAPI_WRONG_BYTES              1711
#define BATTERYAPI_BAD_CRC                  1712
#define BATTERYAPI_I2C_TIMEOUT_ERROR        1713
#define BATTERYAPI_SEQID_MISMATCH           1714
#define BATTERYAPI_I2C_STATUS_ERROR         1715
#define BATTERYAPI_PACKET_ERROR             1716
#define BATTERYAPI_INVALID_VALUE_ERROR      1717
#define BATTERYAPI_NACK_ERROR               1718
#define BATTERYAPI_SET_CONFIG_ERROR         1719

#define BATTERYAPI_DAYS_MIN_THRESHOLD (1)
#define BATTERYAPI_DAYS_MAX_THRESHOLD (30)
#define BATTERYAPI_DAYS_DEFAULT_THRESHOLD (14)
#define BATTERYAPI_PERCENTAGE_MIN_THRESHOLD (0)
#define BATTERYAPI_PERCENTAGE_MAX_THRESHOLD (80)
#define BATTERYAPI_PERCENTAGE_DEFAULT_THRESHOLD (20)


typedef enum {DEEP_SLEEP_DISABLED=0, DEEP_SLEEP_ENABLED=1 } T_DEEP_SLEEP_MODE;
typedef struct _deep_sleep_config
{
    T_DEEP_SLEEP_MODE mode;     /* 0=Disable, 1=Enable */
    uint8_t days;               /* Number of days to enter deep sleep mode */
    uint8_t percentage;         /* Battery percentage threshold to enter deep sleep mode*/
} T_DEEP_SLEEP_CONFIG;

/**************************************************************************************************
 * @brief   This function initializes bus and context
 * 	    initialize the global i2c context as a structure.
 *	    If the bus cannot be initialized then initialize the context to NULL
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int BatteryAPIInit(void);

/**************************************************************************************************
 *
 * @brief   This function deinitialize the global i2c context
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int BatteryAPIDeInit(void);



/**************************************************************************************************
 *
 * @brief   This function get charger temperature in C
 *
 * @param   o_f_temp returns charger temperature
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryChargerTemp(float *o_f_temp);

/**************************************************************************************************
 *
 * @brief   This function get battery voltage in Volts
 *
 * @param   o_f_voltage returns battery voltage
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryVoltage(float *o_f_voltage);

/**************************************************************************************************
 *
 * @brief   This function get battery current in A
 *
 * @param   o_f_current returns battery current
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryCurrent(float *o_f_current);

/**************************************************************************************************
 *
 * @brief   This function get the predicted remaining battery capacity as a percentage of FullChargeCapacity
 *
 * @param   o_f_chargePercentage returns battery charge percentage
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryChargePercentage(float *o_f_chargePercentage);

/**************************************************************************************************
 *
 * @brief   This function get desired charging voltage in Volts
 *
 * @param   o_f_chargerVoltage returns battery charging voltage
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryChargerVoltage(float *o_f_chargerVoltage);

/**************************************************************************************************
 *
 * @brief   This function get battery charge cycle
 *
 * @param   o_ui_chrgcycle returns battery charge cycle count
 *
 * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetBatteryChargeCycle(uint16_t *o_ui_chrgcycle);

/**************************************************************************************************
    * @brief   This function get battery deep sleep configuration
    * @param   o_px_deepSleepConfig returns battery deep sleep configuration
    * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
    *
    *************************************************************************************************   
 */
int GetBatteryDeepSleepConfig(T_DEEP_SLEEP_CONFIG *o_px_deepSleepConfig);

/**************************************************************************************************
    * @brief   This function set battery deep sleep configuration(Mode, Days, Percentage)
    * @param   i_px_deepSleepConfig set battery deep sleep configuration
    * @return  On success, returns BATTERYAPI_SUCCESS, error code otherwise
    *
    *************************************************************************************************   
 */
int SetBatteryDeepSleepConfig(T_DEEP_SLEEP_CONFIG *i_px_deepSleepConfig);
#endif

