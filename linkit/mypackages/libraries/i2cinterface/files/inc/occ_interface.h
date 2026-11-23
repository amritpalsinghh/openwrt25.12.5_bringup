/**
 *  @file 	occ_interface.h
 *  @brief 	OCC API Library
 *
 *  This header file contains OCC command APIs declarations
 *
 */

#ifndef OCC_H
#define OCC_H

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>

/* mraa header */
#include "mraa/i2c.h"

/* sensor address */
#define ADC_ADDRESS    0x37
#define ADG_ADDRESS    0x49
#define EEPROM_ADDRESS 0x56

/** Errors */
#define ADC_INIT_ERROR     						1501
#define ADC_SET_ADDR_ERROR 						1502
#define ADG_INIT_ERROR     						1503
#define ADG_SET_ADDR_ERROR 						1504
#define OCC_PD_OUTOF_RANGE_ERROR 				1505
#define OCC_NULL_POINTER						1506
#define OCC_READ_ERROR							1507
#define OCC_WRITE_ERROR							1508
#define EEPROM_INIT_ERROR     					1509
#define EEPROM_SET_ADDR_ERROR 					1510
#define OCC_FREQUENCY_ERROR						1511
#define OCC_NACK_ERROR						    1514


enum
{   OCC_CAL_ERROR=1512,
    OCC_CAL_FACTOR_ERROR
} E_OCCCALERROR;


/* Success fail error */
#define OCC_SUCCESS 0
#define OCC_ERROR   1

/* Debug flag*/
#define OCC_DEBUG   0

/* Register */
#define ADC_CONFIG          0x00
#define ADC_INTERRUPT       0x01
#define ADC_INTERRUPT_MASK  0x03
#define ADC_CONV_RATE       0x07
#define ADC_CH_DISABLE      0x08
#define ADC_ONE_SHOT        0x09
#define ADC_DEEP_SHUTDOWN   0x0a
#define ADC_ADV_CONFIG      0x0b
#define ADC_STATUS          0x0c
#define ADC_MANU_ID         0x3e
#define ADC_REV_ID          0x3f

/**
 * PD Numbers
 */

typedef enum { PD_1577=0, PD_1550, PD_1270, PD_4, PD_NONE } E_PDNUM;


/**
 * @brief This function gets the Max Current value for passed pd
 *
 * @param i_i_pdNum          Pd Number
 * @param o_f_pwrVal     	 returns Max Power value for passed pd number
 * @return On success, returns EXIT_SUCCESS, error code otherwise
*/
int OCCReadPWR(int i_i_pdNum, float *o_f_pwrVal);

/**
 * @brief This function Initialize  OCC
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int OCCInterfaceInit();

/**
 * @brief This function DeInitialize  OCC
 * @return no returns
 */
void OCCInterfaceDeInit();

#endif
