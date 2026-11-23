/**
 *  @file 	ocm_interface.c
 *  @brief 	OCM Interface API Library
 *
 *  	This file contains OCM command APIs implementations.
 *
 */

/* Standard Header */
#include <ocm_interface.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>

/* Custom/Library Header */
#include "logMgr.h"
#include "serial.h"
#include "power_socket_client.h"
#include "json_object.h"
#include "json_tokener.h"


#define OCM_API_COMMAND_LENGTH          64
#define OCM_API_SHORT_RESPONSE_LENGTH   128
#define OCM_API_MEDIUM_RESPONSE_LENGTH  1024
#define OCM_API_LARGE_RESPONSE_LENGTH   8192
#define OCM_REQUEST_TIMEOUT             500000

#define OCM_API_DREV_COMMAND	"DREV"
#define OCM_API_FWVR_COMMAND	"FWVR"
#define OCM_API_HWVR_COMMAND	"HWVR"
#define OCM_API_SNUM_COMMAND	"SNUM"
#define OCM_API_CSSP_COMMAND	"CSSP"
#define OCM_API_CSSP0_COMMAND	"CSSP0"
#define OCM_API_CSSP1_COMMAND	"CSSP1"
#define OCM_API_CSSP2_COMMAND	"CSSP2"
#define OCM_API_CSTA0_COMMAND	"CSTA0"
#define OCM_API_CFCR_COMMAND	"CFCR"
#define OCM_API_CGTP1B_COMMAND	"CGTP10B"
#define OCM_API_CGTP11B_COMMAND	"CGTP11B"
#define OCM_API_CGTP12B_COMMAND	"CGTP12B"
#define OCM_API_CBDI_COMMAND	"CBDI"
#define OCM_API_CGSP1_COMMAND	"CGSP1"
#define OCM_API_CTPR1_COMMAND	"CTPR1"
#define OCM_API_CENB1_COMMAND	"CENB1"
#define OCM_API_CSTS0_COMMAND	"CSTS0"
#define OCM_API_CRST_COMMAND	"CRST"
#define OCM_API_CDMO_COMMAND	"CDMO 1"
#define OCM_API_GNHI1_COMMAND	"GNHI1"
#define OCM_API_GNLO1_COMMAND	"GNLO1"
#define OCM_API_CMPP1_COMMAND	"CMPP1 "
#define OCM_API_CSIL1_COMMAND	"CSIL1 "
#define OCM_API_CSPT1_COMMAND	"CSPT1 "
#define OCM_API_CGET1_COMMAND	"CGET1"
#define OCM_API_CDDFB_COMMAND	"CDDFB"
#define OCM_API_CDGT1_COMMAND	"CDGT1"
#define OCM_API_SSBC1_COMMAND	"SSBC1 "
#define OCM_API_SSTC1_COMMAND	"SSTC1 "



#define OCM_API_LBDX_COMMAND	"LBDX"
#define OCM_API_UPFW_COMMAND	"UPFW"
#define OCM_API_SLCT_COMMAND	"SLCT"
#define OCM_API_CONT_COMMAND	"CONT"
#define OCM_API_COPY_COMMAND	"COPY"
#define OCM_API_LIST_COMMAND	"LIST"
#define OCM_API_LOAD_COMMAND	"LOAD"
#define OCM_API_NOOP_COMMAND	"NOOP"
#define OCM_API_UNPF_COMMAND	"UNPF"

/*CRC for boot-loader commands*/
#define OCM_BOOTLOAD_CRST_CRC	"00179"
#define OCM_BOOTLOAD_LBDX_CRC	"52310"
#define OCM_BOOTLOAD_UPFW_CRC	"40456"
#define OCM_BOOTLOAD_SLCT_CRC	"16389"
#define OCM_BOOTLOAD_CONT_CRC	"29870"
#define OCM_BOOTLOAD_COPY_CRC	"24578"
#define OCM_BOOTLOAD_LIST_CRC	"26575"
#define OCM_BOOTLOAD_LOAD_CRC	"41551"
#define OCM_BOOTLOAD_NOOP_CRC	"16029"
#define OCM_BOOTLOAD_UNPF_CRC	"28079"

#define OCM_BOOTLOAD_COPY_SRC	"001"
#define OCM_BOOTLOAD_COPY_DST	"000"

#define OCM_POWERINGUP_TIME     25

#define OCM_INIT_PROGRESS_FILE "/tmp/ocm_init_progress"
#define PROD_INFO_FILE_PATH     "/tmp/product_info.json"

#define BUF 2048

int OcmdownloadFileStatus(uint8_t *file, long *filesize, int *totalblocks, int *rembytes);
int OcmCONT();
int OcmSLCT();
int OcmUPFW();
int OcmLBDX();
int OcmCRST();
int BootloaderErrorCheck(unsigned char *i_puc_respBuff);
int TransferFile(uint8_t *l_ui8_file);

//Array of error_string
const char * ocm_bootloader_resp[]=
{   /*UART Library Error*/
    "OK",                      //index 0
    "UC",
    "IC",
    "DE",
    "NVI",
    "ER",
    "ER1",
    "BIE",
};

//Enum for error number
typedef enum
{   OCM_NO_ERR		= 0,        //!< NO_ERR
    OCM_UNKOWN_COMMAND,			// Unknown Command Error
    OCM_INVALID_COMMAND,		//InValid OCM Command
    OCM_INVALID_CRC,
    OCM_INVALID_IMAGE,
    OCM_ERROR,
    OCM_DOWNLOAD_ERROR,
    OCM_BOOTIMAGE_ERROR,
    OCM_MAX_ERROR_COUNT
} OCM_BOOTLOADER_ERROR_CODE;

#define CR						"\r"
#define NL						"\n"
#define CRNL_LEN				3
#define OCM_SERIALNODE				"ttyS1"
#define OCM_BINARY_FILE_PATH   "/tmp/"
#define PRODUCT_INFO_FILE_PATH					"/data/config/.product_info.json"
/** For logger */
#define OCM_LOGGER_FILE_PATH   "/var/log/OCM.log"

#define PRODUCT_INFO_BUF		8192

int ocmcomm_handle =  -1;       //UART Linux device handler
int ocm_powercomm_handle = -1;       //UART Linux device handler

bool g_b_OCMPoweringON = false;

int g_i_dataLen = 0;
unsigned char g_ac_dataResp[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

int g_ui8_debug_ocm = 0;

FILE * g_px_OcmlgrFileptr = NULL;

static pthread_mutex_t ocm_lock = PTHREAD_MUTEX_INITIALIZER;

int (*powertransportReadWrite)(int, uint8_t *,uint16_t, uint8_t *, uint32_t *, uint32_t);


/**
 * @brief Checks if library is initialized.
 *
 * @return 1 if library is initialized or 0 if not.
 */
static int checkInit(void)
{   pthread_mutex_lock(&ocm_lock);
    const int initialized = (ocmcomm_handle > 0);
    pthread_mutex_unlock(&ocm_lock);
    if (!initialized && g_ui8_debug_ocm)
        printf("[OCM Interface Lib] Error: OCM Interface Init is pending\n");

    if (POWERMGT_DEVICE_ON != getOCMStatus())
        return 0;

    return initialized;
}

/**
 * @brief Checks if library is initialized for socket communication.
 *
 * @return 1 if library is initialized or 0 if not.
 */
static int checkOCMPowerInit(void)
{   const int initialized = (ocm_powercomm_handle > 0);
    if (!initialized)
        LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[%s] OCM Interface is not initialized for Power communication ", __func__);
    return initialized;
}

/**
 * @brief  Remove trailing spaces from input string
 *         This function remove consecutive trailing spaces from input string
 *
 * @param  str  pointer to a string
 */

void trimTrailing(char * str)
{   int index, i;

    /* Set default index */
    index = -1;

    /* Find last index of non-white space character */
    i = 0;
    while(str[i] != '\0')
    {   if(str[i] != ' ' && str[i] != '\t' && str[i] != '\n')
        {   index= i;
        }

        i++;
    }

    /* Mark next character to last non-white space character as NULL */
    str[index + 1] = '\0';
}


/**
 * @brief  Remove space from input string
 *         This function remove consecutive space from input string
 *
 * @param  io_puc_line  pointer to a string
 * @return On Success DEF_TRUE, On error DEF_FALSE
 */

int8_t remove_space(uint8_t *io_puc_line)
{   uint16_t l_ui16_index = 0;
    uint16_t l_ui16_cnt   = 0;
    int8_t l_i8_Ret = EINVAL;

    if(io_puc_line == NULL)
    {   return l_i8_Ret;            //checking pointer should not be null
    }

    while(io_puc_line[l_ui16_index])
    {   if((io_puc_line[l_ui16_index] == ' ') && (io_puc_line[l_ui16_index+1] == ' ')) //Remove consecutive space
        {   l_ui16_cnt=l_ui16_index;
            while(io_puc_line[l_ui16_cnt] != '\0')
            {   io_puc_line[l_ui16_cnt] = io_puc_line[l_ui16_cnt+1];
                ++l_ui16_cnt;
            }
            io_puc_line[l_ui16_cnt]='\0';
        }
        else
            ++l_ui16_index;
    }

    if(io_puc_line[0] == ' ')                       //Remove initial space
    {   l_ui16_cnt=0;
        while(io_puc_line[l_ui16_cnt] != '\0')
        {   io_puc_line[l_ui16_cnt] = io_puc_line[l_ui16_cnt+1];
            ++l_ui16_cnt;
        }

        io_puc_line[l_ui16_cnt]='\0';
    }
    l_i8_Ret = 0;
    return l_i8_Ret;
}


/**
 * @brief This function checks the error from the received frame
 *
 * @param errorVal	input error response
 *
 * @return  On Success DEF_TRUE, On error DEF_FALSE
 */
int errorCheck(int errorVal)
{   int i = 0;
    int l_i_retStatus = 0;

    for (i = 31; i >= 0; i--)
    {   if ((errorVal >> i) & 0x01)
        {   break;
        }
    }

    switch (i)
    {   case 31:
            l_i_retStatus = OCM_NOT_READY_ERROR; /** OCM Not Ready */
            break;

        case 30:
            l_i_retStatus = OCM_RANGE_ERROR; /** OCM Range Error */
            break;

        case 29:
            l_i_retStatus = OCM_HARDWARE_ERROR; /** OCM Hardware Error */
            break;

        case 28:
            l_i_retStatus = OCM_INTIALIZATION_ERROR; /** OCM Initialization Error */
            break;

        case 27:
        case 26:
        case 25:
        case 24:
            l_i_retStatus = OCM_RESERVED_BYTE_ERROR; /** Reserved */
            break;

        case 23:
        case 22:
        case 21:
        case 20:
        case 19:
        case 18:
        case 17:
        case 16:
        case 15:
        case 14:
        case 13:
        case 12:
        case 11:
        case 10:
        case 9:
        case 8:
        case 7:
        case 6:
            l_i_retStatus = OCM_UNSUSED_POSITION_ERROR; /** Unused positions */
            break;

        case 5:
            l_i_retStatus = OCM_TOF_DATA_INVALID_ERROR; /** TOF Data Invalid */
            break;

        case 4:
            l_i_retStatus = OCM_TOF_HW_ERROR; /** TOF Hardware Error */
            break;

        case 3:
            l_i_retStatus = OCM_TOF_HIGHTEMP_LIMIT_EXCEED; /** TOF High Temperature Limit Exceeded */
            break;

        case 2:
            l_i_retStatus = OCM_TOF_LOWTEMP_LIMIT_EXCEED; /** TOF Low Temperature Limit Exceeded */
            break;

        case 1:
            l_i_retStatus = OCM_TOF_OPTICAL_OVERLOAD; /** TOF Optical Overload */
            break;

        case 0:
            l_i_retStatus = OCM_TOF_NOT_READY; /** TOF Not Ready */
            break;

    }

    return l_i_retStatus;
}
float q8_converter(uint8_t first_byte, uint8_t last_byte)
{   float result = 0;
    int j = 1;

    for(int i = 7; i >= 0; i--)
    {   result += (float)((first_byte >> i) & 0x01) /(pow(2,j++));
    }


    if((last_byte >> 7) & 0x01)
    {   result =  last_byte - 256 + result;
    }
    else
    {

        result = last_byte + result;
    }

    return result;
}
/**
 * @brief This function reverse the frame in order to get processed response.
 *
 * @param inbuf	 input frame buffer
 * @param length length of the input frame buffer
 */
void Reverse(unsigned char *inbuf, int length)
{   int count = (length % 4);
    int i = 0;

    /*Clear the global Buffer */
    memset(g_ac_dataResp, 0x00, sizeof(g_ac_dataResp));

    for (i = 0; i < (length - count); i += 4)
    {   g_ac_dataResp[i] = inbuf[i + 3];
        g_ac_dataResp[i + 1] = inbuf[i + 2];
        g_ac_dataResp[i + 2] = inbuf[i + 1];
        g_ac_dataResp[i + 3] = inbuf[i];
    }

    if (count == 3)
    {   g_ac_dataResp[i] = inbuf[i + 2];
        g_ac_dataResp[i + 1] = inbuf[i + 1];
        g_ac_dataResp[i + 2] = inbuf[i];
    }
    else if (count == 2)
    {   g_ac_dataResp[i] = inbuf[i + 1];
        g_ac_dataResp[i + 1] = inbuf[i];
    }
    else
    {   g_ac_dataResp[i] = inbuf[i];
    }
}

/**
 * @brief This functions performs the crc16 check on the frame buffer
 * 		  response
 *
 * @param crcout	output crc value
 * @param crcin		crcin(0xffff) to calculate the crcout
 * @param ch		frame buffer
 * @param length	length of the frame buffer
 */
void crc16(unsigned short *crcout, unsigned short crcin, unsigned char *ch,
           int length)
{   unsigned long d, i, k;
    unsigned short test = crcin;

    for (i = 0; i < length; i++)
    {   d = *ch++ << 8;
        for (k = 0; k < 8; k++)
        {   if ((crcin ^ d) & 0x8000)
            {   crcin = ((crcin << 1) ^ 0x1021) & 0xFFFF;
            }
            else
            {   crcin = (crcin << 1) & 0xFFFF;
            }
            d <<= 1;
        }
    }
    *crcout = crcin;

    return;
}

/**
 * @brief This function performs the initial check on the frame
 * 		  creates the response
 *
 * @param i_pc_respBuff		Input frame buffer
 * @param i_i_respLength	Input frame buffer length
 * @param o_ui32_StatusWord returns the 32-bit Status word value
 *
 * @return  On Success 0, and error code otherwise.
 */
int createResponse(unsigned char *i_pc_respBuff, int i_i_respLength, uint32_t *o_ui32_StatusWord)
{   int i_i_retStatus = 0;
    int i_i_dataLen = 0;
    int i_i_padLen = 0;
    unsigned char l_ac_dataBuff[8192] = { 0 };
    unsigned short l_s_crcout = 0;
    unsigned short l_s_framecrc = 0;

    do   /*Check the NULL Response*/
    {   if (i_pc_respBuff == NULL || i_i_respLength == 0)
        {   i_i_retStatus = OCM_NO_RESPONCE_RECEIVED;
            break;
        }

        /*Check the response of the command for IC Error*/
        if (i_pc_respBuff[0] == 'I' && i_pc_respBuff[1] == 'C')
        {   i_i_retStatus = OCM_IC_COMMAND_ERROR;
            break;
        }

        /*Check the response of the command for UC Error*/
        if (i_pc_respBuff[0] == 'U' && i_pc_respBuff[1] == 'C')
        {   i_i_retStatus = OCM_UC_COMMAND_ERROR;
            break;
        }

        /*Check the first byte*/
        if (i_pc_respBuff[0] != 0x0a || i_pc_respBuff[1] != 0x0a)
        {   i_i_retStatus = OCM_WRONG_FIRST_BYTE;
            break;
        }

        /*check the error code for the code*/
        *o_ui32_StatusWord = (i_pc_respBuff[5] << 24) | (i_pc_respBuff[4] << 16)
                             | (i_pc_respBuff[3] << 8) | i_pc_respBuff[2];

        /*Calculate the data length*/
        i_i_dataLen = (i_pc_respBuff[7] << 8) | i_pc_respBuff[6];

        /*Calculate the padding Byte*/
        if (i_i_respLength > 10 && (!(i_i_respLength % 4)))   /*Calculate the padding*/
        {   i_i_padLen = i_i_respLength - 10 - i_i_dataLen;

        }

        /*Calculate the total data Len*/
        g_i_dataLen = i_i_respLength - 10 - i_i_padLen;

        /*Prepare the data*/
        memcpy(l_ac_dataBuff, &i_pc_respBuff[8], g_i_dataLen);

        l_s_framecrc = (i_pc_respBuff[g_i_dataLen + 9] << 8)
                       | i_pc_respBuff[g_i_dataLen + 8];

        /*Reverse the Buffer to calculate the checkSum*/
        Reverse(l_ac_dataBuff, g_i_dataLen);

        /*Calculate the crc16 for the Data*/
        crc16(&l_s_crcout, 0xffff, g_ac_dataResp, g_i_dataLen);

    }
    while (0);

    return i_i_retStatus;
}

/**
 * @brief  This function open serial port and initialize UART parameters (BaudRate,parity,stop-bit)
 *
 *@param   o_puc_err		copy error into string pointer
 *@return  On success returns 0, error code otherwise.
 *
 */

int OcmInterfaceInit()
{   int ret_sinit = OCM_INIT_ERROR;

    /** Initialize Logger */
    setLogOutput(LOG_OUTPUT_FILE);
    if(!g_px_OcmlgrFileptr)
        setFileLogInfo(&g_px_OcmlgrFileptr, OCM_LOGGER_FILE_PATH, NULL);
    setLogLevel(LOG_LEVEL_INFO);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    if (access((char *) OCM_INIT_PROGRESS_FILE, F_OK) == -1)
    {   pthread_mutex_lock(&ocm_lock);
        if(ocmcomm_handle < 0)
        {   ret_sinit = serialPortInit(OCM_SERIALNODE, &ocmcomm_handle);
            if (ret_sinit)
            {   ret_sinit = OCM_INIT_ERROR;
            }
        }
        pthread_mutex_unlock(&ocm_lock);
    }
    else
    {   ret_sinit = OCM_INIT_PROGRESS;
        LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[OCM initialization is in progress]");
    }

    if (ocm_powercomm_handle < 0 && ret_sinit == OCM_COMMAND_SUCCESS)
    {   int l_i_ret;
        powertransportReadWrite = socketPowerMgtReadWrite;
        l_i_ret = powerMgtSocketInit(&ocm_powercomm_handle);
        if(l_i_ret)
        {   ret_sinit = OCM_INIT_ERROR;
            LOG_INFO(&g_px_OcmlgrFileptr, NULL, 0, "OCM:[Power Management initialization failed]");
        }
        else
        {   ret_sinit = OCM_COMMAND_SUCCESS;
        }
    }
    else
    {   LOG_INFO(&g_px_OcmlgrFileptr, NULL, 0, "OCM:[OCM serial initialization failed]");
    }

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, ret_sinit);

    if (ret_sinit && g_px_OcmlgrFileptr)
        logMgrClose(&g_px_OcmlgrFileptr);

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

int OcmInterfaceDeInit()
{   int ret_sdeinit = OCM_DEINIT_ERROR;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    pthread_mutex_lock(&ocm_lock);
    if(ocmcomm_handle >0)
    {   ret_sdeinit = serialPortDeInit(OCM_SERIALNODE, ocmcomm_handle);
        ocmcomm_handle = -1;

        if (ret_sdeinit)
        {   LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Serial Deinitialization Failed: Return Value: %d]", ret_sdeinit);
            ret_sdeinit = OCM_DEINIT_ERROR;
        }
    }
    pthread_mutex_unlock(&ocm_lock);

    if (ocm_powercomm_handle > 0)
    {   int l_i_ret = 0;
        l_i_ret = powerMgtsocketDeInit(ocm_powercomm_handle);
        if(l_i_ret)
        {   ret_sdeinit = OCM_DEINIT_ERROR;
            LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Power Management Deinit failed: %d]", l_i_ret);
        }
        else
        {   ret_sdeinit = OCM_COMMAND_SUCCESS;
            ocm_powercomm_handle = -1;
        }
    }

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, ret_sdeinit);

    /** Deinit Logger */
    if(g_px_OcmlgrFileptr)
        logMgrClose(&g_px_OcmlgrFileptr);

    return ret_sdeinit;
}

/**
 * @brief This function get development revision
 *
 * @param revision return development revision as a string
 * @return On success, returns 0, error code otherwise.
 */
int OcmGetDREV(char *revision)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   /** Check the revision*/
        if (revision == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /** Prepare the command*/
        snprintf((char *) l_ui8_command,
                 (strlen((const char *) OCM_API_DREV_COMMAND) + CRNL_LEN), "%s%s%s%s",
                 NL, OCM_API_DREV_COMMAND, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }


        /** Return the response*/
        strncpy((char *) revision, (char *) l_ui8_response,
                strlen((const char *) l_ui8_response));

    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;

}

/**
 * @brief This function get the firmware revision as a string in the form ‘XX.XX’
 *
 * @param fwvr return the firmware revision as a string
 * @return On success, returns 0, error code otherwise.
 */
int OcmGetFWVR(char *fwvr)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   /** Check the revision from that*/
        if (fwvr == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /** Prepare the command*/
        snprintf((char *) l_ui8_command,
                 (strlen((const char *) OCM_API_FWVR_COMMAND) + CRNL_LEN), "%s%s%s%s",
                 NL, OCM_API_FWVR_COMMAND, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Return the response*/
        strncpy(fwvr,(const char *) l_ui8_response,l_ui_respLen);

    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function get the hardware revision as a string in the form ‘XX.XX’
 *
 * @param hwvr return the hardware revision as a string
 * @return On success, returns 0, error code otherwise.
 */
int OcmGetHWVR(char *hwvr)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   /** Check the revision from that*/
        if (hwvr == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /** Prepare the command*/
        snprintf((char *) l_ui8_command,
                 (strlen((const char *) OCM_API_HWVR_COMMAND) + CRNL_LEN), "%s%s%s%s",
                 NL, OCM_API_HWVR_COMMAND, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Return the response*/
        strncpy(hwvr,(const char *) l_ui8_response,l_ui_respLen);
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function get the CTM serial number
 *
 * @param snum return the CTM serial number as a string
 * @return On success, returns 0, error code otherwise.
 */
int OcmGetSNUM(char *idCode, char *manufactureDate, int *buildNumber)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   /** Check the revision from that*/
        if (idCode == NULL || manufactureDate == NULL || buildNumber == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /** Prepare the command*/
        snprintf((char *) l_ui8_command,
                 (strlen((const char *) OCM_API_SNUM_COMMAND) + CRNL_LEN), "%s%s%s%s",
                 NL, OCM_API_SNUM_COMMAND, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Remove consecutive space*/
        remove_space(l_ui8_response);
        /** Return the response*/
        strncpy((char *) idCode, (char *) l_ui8_response,8);
        strncpy((char *) manufactureDate, (char *) &l_ui8_response[8],8);
        sscanf((char *) &l_ui8_response[16],"%d",buildNumber);
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

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

int OcmGetCSSP(uint32_t *channalPlan, float *startingFreq, uint32_t *o_ui32_StatusWord)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    unsigned int l_ui_startFreq = 0;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   /** Check the NULL Value*/
        if (channalPlan == NULL || startingFreq == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /** Prepare the command*/
        snprintf(l_ui8_command, strlen((const char *) OCM_API_CSSP_COMMAND) + CRNL_LEN,
                 "%s%s%s%s",
                 NL, OCM_API_CSSP_COMMAND,CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }


        /** Copy the Response to the Global Buffer */
        memcpy(g_ac_dataResp, l_ui8_response, l_ui_respLen);

        /*Calculate the Response*/
        if ((l_i_retstatus = createResponse(l_ui8_response, l_ui_respLen, o_ui32_StatusWord))
                != 0)
        {   break;
        }
        /**Check the Status word value*/
        if( *o_ui32_StatusWord > 0)
        {   l_i_retstatus = OCM_INTERNAL_STATUS_ERROR;
        }

        /** Fill the Channel Plan Value to the Buffer*/
        *channalPlan = (g_ac_dataResp[0] << 24)
                       | (g_ac_dataResp[1] << 16) | (g_ac_dataResp[2] << 8)
                       | (g_ac_dataResp[3]);

        l_ui_startFreq = (g_ac_dataResp[4] << 24) | (g_ac_dataResp[5] << 16)
                         | (g_ac_dataResp[6] << 8) | (g_ac_dataResp[7]);

        *startingFreq = *(float *) &l_ui_startFreq;

        /*Clear the Buffer for the Response*/
        memset(g_ac_dataResp, 0x00, sizeof(g_ac_dataResp));

        g_i_dataLen = 0;
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

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
int OcmSetCSSP(uint32_t *channalPlan, float *startingFreq, uint32_t *o_ui32_StatusWord)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    unsigned int l_ui_startFreq = 0;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   /** Check the NULL Value*/
        if (channalPlan == NULL ||  startingFreq == NULL)
        {   l_i_retstatus = EINVAL;
            break;

        }

        /** Prepare the command*/
        snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_CSSP_COMMAND) + CRNL_LEN + 1, "%s%s%d%s%s",
                 NL, OCM_API_CSSP_COMMAND,*channalPlan, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Copy the Response to the Global Buffer */
        memcpy(g_ac_dataResp, l_ui8_response, l_ui_respLen);

        /*Calculate the Response*/
        if ((l_i_retstatus = createResponse(l_ui8_response, l_ui_respLen, o_ui32_StatusWord))
                != 0)
        {   break;
        }

        /**Check the Status word value*/
        if( *o_ui32_StatusWord > 0)
        {   l_i_retstatus = OCM_INTERNAL_STATUS_ERROR;
        }

        /** Fill the Channal Plan Value to the Buffer*/
        *channalPlan = (g_ac_dataResp[0] << 24)
                      | (g_ac_dataResp[1] << 16) | (g_ac_dataResp[2] << 8)
                      | (g_ac_dataResp[3]);

        l_ui_startFreq = (g_ac_dataResp[4] << 24) | (g_ac_dataResp[5] << 16)
                         | (g_ac_dataResp[6] << 8) | (g_ac_dataResp[7]);

        *startingFreq = *(float *) &l_ui_startFreq;

        /*Clear the Buffer for the Response*/
        memset(g_ac_dataResp, 0x00, sizeof(g_ac_dataResp));

        g_i_dataLen = 0;
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function gets control status information
 *
 * @param csta0 return control status information as a struct csta0
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetCSTA0(T_CSTA0 *csta0)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    unsigned int l_ui_temprature = 0;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   /** Check the NULL Value*/
        if (csta0 == NULL)
        {   l_i_retstatus = EINVAL;
            break;

        }

        /** Prepare the command*/
        snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_CSTA0_COMMAND) + CRNL_LEN, "%s%s%s%s",
                 NL, OCM_API_CSTA0_COMMAND, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Copy the Response to the Global Buffer */
        memcpy(g_ac_dataResp, l_ui8_response, l_ui_respLen);

        /*Calculate the Response*/
        if ((l_i_retstatus = createResponse(l_ui8_response, l_ui_respLen, &csta0->m_ui32_ChkErrorStatus))
                != 0)
        {   break;
        }

        /**Check the Status word value*/
        if(csta0->m_ui32_ChkErrorStatus > 0)
        {   l_i_retstatus = OCM_INTERNAL_STATUS_ERROR;
        }

        /*Get the Reserved val*/
        l_ui_temprature = (g_ac_dataResp[0] << 24) | (g_ac_dataResp[1] << 16)
                          | (g_ac_dataResp[2] << 8) | (g_ac_dataResp[3]);

        //*Copy the Reserved to the float value*/
        csta0->m_f_deviceTemperature = *(float *) &l_ui_temprature;

        /*Create the CPTR Response */
        csta0->m_ui32_statusWord = (g_ac_dataResp[4] << 24)
                                   | (g_ac_dataResp[5] << 16) | (g_ac_dataResp[6] << 8)
                                   | (g_ac_dataResp[7]);

        /*Clear the Buffer for the Response*/
        memset(g_ac_dataResp, 0x00, sizeof(g_ac_dataResp));

        g_i_dataLen = 0;
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function gets the optical spectrum including the total optical power for TOF 1
 *        integrated over 0.2 nm, 0.4 nm or 0.8 nm.
 *
 * @param integration specified the integration vaslue as type T_INTEGRATION.
 * @param cgtp1b return the optical spectrum as type T_CGTP1B.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 * @response_format
 *		Total Pow:  total power, mW
 *		Freq:  channel frequency: THz
 *		pow:  channel power, scaled mW (10*mW)
 *		peak: channel peak detection flag (0 no peak detected / 1 peak detected)
 *		sat:  channel saturation flag (0 not saturated / 1 saturated)
 */
int OcmGetCGTP1B(T_INTEGRATE integration,T_CGTP1B *cgtp1)
{   int l_i_retstatus = 0;
    int l_i_count = 0;
    int l_i_chCountNo = 0;
    int l_i_chfreqcnt = 0;
    int l_i_chpwrcnt = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    unsigned int l_ui_temprature = 0;
    unsigned int l_i_totalPower = 0;
    unsigned int l_i_freq = 0;
    unsigned int l_i_power = 0;
    unsigned int l_i_peakDtc[3] = { 0 };
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   if (cgtp1 == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /** Prepare the command*/
        if(integration == 0)
        {   snprintf(l_ui8_command,
                     strlen((const char *) OCM_API_CGTP1B_COMMAND) + CRNL_LEN, "%s%s%s%s",
                     NL, OCM_API_CGTP1B_COMMAND, CR, NL);
        }
        else if (integration == 1)
        {   snprintf(l_ui8_command,
                     strlen((const char *) OCM_API_CGTP11B_COMMAND) + CRNL_LEN, "%s%s%s%s",
                     NL, OCM_API_CGTP11B_COMMAND, CR, NL);
        }
        else if (integration == 2)
        {   snprintf(l_ui8_command,
                     strlen((const char *) OCM_API_CGTP12B_COMMAND) + CRNL_LEN, "%s%s%s%s",
                     NL, OCM_API_CGTP12B_COMMAND, CR, NL);
        }
        else
        {   l_i_retstatus = OCM_UC_COMMAND_ERROR;
            break;
        }

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Copy the Response to the Global Buffer */
        memcpy(g_ac_dataResp, l_ui8_response, l_ui_respLen);

        /**Calculate the Response*/
        if ((l_i_retstatus = createResponse(l_ui8_response, l_ui_respLen, &cgtp1->m_ui32_ChkErrorStatus))
                != 0)
        {   break;
        }

        /**Check the Status word value*/
        if(cgtp1->m_ui32_ChkErrorStatus > 0)
        {   l_i_retstatus = OCM_INTERNAL_STATUS_ERROR;
        }

        /*********************************Peak Detection*****************/
        /**Peak Detect 1*/
        cgtp1->m_peakDetectionFlags[0] = g_ac_dataResp[0] << 24 | g_ac_dataResp[1] << 16
                                         | g_ac_dataResp[2] << 8 | g_ac_dataResp[3];

        /**Peak Detect 2*/
        cgtp1->m_peakDetectionFlags[1] = g_ac_dataResp[4] << 24 | g_ac_dataResp[5] << 16
                                         | g_ac_dataResp[6] << 8 | g_ac_dataResp[7];

        /**Peak Detect 3*/
        cgtp1->m_peakDetectionFlags[2] = g_ac_dataResp[8] << 24 | g_ac_dataResp[9] << 16
                                         | g_ac_dataResp[10] << 8 | g_ac_dataResp[11];

        /*********************************Saturation Detection*****************/
        /**Saturation Detect 1*/
        cgtp1->m_channelSaturationFlags[0] = g_ac_dataResp[12] << 24 | g_ac_dataResp[13] << 16
                                             | g_ac_dataResp[14] << 8 | g_ac_dataResp[15];

        /**Saturation Detect 2*/
        cgtp1->m_channelSaturationFlags[1] = g_ac_dataResp[16] << 24 | g_ac_dataResp[17] << 16
                                             | g_ac_dataResp[18] << 8 | g_ac_dataResp[19];

        /**Saturation Detect 3*/
        cgtp1->m_channelSaturationFlags[2] = g_ac_dataResp[20] << 24 | g_ac_dataResp[21] << 16
                                             | g_ac_dataResp[22] << 8 | g_ac_dataResp[23];

        /**Calculate the Total Power*/
        l_i_totalPower = g_ac_dataResp[24] << 24 | g_ac_dataResp[25] << 16
                         | g_ac_dataResp[26] << 8 | g_ac_dataResp[27];

        /**Copy the Reserved to the float value*/
        cgtp1->m_totalPower = *(float *) &l_i_totalPower;

        /**Calculate the CH# Power*/
        for (l_i_count = 0; l_i_count < g_i_dataLen - 28; l_i_count += 8)
        {   l_i_power = g_ac_dataResp[28 + l_i_count] << 24
                        | g_ac_dataResp[29 + l_i_count] << 16
                        | g_ac_dataResp[30 + l_i_count] << 8
                        | g_ac_dataResp[31 + l_i_count];

            /**Individual Channel Power*/
            cgtp1->m_channel[l_i_chpwrcnt++].channelPower = *(float *) &l_i_power;
        }

        /**Calculate the CH# freq*/
        for (l_i_count = 0; l_i_count < g_i_dataLen - 28; l_i_count += 8)
        {   l_i_freq = g_ac_dataResp[32 + l_i_count] << 24
                       | g_ac_dataResp[33 + l_i_count] << 16
                       | g_ac_dataResp[34 + l_i_count] << 8
                       | g_ac_dataResp[35 + l_i_count];

            /**Individual Channel Freq*/
            cgtp1->m_channel[l_i_chfreqcnt++].channelFrequency = *(float *) &l_i_freq;
        }
        cgtp1->m_channels = l_i_chfreqcnt;
        /**Clear the Buffer for the Response*/
        memset(g_ac_dataResp, 0x00, sizeof(g_ac_dataResp));

        g_i_dataLen = 0;
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function get the board record information
 *
 * @param cbdi return board record information as a struct cbdi
 * @return On success, returns 0, error code otherwise.
 */

int OcmGetCBDI(T_CBDI *cbdi)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    unsigned int l_ui_temprature = 0;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    unsigned int l_i_ctmModelNUM = 0;
    unsigned int l_i_ctmRevision = 0;
    unsigned int l_i_ctmFirmwareRevision = 0;
    unsigned int l_i_tofRevision = 0;
    unsigned int l_i_tofStartFrequency = 0;
    unsigned int l_i_tofEndFrequency = 0;
    unsigned int l_i_tofInsertionLossCorrection = 0;
    unsigned int l_i_tofPeakDetectionThreshlod = 0;
    unsigned int l_i_tofAppliedSBOC = 0;
    unsigned int l_i_tofAppliedSBTC = 0;
    unsigned int l_i_tofMinimumPeakPower = 0;
    unsigned int l_i_tofReserved1 = 0;
    unsigned int l_i_Reserved1 = 0;
    unsigned int l_i_Reserved2 = 0;


    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   /** Check the Parameter is NULL*/
        if (cbdi == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /** Prepare the command*/
        snprintf(l_ui8_command, strlen((const char *) OCM_API_CBDI_COMMAND) + CRNL_LEN,
                 "%s%s%s",
                 OCM_API_CBDI_COMMAND, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Check the response of the command for IC Error*/
        if (l_ui8_response[0] == 'I' && l_ui8_response[1] == 'C')
        {   l_i_retstatus = OCM_IC_COMMAND_ERROR;
            break;
        }

        /** Check the response of the command for UC Error*/
        if (l_ui8_response[0] == 'U' && l_ui8_response[1] == 'C')
        {   l_i_retstatus = OCM_UC_COMMAND_ERROR;
            break;
        }

        /** convert the ctm model number*/
        cbdi->m_ui32_CTMModelNumber = (l_ui8_response[3] << 24)
                                      | (l_ui8_response[2] << 16) | (l_ui8_response[1] << 8)
                                      | (l_ui8_response[0]);

        /** Get the pcb serial number */
        cbdi->m_i32_PCBSerialNumber = (l_ui8_response[7] << 24)
                                      | (l_ui8_response[6] << 16) | (l_ui8_response[5] << 8)
                                      | (l_ui8_response[4]);

        /** Get the ctm Revision */
        cbdi->m_ui32_CTMRevision = (l_ui8_response[11] << 24)
                                   | (l_ui8_response[10] << 16) | (l_ui8_response[9] << 8)
                                   | (l_ui8_response[8]);

        /** Get the ctm firmware Revision number */
        cbdi->m_ui32_CTMFirmwareVersion = (l_ui8_response[15] << 24)
                                          | (l_ui8_response[14] << 16) | (l_ui8_response[13] << 8)
                                          | (l_ui8_response[12]);

        /** Get the Data Code */
        cbdi->m_ui32_CTMDataCode = (l_ui8_response[19] << 24)
                                   | (l_ui8_response[18] << 16) | (l_ui8_response[17] << 8)
                                   | (l_ui8_response[16]);

        /** Get the Serial number */
        cbdi->m_ui32_tofSerialNumber = (l_ui8_response[23] << 24)
                                       | (l_ui8_response[22] << 16) | (l_ui8_response[21] << 8)
                                       | (l_ui8_response[20]);

        /** Get the Revision */
        cbdi->m_ui32_tofRevision = (l_ui8_response[27] << 24)
                                   | (l_ui8_response[26] << 16) | (l_ui8_response[25] << 8)
                                   | (l_ui8_response[24]);

        /** Get the Start of the Frequency */
        l_i_tofStartFrequency = (l_ui8_response[31] << 24)
                                | (l_ui8_response[30] << 16) | (l_ui8_response[29] << 8)
                                | (l_ui8_response[28]);

        cbdi->m_f_tofStartFrequency = *(float *) &l_i_tofStartFrequency;

        /** Get the End of the Frequency */
        l_i_tofEndFrequency = (l_ui8_response[35] << 24)
                              | (l_ui8_response[34] << 16) | (l_ui8_response[33] << 8)
                              | (l_ui8_response[32]);

        /** Copy the Ending frequency to the double value*/
        cbdi->m_f_tofEndFrequency = *(float *) &l_i_tofEndFrequency;

        /** Get the Insertion loss*/
        l_i_tofInsertionLossCorrection = (l_ui8_response[39] << 24)
                                         | (l_ui8_response[38] << 16) | (l_ui8_response[37] << 8)
                                         | (l_ui8_response[36]);

        /** Copy the Inserion loss  to the double value*/
        cbdi->m_f_tofInsertionLoss = *(float *) &l_i_tofInsertionLossCorrection;

        /** Get the the peak Detection threshold*/
        l_i_tofPeakDetectionThreshlod = (l_ui8_response[43] << 24)
                                        | (l_ui8_response[42] << 16) | (l_ui8_response[41] << 8)
                                        | (l_ui8_response[40]);

        /** Copy the peak Detection threshold to the double value*/
        cbdi->m_f_tofPeakDetectionThresold =
            *(float *) &l_i_tofPeakDetectionThreshlod;

        /** Get the applied SBOC */
        l_i_tofAppliedSBOC = (l_ui8_response[47] << 24)
                             | (l_ui8_response[46] << 16) | (l_ui8_response[45] << 8)
                             | (l_ui8_response[44]);

        /** Copy the Applied SBOC to the double value*/
        // cbdi->m_f_tofAppliedSBOC = *(float *) &l_i_tofAppliedSBOC;
        memcpy(&(cbdi->m_f_tofAppliedSBOC), &l_ui8_response[44], sizeof(float));
        /*Get the applied SBTC*/
        l_i_tofAppliedSBTC = (l_ui8_response[51] << 24)
                             | (l_ui8_response[50] << 16) | (l_ui8_response[49] << 8)
                             | (l_ui8_response[48]);

        /** Copy the Applied SBTC to the double value*/
        //cbdi->m_f_tofAppliedSBTC = *(float *) &l_i_tofAppliedSBTC;
        memcpy(&(cbdi->m_f_tofAppliedSBTC), &l_ui8_response[48], sizeof(float));

        /** Get the Minimum Peak Power */
        l_i_tofMinimumPeakPower = (l_ui8_response[55] << 24)
                                  | (l_ui8_response[54] << 16) | (l_ui8_response[53] << 8)
                                  | (l_ui8_response[52]);

        /** Copy the Minimum Peak Power to the double value */
        cbdi->m_f_tofMinimumPower = *(float *) &l_i_tofMinimumPeakPower;

        /** Get the Reserved val */
        l_i_tofReserved1 = (l_ui8_response[59] << 24)
                           | (l_ui8_response[58] << 16) | (l_ui8_response[57] << 8)
                           | (l_ui8_response[56]);

        /** Copy the Reserved to the double value */
        cbdi->m_f_tofReserved1 = *(float *) &l_i_tofReserved1;

        /** Get the Reserved val */
        l_i_Reserved1 = (l_ui8_response[63] << 24) | (l_ui8_response[62] << 16)
                        | (l_ui8_response[61] << 8) | (l_ui8_response[60]);

        /** Copy the Reserved to the double value */
        cbdi->m_f_reserved1 = *(float *) &l_i_Reserved1;

        /** Get the Reserved val */
        l_i_Reserved2 = (l_ui8_response[67] << 24) | (l_ui8_response[66] << 16)
                        | (l_ui8_response[65] << 8) | (l_ui8_response[64]);

        /** Copy the Reserved to the double value */
        cbdi->m_f_reserved2 = *(float *) &l_i_Reserved2;

        /** Convert the CTX Vendor Hex String to the ASCII **/
        memset(cbdi->m_aui8_CTMVendor, 0x00,sizeof(cbdi->m_aui8_CTMVendor));
        memcpy(cbdi->m_aui8_CTMVendor, l_ui8_response + 68,20);

        /** Convert the CTX Serial Number Hex String to the ASCII **/
        memset(cbdi->m_aui8_CTMSerialNumber, 0x00,sizeof(cbdi->m_aui8_CTMSerialNumber));
        memcpy(cbdi->m_aui8_CTMSerialNumber, l_ui8_response + 88,32);

        /** Remove consecutive space*/
        remove_space(cbdi->m_aui8_CTMSerialNumber);

        /*Remove trailing spaces*/
        trimTrailing(cbdi->m_aui8_CTMSerialNumber);

        /** Copy the CRC to the Struct */
        cbdi->m_ui16_crc16 = (l_ui8_response[121] << 8) | (l_ui8_response[120]);

        if (g_ui8_debug_ocm)
        {   printf("CTM Model No : %u\n"
                   "PCB Serial Number : %d\n"
                   "CTM Revision : %u\n"
                   "CTM Firmware Version : %u\n"
                   "CTM data Code : %d\n"
                   "tof Serial Number : %d\n"
                   "tof Revision : %u\n"
                   "tof start Frequency : %f THz\n"
                   "tof end Frequency : %f THz\n"
                   "tof Insertion loss : %f\n"
                   "tof peak detection thresold : %f\n"
                   "tof apllied SBOC : %f GHz\n"
                   "tof Applied SBTC : %f GHz\n"
                   "tof Minimum Power: %f\n"
                   "tof Reserved1 : %f\n"
                   "Reserved 1 : %f\n"
                   "Reserved 2 : %f\n"
                   "CTM Vendor : %s\n"
                   "CTM CTM Serial Number : %s\n"
                   "crc16 : %02x\n", cbdi->m_ui32_CTMModelNumber,
                   cbdi->m_i32_PCBSerialNumber, cbdi->m_ui32_CTMRevision,
                   cbdi->m_ui32_CTMFirmwareVersion, cbdi->m_ui32_CTMDataCode,
                   cbdi->m_ui32_tofSerialNumber, cbdi->m_ui32_tofRevision,
                   cbdi->m_f_tofStartFrequency, cbdi->m_f_tofEndFrequency,
                   cbdi->m_f_tofInsertionLoss,
                   cbdi->m_f_tofPeakDetectionThresold,
                   cbdi->m_f_tofAppliedSBOC, cbdi->m_f_tofAppliedSBTC,
                   cbdi->m_f_tofMinimumPower, cbdi->m_f_tofReserved1,
                   cbdi->m_f_reserved1, cbdi->m_f_reserved2,
                   cbdi->m_aui8_CTMVendor, cbdi->m_aui8_CTMSerialNumber,
                   cbdi->m_ui16_crc16);
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
/**
 * @brief This function gets the total power in band for TOF1
 *
 * @param totPower return total power as a float normalized to 1.0 mW
 * @param o_ui32_StatusWord returns the 32-bit Status word value
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 * @response_format :mW, for dBm use "10*log10(totPower)"
 */
int OcmGetCTPR1(float *totPower, uint32_t *o_ui32_StatusWord)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    unsigned int l_ui_totPower = 0;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   /** Check the NULL Value*/
        if (totPower == NULL)
        {   l_i_retstatus = EINVAL;
            break;

        }

        /** Prepare the command*/
        snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_CTPR1_COMMAND) + CRNL_LEN, "%s%s%s%s",
                 NL, OCM_API_CTPR1_COMMAND, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Copy the Response to the Global Buffer */
        memcpy(g_ac_dataResp, l_ui8_response, l_ui_respLen);

        /*Calculate the Response*/
        if ((l_i_retstatus = createResponse(l_ui8_response, l_ui_respLen, o_ui32_StatusWord))
                != 0)
        {   break;
        }

        /**Check the Status word value*/
        if( *o_ui32_StatusWord > 0)
        {   l_i_retstatus = OCM_INTERNAL_STATUS_ERROR;
        }

        /*Get the Reserved val*/
        l_ui_totPower = (g_ac_dataResp[0] << 24) | (g_ac_dataResp[1] << 16)
                        | (g_ac_dataResp[2] << 8) | (g_ac_dataResp[3]);

        //*Copy the Reserved to the float value*/
        *totPower = *(float *) &l_ui_totPower;
        /*Clear the Buffer for the Response*/
        memset(g_ac_dataResp, 0x00, sizeof(g_ac_dataResp));

        g_i_dataLen = 0;
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
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

int OcmDoCENB1(T_TOFSCAN_MODE scanMode, uint32_t *scanSetting, uint32_t *o_ui32_StatusWord)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   /** Check the NULL Value*/
        if (scanSetting == NULL || scanMode > 2 || scanMode < 0)
        {   l_i_retstatus = EINVAL;
            break;

        }

        /** Prepare the command*/
        snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_CENB1_COMMAND) + CRNL_LEN + 2, "%s%s%c%d%s%s",
                 NL, OCM_API_CENB1_COMMAND,' ',scanMode, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Copy the Response to the Global Buffer */
        memcpy(g_ac_dataResp, l_ui8_response, l_ui_respLen);

        /*Calculate the Response*/
        if ((l_i_retstatus = createResponse(l_ui8_response, l_ui_respLen, o_ui32_StatusWord))
                != 0)
        {   break;
        }

        /**Check the Status word value*/
        if( *o_ui32_StatusWord > 0)
        {   l_i_retstatus = OCM_INTERNAL_STATUS_ERROR;
        }

        /*Get the Reserved val*/
        *scanSetting = (g_ac_dataResp[0] << 24) | (g_ac_dataResp[1] << 16)
                       | (g_ac_dataResp[2] << 8) | (g_ac_dataResp[3]);


        /*Clear the Buffer for the Response*/
        memset(g_ac_dataResp, 0x00, sizeof(g_ac_dataResp));

        g_i_dataLen = 0;
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function gets the full channel report. Total power in mW. Channel powers in Q8 dBm.
 * @param cfcr return the full channel report as type T_CFCR.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 * @response_format
 * 		Total Pow:  total power, mW
 * 		m_channels: number of channels
 * 		m_Peak_Detection_Flags: channel peak detection flag (0 no peak detected / 1 peak detected)
 * 		m_peakTypeFlags:  channel data format flags (0 – 10 Gbps / 1 – 40+ Gbps)
 * 		channelPowerITU:  channel power integrated around channel center frequency, Q8 format
 * 		channelPowerPeak: channel power integrated around channel peak frequency, Q8 format
 * 		channelFrequency:  channel frequency: scaled THz  (FreqTHz = 190+channelFrequency*0.0001)
 */
int OcmGetCFCR(T_CFCR *cfcr)
{   int l_i_retstatus = 0;
    int l_i_count = 0;
    int l_i_chCountNo = 0;
    int l_i_chfreqcnt = 0;
    int l_i_chpwrcnt = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    unsigned int l_ui_temprature = 0;
    unsigned int l_i_totalPower = 0;
    uint16_t l_i_freq = 0;
    uint16_t l_i_powerpeak = 0;
    uint16_t l_i_poweritu = 0;

    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };
    int i_i_dataLen = 0;
    int i_i_padLen = 0;
    unsigned short l_s_crcout = 0;
    unsigned short l_s_framecrc = 0;
    int l_i_errorVal = 0;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   if (cfcr == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /** Prepare the command*/
        snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_CFCR_COMMAND) + CRNL_LEN, "%s%s%s%s",
                 NL, OCM_API_CFCR_COMMAND, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Copy the Response to the Global Buffer */
        memcpy(g_ac_dataResp, l_ui8_response, l_ui_respLen);

        /*Check the NULL Response*/
        if (l_ui8_response == NULL || l_ui_respLen == 0)
        {   l_i_retstatus = OCM_NO_RESPONCE_RECEIVED;
            break;
        }

        /*Check the response of the command for IC Error*/
        if (l_ui8_response[0] == 'I' && l_ui8_response[1] == 'C')
        {   l_i_retstatus = OCM_IC_COMMAND_ERROR;
            break;
        }

        /*Check the response of the command for UC Error*/
        if (l_ui8_response[0] == 'U' && l_ui8_response[1] == 'C')
        {   l_i_retstatus = OCM_UC_COMMAND_ERROR;
            break;
        }

        /*Check the first byte*/
        if (l_ui8_response[0] != 0x0a || l_ui8_response[1] != 0x0a)
        {   l_i_retstatus = OCM_WRONG_FIRST_BYTE;
            break;
        }

        /*check the error code for the code*/
        cfcr->m_ui32_ChkErrorStatus = (l_ui8_response[5] << 24) | (l_ui8_response[4] << 16)
                                      | (l_ui8_response[3] << 8) | l_ui8_response[2];

        /**Check the Status word value*/
        if(cfcr->m_ui32_ChkErrorStatus > 0)
        {   l_i_retstatus = OCM_INTERNAL_STATUS_ERROR;
        }

        /*Calculate the data length*/
        i_i_dataLen = (l_ui8_response[7] << 8) | l_ui8_response[6];

        /*Calculate the padding Byte*/
        if (l_ui_respLen > 10 && (!(l_ui_respLen % 4)))   /*Calculate the padding*/
        {   i_i_padLen = l_ui_respLen - 10 - i_i_dataLen;

        }

        /*Calculate the total data Len*/
        g_i_dataLen = l_ui_respLen - 10 - i_i_padLen;

        /*Clear the global Buffer */
        memset(g_ac_dataResp, 0x00, sizeof(g_ac_dataResp));

        l_s_framecrc = (l_ui8_response[g_i_dataLen + 9] << 8)
                       | l_ui8_response[g_i_dataLen + 8];

        /*Prepare the data*/
        memcpy(g_ac_dataResp, &l_ui8_response[8], g_i_dataLen);

        /*Calculate the crc16 for the Data*/
        crc16(&l_s_crcout, 0xffff, g_ac_dataResp, g_i_dataLen);

        l_i_totalPower = g_ac_dataResp[3] << 24 | g_ac_dataResp[2] << 16
                         | g_ac_dataResp[1] << 8 | g_ac_dataResp[0];

        /**Copy the Reserved to the float value*/
        cfcr->m_totalPower = *(float *) &l_i_totalPower;
        /*********************************Peak Detection*****************/
        /**Peak Detect 1*/
        cfcr->m_peakDetectionFlags[0] = g_ac_dataResp[7] << 24 | g_ac_dataResp[6] << 16
                                        | g_ac_dataResp[5] << 8 | g_ac_dataResp[4];

        /**Peak Detect 2*/
        cfcr->m_peakDetectionFlags[1] = g_ac_dataResp[11] << 24 | g_ac_dataResp[10] << 16
                                        | g_ac_dataResp[9] << 8 | g_ac_dataResp[8];

        /**Peak Detect 3*/
        cfcr->m_peakDetectionFlags[2] = g_ac_dataResp[15] << 24 | g_ac_dataResp[14] << 16
                                        | g_ac_dataResp[13] << 8 | g_ac_dataResp[12];


        cfcr->m_peakTypeFlags[0] = g_ac_dataResp[19] << 24 | g_ac_dataResp[18] << 16
                                   | g_ac_dataResp[17] << 8 | g_ac_dataResp[16];

        /**Peak Detect 2*/
        cfcr->m_peakTypeFlags[1] = g_ac_dataResp[23] << 24 | g_ac_dataResp[22] << 16
                                   | g_ac_dataResp[21] << 8 | g_ac_dataResp[20];

        /**Peak Detect 3*/
        cfcr->m_peakTypeFlags[2] = g_ac_dataResp[27] << 24 | g_ac_dataResp[26] << 16
                                   | g_ac_dataResp[25] << 8 | g_ac_dataResp[24];

        for (l_i_count = 0; l_i_count < g_i_dataLen - 28; l_i_count += 6)
        {   l_i_poweritu = g_ac_dataResp[29 + l_i_count] << 8 | g_ac_dataResp[28 + l_i_count];
            l_i_powerpeak = g_ac_dataResp[31 + l_i_count] << 8| g_ac_dataResp[30 + l_i_count];
            l_i_freq = g_ac_dataResp[33 + l_i_count] << 8| g_ac_dataResp[32 + l_i_count];

            /**Individual Channel Power*/
            (cfcr->m_channel[l_i_chpwrcnt]).channelPowerITU = l_i_poweritu;
            (cfcr->m_channel[l_i_chpwrcnt]).channelPowerPeak = l_i_powerpeak;
            (cfcr->m_channel[l_i_chpwrcnt++]).channelFrequency = l_i_freq;

        }
        cfcr->m_channels = l_i_chpwrcnt;

        /**Clear the Buffer for the Response*/
        memset(g_ac_dataResp, 0x00, sizeof(g_ac_dataResp));

        g_i_dataLen = 0;
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
/**
* @brief This function gets the control status information
*
* @param ctmStatus return control status information as an unsigned 32-bit integer.
*
* @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
*/

int OcmGetCSTS0(uint32_t *ctmStatus)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    unsigned int l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   if (ctmStatus == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_CSTS0_COMMAND) + CRNL_LEN, "%s%s%s%s",
                 NL, OCM_API_CSTS0_COMMAND, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        *ctmStatus = l_ui8_response[3] << 24 | l_ui8_response[2] << 16 | l_ui8_response[1] << 8 | l_ui8_response[0] ;
    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

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
int OcmGetCGSP1(T_SLICE_WIDTH sliceWidth, T_SPECTRUM_FORMAT spectrumFormat, T_EDGE_BINS edgeBins, T_CGSP1 *cgsp1)
{   int l_i_retstatus = 0;
    int l_i_count = 0;
    int l_i_slices = 0;
    int l_i_chfreqcnt = 0;
    int l_i_chpwrcnt = 0;
    uint32_t timeout = 1000000;
    uint l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    int l_ui_sCount = 0;
    unsigned int l_i_totalPower = 0;
    unsigned int l_i_freq = 0;
    unsigned int l_i_power = 0;
    unsigned int l_i_swidth = 0;
    unsigned int l_i_peakDtc[3] = { 0 };
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_command_prep[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   if (cgsp1 == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /*Preparing Command as per Arguments*/
        if(sliceWidth == 0 && spectrumFormat == 0)
        {   snprintf(l_ui8_command_prep,
                     strlen((const char *) OCM_API_CGSP1_COMMAND) + 1, "%s",OCM_API_CGSP1_COMMAND);
        }
        else if (sliceWidth == 0 && spectrumFormat == 1)
        {   snprintf(l_ui8_command_prep,
                     strlen((const char *) OCM_API_CGSP1_COMMAND) + 2, "%s%c",OCM_API_CGSP1_COMMAND,'A');
        }
        else if (sliceWidth == 1 && spectrumFormat == 0)
        {   snprintf(l_ui8_command_prep,
                     strlen((const char *) OCM_API_CGSP1_COMMAND) + 2, "%s%c",OCM_API_CGSP1_COMMAND,'S');
        }
        else if(sliceWidth == 1 && spectrumFormat == 1)
        {   snprintf(l_ui8_command_prep,
                     strlen((const char *) OCM_API_CGSP1_COMMAND) + 2, "%s%c",OCM_API_CGSP1_COMMAND,'T');
        }
        else if(sliceWidth == 2 && spectrumFormat == 1)
        {   snprintf(l_ui8_command_prep,
                     strlen((const char *) OCM_API_CGSP1_COMMAND) + 2, "%s%c",OCM_API_CGSP1_COMMAND,'U');
        }
        else
        {   l_i_retstatus = OCM_UC_COMMAND_ERROR;
            break;
        }
        switch(edgeBins)
        {   case EDGE_NONE :
                snprintf(l_ui8_command,
                         strlen((const char *) l_ui8_command_prep) + CRNL_LEN, "%s%s%s%s",NL,l_ui8_command_prep, CR, NL);
                break;
            case EDGE_BOS :
                snprintf(l_ui8_command,
                         strlen((const char *) l_ui8_command_prep) + CRNL_LEN + 1, "%s%s%c%s%s",NL,l_ui8_command_prep,'1', CR, NL);
                break;

            case EDGE_EOS :
                snprintf(l_ui8_command,
                         strlen((const char *) l_ui8_command_prep) + CRNL_LEN + 1, "%s%s%c%s%s",NL,l_ui8_command_prep,'2', CR, NL);
                break;
            case EDGE_BOS_EOS :
                snprintf(l_ui8_command,
                         strlen((const char *) l_ui8_command_prep) + CRNL_LEN + 1, "%s%s%c%s%s",NL,l_ui8_command_prep,'3', CR, NL);
                break;

            case EDGE_BOS_EOS_1 :
                snprintf(l_ui8_command,
                         strlen((const char *) l_ui8_command_prep) + CRNL_LEN + 1, "%s%s%c%s%s",NL,l_ui8_command_prep,'4', CR, NL);
                break;
            case EDGE_BOS_EOS_2 :
                snprintf(l_ui8_command,
                         strlen((const char *) l_ui8_command_prep) + CRNL_LEN + 1, "%s%s%c%s%s",NL,l_ui8_command_prep,'5', CR, NL);
                break;

            default:
                l_i_retstatus = OCM_UC_COMMAND_ERROR;
                break;
        }

        /** Call the read/write command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command) + 1,
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Copy the Response to the Global Buffer */
        memcpy(g_ac_dataResp, l_ui8_response, l_ui_respLen);

        /**Create the Response*/
        if (0 != (l_i_retstatus = createResponse(l_ui8_response, l_ui_respLen, &cgsp1->m_ui32_ChkErrorStatus)))
        {   break;
        }

        /**Check the Status word value*/
        if(cgsp1->m_ui32_ChkErrorStatus > 0)
        {   l_i_retstatus = OCM_INTERNAL_STATUS_ERROR;
        }

        /*Calculating total power from response bytes*/
        l_i_totalPower = g_ac_dataResp[4] << 24 | g_ac_dataResp[5] << 16
                         | g_ac_dataResp[6] << 8 | g_ac_dataResp[7];
        cgsp1->m_totalPower = *(float *) &l_i_totalPower;

        /*Calculating Number of Slices from response bytes*/
        l_ui_sCount = g_ac_dataResp[8] << 24 | g_ac_dataResp[9] << 16
                      | g_ac_dataResp[10] << 8 | g_ac_dataResp[11];

        cgsp1->m_slices = (int)l_ui_sCount;

        /*Calculating Start Frequency from response bytes*/
        l_i_freq =  g_ac_dataResp[12] << 24 | g_ac_dataResp[13] << 16
                    | g_ac_dataResp[14] << 8 | g_ac_dataResp[15];
        cgsp1->m_startFreq = *(float *) &l_i_freq;

        /*Calculating Slice Width from response bytes*/
        l_i_swidth = g_ac_dataResp[16] << 24 | g_ac_dataResp[17] << 16
                     | g_ac_dataResp[18] << 8 | g_ac_dataResp[19];
        cgsp1->m_sliceWidth = *(float *) &l_i_swidth;

        /*Calculating Scan Counter from response bytes if edge bin is exist*/
        if(edgeBins != 0)
        {   l_ui_sCount = 0;
            l_ui_sCount = g_ac_dataResp[20] << 24 | g_ac_dataResp[21] << 16
                          | g_ac_dataResp[22] << 8 | g_ac_dataResp[23];

            cgsp1->	m_scanCounter = (uint32_t)l_ui_sCount;
            l_ui_sCount = 24;
        }
        else
        {   cgsp1->	m_scanCounter = 0;
            l_ui_sCount = 20;
        }

        cgsp1->m_slicePower = (float*)malloc(8192*sizeof(float));
        if(NULL == cgsp1->m_slicePower)
        {   l_i_retstatus = OCM_MEMORY_ALLOCATION_FAILED;
            break;
        }
        /*Calculating individual slice power from response bytes*/

        /*float32 Scaled mW Format*/
        if ((sliceWidth == 0 || sliceWidth == 1) && (spectrumFormat == 0))
        {   l_i_chpwrcnt = 0;
            for (l_i_count = 0; l_i_count < g_i_dataLen - l_ui_sCount; l_i_count += 4)
            {   l_i_power = g_ac_dataResp[l_ui_sCount + l_i_count] << 24 | g_ac_dataResp[l_ui_sCount + 1 +l_i_count] << 16
                            | g_ac_dataResp[l_ui_sCount + 2 +l_i_count] << 8 | g_ac_dataResp[l_ui_sCount+ 3 +l_i_count] ;

                cgsp1->m_slicePower[l_i_chpwrcnt++] =  *(float *) &l_i_power;

            }
        }
        /*Q8 frame Format*/
        else if ((sliceWidth == 0 || sliceWidth == 1 || sliceWidth == 2) && (spectrumFormat == 1))
        {   for (l_i_count = 0; l_i_count < g_i_dataLen - 24; l_i_count += 2)
            {   cgsp1->m_slicePower[l_i_chpwrcnt++] =  q8_converter(g_ac_dataResp[l_ui_sCount+l_i_count],g_ac_dataResp[l_ui_sCount + 1+l_i_count]);

            }
        }

        else
        {   l_i_retstatus = OCM_IC_COMMAND_ERROR;
            break;
        }
        cgsp1->m_slices = l_i_chpwrcnt;
    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
/**
 * @brief This function initiates a soft reset of the CTM
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetCRST(void)
{   int l_i_retstatus = 0;
    uint32_t timeout = 10000000;
    unsigned int l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_CRST_COMMAND) + CRNL_LEN + 1, "%s%s%s%s",
                 NL, OCM_API_CRST_COMMAND, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }


    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
/**
 * @brief This function enables/disables CTM Demo mode
 *
 * @param enableDemoMode sets demo mode if true; sets normal operating mode if false.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetCDMO(bool enableDemoMode)
{   int l_i_retstatus = 0;
    uint32_t timeout = 1000000;
    unsigned int l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_CDMO_COMMAND)+ 1 + CRNL_LEN, "%s%s%d%s%s",
                 NL, OCM_API_CDMO_COMMAND,enableDemoMode, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }
    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
/**
 * @brief This function sets the front-end gain to HIGH or LOW for TOF1
 *
 * @param highGain sets GNHI1 if true; sets GNLO1 if false.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetGain(bool highGain)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    unsigned int l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   if(highGain)
        {   snprintf(l_ui8_command,
                     strlen((const char *) OCM_API_GNHI1_COMMAND)+ 1 + CRNL_LEN, "%s%s%s%s",
                     NL, OCM_API_GNHI1_COMMAND, CR, NL);
        }
        else
        {   snprintf(l_ui8_command,
                     strlen((const char *) OCM_API_GNLO1_COMMAND)+ 1 + CRNL_LEN, "%s%s%s%s",
                     NL, OCM_API_GNLO1_COMMAND, CR, NL);
        }

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }
    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
/**
 * @brief This function sets minimum peak power value.
 *
 * @param minimumPeakPower sets the value as a float in linear units (mW).
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetCMPP1(float minimumPeakPower)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    unsigned int l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   if((minimumPeakPower >= 0.00001) && (minimumPeakPower <= 0.031622))
        {   snprintf(l_ui8_command,
                     strlen((const char *) OCM_API_CMPP1_COMMAND)+ 8 + CRNL_LEN, "%s%s%f%s%s",
                     NL, OCM_API_CMPP1_COMMAND,minimumPeakPower, CR, NL);
        }
        else
        {   l_i_retstatus = OCM_MINIMUM_PEAK_POWER_INVALID;
            break;
        }

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }
    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
/**
 * @brief This function sets insertion loss correction value.
 *
 * @param insertionLoss sets the value as a float in linear units (mW).
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetCSIL1(float inserttionLoss)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    unsigned int l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   if((inserttionLoss >= -100000) && (inserttionLoss <= 100000))
        {   sprintf(l_ui8_command,"%s%s%f%s%s",
                    NL, OCM_API_CSIL1_COMMAND,inserttionLoss, CR, NL);
        }
        else
        {   l_i_retstatus = OCM_INSERTION_LOSS_INVALID;
            break;
        }

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }
    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
/**
 * @brief This function sets peak detection threshold value.
 *
 * @param peakDetectThreshold sets the value as a float in linear units (mW).
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetCSPT1(float peakDetectThreshold)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    unsigned int l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   if((peakDetectThreshold >= 0.001) && (peakDetectThreshold <= 0.3162))
        {   snprintf(l_ui8_command,
                     strlen((const char *) OCM_API_CSPT1_COMMAND)+ 8 + CRNL_LEN, "%s%s%f%s%s",
                     NL, OCM_API_CSPT1_COMMAND,peakDetectThreshold, CR, NL);
        }
        else
        {   l_i_retstatus = OCM_INSERTION_LOSS_INVALID;
            break;
        }

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }
    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function gets channel powers integrated over 0.2 nm, 0.4 nm or 0.8 nm.
 *
 * @param integration specified the integration vaslue as type T_INTEGRATION.
 * @param cget1 return the optical spectrum as type T_CGET1.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetCGET1(T_INTEGRATE integration, T_CGET1 *cget1)
{   int l_i_retstatus = 0;
    int l_i_count = 0;
    int l_i_chfreqcnt = 0;
    int l_i_chpwrcnt = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    unsigned int l_i_freq = 0;
    unsigned int l_i_power = 0;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   if (cget1 == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /** Prepare the command*/
        if((integration >= 0) && (integration < 3))
        {   snprintf(l_ui8_command,
                     strlen((const char *) OCM_API_CGET1_COMMAND) + 1 +CRNL_LEN, "%s%s%d%s%s",
                     NL, OCM_API_CGET1_COMMAND,integration, CR, NL);
        }
        else
        {   l_i_retstatus = OCM_TOF_DATA_INVALID_ERROR;
            break;
        }

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Copy the Response to the Global Buffer */
        memcpy(g_ac_dataResp, l_ui8_response, l_ui_respLen);

        /**Calculate the Response*/
        if ((l_i_retstatus = createResponse(l_ui8_response, l_ui_respLen, &cget1->m_ui32_ChkErrorStatus))
                != 0)
        {   break;
        }

        /**Check the Status word value*/
        if(cget1->m_ui32_ChkErrorStatus > 0)
        {   l_i_retstatus = OCM_INTERNAL_STATUS_ERROR;
        }

        /*********************************Peak Detection*****************/
        /**Peak Detect 1*/
        cget1->m_peakDetectionFlags[0] = g_ac_dataResp[0] << 24 | g_ac_dataResp[1] << 16
                                         | g_ac_dataResp[2] << 8 | g_ac_dataResp[3];

        /**Peak Detect 2*/
        cget1->m_peakDetectionFlags[1] = g_ac_dataResp[4] << 24 | g_ac_dataResp[5] << 16
                                         | g_ac_dataResp[6] << 8 | g_ac_dataResp[7];

        /**Peak Detect 3*/
        cget1->m_peakDetectionFlags[2] = g_ac_dataResp[8] << 24 | g_ac_dataResp[9] << 16
                                         | g_ac_dataResp[10] << 8 | g_ac_dataResp[11];

        /**Calculate the CH# Power*/
        for (l_i_count = 0; l_i_count < g_i_dataLen - 12; l_i_count += 8)
        {   l_i_power = g_ac_dataResp[12 + l_i_count] << 24
                        | g_ac_dataResp[13 + l_i_count] << 16
                        | g_ac_dataResp[14 + l_i_count] << 8
                        | g_ac_dataResp[15 + l_i_count];

            /**Individual Channel Power*/
            cget1->m_channel[l_i_chpwrcnt++].channelPower = *(float *) &l_i_power;
        }

        /**Calculate the CH# freq*/
        for (l_i_count = 0; l_i_count < g_i_dataLen - 12; l_i_count += 8)
        {   l_i_freq = g_ac_dataResp[16 + l_i_count] << 24
                       | g_ac_dataResp[17 + l_i_count] << 16
                       | g_ac_dataResp[18 + l_i_count] << 8
                       | g_ac_dataResp[19 + l_i_count];

            /**Individual Channel Freq*/
            cget1->m_channel[l_i_chfreqcnt++].channelFrequency = *(float *) &l_i_freq;

        }
        cget1->m_channels = l_i_chfreqcnt;

        /**Clear the Buffer for the Response*/
        memset(g_ac_dataResp, 0x00, sizeof(g_ac_dataResp));

        g_i_dataLen = 0;
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
/**
 * @brief This function gets the control dual data format
 * @param cddfb return the full control dual data as type T_CDDFB.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetCDDFB(T_CDDFB *cddfb)
{   int l_i_retstatus = 0;
    int l_i_count = 0;
    int l_i_chfreqcnt = 0;
    int l_i_chpwrcnt = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    unsigned int l_i_totalPower = 0;
    unsigned int l_i_freq = 0;
    unsigned int l_i_power = 0;
    unsigned int l_i_swidth = 0;
    unsigned int l_i_peakDtc[3] = { 0 };
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_command_prep[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   if (cddfb == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /*Preparing Command as per Arguments*/
        snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_CDDFB_COMMAND)  +CRNL_LEN, "%s%s%s%s",
                 NL, OCM_API_CDDFB_COMMAND, CR, NL);

        /** Call the read/write command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command) + 1,
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Copy the Response to the Global Buffer */
        memcpy(g_ac_dataResp, l_ui8_response, l_ui_respLen);

        /**Create the Response*/
        if (0 != (l_i_retstatus = createResponse(l_ui8_response, l_ui_respLen, &cddfb->m_ui32_ChkErrorStatus)))
        {   break;
        }

        /**Check the Status word value*/
        if(cddfb->m_ui32_ChkErrorStatus > 0)
        {   l_i_retstatus = OCM_INTERNAL_STATUS_ERROR;
        }

        /*Calculating total power from response bytes*/
        l_i_totalPower = g_ac_dataResp[0] << 24 | g_ac_dataResp[1] << 16
                         | g_ac_dataResp[2] << 8 | g_ac_dataResp[3];
        cddfb->m_totalPower = *(float *) &l_i_totalPower;
        /*********************************Peak Detection*****************/

        /**Peak Detect 1*/
        cddfb->m_peakDetectionFlags[0] = g_ac_dataResp[4] << 24 | g_ac_dataResp[5] << 16
                                         | g_ac_dataResp[6] << 8 | g_ac_dataResp[7];

        /**Peak Detect 2*/
        cddfb->m_peakDetectionFlags[1] = g_ac_dataResp[8] << 24 | g_ac_dataResp[9] << 16
                                         | g_ac_dataResp[10] << 8 | g_ac_dataResp[11];

        /**Peak Detect 3*/
        cddfb->m_peakDetectionFlags[2] = g_ac_dataResp[12] << 24 | g_ac_dataResp[13] << 16
                                         | g_ac_dataResp[14] << 8 | g_ac_dataResp[15];

        /************************m_peakTypeFlagsDetection*****************/

        /**Saturation Detect 1*/
        cddfb->m_peakTypeFlags[0] = g_ac_dataResp[16] << 24 | g_ac_dataResp[17] << 16
                                    | g_ac_dataResp[18] << 8 | g_ac_dataResp[19];

        /**Saturation Detect 2*/
        cddfb->m_peakTypeFlags[1] = g_ac_dataResp[20] << 24 | g_ac_dataResp[21] << 16
                                    | g_ac_dataResp[22] << 8 | g_ac_dataResp[23];

        /**Saturation Detect 3*/
        cddfb->m_peakTypeFlags[2] = g_ac_dataResp[24] << 24 | g_ac_dataResp[25] << 16
                                    | g_ac_dataResp[26] << 8 | g_ac_dataResp[27];

        /*********************************Saturation Detection*****************/

        /**Saturation Detect 1*/
        cddfb->m_channelSaturationFlags[0] = g_ac_dataResp[28] << 24 | g_ac_dataResp[29] << 16
                                             | g_ac_dataResp[30] << 8 | g_ac_dataResp[31];

        /**Saturation Detect 2*/
        cddfb->m_channelSaturationFlags[1] = g_ac_dataResp[32] << 24 | g_ac_dataResp[33] << 16
                                             | g_ac_dataResp[34] << 8 | g_ac_dataResp[35];

        /**Saturation Detect 3*/
        cddfb->m_channelSaturationFlags[2] = g_ac_dataResp[36] << 24 | g_ac_dataResp[37] << 16
                                             | g_ac_dataResp[38] << 8 | g_ac_dataResp[39];

        l_i_chpwrcnt = 0;
        for (l_i_count = 0; l_i_count < g_i_dataLen - 40; l_i_count += 8)
        {   cddfb->m_channel[l_i_chpwrcnt].channelPower2nm =  g_ac_dataResp[40 +l_i_count] << 8 | g_ac_dataResp[41 +l_i_count];

            cddfb->m_channel[l_i_chpwrcnt].channelPower4nm =  g_ac_dataResp[42 +l_i_count] << 8 | g_ac_dataResp[43 +l_i_count];

            l_i_freq = g_ac_dataResp[44 + l_i_count] << 24 | g_ac_dataResp[45 +l_i_count] << 16
                       | g_ac_dataResp[46 +l_i_count] << 8 | g_ac_dataResp[47 +l_i_count] ;

            cddfb->m_channel[l_i_chpwrcnt++].channelFrequency =  *(float *) &l_i_freq;
        }
        cddfb->m_channels = l_i_chpwrcnt;

        /**Clear the Buffer for the Response*/
        memset(g_ac_dataResp, 0x00, sizeof(g_ac_dataResp));

        g_i_dataLen = 0;

    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

int OcmUpdateProductInfo()
{   FILE *fp = NULL;
    char buffer[BUF] = {0};
    char *buf = NULL;
    int  ret_api = 0;
    char l_c_ocm_ver[128] = {0};

    json_bool ret_json_val = 0;
    struct json_object *parsed_json = NULL;
    struct json_object *parsed_json1 = NULL;
    struct json_object *root = NULL;
    struct json_object *subroot = NULL;
    struct json_object *subroot1 = NULL;
    struct json_object *leaf = NULL;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    do
    {   /* opening file & parsing string to json object*/
        fp = fopen(PROD_INFO_FILE_PATH,"r+");
        if(fp == NULL)
        {   ret_api = OCM_NULL_POINTER;
            break;
        }
        fread(buffer, BUF, 1, fp);

        root = json_tokener_parse(buffer);
        if(root == NULL)
        {   ret_api = OCM_JSON_PARSE_ERROR;
            break;
        }

        do
        {   ret_json_val = json_object_object_get_ex(root, "XMFH", &subroot);
            if(!ret_json_val)
            {   ret_api = OCM_JSON_PARSE_ERROR;
                break;
            }

            buf = strdup(json_object_get_string(subroot));

            /* Parsing subobject */
            parsed_json = json_tokener_parse(buf);
            if (buf != NULL)
            {   free(buf);
                buf = NULL;
            }
            /************ Update version for OCM ************/
            ret_api = json_object_object_get_ex(subroot, "OCM", &subroot1);
            if(!ret_api)
            {   ret_api = OCM_JSON_PARSE_ERROR;
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
            {   /*Getting Firmware Revision from the CTM*/
                ret_api = OcmGetDREV((char *)l_c_ocm_ver);
                if(ret_api)
                {   break;
                }

                if (strstr((char *) l_c_ocm_ver, (char *)"Compiled") == NULL)
                {   l_c_ocm_ver[0] = '\0';
                }

                /* Add version details in leaf */
                leaf=json_object_new_string (l_c_ocm_ver);
                /* Adding change in respective subobject */
                json_object_object_add(parsed_json1, "Firmware Ver",leaf);
            }
            while(0);
            /* Adding change in root object */
            json_object_object_add(parsed_json, "OCM", parsed_json1);
            /* Adding change in root object */
            json_object_object_add(root, "XMFH", parsed_json);
        }
        while(0);

        strcpy(buffer, json_object_get_string(root)); //json object convert into string and copy to buffer

        if (fp != NULL)
            fclose(fp);

        fp = fopen(PROD_INFO_FILE_PATH,"w+");
        if(fp == NULL)
        {   ret_api = errno;
            break;
        }
        fwrite(buffer, 1, strlen(buffer), fp); //write in file  with new changes
    }
    while(0);

    if (fp != NULL)
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

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, ret_api);

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
* @brief The function Upgrades the OCM Firmware.
*
* @param  i_ui8_fileName OCM Firmware file name, which is expected at /tmp/ location.
*
* @return On success returns OCM_COMMAND_SUCCESS and error code otherwise.
*/
int OcmFirmwareUpgrade(uint8_t *i_ui8_fileName)
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    uint8_t l_ui8_file[256] = { 0 };
    uint8_t l_puc_cmd[256] = { 0 };
    char l_pc_ext[8] = {0};

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s. File name: %s]", __func__, i_ui8_fileName);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    /** The firmware upgrade process */
    /**
     * 1. call the CRST Command
     * 2. Send the LBDX Command
     * 3. Open the File and Transfer the 256 Byte of data to the OCM
     * 4. Send the UPFW Command OCM
     * 5. Send the SLCT Command to the OCM
     * 6. Send the CONT Command to the OCM
     * 7. Update Product Information File
     *
     * */
    do
    {   if (l_puc_cmd != NULL)
        {   snprintf(l_ui8_file, 256, "%s%s", OCM_BINARY_FILE_PATH, i_ui8_fileName);
        }
        else
        {   l_i_retstatus = OCM_NULL_POINTER;
            break;
        }

        /** Check file exist or Not*/
        if (access((char *) l_ui8_file, F_OK) == -1)
        {   l_i_retstatus = OCM_FW_UPGRADE_FILE_NOT_FOUND;
            break;
        }

        snprintf(l_pc_ext, 8, "%s", get_filename_ext(i_ui8_fileName));

        if (strcmp((char *)l_pc_ext,(char *)"bdx") != 0)
        {   l_i_retstatus = OCM_INVALID_INPUT;
            LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Invalid OCM Firmware file is supplied]");
            break;
        }

        if (POWERMGT_DEVICE_ON != getOCMStatus())
        {   l_i_retstatus = OCM_POWERED_OFF;
            break;
        }

        /****************** Step 1: call the CRST Command ******************/
        /** Prepare the command*/
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = OcmCRST()))
        {   break;
        }

        /*****************  Step 2: call the LBDX Command *****************/
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = OcmLBDX()))
        {   break;
        }

        /*****************  Step 3: Open the File and Transfer the 256 Byte of data
         * 									 to the OCM Starts *****************/
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = (TransferFile(l_ui8_file))))
        {   break;
        }

        /*****************  Step 4: Send the UPFW Command OCM Starts *****************/

        /**  all packet transmitted then wait for the 1sec */
        sleep(1);

        /** Send the UPFW command and wait for the response within 15 sec*/
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = OcmUPFW()))
        {   break;
        }

        /*****************  Step 5: Send the SLCT Command to the OCM Starts *****************/
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = OcmSLCT()))
        {   break;
        }

        /*****************  Step 6: Send the CONT Command to the OCM Starts *****************/
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = OcmCONT()))
        {   break;
        }

        /*****************  Step 7: Update Product Information File *****************/
        if(l_i_retstatus = OcmUpdateProductInfo())
        {   LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Failed to update OCM Firmware version in Product Info File]. Returned: %d", l_i_retstatus);
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function Control ReSet – Soft reset the CTM
 *
 * @return On success, returns 0, error code otherwise.
 */
int OcmCRST()
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    uint8_t l_ui8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t timeout = 2000000;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    snprintf((char *) l_ui8_command,
             OCM_API_SHORT_RESPONSE_LENGTH, "%s%s%s%s%s",
             NL, OCM_API_CRST_COMMAND, OCM_BOOTLOAD_CRST_CRC, CR, NL);
    do
    {   /* Check if OCM interface init is performed & handler is available */
        if (!checkInit())
            return (int)OCM_COMMUNICATION_ERROR;

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Check the bootloader Error */
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = BootloaderErrorCheck((unsigned char *) l_ui8_response)))
        {   break;
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function Initiate binary data exchange
 * firmware download
 *
 * @return On success, returns 0, error code otherwise.
 */
int OcmLBDX()
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    uint8_t l_ui8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /** Prepare the command*/
    snprintf((char *) l_ui8_command,
             OCM_API_SHORT_RESPONSE_LENGTH, "%s%s%s%s%s",
             NL, OCM_API_LBDX_COMMAND, OCM_BOOTLOAD_LBDX_CRC, CR, NL);
    strlen((const char *) l_ui8_command);
    do
    {   /* Check if OCM interface init is performed & handler is available */
        if (!checkInit())
            return (int)OCM_COMMUNICATION_ERROR;

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Check the bootloader Error */
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus  = BootloaderErrorCheck((unsigned char *) l_ui8_response)))
        {   break;
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function Update firmware command
 *
 * @return On success, returns 0, error code otherwise.
 */
int OcmUPFW()
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    uint8_t l_ui8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t timeout = 30000000;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /** Prepare the command*/
    snprintf((char *) l_ui8_command,
             OCM_API_SHORT_RESPONSE_LENGTH, "%s%s%s%s%s",
             NL, OCM_API_UPFW_COMMAND, OCM_BOOTLOAD_UPFW_CRC, CR, NL);
    strlen((const char *) l_ui8_command);

    do
    {   /* Check if OCM interface init is performed & handler is available */
        if (!checkInit())
            return (int)OCM_COMMUNICATION_ERROR;

        /** Call the setget command*/
        if ((l_i_retstatus = serialSwitchReadWrite(ocmcomm_handle,
                             (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                             (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Check the bootloader Error */
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = BootloaderErrorCheck((unsigned char *) l_ui8_response)))
        {   break;
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function Select Firmware Image
 *
 * @return On success, returns 0, error code otherwise.
 */
int OcmSLCT()
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    uint8_t l_ui8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t timeout = 1200000;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /** Prepare the command*/
    snprintf((char *) l_ui8_command,
             OCM_API_SHORT_RESPONSE_LENGTH, "%s%s%s%s%s",
             NL, OCM_API_SLCT_COMMAND, OCM_BOOTLOAD_SLCT_CRC, CR, NL);

    do
    {   /* Check if OCM interface init is performed & handler is available */
        if (!checkInit())
            return (int)OCM_COMMUNICATION_ERROR;

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Check the bootloader Error */
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = BootloaderErrorCheck((unsigned char *) l_ui8_response)))
        {   break;
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function continue boot process into main
 * CTM code
 *
 * @return On success, returns 0, error code otherwise.
 */
int OcmCONT()
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    uint8_t l_ui8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t timeout = 5000000;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /** Prepare the command*/
    snprintf((char *) l_ui8_command,
             OCM_API_SHORT_RESPONSE_LENGTH, "%s%s%s%s%s",
             NL, OCM_API_CONT_COMMAND, OCM_BOOTLOAD_CONT_CRC, CR, NL);
    do
    {   /* Check if OCM interface init is performed & handler is available */
        if (!checkInit())
            return (int)OCM_COMMUNICATION_ERROR;

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Check the bootloader Error */
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = BootloaderErrorCheck((unsigned char *) l_ui8_response)))
        {   break;
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function copy the image in image position
 * <src> to the image position <dst>
 *
 * @return On success, returns 0, error code otherwise.
 */
int OcmCOPY()
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    uint8_t l_ui8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t timeout = 30500000;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /** Prepare the command*/
    snprintf((char *) l_ui8_command,
             OCM_API_SHORT_RESPONSE_LENGTH, "%s%s%s%s%s%s%s",
             NL, OCM_API_COPY_COMMAND, OCM_BOOTLOAD_COPY_SRC, OCM_BOOTLOAD_COPY_SRC,
             OCM_BOOTLOAD_COPY_CRC, CR, NL);
    do
    {   /* Check if OCM interface init is performed & handler is available */
        if (!checkInit())
            return (int)OCM_COMMUNICATION_ERROR;

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Check the bootloader Error */
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = BootloaderErrorCheck((unsigned char *) l_ui8_response)))
        {   break;
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function list command lists the valid
 * primary and secondary images in flash memory
 *
 * @return On success, returns 0, error code otherwise.
 */
int OcmLIST()
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    uint8_t l_ui8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t timeout = 1000000;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /** Prepare the command*/
    snprintf((char *) l_ui8_command,
             OCM_API_SHORT_RESPONSE_LENGTH, "%s%s%s%s%s",
             NL, OCM_API_LIST_COMMAND, OCM_BOOTLOAD_LIST_CRC, CR, NL);
    do
    {   /* Check if OCM interface init is performed & handler is available */
        if (!checkInit())
            return (int)OCM_COMMUNICATION_ERROR;

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Check the bootloader Error */
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = BootloaderErrorCheck((unsigned char *) l_ui8_response)))
        {   break;
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function initiate Motorola S-Record firmware
 * download
 *
 * @return On success, returns 0, error code otherwise.
 */
int OcmLOAD()
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    uint8_t l_ui8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t timeout = 500000;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /** Prepare the command*/
    snprintf((char *) l_ui8_command,
             OCM_API_SHORT_RESPONSE_LENGTH, "%s%s%s%s%s",
             NL, OCM_API_LOAD_COMMAND, OCM_BOOTLOAD_LOAD_CRC, CR, NL);
    do
    {   /* Check if OCM interface init is performed & handler is available */
        if (!checkInit())
            return (int)OCM_COMMUNICATION_ERROR;

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Check the bootloader Error */
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = BootloaderErrorCheck((unsigned char *) l_ui8_response)))
        {   break;
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function No Operation – echos the string
 * ‘NOOP’
 *
 * @return On success, returns 0, error code otherwise.
 */
int OcmNOOP()
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    uint8_t l_ui8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t timeout = 500000;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /** Prepare the command*/
    snprintf((char *) l_ui8_command,
             OCM_API_SHORT_RESPONSE_LENGTH, "%s%s%s%s%s",
             NL, OCM_API_NOOP_COMMAND, OCM_BOOTLOAD_NOOP_CRC, CR, NL);
    do
    {   /* Check if OCM interface init is performed & handler is available */
        if (!checkInit())
            return (int)OCM_COMMUNICATION_ERROR;

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Check the bootloader Error */
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = BootloaderErrorCheck((unsigned char *) l_ui8_response)))
        {   break;
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function unprotect the flash memory to allow
 * Bootloader image overwrite
 *
 * @return On success, returns 0, error code otherwise.
 */
int OcmUNPF()
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    uint8_t l_ui8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t timeout = 1000000;
    uint l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /** Prepare the command*/
    snprintf((char *) l_ui8_command,
             OCM_API_SHORT_RESPONSE_LENGTH, "%s%s%s%s%s",
             NL, OCM_API_UNPF_COMMAND, OCM_BOOTLOAD_UNPF_CRC, CR, NL);
    do
    {   /* Check if OCM interface init is performed & handler is available */
        if (!checkInit())
            return (int)OCM_COMMUNICATION_ERROR;

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }

        /** Check the bootloader Error */
        if (OCM_COMMAND_SUCCESS != (l_i_retstatus = BootloaderErrorCheck((unsigned char *) l_ui8_response)))
        {   break;
        }
    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

int TransferFile(uint8_t *l_ui8_file)
{   int l_i_retstatus = OCM_COMMAND_SUCCESS;
    int l_i_totalblocks = 0;
    int l_i_rembytes = 0;
    int l_i_ResponseLen = 0;
    int l_i_blocksize = 1;
    int8_t l_i8_retry = 3;
    uint32_t timeout = 500000;
    FILE *l_px_swFile = NULL;
    long l_l_filesize = 0;
    uint8_t l_puc_ResBuff[8192] = { 0 };
    uint8_t l_aui8_strmBuff[256] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    do
    {   /** Write the file transfer logic here */
        /** Divide the data in 256 Bytes */
        /** Download the Firmware*/
        if (!OcmdownloadFileStatus(l_ui8_file, &l_l_filesize, &l_i_totalblocks,
                                   &l_i_rembytes))
        {   break;
        }
        l_px_swFile = fopen((const char *) l_ui8_file, "rb");

        if (!l_px_swFile)
        {   LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Could not open the file: %s]", l_ui8_file);
            break;
        }

        while ((l_i_totalblocks--) && !feof(l_px_swFile))
        {

            if (g_ui8_debug_ocm)
            {   printf("Value of the l_i_totalblocks: <%d>\n", l_i_totalblocks);
            }
            if (fread(&l_aui8_strmBuff, 1, 256, l_px_swFile) == 256)
            {   /* Check if OCM interface init is performed & handler is available */
                if (!checkInit())
                {   fclose(l_px_swFile);
                    return (int)OCM_COMMUNICATION_ERROR;
                }

                while (l_i8_retry)
                {   /** Call the serialOcmReadWrite command*/
                    if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                            (uint8_t *) l_aui8_strmBuff, 256,
                                                            (uint8_t *) l_puc_ResBuff, &l_i_ResponseLen, timeout)) != 0)
                    {   LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Read block error..Breaking the loop]");
                        l_i8_retry--;
                    }
                    else
                    {   l_i8_retry = 3;
                        break;
                    }
                    memset(l_puc_ResBuff, 0x00, sizeof(l_puc_ResBuff));
                }

                if (!l_i8_retry)
                {   l_i_retstatus = EINVAL;
                    break;
                }

                /* Clear all the Buffer */
                memset(&l_aui8_strmBuff, 0x00, sizeof(l_aui8_strmBuff));

                l_i_blocksize++;

                l_i_retstatus = 0;
            }
            else
            {   LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Error in Read]");
                l_i_retstatus = EINVAL;
            }
        }

        if ((!feof(l_px_swFile) && (l_i_rembytes > 0)) && (0 == l_i_retstatus))
        {   memset(&l_aui8_strmBuff, 0x00, sizeof(l_aui8_strmBuff));
            if (fread(&l_aui8_strmBuff, 1, l_i_rembytes, l_px_swFile)
                    == l_i_rembytes)
            {   while (l_i8_retry)
                {   if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                            (uint8_t *) l_aui8_strmBuff, l_i_rembytes,
                                                            (uint8_t *) l_puc_ResBuff, &l_i_ResponseLen, timeout)) != 0)
                    {   LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Read block error..Breaking the loop]");
                        l_i8_retry--;
                    }
                    else
                    {   l_i8_retry = 3;
                        l_i_retstatus = 0;
                        break;
                    }
                }

                if (!l_i8_retry)
                {   l_i_retstatus = EINVAL;
                    break;
                }
            }
        }
    }
    while (0);
    if (l_px_swFile != NULL)
    {   fclose(l_px_swFile);
    }

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function checks the error related to the bootloader command
 * 		  from response buffer
 *
 * @param i_puc_respBuff	Input response buffer
 *
 * @return On success returns DEF_TRUE, On error return_code and returns DEF_FALSE
 */
int BootloaderErrorCheck(unsigned char *i_puc_respBuff)
{   int l_i_retStatus = OCM_COMMAND_SUCCESS;
    int l_i_count = 0;

    /* Error Checking functionality*/
    for (l_i_count = OCM_UNKOWN_COMMAND; l_i_count < OCM_MAX_ERROR_COUNT;
            l_i_count++)
    {   /*Check the response of the command for UC Error*/
        if (strncmp(i_puc_respBuff, ocm_bootloader_resp[l_i_count],
                    strlen(ocm_bootloader_resp[l_i_count])) == OCM_COMMAND_SUCCESS)
        {

            l_i_retStatus = OCM_UC_COMMAND_ERROR;
            break;
        }
    }

    return l_i_retStatus;
}

/**
 * @brief This function calculates the total no of 256 byte chunk
 *
 * @param file			Name fo the file
 * @param filesize		sizeof the file
 * @param totalblocks	total block number
 * @param rembytes		remaining bytes after 256 byte of chunk
 *
 * @return This function returns 0 on fail and 1 on success.
 */
int OcmdownloadFileStatus(uint8_t *file, long *filesize, int *totalblocks,
                          int *rembytes)
{   int l_i_retStatus = OCM_COMMAND_SUCCESS;
    FILE *fp;

    if (access((char *) file, F_OK) != -1)         //Check the file exit or not
    {   fp = fopen((char *) file, "r");
        if (fp != NULL)
        {   fseek(fp, 0L, SEEK_END);
            *filesize = ftell(fp);
            fseek(fp, 0, SEEK_SET);            // seek back to beginning of file
            *rembytes = (*filesize) % 256;
            *totalblocks = ((*filesize) / 256);

            fclose(fp);
        }
    }
    else
    {   return l_i_retStatus;
    }
    return 1;
}

/**************************************************************************************************
 *
 * @brief   This function POWERING TIME, status thread
 *
 **************************************************************************************************/
void *OCMPoweringOnTimer()
{   int l_i_OCMStartTime = OCM_POWERINGUP_TIME;

    while (l_i_OCMStartTime)
    {
#ifdef DEBUG
        printf("OCM Booting time [%d]s\n",l_i_OCMStartTime );
#endif
        sleep(1);
        l_i_OCMStartTime--;
    }
    g_b_OCMPoweringON = false;
    return NULL;
}

/**************************************************************************************************
 *
 * @brief   This function get OCM status
 *
 * @return  On success, returns device status[POWERMGT_DEVICE_ON, POWERMGT_DEVICE_OFF, POWERMGT_DEVICE_POWERING_UP,
            POWERMGT_DEVICE_POWERING_OFF ], error code otherwise
 *
 **************************************************************************************************/
int getOCMStatus(void)
{   int l_i_Status = POWERMGT_SUCCESS;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    int8_t l_i8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint8_t l_i8_Resp[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    uint8_t l_ui8_pcf_data[2] = {0x00}; //pcf reads and write two bytes data only

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init for power communication is performed & handler is available */
    if (!checkOCMPowerInit())
        return (int)POWERMGT_DEVICE_COMMUNICATION_ERROR;

    do
    {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "pcfread");

        /** Call the setget command*/
        l_i_Status =  (*powertransportReadWrite)(ocm_powercomm_handle,
                      (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                      (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

        if (l_i_Status == DEVICE_COMMUNICATION_ERROR)
        {   l_i_Status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
            LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Error: ocm communication error in ReadWrite: %d]", l_i_Status);
            break;
        }
        else if (l_i_Status == DEVICE_READ_TIMEOUT)
        {   l_i_Status = POWERMGT_DEVICE_READ_TIMEOUT;
            LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Error: ocm timeout in ReadWrite: %d]", l_i_Status);
            break;
        }
        else if ((l_i_Status != 0) && (l_ui_respLen <= 0))
        {   LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Error: ocm ReadWrite: %d]", l_i_Status);
            break;
        }

        LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[PCF Read Bytes: 0x%X, 0x%X]", l_i8_Resp[0], l_i8_Resp[1]);
        if((l_i8_Resp[1] & 0x40) != 0)
        {   l_i_Status = POWERMGT_DEVICE_OFF;
            break;
        }
        else if(((l_i8_Resp[1] & 0x40) == 0) && (g_b_OCMPoweringON == true))
        {   l_i_Status = POWERMGT_DEVICE_POWERING_UP;
            break;
        }
        else if((l_i8_Resp[1] & 0x40) == 0)
        {   l_i_Status = POWERMGT_DEVICE_ON;
            break;
        }
    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting from [%s]. Return status: %d", __func__, l_i_Status);

    return l_i_Status;
}

/**************************************************************************************************
 *
 * @brief   This function reset OCM
 *
 * @return  On success, returns POWERMGT_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int OCMReset(void)
{   int l_i_Status = POWERMGT_SUCCESS;
    pthread_t OCMtid = 0;
    do
    {   //Create status update timer thread
        g_b_OCMPoweringON = true;
        pthread_create(&(OCMtid), NULL, &OCMPoweringOnTimer, NULL);
        pthread_detach(OCMtid);
    }
    while(0);

    return l_i_Status;
}

/**************************************************************************************************
 *
 * @brief   This function POWER-ON/OFF CTM as per the argument
 *
 * @param   i_e_switch, enum to the power status (ON/OF)
 *
 * @return  On success, returns POWERMGT_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int OCMPower(E_POWERSWITCH i_e_switch)
{   int l_i_Status = POWERMGT_SUCCESS;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    int8_t l_i8_command[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    int8_t l_i8_Resp[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };
    uint32_t l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;

    /**Log Info*/
    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering into %s. Value: %d]", __func__, i_e_switch);

    /* caller of OCMPower() function first get the OCM Power off/on status using getSFPStatus()
     * Based on the return status, one should switch on & off the OCM.
     */

    /* Check if OCM interface init for power communication is performed & handler is available */
    if (!checkOCMPowerInit())
        return (int)POWERMGT_DEVICE_COMMUNICATION_ERROR;

    do
    {   if(i_e_switch == ON)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "ctmon");

            /** Call the setget command*/
            l_i_Status =  (*powertransportReadWrite)(ocm_powercomm_handle,
                          (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                          (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

            if (l_i_Status == DEVICE_COMMUNICATION_ERROR)
            {   l_i_Status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
                LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Error: ocm communication error in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if (l_i_Status == DEVICE_READ_TIMEOUT)
            {   l_i_Status = POWERMGT_DEVICE_READ_TIMEOUT;
                LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Error: ocm timeout in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if ((l_i_Status != 0) && (l_ui_respLen <= 0))
            {   LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Error: ocm ReadWrite: %d]", l_i_Status);
                break;
            }

            //OCM Power UP
            //Toggle the OCM reset pin
            sleep(0.5);
            l_i_Status = OCMReset();
            break;
        }
        else if(i_e_switch == OFF)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "ctmoff");

            /** Call the setget command*/
            l_i_Status =  (*powertransportReadWrite)(ocm_powercomm_handle,
                          (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                          (uint8_t *) l_i8_Resp, &l_ui_respLen, timeout);

            if (l_i_Status == DEVICE_COMMUNICATION_ERROR)
            {   l_i_Status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
                LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Error: ocm communication error in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if (l_i_Status == DEVICE_READ_TIMEOUT)
            {   l_i_Status = POWERMGT_DEVICE_READ_TIMEOUT;
                LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Error: ocm timeout in ReadWrite: %d]", l_i_Status);
                break;
            }
            else if ((l_i_Status != 0) && (l_ui_respLen <= 0))
            {   LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Error: ocm ReadWrite: %d]", l_i_Status);
                break;
            }
        }
        else
        {   l_i_Status = POWERMGT_INVALID_INPUT;
        }
    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting from [%s]. Return status: %d]", __func__, l_i_Status);

    return l_i_Status;
}


/**
 * @brief This function gets response of CDGT1
 *
 * @param integration specified the integration vaslue as type T_CDGT1.
 * @param cgtp1b return the optical spectrum as type T_CDGT1.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmGetCDGT1(T_CDGT1 *cdgt1)
{   int l_i_retstatus = 0;
    int l_i_count = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT;
    uint l_ui_respLen = OCM_API_LARGE_RESPONSE_LENGTH;
    unsigned int l_i_freq = 0;
    unsigned int l_i_power = 0;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_LARGE_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s]", __func__);

    /* Check if OCM interface init is performed & handler is available */
    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   if (cdgt1 == NULL)
        {   l_i_retstatus = EINVAL;
            break;
        }

        /** Prepare the command*/
        snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_CDGT1_COMMAND) + CRNL_LEN, "%s%s%s%s",
                 NL, OCM_API_CDGT1_COMMAND, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }
        //printf("CDGT1 Response \n");
        for (int i=0; i<l_ui_respLen; i++)
        {   //printf("%x ", l_ui8_response[i]);
        }


        l_i_freq = (unsigned int)floor(l_ui_respLen/4);
        if(l_i_freq != 785)
        {   l_i_retstatus = OCM_INVALID_FREQ_VAL;
            break;
        }
        //printf("CDGT1respLen:%d, total Freq: %d\n", l_ui_respLen, l_i_freq);
        cdgt1->m_frequency = l_i_freq;
        while(l_i_count < l_i_freq)
        {   l_i_power = l_ui8_response[(l_i_count * 4) + 3]  << 24
                        | l_ui8_response[((l_i_count * 4) + 2)] << 16
                        | l_ui8_response[((l_i_count * 4) + 1)] << 8
                        | l_ui8_response[((l_i_count * 4))];

            /**Individual Channel Power*/
            cdgt1->m_power[l_i_count] = *(float *) &l_i_power;
            //printf("Freq %d, Power: %f\n", l_i_count, cdgt1->m_power[l_i_count]);
            l_i_count+=1;
        }

    }
    while (0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function sets SBOC value.
 *
 * @param i_f_sbocVal sets the value as a float in THz.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetSSBC1(float i_f_sbocVal)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT ;
    unsigned int l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s i_f_sbocVal:%f]", __func__,i_f_sbocVal);

    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_SSBC1_COMMAND)+ 8 + CRNL_LEN, "%s%f%s%s",
                 OCM_API_SSBC1_COMMAND,i_f_sbocVal, CR, NL);


        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }
    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}

/**
 * @brief This function sets SBTC value.
 *
 * @param i_f_sbtcVal sets the value as a float in THz.
 * @return On success, returns OCM_COMMAND_SUCCESS, error code otherwise.
 */
int OcmSetSSTC1(float i_f_sbtcVal)
{   int l_i_retstatus = 0;
    uint32_t timeout = OCM_REQUEST_TIMEOUT ;
    unsigned int l_ui_respLen = OCM_API_SHORT_RESPONSE_LENGTH;
    uint8_t l_ui8_command[OCM_API_COMMAND_LENGTH] = { 0 };
    uint8_t l_ui8_response[OCM_API_SHORT_RESPONSE_LENGTH] = { 0 };

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Entering function: %s, i_f_sbtcVal:%f]", __func__,i_f_sbtcVal);

    if (!checkInit())
        return (int)OCM_COMMUNICATION_ERROR;

    do
    {   snprintf(l_ui8_command,
                 strlen((const char *) OCM_API_SSTC1_COMMAND)+ 8 + CRNL_LEN, "%s%f%s%s",
                 OCM_API_SSTC1_COMMAND,i_f_sbtcVal, CR, NL);

        /** Call the setget command*/
        if ((l_i_retstatus = serialOcmReadWrite(ocmcomm_handle,
                                                (uint8_t *) l_ui8_command, strlen((const char *) l_ui8_command),
                                                (uint8_t *) l_ui8_response, &l_ui_respLen, timeout)) != 0)
        {   break;
        }
    }
    while(0);

    LOG_INFO(&g_px_OcmlgrFileptr,NULL, 0, "OCM:[Exiting function: %s: Return Value: %d]", __func__, l_i_retstatus);

    return l_i_retstatus;
}
