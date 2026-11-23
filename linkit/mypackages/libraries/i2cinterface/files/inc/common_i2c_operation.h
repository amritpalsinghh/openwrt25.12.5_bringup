/**************************************************************************************************
 *  @file   common_i2c_operation.h
 *  @brief  common i2c operation API Library
 *
 *  This file contains common i2c operation APIs implementations.
 *
**************************************************************************************************/

#ifndef COMMON_I2C_OPERATION_H
#define COMMON_I2C_OPERATION_H

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

/** Return value or Error code */
#define DIAG_SUCCESS            0
#define DIAG_FAIL               1
#define DIAG_INIT_ERROR         1751
#define DIAG_SET_ADDR_ERROR     1752
#define DIAG_FREQUENCY_ERROR    1753
#define DIAG_READ_ERROR         1754
#define DIAG_WRITE_ERROR        1755
#define DIAG_READ_WORD_ERROR    1756
#define DIAG_I2C_NACK_ERROR     1757

/* Diagnostics Parameters */
/*  Channel array contains values as mentioned below:
	Channel[0]: TIA OUT
    Channel[1]: Ref Voltage
	Channel[2]: Current_PL
	Channel[3]: +3v3
	Channel[4]: VAdaptor
	Channel[5]: Pack_Load
	Channel[6]: +5VUSB
	Channel[7]: 12V_CTM
*/
typedef struct _diag
{   uint8_t conf_reg;		// Configuration Register
    uint8_t mfg_id;			// Manufacturing ID
    uint8_t rev_id;			// Revision ID
    uint8_t conv_rate;		// Conversion Rate
    uint8_t ch_disable_reg;	// Channel Disable
    uint8_t status_reg;		// Status Register
    float temp;				// Temperature
    float channel[8];		// channels

} T_DIAG;

/* Elements in following structures considers characters as per taken array size.
Any additional string will be truncated */
typedef struct _eeprom_info
{   unsigned char m_uc_hardwareRivision[5];
    unsigned char m_uc_serialnumber[17];
    unsigned char m_uc_partNumber[17];
} T_EEPROMIFO;


/**************************************************************************************************
 *
 * @brief   This function reads diagnostics data
 *
 * @param   diag returns diagnostics data as a structure T_DIAG
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int GetDiagnosticsData(T_DIAG *diag);

/***************************************************************************************************
 * @brief  Enable / disable retry and configure timeout in the instances where MCU read operation indicates
 *         In Progress status.
 *
 * @param  config pass true or false to enable/disable Inprogress timeout
 * @param  InProgressTimeout value of timeout for Inprogress error from i2c
 ***************************************************************************************************/
void configure_mcu_inprogress_timeout(bool config, unsigned int InProgressTimeout);

/***************************************************************************************************
 * @brief  Configure Read timeout in the instances where MCU read operation indicates
 *         In Progress status.
 *
 * @param  readTimeout  value of timeout for overall read timeout
 ***************************************************************************************************/
void configure_mcu_read_timeout(unsigned int readTimeout);


/**************************************************************************************************
 * @brief This function reads hardware revision information from EEPROM
 *
 * @param Get EEPROM information as a structure (T_EEPROMIFO) containing string elements.
 *
 * @return On success, returns 0, error code otherwise
 *
 **************************************************************************************************/
int getEEPROMHardRev(uint8_t *o_c_harRev);

/**************************************************************************************************
 * @brief This function reads APD Vendor information from EEPROM
 *
 * @param Get EEPROM information as a structure (T_EEPROMIFO) containing string elements.
 *
 * @return On success, returns 0, error code otherwise
 *
 **************************************************************************************************/
int getEEPROMApdVendor(uint8_t *o_c_ApdVen);

/**************************************************************************************************
 * @brief This function reads data from EEPROM
 *
 * @param Get EEPROM information as a structure (T_EEPROMIFO) containing string elements.
 *
 * @return On success, returns 0, error code otherwise
 *
 **************************************************************************************************/
int getEEPROMInfo(T_EEPROMIFO *o_x_eeprominfo);

/*******************************************************************************************************
 * @brief This function to toggle GPIO for front connector.
 *
 * @param i_i_gpio A GPIO number
 *
 * @param i_i_value A GPIO value
 *
 * @return  On success returns 0 and error code otherwise.
*******************************************************************************************************/
int OPSWGPIOWrite(int i_i_gpio, int i_i_value);

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
int EEPROMPageRead(uint8_t i_ui8_registerAddress, uint16_t i_ui16_n, uint8_t *o_ui8_data);

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
int EEPROMPageWrite(uint8_t i_ui8_registerAddress, uint16_t i_ui16_n, uint8_t *i_ui8_data);

#endif
