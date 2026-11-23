/**************************************************************************************************
 *  @file 	power_management.h
 *  @brief 	Power Management API declarations
 *
 *  This header file contains Power Management APIs declarations related to OTDR, CTM and SFP.
 *
 **************************************************************************************************/

#ifndef POWER_MANAGEMENT_H
#define POWER_MANAGEMENT_H

/************************** Power Management Response Codes **************************/
#define POWERMGT_SUCCESS                        0
#define POWERMGT_INVALID_INPUT                  2001
#define POWERMGT_DEVICE_COMMUNICATION_ERROR     2002
#define POWERMGT_DEVICE_READ_TIMEOUT            2003

/** Device Status */
#define POWERMGT_DEVICE_ON                      2007
#define POWERMGT_DEVICE_OFF                     2008
#define POWERMGT_DEVICE_POWERING_UP             2009
#define POWERMGT_DEVICE_POWERING_OFF            2010
#define FREESCALE_FW_UPGRADE_INPROGRESS		2011
#define POWERMGT_DEVICE_FAILED_TO_INITIALIZE    2012

typedef enum
{   OFF = 0, ON
} E_POWERSWITCH;

/*******************************************PUBLIC API********************************************/

/**************************************************************************************************
 *
 * @brief   This function get OTDR status
 *
 * @return  On success, returns device status, error code otherwise
 *
 **************************************************************************************************/
int getOTDRStatus();

/**************************************************************************************************
 *
 * @brief   This function POWER-ON/OFF OTDR as per the input
 *
 * @param   i_e_switch, enum to the power status (ON/OFF)
 *
 * @return  On success, returns POWERMGT_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int OTDRPower(E_POWERSWITCH i_e_switch);

/**************************************************************************************************
 *
 * @brief   This function get OCM status
 *
 * @return  On success, returns device status, error code otherwise
 *
 **************************************************************************************************/
int getOCMStatus(void);

/**************************************************************************************************
 *
 * @brief   This function POWER-ON/OFF CTM as per the input
 *
 * @param   i_e_switch, enum to the power status (ON/OFF)
 *
 * @return  On success, returns POWERMGT_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int OCMPower(E_POWERSWITCH i_e_switch);
/**************************************************************************************************
 *
 * @brief   This function get SFP status
 *
 * @return  On success, returns device status, error code otherwise
 *
 **************************************************************************************************/
int getSFPStatus(void);

/**************************************************************************************************
 *
 * @brief   This function POWER-ON/OFF SFP as per the input.
 *
 * @param   i_e_switch, enum to the power status (ON/OFF)
 *
 * @return  On success, returns POWERMGT_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPPower(E_POWERSWITCH i_e_switch);
#endif
