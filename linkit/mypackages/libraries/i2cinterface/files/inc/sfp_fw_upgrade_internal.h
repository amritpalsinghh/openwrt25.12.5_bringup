/**************************************************************************************************
 *  @file       sfp_fw_upgrade_internal.h
 *  @brief      SFP Firmware API Application
 *
 *  The file contains SFP Firmware Upgrade API Declaration.
 *
**************************************************************************************************/

/* Standard Header */
#include <stdio.h>

/* mraa header */
#include "mraa/i2c.h"

#ifndef SFP_FW_UPGRADE_INTERNAL_H
#define SFP_FW_UPGRADE_INTERNAL_H

/* SFP I2C init node */
extern mraa_i2c_context g_p_sfp_init;

//UART Linux device handler
extern int sfp_powercomm_handle;

#endif
