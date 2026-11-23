/**************************************************************************************************
 *  @file 	freescale_interface.h
 *
 *  @brief 	Freescale Interface header file
 *
 *  This file contains Freescale Interface APIs Declarations.
 *
**************************************************************************************************/

#ifndef FREESCALE_INTERFACE_H
#define FREESCALE_INTERFACE_H

/* standard header */
#include <stdio.h>
#include <stdbool.h>

#define SUCCESS 0

/** Freescale I2C Slave Communication*/
#define FSL_I2C_IN_PROGRESS_ERROR           2200
#define FSL_I2C_CRC_MISMATCH_ERROR          2201
#define FSL_I2C_NACK_ERROR                  2202
#define FSL_I2C_BUS_ERROR                   2203
#define FSL_I2C_TIMEOUT_ERROR               2204
#define FSL_I2C_BUSY_SKIPPED_ERROR          2205
#define FSL_I2C_UNEXPECTED_ERROR            2206

#define FSL_RST_SUCCESS 0
#define FSL_RST_ERROR 	1

/**
 *
 * GPIO RESET ERROR for ResetPowerContolLogic
 */
#define FSL_GPIO_INIT_ERROR             1850
#define FSL_GPIO_SET_DIR_ERROR          1851
#define FSL_GPIO_SET_VAL_ERROR          1852


/**************************************************************************************************
 *
 * @brief This function checks Freescale I2C communication errors
 *
 * @param i_puc_responseBuf     input response buffer
 *
 * @return On success, returns SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int FslErrChk(unsigned char *i_puc_responseBuf);


/**
 * @brief This function toggle reset pin of the Freescale processor
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int ResetPowerContolLogic(void);
#endif
