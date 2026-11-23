/**
*  @file   osa_calibration.h
*  @brief  OSA Calibration API
*
*  This header file contains API to check status and perform OSA Calibration.
*
*/

#ifndef OSA_CALIBRATION_H
#define OSA_CALIBRATION_H

#include <stdio.h>
#include <stdbool.h>

#define DAC_INIT_ERROR      1781
#define DAC_SET_ADDR_ERROR  1782
#define DAC_FREQUENCY_ERROR 1783
#define DAC_READ_ERROR      1784
#define DAC_WRITE_ERROR     1785

#define CAL_NOT_POSSIBLE        2300
#define CAL_MFG_BL_NOT_FOUND    2301
#define CAL_NOTCHFLTR_CAL_FAILED    2302

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
int  osaCalibration(bool i_b_commit,float* new_sboc, float* new_sbtc); 

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
int SetCTMCalibration(float sboc, float sbtc);  	

/**************************************************************************************************
 *
 * @brief   This function reset calibration data to MFG Baseline
 *          
 *
 * @return  returns 0 in case success, error code otherwise.
 *
 **************************************************************************************************/
int ResetCTMCalibrationToMFGBaseline(void);

#endif
