/**
 *  @file 	otdr_interface.c
 *  @brief 	OTDR Interface API Library
 *
 *  	This file contains OTDR command APIs implementations.
 *
 */
#include <dirent.h>
#include "otdr_interface.h"
#include "serial.h"
#include "socket_client.h"
#include "power_management.h"
#include "power_socket_client.h"
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <json.h>
#include "logMgr.h"
/* sfp_interface.h is not included here: it pulls mraa and would cycle with libi2cinterface.
 * GCC 14 rejects the old implicit declaration; prototype only. Resolved at load time. */
int SFPInterfaceInit(void);

#define CRLF                        "\r\n"
#define OTDR                        "otdr"
#define OTDR_EVENT_LOSS_THR         "otdr event_loss_thr"
#define OTDR_EVENT_REFL_THR         "otdr event_refl_thr"
#define OTDR_PULSE_WIDTH            "otdr pulse_width"
#define OTDR_MEASURE_TIME           "otdr measure_time"
#define OTDR_RESOLUTION             "otdr resolution"
#define OTDR_WINDOW_SIZE            "otdr window_size"
#define OTDR_WINDOW_OFFSET          "otdr window_offset"
#define OTDR_SPAN_LENGTH            "otdr span_length"
#define OTDR_REFRACTIVE_INDEX       "otdr refractive_index"
#define OTDR_MAX_NE                 "otdr max_ne"
#define OTDR_EOF_LTF                "otdr eof_ltf"
#define OTDR_EOF_PTF                "otdr eof_ptf"
#define OTDR_ECHO                   "echo"
#define OTDR_CMD_ECHO_ON            "echo on"
#define OTDR_CMD_ECHO_OFF           "echo off"
#define OTDR_BAUD                   "baud"
#define OTDR_MT                     "mt"
#define OTDR_RUN                    "otdr run"
#define OTDR_STOP                   "otdr stop"
#define OTDR_RESET                  "rst"
#define OTDR_ANALYZE                "otdr analyze"
#define OTDR_RESULT                 "otdr result"
#define OTDR_STATUS                 "otdr status"
#define OTDR_VER                    "ver"
#define OTDR_VER_HW                 "ver hw"
#define OTDR_VER_FULL               "ver full"
#define OTDR_PD                     "pd"
#define OTDR_ALRM                   "alrm"
#define OTDR_ALRM_MTX               "alrm mtx"
#define OTDR_ALRM_MTH               "alrm mth"
#define OTDR_ALRM_MTL               "alrm mtl"
#define OTDR_ALRM_TEC1TMP           "alrm tec1tmp"
#define OTDR_ALRM_THRESHOLD         "thr"
#define OTDR_ALRM_HYSTERESIS        "hys"
#define OTDR_ALRM_CLR               "clr"
#define OTDR_DUMP_TRACE             "otdr dump trace"
#define OTDR_DUMP_LTRACE            "otdr dump ltrace"
#define OTDR_DUMP_EVENT             "otdr dump event"
#define OTDR_RECEIVE_FIRMWARE       "recv fw"
#define OTDR_RECEIVE_HARDWARE       "recv hw"
#define OTDR_BOOT                   "boot"
#define OTDR_HWBOOT                 "hwboot"
#define OTDR_RH                     "RH"
#define OTDR_B                      "B"
#define OTDR_RTC                    "rtc"
#define OTDR_SUPPLY                 "supply"
#define OTDR_SUPPLY_CURRENT         "supply current"
#define OTDR_SUPPLY_VOLTAGE         "supply voltage"
#define OTDR_SUPPLY_POWER           "supply power"
#define OTDR_AST                    "ast"
#define OTDR_ASTM                   "astm"
#define OTDR_ASTM_NORMAL            "astm n"
#define OTDR_ASTM_STICKY            "astm s"
#define OTDR_KEY_1                  "key 1"
#define OTDR_X_DUMP_ZMM	     "otdr x dump zmm"
#define OTDR_LIVE_RUN 		"otdr live run"
#define OTDR_LIVE_STOP 		"otdr live stop"
#define OTDR_LIVE_ANALYZE 	"otdr live analyze"
#define OTDR_LIVE_EVENT 	"otdr live event"
#define OTDR_LIVE_TRACE 	"otdr live trace"
#define OTDR_LIVE_ANALYZE_STATUS 	"otdr live analyzestatus"
#define OTDR_LIVE_TRACE_COUNT   "otdr live tracecount"
#define OTDR_LIVE_AVG_ANALYZE  "otdr live avganalyze"
#define OTDR_LIVE_AVG_TRACE 	"otdr live avgtrace"
#define OTDR_LIVE_AVG_EVENT 	"otdr live avgevent"
#define OTDR_LIVE_ANALYZE_TRACE     "otdr live analyzetrace"
#define OTDR_LIVE_AVG_ANALYZE_TRACE     "otdr live avganalyzetrace"
#define OTDR_TONE_GENERATOR_MODE    "otdr txmode"


#define OTDR_ECHO_OFF 			0
#define OTDR_ECHO_ON  			1

#define OTDR_BINARY_FILE_PATH   "/tmp/"
#define OTDR_RESET_FILE_PATH	"/usr/bin/"
#define OTDR_RESET_FILE_NAME	"instrument_reset_config"
#define PROD_INFO_FILE_PATH     "/tmp/product_info.json"

#define BUF                     2048

/** For logger */
#define OTDR_LOGGER_FILE_PATH   "/var/log/OTDR.log"

volatile int  OTDR_REQUEST_TIMEOUT	=	1000000;
volatile int  OTDR_REQUEST_EXT_TIMEOUT =	3000000;
#define OTDR_API_SHORT_RESP_LENGTH	1024
#define OTDR_API_MEDIUM_RESP_LENGTH	8192
#define OTDR_API_SHORT_CMD_LENGTH 	128
#define OTDR_API_MEDIUM_CMD_LENGTH 	1024
#define OTDR_API_LARGE_RESP_LENGTH 	(2*1024*1024)

#define OTDR_SERIALNODE                 "ttyS0"

#define EMU_INIT_PROGRESS_FILE          "/tmp/otdr_init_progress"
#define KL02_FW_UPGRADE_INDICATOR       "/tmp/kl02_fw_upgrade"

int external_otdr_flag = 0;           //External OTDR (UART switched to external for XMF-HD)
int otdrcomm_handle = -1;             //UART Linux device handler
int otdr_powercomm_handle = -1;       //Power management daemon handler
static pthread_mutex_t otdr_lock = PTHREAD_MUTEX_INITIALIZER;
int g_ui8_debug_otdr = 0;
int (*transportReadWrite)(int, uint8_t *,uint16_t, uint8_t *, uint32_t *, uint32_t);
int (*powertransportReadWrite)(int, uint8_t *,uint16_t, uint8_t *, uint32_t *, uint32_t);
/** For Logger Use */
int g_i_lgrOtdrMode = 0;
FILE *g_px_lgrFileptr = NULL;

int OTDRPowerForce(E_POWERSWITCH i_e_switch);
int getOTDRStatusInternal(void);
int OtdrKey1(void);
int isnanf(float x);
/**
 * @brief  Remove trailing spaces from input string
 *         This function remove consecutive leading & trailing spaces from input string
 *
 * @param  str  pointer to a string
 */
void trimleadingandTrailing(char *s)
{   int  i,j;

    for(i=0; s[i]==' '||s[i]=='\t' || s[i] == '\r' || s[i] == '\n'; i++);

    for(j=0; s[i]; i++)
    {   s[j++]=s[i];
    }
    s[j]='\0';
    for(i=0; s[i]!='\0'; i++)
    {   if(s[i]!=' '&& s[i]!='\t' && s[i]!='\r' && s[i]!='\n')
            j=i;
    }
    s[j+1]='\0';
}

/**
 * @brief  This function open serial port and initialize UART parameters (BaudRate,parity,stop-bit)
 *
 *@param   o_puc_err		copy error into string pointer
 *@return  On success returns 0, error code otherwise.
 *
 */
int OtdrInterfaceInit(T_OTDRCOMMMODE mode)
{   int ret_sinit = OTDR_INIT_FAILED;
    otdrcomm_handle = -1;
    /* Initialize Logger */
    g_i_lgrOtdrMode = mode;
    setLogOutput(LOG_OUTPUT_FILE);
    if (!g_px_lgrFileptr)
        setFileLogInfo(&g_px_lgrFileptr, OTDR_LOGGER_FILE_PATH, NULL);
    setLogLevel(LOG_LEVEL_INFO);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s, Mode: %d: Handle: %d]", __func__, mode, otdrcomm_handle);
    updateExternalOtdrFlag (); /*update the external otdr flag for further processing the command*/
    pthread_mutex_lock(&otdr_lock);
    if (otdrcomm_handle < 0)
    {   int rc = -1;int retryCount = 2;

        if (mode == OTDR_SPEED_MODE)
        {   /* Check the /tmp/otdr_init_progress file exist or not */
RETRY:    if (access((char *) EMU_INIT_PROGRESS_FILE, F_OK) == -1)
            {   transportReadWrite = socketOtdrReadWrite;
                /* Setup socket communication with EMU */
                rc = socketInit(&otdrcomm_handle);
                if ((rc != 0) && (otdrcomm_handle < 0) )
                {
                    if(retryCount)
                    {


                        retryCount--;
                        sleep(1);
                        goto RETRY;
                    }
                    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[EMU initialization failed]");

                }
		        else
                {
                    retryCount = 0;
		            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[EMU initialization Successfull on Socket]");
                }

            }
            else
            {   
                if(retryCount)
                {
                    retryCount--;
                    sleep(1);
                    goto RETRY;
                }
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[OTDR initialization is in progress]");
                ret_sinit = OTDR_INIT_PROGRESS;
            }
        }
        else
        {   
            /**** Initialize the serial port to communicate over uart ******/
            if (access("/tmp/lock/ttyS0.LCK", F_OK) == 0)  
                remove("/tmp/lock/ttyS0.LCK");            
            transportReadWrite = serialOtdrReadWrite;
            /* Setup serial communication with OTDR */
            rc = serialPortInit(OTDR_SERIALNODE, &otdrcomm_handle);
	        if ((rc != 0) && (otdrcomm_handle < 0))
                    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Serial communication with OTDR is failed]");
            else
                    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Serial communication with OTDR is Successfull on UART]");
        }

        if ((rc == 0) && (otdrcomm_handle > 0))
            ret_sinit = OTDR_COMMAND_SUCCESS;
    }
    pthread_mutex_unlock(&otdr_lock);

    if (otdr_powercomm_handle < 0 && ret_sinit == OTDR_COMMAND_SUCCESS)
    {   int l_i_ret;
        powertransportReadWrite = socketPowerMgtReadWrite;
        /* Initialize Power Management */
        l_i_ret = powerMgtSocketInit(&otdr_powercomm_handle);
        if(l_i_ret)
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Power Management initialization failed:%d]", otdr_powercomm_handle);
            ret_sinit = OTDR_INIT_FAILED;
        }
        else
        {   ret_sinit = OTDR_COMMAND_SUCCESS;
        }
    }
    else
    {   if (ret_sinit != OTDR_INIT_PROGRESS)
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[OTDR serial/socket initialization failed:%d]", otdrcomm_handle);
        }
    }

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, ret_sinit);

    if (ret_sinit && g_px_lgrFileptr)
        logMgrClose(&g_px_lgrFileptr);

    return ret_sinit;
}

/**
 *
 *  @brief   This function remove semaphore and close UART serial node
 *
 *  @param	 o_puc_err	 copy error string to string pointer
 *  @return  On success, returns 0, error code otherwise.
 *
 */
int OtdrInterfaceDeInit(void)
{   int ret_sdeinit = OTDR_DEINIT_FAILED;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    pthread_mutex_lock(&otdr_lock);
    if (otdrcomm_handle > 0)
    {   int rc;
        if (transportReadWrite == socketOtdrReadWrite)
            rc = socketDeInit(otdrcomm_handle);
        else if (transportReadWrite == serialOtdrReadWrite)
            rc = serialPortDeInit(OTDR_SERIALNODE, otdrcomm_handle);
        else
            rc = -1;
        otdrcomm_handle = -1;
        if (rc == 0)
            ret_sdeinit = OTDR_COMMAND_SUCCESS;
        else
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s: %d otdr serial/socket deinit failed]", __func__, rc);
    }
    pthread_mutex_unlock(&otdr_lock);

    if (otdr_powercomm_handle > 0)
    {   int l_i_ret = 0;
        l_i_ret = powerMgtsocketDeInit(otdr_powercomm_handle);
        if(l_i_ret)
        {   ret_sdeinit = OTDR_DEINIT_FAILED;
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Power Management Deinit failed: %d]", l_i_ret);
        }
        else
        {   ret_sdeinit = OTDR_COMMAND_SUCCESS;
            otdr_powercomm_handle = -1;
        }
    }

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, ret_sdeinit);
    if (g_px_lgrFileptr)
        logMgrClose(&g_px_lgrFileptr);

    return ret_sdeinit;
}

/**
 * @brief Ascending sorting of events based on KMs.
 */
void event_sort(T_DUMPEVENTENTRY* events,int n)
{   T_DUMPEVENTENTRY temp = {0};
    int i = 0, j = 0;
    for (i = 1; i < n; i++)
        for (j = 0; j < n - i; j++)
        {   if (events[j].x_km > events[j + 1].x_km)
            {   temp = events[j];
                events[j] = events[j + 1];
                events[j + 1] = temp;
            }
        }
}


/**
 * @brief Checks if library is initialized.
 *
 * @return 1 if library is initialized or 0 if not, And it returns error number
 *         in case of kl02 fw upgrade process
 */
static int checkInit(void)
{   pthread_mutex_lock(&otdr_lock);
    const int initialized = (otdrcomm_handle > 0);
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s: Communication handle: %d, Is OTDR still initialized?: %d]", __func__, otdrcomm_handle, initialized);
    pthread_mutex_unlock(&otdr_lock);
    if (!initialized)
        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s Error: "
                 "OTDR Interface is not initialized, otdrcomm_handle: %d]", __func__, otdrcomm_handle);

    else
    {   /* Check this flag for kl02 firmware upgrader */
        if (access((char *) KL02_FW_UPGRADE_INDICATOR, F_OK) == 0)
        {   return (int) FREESCALE_FW_UPGRADE_INPROGRESS;
        }
	
		if(external_otdr_flag)
        {
            if (POWERMGT_DEVICE_ON != getSFPStatus())
            {   
                //return OTDR_POWERED_OFF;
            }
	    	LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[OTDR routed to external, flag: %d]", external_otdr_flag);
        }
        else if (POWERMGT_DEVICE_ON != getOTDRStatusInternal())
        {   return OTDR_POWERED_OFF;
        }
    }

    return initialized;
}

/**
 * @brief Checks if library is initialized for power communication or not.
 *
 * @return 1 if library is initialized or 0 if not, And it returns error number
 *         in case of kl02 fw upgrade process
 */
static int checkOTDRPowerInit(void)
{   const int initialized = (otdr_powercomm_handle > 0);
    if (!initialized)
        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s Error: "
                 "OTDR Interface is not initialized for Power communication, otdr_powercomm_handle: %d]", __func__, otdr_powercomm_handle);

    else
    {   /* Check this flag for kl02 firmware upgrader */
        if (access((char *) KL02_FW_UPGRADE_INDICATOR, F_OK) == 0)
        {   return (int) FREESCALE_FW_UPGRADE_INPROGRESS;
        }
    }
    return initialized;
}

/**
 * @brief This function process the otdr response and return expected values
 *
 * @param fullResponse Input string providing complete OTDR response
 * @param PrefixStr Input string providing sub string before expected value
 * @param PostfixStr Input string providing sub string after expected value
 * @param expectedVal Output string providing expected value
 * @return On success returns 0, While in case of fail returns error code
 */
int processResponse(int8_t *fullResponse, int8_t *PrefixStr, int8_t *PostfixStr,
                    int8_t *expectedVal)
{   int result_len = 0;
    int8_t *PrefixPtr = NULL;
    int8_t *PostfixPtr = NULL;
    int8_t *ResultPtr = NULL;

    PrefixPtr = strstr((const char *) fullResponse, (const char *) PrefixStr);
    PostfixPtr = strstr((const char *) fullResponse, (const char *) PostfixStr);
    if ((PrefixPtr == NULL) || (PostfixPtr == NULL)) /* Substring NOT found */
    {   if (g_ui8_debug_otdr)
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s: '%s' doesn't contain '%s' or '%s']", __func__, fullResponse,
                     PrefixStr, PostfixStr);
        }
        return OTDR_INVALID_INPUT;
    }

    ResultPtr = PrefixPtr + strlen(PrefixStr);
    result_len = PostfixPtr - ResultPtr;

    /* copying result_len bytes starting from ResultPtr to expectedVal */
    strncpy(expectedVal, ResultPtr, abs(result_len));
    expectedVal[abs(result_len)] = 0;
    /* Debug print */
    if (g_ui8_debug_otdr)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s: Response string:%s]", __func__, expectedVal);
    }
    return OTDR_COMMAND_SUCCESS;
}

/**
 *
 *  @brief   This function remove spaces in a string
 *
 *  @param	 s	 copy string to string pointer
 *  @return  On success, returns 0, On error returns 0-1.
 *
 */
int remove_spaces(char* s)
{   const char* d = s;
    do
    {   while ((*d == '\r') || (*d == '\n') || (*d == ' '))
        {   ++d;
        }
    }
    while ((*s++ = *d++) != '\0');
    return OTDR_COMMAND_SUCCESS;
}

/**
 * @brief This function get value after colon
 *
 * @param inputstr input string
 * @param outstr output string as a value
 */
void valueAfterColon(uint8_t inpstr[], uint8_t outstr[])
{   char* token=NULL;
    if (NULL != inpstr)
    {   token = strtok((char *) inpstr, ":");
        token = strtok(NULL, "");
    }
    if (NULL != token)
    {   remove_spaces(token);
        strncpy((char *) outstr, token, strlen(token));
        outstr[strlen(token)] = '\0';
    }
}

/**
 * @brief This function parses an alarm value string as a floating-point number (returns NaN for 'D').
 *
 * @param inputstr input string
 * @return floating-point translation of input string
 */
float parseAlrmValFloat(uint8_t inpval[])
{   return (strstr(inpval, "D") != NULL) ? NAN : strtof(inpval, NULL);
}

/**
 * @brief This function parses an alarm value string as status: 0=OFF and 1=ON (or -1 if error).
 *
 * @param inputstr input string
 * @return integer (boolean) translation of input string
 */
int parseAlrmValStatus(uint8_t inpval[])
{   return (strstr(inpval, "OFF") != NULL) ? 0 : (strstr(inpval, "ON") != NULL) ? 1 : -1;
}

int responseErrorCheck(char *str)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    if ((strstr(str, "?Argument") != NULL) && (strstr(str, "out of range") != NULL))
    {   l_i_retstatus = OTDR_INPUT_OUT_OF_RANGE;
    }
    else if ((strstr(str, "?Argument") != NULL) && (strstr(str, "invalid") != NULL))
    {   
        l_i_retstatus = OTDR_INVALID_INPUT;
    }
    else if (strstr(str, "?Unknown") != NULL)
    {   l_i_retstatus = OTDR_COMMAND_NOT_SUPPORTED;
    }
    else if (strstr(str, "$") != NULL)
    {   l_i_retstatus = OTDR_CORRUPTION_ERROR;
    }
    else if (strstr(str, "Error:") != NULL)
    {   if (strstr(str,"Busy") != NULL)
        {   l_i_retstatus = OTDR_LIVE_BUSY;
        }
        else if (strstr(str,"Not in live scan mode") != NULL)
        {   l_i_retstatus = OTDR_LIVE_SCAN_DISABLED;
        }
        else if (strstr(str,"No valid data") != NULL)
        {   l_i_retstatus = OTDR_LIVE_INVALID_DATA;
        }
        else if (strstr(str,"in live scan mode") != NULL)
        {   l_i_retstatus = OTDR_LIVE_SCAN_ENABLED;
        }
        else
        {   l_i_retstatus = OTDR_UNEXPECTED_ERROR;
        }
    }
    return l_i_retstatus;
}

/**
 * Function to remove all occurrences of a character from the string.
 */
void removeChar(char *s, int c)
{

    int j, n = strlen(s);
    for (int i=j=0; i<n; i++)
        if (s[i] != c)
            s[j++] = s[i];

    s[j] = '\0';
}

/**
 * @brief This function send file to otdr
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSendFile(int8_t *fileName)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint8_t l_aui8_strmBuff[1024 * 1024] = { 0 };
    char node[32] = {0};

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s. Filename: %s]", __func__, fileName);

    strcpy(node,"/dev/");
    strcat(node,OTDR_SERIALNODE);
    snprintf(l_aui8_strmBuff, sizeof(l_aui8_strmBuff), "lsz -bqkX %s <%s >/dev/null 2>&1 >%s",
             fileName, node,node);
    system((const char *) l_aui8_strmBuff);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/*********************************** API IMPLEMENTATION ************************/

/**
 * @brief This function configures event loss thresold
 *
 * @param thr Input values in dB
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetEventLossThreshold(float thr)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %f%s",
                 OTDR_EVENT_LOSS_THR, thr, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function gets the current OTDR configure parameters and state
 *
 * @param params Output value as a T_OTDRCONFIG structure
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetParameters(T_OTDRCONFIG *params)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int retProcessResp = -1;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    uint8_t expectedVal[28] = { };
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    uint8_t lines[32][128];
    char newLineDelim[] = "\n";
    char commaDelim[] = ",";
    int lineCount = 0;
    char *end_str = NULL;
    char *token = NULL;
    char *end_token = NULL;
    uint8_t retstr[32] = {0};
    int resCounter = 0;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (params == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);
                                              
        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        if (NULL != l_i8_Resp)
        {   token = strtok_r(l_i8_Resp, newLineDelim, &end_str);

            while (token != NULL)
            {   if (strstr(token, "otdr") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                else if (strstr((char *) token, "OTDR EVENT_LOSS_THR") != NULL)
                {   /** Process the Command to get the Output*/
                    retProcessResp = processResponse(token, ": ", "dB",
                                                     expectedVal);
                    params->lossThreshold = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token,
                                "OTDR EVENT_REFL_THR") != NULL)
                {   /** Process the Command to get the Output*/
                    retProcessResp = processResponse(token, ": ", "dB",
                                                     expectedVal);
                    params->reflectionThreshold = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token,
                                "OTDR MEASURE_TIME") != NULL)
                {   retProcessResp = processResponse(token, ": ", " s",
                                                     expectedVal);
                    params->measureTime = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token, "OTDR PULSE_WIDTH") != NULL)
                {   retProcessResp = processResponse(token, ": ", "us",
                                                     expectedVal);
                    params->pulseWidth = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token, "OTDR RESOLUTION") != NULL)
                {   retProcessResp = processResponse(token, ": ", " m",
                                                     expectedVal);
                    params->resolution = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token, "OTDR SPAN_LENGTH") != NULL)
                {   retProcessResp = processResponse(token, ": ", "km",
                                                     expectedVal);
                    params->spanLength = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token,
                                "OTDR WINDOW_OFFSET") != NULL)
                {   retProcessResp = processResponse(token, ": ", "km",
                                                     expectedVal);
                    params->windowOffset = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token, "OTDR WINDOW_SIZE") != NULL)
                {   retProcessResp = processResponse(token, ": ", "km",
                                                     expectedVal);
                    params->windowSize = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token, "OTDR REFRACTIVE_INDEX") != NULL)
                {   valueAfterColon(token,expectedVal);
                    params->refractiveIndex = strtod(expectedVal, NULL);
                    resCounter++;
                }

                token = strtok_r(NULL, newLineDelim, &end_str);
            }

            if (resCounter != 9)
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            }

        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function configure refractive index
 *
 * @param index input value
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetRefractiveIndex(double index)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %0.7lf%s",
                 OTDR_REFRACTIVE_INDEX, index, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function configure pulse width to x microseconds
 *
 * @param pw input value pulse width to x microseconds
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetPulseWidth(float pw)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %f%s",
                 OTDR_PULSE_WIDTH, pw, CRLF);
        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite pulseWidth: %s]", (char *) l_i8_command);
        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function configure the measurement time to x seconds
 *
 * @param mtime input value measurement time
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetMeasureTime(float mtime)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %f%s",
                 OTDR_MEASURE_TIME, mtime, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function configure the size of the measurement window to x
 kilometers
 *
 * @param ws input value window size
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetWindowSize(float ws)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %f%s",
                 OTDR_WINDOW_SIZE, ws, CRLF);
        
        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite windowsize: %s]", (char *) l_i8_command);
        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function configure the resolution to x meters
 *
 * @param res input value resolution
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetResolution(float res)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %f%s",
                 OTDR_RESOLUTION, res, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function configure the span length to x kilometers
 *
 * @param sl input value span length
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetSpanLength(float sl)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %f%s",
                 OTDR_SPAN_LENGTH, sl, CRLF);

        /** Call the setget command*/
        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite spanlength: %s]", (char *) l_i8_command);
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function configure the size of the measurement window to x
 kilometers
 *
 * @param wo input value window offset
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetWindowOffset(float wo)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %f%s",
                 OTDR_WINDOW_OFFSET, wo, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function configures current echoing to "ON" or "OFF"
 *
 * @param echo input echoing  Value will be either 1 (for OTDR_ECHO_ON) / 0 (for OTDR_ECHO_OFF)
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetEcho(int echo)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }

    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   if (echo == OTDR_ECHO_OFF)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                     OTDR_CMD_ECHO_OFF, CRLF);
        }
        else if (echo == OTDR_ECHO_ON)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                     OTDR_CMD_ECHO_ON, CRLF);
        }
        else
        {   l_i_retstatus = OTDR_INVALID_INPUT;
            break;
        }

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function get current echoing
 *
 * @param echo return echoing. Value will be either 1 (for OTDR_ECHO_ON) or 0 (for OTDR_ECHO_OFF)
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetEcho(int *echo)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char* val = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (echo == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s", OTDR_ECHO,
                 CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        /** Process the Command to get the Output*/
        val = strtok((char *) l_i8_Resp, ":");
        if (val != NULL)
        {   val = strtok(NULL, ":");
            if (val != NULL)
            {   l_i_retstatus = remove_spaces((char *) val);
            }
            else
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
                break;
            }
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
        if (!l_i_retstatus)
        {   if (g_ui8_debug_otdr)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Expected Value: %s]", val);
            }
            if (strstr((char *) val, "ON") != NULL)
            {   *echo = OTDR_ECHO_ON;
            }
            else if (strstr((char *) val, "OFF") != NULL)
            {   *echo = OTDR_ECHO_OFF;
            }
            else
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
                break;
            }
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function configure baud rate to x bits-per-second
 *
 * @param baud input baud rate to x bits-per-second as a integer
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetBaud(int baud)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %d%s",
                 OTDR_BAUD, baud, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function get baud rate to x bits-per-second
 *
 * @param baud return baud rate to x bits-per-second as a integer
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetBaud(int *baud)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char* val = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (baud == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_BAUD, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        /** Process the Command to get the Output*/
        val = strtok((char *) l_i8_Resp, ":");
        if (val != NULL)
        {   val = strtok(NULL, ":");
            if (val != NULL)
            {   l_i_retstatus = remove_spaces(val);
            }
            else
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
                break;
            }
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
        if (!l_i_retstatus)
        {   if (g_ui8_debug_otdr)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Expected Value: %s]", val);
            }
            *baud = atoi(val);
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function get module case temperature in °C
 *
 * @param mt return module case temperature in °C as a array of float
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetMt(float *mt)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int retProcessResp = -1;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    char* val = NULL;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    int8_t *l_i8_respPtr = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (mt == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_MT, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);


        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        if( NULL == (l_i8_respPtr = strstr((char *) l_i8_Resp, "MT:")))
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }

        /** Process the Command to get the Output*/
        val = strtok((char *) l_i8_respPtr, ":");
        if (val != NULL)
        {   val = strtok(NULL, ":");
            if (val != NULL)
            {   retProcessResp = remove_spaces(val);
            }
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
        }
        if ((!retProcessResp) && (val != NULL))
        {   if (g_ui8_debug_otdr)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Expected Value: %s]", val);
            }
            *mt = strtof(val, NULL);
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function sets threshold value for the specified alarm
 *
 * @param alarm input value specifying which alarm to set. OTDR_ALARM_ALL is invalid for this API.
 * @param threshold input value temperature in °C
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */

int OtdrSetAlrmThreshold(T_OTDRALARMS alarm, float threshold)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   const char *alrm = NULL;
        switch (alarm)
        {   case OTDR_ALARM_MTX:
                alrm = OTDR_ALRM_MTX;
                break;
            case OTDR_ALARM_MTH:
                alrm = OTDR_ALRM_MTH;
                break;
            case OTDR_ALARM_MTL:
                alrm = OTDR_ALRM_MTL;
                break;
            case OTDR_ALARM_TEC1TMP:
                alrm = OTDR_ALRM_TEC1TMP;
                break;
            case OTDR_ALARM_ALL:
                break;
        }
        if (alrm != NULL)
        {   if (!isnanf(threshold))
                snprintf(l_i8_command, sizeof(l_i8_command), "%s %s %g%s", alrm, OTDR_ALRM_THRESHOLD, threshold, CRLF);
            else
                snprintf(l_i8_command, sizeof(l_i8_command), "%s %s %c%s", alrm, OTDR_ALRM_THRESHOLD, 'D', CRLF);
        }
        else
        {   l_i_retstatus = OTDR_INVALID_INPUT;
            break;
        }

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function sets hysteresis value for the specified alarm
 *
 * @param alarm input value specifying which alarm to set. OTDR_ALARM_ALL is invalid for this API.
 * @param hysteresis input value temperature in °C
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */

int OtdrSetAlrmHysteresis(T_OTDRALARMS alarm, float hysteresis)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   const char *alrm = NULL;
        switch (alarm)
        {   case OTDR_ALARM_MTX:
                alrm = OTDR_ALRM_MTX;
                break;
            case OTDR_ALARM_MTH:
                alrm = OTDR_ALRM_MTH;
                break;
            case OTDR_ALARM_MTL:
                alrm = OTDR_ALRM_MTL;
                break;
            case OTDR_ALARM_TEC1TMP:
                alrm = OTDR_ALRM_TEC1TMP;
                break;
            case OTDR_ALARM_ALL:
                break;
        }
        if (alrm != NULL)
        {   snprintf(l_i8_command, sizeof(l_i8_command), "%s %s %g%s", alrm, OTDR_ALRM_HYSTERESIS, hysteresis, CRLF);
        }
        else
        {   l_i_retstatus = OTDR_INVALID_INPUT;
            break;
        }

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function clears the latching status for the specified alarm.
 * ALRM CLR/ALRM x CLR resets the sticky status only if the current alarm status (as reported by ALRM STA) is OFF
 *
 * @param alarm input value specifying which alarm to clear. OTDR_ALARM_ALL is allowed for this API.
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */

int OtdrClearAlarmLatch(T_OTDRALARMS alarm)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (alarm == 0)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %s%s",
                     OTDR_ALRM_MTX, OTDR_ALRM_CLR, CRLF);
        }
        else if (alarm == 1)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %s%s",
                     OTDR_ALRM_MTH, OTDR_ALRM_CLR, CRLF);
        }
        else if (alarm == 2)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %s%s",
                     OTDR_ALRM_MTL, OTDR_ALRM_CLR, CRLF);
        }
        else if (alarm == 3)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %s%s",
                     OTDR_ALRM_TEC1TMP, OTDR_ALRM_CLR, CRLF);
        }
        else if (alarm == 4)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %s%s",
                     OTDR_ALRM, OTDR_ALRM_CLR, CRLF);
        }
        else
        {   l_i_retstatus = OTDR_INVALID_INPUT;
            break;
        }

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function starts a new OTDR measurement. State can be checked using OtdrStatus()
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrRun(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char newLineDelim[] = "\n";
    char *end_str = NULL;
    char* token = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_RUN, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }
        //
        if(strstr(l_i8_Resp, "?In live scan mode."))
        {
            l_i_retstatus = OTDR_LIVE_SCAN_ENABLED;
            break;
        }
        //
        /** Process the Command to get the Output*/

        if (NULL != l_i8_Resp)
        {   token = strtok(l_i8_Resp, newLineDelim);

            // Keep printing tokens while one of the
            // delimiters present in l_i8_Resp[].
            while (token != NULL)
            {   if (strstr(token, "!busy") != NULL)
                {   l_i_retstatus = OTDR_BUSY;
                }
                token = strtok(NULL, newLineDelim);
            }
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function returns the result of the last OTDR measurement.
 *
 * @param result Pointer to variable where result is stored.
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrResult(T_OTDRRESULT *result)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int retProcessResp = -1;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    char* val = NULL;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char *last = NULL;
    char percent[5];
    int8_t *l_i8_respPtr = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (result == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_RESULT, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }
        if(NULL == (l_i8_respPtr = strstr(l_i8_Resp,"OTDR RESULT:")))
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
        /** Process the Command to get the Output*/
        val = strchr(l_i8_respPtr, ':');
        if (val != NULL)
        {   ++val;
            if (g_ui8_debug_otdr)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Expected Value: %s]", val);
            }
            char *p;
            if ((p = strstr(val, "OK_HR")) != NULL)
            {   char *e;
                result->code = OTDR_RESULT_OK_HR;
                result->hrLocation = strtof(p += 5, &e);
                if ((e == p) || (*e != ' '))
                {   l_i_retstatus = OTDR_PROCESSING_ERROR;
                    break;
                }
            }
            else if (strstr(val, "OK") != NULL)
            {   result->code = OTDR_RESULT_OK;
            }
            else if (strstr(val, "ABORT") != NULL)
            {   result->code = OTDR_RESULT_ABORT;
            }
            else if (strstr(val, "ERROR") != NULL)
            {   result->code = OTDR_RESULT_ERROR;
            }
            else
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
                break;
            }
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function return otdr status
 *
 * @param status Output values lists current state and state completion percentage.
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrStatus(T_OTDRSTATUS *status)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int retProcessResp = -1;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    char* val = NULL;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char *last = NULL;
    char percent[5];
    int8_t *l_i8_respPtr = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s. Communication handle: %d]", __func__, otdrcomm_handle);

    if ((l_i_retstatus = checkInit()) > 1)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Init check failed. Return Value: %d]", OTDR_COMMUNICATION_ERROR);
        return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Init check successful]");
    l_i_retstatus = OTDR_COMMAND_SUCCESS;


    do
    {   if (status == NULL)
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[NULL Pointer received from application: %d.", OTDR_NULL_POINTER);
            l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        status->currentState = OTDR_STATE_UNKNOWN;
        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_STATUS, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: OTDR ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }
        if(NULL == (l_i8_respPtr = strstr(l_i8_Resp,"OTDR STATUS:")))
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
        /** Process the Command to get the Output*/
        val = strtok((char *) l_i8_respPtr, ":");
        if (val != NULL)
        {   val = strtok(NULL, ":");
            if (val != NULL)
            {   trimleadingandTrailing(val);
                if (g_ui8_debug_otdr)
                {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Expected Value: %s]", val);
                }
              
                if (strstr((char *) val, "LIVE") != NULL)
                {   
                    status->currentState = OTDR_STATE_LIVE_CAPTURE; 
                }
                else if (strstr((char *) val, "CAPTURE") != NULL)
                {   status->currentState = OTDR_STATE_CAPTURE;
                    if (strstr((char *) val, " ") != NULL)
                    {   last = strrchr(val, ' ');
                        if (last != NULL)
                        {   strcpy(percent, last+1);
                            removeChar(percent,'%');
                            status->percentage = strtof(percent, NULL);
                        }
                    }
                }
                else if (strstr((char *) val, "IDLE") != NULL)
                {   status->currentState = OTDR_STATE_IDLE;
                }
                else if (strstr((char *) val, "ANALYZE") != NULL)
                {   status->currentState = OTDR_STATE_ANALYZE;
                }
                else if (strstr((char *) val, "PROCESS") != NULL)
                {   status->currentState = OTDR_STATE_PROCESS;
                }
                else if (strstr((char *) val, "TRANSFER") != NULL)
                {   status->currentState = OTDR_STATE_TRANSFER;
                }
                else if (strstr((char *) val, "SETUP") != NULL)
                {   status->currentState = OTDR_STATE_SETUP;
                }                
                else
                {   status->currentState = OTDR_STATE_UNKNOWN;
                }

                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[OTDR State: %s]", val);
            }
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function gets version information
 *
 * @param ver return version information as a structure containing string elements. On return, each string is a null-terminated unsigned char string; trailing spaces are removed
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetVer(T_VER *ver)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char newLineDelim[] = "\n";
    char colonDelim[] = ":";
    char *end_str;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (ver == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        memset(ver, 0, sizeof(T_VER));

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_VER, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);        

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        /** Process the Command to get the Output*/

        if (NULL != l_i8_Resp)
        {   char *token = strtok_r(l_i8_Resp, newLineDelim, &end_str);

            while (token != NULL)
            {   char *end_token;
                if (strstr(token, "ver") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                else if (strstr((char *) token, ":") != NULL)
                {   unsigned char *token2 = strtok_r(token, colonDelim, &end_token);
                    while (token2 != NULL)
                    {   token2 = strtok_r(NULL, colonDelim, &end_token);
                        if (NULL != token2)
                        {   remove_spaces(token2);
                            if ((strstr(token, "Configuration") != NULL) || (strstr(token, "Design Version") != NULL))
                            {   strncpy((unsigned char *) ver->partnumber, token2, sizeof(ver->partnumber) - 1);
                            }
                            else if (strstr(token, "Firmware Vers") != NULL)
                            {   strncpy((unsigned char *) ver->version, token2, sizeof(ver->version) - 1);
                            }
                            else if (strstr(token, "Serial Number") != NULL)
                            {   strncpy((unsigned char *) ver->serialnumber, token2, sizeof(ver->serialnumber) - 1);
                            }
                        }
                        break;
                    }

                    token = strtok_r(NULL, newLineDelim, &end_str);
                }
                else
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                }
            }
            if (ver->version[0] == '\0') /* Removed SN comparision due to exception (ver->serialnumber[0] == '\0')*/
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            }
            if (ver->partnumber[0] == '\0')
                strcpy(ver->partnumber, "(unknown)");
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}


/**
 * @brief This function gets full version information
 *
 * @param ver return version information as a structure containing string elements. On return, each string is a null-terminated unsigned char string; leading and trailing spaces are removed
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetVerFull(T_VER_FULL *ver_full)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char newLineDelim[] = "\n";
    char colonDelim[] = ":";
    char *end_str;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);


    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   if (ver_full == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        memset(ver_full, 0, sizeof(T_VER_FULL));

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_VER_FULL, CRLF);
           
        /**Get version details over uart if the otdr initialized with speed mode*/    
        if (transportReadWrite == socketOtdrReadWrite)
        {
            int rc;
            int otdrcomm_handle_uart = 0;
            /**** Initialize the serial port to communicate over uart ******/
            if (access("/tmp/lock/ttyS0.LCK", F_OK) == 0)  
                remove("/tmp/lock/ttyS0.LCK");            
            transportReadWrite = serialOtdrReadWrite;
            /* Setup serial communication with OTDR */
            rc = serialPortInit(OTDR_SERIALNODE, &otdrcomm_handle_uart);
	        if ((rc != 0) && (otdrcomm_handle_uart < 0))
                    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Serial communication with OTDR is failed]");
            else
            {
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Serial communication with OTDR is Successfull on UART]");
                /** Call the setget command*/
                l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle_uart,
                                                    (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                                    (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);
            }
            
            /**Deinitialize the serial port*/
            transportReadWrite = socketOtdrReadWrite;
            rc = serialPortDeInit(OTDR_SERIALNODE, otdrcomm_handle_uart);
            if (rc)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s: %d otdr serial/socket deinit failed]", __func__, rc);

            }

        }
        else
        {
            /** Call the setget command*/
            l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                                (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                                (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);
        }


        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        /** Process the Command to get the Output*/

        if (NULL != l_i8_Resp)
        {   char *token = strtok_r(l_i8_Resp, newLineDelim, &end_str);

            while (token != NULL)
            {   char *end_token;
                if (strstr(token, "ver_full") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                else if (strstr((char *) token, ":") != NULL)
                {   unsigned char *token2 = strtok_r(token, colonDelim, &end_token);
                    while (token2 != NULL)
                    {   token2 = strtok_r(NULL, colonDelim, &end_token);
                        if (NULL != token2)
                        {   trimleadingandTrailing(token2);
                            if ((strstr(token, "Configuration") != NULL))
                            {   strncpy((unsigned char *) ver_full->partnumber, token2, sizeof(ver_full->partnumber) - 1);
                            }
                            else if (strstr(token, "Firmware Vers") != NULL)
                            {   strncpy((unsigned char *) ver_full->version, token2, sizeof(ver_full->version) - 1);
                            }
                            else if (strstr(token, "Serial Number") != NULL)
                            {   strncpy((unsigned char *) ver_full->serialnumber, token2, sizeof(ver_full->serialnumber) - 1);
                            }
                            else if  (strstr(token, "Design version") != NULL)
                            {   strncpy((unsigned char *) ver_full->designversion, token2, sizeof(ver_full->designversion) - 1);
                            }
                            else if  (strstr(token, "Emulator Vers") != NULL)
                            {   strncpy((unsigned char *) ver_full->emuversion, token2, sizeof(ver_full->emuversion) - 1);
                            }
                            else if (strstr(token, "Loader version") != NULL)
                            {   strncpy((unsigned char *) ver_full->loader, token2, sizeof(ver_full->loader)-1);
                            }
                            else if (strstr(token, "SPI peripheral version") != NULL)
                            {   strncpy((unsigned char *) ver_full->spiPeripheral, token2, sizeof(ver_full->spiPeripheral)-1);
                            }
                            else if (strstr(token, "System clock") != NULL)
                            {   strncpy((unsigned char *) ver_full->sysClock, token2, sizeof(ver_full->sysClock)-1);
                            }
                            else if (strstr(token, "Scheduling period") != NULL)
                            {   strncpy((unsigned char *) ver_full->schedulePeriod, token2, sizeof(ver_full->schedulePeriod)-1);
                            }
                            else if (strstr(token, "Data cache") != NULL)
                            {   strncpy((unsigned char *) ver_full->dataCache, token2, sizeof(ver_full->dataCache)-1);
                            }
                            else if (strstr(token, "Inst cache") != NULL)
                            {   strncpy((unsigned char *) ver_full->instCache, token2, sizeof(ver_full->instCache)-1);
                            }
                            else if (strstr(token, "Vendor Name") != NULL)
                            {   strncpy((unsigned char *) ver_full->VendorName, token2, sizeof(ver_full->VendorName)-1);
                            }
                            else if (strstr(token, "Vendor PN") != NULL)
                            {   strncpy((unsigned char *) ver_full->VendorPN, token2, sizeof(ver_full->VendorPN)-1);
                            }
                            else if (strstr(token, "Hardware Revision") != NULL)
                            {   strncpy((unsigned char *) ver_full->HwRev, token2, sizeof(ver_full->HwRev)-1);
                            }
                            else if (strstr(token, "Platform") != NULL)
                            {   ver_full->platform = atoi(token2);
                            }
                        }
                        break;
                    }

                    token = strtok_r(NULL, newLineDelim, &end_str);
                }
                else
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                }
            }
            /* if ((ver_full->version[0] == '\0') || (ver_full->serialnumber[0] == '\0'))
             {   l_i_retstatus = OTDR_PROCESSING_ERROR;
             }*/
            if (ver_full->partnumber[0] == '\0')
                strcpy(ver_full->partnumber, "(unknown)");
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function get the resolution to x meters
 *
 * @param pd return pd value  as array of float
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetPd(float pd[])
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int retProcessResp = -1;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    uint8_t expectedVal[10] = { };
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char newLineDelim[] = "\n";
    char colonDelim[] = ":";
    char *end_str = NULL;
    char *token = NULL;
    char *token2 = NULL;
    int resCounter = 0;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (pd == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_PD, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        if (l_i8_Resp != NULL)
        {   /** Process the Command to get the Output*/

            token = strtok_r(l_i8_Resp, newLineDelim, &end_str);

            while (token != NULL)
            {   char *end_token;
                if (strstr(token, "pd") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                else if (strstr((char *) token, ":") != NULL)
                {   token2 = strtok_r(token, colonDelim, &end_token);
                    while (token2 != NULL)
                    {   token2 = strtok_r(NULL, colonDelim, &end_token);
                        if (NULL != token2)
                        {   if (strstr(token, "PD 1") != NULL)
                            {   resCounter++;
                                pd[0] = strtof(token2, NULL);
                            }
                            else if (strstr(token, "PD 2") != NULL)
                            {   resCounter++;
                                pd[1] = strtof(token2, NULL);
                            }
                            else
                            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
                            }
                        }
                        break;
                    }

                    token = strtok_r(NULL, newLineDelim, &end_str);
                }
                else
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                }
            }
            if (resCounter != 2)
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            }
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }


    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function get information about alarms
 *
 * @param alrm return information about alarms as a struct alrm with following values (0 for "OFF" or "d") as structure of float values)
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetALRM(T_ALRM *alrm)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_MEDIUM_CMD_LENGTH;
    char *end_str = NULL;
    char newLineDelim[] = "\n";
    char colonDelim[] = ":";
    char *end_token = NULL;
    char *token2 = NULL;
    char *token = NULL;
    int resCounter = 0;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (alrm == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_ALRM, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        if (l_i8_Resp != NULL)
        {   /** Process the Command to get the Output*/

            token = strtok_r(l_i8_Resp, newLineDelim, &end_str);

            while (token != NULL)
            {   if (strstr(token, "alrm") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                else if (strstr((char *) token, ":") != NULL)
                {   token2 = strtok_r(token, colonDelim, &end_token);
                    while (token2 != NULL)
                    {   token2 = strtok_r(NULL, colonDelim, &end_token);
                        if (NULL != token2)
                        {   if (strstr((char *) token, "ALRM MTX STA") != NULL)
                            {   resCounter++;
                                alrm->MtxSta = parseAlrmValStatus(token2);
                            }
                            else if (strstr((char *) token, "ALRM MTX SST") != NULL)
                            {   resCounter++;
                                alrm->MtxSst = parseAlrmValStatus(token2);
                            }
                            else if (strstr((char *) token, "ALRM MTX THR") != NULL)
                            {   resCounter++;
                                alrm->MtxThr = parseAlrmValFloat(token2);
                            }
                            else if (strstr((char *) token, "ALRM MTX HYS") != NULL)
                            {   resCounter++;
                                alrm->MtxHys = parseAlrmValFloat(token2);
                            }
                            else if (strstr((char *) token, "ALRM MTH STA") != NULL)
                            {   resCounter++;
                                alrm->MthSta = parseAlrmValStatus(token2);
                            }
                            else if (strstr((char *) token, "ALRM MTH SST") != NULL)
                            {   resCounter++;
                                alrm->MthSst = parseAlrmValStatus(token2);
                            }
                            else if (strstr((char *) token, "ALRM MTH THR") != NULL)
                            {   resCounter++;
                                alrm->MthThr = parseAlrmValFloat(token2);
                            }
                            else if (strstr((char *) token, "ALRM MTH HYS") != NULL)
                            {   resCounter++;
                                alrm->MthHys = parseAlrmValFloat(token2);
                            }
                            else if (strstr((char *) token, "ALRM MTL STA") != NULL)
                            {   resCounter++;
                                alrm->MtlSta = parseAlrmValStatus(token2);
                            }
                            else if (strstr((char *) token, "ALRM MTL SST") != NULL)
                            {   resCounter++;
                                alrm->MtlSst = parseAlrmValStatus(token2);
                            }
                            else if (strstr((char *) token, "ALRM MTL THR") != NULL)
                            {   resCounter++;
                                alrm->MtlThr = parseAlrmValFloat(token2);
                            }
                            else if (strstr((char *) token, "ALRM MTL HYS") != NULL)
                            {   resCounter++;
                                alrm->MtlHys = parseAlrmValFloat(token2);
                            }
                            else if (strstr((char *) token, "ALRM TEC1TMP STA") != NULL)
                            {   //resCounter++;
                                alrm->Tec1tmpSta = parseAlrmValStatus(token2);
                            }
                            else if (strstr((char *) token, "ALRM TEC1TMP SST") != NULL)
                            {   //resCounter++;
                                alrm->Tec1tmpSst = parseAlrmValStatus(token2);
                            }
                            else if (strstr((char *) token, "ALRM TEC1TMP THR") != NULL)
                            {   //resCounter++;
                                alrm->Tec1tmpThr = parseAlrmValFloat(token2);
                            }
                            else if (strstr((char *) token, "ALRM TEC1TMP HYS") != NULL)
                            {   //resCounter++;
                                alrm->Tec1tmpHys = parseAlrmValFloat(token2);
                            }
                            else if (strstr((char *) token, "ALRM TEC2TMP STA") != NULL)
                            {   //resCounter++;
                                alrm->Tec2tmpSta = parseAlrmValStatus(token2);
                            }
                            else if (strstr((char *) token, "ALRM TEC2TMP SST") != NULL)
                            {   //resCounter++;
                                alrm->Tec2tmpSst = parseAlrmValStatus(token2);
                            }
                            else if (strstr((char *) token, "ALRM TEC2TMP THR") != NULL)
                            {   //resCounter++;
                                alrm->Tec2tmpThr = parseAlrmValFloat(token2);
                            }
                            else if (strstr((char *) token, "ALRM TEC2TMP HYS") != NULL)
                            {   //resCounter++;
                                alrm->Tec2tmpHys = parseAlrmValFloat(token2);
                            }
                        }
                        break;
                    }

                    token = strtok_r(NULL, newLineDelim, &end_str);
                }
                else
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                }
            }
            if (resCounter != 12)
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            }
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

static int OtdrDumpTraceInternal ( T_DUMPTRACE *dumpTrace, const char *command )
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_LARGE_RESP_LENGTH;
    char *end_str = NULL;
    char newLineDelim[] = "\n";
    char commaDelim[] = ",";
    char *end_token = NULL;
    char *token = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s - %s]", __func__, command);

    if (dumpTrace != 0)
        memset(dumpTrace, 0, sizeof(T_DUMPTRACE));
    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    int8_t * const l_i8_Resp = (int8_t *)malloc(OTDR_API_LARGE_RESP_LENGTH * sizeof(int8_t));
    if (l_i8_Resp == NULL)
    {   l_i_retstatus = OTDR_MEMORY_ALLOCATION_FAILED;
        return l_i_retstatus;
    }
    memset(l_i8_Resp, 0, OTDR_API_LARGE_RESP_LENGTH*sizeof(l_i8_Resp[0]));

    do
    {

        if (dumpTrace != NULL)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                     command, CRLF);

            /** Call the setget command*/
            l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                                   (uint8_t *) l_i8_command,
                                                   strlen((const char*) l_i8_command), (uint8_t *) l_i8_Resp,
                                                   &l_ui_respLen, timeout);

            if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
                break;
            }
            l_i_retstatus = responseErrorCheck(l_i8_Resp);
            if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
            {   break;
            }
            // count number of lines in response
            const char *line = l_i8_Resp;
            int nlines = 0;
            for (const char *p = strchr(line, '\n'); p != NULL; p = strchr(line, '\n'))
            {   if (line[0] == '!')
                {   nlines = -1; // error
                    break;
                }
                nlines++;
                line = p + 1;
            }
            if (nlines <= 0)
            {   l_i_retstatus = OTDR_NOT_DEFINED;
                break;
            }
            float * const ybuf = (float *)malloc(nlines * sizeof(float));
            if (ybuf == NULL)
            {   l_i_retstatus = OTDR_MEMORY_ALLOCATION_FAILED;
                break;
            }

              if(strstr(l_i8_Resp, "?No valid data"))
              {
                l_i_retstatus =  OTDR_LIVE_INVALID_DATA;
                break;
              }
            // parse lines with floating-point numbers
            int npoints = 0;
            float x0 = NAN;
            float xf = NAN;

            token = strtok_r(l_i8_Resp, newLineDelim, &end_str);
            while (token != NULL)
            {   if (strstr(token, "dump trace") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;
                }
                else if (strstr((char *) token, "!") != NULL)
                {   l_i_retstatus = OTDR_NOT_DEFINED;
                    break;
                }
               
                else if (strstr((char *) token, "km") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;
                }
                else if (strstr((char *) token, "x") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;
                }
                else if (strstr((char *) token, ",") != NULL)
                {   char *token2 = strtok_r(token, commaDelim, &end_token);
                    while (token2 != NULL)
                    {   if (npoints == 0)
                            x0 = strtof(token2, NULL);
                        else
                            xf = strtof(token2, NULL);
                        token2 = strtok_r(NULL, commaDelim, &end_token);
                        if(token2 != NULL)
                            ybuf[npoints++] = strtof(token2, NULL);
                        break;
                    }
                }
                token = strtok_r(NULL, newLineDelim, &end_str);
            }
            if (npoints >= 2)
            {   dumpTrace->points = npoints;
                dumpTrace->windowOffset = x0;
                dumpTrace->dx = (xf - x0) / (npoints - 1);
                dumpTrace->power = ybuf;
            }
            else
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
                free(ybuf);
            }
        }
        else
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: NULL POINTER Detected!!!]");
            l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }
    }
    while (0);

    if (l_i8_Resp != NULL)
    {   free (l_i8_Resp);
    }

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;

}

/**
* @brief This function return OTDR x dump zmm data
*
* @param Output trace parameters and trace data as a T_DUMPTRACE struct
* @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
*
* The power array is allocated by the library and deallocated by the caller.
*/
static int OtdrxDumpZmm(T_DUMPTRACE *dumpTrace)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_LARGE_RESP_LENGTH;
    char *end_str = NULL;
    char newLineDelim[] = "\n";
    char commaDelim[] = ",";
    char *end_token = NULL;
    char *token = NULL;
    char *token2 = NULL;
    uint32_t l_ui32_value = 0;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s - %s]", __func__, OTDR_X_DUMP_ZMM);

    if (dumpTrace != 0)
        memset(dumpTrace, 0, sizeof(T_DUMPTRACE));

    /** call the OTDR KEY 1 API */
    l_i_retstatus = OtdrKey1();
    if (l_i_retstatus != 0)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: OtdrKey1: %d]", l_i_retstatus);
        l_i_retstatus = OTDR_COMMAND_FAIL;
        return l_i_retstatus;
    }

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    int8_t * const l_i8_Resp = (int8_t *)malloc(OTDR_API_LARGE_RESP_LENGTH * sizeof(int8_t));
    if (l_i8_Resp == NULL)
    {   l_i_retstatus = OTDR_MEMORY_ALLOCATION_FAILED;
        return l_i_retstatus;
    }
    memset(l_i8_Resp, 0, OTDR_API_LARGE_RESP_LENGTH*sizeof(l_i8_Resp[0]));

    do
    {

        if (dumpTrace != NULL)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                     OTDR_X_DUMP_ZMM, CRLF);

            /** Call the setget command*/
            l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                                   (uint8_t *) l_i8_command,
                                                   strlen((const char*) l_i8_command), (uint8_t *) l_i8_Resp,
                                                   &l_ui_respLen, timeout);

            if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
                break;
            }

            l_i_retstatus = responseErrorCheck(l_i8_Resp);
            if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
            {   break;
            }

            // count number of lines in response
            const char *line = l_i8_Resp;
            int nlines = 0;
            for (const char *p = strchr(line, '\n'); p != NULL; p = strchr(line, '\n'))
            {   if (line[0] == '!')
                {   nlines = -1; // error
                    break;
                }
                nlines++;
                line = p + 1;
            }
            if (nlines <= 0)
            {   l_i_retstatus = OTDR_NOT_DEFINED;
                break;
            }
            float * const ybuf = (float *)malloc(nlines * sizeof(float));
            if (ybuf == NULL)
            {   l_i_retstatus = OTDR_MEMORY_ALLOCATION_FAILED;
                break;
            }
            float * const xbuf = (float *)malloc(nlines * sizeof(float));
            if (ybuf == NULL)
            {   l_i_retstatus = OTDR_MEMORY_ALLOCATION_FAILED;
                break;
            }
            // parse lines with floating-point numbers
            int npoints = 0;

            token = strtok_r(l_i8_Resp, newLineDelim, &end_str);
            while (token != NULL)
            {   if (strstr(token, "zmm") != NULL)				//Ignore this parameter from the response
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;
                }
                else if (strstr((char *) token, "!") != NULL)
                {   l_i_retstatus = OTDR_NOT_DEFINED;
                    break;
                }
                else if (strstr((char *) token, "pulseWidth") != NULL)	//Ignore this parameter from the response
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;
                }
                else if (strstr((char *) token, "noiseLevel") != NULL)	//Ignore this parameter from the response
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                else if (strstr((char *) token, "iirTau") != NULL)		//Ignore this parameter from the response
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                else if (strstr((char *) token, "x0") != NULL)	//Check for the distance of first point in km in response, Value is in HEX
                {   token2 = strstr(token, commaDelim);
                    if(token2)
                    {   sscanf((const char *) &token2[1], "%x", &l_ui32_value);
                        dumpTrace->windowOffset = *((float*)&l_ui32_value);
                    }
                }
                else if (strstr((char *) token, "dx") != NULL)	//Check for the distance between adjacent points in km in response, Value is in HEX
                {   token2 = strstr(token, commaDelim);
                    if(token2)
                    {   sscanf((const char *) &token2[1], "%x", &l_ui32_value);
                        dumpTrace->dx = *((float*)&l_ui32_value);
                    }
                }
                else if (strstr((char *) token, "n") != NULL)	//Check for the  in response, Value is in HEX
                {   token2 = strstr(token, commaDelim);
                    if(token2)
                    {   sscanf((const char *) &token2[1], "%x", &(dumpTrace->points));
                    }
                }
                else
                {   /** Fill the data points to the buffer*/
                    if(dumpTrace->points)
                    {   sscanf((const char *) &token[0], "%x", &l_ui32_value);
                        ybuf[npoints] = *((float*)&l_ui32_value);
                        npoints++;
                    }
                }
                token = strtok_r(NULL, newLineDelim, &end_str);	//Check for the Next Line
            }
            if (dumpTrace->points < 2)
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
                free(ybuf);
            }
            else
            {   dumpTrace->power = ybuf;
            }
        }
        else
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: NULL POINTER Detected!!!]");
            l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }
    }
    while (0);

    if (l_i8_Resp != NULL)
    {   free (l_i8_Resp);
    }

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);
    return l_i_retstatus;


}
/**
 * @brief This function return OTDR trace data
 *
 * @param dumpTrace output trace data as a T_DUMPTRACE struct
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 * The power member in T_DUMPTRACE is dynamically allocated by the library on success and must be freed by the caller.
 */
int OtdrDumpTrace(T_DUMPTRACE *dumpTrace)
{     return OtdrDumpTraceInternal(dumpTrace, OTDR_DUMP_TRACE);    
}

/**
 * @brief This function return OTDR trace data in linear format
 *
 * @param dumpTrace output trace data as a T_DUMPTRACE struct
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 * The power member in T_DUMPTRACE is dynamically allocated by the library on success and must be freed by the caller.
 */
int OtdrDumpTraceLinear(T_DUMPTRACE *dumpTrace)
{   return OtdrDumpTraceInternal(dumpTrace, OTDR_DUMP_LTRACE);
}

/**
 * @brief This function return OTDR event table
 *
 * @param dumpEvent output event table as a T_DUMPEVENT struct
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 * The T_DUMPEVENT structure and T_DUMPEVENTENTRY array (of max 128 events) are allocated by the invoking application.
 */
int OtdrDumpEvent(T_DUMPEVENT *event_table)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_MEDIUM_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_MEDIUM_RESP_LENGTH;
    T_OTDRRESULT result = {0};
    int l_i_isOkHR = 0;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if (event_table != NULL)
        memset(event_table, 0, sizeof(T_DUMPEVENT));
    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   if (event_table == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_DUMP_EVENT, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command,
                                               strlen((const char*) l_i8_command), (uint8_t *) l_i8_Resp,
                                               &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        l_i_retstatus = OtdrResult(&result);
        if (l_i_retstatus)
        {   break;
        }
        else
        {   if (result.code == OTDR_RESULT_OK_HR)
            {   l_i_isOkHR = 1;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[OTDR result is OK HR]");
            }
        }

        // count number of lines in response
        const char *line = l_i8_Resp;
        int nlines = 0;
        for (const char *p = strchr(line, '\n'); p != NULL; p = strchr(line, '\n'))
        {   if (line[0] == '!')
            {   nlines = -1; // error
                break;
            }
            nlines++;
            line = p + 1;
        }

        if (l_i_isOkHR)
        {   nlines+=1;
        }

        if (nlines < 0)
        {   l_i_retstatus = OTDR_NOT_DEFINED;
            break;
        }
        T_DUMPEVENTENTRY * const ebuf = (T_DUMPEVENTENTRY *)malloc(nlines * sizeof(T_DUMPEVENTENTRY));
        if (ebuf == NULL)
        {   l_i_retstatus = OTDR_MEMORY_ALLOCATION_FAILED;
            break;
        }
        // parse event table entries
        int nevents = 0;

        if (l_i_isOkHR)
        {   T_DUMPEVENTENTRY e;
            e.type = OTDR_EVENT_TYPE_HIGH_REF;
            e.x_km = result.hrLocation;
            e.loss_u_km = NAN;
            e.refl_u_km = NAN;
            e.loss_dB = NAN;
            e.refl_dB = INFINITY;
            e.attn_dB_km = NAN;
            e.c_loss_dB = NAN;

            ebuf[nevents++] = e;
        }

        for (const char *p = l_i8_Resp; *p != '\0'; )
        {   const char * const q = strchr(p, '\n');
            if (q == NULL)
            {   if(!l_i_isOkHR)
                {   nevents = -1;
                }
                break;
            }
            T_DUMPEVENTENTRY e;
            e.c_loss_dB = NAN;
            const int c = sscanf(p, "%u,%f,%f,%f,%f,%f,%f,%f", &e.type, &e.x_km, &e.loss_u_km, &e.refl_u_km, &e.loss_dB, &e.refl_dB, &e.attn_dB_km, &e.c_loss_dB);
            if (c >= 7)
            {   if (nevents == nlines)
                {   if(!l_i_isOkHR)
                    {   nevents = -1;
                    }
                    break;
                }
                ebuf[nevents++] = e;
            }
            else if (c != 0)
            {   if(!l_i_isOkHR)
                {   nevents = -1;
                }
                break;
            }
            p = q + 1;
        }
        if (nevents >= 0)
        {   event_table->n = nevents;
            event_sort(ebuf,nevents);
            event_table->e = ebuf;
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            free(ebuf);
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;

}

/**
 * @brief This function call the RECV FW Command
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrRecvFirmware(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_RECEIVE_FIRMWARE, CRLF);

        /** Call the setget command*/
        l_i_retstatus = uartOtdrReadWrite(otdrcomm_handle,
                                          (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                          (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function call the RECV HW Command
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrRecvHardware(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_RECEIVE_HARDWARE, CRLF);

        /** Call the setget command*/
        l_i_retstatus = uartOtdrReadWrite(otdrcomm_handle,
                                          (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                          (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function call the KEY 1 Command
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */

int OtdrKey1(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_KEY_1, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function hardware boots the OTDR.
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */

int OtdrHWBoot(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_HWBOOT, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  uartOtdrReadWrite(otdrcomm_handle,
                                           (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                           (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function reboots the OTDR.
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */

int OtdrReboot(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_BOOT, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

int OtdrUpdateProductInfo()
{   FILE *fp = NULL;
    char buffer[BUF] = {0};
    char *buf = NULL;
    char ver[64] = {0};
    int  ret_api = 0;
    char l_c_otdr_ver[64] = {0};
    char l_c_otdr_FPGAver[64] = {0};
    T_VER_FULL otdr_ver = {0};

    json_bool ret_json_val = 0;
    struct json_object *parsed_json = NULL;
    struct json_object *parsed_json1 = NULL;
    struct json_object *root = NULL;
    struct json_object *subroot = NULL;
    struct json_object *subroot1 = NULL;
    struct json_object *leaf = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    do
    {   /* opening file & parsing string to json object*/
        fp = fopen(PROD_INFO_FILE_PATH,"r+");
        if(fp == NULL)
        {   ret_api = OTDR_NULL_POINTER;
            break;
        }

        fread(buffer, BUF, 1, fp);

        root = json_tokener_parse(buffer);
        if(root == NULL)
        {   ret_api = OTDR_JSON_PARSE_ERROR;
            break;
        }

        do
        {   ret_json_val = json_object_object_get_ex(root, "XMFH", &subroot);
            if(!ret_json_val)
            {   ret_api = OTDR_JSON_PARSE_ERROR;
                break;
            }

            buf = strdup(json_object_get_string(subroot));

            /* Parsing subobject */
            parsed_json = json_tokener_parse(buf);
            if (buf != NULL)
            {   free(buf);
                buf = NULL;
            }
            /************ Update version for OTDR ************/
            ret_api = json_object_object_get_ex(subroot, "OTDR", &subroot1);
            if(!ret_api)
            {   ret_api = OTDR_JSON_PARSE_ERROR;
                break;
            }
            buf = strdup(json_object_get_string(subroot1));

            /* Parsing subobject */
            parsed_json1 = json_tokener_parse(buf);
            if (buf != NULL)
            {   free(buf);
                buf = NULL;
            }

            do
            {   /*Getting Firmware version from the OTDR*/
                ret_api = OtdrGetVerFull(&otdr_ver);
                if(ret_api)
                {   break;
                }

                if(strlen(otdr_ver.version) < sizeof(l_c_otdr_ver) && strlen(otdr_ver.designversion) < sizeof(l_c_otdr_FPGAver))
                {   strncpy(l_c_otdr_ver, otdr_ver.version, strlen(otdr_ver.version));
                    otdr_ver.version[strlen(otdr_ver.version)] = 0;
                    strncpy(l_c_otdr_FPGAver, otdr_ver.designversion, strlen(otdr_ver.designversion));
                    otdr_ver.designversion[strlen(otdr_ver.designversion)] = 0;
                }

                /* Add version details in leaf */
                leaf=json_object_new_string (l_c_otdr_ver);
                /* Adding change in respective subobject */
                json_object_object_add(parsed_json1, "Firmware Ver",leaf);

                /* Add version details in leaf */
                leaf=json_object_new_string (l_c_otdr_FPGAver);
                /* Adding change in respective subobject */
                json_object_object_add(parsed_json1, "FPGA Ver",leaf);
            }
            while(0);
            /* Adding change in root object */
            json_object_object_add(parsed_json, "OTDR", parsed_json1);
            /* Adding change in root object */
            json_object_object_add(root, "XMFH", parsed_json);
        }
        while(0);

        strcpy(buffer, json_object_get_string(root)); //json object convert into string and copy to buffer

        if (NULL != fp)
            fclose(fp);
        fp = fopen(PROD_INFO_FILE_PATH,"w+");
        if(fp == NULL)
        {   ret_api = errno;
            break;
        }
        fwrite(buffer, 1, strlen(buffer), fp); //write in file  with new changes
    }
    while(0);

    if (NULL != fp)
        fclose(fp);
    if (buf != NULL)
        free(buf);
    if(parsed_json!= NULL)
        json_object_put(parsed_json);
    if(parsed_json1!= NULL)
        json_object_put(parsed_json1);
    if(root!= NULL)
        json_object_put(root);
    if(subroot!= NULL)
        json_object_put(subroot);
    if(subroot1!= NULL)
        json_object_put(subroot1);
    if(leaf!= NULL)
        json_object_put(leaf);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: %d]", __func__, ret_api);

    return ret_api;
}

/**
 * @brief This functions return file name extension
 *
 * @param OTDR firmware file name
 * @return  On success returns 0 and error code otherwise.
 */
const char *get_filename_ext(const char *filename)
{   const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

/**
 * @brief This function power cycle OTDR
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OTDRPowerCycle(void)
{   int l_i_status = -1;
    int count = 0;
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    do
    {   l_i_status = OTDRPower(OFF);
        if (l_i_status)
        {   l_i_status = OTDR_POWER_CYCLE_FAILED;
            break;
        }
        l_i_status = OTDRPower(ON);
        if (l_i_status)
        {   l_i_status = OTDR_POWER_CYCLE_FAILED;
            break;
        }
        while(1)
        {   l_i_status = getOTDRStatus();
            if(POWERMGT_DEVICE_ON == l_i_status)
            {   l_i_status = OTDR_COMMAND_SUCCESS;
                break;
            }
            else if (POWERMGT_DEVICE_OFF == l_i_status)
            {   l_i_status = OTDR_POWER_CYCLE_FAILED;
                break;
            }
            else if(count > 3 || POWERMGT_DEVICE_FAILED_TO_INITIALIZE == l_i_status) //Re-try for 3 iterations if otdr is in powering up status
            {
                l_i_status = POWERMGT_DEVICE_FAILED_TO_INITIALIZE;
                break;
            }
            count++;
            usleep(100000);
        }
    }
    while(0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Count: %d: Return Value: %d]", __func__, count, l_i_status);

    return l_i_status;
}

/**
 * @brief This function get the otdr boot mode status.
 * @param mode accepts 0 and 1 *           
 *          1: Check for normal mode
 *          0: Check for boot mode
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrBootModeStatus(T_OTDREXECMODE mode)
{
    uint8_t l_i8_Resp[512] = { 0 };
    uint32_t l_ui_respLen = 512;
    int l_i_status = OTDR_COMMAND_SUCCESS, rc = 0, otdrcomm_handle =0;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);
    
    /**** Initialize the serial port to communicate over uart ******/
    if (access("/tmp/lock/ttyS0.LCK", F_OK) == 0)  
        remove("/tmp/lock/ttyS0.LCK");

    rc = serialPortInit(OTDR_SERIALNODE, &otdrcomm_handle);
    if ((rc == 0) && !(otdrcomm_handle < 0))                            
    {
        switch(mode)
        {
            case OTDR_BOOT_MODE: //Check for $ (boot) mode 
                    l_i_status = 1;
                    rc = serialOtdrReadWrite(otdrcomm_handle,"ver\r\n", strlen("ver\r\n"),l_i8_Resp,&l_ui_respLen,300000);
                    if ((rc != 0) && (l_ui_respLen <= 0))
                    { 
                        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[In boot mode: Serial communication with OTDR is failed]");  
                        l_i_status = 1;
                        break;                                    
                    }
                    else if(strstr(l_i8_Resp, "$") != NULL)
                    {
                        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[In boot mode]");
                        l_i_status = OTDR_COMMAND_SUCCESS;
                    }
                    break;
                    
            case OTDR_ACTIVE_MODE: // Check for > (normal) mode
                    l_i_status = 1;
                    rc = serialOtdrReadWrite(otdrcomm_handle,"ver\r\n", strlen("ver\r\n"),l_i8_Resp,&l_ui_respLen,300000);
                    if ((rc != 0) && (l_ui_respLen <= 0))
                    { 
                        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[In normal mode: Serial communication with OTDR is failed]");  
                        l_i_status = 1;
                        break;                                    
                    }
                    else if(strstr(l_i8_Resp, "OTDR") != NULL)
                    {
                        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[In normal mode]");
                        l_i_status = OTDR_COMMAND_SUCCESS;
                    }
                    break;
            default :
                         l_i_status = 1;
                         LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[In boot mode: Serial communication with OTDR is failed]");            
        }     
        
        serialPortDeInit(OTDR_SERIALNODE, otdrcomm_handle);                                
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_status);

    return l_i_status;
    
}

/**
 * @brief This function issue reinit command to the OTDR.
 * @param mode accepts 0 and 1 *           
 *          1: Check for emu_init lock file
 *          0: Check for emu server socket file
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrEmuReinitStatus(T_OTDRINITCHECK mode)
{
    int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    struct dirent *d;
    DIR *dh;
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s, mode: %d]", __func__, mode);
    switch (mode)
    {
        case OTDR_SOCKET_CHECK: dh = opendir("/tmp/");
                l_i_retstatus = 1;
                if (dh)
                {
                    while ((d = readdir(dh)) != NULL)
                    {
                        if ( strstr(d->d_name,"ii_vi_otdr_emu") !=NULL)
                        {                    
                            l_i_retstatus = 0;
                            break;
                        }
                    }
                }
                break;
        case OTDR_EMU_STATUS_CHECK:
                l_i_retstatus = OTDR_COMMAND_SUCCESS;
                uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
                int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
                int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
                uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;      
                do
                {
                    /** Prepare command */
                    snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                            OTDR_VER, CRLF);

                    /** Call version command to confirm emu is ready */
                    l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                                        (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                                        (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);
                    if (((l_i_retstatus != 0) && (l_ui_respLen <= 10)) || (strstr(l_i8_Resp, "IIVI") == NULL))
                    { 
                        l_i_retstatus = 1;
                        break;
                    } 

                    /** Prepare command */
                    snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                            OTDR, CRLF);

                    /** Call parameters command to confirm otdr is ready*/
                    l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                                        (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                                        (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);
                    if (((l_i_retstatus != 0) && (l_ui_respLen <= 10)) || (strstr(l_i8_Resp, "EVENT") == NULL))
                    { 
                        l_i_retstatus = 1;
                        break;
                    }

                }while(0);
                break;
        default :
                l_i_retstatus = 1;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Invalid case : %d]", mode);

    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);
    return l_i_retstatus;    
}

/**
 * @brief This function issue reinit command to the OTDR.
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrEmuReinit(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    do{
        /****Stop otdr emu with system command ****/
        int l_i_maxEmuSeconds = 10;
        system("/etc/init.d/otdr_emu.sh stop"); 
        /****Check for old socket file is delted successfully ****/       
        do{
            usleep(1000000);    
            l_i_maxEmuSeconds--;
        }while(!OtdrEmuReinitStatus(OTDR_SOCKET_CHECK) && l_i_maxEmuSeconds > 0);
        
        if (l_i_maxEmuSeconds <= 0)
        {
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:emu stop is NOT successful]");
            l_i_retstatus = OTDR_COMMAND_FAIL;
            break;
        }
        else
        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:emu stop is successful]");
        usleep(1000000);
        /****Start otdr emu with system command ****/
        system("/etc/init.d/otdr_emu.sh start");
        /****Check for new socket file is created successfully ****/
        do{
            usleep(1000000);    
            l_i_maxEmuSeconds--;
        }while(OtdrEmuReinitStatus(OTDR_SOCKET_CHECK) && l_i_maxEmuSeconds > 0);
        
        if (l_i_maxEmuSeconds <= 0)
        {
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:emu start is NOT successful]");
            l_i_retstatus = OTDR_COMMAND_FAIL;
            break;
        }
        else
        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:emu start is successful]");  
    }while(0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function issue B command to the OTDR.
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrB(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if(external_otdr_flag)
        system("echo 'B' >/dev/ttyS0");
    else
    {
        do
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                    OTDR_B, CRLF);

            /** Call the setget command*/
            l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                                (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                                (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

            if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
                break;
            }
            l_i_retstatus = responseErrorCheck(l_i8_Resp);
        }
        while (0);
    }

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
/**
 * @brief This function issue RH command to the OTDR.
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrRH(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_MEDIUM_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_MEDIUM_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_RH, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  uartOtdrReadWrite(otdrcomm_handle,
                                           (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                           (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

int UpgradePoweringOnTimer(void)
{   int l_i_status = -1;
    int l_i_retryCnt = 0;
    float l_f_Mt = 0;
    int l_i_maxRetry = 10;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    do
    {   l_i_status = OtdrGetMt(&l_f_Mt);
        if (OTDR_COMMAND_SUCCESS == l_i_status)
        {   if((FP_NORMAL == fpclassify(l_f_Mt)))
            {   break;
            }
        }
        if (OTDR_CORRUPTION_ERROR == l_i_status)
        {   break;
        }
        l_i_retryCnt++;

        usleep(500000);
    }
    while(l_i_retryCnt <= l_i_maxRetry);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_status);

    return l_i_status;
}

/**
* @brief The function Upgrades the OTDR Firmware.
*
* @param  i_ui8_fileName OTDR Firmware file name, which is expected at /tmp/ location.
*
* @return On success returns OTDR_COMMAND_SUCCESS and error code otherwise. (OTDR_FILE_NOT_SUPPORTED in case
* OTDR is corrupted and other than OHW file is supplied)
*/
int OtdrFirmwareUpgrade(uint8_t *i_ui8_fileName)
{   int l_i8_retStatus = OTDR_COMMAND_SUCCESS;
    char l_c_file[256] = { 0 };
    char ext[8] = {0};

    /**1. Force Power ON OTDR in case OTDR is not Powered ON
     * 2. Check OTDR Corruption
    **/
    /** The hardware recovery process in case OTDR corruption (OHW) */
    /**
     * 1. call the RH API
     * 2. call the OtdrSendFile API
     * 3. call the B API
     * */
    /** The firmware upgrade process in case normal mode (BIN BUNDLE OHW) */
    /**
     * 1. call the echo off API
     * 2. call the RECV FW API / (call the Key 1 API and call the RECV HW API)
     * 3. call the OtdrSendFile API
     * 4. call the HWBOOT API
     * */
    /**
     * Common
     * 1. Ensure OTDR is Powered ON
     * 2. Update the productInfo File
     **/

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s File name: %s]", __func__, i_ui8_fileName);

    do
    {   if (NULL != i_ui8_fileName)
        {   if (strstr(i_ui8_fileName, "hwrev") != NULL)
            {   snprintf(l_c_file, 256, "%s", i_ui8_fileName);
            }
            else
            {   snprintf(l_c_file, 256, "%s%s", OTDR_BINARY_FILE_PATH, i_ui8_fileName);
            }
        }
        else
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: File Path NULL received: %s]", i_ui8_fileName);
            l_i8_retStatus = OTDR_NULL_POINTER;
            break;
        }

        //Check OTDR firmware file exist at location or not
        if (access((char *) l_c_file, F_OK) == -1)
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: Invalid file path given in OTDR FW Upgrade: %s]", i_ui8_fileName);
            l_i8_retStatus = OTDR_INVALID_INPUT;
            break;
        }

        if (OTDR_NORMAL_MODE != g_i_lgrOtdrMode)
        {   l_i8_retStatus = OTDR_INVALID_MODE_INTITIALIZED;
            break;
        }

        /* Check this flag for kl02 firmware upgrader */
        if (access((char *) KL02_FW_UPGRADE_INDICATOR, F_OK) == 0)
        {   l_i8_retStatus = FREESCALE_FW_UPGRADE_INPROGRESS;
            break;
        }

        int ext_uart_st = 0;
        isExternalOTDR(&ext_uart_st); /*Check for external OTDR*/
        if(ext_uart_st) /*External OTDR (XMF-HD)mode*/
        {   int ret_fn = 0;
            /** sfp Initialization **/
                if (ret_fn = SFPInterfaceInit() != 0)
                {   printf("ERROR: SFPinit Error no: %d\n", ret_fn);
                    break; ;
                }
                else
                {   printf("SFP Initialized Successfully\n");
                }

                if (POWERMGT_DEVICE_ON == getOTDRStatus())
                {
                    if (l_i8_retStatus = OTDRPowerForce(OFF))
                    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error:Internal OTDR is failed to power off: %d]", l_i8_retStatus);
                        l_i8_retStatus = OTDR_COMMAND_FAIL;
                        break;
                    }
                }

                if (POWERMGT_DEVICE_ON != getSFPStatus())
                {
                    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error:External OTDR is power off: %d]", l_i8_retStatus);
                    l_i8_retStatus = OTDR_COMMAND_FAIL;
                    break;
                }

        }
        else if (POWERMGT_DEVICE_ON != getOTDRStatus())
        {   if (l_i8_retStatus = OTDRPowerForce(ON))
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: OTDR is failed to power on: %d]", l_i8_retStatus);
                l_i8_retStatus = OTDR_COMMAND_FAIL;
                break;
            }
        }

        snprintf(ext, 8, "%s", get_filename_ext(i_ui8_fileName));

        if ((strcmp((char *)ext,(char *)"ohw") != 0) && (strcmp((char *)ext,(char *)"bin") != 0) && (strcmp((char *)ext,(char *)"bundle") != 0))
        {   l_i8_retStatus = OTDR_INVALID_INPUT;
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: Only BIN, BUNDLE and OHW files are supported: %d]", l_i8_retStatus);
            break;
        }

        /* Check OTDR is on */
        l_i8_retStatus = UpgradePoweringOnTimer();
        if (l_i8_retStatus && l_i8_retStatus != OTDR_CORRUPTION_ERROR)
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: Pre UpgradePoweringOnTimer: %d]", l_i8_retStatus);
            l_i8_retStatus = OTDR_COMMAND_FAIL;
            break;
        }

        if ((l_i8_retStatus == OTDR_CORRUPTION_ERROR) && (strcmp((char *)ext,(char *)"ohw") != 0))
        {   l_i8_retStatus = OTDR_FILE_NOT_SUPPORTED;
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: wrong file supplied. supply OHW in case OTDR is corrupted: %d]", l_i8_retStatus);
            break;
        }

        if ((l_i8_retStatus == OTDR_CORRUPTION_ERROR) && (strcmp((char *)ext,(char *)"ohw") == 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[OTDR is corrupted: %d]", l_i8_retStatus);

            l_i8_retStatus = OtdrRH();
            if (l_i8_retStatus != 0)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: OtdrRH: %d]", l_i8_retStatus);
                l_i8_retStatus = OTDR_COMMAND_FAIL;
                break;
            }

            l_i8_retStatus = OtdrSendFile(l_c_file);
            if (l_i8_retStatus != 0)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: OtdrSendFile: %d]", l_i8_retStatus);
                l_i8_retStatus = OTDR_COMMAND_FAIL;
                break;
            }

            l_i8_retStatus = OtdrB();
            if (l_i8_retStatus != 0)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: OtdrB: %d]", l_i8_retStatus);
                l_i8_retStatus = OTDR_COMMAND_FAIL;
                break;
            }
        }
        else
        {   /********************************* Step1 ***************************/

            /** call the echo off API */
            int echoVal = 0;
            l_i8_retStatus = OtdrSetEcho(echoVal);
            if ((l_i8_retStatus) && (l_i8_retStatus != OTDR_CORRUPTION_ERROR))
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: OtdrSetEcho: %d]", l_i8_retStatus);
                l_i8_retStatus = OTDR_COMMAND_FAIL;
                break;
            }
            /********************************* Step2 ***************************/

            if (strcmp((char *)ext,(char *)"ohw") == 0)
            {   /** call the OTDR KEY 1 API */
                l_i8_retStatus = OtdrKey1();
                if (l_i8_retStatus != 0)
                {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: OtdrKey1: %d]", l_i8_retStatus);
                    l_i8_retStatus = OTDR_COMMAND_FAIL;
                    break;
                }

                /** call the RECV FW API */
                l_i8_retStatus = OtdrRecvHardware();
            }
            else
            {   /** call the RECV HW API */
                l_i8_retStatus = OtdrRecvFirmware();
            }

            if (l_i8_retStatus != 0)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: OtdrRecvFirmware/OtdrRecvHardware: %d]", l_i8_retStatus);
                l_i8_retStatus = OTDR_COMMAND_FAIL;
                break;
            }
            /********************************* Step3 ***************************/
            /** call the OtdrSendFile API */

            l_i8_retStatus = OtdrSendFile(l_c_file);
            if (l_i8_retStatus != 0)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: OtdrSendFile: %d]", l_i8_retStatus);
                l_i8_retStatus = OTDR_COMMAND_FAIL;
                break;
            }
            /********************************* Step4 ***************************/
            /** Hardware boot OTDR */

            OtdrHWBoot();
        }

        /********************************* Step5 ***************************/
        /* Check OTDR is on */
        if (l_i8_retStatus = UpgradePoweringOnTimer())
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: PostUpgradePoweringOnTimer: %d]", l_i8_retStatus);
            l_i8_retStatus = OTDR_COMMAND_FAIL;
            break;
        }

        /** Update product information file */
        if(!l_i8_retStatus)
        {   if(l_i8_retStatus = OtdrUpdateProductInfo())
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[[Warning]: Failed to "
                         "update OTDR Firmware and FPGA version in Product Info File]");
            }
        }
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: %d]", __func__, l_i8_retStatus);

    return l_i8_retStatus;
}

/**
 * @brief This function reset all the settings
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrReset(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int reset_argument = 1;
    uint8_t l_aui8_cmdBuff[OTDR_API_SHORT_CMD_LENGTH] = { 0 };

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    snprintf(l_aui8_cmdBuff, sizeof(l_aui8_cmdBuff), "%s%s %d",
             OTDR_RESET_FILE_PATH, OTDR_RESET_FILE_NAME, reset_argument);

    system((const char *) l_aui8_cmdBuff);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function gets hardware version information
 *
 * @param ver_hw return hardware version information as a structure containing string elements. On return, each string is a null-terminated unsigned char string; trailing spaces are removed
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetHwVer(T_HWVER *verHw)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int retProcessResp = -1;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char newLineDelim[] = "\n";
    char colonDelim[] = ":";
    char *end_str = NULL;
    char *end_token = NULL;
    char *token2 = NULL;
    unsigned char *token = NULL;
    int resCounter = 0;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (verHw == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_VER_HW, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        /** Process the Command to get the Output*/
        if (l_i8_Resp != NULL)
        {

            token = strtok_r(l_i8_Resp, newLineDelim, &end_str);

            while (token != NULL)
            {   if (strstr(token, "ver hw") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                else if (strstr((char *) token, ":") != NULL)
                {   token2 = strtok_r(token, colonDelim, &end_token);
                    while (token2 != NULL)
                    {   token2 = strtok_r(NULL, colonDelim, &end_token);
                        if (NULL != token2)
                        {   if (strstr(token, "Design version") != NULL)
                            {   resCounter++;
                                strncpy((unsigned char *) verHw->design, token2, sizeof(verHw->design)-1);
                            }
                            else if (strstr(token, "Loader version") != NULL)
                            {   resCounter++;
                                strncpy((unsigned char *) verHw->loader, token2, sizeof(verHw->loader)-1);
                            }
                            else if (strstr(token, "SPI peripheral version") != NULL)
                            {   resCounter++;
                                strncpy((unsigned char *) verHw->spiPeripheral, token2, sizeof(verHw->spiPeripheral)-1);
                            }
                            else if (strstr(token, "System clock") != NULL)
                            {   resCounter++;
                                strncpy((unsigned char *) verHw->sysClock, token2, sizeof(verHw->sysClock)-1);
                            }
                            else if (strstr(token, "Scheduling period") != NULL)
                            {   resCounter++;
                                strncpy((unsigned char *) verHw->schedulePeriod, token2, sizeof(verHw->schedulePeriod)-1);
                            }
                            else if (strstr(token, "Data cache") != NULL)
                            {   resCounter++;
                                strncpy((unsigned char *) verHw->dataCache, token2, sizeof(verHw->dataCache)-1);
                            }
                            else if (strstr(token, "Inst cache") != NULL)
                            {   resCounter++;
                                strncpy((unsigned char *) verHw->instCache, token2, sizeof(verHw->instCache)-1);
                            }
                        }
                        break;
                    }

                    token = strtok_r(NULL, newLineDelim, &end_str);
                }
                else
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                }
            }
            if (resCounter != 7)
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            }
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function stops the current OTDR measurement. State can be checked using OtdrStatus()
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrStop(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_STOP, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function reset the configuration to factory defaults. The values do not change until after rebooting or power cycling.
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */

int OtdrFactoryReset()
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_RESET, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function re-analyzes the most recent OTDR measurement. State can be checked using OtdrStatus()
 *
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrAnalyze(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_ANALYZE, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function sets real-time clock date to x (format: yyyy-mm-dd)
 * and/or time to y (format: hh:mm:ss)
 *
 * @param time input time (format: hh:mm:ss) as a string
 * @param date input date (format: yyyy-mm-dd) as a string
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetRtc(char *time, char *date)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (NULL == time || NULL == date)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }
        else
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %s %s%s",
                     OTDR_RTC, date, time, CRLF);
        }

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function gets real-time clock date to x (format: yyyy-mm-dd)
 * and/or time to y (format: hh:mm:ss)
 * @param time return time (format: hh:mm:ss) as a string
 * @param date return date (format: yyyy-mm-dd) as a string
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetRtc(char *time, char *date)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    uint8_t expectedVal[10] = { };
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char *token = NULL;
    int8_t * l_i8_respPtr = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (time == NULL || date == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_RTC, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        if(NULL == (l_i8_respPtr =  strstr(l_i8_Resp,"RTC:")))
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
        else
        {   /** Process the Command to get the Output*/
            token = strtok((char *) l_i8_respPtr, ": ");
            if (NULL != token)
            {   token = strtok(NULL, "");
            }
            else
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
                break;
            }

            token = strtok((char *) token, " ");
            if (NULL != token)
            {   strcpy((char *) date, token);
                token = strtok(NULL, "");
                if(token !=  NULL)
                {   remove_spaces(token);
                    strcpy((char *) time, token);
                }
            }
            if (time == NULL || date == NULL)
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            }
        }

    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function displays module supply voltage, current, and/or power
 *
 * @param supply return module supply voltage, current, and/or power as a struct of float values
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetSupply(T_SUPPLY *supply)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int retProcessResp = -1;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char newLineDelim[] = "\n";
    char colonDelim[] = ":";
    char *end_str = NULL;
    char *token = NULL;
    char *end_token = NULL;
    char *token2 = NULL;
    int resCounter = 0;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {

        if (supply == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_SUPPLY, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        /** Process the Command to get the Output*/
        if (l_i8_Resp != NULL)
        {

            token = strtok_r(l_i8_Resp, newLineDelim, &end_str);

            while (token != NULL)
            {   if (strstr(token, "supply") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                else if (strstr((char *) token, ":") != NULL)
                {   token2 = strtok_r(token, colonDelim, &end_token);
                    while (token2 != NULL)
                    {   token2 = strtok_r(NULL, colonDelim, &end_token);
                        if (NULL != token2)
                        {   remove_spaces(token2);
                            if (strstr(token, "SUPPLY VOLTAGE") != NULL)
                            {   resCounter++;
                                supply->voltage = strtof(token2, NULL);
                            }
                            else if (strstr(token, "SUPPLY CURRENT") != NULL)
                            {   resCounter++;
                                supply->current = strtof(token2, NULL);
                            }
                            else if (strstr(token, "SUPPLY POWER") != NULL)
                            {   resCounter++;
                                supply->power = strtof(token2, NULL);
                            }
                        }
                        break;
                    }

                    token = strtok_r(NULL, newLineDelim, &end_str);
                }
                else
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                }
            }
            if (resCounter != 3)
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            }
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function sets the threshold for reflection events to x dB
 * @param thr input values in dB
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetEventReflectionThreshold(float thr)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %f%s",
                 OTDR_EVENT_REFL_THR, thr, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function sets alarm status mode to normal (N) mode
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetAstmNormal(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_ASTM_NORMAL, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function sets alarm status mode to latching (S, for
 *	"sticky") mode
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrSetAstmSticky(void)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_ASTM_STICKY, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function get status mode to normal (N) or latching (S, for
 *	"sticky") mode
 *
 * @param mode return value as integer 1 for normal (N) or 2 for latching (S, for
 *	"sticky") mode
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetAstm(int *mode)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int retProcessResp = -1;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    uint8_t expectedVal[10] = { };
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char* token = NULL;
    int8_t * l_i8_respPtr = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   /** Check the thresold value from that*/
        if (mode == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;

        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_ASTM, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        if(NULL == (l_i8_respPtr = strstr(l_i8_Resp,"ASTM:")))
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
        else
        {   /** Process the Command to get the Output*/
            token = strtok(l_i8_respPtr, ":");

            // Keep printing tokens while one of the
            // delimiters present in str[].
            while (token != NULL)
            {   if (strstr(token, "N") != NULL)
                {   *mode = OTDR_ASTM_NORMAL_VALUE;
                }
                else if (strstr(token, "S") != NULL)
                {   *mode = OTDR_ASTM_STICKY_VALUE;
                }
                else
                {   l_i_retstatus = OTDR_PROCESSING_ERROR;
                }
                token = strtok(NULL, ":");
            }
        }

    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function lists all alarms whose status is on. If no alarms are on, the response is 0
 *
 * @return  On success returns 0 and error code otherwise.
 */
int OtdrGetAst(T_AST *ast)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int retProcessResp = -1;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    uint8_t expectedVal[10] = { };
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    int8_t *l_i8_respPtr = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   /** Check the thresold value from that*/
        if (ast == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;

        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_AST, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        if(NULL == (l_i8_respPtr = strstr(l_i8_Resp,"AST:")))
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
        else
        {   /** Process the Command to get the Output*/
            if (strstr((char *) l_i8_respPtr, "MTH") != NULL)
            {   ast->Mth = 1;
            }
            else if(strstr((char *) l_i8_respPtr, "MTX") != NULL)
            {   ast->Mtx = 1;
            }
            else if(strstr((char *) l_i8_respPtr, "MTL") != NULL)
            {   ast->Mtl = 1;
            }
            else if(strstr((char *) l_i8_respPtr, "TEC1TMP") != NULL)
            {   ast->Tec1tmp = 1;
            }
            else if(strstr((char *) l_i8_respPtr, "OK") != NULL)
            {   /* Ignore if no alarms set */
            }
            else
            {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            }
        }

    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**************************************************************************************************
 *
 * @brief   This function DISABLE ALRM 1 and ALRM 2
 *
 * @return  void
 *
 **************************************************************************************************/
void setupOTDRparams()
{   system("echo -ne \"\nalrm tec1tmp thr d\r\n\" > /dev/ttyS0");
    system("echo -ne \"\nalrm tec2tmp thr d\r\n\" > /dev/ttyS0");
}

/**************************************************************************************************
 *
 * @brief   This internal function get OTDR power status
 *
 * @param   o_b_status returns the OTDR status
 *
 * @return  On success, returns POWERMGT_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int getOTDRStatusInternal(void)
{   int l_i_Status = POWERMGT_SUCCESS;
    uint8_t l_ui8_pcf_data[2] = {0x00}; //pcf reads and write two bytes data only
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    uint8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_Status = checkOTDRPowerInit()) > 1)
    {   return l_i_Status;
    }
    else if(l_i_Status == 0)
    {   return (int)POWERMGT_DEVICE_COMMUNICATION_ERROR;
    }

    l_i_Status = POWERMGT_SUCCESS;

    snprintf((char *) l_i8_command, sizeof(l_i8_command), "otdrstatus");
    do
    {   /** Call the setget command*/
        l_i_Status =  (*powertransportReadWrite)(otdr_powercomm_handle,
                      (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                      (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if (l_i_Status == DEVICE_COMMUNICATION_ERROR)
        {   l_i_Status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr communication error in ReadWrite: %d]", l_i_Status);
            break;
        }
        else if (l_i_Status == DEVICE_READ_TIMEOUT)
        {   l_i_Status = POWERMGT_DEVICE_READ_TIMEOUT;
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr timeout in ReadWrite: %d]", l_i_Status);
            break;
        }
        else if ((l_i_Status != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_Status);
            break;
        }

        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[PCF Read Bytes: 0x%X, 0x%X]", l_i8_Resp[0], l_i8_Resp[1]);

        //Return Device power status
        if(l_i8_Resp[0]  == 0x01)
        {   l_i_Status = POWERMGT_DEVICE_OFF;
            break;
        }
        else if(l_i8_Resp[0] == 0x02)
        {   l_i_Status = POWERMGT_DEVICE_POWERING_UP;
            break;
        }
        else if(l_i8_Resp[0]  == 0x03)
        {   l_i_Status = POWERMGT_DEVICE_ON;
            break;
        }
        else if(l_i8_Resp[0]  == 0x04)
        {   l_i_Status = POWERMGT_DEVICE_FAILED_TO_INITIALIZE;
            break;
        }
    }
    while(0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_Status);

    return l_i_Status;
}
/***************************************************************************************************
*               PUBLIC FUNCTION
***************************************************************************************************/
/**************************************************************************************************
 *
 * @brief   This function get OTDR status
 *
 * @param   o_b_status returns the OTDR status
 *
 * @return  On success, returns POWERMGT_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int getOTDRStatus()
{   int l_i_Status = POWERMGT_SUCCESS;
    uint8_t l_ui8_pcf_data[2] = {0x00}; //pcf reads and write two bytes data only
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    uint8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_Status = checkOTDRPowerInit()) > 1)
    {   return l_i_Status;
    }
    else if(l_i_Status == 0)
    {   return (int)POWERMGT_DEVICE_COMMUNICATION_ERROR;
    }

    l_i_Status = POWERMGT_SUCCESS;

    snprintf((char *) l_i8_command, sizeof(l_i8_command), "otdrstatus");
    do
    {   /** Call the setget command*/
        l_i_Status =  (*powertransportReadWrite)(otdr_powercomm_handle,
                      (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                      (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if (l_i_Status == DEVICE_COMMUNICATION_ERROR)
        {   l_i_Status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr communication error in ReadWrite: %d]", l_i_Status);
            break;
        }
        else if (l_i_Status == DEVICE_READ_TIMEOUT)
        {   l_i_Status = POWERMGT_DEVICE_READ_TIMEOUT;
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr timeout in ReadWrite: %d]", l_i_Status);
            break;
        }
        else if ((l_i_Status != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_Status);
            break;
        }

        LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[PCF Read Bytes: 0x%X, 0x%X]", l_i8_Resp[0], l_i8_Resp[1]);

        //Return Device power status
        if(l_i8_Resp[0]  == 0x01)
        {   l_i_Status = POWERMGT_DEVICE_OFF;
            break;
        }
        else if(l_i8_Resp[0] == 0x02)
        {   l_i_Status = POWERMGT_DEVICE_POWERING_UP;
            break;
        }
        else if(l_i8_Resp[0]  == 0x03)
        {   l_i_Status = POWERMGT_DEVICE_ON;
            break;
        }
        else if(l_i8_Resp[0]  == 0x04)
        {   l_i_Status = POWERMGT_DEVICE_FAILED_TO_INITIALIZE;
            break;
        }
    }
    while(0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_Status);

    return l_i_Status;
}
/**************************************************************************************************
 *
 * @brief   This function POWER-ON/OFF OTDR as per the argument
 *
 * @param   i_e_switch, enum to the power status (ON/OF)
 *
 * @return  On success, returns POWERMGT_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int OTDRPower(E_POWERSWITCH i_e_switch)
{   int l_i_Status = POWERMGT_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s Value: %d]", __func__, i_e_switch);

    /* caller of OTDRPower() function first get the OTDR Power off/on status using getOTDRStatus()
     * Based on the return status, one should switch on & off the OTDR
     */
    if ((l_i_Status = checkOTDRPowerInit()) > 1)
    {   return l_i_Status;
    }
    else if(l_i_Status == 0)
    {   return (int)POWERMGT_DEVICE_COMMUNICATION_ERROR;
    }
    l_i_Status = POWERMGT_SUCCESS;

    do
    {   if(i_e_switch == ON)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "otdron");

            /** Call the setget command*/
            l_i_Status =  (*powertransportReadWrite)(otdr_powercomm_handle,
                          (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                          (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

            if (l_i_Status == DEVICE_COMMUNICATION_ERROR)
            {   l_i_Status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr communication error in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if (l_i_Status == DEVICE_READ_TIMEOUT)
            {   l_i_Status = POWERMGT_DEVICE_READ_TIMEOUT;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr timeout in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if ((l_i_Status != 0) && (l_ui_respLen <= 0))
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_Status);
                break;
            }

        }
        else if(i_e_switch == OFF)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "otdroff");

            /** Call the setget command*/
            l_i_Status =  (*powertransportReadWrite)(otdr_powercomm_handle,
                          (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                          (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

            if (l_i_Status == DEVICE_COMMUNICATION_ERROR)
            {   l_i_Status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr communication error in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if (l_i_Status == DEVICE_READ_TIMEOUT)
            {   l_i_Status = POWERMGT_DEVICE_READ_TIMEOUT;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr timeout in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if ((l_i_Status != 0) && (l_ui_respLen <= 0))
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_Status);
                break;
            }
        }
        else
        {   l_i_Status = POWERMGT_INVALID_INPUT;
            break;
        }
    }
    while(0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_Status);

    return l_i_Status;
}

/**************************************************************************************************
 *
 * @brief   This function POWER-ON OTDR forcefully.
 *
 * @return  On success, returns OTDR_COMMAND_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int OTDRPowerForce(E_POWERSWITCH i_e_switch)
{   int l_i_Status = POWERMGT_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s Value: %d]", __func__, i_e_switch);

    if ((l_i_Status = checkOTDRPowerInit()) > 1)
    {   return l_i_Status;
    }
    else if(l_i_Status == 0)
    {   return (int)POWERMGT_DEVICE_COMMUNICATION_ERROR;
    }
    l_i_Status = POWERMGT_SUCCESS;

    do
    {   if(i_e_switch == ON)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "otdrforceon");

            /** Call the setget command*/
            l_i_Status =  (*powertransportReadWrite)(otdr_powercomm_handle,
                          (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                          (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

            if (l_i_Status == DEVICE_COMMUNICATION_ERROR)
            {   l_i_Status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr communication error in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if (l_i_Status == DEVICE_READ_TIMEOUT)
            {   l_i_Status = POWERMGT_DEVICE_READ_TIMEOUT;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr timeout in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if ((l_i_Status != 0) && (l_ui_respLen <= 0))
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_Status);
                break;
            }

        }
        else if(i_e_switch == OFF)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "otdroff");

            /** Call the setget command*/
            l_i_Status =  (*powertransportReadWrite)(otdr_powercomm_handle,
                          (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                          (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

            if (l_i_Status == DEVICE_COMMUNICATION_ERROR)
            {   l_i_Status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr communication error in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if (l_i_Status == DEVICE_READ_TIMEOUT)
            {   l_i_Status = POWERMGT_DEVICE_READ_TIMEOUT;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr timeout in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if ((l_i_Status != 0) && (l_ui_respLen <= 0))
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_Status);
                break;
            }
        }
        else
        {   l_i_Status = POWERMGT_INVALID_INPUT;
            break;
        }
    }
    while(0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_Status);

    return l_i_Status;
}

int OtdrLiveRun()
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {  
         return l_i_retstatus;
    }
    
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;
    
    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_LIVE_RUN, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);
       
        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp);
         if(strstr(l_i8_Resp, "?Busy."))
        {
            l_i_retstatus = OTDR_LIVE_BUSY;
            break;
        }
         if(strstr(l_i8_Resp, "?In live scan mode."))
        {           
            l_i_retstatus = OTDR_LIVE_SCAN_ENABLED;
            break;
        }
    }
    while (0);
  
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);
    return l_i_retstatus;
}


int OtdrLiveStop()
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_LIVE_STOP, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp);

           if(strstr(l_i8_Resp, "?Not in live scan mode."))
        {
            l_i_retstatus = OTDR_LIVE_SCAN_DISABLED;
            break;
        } 
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}


int OtdrLiveAnalyze()
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_LIVE_ANALYZE, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}


int OtdrLiveAvgAnalyze()
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_LIVE_AVG_ANALYZE, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }
        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

int OtdrLiveTrace(T_DUMPTRACE *liveDumpTrace)
{   return OtdrDumpTraceInternal(liveDumpTrace, OTDR_LIVE_TRACE);
}
int OtdrLiveAvgTrace(T_DUMPTRACE *liveDumpAvgTrace)
{   return OtdrDumpTraceInternal(liveDumpAvgTrace, OTDR_LIVE_AVG_TRACE);
}
int OtdrLiveEvent(T_DUMPEVENT *live_event_table)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_MEDIUM_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_MEDIUM_RESP_LENGTH;
    T_OTDRRESULT result = {0};
    int l_i_isOkHR = 0;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if (live_event_table != NULL)
        memset(live_event_table, 0, sizeof(T_DUMPEVENT));
    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   if (live_event_table == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_LIVE_EVENT, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command,
                                               strlen((const char*) l_i8_command), (uint8_t *) l_i8_Resp,
                                               &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }
        if(strstr(l_i8_Resp, "?No valid data"))
              {
                l_i_retstatus =  OTDR_LIVE_INVALID_DATA;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: %d]", l_i_retstatus);
                break;
              }
        l_i_retstatus = OtdrResult(&result);
        if (l_i_retstatus)
        {   break;
        }
        else
        {   if (result.code == OTDR_RESULT_OK_HR)
            {   l_i_isOkHR = 1;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[OTDR result is OK HR]");
            }
        }

        // count number of lines in response
        const char *line = l_i8_Resp;
        int nlines = 0;
        for (const char *p = strchr(line, '\n'); p != NULL; p = strchr(line, '\n'))
        {   if (line[0] == '!')
            {   nlines = -1; // error
                break;
            }
            nlines++;
            line = p + 1;
        }

        if (l_i_isOkHR)
        {   nlines+=1;
        }

        if (nlines < 0)
        {   l_i_retstatus = OTDR_NOT_DEFINED;
            break;
        }
        T_DUMPEVENTENTRY * const ebuf = (T_DUMPEVENTENTRY *)malloc(nlines * sizeof(T_DUMPEVENTENTRY));
        if (ebuf == NULL)
        {   l_i_retstatus = OTDR_MEMORY_ALLOCATION_FAILED;
            break;
        }
        // parse event table entries
        int nevents = 0;

        if (l_i_isOkHR)
        {   T_DUMPEVENTENTRY e;
            e.type = OTDR_EVENT_TYPE_HIGH_REF;
            e.x_km = result.hrLocation;
            e.loss_u_km = NAN;
            e.refl_u_km = NAN;
            e.loss_dB = NAN;
            e.refl_dB = INFINITY;
            e.attn_dB_km = NAN;
            e.c_loss_dB = NAN;

            ebuf[nevents++] = e;
        }

        for (const char *p = l_i8_Resp; *p != '\0'; )
        {   const char * const q = strchr(p, '\n');
            if (q == NULL)
            {   if(!l_i_isOkHR)
                {   nevents = -1;
                }
                break;
            }
            T_DUMPEVENTENTRY e;
            e.c_loss_dB = NAN;
            const int c = sscanf(p, "%u,%f,%f,%f,%f,%f,%f,%f", &e.type, &e.x_km, &e.loss_u_km, &e.refl_u_km, &e.loss_dB, &e.refl_dB, &e.attn_dB_km, &e.c_loss_dB);
            if (c >= 7)
            {   if (nevents == nlines)
                {   if(!l_i_isOkHR)
                    {   nevents = -1;
                    }
                    break;
                }
                ebuf[nevents++] = e;
            }
            else if (c != 0)
            {   if(!l_i_isOkHR)
                {   nevents = -1;
                }
                break;
            }
            p = q + 1;
        }
        if (nevents >= 0)
        {   live_event_table->n = nevents;
            event_sort(ebuf,nevents);
            live_event_table->e = ebuf;
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            free(ebuf);
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;

}
/**
 * @brief This function set max_ne OTDR paramter
 *
 * @param max_ne input Parameter for OTDR
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 */
int OtdrSetMaxNE(int max_ne)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
   
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }

    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   if ((max_ne >= 1) && (max_ne <= 4))
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %d%s",
                     OTDR_MAX_NE, max_ne, CRLF);
        }
        else
        {   l_i_retstatus = OTDR_INVALID_INPUT;
            break;
        }

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
/**
 * @brief This function return max_ne OTDR paramter data
 *
 * @param max_ne output Parameter for OTDR
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 */
int OtdrGetMaxNE(int* max_ne)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    uint8_t expectedVal[28] = {0};
    char newLineDelim[] = "\n";
    char *end_str = NULL;
    char *token = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }

    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_MAX_NE, CRLF);


        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp);

        if (NULL != l_i8_Resp)
        {   token = strtok_r(l_i8_Resp, newLineDelim, &end_str);

            while (token != NULL)
            {   if (strstr(token, "otdr") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                else if (strstr((char *) token, "OTDR MAX_NE") != NULL)
                {   valueAfterColon(token,expectedVal);
                    *max_ne = strtod(expectedVal, NULL);
                }

                token = strtok_r(NULL, newLineDelim, &end_str);
            }
        }


    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function configures the end of fiber postion threshold front OTDR parameter to x km(float).
 *
 * @param eof_ptf end of fiber position threshold front parameter for OTDR. Valid range is [XX, XX].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetEofPosThresholdFront(float eof_ptf)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
   
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }

    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   if (eof_ptf>0)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %f%s",
                     OTDR_EOF_PTF, eof_ptf, CRLF);
        }
        else
        {   l_i_retstatus = OTDR_INVALID_INPUT;
            break;
        }

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function gets the end of fiber position threshold front OTDR parameter value.
 *
 * @param eof_ptf return the end of fiber position threshold front OTDR parameter value in km(float). [Default value is 0.5]
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetEofPosThresholdFront(float* eof_ptf)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    uint8_t expectedVal[28] = {0};
    char newLineDelim[] = "\n";
    char *end_str = NULL;
    char *token = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }

    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_EOF_PTF, CRLF);


        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp);

        if (NULL != l_i8_Resp)
        {   token = strtok_r(l_i8_Resp, newLineDelim, &end_str);

            while (token != NULL)
            {   if (strstr(token, "otdr") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                else if (strstr((char *) token, "OTDR EOF_PTF") != NULL)
                {   valueAfterColon(token,expectedVal);
                    *eof_ptf = strtof(expectedVal, NULL);
                }

                token = strtok_r(NULL, newLineDelim, &end_str);
            }
        }


    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}


/**
 * @brief This function configures the end of fiber loss threshold front OTDR parameter to x db(float).
 * @param eof_ltf end of fiber loss threshold front parameter for OTDR. Valid range is [XX, XX].
 * @param eof_lt end of fiber loss threshold parameter for OTDR. Valid range is [XX, XX].
 * @param eof_ltlz end of fiber loss threshold last zone parameter for OTDR. Valid range is [XX, XX].
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrSetEofLossThreshold(float eof_ltf, float eof_lt, float eof_ltlz)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    
   
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }

    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   if (eof_ltf>0 && eof_lt>0 && eof_ltlz >0)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %f %f %f %s",
                     OTDR_EOF_LTF, eof_ltf, eof_lt, eof_ltlz, CRLF);
        }
        else
        {   
	    l_i_retstatus = OTDR_INVALID_INPUT;
            break;
        }

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);
    return l_i_retstatus;
}

/**
 * @brief This function gets the end of fiber loss threshold front OTDR parameter value.
 *
 *@param eof_ltf return the end of fiber loss threshold front OTDR parameter value in db(float). [Default value is 14.0]
 * @param eof_lt return the end of fiber loss threshold OTDR parameter value in db(float). [Default value is 5.0]
 * @param eof_ltlz return the end of fiber loss threshold last zone OTDR parameter value in db(float). [Default value is 3.0]
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 */
int OtdrGetEofLossThreshold(float* eof_ltf, float* eof_lt, float* eof_ltlz)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    uint8_t expectedVal[28] = {0};
    char newLineDelim[] = "\n";
    char *end_str = NULL;
    char *token = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }

    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_EOF_LTF, CRLF);


        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp);

        if (NULL != l_i8_Resp)
        {   token = strtok_r(l_i8_Resp, ",\n", &end_str);

            while (token != NULL)
            {	 if (strstr(token, "otdr") != NULL) {
                    token = strtok_r(NULL, ",\n", &end_str);
                    continue;
                } 
                if (strstr((char *)token, "OTDR EOF_LTF :") != NULL) {
                    memset(expectedVal, 0, sizeof(expectedVal)); // Clear the buffer
                    valueAfterColon(token, expectedVal);
                    *eof_ltf = strtof(expectedVal, NULL);
                    token = strtok_r(NULL, ",\n", &end_str);
                } 
                if (strstr((char *)token, "EOF_LT :") != NULL) {
                    memset(expectedVal, 0, sizeof(expectedVal)); // Clear the buffer
                    valueAfterColon(token, expectedVal);
                    *eof_lt = strtof(expectedVal, NULL);
                    token = strtok_r(NULL, ",\n", &end_str);
                } 
                if (strstr((char *)token, "EOF_LTLZ :") != NULL) {
                    memset(expectedVal, 0, sizeof(expectedVal)); // Clear the buffer
                    valueAfterColon(token, expectedVal);
                    *eof_ltlz = strtof(expectedVal, NULL);
                    token = strtok_r(NULL, ",\n", &end_str);
                }
                token = strtok_r(NULL, ",\n", &end_str);
            }
        }
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

int OtdrLiveAnalyzeStatus(T_LIVEANALYZESTATUS *live_analyze_status)
{
    int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int retProcessResp = -1;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    char* val = NULL;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    char *last = NULL;
    int8_t *l_i8_respPtr = NULL;
    char *token=NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s. Communication handle: %d]", __func__, otdrcomm_handle);

    if ((l_i_retstatus = checkInit()) > 1)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Init check failed. Return Value: %d]", OTDR_COMMUNICATION_ERROR);
        return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Init check successful]");
    l_i_retstatus = OTDR_COMMAND_SUCCESS;


    do
    {   if (live_analyze_status == NULL)
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[NULL Pointer received from application: %d.", OTDR_NULL_POINTER);
            l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        live_analyze_status->analyzestatus = OTDR_STATE_UNKNOWN;
        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_LIVE_ANALYZE_STATUS, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: OTDR ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }
       if (NULL != l_i8_Resp)
         {
            token = strstr(l_i8_Resp, "IDLE");
            if(token)
            live_analyze_status->analyzestatus = LIVE_ANALYZESTA_IDLE;//1
            token = strstr(l_i8_Resp, "ANALYZE");
            if(token)
            live_analyze_status->analyzestatus= LIVE_ANALYZESTA_ANALYZE;//0

        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}


int OtdrLiveTraceCount(T_LIVETRACECOUNT *params)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    int retProcessResp = -1;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    uint8_t expectedVal[32] = { };
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    uint8_t lines[32][128];
    char newLineDelim[] = "\n";
    char commaDelim[] = ",";
    int lineCount = 0;
    char *end_str = NULL;
    char *token = NULL;
    char *end_token = NULL;
    uint8_t retstr[32] = {0};
    int resCounter = 0;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if (!checkInit())
        return (int)OTDR_COMMUNICATION_ERROR;

    do
    {

        if (params == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_LIVE_TRACE_COUNT, CRLF);

        /** Call the setget command*/
        l_i_retstatus = (*transportReadWrite)(otdrcomm_handle,
                                              (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                              (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }

        if (NULL != l_i8_Resp)
        {   token = strtok_r(l_i8_Resp, newLineDelim, &end_str);

            while (token != NULL)
            {   if (strstr(token, "otdr") != NULL)
                {   token = strtok_r(NULL, newLineDelim, &end_str);
                    continue;

                }
                 else if (strstr((char *) token, "LiveCaptureCount") != NULL)
                {   /** Process the Command to get the Output*/
                    retProcessResp = processResponse(token, ":","",
                                                     expectedVal);
                    params->liveScanCaptureCnt = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token, "LiveTraceCount") != NULL)
                {   /** Process the Command to get the Output*/
                    retProcessResp = processResponse(token, ":","",
                                                     expectedVal);
                    params->liveScanProcessCnt = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token,
                                "LiveAvgTraceCount") != NULL)
                {   /** Process the Command to get the Output*/
                    retProcessResp = processResponse(token, ":","",
                                                     expectedVal);
                    params->liveScanAvgProcessCnt = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token,
                                "LiveAnalyzeTraceIndex") != NULL)
                {   retProcessResp = processResponse(token, ":","",
                                                     expectedVal);
                    params->analyzeTraceindex = strtof(expectedVal, NULL);
                    resCounter++;
                }
                else if (strstr((char *) token,
                                "LiveAvgAnalyzeTraceindex") != NULL)
                {   retProcessResp = processResponse(token, ":","",
                                                     expectedVal);
                    params->avgAnalyzeTraceindex = strtof(expectedVal, NULL);
                    resCounter++;
                }
                token = strtok_r(NULL, newLineDelim, &end_str);
            }
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            break;
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}



int OtdrLiveAnalyzeTrace(T_DUMPTRACE *live_trace)
{
    return OtdrDumpTraceInternal(live_trace, OTDR_LIVE_ANALYZE_TRACE);
}


int OtdrLiveAvgAnalyzeTrace(T_DUMPTRACE *live_avgtrace)
{
    return OtdrDumpTraceInternal(live_avgtrace, OTDR_LIVE_AVG_ANALYZE_TRACE);
}

int OtdrLiveAvgEvent(T_DUMPEVENT *live_event_table)
{   int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_EXT_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_MEDIUM_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_MEDIUM_RESP_LENGTH;
    T_OTDRRESULT result = {0};
    int l_i_isOkHR = 0;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if (live_event_table != NULL)
        memset(live_event_table, 0, sizeof(T_DUMPEVENT));
    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }
    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   if (live_event_table == NULL)
        {   l_i_retstatus = OTDR_NULL_POINTER;
            break;
        }

        snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_LIVE_AVG_EVENT, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command,
                                               strlen((const char*) l_i8_command), (uint8_t *) l_i8_Resp,
                                               &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if (OTDR_COMMAND_SUCCESS != l_i_retstatus)
        {   break;
        }
        if(strstr(l_i8_Resp, "?No valid data"))
              {
                l_i_retstatus =  OTDR_LIVE_INVALID_DATA;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: %d]", l_i_retstatus);
                break;
              }
        l_i_retstatus = OtdrResult(&result);
        if (l_i_retstatus)
        {   break;
        }
        else
        {   if (result.code == OTDR_RESULT_OK_HR)
            {   l_i_isOkHR = 1;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[OTDR result is OK HR]");
            }
        }

        // count number of lines in response
        const char *line = l_i8_Resp;
        int nlines = 0;
        for (const char *p = strchr(line, '\n'); p != NULL; p = strchr(line, '\n'))
        {   if (line[0] == '!')
            {   nlines = -1; // error
                break;
            }
            nlines++;
            line = p + 1;
        }

        if (l_i_isOkHR)
        {   nlines+=1;
        }

        if (nlines < 0)
        {   l_i_retstatus = OTDR_NOT_DEFINED;
            break;
        }
        T_DUMPEVENTENTRY * const ebuf = (T_DUMPEVENTENTRY *)malloc(nlines * sizeof(T_DUMPEVENTENTRY));
        if (ebuf == NULL)
        {   l_i_retstatus = OTDR_MEMORY_ALLOCATION_FAILED;
            break;
        }
        // parse event table entries
        int nevents = 0;

        if (l_i_isOkHR)
        {   T_DUMPEVENTENTRY e;
            e.type = OTDR_EVENT_TYPE_HIGH_REF;
            e.x_km = result.hrLocation;
            e.loss_u_km = NAN;
            e.refl_u_km = NAN;
            e.loss_dB = NAN;
            e.refl_dB = INFINITY;
            e.attn_dB_km = NAN;
            e.c_loss_dB = NAN;

            ebuf[nevents++] = e;
        }

        for (const char *p = l_i8_Resp; *p != '\0'; )
        {   const char * const q = strchr(p, '\n');
            if (q == NULL)
            {   if(!l_i_isOkHR)
                {   nevents = -1;
                }
                break;
            }
            T_DUMPEVENTENTRY e;
            e.c_loss_dB = NAN;
            const int c = sscanf(p, "%u,%f,%f,%f,%f,%f,%f,%f", &e.type, &e.x_km, &e.loss_u_km, &e.refl_u_km, &e.loss_dB, &e.refl_dB, &e.attn_dB_km, &e.c_loss_dB);
            if (c >= 7)
            {   if (nevents == nlines)
                {   if(!l_i_isOkHR)
                    {   nevents = -1;
                    }
                    break;
                }
                ebuf[nevents++] = e;
            }
            else if (c != 0)
            {   if(!l_i_isOkHR)
                {   nevents = -1;
                }
                break;
            }
            p = q + 1;
        }
        if (nevents >= 0)
        {   live_event_table->n = nevents;
            event_sort(ebuf,nevents);
            live_event_table->e = ebuf;
        }
        else
        {   l_i_retstatus = OTDR_PROCESSING_ERROR;
            free(ebuf);
        }
    }
    while (0);

    if (OTDR_PROCESSING_ERROR == l_i_retstatus)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Processing error in response: %s]", l_i8_Resp);
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;

}


/**
 * @brief This function switches UART connection from internal to external otdr and vice versa
 *
 * @param otdr_connection accepts 0 and 1 *           
 *          1: Switch UART connection to External otdr
 *          0: Switch UART connection back to Internal otdr,
 * @return  On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 *
 */
int OtdrSwitchUart(int otdr_connection)
{

    FILE *fp = NULL;
    int l_i_status = -1;
    int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    /* Initialize Logger */
    g_i_lgrOtdrMode = OTDR_SPEED_MODE;
    setLogOutput(LOG_OUTPUT_FILE);
    if (!g_px_lgrFileptr)
        setFileLogInfo(&g_px_lgrFileptr, OTDR_LOGGER_FILE_PATH, NULL);
    setLogLevel(LOG_LEVEL_INFO);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    switch(otdr_connection)
    {   case 0:
            fp = fopen(EXTERNAL_OTDR_FILE, "w");
            if(fp)
            {   fprintf(fp,"%d", 0);
                fclose(fp);
                external_otdr_flag = 0;
                OTDR_REQUEST_TIMEOUT	=	1000000;
                OTDR_REQUEST_EXT_TIMEOUT =	3000000;
                system("echo 1 >/sys/class/gpio/gpio17/value");
            }
            else
            {
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error:Case 0: Not able to access %s]", EXTERNAL_OTDR_FILE);
                l_i_retstatus = OTDR_PROCESSING_ERROR;
            }
            
            break;

        case 1:
            fp = fopen(EXTERNAL_OTDR_FILE, "w");
            if(fp)
            {   fprintf(fp,"%d", 1);
                fclose(fp);
                external_otdr_flag = 1;
                OTDR_REQUEST_TIMEOUT	=	1000000 * 15;
                OTDR_REQUEST_EXT_TIMEOUT =	3000000 * 15;
                system("echo 0 >/sys/class/gpio/gpio17/value");
            }
            else
            {
                l_i_retstatus = OTDR_PROCESSING_ERROR;
                LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error:Case 1: Not able to access %s]", EXTERNAL_OTDR_FILE);
            }
			
            break;
    }
 
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);
    return l_i_retstatus;   

}

/**************************************************************************************************
 * @brief The function updates the external_otdr_flag.
 *
 **************************************************************************************************/
void updateExternalOtdrFlag (void)
{
    int otdr_connection = 0;
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);
    isExternalOTDR (&otdr_connection);
    external_otdr_flag = otdr_connection;
    if(external_otdr_flag)
    {
        OTDR_REQUEST_TIMEOUT	=	1000000 * 12;
        OTDR_REQUEST_EXT_TIMEOUT =	3000000 * 12;
    }
    else
    {
        OTDR_REQUEST_TIMEOUT	=	1000000;
        OTDR_REQUEST_EXT_TIMEOUT =	3000000;
    }
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s]", __func__);
}

/**************************************************************************************************
 * @brief The function reads External OTDR file to analyze OTDR to run.
 * @param otdr_connection Pointer will store the external_otdr_flag value.
 * @return On success returns OTDR_COMMAND_SUCCESS and error code otherwise.
 **************************************************************************************************/
int isExternalOTDR (int *otdr_connection)
{   
    int l_i_status = OTDR_COMMAND_SUCCESS;
    FILE *fp = NULL;
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);
    /* Read lock file. */
    if(fp = fopen(EXTERNAL_OTDR_FILE,"r"))
    {   
        fscanf(fp, "%d", otdr_connection);
        fclose(fp);
        fp = NULL;
      
    }
    else
    {
        *otdr_connection = 0;
        l_i_status = OTDR_UNEXPECTED_ERROR;
    }

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_status);
    return l_i_status;
}

/**
* @brief The function sets the OTDR tone generator mode.
* @param mode indicates the tone generator mode to enable. OTDR_TONE_OFF returns the OTDR to the default otdr scan mode.
* @return On success returns 0 and error code otherwise.
*/
int SetToneGeneratorMode(T_TONEMODE mode)
{
    int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }

    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   if ((mode >= 0) && (mode < OTDR_INVALID_MODE)) // 0:OTDR Steady OFF, 1:OTDR Steady ON, 2:OTDR, 3:270Hz, 4:1KHz, 5:2KHz
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s %s%s",
                     OTDR_TONE_GENERATOR_MODE, (mode == OTDR_STEADY_OFF ? "STEADY_OFF" : mode == OTDR_STEADY_ON ? "STEADY_ON" : 
                     mode == OTDR_TONE_270HZ ? "270HZ" : mode == OTDR_TONE_1KHZ ? "1KHZ" : mode == OTDR_TONE_2KHZ ? "2KHZ" : "OTDR"), CRLF);            
        }
        else
        {   l_i_retstatus = OTDR_INVALID_INPUT;
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr - Invalid mode: %d]", (int)mode);
            break;
        }
        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp);
    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
* @brief The function gets the current tone generator mode..
* @param mode will be updated with a enum value indicating the current tone generator mode. OTDR_TONE_OFF indicates default otdr scan mode.
* @return  On success returns 0 and error code otherwise.
*/
int GetToneGeneratorMode(T_TONEMODE *mode)
{
    int l_i_retstatus = OTDR_COMMAND_SUCCESS;
    uint32_t timeout = OTDR_REQUEST_TIMEOUT;
    int8_t l_i8_command[OTDR_API_SHORT_CMD_LENGTH] = { 0 };
    int8_t l_i8_Resp[OTDR_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OTDR_API_SHORT_RESP_LENGTH;
    uint8_t expectedVal[28] = {0};
    char newLineDelim[] = "\n";
    char *end_str = NULL;
    char *token = NULL;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Entering function: %s]", __func__);

    if ((l_i_retstatus = checkInit()) > 1)
    {   return l_i_retstatus;
    }
    else if(l_i_retstatus == 0)
    {   return (int)OTDR_COMMUNICATION_ERROR;
    }

    l_i_retstatus = OTDR_COMMAND_SUCCESS;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "%s%s",
                 OTDR_TONE_GENERATOR_MODE, CRLF);

        /** Call the setget command*/
        l_i_retstatus =  (*transportReadWrite)(otdrcomm_handle,
                                               (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                                               (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);
        
        if ((l_i_retstatus != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Error: otdr ReadWrite: %d]", l_i_retstatus);
            break;
        }

        l_i_retstatus = responseErrorCheck(l_i8_Resp);
        if(l_i_retstatus)
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Response: %s]", l_i8_Resp); 
        
        if (NULL != l_i8_Resp)
        {   token = strtok_r(l_i8_Resp, newLineDelim, &end_str);

            while (token != NULL)
            {   if (strstr((char *) token, "OTDR TX Mode") != NULL)
                {   valueAfterColon(token,expectedVal);
                    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:Expected value: %s]", expectedVal);
                    if(strstr((char *) expectedVal, "STEADY_OFF") != NULL)
                        *mode = OTDR_STEADY_OFF;
                    else if(strstr((char *) expectedVal, "STEADY_ON") != NULL)
                        *mode = OTDR_STEADY_ON;
                    else if(strstr((char *) expectedVal, "OTDR") != NULL)
                        *mode = OTDR_MODE;
                    else if(strstr((char *) expectedVal, "270HZ") != NULL)
                        *mode = OTDR_TONE_270HZ;
                    else if(strstr((char *) expectedVal, "1KHZ") != NULL)
                        *mode = OTDR_TONE_1KHZ;
                    else if(strstr((char *) expectedVal, "2KHZ") != NULL)
                        *mode = OTDR_TONE_2KHZ;
                    else
                        *mode = OTDR_INVALID_MODE;
                }

                token = strtok_r(NULL, newLineDelim, &end_str);

            }
        }

    }
    while (0);

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Exiting function: %s: Mode: %s (%d),Return Value: %d]", __func__, expectedVal, *mode, l_i_retstatus);

    return l_i_retstatus;
}


