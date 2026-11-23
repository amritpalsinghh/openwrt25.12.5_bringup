/**
 *  @file 	occ_interface_internal.h
 *  @brief 	OCC internal API Declarations.
 *
 *  This header file contains internal OCC API declaration
 *
 */

#ifndef OCC_INTERNAL_H
#define OCC_INTERNAL_H

typedef struct _power
{   float f_pwrVal;
    float f_current;
} T_POWER;

/**
 * @brief This function gets the Max Current value for passed pd
 *
 * @param i_i_pdNum          Pd Number
 * @param o_s_power     	 returns power and current value as a structure pointer
 * @return On success, returns 0, error code otherwise
*/
int OCCReadCurrPWR(int i_i_pdNum, T_POWER *o_s_power);
#endif
