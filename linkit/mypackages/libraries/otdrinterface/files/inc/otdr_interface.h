/**
 *  @file 	otdr_interface.h
 *  @brief 	OTDR Interface API Library
 *
 *  	This header file contains OTDR command APIs declarations.
 *
 */

#ifndef OTDR_INTERFACE
#define OTDR_INTERFACE

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include "power_management.h"

/************************** OTDR Response Codes **************************/

/**Return value/Error code*/
#define OTDR_COMMAND_SUCCESS                   0
#define OTDR_INVALID_INPUT                  1024
#define OTDR_PROCESSING_ERROR               1025
#define OTDR_NOT_DEFINED                    1026
#define OTDR_MEMORY_ALLOCATION_FAILED       1027
#define OTDR_INIT_FAILED                    1028
#define OTDR_DEINIT_FAILED                  1029
#define OTDR_BUSY                   	    1030
#define OTDR_INPUT_OUT_OF_RANGE             1031
#define OTDR_READ_TIMEOUT                   1032
#define OTDR_COMMUNICATION_ERROR            1033
#define OTDR_NULL_POINTER                   1034
#define OTDR_COMMAND_NOT_SUPPORTED          1035
#define OTDR_COMMAND_FAIL                   1036
#define OTDR_JSON_PARSE_ERROR               1037
#define OTDR_INIT_PROGRESS                  1038
#define OTDR_POWERED_OFF                    1039
#define OTDR_POWER_CYCLE_FAILED             1040
#define OTDR_INVALID_MODE_INTITIALIZED      1041
#define OTDR_CORRUPTION_ERROR               1042 /* Dollar mode is observed. Recovery is required */
#define OTDR_FILE_NOT_SUPPORTED             1043 /* OTDR is corrupted and OHW file is not supplied */
#define OTDR_LIVE_API_ERROR                 1044
#define OTDR_LIVE_BUSY 			            1045
#define OTDR_LIVE_SCAN_DISABLED             1046
#define OTDR_LIVE_SCAN_ENABLED              1047
#define OTDR_LIVE_INVALID_DATA 	            1050
#define OTDR_UNEXPECTED_ERROR               1051

#define OTDR_ASTM_NORMAL_VALUE  			1
#define OTDR_ASTM_STICKY_VALUE  			2

#define EXTERNAL_OTDR_FILE "/tmp/external_OTDR.TXT"
/**
 * OTDR result codes
 */

typedef enum { OTDR_RESULT_OK=0, OTDR_RESULT_OK_HR, OTDR_RESULT_ABORT, OTDR_RESULT_ERROR } T_OTDRRESULTCODE;

/**
 * OTDR State Defines
 */

typedef enum {OTDR_STATE_IDLE=0, OTDR_STATE_CAPTURE, OTDR_STATE_LIVE_CAPTURE, OTDR_STATE_PROCESS, OTDR_STATE_ANALYZE, OTDR_STATE_TRANSFER, OTDR_STATE_SETUP, OTDR_STATE_UNKNOWN } T_OTDRSTATE;

/**
 * OTDR communication mode
 */

typedef enum {OTDR_NORMAL_MODE=0, OTDR_SPEED_MODE } T_OTDRCOMMMODE;
typedef enum {OTDR_BOOT_MODE=0, OTDR_ACTIVE_MODE} T_OTDREXECMODE;
typedef enum {OTDR_SOCKET_CHECK=0, OTDR_EMU_LOCK_CHECK, OTDR_EMU_STATUS_CHECK} T_OTDRINITCHECK;

/**
 * OTDR Alarms
 */

typedef enum { OTDR_ALARM_MTX=0, OTDR_ALARM_MTH, OTDR_ALARM_MTL, OTDR_ALARM_TEC1TMP, OTDR_ALARM_ALL } T_OTDRALARMS;

/**
* OTDR Tone Generator Modes
*/

typedef enum { OTDR_STEADY_OFF=0, OTDR_STEADY_ON, OTDR_MODE, OTDR_TONE_270HZ, OTDR_TONE_1KHZ, OTDR_TONE_2KHZ,OTDR_INVALID_MODE } T_TONEMODE;

/* Elements in following structures considers 32 characters. Any additional string will be truncated */
typedef struct _ver_full
{   unsigned char partnumber[32];
    unsigned char version[64];
    unsigned char serialnumber[32];
    unsigned char emuversion[32];
    unsigned char designversion[32];
    unsigned char loader[32];
    unsigned char spiPeripheral[32];
    unsigned char sysClock[32];
    unsigned char schedulePeriod[32];
    unsigned char dataCache[32];
    unsigned char instCache[32];
    unsigned char VendorName[32];
    unsigned char VendorPN[32];
    unsigned char HwRev[32];
    int platform;
} T_VER_FULL;

/* Elements in following structures considers 32 characters. Any additional string will be truncated */
typedef struct _ver
{   unsigned char partnumber[32];
    unsigned char version[64];
    unsigned char serialnumber[32];
} T_VER;

typedef struct _hardwareVer
{   unsigned char design[32];
    unsigned char loader[32];
    unsigned char spiPeripheral[32];
    unsigned char sysClock[32];
    unsigned char schedulePeriod[32];
    unsigned char dataCache[32];
    unsigned char instCache[32];
} T_HWVER;

typedef struct _otdrConfig
{   float lossThreshold;
    float reflectionThreshold;
    float measureTime;
    float pulseWidth;
    float resolution;
    float spanLength;
    float windowOffset;
    float windowSize;
    double refractiveIndex;
} T_OTDRCONFIG;

typedef struct _alrm
{   int MtxSta;
    int MtxSst;
    float MtxThr;
    float MtxHys;
    int MthSta;
    int MthSst;
    float MthThr;
    float MthHys;
    int MtlSta;
    int MtlSst;
    float MtlThr;
    float MtlHys;
    int Tec1tmpSta;
    int Tec1tmpSst;
    float Tec1tmpThr;
    float Tec1tmpHys;
    int Tec2tmpSta;
    int Tec2tmpSst;
    float Tec2tmpThr;
    float Tec2tmpHys;
} T_ALRM;

typedef struct _ast
{   unsigned int Mtx;
    unsigned int Mth;
    unsigned int Mtl;
    unsigned int Tec1tmp;
} T_AST;

typedef struct _otdrResult
{   T_OTDRRESULTCODE code;
    float hrLocation;   // high reflection location (km)
} T_OTDRRESULT;

typedef struct _otdrStatus
{   T_OTDRSTATE currentState;
    float       percentage;    // percentage of state completed.
} T_OTDRSTATUS;

typedef struct _supply
{   float voltage;
    float current;
    float power;
} T_SUPPLY;

typedef struct _otdrTraceTable
{   unsigned int points;  // number of points in trace
    float windowOffset;   // distance of first point in km
    float dx;             // distance between adjacent points in km
    float *power;         // relative power at each point in dB
} T_DUMPTRACE;

#define OTDR_EVENT_TYPE_NON_REFLECTIVE 1
#define OTDR_EVENT_TYPE_REFLECTIVE     2
#define OTDR_EVENT_TYPE_EOF            4
#define OTDR_EVENT_TYPE_EOD            8
#define OTDR_EVENT_TYPE_HIGH_REF       16

typedef struct _otdrEventEntry
{   unsigned int type; // 1 == non-reflective, 2 == reflective, 4 == end_of_fiber, 8 == endOfData, 16 == High_Ref
    float x_km;        // distance in km
    float loss_u_km;   //
    float refl_u_km;   //
    float loss_dB;     // loss magnitude in dB
    float refl_dB;     // reflection magnitude in dB
    float attn_dB_km;  // attenuation in dB/km from previous event
    float c_loss_dB;   //
} T_DUMPEVENTENTRY;

typedef struct _otdrEventTable
{   unsigned int n;      // number of events
    T_DUMPEVENTENTRY *e; // array of n events
} T_DUMPEVENT;


typedef struct _liveTraceCount
{ int liveScanCaptureCnt;
  int liveScanProcessCnt;
  int liveScanAvgProcessCnt;
  int analyzeTraceindex;
  int avgAnalyzeTraceindex;
}T_LIVETRACECOUNT;

typedef enum { LIVE_ANALYZESTA_ANALYZE = 0, LIVE_ANALYZESTA_IDLE } T_LIVEANALYZESTATE;	
typedef struct _otdrLiveAnalyzeStatus	
{	
    T_LIVEANALYZESTATE analyzestatus;	
} T_LIVEANALYZESTATUS;


//Library function

/**
 * @brief  This function opens serial port and initializes UART parameters (BaudRate,parity,stop-bit).
 * 			If the node is already open, this function returns OTDR_COMMAND_SUCCESS and is essentially a NOOP.
 *
 *@return  On success returns OTDR_COMMAND_SUCCESS, error code otherwise.
 *
 */
int OtdrInterfaceInit(T_OTDRCOMMMODE mode);

/**
 *
 *  @brief   This function removes semaphore and closes UART serial node.
 *			If the node is not open, this function returns OTDR_COMMAND_SUCCESS and is essentially a NOOP.
 *
 *@return  On success returns OTDR_COMMAND_SUCCESS, error code otherwise.
 *
 */
int OtdrInterfaceDeInit(void);

/**
 * @brief This function gets version information
 *
 * @param ver return version information as a structure containing string elements. On return, each string is a null-terminated unsigned char string; trailing spaces are removed
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetVer(T_VER *ver);

/**
 * @brief This function gets hardware version information
 *
 * @param verHw return hardware version information as a structure containing string elements. On return, each string is a null-terminated unsigned char string; trailing spaces are removed
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetHwVer(T_HWVER *verHw);

/**
 * @brief This function gets the module case temperature in °C
 *
 * @param mt return module case temperature in °C as a array of float
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetMt(float *mt);

/**
 * @brief This function gets the current OTDR configure parameters and state
 *
 * @param params Output value as a T_OTDRCONFIG structure
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetParameters(T_OTDRCONFIG *params);

/**
 * @brief This function configures event loss thresold
 *
 * @param thr Input values in dB
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetEventLossThreshold(float thr);

/**
 * @brief This function configures event refelction thresold
 * @param thr Input values in dB
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetEventReflectionThreshold(float thr);

/**
 * @brief This function configures the measurement time to x seconds
 * @param mtime input value measurement time. Valid range is [0.0,360.0].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetMeasureTime(float mtime);

/**
 * @brief This function configures pulse width to x microseconds
 * @param pw input value pulse width to x microseconds. Valid range is [0.008,100.0].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetPulseWidth(float pw);

/**
 * @brief This function configures the resolution to x meters
 *
 * @param res input value resolution. Valid range is [0.4,100.0].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetResolution(float res);

/**
 * @brief This function configures the span length to x kilometers
 *
 * @param sl input value span length. Valid range is [0.0,200.0].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetSpanLength(float sl);

/**
 * @brief This function configures the size of the measurement window to x
 kilometers
 * @param wo input value window offset. Valid range is [0.0,200.0].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetWindowOffset(float wo);

/**
 * @brief This function configures the size of the measurement window to x
 kilometers
 * @param ws input value window size. Valid range is [0.0,200.0].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetWindowSize(float ws);

/**
 * @brief This function configure refractive index
 *
 * @param index input value. Valid range is [1.0,2.0].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetRefractiveIndex(double index);

/**
 * @brief This function gets information about alarms
 *
 * @param alrm return information about alarms as a struct alrm with following values(0 for "OFF" or "d") as structure of float values)
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetALRM(T_ALRM *alrm);

/**
 * @brief This function sets threshold value for the specified alarm
 *
 * @param alarm input value specifying which alarm to set. OTDR_ALARM_ALL is invalid for this API.
 * @param threshold input value temperature in °C or NaN to disable alarm.
          Valid range of OTDR_ALARM_MTH is [20.0,80.0].
          Valid range of OTDR_ALARM_MTL is [-10.0,50.0].
          Valid range of OTDR_ALARM_MTX is [20.0,100.0].
          Valid range of OTDR_ALARM_TEC1TMP is [0.0,10.0].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetAlrmThreshold(T_OTDRALARMS alarm, float threshold);

/**
 * @brief This function sets hysteresis value for the specified alarm
 *
 * @param alarm input value specifying which alarm to set. OTDR_ALARM_ALL is invalid for this API.
 * @param hysteresis input value temperature in °C.
          Valid range of OTDR_ALARM_MTH is [0.0,10.0].
          Valid range of OTDR_ALARM_MTL is [0.0,10.0].
          Valid range of OTDR_ALARM_MTX is [0.0,10.0].
          Valid range of OTDR_ALARM_TEC1TMP is [0.0,5.0].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetAlrmHysteresis(T_OTDRALARMS alarm, float hysteresis);

/**
 * @brief This function clears the latching status for the specified alarm
 * ALRM CLR/ALRM x CLR resets the sticky status only if the current alarm status (as reported by ALRM STA) is OFF
 *
 * @param alarm input value specifying which alarm to clear. OTDR_ALARM_ALL is allowed for this API.
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrClearAlarmLatch(T_OTDRALARMS alarm);

/**
 * @brief This function reset the configuration to factory defaults. The values do not change until after rebooting or power cycling.
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrFactoryReset(void);

/**
 * @brief This function reboots the OTDR.
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrReboot(void);

/**
 * @brief This function returns the result of the last OTDR measurement.
 *
 * @param result Pointer to variable where result is stored.
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrResult(T_OTDRRESULT *result);

/**
 * @brief This function return otdr status
 *
 * @param status Output values lists current state and state completion percentage.
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrStatus(T_OTDRSTATUS *status);

/**
 * @brief This function starts a new OTDR measurement. State can be checked using OtdrStatus()
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrRun(void);

/**
 * @brief This function stops the current OTDR measurement. State can be checked using OtdrStatus()
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrStop(void);

/**
 * @brief This function re-analyzes the most recent OTDR measurement. State can be checked using OtdrStatus()
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrAnalyze(void);

/**
 * @brief This function return OTDR trace data
 *
 * @param dumpTrace output trace data as a T_DUMPTRACE struct
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 * The power array is allocated by the library and deallocated by the caller.
 */
int OtdrDumpTrace(T_DUMPTRACE *dumpTrace);

/**
 * @brief This function return OTDR trace data in linear format
 *
 * @param dumpTrace output trace data as a T_DUMPTRACE struct
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 * The power array is allocated by the library and deallocated by the caller.
 */
int OtdrDumpTraceLinear(T_DUMPTRACE *dumpTrace);

/**
 * @brief This function return OTDR event table
 *
 * @param dumpEvent output event table as a T_DUMPEVENT struct
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 * The T_DUMPEVENTENTRY array is allocated by the library and deallocated by the caller.
 */
int OtdrDumpEvent(T_DUMPEVENT *dumpEvent);

/**
 * @brief This function get the resolution to x meters
 *
 * @param pd return pd value as a array of float
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetPd(float pd[]);

/**
 * @brief This function sets real-time clock date to x (format: yyyy-mm-dd)
 * and/or time to y (format: hh:mm:ss)
 *
 * @param time input time (format: hh:mm:ss) as a string
 * @param date input date (format: yyyy-mm-dd) as a string
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetRtc(char *time, char *date);

/**
 * @brief This function gets real-time clock date to x (format: yyyy-mm-dd)
 * and/or time to y (format: hh:mm:ss)
 * @param time return time (format: hh:mm:ss) as a string
 * @param date return date (format: yyyy-mm-dd) as a string
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetRtc(char *time, char *date);

/**
 * @brief This function displays module supply voltage, current, and/or power
 *
 * @param supply return module supply voltage, current, and/or power as a struct of float values
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetSupply(T_SUPPLY *supply);

/**
 * @brief This function sets alarm status mode to normal (N) mode
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetAstmNormal(void);

/**
 * @brief This function sets alarm status mode to latching (S, for
 *	"sticky") mode
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetAstmSticky(void);

/**
 * @brief This function get status mode to normal (N) or latching (S, for
 *	"sticky") mode
 *
 * @param mode return value as integer 1 for normal (N) or 2 for latching (S, for
 *	"sticky") mode
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetAstm(int *mode);

/**
 * @brief This function lists all alarms whose status is on. If no alarms are on, the response is 0
 *
 * @param ast return 1 (for whose status is on) or 0 (for whose status is off) as a T_AST struct of unsigned integer values
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetAst(T_AST *ast);

/**
 * @brief This function gets full version information
 *
 * @param ver return version information as a structure containing string elements. On return, each string is a null-terminated unsigned char string; leading and trailing spaces are removed
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetVerFull(T_VER_FULL *ver_full);
/**
* @brief The function Upgrades the OTDR Firmware.
*
* @param  i_ui8_fileName OTDR Firmware file name, which is expected at /tmp/ location.
*
* @return On success returns OTDR_COMMAND_SUCCESS and error code otherwise. (OTDR_FILE_NOT_SUPPORTED in case
* OTDR is corrupted and other than OHW file is supplied)
*/
int OtdrFirmwareUpgrade(uint8_t *i_ui8_fileName);
/**
 * @brief This function starts otdr live diagnostic scan run.
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrLiveRun(void);
/**
 * @brief This function stops otdr live diagnostic scan run.
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrLiveStop(void);

/**
 * @brief This function analyzes otdr live diagnostic scan run.
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrLiveAnalyze(void);

/**
 * @brief This function return live OTDR event data
 *
 * @param live_event_table output event data as a T_DUMPEVENT struct
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 */
int OtdrLiveEvent(T_DUMPEVENT *live_event_table);

/**
 * @brief This function return live OTDR trace data
 *
 * @param liveDumpTrace output trace data as a T_DUMPTRACE struct
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 */
int OtdrLiveTrace(T_DUMPTRACE *liveDumpTrace);

/**
 * @brief This function return live OTDR analyze status
 *
 * @param live_otdr_state output state data as a T_LIVEANALYZESTATUS struct
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 */
int OtdrLiveAnalyzeStatus( T_LIVEANALYZESTATUS *live_analyze_status);

/**
 * @brief This function displays live average trace.
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrLiveAvgTrace(T_DUMPTRACE *liveDumpTrace);

/**
 * @brief This function shows .
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrLiveAvgAnalyze(void);

/**
 * @brief This function shows otdr live average event.
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrLiveAvgEvent(T_DUMPEVENT *live_event_table);

/**
 * @brief This function shows otdr live parameters.
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrLiveTraceCount(T_LIVETRACECOUNT *params);

/**
 * @brief This function shows index of the current trace being analyzed and along with the trace.
 * @param live_trace output trace data as a T_DUMPTRACE struct.
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrLiveAnalyzeTrace(T_DUMPTRACE *live_trace);

/**
 * @brief This function shows how many traces have been averaged along with trace.
 * @param live_avgtrace output trace data as a T_DUMPTRACE struct.
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrLiveAvgAnalyzeTrace(T_DUMPTRACE *live_avgtrace);

/**
 * @brief This function set max_ne OTDR Paramter
 *
 * @param max_ne input Parameter for OTDR
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 */
int OtdrSetMaxNE(int max_ne);

/**
 * @brief This function return max_ne OTDR paramter data
 *
 * @param max_ne output Parameter for OTDR
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 */
int OtdrGetMaxNE(int* max_ne);

/**
 * @brief This function configures the end of fiber loss threshold front OTDR parameter to x db(float).
 *
 * @param eof_ltf end of fiber loss threshold front parameter for OTDR. Valid range is [XX, XX].
 * @param eof_lt end of fiber loss threshold parameter for OTDR. Valid range is [XX, XX].
 * @param eof_ltlz end of fiber loss threshold last zone parameter for OTDR. Valid range is [XX, XX].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetEofLossThreshold(float eof_ltf, float eof_lt, float eof_ltlz);
 
/**
 * @brief This function gets the end of fiber loss threshold front OTDR parameter value.
 *
 * @param eof_ltf return the end of fiber loss threshold front OTDR parameter value in db(float). [Default value is 14.0]
 * @param eof_lt return the end of fiber loss threshold OTDR parameter value in db(float). [Default value is 5.0]
 * @param eof_ltlz return the end of fiber loss threshold last zone OTDR parameter value in db(float). [Default value is 3.0]
 *  @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetEofLossThreshold(float* eof_ltf, float* eof_lt, float* eof_ltlz);
 
/**
 * @brief This function configures the end of fiber postion threshold front OTDR parameter to x km(float).
 *
 * @param eof_ptf end of fiber position threshold front parameter for OTDR. Valid range is [XX, XX].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetEofPosThresholdFront(float eof_ptf);
/**
 * @brief This function gets the end of fiber position threshold front OTDR parameter value.
 *
 * @param eof_ptf return the end of fiber position threshold front OTDR parameter value in km(float). [Default value is 0.5]
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetEofPosThresholdFront(float* eof_ptf);

/**
 * @brief This function switches UART connection from internal to external otdr and vice versa
 *
 * @param otdr_connection accepts 0 and 1 *           
 *          1: Switch UART connection to External otdr
 *          0: Switch UART connection back to Internal otdr,
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 */
int OtdrSwitchUart(int otdr_connection);

/**
 * @brief This function issue B command to the OTDR.
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrB(void);

/**
 * @brief This function issue reinit command to the OTDR.
 * @param mode accepts 0 and 1 *           
 *          1: Check for normal mode
 *          0: Check for boot mode
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrBootModeStatus(T_OTDREXECMODE mode);

/**
 * @brief This function issue reinit command to the OTDR.
 * @param mode accepts 0 and 1 *           
 *          1: Check for emu_init lock file
 *          0: Check for emu server socket file
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrEmuReinitStatus(T_OTDRINITCHECK mode);

/**
 * @brief This function issue reinit command to the OTDR.
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrEmuReinit(void);

/**
 * @brief The function reads External OTDR flag from file.
 * @param otdr_connection will updated with an integer 1 for external or 0 for internal
 * @return  On success returns 0 and error code otherwise.
 */
int isExternalOTDR (int *otdr_connection);

/**
 * @brief The function updates the external_otdr_flag.
 */
void updateExternalOtdrFlag (void);

/**
* @brief The function sets the OTDR tone generator mode.
* @param mode indicates the tone generator mode to enable. OTDR_TONE_OFF returns the OTDR to the default otdr scan mode.
* @return On success returns 0 and error code otherwise.
*/
int SetToneGeneratorMode(T_TONEMODE mode);

/**
* @brief The function gets the current tone generator mode..
* @param mode will be updated with a enum value indicating the current tone generator mode. OTDR_TONE_OFF indicates default otdr scan mode.
* @return  On success returns 0 and error code otherwise.
*/
int GetToneGeneratorMode(T_TONEMODE *mode);


#endif
