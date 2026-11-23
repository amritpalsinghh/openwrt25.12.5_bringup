/**************************************************************************************************
 *  @file       sfp_fw_upgrade.h
 *  @brief      SFP Firmware Upgrade API
 *
 *  The file contains SFP Firmware Upgrade API Declaration.
 *
**************************************************************************************************/

#ifndef SFP_FW_UPGRADE_H
#define SFP_FW_UPGRADE_H

/* Standard Header */
#include <stdio.h>
#include <stdint.h>

/* Response Codes */
#define SFP_SUCCESS               0
#define SFP_FW_INIT_ERROR         1630
#define SFP_FW_DEINIT_ERROR       1631
#define SFP_FW_BOOT_MODE_ERROR    1632
#define SFP_FW_BAUD_RATE_ERROR    1633
#define SFP_FW_SEND_FILE_ERROR    1634
#define SFP_FW_PRE_SETTING_ERROR  1635
#define SFP_FW_POST_SETTING_ERROR 1636

/**************************************************************************************************
 * @brief The function Upgrades the SFP Firmware.
 *
 * @param  i_ui8_fileName SFP firmware file name, which is expected at /tmp/ location.
 *         (Note: Remote firmware upgrade procedure places files in /tmp/ directory of XMF-H unit,
 *                which can be utilized here to initiate further procedure using this API.)
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int SFPFWUpgrade(uint8_t *i_ui8_fileName);
#endif
