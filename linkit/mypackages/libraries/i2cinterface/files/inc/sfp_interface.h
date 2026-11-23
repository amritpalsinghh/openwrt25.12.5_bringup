/**************************************************************************************************
 *  @file 	sfp_interface.h
 *  @brief 	SFP Interface API Library
 *
 *  This file contains SFP command APIs implementations.
 *
**************************************************************************************************/

#ifndef SFP_INTERFACE_H
#define SFP_INTERFACE_H

/* standard header */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>
#include "power_management.h"

/* mraa header */
#include "mraa/i2c.h"

/* i2c bus */
#define I2C_BUS 0

#define SFP_ADDRESS_A0 0xA0
#define SFP_ADDRESS_A2 0xA2
#define SFP_ADDRESS_A4 0xA4

/* Response Codes */
#define SFP_SUCCESS                   0
#define SFP_INIT_ERROR                1601
#define SFP_DEINIT_ERROR              1602
#define SFP_SET_ADDR_ERROR            1603
#define SFP_INVALID_INPUT             1604
#define SFP_INVALID_DEV_ADDR          1605
#define SFP_INVALID_REG_ADDR          1606
#define SFP_FREQUENCY_ERROR           1607
#define SFP_INVALID_PAGE_LENGTH       1608
#define SFP_CHECK_INIT_ERROR          1609
#define SFP_READ_ERROR                1610
#define SFP_WRITE_ERROR               1611
#define SFP_MEMORY_ALLOCATION_FAILED  1612
#define SFP_READ_TIMEOUT              1613
#define SFP_NOT_INSERTED              1614
#define SFP_POWERED_OFF               1615
#define SFP_NACK_ERROR                1616
#define SFP_TONE_INIT_ERROR           1617

/**
* SFP Tone Generator Modes
*/
typedef enum {SFP_STEADY_OFF=0, SFP_STEADY_ON, SFP_MODE, SFP_TONE_270HZ, SFP_INVALID_MODE } SFP_TONEMODE;

/**************************************************************************************************
 * @brief   This function initializes bus and context
 *
 * @param   initialize the global i2c context as a structure.
 *          If the bus cannot be initialized then initialize the context to NULL
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPInterfaceInit();

/**************************************************************************************************
 *
 * @brief   This function deinitialize the global i2c context
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPInterfaceDeInit();

/**************************************************************************************************
 *
 * @brief   This function read a single byte from designated register
 *
 * @param   i_ui8_deviceAddress input device address
 * @param   i_ui8_registerAddress input register address
 * @param   o_ui8_data returns a single byte
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadByte(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint8_t* o_ui8_data);

/**************************************************************************************************
 *
 * @brief   This function read a single word from designated register
 *
 * @param   i_ui8_deviceAddress input device address
 * @param   i_ui8_registerAddress input register address
 * @param   o_ui16_data returns single word
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadWord(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint16_t* o_ui16_data);

/**************************************************************************************************
 *
 * @brief   This function write a single byte to designated register
 *
 * @param   i_ui8_deviceAddress input device address
 * @param   i_ui8_registerAddress input register address
 * @param   i_ui8_data input single byte
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPWriteByte(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint8_t i_ui8_data);

/**************************************************************************************************
 *
 * @brief   This function write a single word to designated register
 *
 * @param   i_ui8_deviceAddress input device address
 * @param   i_ui8_registerAddress input register address
 * @param   i_ui16_data input single word
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPWriteWord(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint16_t i_ui16_data);

/**************************************************************************************************
 *
 * @brief This function read a single page from designated register
 *
 * @param i_ui8_deviceAddress input device address
 * @param i_ui8_registerAddress input register address
 * @param i_ui16_n input length of bytes (max limit is 63)
 * @param o_ui8_data returns bytes of given length
 *
 * @return On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPPageRead(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint16_t i_ui16_n, uint8_t *o_ui8_data);

/**************************************************************************************************
 *
 * @brief This function write a single page to designated register
 *
 * @param i_ui8_deviceAddress input device address
 * @param i_ui8_registerAddress input register address
 * @param i_ui16_n input length of bytes (max limit is 8)
 * @param i_ui8_data input bytes of given length
 *
 * @return On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPPageWrite(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint16_t i_ui16_n, const uint8_t* i_ui8_data);

/**************************************************************************************************
 *
 * @brief   This function read a mode ABS
 *
 * @param   o_b_mode returns mode value
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadModABS(bool *o_b_mode);

/**************************************************************************************************
 *
 * @brief   This function reads the Tx_Fault SFP IO in and retruns true if the SFP asserts a TX_FAULT
 *
 * @param   o_b_fault returns mode value
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadTxFault(bool *o_b_fault);

/**************************************************************************************************
 *
 * @brief   This function reads the LOS SFP IO in and retruns true if the SFP asserts a LOS (Loss of signal)
 *
 * @param   o_b_los returns mode value
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadLOS(bool *o_b_los);

/**************************************************************************************************
 *
 * @brief   reads the Tx_Diasable SFP IO in and retruns true if the SFP transmitter is disabled
 *
 * @param   o_b_tx_disable returns Tx_Disable value
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadTxDisable(bool *o_b_tx_disable);

/**************************************************************************************************
 *
 * @brief   set or clear the SFP Tx_Disable pin thus turning the SFP Tx laser off or on
 *
 * @param   i_b_status Value: pass 1 to set TxDisable thus disable Tx laser; pass 0 to clear TxDisable thus enable Tx laser.
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPSetTxDisable(bool i_b_status);

/**************************************************************************************************
 *
 * @brief   This function read a PCF8575
 *
 * @param   o_ui8_data    returns byte
 * @param   i_ui8_dataLen Read Buffer lenght
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int ReadPCF8575(uint8_t* o_ui8_data, uint8_t i_ui8_dataLen);

/**************************************************************************************************
 *
 * @brief   This function sets the SFP tone generator mode.
 *
 * @param   mode indicates the tone generator mode to enable. SFP_MODE returns the SFP to the default operation mode.
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPSetToneGeneratorMode(SFP_TONEMODE mode);

/**************************************************************************************************
 *
 * @brief   This function read the current SFP tone generator mode.
 *
 * @param   mode will be updated with a enum value indicating the current tone generator mode. SFP_MODE indicates sfp default operation mode.
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPGetToneGeneratorMode(SFP_TONEMODE *mode);

#endif
