/**
*  @file   ocm_interface.h
*  @brief  OCM API Library
*
*  This header file contains OCM set/get command API declaration
*
*/

#ifndef OCM_INTERFACE_H
#define OCM_INTERFACE_H

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include "power_management.h"

/************************** Ocm Errors **************************/
#define OCM_COMMAND_SUCCESS                   0   // Indicate the request succeeded

#define OCM_NO_RESPONCE_RECEIVED           1234
#define OCM_IC_COMMAND_ERROR               1235
#define OCM_UC_COMMAND_ERROR               1236
#define OCM_WRONG_FIRST_BYTE               1237
#define OCM_NOT_READY_ERROR                1238
#define OCM_RANGE_ERROR                    1239
#define OCM_HARDWARE_ERROR                 1240
#define OCM_INTIALIZATION_ERROR            1241
#define OCM_RESERVED_BYTE_ERROR            1242
#define OCM_UNSUSED_POSITION_ERROR         1243
#define OCM_TOF_DATA_INVALID_ERROR         1244
#define OCM_TOF_HW_ERROR                   1245
#define OCM_TOF_HIGHTEMP_LIMIT_EXCEED      1246
#define OCM_TOF_LOWTEMP_LIMIT_EXCEED       1247
#define OCM_TOF_OPTICAL_OVERLOAD           1248
#define OCM_TOF_NOT_READY                  1249
#define OCM_COMMUNICATION_ERROR            1250
#define OCM_READ_TIMEOUT                   1251
#define OCM_NULL_POINTER                   1252
#define OCM_INIT_ERROR                     1253    //Failure Codes for OcmInterfaceInit()
#define OCM_DEINIT_ERROR                   1254    //Failure Codes for OcmInterfaceDeInit()
#define OCM_CHANNEL_PLAN_INVALID           1255    // Attempt to set invalid channel plan configuration.
#define OCM_INSERTION_LOSS_INVALID         1256    // Attempt to set insertion loss correction to an invalid value.
#define OCM_MINIMUM_PEAK_POWER_INVALID     1257    // Attempt to set minimum peak power to an invalid value.
#define OCM_PEAK_DETECT_THRESHOLD_INVALID  1258    // Attempt to set peak detect threshold to an invalid value.
#define OCM_FW_UPGRADE_FILE_NOT_FOUND      1259    // OCM firmware file not found
#define OCM_MEMORY_ALLOCATION_FAILED	   1260	   // OCM Memory allocation failed
#define OCM_INVALID_INPUT                  1261	   // OCM Invalid input
#define OCM_INIT_PROGRESS                  1262    // OCM initialization is in progress
#define OCM_JSON_PARSE_ERROR               1263    // Failed to parse product info json file
#define OCM_POWERED_OFF                    1264
#define OCM_INVALID_FREQ_VAL               1265
#define OCM_INTERNAL_STATUS_ERROR          1266


/************************** Channel Plan Spacing **************************/

#define CSSP_SPACING_50_GHZ         (0)
#define CSSP_SPACING_100_GHZ_PLAN_A (1)
#define CSSP_SPACING_100_GHZ_PLAN_B (2)
#define CSSP_SPACING_INVALID        (0xFFFFFFF0)

/********************** CENB1 TOF Scan Return Codes ***********************/
#define CENB_TOF_SCANNING_ENABLED  (7)
#define CENB_TOF_SCANNING_DISABLED (1)
#define CENB_TOF_NOT_PRESENT       (0)

/**
 * OCM API Types
 */

typedef enum
{   SLICE_12_5_GHZ=0,
    SLICE_6_25_GHZ,
    SLICE_3_125_GHZ
} T_SLICE_WIDTH;

typedef enum
{   SPECTRUM_FORMAT_MW=0,
    SPECTRUM_FORMAT_DBM
} T_SPECTRUM_FORMAT;

typedef enum
{   EDGE_NONE=0,       // <X> not present
    EDGE_BOS,          // <X> 1 : Additional 4x12.5 or 8x6.25 GHz slices at BOS
    EDGE_EOS,          // <X> 2 : Additional 4x12.5 or 8x6.25 GHz slices at EOS
    EDGE_BOS_EOS,      // <X> 3 : Additional 4x12.5 or 8x6.25 GHz slices at BOS and EOS
    EDGE_BOS_EOS_1,    // <X> 4 : Additional 1x12.5 or 2x6.25 GHz slices at BOS and 1x6.25 GHz slices EOS
    EDGE_BOS_EOS_2     // <X> 45: Additional 1x12.5 or 1x6.25 GHz slices at BOS and 2x6.25 GHz slices EOS
} T_EDGE_BINS;

typedef enum
{   DISABLE_TOF_SCAN=0,
    ENABLE_TOF_SCAN=1,
    RETURN_TOF_SCAN=2
} T_TOFSCAN_MODE;

typedef enum
{   INTEGRATE_2_NM = 0,
    INTEGRATE_4_NM,
    INTEGRATE_8_NM
} T_INTEGRATE;

/**
 * CBDI Control Board Data Information Structure
 */

typedef struct cbdi
{   uint32_t m_ui32_CTMModelNumber;
    int32_t m_i32_PCBSerialNumber;
    uint32_t m_ui32_CTMRevision;
    uint32_t m_ui32_CTMFirmwareVersion;
    int32_t m_ui32_CTMDataCode;
    int32_t m_ui32_tofSerialNumber;
    uint32_t m_ui32_tofRevision;
    float m_f_tofStartFrequency;
    float m_f_tofEndFrequency;
    float m_f_tofInsertionLoss;
    float m_f_tofPeakDetectionThresold;
    float m_f_tofAppliedSBOC;
    float m_f_tofAppliedSBTC;
    float m_f_tofMinimumPower;
    float m_f_tofReserved1;
    float m_f_reserved1;
    float m_f_reserved2;
    uint8_t m_aui8_CTMVendor[128];
    uint8_t m_aui8_CTMSerialNumber[128];
    uint16_t m_ui16_crc16;
} T_CBDI;

typedef struct cssp
{   uint32_t m_ui32_ctmChannalPlan;
    float m_f_ctm_startingFreq;
} T_CSSP;

typedef struct csta0
{   float m_f_deviceTemperature;
    uint32_t m_ui32_statusWord;
    uint32_t m_ui32_ChkErrorStatus;
} T_CSTA0;

typedef struct cgsp1
{   float    m_totalPower;   // total power in mW
    int 	 m_slices;       // number of slices returned
    float    m_startFreq;    // frequency or the first slice
    float    m_sliceWidth;   // width of each slice
    uint32_t m_scanCounter;  // scan counter if the edge bin parameter was used. 0 otherwise.
    uint32_t m_ui32_ChkErrorStatus; // Error Status Word
    float   *m_slicePower;   // power value at each slice in mW or dBm.
} T_CGSP1;

typedef struct
{   uint16_t channelPowerITU;  // reported in Q8 dBm format
    uint16_t channelPowerPeak; // reported in Q8 dBm format
    uint16_t channelFrequency; // reported in Q8 dBm format
} T_CFCR_CHANNEL;

typedef struct cfcr
{   float          m_totalPower;              // total power in mW
    u_int32_t      m_peakDetectionFlags[3];
    u_int32_t      m_peakTypeFlags[3];
    uint32_t       m_ui32_ChkErrorStatus;
    int            m_channels;                // Number of channels returned
    T_CFCR_CHANNEL m_channel[96];
} T_CFCR;

typedef struct
{   u_int16_t channelPower4nm;  // reported in Q8 dBm format
    u_int16_t channelPower2nm;  // reported in Q8 dBm format
    float     channelFrequency; // reported in float format THz
} T_CDDF_CHANNEL;

typedef struct cddfb
{   float          m_totalPower;              // total power in mW
    u_int32_t      m_peakDetectionFlags[3];
    u_int32_t      m_peakTypeFlags[3];
    u_int32_t      m_channelSaturationFlags[3];
    uint32_t       m_ui32_ChkErrorStatus;
    int            m_channels;                // Number of channels returned
    T_CDDF_CHANNEL m_channel[96];
} T_CDDFB;

typedef struct
{   float     channelPower;     // reported in mW
    float     channelFrequency; // reported in float format THz
} T_CGET1_CHANNEL;

typedef struct cget1
{   u_int32_t       m_peakDetectionFlags[3];
    uint32_t       m_ui32_ChkErrorStatus;
    int             m_channels;                // Number of channels returned
    T_CGET1_CHANNEL m_channel[96];
} T_CGET1;

typedef struct
{   float     channelPower;     // reported in mW
    float     channelFrequency; // reported in float format THz
} T_CGTP1_CHANNEL;

typedef struct cgtp1
{   float          m_totalPower;              // total power in mW
    u_int32_t      m_peakDetectionFlags[3];
    u_int32_t      m_channelSaturationFlags[3];
    uint32_t       m_ui32_ChkErrorStatus;
    int            m_channels;                // Number of channels returned
    T_CGTP1_CHANNEL m_channel[96];
} T_CGTP1B;

typedef struct cdgt1
{   int        m_frequency;
    float      m_power[1024];
} T_CDGT1;

/**
 * @brief  This function open serial port and initialize UART parameters (BaudRate,parity,stop-bit)
 *         If the node is already open, this function returns OCM_COMMAND_SUCCESS and is essentially a NOOP.
 *
 *@return  On success returns OCM_COMMAND_SUCCESS, error code otherwise.
 *
 */

int OcmInterfaceInit(void);

/**
 *
 *  @brief   This function remove semaphore and close UART serial node
 *           If the node is not open, this function returns OCM_COMMAND_SUCCESS and is essentially a NOOP.
 *
 *  @return  On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 *
 */

int OcmInterfaceDeInit(void);

/**
 * @brief This function gets the development revision
 *
 * @param revision return development revision as a null-terminated string.
 *        The maximum length of the string will be 128 characters.
 *
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetDREV(char *revision);

/**
 * @brief This function gets the firmware revision as a string in the form ‘XX.XX’
 *
 * @param fwvr return the firmware revision as a null-terminated string.
 *
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetFWVR(char *fwvr);

/**
 * @brief This function get the hardware revision as a string in the form ‘XX.XX’
 *
 * @param hwvr return the hardware revision as a null-terminated string.
 *
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetHWVR(char *hwvr);

/**
 * @brief This function gets the CTM serial number information
 *
 * @param idCode return the II-VI Photonics identifier code as a null-terminated string.
 * @param manufactureDate return the manufacture date as a null-terminated string in the the form of MMDDYYY.
 * @param buildNumber return the OCM build number as an integer N such that 1 >= N >= 999.
 *
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetSNUM(char *idCode, char *manufactureDate, int *buildNumber);

/**
 * @brief This function gets channel spacing setting
 *
 * @param channelPlan return channel spacing setting as an unsigned integer
 *        legal values are {CSSP_SPACING_50_GHZ, CSSP_SPACING_100_GHZ_PLAN_A, CSSP_SPACING_100_GHZ_PLAN_B, CSSP_SPACING_INVALID}
 * @param startingFreq return the CTM starting frequency as a float
 * @param o_ui32_StatusWord returns the 32-bit Status word value
 *
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */

int OcmGetCSSP(uint32_t *channalPlan, float *startingFreq, uint32_t *o_ui32_StatusWord);

/**
 * @brief This function sets the channel spacing setting
 *
 * @param channelPlan on input contains the desired channel spacing setting as an unsigned integer.
 *        legal values on input are {CSSP_SPACING_50_GHZ, CSSP_SPACING_100_GHZ_PLAN_A, CSSP_SPACING_100_GHZ_PLAN_B}
 *        upon return channelPlan contains the channel spacing setting.
 * @param startingFreq return the CTM starting frequency as a float
 * @param o_ui32_StatusWord returns the 32-bit Status word value
 *
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.  Attempting to set an invalid
 *         channel spacing configuration returns OCM_CHANNEL_PLAN_INVALID and channelPlan and startingFreq
 *         are not modified.
 */

int OcmSetCSSP(uint32_t *channalPlan, float *startingFreq, uint32_t *o_ui32_StatusWord);

/**
 * @brief This function gets/sets the TOF Scan Enable Setting
 *
 * @param canMode defines whether to enable, disable or report the TOF Scan setting. Value is as defined by T_TOFSCAN_MODE.
 * @param scanSetting return the current setting as a uint32_t. Legal return values will be one of
 *        {CENB_TOF_SCANNING_ENABLED, CENB_TOF_SCANNING_DISABLED, CENB_TOF_NOT_PRESENT }
 * @param o_ui32_StatusWord returns the 32-bit Status word value
 *
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 * @response
 *		scanMode: 1 – enable scanning
 *		scanMode: 0 – disable scanning
 *		scanSetting response:   7 – TOD Enabled
 *								1 – TOD Disabled
 */

int OcmDoCENB1(T_TOFSCAN_MODE scanMode, uint32_t *scanSetting, uint32_t *o_ui32_StatusWord);

/**
 * @brief This function gets the control status information
 *
 * @param ctmStatus return control status information as an unsigned 32-bit integer.
 *
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 * @response 32-bit OCM/TOD status word, matching the status word returned in the command response headers for packetized commands.
 */

int OcmGetCSTS0(uint32_t *ctmStatus);

/**
 * @brief This function gets control status information
 *
 * @param csta0 return control status information as a struct csta1
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetCSTA0(T_CSTA0 *csta0);

/**
 * @brief This function gets the total power in band for TOF1
 *
 * @param totPower return total power as a float normalized to 1.0 mW
 * @param o_ui32_StatusWord returns the 32-bit Status word value
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 * @response_format :mW, for dBm use "10*log10(totPower)"
 */
int OcmGetCTPR1(float *totPower, uint32_t *o_ui32_StatusWord);
/**
 * @brief This function gets the board record information
 *
 * @param cbdi return board record information as a pointer to a T_CBDI structure.
 *
 * The m_ui16_crc16 member of T_CBDI is validated by OcmGetCBDI().
 *
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */

int OcmGetCBDI(T_CBDI *cbdi);

/**
 * @brief This function gets the optical spectrum in 12.5 GHz or 6.25 GHz slices.
 * The slices are reported as a float (scaled mW or dBm). The internal Q8 format for
 * reporting as dBm is converted to a float.
 *
 * @param sliceWidth specifies whether to get 12.5 GHz, 6.25 GHz or 3.125 GHz slices.
 * @param spectrumFormat specifies whether to return the slice power values in mW or dBm. Both formats returned as a float.
 * @param edgeBins specifies whether to report bins outside the standard channel.
 * @param cgsp1 return the spectrum data returned as type T_CGSP1. The m_slicePower member in T_CGSP1 structure is
 *              dynamically allocated by the library and must be freed by the caller.
 *
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetCGSP1(T_SLICE_WIDTH sliceWidth, T_SPECTRUM_FORMAT spectrumFormat, T_EDGE_BINS edgeBins, T_CGSP1 *cgsp1);

/**
 * @brief This function gets the full channel report. Total power in mW. Channel powers in Q8 dBm.
 * @param cfcr return the full channel report as type T_CFCR.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 * @response_formats
 *		Total Pow:  total power, mW
 *		m_channels: number of channels
 *		m_Peak_Detection_Flags: channel peak detection flag (0 no peak detected / 1 peak detected)
 *		m_peakTypeFlags:  channel data format flags (0 – 10 Gbps / 1 – 40+ Gbps)
 *		channelPowerITU:  channel power integrated around channel center frequency, Q8 format
 *		channelPowerPeak: channel power integrated around channel peak frequency, Q8 format
 *		channelFrequency:  channel frequency: scaled THz  (FreqTHz = 190+channelFrequency*0.0001)
 */
int OcmGetCFCR(T_CFCR *cfcr);

/**
 * @brief This function gets the control dual data format
 * @param cddfb return the full control dual data as type T_CDDFB.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 * @response_format :
 * 		freq: 		<float>
 *		pwr2nm :	<uint16_t> to be convert into Q8
 *		pwr4nm :	<uint16_t> to be convert into Q8
 *		PeakDetct : <uint8_t>
 *		peakTyp :	<uint8_t>
 *		SatFlg : 	<SatFlg>
 */
int OcmGetCDDFB(T_CDDFB *cddfb);

/**
 * @brief This function gets channel powers integrated over 0.2 nm, 0.4 nm or 0.8 nm.
 *
 * @param integration specified the integration vaslue as type T_INTEGRATION.
 * @param cget1 return the optical spectrum as type T_CGET1.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetCGET1(T_INTEGRATE integration, T_CGET1 *cget1);

/**
 * @brief This function gets the optical spectrum including the total optical power for TOF 1
 *        integrated over 0.2 nm, 0.4 nm or 0.8 nm.
 *
 * @param integration specified the integration vaslue as type T_INTEGRATION.
 * @param cgtp1b return the optical spectrum as type T_CGTP1B.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 * @response_formats
 *		Total Pow:  total power, mW
 * 		Freq:  channel frequency: THz
 * 		pow:  channel power, scaled mW (10*mW)
 *		peak: channel peak detection flag (0 no peak detected / 1 peak detected)
 *		sat:  channel saturation flag (0 not saturated / 1 saturated)
 */
int OcmGetCGTP1B(T_INTEGRATE integration, T_CGTP1B *cgtp1b);

/**
 * @brief This function initiates a soft reset of the CTM
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetCRST(void);

/**
 * @brief This function enables/disables CTM Demo mode
 *
 * @param enableDemoMode sets demo mode if true; sets normal operating mode if false.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetCDMO(bool enableDemoMode);

/**
 * @brief This function sets the front-end gain to HIGH or LOW for TOF1
 *
 * @param highGain sets GNHI1 if true; sets GNLO1 if false.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetGain(bool highGain);

/**
 * @brief This function sets minimum peak power value.
 *
 * @param minimumPeakPower sets the value as a float in linear units (mW).
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetCMPP1(float minimumPeakPower);

/**
 * @brief This function sets insertion loss correction value.
 *
 * @param insertionLoss sets the value as a float in linear units (mW).
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetCSIL1(float inserttionLoss);

/**
 * @brief This function sets peak detection threshold value.
 *
 * @param peakDetectThreshold sets the value as a float in linear units (mW).
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetCSPT1(float peakDetectThreshold);

/**
* @brief The function Upgrades the OCM Firmware.
*
* @param  i_ui8_fileName OCM Firmware file name, which is expected at /tmp/ location.
*
* @return On success returns OCM_COMMAND_SUCCESS and error code otherwise.
*/
int OcmFirmwareUpgrade(uint8_t *i_ui8_fileName);

/**
 * @brief This function gets response of CDGT1
 *
 * @param integration specified the integration vaslue as type T_CDGT1.
 * @param cdgt1 return the optical spectrum as type T_CDGT1.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetCDGT1(T_CDGT1 *cdgt1);

/**
 * @brief This function gets response of CDGT1
 * @param i_f_sbtcVal input SBTC Calibration data.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetSSTC1(float i_f_sbtcVal);

/**
 * @brief This function gets response of CDGT1
 * @param i_f_sbocVal input SBOC Calibration data.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetSSBC1(float i_f_sbocVal);
#endif
