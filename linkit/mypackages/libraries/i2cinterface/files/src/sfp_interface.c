/**************************************************************************************************
 *  @file 	sfp_interface.c
 *  @brief 	SFP Interface API Library
 *
 *  This file contains SFP command APIs implementations.
 *
**************************************************************************************************/

/* SFP interface header */
#include "sfp_interface.h"
#include <time.h>
#include "power_socket_client.h"
#include "sfp_fw_upgrade_internal.h"
#include "logMgr.h"
#include "xmfh_err.h"

volatile uint8_t g_b_pcf_data[2] = {0};
uint8_t hw_Event_data[2] = {0}; //This is dummpy buffer, added to support wss daemon till ComCast remove the dependancy on this buffer.
bool g_f_pcf_read = 0;
/* SFP I2C init node */
mraa_i2c_context g_p_sfp_init = NULL;
static int g_sfp_tonemode = -1;

int sfp_powercomm_handle = -1;       //UART Linux device handler
volatile bool g_b_stopSFPToneThread = true;        // This variable stops the thread

int (*sfppowertransportReadWrite)(int, uint8_t *,uint16_t, uint8_t *, uint32_t *, uint32_t);

/** For Logger */
#define SFP_LOGGER_FILE_PATH   "/var/log/SFP.log"
FILE * g_px_SfplgrFileptr = NULL;

#define SFP_REQUEST_TIMEOUT        1000000
#define SFP_API_SHORT_RESP_LENGTH  1024
#define SFP_API_SHORT_CMD_LENGTH   128
#define KL02_FW_UPGRADE_INDICATOR  "/tmp/kl02_fw_upgrade"

int getSFPStatusInternal(void);

/**************************************************************************************************
 * @brief Checks if freescale upgrade is inprogress
 *
 * @return  it returns 1 in case of freescale fw upgrade is in progress
 *          0 in if not
 *
 **************************************************************************************************/
static int SFPCheckFreescalUpgrade(void)
{   /* Check this flag for kl02 firmware upgrader */
    if (access((char *) KL02_FW_UPGRADE_INDICATOR, F_OK) == 0)
    {   return 1;
    }
    return 0;
}


/**************************************************************************************************
 * @brief Initialiize SFP logger
 *
 **************************************************************************************************/
void initSFPLogger(void)
{   /** Initialize Logger */
    setLogOutput(LOG_OUTPUT_FILE);
    if(!g_px_SfplgrFileptr)
        setFileLogInfo(&g_px_SfplgrFileptr, SFP_LOGGER_FILE_PATH, NULL);
    setLogLevel(LOG_LEVEL_INFO);
}

/**************************************************************************************************
 * @brief deinitialiize SFP logger
 *
 **************************************************************************************************/
void deInitSFPLogger(void)
{   if(g_px_SfplgrFileptr)
        logMgrClose(&g_px_SfplgrFileptr);
}

/**************************************************************************************************
 * @brief Ensures SFP is inserted and is powered ON
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int CheckSFPConnection(void)
{   int l_i_status = SFP_SUCCESS;
	bool l_b_tx_fault = 0;
	bool l_b_mode_abs = 0;
    do
    {   l_i_status = SFPReadModABS(&l_b_mode_abs);
        if (l_i_status)
        {   break;
        }

        if (l_b_mode_abs)
		{   l_i_status = SFPReadTxFault(&l_b_tx_fault);
            if (l_i_status)
            {   break;
            }

            if (l_b_tx_fault)
            {   l_i_status = SFP_NOT_INSERTED;
                break;
            }
        }
        if(POWERMGT_DEVICE_ON != getSFPStatusInternal())
        {   l_i_status = SFP_POWERED_OFF;
            break;
        }

    }
    while(0);

    return l_i_status;
}


/**************************************************************************************************
 * @brief   The thread block on read until any tone event received
 *  On tone event, thread reads the data from the PCF and write the data on PCF accroding to the tone event 
 *
 **************************************************************************************************/
void* SFPToneHandle(void* arg)
{   uint8_t o_ui8_data[2] = {0x0000};
    uint8_t i_ui8_dataLen = sizeof(o_ui8_data);
    uint8_t l_ui8_deviceAddress = 0x27;
    mraa_i2c_context l_p_sfp_init = NULL;   
    int failCount = 0;
    int l_i_status = 0;
    bool update_flag = false;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s]", __func__);

    /** i2c bus init**/
    l_p_sfp_init = mraa_i2c_init(I2C_BUS); 
    if (!l_p_sfp_init)
    {   LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Error: SFP i2c init error: %d]",SFP_INIT_ERROR);
        return NULL;
    }
    /*Run the device at 100kHz */
    if (mraa_i2c_frequency(l_p_sfp_init, MRAA_I2C_STD) != SFP_SUCCESS)
    {   LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Error: SFP i2c freequency set error: %d]",SFP_FREQUENCY_ERROR);
        return NULL;
    }
    /* 0x27 valid device address */    
    if (mraa_i2c_address(l_p_sfp_init, l_ui8_deviceAddress))
    {   LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Error: Failed to set the I2C address: %d]", SFP_SET_ADDR_ERROR);
        return NULL;          
    } 
    /** Start the thread ***/
    while(false == g_b_stopSFPToneThread)
    {   /** Check for the fail attempts */
        if(failCount > 10)
        {   LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Error: SFP i2c communication failcount : %d]", failCount);
            break;
        }
        /** Check the Global file descriptor */
        if(g_sfp_tonemode < 0 || g_sfp_tonemode == SFP_MODE || g_sfp_tonemode > SFP_INVALID_MODE)
        {   usleep(50000); // 50ms
            continue;        
        }
        /* Read the data */
        if(mraa_i2c_read(l_p_sfp_init, o_ui8_data, i_ui8_dataLen) == 0)
        {   LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Error: Read error: %d]", SFP_READ_ERROR);
            failCount++;
            continue;
        }
        switch(g_sfp_tonemode)
        {   
             case SFP_STEADY_OFF:
                if(!(o_ui8_data[0] & 0x8))
                {   o_ui8_data[0] = o_ui8_data[0] | 0x8; // set bit 3 to turn off (Tx disable) 
                    update_flag = true;
                }
                break;
            case SFP_STEADY_ON:
                if(o_ui8_data[0] & 0x8)
                {   o_ui8_data[0] = o_ui8_data[0] & 0xF7; // clear bit 3 to turn on (Tx disable)
                    update_flag = true;
                }                
                break;
            case SFP_TONE_270HZ:  
                if(o_ui8_data[0] & 0x8)
                {   o_ui8_data[0] = o_ui8_data[0] & 0xF7; // clear bit 3 (Tx disable)
                }
                else
                {   o_ui8_data[0] = o_ui8_data[0] | 0x8; // set bit 3 (Tx disable)
                } 
                update_flag = true; 
                usleep(370);                         
                break;
            default:
                g_sfp_tonemode = -1; //Invalid mode
                update_flag = false;
                break;
        }

        if(update_flag)
        {   if(mraa_i2c_write(l_p_sfp_init, o_ui8_data, i_ui8_dataLen) != 0)
            {   LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Error: Write error: %d]", SFP_WRITE_ERROR);
                failCount++;
            }
            else
            {   failCount = 0;
            }
        }        
    }
    mraa_i2c_stop(l_p_sfp_init);
    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s]", __func__);
    return NULL;
}

/**************************************************************************************************
 * @brief   This function initializes bus and context
 *
 * @param   initialize the global i2c context as a structure.
 *          If the bus cannot be initialized then initialize the context to NULL
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPInterfaceInit()
{   int l_i_status = SFP_SUCCESS;
    pthread_t readSFPThread = 0;

    do
    {   /* Initialize Logger */
        initSFPLogger();

        LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s]", __func__);

        /** SFP Init**/
        g_p_sfp_init = mraa_i2c_init(I2C_BUS);

        if (!g_p_sfp_init)
        {   l_i_status = SFP_INIT_ERROR;
            break;
        }

        /*Run the device at 100kHz */
        l_i_status = mraa_i2c_frequency(g_p_sfp_init, MRAA_I2C_STD);
        if (l_i_status != SFP_SUCCESS)
        {   l_i_status = SFP_FREQUENCY_ERROR;
            break;
        }

        if (sfp_powercomm_handle < 0)
        {   sfppowertransportReadWrite = socketPowerMgtReadWrite;
            l_i_status = powerMgtSocketInit(&sfp_powercomm_handle);
            if(l_i_status)
                l_i_status = SFP_INIT_ERROR;
            else
                l_i_status = SFP_SUCCESS;
        }

        if(l_i_status)
        {   break;
        }
        else
        { 
            g_b_stopSFPToneThread = false;
            /**
             * create thread to handle the SFP tone generation 
             ***/
            if(pthread_create(&readSFPThread,NULL,&SFPToneHandle,NULL))
            {   printf("ERROR!! FAIL to create Event Read Thread\n");
                l_i_status = SFP_TONE_INIT_ERROR;
                break;
            }
            pthread_detach(readSFPThread);
        }

    }
    while(0);

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s, status: %d]", __func__, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function deinitialize the global i2c context
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPInterfaceDeInit()
{   int l_i_status = SFP_SUCCESS;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s]", __func__);
    
    /** SFP Deinit*/
    if (g_p_sfp_init != NULL)
    {   l_i_status = mraa_i2c_stop(g_p_sfp_init);
        g_p_sfp_init = NULL;

        if (l_i_status != SFP_SUCCESS)
            l_i_status = SFP_DEINIT_ERROR;
    }

    if (sfp_powercomm_handle > 0)
    {   int l_i_ret = 0;
        l_i_ret = powerMgtsocketDeInit(sfp_powercomm_handle);
        if(l_i_ret)
        {   l_i_status = SFP_DEINIT_ERROR;
        }
        else
        {   l_i_status = SFP_SUCCESS;
            sfp_powercomm_handle = -1;
        }

    }
    /*stop sfp tone thread*/
    g_b_stopSFPToneThread = true;
    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s]", __func__);

    /* Deinitialize Logger */
    deInitSFPLogger();

    return l_i_status;
}

/**
 * @brief Checks if library is initialized for power communication or not.
 *
 * @return 1 if library is initialized or 0 if not.
 */
static int checkSFPPowerInit(void)
{   const int initialized = (sfp_powercomm_handle > 0);
    return initialized;
}

/**************************************************************************************************
 *
 * @brief   This function validate the device address
 *
 * @param   i_ui8_deviceAddress input device address
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int IsValidSFPAddr(uint8_t i_ui8_deviceAddress)
{   int l_i_status = SFP_INVALID_INPUT;

    if ((i_ui8_deviceAddress == SFP_ADDRESS_A0) || (i_ui8_deviceAddress == SFP_ADDRESS_A2)
            || (i_ui8_deviceAddress == SFP_ADDRESS_A4) )
    {   l_i_status = SFP_SUCCESS;
    }

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function read a single byte from designated register
 *
 * @param   i_ui8_deviceAddress input device address
 * @param   i_ui8_registerAddress input register address
 * @param   o_ui8_data returns a single byte
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadByte(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint8_t* o_ui8_data)
{   int l_i_status = -1;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s Device: %X Register: %X]", __func__, i_ui8_deviceAddress, i_ui8_registerAddress);

    do
    {   /* SFP init check */
        if(!g_p_sfp_init)
        {   l_i_status = SFP_CHECK_INIT_ERROR;
            break;
        }

        if (l_i_status = CheckSFPConnection())
        {   break;
        }

        /*Check if freescale upgrade is in progress */
        if(SFPCheckFreescalUpgrade())
        {   l_i_status = FREESCALE_FW_UPGRADE_INPROGRESS;
            break;
        }

        /* Device address validation */
        if(IsValidSFPAddr(i_ui8_deviceAddress))
        {   l_i_status = SFP_INVALID_DEV_ADDR;
            break;
        }

        /* Set the I2C slave address for this device */
        /*A0>>1 or A2>>1 valid device address*/
        if (mraa_i2c_address(g_p_sfp_init, (i_ui8_deviceAddress>>1)) != SFP_SUCCESS)
        {   l_i_status = SFP_SET_ADDR_ERROR;
            break;
        }

        /* Read the register */
        l_i_status = mraa_i2c_read_bytes_data(g_p_sfp_init, i_ui8_registerAddress, o_ui8_data, 1);
        if(l_i_status == -1 && errno == 6)
        {   l_i_status = SFP_NACK_ERROR;
            break;
        }
        else if(l_i_status == -1)
        {   l_i_status =  SFP_READ_ERROR;
            break;
        }
        l_i_status = SFP_SUCCESS;
    }
    while(0);

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. Return Value: %d]", __func__, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function read a single word from designated register
 *
 * @param   i_ui8_deviceAddress input device address
 * @param   i_ui8_registerAddress input register address
 * @param   o_ui16_data returns single word
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadWord(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint16_t* o_ui16_data)
{   int l_i_status = -1;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s Device: %X Register: %X]", __func__, i_ui8_deviceAddress, i_ui8_registerAddress);

    do
    {   /* SFP init check */
        if(!g_p_sfp_init)
        {   l_i_status = SFP_CHECK_INIT_ERROR;
            break;
        }

        if (l_i_status = CheckSFPConnection())
        {   break;
        }

        /*Check if freescale upgrade is in progress */
        if(SFPCheckFreescalUpgrade())
        {   l_i_status = FREESCALE_FW_UPGRADE_INPROGRESS;
            break;
        }

        /* Device address validation */
        if(IsValidSFPAddr(i_ui8_deviceAddress))
        {   l_i_status = SFP_INVALID_DEV_ADDR;
            break;
        }

        if (i_ui8_registerAddress > 254)
        {   l_i_status = SFP_INVALID_REG_ADDR;
            break;
        }

        /* Set the I2C slave address for this device */
        /*A0>>1 or A2>>1 valid device address*/
        if (mraa_i2c_address(g_p_sfp_init, (i_ui8_deviceAddress>>1)) != SFP_SUCCESS)
        {   l_i_status = SFP_SET_ADDR_ERROR;
            break;
        }

        /* Read the register */
        l_i_status = mraa_i2c_read_bytes_data(g_p_sfp_init, i_ui8_registerAddress, (uint8_t*)o_ui16_data, 2);
        if(l_i_status == -1 && errno == 6)
        {   l_i_status = SFP_NACK_ERROR;
            break;
        }
        else if(l_i_status == -1)
        {   l_i_status =  SFP_READ_ERROR;
            break;
        }
        l_i_status = SFP_SUCCESS;

    }
    while(0);

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. Return Value: %d]", __func__, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function write a single byte to designated register
 *
 * @param   i_ui8_deviceAddress input device address
 * @param   i_ui8_registerAddress input register address
 * @param   i_ui8_data input single byte
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPWriteByte(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint8_t i_ui8_data)
{   int l_i_status = SFP_SUCCESS;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s Device: %X Register: %X]", __func__, i_ui8_deviceAddress, i_ui8_registerAddress);

    do
    {   /* SFP init check */
        if(!g_p_sfp_init)
        {   l_i_status = SFP_CHECK_INIT_ERROR;
            break;
        }

        if (l_i_status = CheckSFPConnection())
        {   break;
        }

        /*Check if freescale upgrade is in progress */
        if(SFPCheckFreescalUpgrade())
        {   l_i_status = FREESCALE_FW_UPGRADE_INPROGRESS;
            break;
        }

        /* Device address validation */
        if(IsValidSFPAddr(i_ui8_deviceAddress))
        {   l_i_status = SFP_INVALID_DEV_ADDR;
            break;
        }


        /* Set the I2C slave address for this device */
        /*A0>>1 or A2>>1 valid device address*/
        l_i_status = mraa_i2c_address(g_p_sfp_init, (i_ui8_deviceAddress>>1));
        if (l_i_status != SFP_SUCCESS)
        {   l_i_status = SFP_SET_ADDR_ERROR;
            break;
        }
        uint8_t l_ui8_tx_buf[2] = {0};

        l_ui8_tx_buf[0] = i_ui8_registerAddress;
        l_ui8_tx_buf[1] = i_ui8_data;

        /* Write the register */
        l_i_status = mraa_i2c_write(g_p_sfp_init, l_ui8_tx_buf, 2);
        if(l_i_status != SFP_SUCCESS && errno == 6)
        {   l_i_status = SFP_NACK_ERROR;
        }
        else if(l_i_status != SFP_SUCCESS)
        {   l_i_status =  SFP_WRITE_ERROR;
        }
    }
    while(0);

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. Return Value: %d]", __func__, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function write a single word to designated register
 *
 * @param   i_ui8_deviceAddress input device address
 * @param   i_ui8_registerAddress input register address
 * @param   i_ui16_data input single word
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPWriteWord(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint16_t i_ui16_data)
{   int l_i_status = SFP_SUCCESS;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s Device: %X Register: %X]", __func__, i_ui8_deviceAddress, i_ui8_registerAddress);

    do
    {   /* SFP init check */
        if(!g_p_sfp_init)
        {   l_i_status = SFP_CHECK_INIT_ERROR;
            break;
        }

        if (l_i_status = CheckSFPConnection())
        {   break;
        }

        /*Check if freescale upgrade is in progress */
        if(SFPCheckFreescalUpgrade())
        {   l_i_status = FREESCALE_FW_UPGRADE_INPROGRESS;
            break;
        }
        /* Device address validation */
        if(IsValidSFPAddr(i_ui8_deviceAddress))
        {   l_i_status = SFP_INVALID_DEV_ADDR;
            break;
        }

        /* Valid register address for User writable EEPROM area and Password, Page select Byte */
        if (i_ui8_registerAddress > 254)
        {   l_i_status = SFP_INVALID_REG_ADDR;
            break;
        }

        /* Set the I2C slave address for this device */
        /*A0>>1 or A2>>1 valid device address*/
        l_i_status = mraa_i2c_address(g_p_sfp_init, (i_ui8_deviceAddress>>1));
        if (l_i_status != SFP_SUCCESS)
        {   l_i_status = SFP_SET_ADDR_ERROR;
            break;
        }

        uint8_t l_ui8_partA = (uint8_t)((i_ui16_data & 0xFF00) >> 8);
        uint8_t l_ui8_partB = (uint8_t)(i_ui16_data & 0x00FF);

        uint8_t l_ui8_tx_buf[3] = {0};
        l_ui8_tx_buf[0] = i_ui8_registerAddress;
        l_ui8_tx_buf[1] = l_ui8_partB;
        l_ui8_tx_buf[2] = l_ui8_partA;

        /* Write the register */
        l_i_status = mraa_i2c_write(g_p_sfp_init, l_ui8_tx_buf, 3);
        if(l_i_status != SFP_SUCCESS && errno == 6)
        {   l_i_status = SFP_NACK_ERROR;
        }
        else if(l_i_status != SFP_SUCCESS)
        {   l_i_status =  SFP_WRITE_ERROR;
        }
    }
    while(0);

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. Return Value: %d]", __func__, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief This function read a single page from designated register
 *
 * @param i_ui8_deviceAddress input device address
 * @param i_ui8_registerAddress input register address
 * @param i_ui16_n input length of bytes (max limit is 63)
 * @param o_ui8_data returns bytes of given length
 *
 * @return On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPPageRead(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint16_t i_ui16_n, uint8_t *o_ui8_data)
{   int l_i_status = SFP_SUCCESS;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s Device: %X Register: %X Length: %d]", __func__, i_ui8_deviceAddress, i_ui8_registerAddress, i_ui16_n);

    do
    {   /* SFP init check */
        if(!g_p_sfp_init)
        {   l_i_status = SFP_CHECK_INIT_ERROR;
            break;
        }

        if (l_i_status = CheckSFPConnection())
        {   break;
        }

        /*Check if freescale upgrade is in progress */
        if(SFPCheckFreescalUpgrade())
        {   l_i_status = FREESCALE_FW_UPGRADE_INPROGRESS;
            break;
        }

        /* Device address validation */
        if(IsValidSFPAddr(i_ui8_deviceAddress))
        {   l_i_status = SFP_INVALID_DEV_ADDR;
            break;
        }

        /* Page read limit to 63 bytes validation */
        if (i_ui16_n > 63)
        {   l_i_status = SFP_INVALID_PAGE_LENGTH;
            break;
        }
        /* Page length validation */
        if (i_ui16_n + i_ui8_registerAddress > 256)
        {   l_i_status = SFP_INVALID_PAGE_LENGTH;
            break;
        }

        /* Set the I2C slave address for this device */
        l_i_status = mraa_i2c_address(g_p_sfp_init, i_ui8_deviceAddress>>1);
        if (l_i_status != SFP_SUCCESS)
        {   l_i_status = SFP_SET_ADDR_ERROR;
            break;
        }

        l_i_status = mraa_i2c_read_bytes_data(g_p_sfp_init, i_ui8_registerAddress, o_ui8_data, i_ui16_n);
        if(l_i_status == -1 && errno == 6)
        {   l_i_status = SFP_NACK_ERROR;
            break;
        }
        else if(l_i_status == -1)
        {   l_i_status =  SFP_READ_ERROR;
            break;
        }
        l_i_status = SFP_SUCCESS;
    }
    while(0);

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. Return Value: %d]", __func__, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief This function write a single page to designated register
 *
 * @param i_ui8_deviceAddress input device address
 * @param i_ui8_registerAddress input register address
 * @param i_ui16_n input length of bytes (max limit is 8)
 * @param i_ui8_data input bytes of given length
 *
 * @return On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPPageWrite(uint8_t i_ui8_deviceAddress, uint8_t i_ui8_registerAddress, uint16_t i_ui16_n, const uint8_t* i_ui8_data)
{   int l_i_status = SFP_SUCCESS;
    uint8_t *p_i_Buffer = NULL;
    int l_i_loop;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s Device: %X Register: %X Length: %d]", __func__, i_ui8_deviceAddress, i_ui8_registerAddress, i_ui16_n);

    do
    {   /* SFP init check */
        if(!g_p_sfp_init)
        {   l_i_status = SFP_CHECK_INIT_ERROR;
            break;
        }

        if (l_i_status = CheckSFPConnection())
        {   break;
        }

        /*Check if freescale upgrade is in progress */
        if(SFPCheckFreescalUpgrade())
        {   l_i_status = FREESCALE_FW_UPGRADE_INPROGRESS;
            break;
        }

        /* Device address validation */
        if(IsValidSFPAddr(i_ui8_deviceAddress))
        {   l_i_status = SFP_INVALID_DEV_ADDR;
            break;
        }

        /* Page read limit to 8 bytes validation */
        if (i_ui16_n > 8)
        {   l_i_status = SFP_INVALID_PAGE_LENGTH;
            break;
        }
        /* Page length validation */
        if (i_ui16_n + i_ui8_registerAddress > 256)
        {   l_i_status = SFP_INVALID_PAGE_LENGTH;
            break;
        }

        /* Set the I2C slave address for this device */
        l_i_status = mraa_i2c_address(g_p_sfp_init, i_ui8_deviceAddress>>1);
        if (l_i_status != SFP_SUCCESS)
        {   l_i_status = SFP_SET_ADDR_ERROR;
            break;
        }

        /* Allocate temperory write array */
        p_i_Buffer = (uint8_t *)malloc((i_ui16_n+1) * sizeof(uint8_t));
        /* Check memory validity */
        if(p_i_Buffer == NULL)
        {   return SFP_MEMORY_ALLOCATION_FAILED;
        }

        /* Register address at first position of buffer*/
        p_i_Buffer[0] = i_ui8_registerAddress;
        /* loop to prepare buffer */
        for (l_i_loop = 1; l_i_loop <= i_ui16_n; l_i_loop++)
        {   p_i_Buffer[l_i_loop] = i_ui8_data[l_i_loop-1];
        }

        /* write to register */
        l_i_status = mraa_i2c_write(g_p_sfp_init, p_i_Buffer, i_ui16_n+1);
        if(l_i_status != SFP_SUCCESS && errno == 6)
        {   l_i_status = SFP_NACK_ERROR;
        }
        else if(l_i_status != SFP_SUCCESS)
        {   l_i_status =  SFP_WRITE_ERROR;
        }
    }
    while(0);

    /* Deallocate temporary buffer */
    if(p_i_Buffer != NULL)
    {   free(p_i_Buffer);
    }

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. Return Value: %d]", __func__, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function read a PCF8575
 *
 * @param   o_ui8_data    returns byte
 * @param   i_ui8_dataLen Read Buffer lenght
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int ReadPCF8575(uint8_t* o_ui8_data, uint8_t i_ui8_dataLen)
{   int l_i_status = -1;
    uint8_t l_ui8_deviceAddress = 0x27;
    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s]", __func__);

    if(i_ui8_dataLen > 2)
    {   return SFP_INVALID_INPUT;
    }
    if(g_f_pcf_read == 1) /**HwEvent thread is running, PCF Data is loaded  global buffer data **/
    {   memcpy(o_ui8_data, (const void *)g_b_pcf_data, i_ui8_dataLen);
        l_i_status = 0;
        LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. Return Value: %d, gdata[0]: %x, gdata[1]: %x]", __func__, l_i_status, g_b_pcf_data[0], g_b_pcf_data[1]);
        return l_i_status;
    }
    else    /**HwEvent thread is NOT initiated, read PCF Data by using sfp i2c handle **/
    {
        do
        {   /*Check if freescale upgrade is in progress */
            if(SFPCheckFreescalUpgrade())
            {   l_i_status = FREESCALE_FW_UPGRADE_INPROGRESS;
                break;
            }

            if(!g_p_sfp_init)
            {   l_i_status = SFP_CHECK_INIT_ERROR;
                break;
            }

            /* Set the I2C slave address for this device */
            /* 0x27 valid device address */
            l_i_status = mraa_i2c_address(g_p_sfp_init, (l_ui8_deviceAddress));
            if (l_i_status != SFP_SUCCESS)
            {   l_i_status = SFP_SET_ADDR_ERROR;
                break;
            }

            /* Read the data */
            l_i_status = mraa_i2c_read(g_p_sfp_init, o_ui8_data, i_ui8_dataLen);
            if(l_i_status == 0 && errno == 6)
            {   l_i_status = SFP_NACK_ERROR;
                break;
            }
            else if(l_i_status == 0)
            {   l_i_status =  SFP_READ_ERROR;
                break;
            }
            l_i_status = SFP_SUCCESS;
            LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. Return Value: %d, gdata[0]: %x, gdata[1]: %x, data:%x]", __func__, l_i_status, g_b_pcf_data[0], g_b_pcf_data[1], o_ui8_data[0]);
            
        }
        while(0);
    }
    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function read a mode ABS
 *
 * @param   o_b_mode returns mode value
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadModABS(bool *o_b_mode)
{   int l_i_status = SFP_SUCCESS;
    uint8_t	l_ui8_pcf_data = 0;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s]", __func__);
    do
    {   l_i_status = ReadPCF8575(&l_ui8_pcf_data, 1);
        if (l_i_status == 0)
			if ((l_ui8_pcf_data & 0x40) !=0 )
                *o_b_mode = 1;
            else
                *o_b_mode = 0;

    }
    while(0);
    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. Mode: %d, Return Value: %d, data:%x]", __func__, *o_b_mode, l_i_status, l_ui8_pcf_data);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function reads the Tx_Fault SFP IO in and retruns true if the SFP asserts a TX_FAULT
 *
 * @param   o_b_fault returns mode value
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadTxFault(bool *o_b_fault)
{   int l_i_status = SFP_SUCCESS;
    uint8_t	l_ui8_pcf_data = 0;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s]", __func__);
    do	// Commented below code to fix xmf-hd insertion capture and continue the process
    {   //if (l_i_status = CheckSFPConnection())
        //{   break;
        //}

        l_i_status = ReadPCF8575(&l_ui8_pcf_data, 1);
        if (l_i_status == 0)
            if ((l_ui8_pcf_data & 0x10) !=0)
                *o_b_fault = 1;
            else
                *o_b_fault = 0;
    }
    while(0);
    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. TX Fault: %d Return Value: %d],data: %x", __func__, *o_b_fault, l_i_status, l_ui8_pcf_data);
    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function reads the LOS SFP IO in and retruns true if the SFP asserts a LOS (Loss of signal)
 *
 * @param   o_b_los returns mode value
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadLOS(bool *o_b_los)
{   int l_i_status = SFP_SUCCESS;
    uint8_t	l_ui8_pcf_data = 0;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s]", __func__);
    do
    {   if (l_i_status = CheckSFPConnection())
        {   break;
        }

        l_i_status = ReadPCF8575(&l_ui8_pcf_data, 1);
        if (l_i_status == 0)
            if ((l_ui8_pcf_data & 0x80) !=0)
                *o_b_los = 1;
            else
                *o_b_los = 0;
    }
    while(0);
    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. Read Loss: %d Return Value: %d]", __func__, *o_b_los, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   reads the Tx_Diasable SFP IO in and retruns true if the SFP transmitter is disabled
 *
 * @param   o_b_tx_disable returns Tx_Disable value
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPReadTxDisable(bool *o_b_tx_disable)
{   int l_i_status = SFP_SUCCESS;
    uint8_t	l_ui8_pcf_data = 0;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s]", __func__);

    do
    {   // Commented below code to fix TX_DIS revert to the default state  // 
	//if (l_i_status = CheckSFPConnection())
        //{   break;
        //}

        l_i_status = ReadPCF8575(&l_ui8_pcf_data, 1);
        if (l_i_status == 0)
            if ((l_ui8_pcf_data & 0x08) !=0)
                *o_b_tx_disable = 1;
            else
                *o_b_tx_disable = 0;
    }
    while(0);
    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from %s. TX Disable: %d Return Value: %d]", __func__, *o_b_tx_disable, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   set or clear the SFP Tx_Disable pin thus turning the SFP Tx laser off or on
 *
 * @param   i_b_status Value: pass 1 to set TxDisable thus disable Tx laser; pass 0 to clear TxDisable thus enable Tx laser.
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPSetTxDisable(bool i_b_status)
{   int l_i_status = SFP_SUCCESS;
    uint8_t	l_ui8_pcf_data[2] = {0};

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s. Input: %d]", __func__, i_b_status);

    do
    {   // Commented below code to fix TX_DIS revert to the default state  //
	//if (l_i_status = CheckSFPConnection())
        //{   break;
        //}

        l_i_status = ReadPCF8575(l_ui8_pcf_data, 2);

        /* The | 0xF9 and  | 0x1C guaranties that inputs are not pulled low as this is non-obvious */
        l_ui8_pcf_data[0] = l_ui8_pcf_data[0] | 0xF9;
        l_ui8_pcf_data[1] = l_ui8_pcf_data[1] | 0x1C;

        if(i_b_status)
            l_ui8_pcf_data[0] = l_ui8_pcf_data[0] | 0x8; // set bit 3 (Tx disable)
        else
            l_ui8_pcf_data[0] = l_ui8_pcf_data[0] & 0xF7; // clear bit 3

        l_i_status = mraa_i2c_write(g_p_sfp_init, l_ui8_pcf_data, 2);
        if(l_i_status != SFP_SUCCESS && errno == 6)
        {   l_i_status = SFP_NACK_ERROR;
        }
        else if(l_i_status != SFP_SUCCESS)
        {   l_i_status =  SFP_WRITE_ERROR;
        }

        LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from: %s. Return Value: %d", __func__, l_i_status);
    }
    while(0);
    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This internal function get SFP power status
 *
 * @return  On success, returns device status[POWERMGT_DEVICE_ON, POWERMGT_DEVICE_OFF, POWERMGT_DEVICE_POWERING_UP,
            POWERMGT_DEVICE_POWERING_OFF ], error code otherwise
 *
 **************************************************************************************************/
int getSFPStatusInternal(void)
{   int l_i_status = POWERMGT_SUCCESS;
    uint8_t l_ui8_pcf_data[2] = {0x00}; //pcf reads and write two bytes data only
    uint32_t l_ui32_timeout = SFP_REQUEST_TIMEOUT;
    int8_t l_i8_command[SFP_API_SHORT_CMD_LENGTH] = { 0 };
    uint8_t l_i8_Resp[SFP_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = SFP_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s]", __func__);


    /*Check if freescale upgrade is in progress */
    if(SFPCheckFreescalUpgrade())
    {   return(int) FREESCALE_FW_UPGRADE_INPROGRESS;
    }

    if (!checkSFPPowerInit())
        return POWERMGT_DEVICE_COMMUNICATION_ERROR;

    snprintf((char *) l_i8_command, sizeof(l_i8_command), "sfpstatus");

    do
    {   l_i_status =  (*sfppowertransportReadWrite)(sfp_powercomm_handle,
                      (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                      (uint8_t *) l_i8_Resp, &l_ui_respLen, l_ui32_timeout);

        if (l_i_status == DEVICE_COMMUNICATION_ERROR)
        {   l_i_status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
            break;
        }
        else if (l_i_status == DEVICE_READ_TIMEOUT)
        {   l_i_status = POWERMGT_DEVICE_READ_TIMEOUT;
            break;
        }
        else if ((l_i_status != 0) && (l_ui_respLen <= 0))
        {   break;
        }

        //Return Device power status
        if(l_i8_Resp[0]  == 0x01)
        {   l_i_status = POWERMGT_DEVICE_OFF;
            break;
        }
        else if(l_i8_Resp[0] == 0x02)
        {   l_i_status = POWERMGT_DEVICE_POWERING_UP;
            break;
        }
        else if(l_i8_Resp[0]  == 0x03)
        {   l_i_status = POWERMGT_DEVICE_ON;
            break;
        }
        else if(l_i8_Resp[0]  == 0x04)
        {   l_i_status = POWERMGT_DEVICE_FAILED_TO_INITIALIZE;
            break;
        }
    }
    while(0);

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from: %s. Return Value: %d", __func__, l_i_status);

    return l_i_status;
}
/**************************************************************************************************
 *
 * @brief   This function get SFP power status
 *
 * @return  On success, returns device status[POWERMGT_DEVICE_ON, POWERMGT_DEVICE_OFF, POWERMGT_DEVICE_POWERING_UP,
            POWERMGT_DEVICE_POWERING_OFF ], error code otherwise
 *
 **************************************************************************************************/
int getSFPStatus(void)
{   int l_i_status = POWERMGT_SUCCESS;
    uint8_t l_ui8_pcf_data[2] = {0x00}; //pcf reads and write two bytes data only
    uint32_t l_ui32_timeout = SFP_REQUEST_TIMEOUT;
    int8_t l_i8_command[SFP_API_SHORT_CMD_LENGTH] = { 0 };
    uint8_t l_i8_Resp[SFP_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = SFP_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s]", __func__);


    /*Check if freescale upgrade is in progress */
    if(SFPCheckFreescalUpgrade())
    {   return(int) FREESCALE_FW_UPGRADE_INPROGRESS;
    }

    if (!checkSFPPowerInit())
        return POWERMGT_DEVICE_COMMUNICATION_ERROR;

    snprintf((char *) l_i8_command, sizeof(l_i8_command), "sfpstatus");

    do
    {   l_i_status =  (*sfppowertransportReadWrite)(sfp_powercomm_handle,
                      (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                      (uint8_t *) l_i8_Resp, &l_ui_respLen, l_ui32_timeout);

        if (l_i_status == DEVICE_COMMUNICATION_ERROR)
        {   l_i_status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
            break;
        }
        else if (l_i_status == DEVICE_READ_TIMEOUT)
        {   l_i_status = POWERMGT_DEVICE_READ_TIMEOUT;
            break;
        }
        else if ((l_i_status != 0) && (l_ui_respLen <= 0))
        {   break;
        }

        //Return Device power status
        if(l_i8_Resp[0]  == 0x01)
        {   l_i_status = POWERMGT_DEVICE_OFF;
            break;
        }
        else if(l_i8_Resp[0] == 0x02)
        {   l_i_status = POWERMGT_DEVICE_POWERING_UP;
            break;
        }
        else if(l_i8_Resp[0]  == 0x03)
        {   l_i_status = POWERMGT_DEVICE_ON;
            break;
        }
        else if(l_i8_Resp[0]  == 0x04)
        {   l_i_status = POWERMGT_DEVICE_FAILED_TO_INITIALIZE;
            break;
        }
    }
    while(0);

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from: %s. Return Value: %d", __func__, l_i_status);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function POWER-ON/OFF SFP as per the input.
 *
 * @param   i_e_switch, enum to the power status (ON/OFF)
 *
 * @return  On success, returns POWERMGT_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPPower(E_POWERSWITCH i_e_switch)
{   int l_i_status = POWERMGT_SUCCESS;
    pthread_t SFPtid = 0;
    uint32_t l_ui32_timeout = SFP_REQUEST_TIMEOUT;
    int8_t l_i8_command[SFP_API_SHORT_CMD_LENGTH] = { 0 };
    uint8_t l_i8_Resp[SFP_API_SHORT_RESP_LENGTH] = { 0 };
    uint32_t l_ui_respLen = SFP_API_SHORT_RESP_LENGTH;

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s. Input: %d]", __func__, i_e_switch);

    do
    {   /*Check if freescale upgrade is in progress */
        if(SFPCheckFreescalUpgrade())
        {   l_i_status = FREESCALE_FW_UPGRADE_INPROGRESS;
            break;
        }
        /* caller of SFPPower() function first get the SFP Power off/on status using getSFPStatus()
         * Based on the return status, one should switch on & off the SFP.
         */
        if (!checkSFPPowerInit())
        {   l_i_status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
            break;
        }


        if(i_e_switch == ON)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "sfpon");

            /** Call the setget command*/
            l_i_status =  (*sfppowertransportReadWrite)(sfp_powercomm_handle,
                          (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                          (uint8_t *) l_i8_Resp, &l_ui_respLen, l_ui32_timeout);

            if (l_i_status == DEVICE_COMMUNICATION_ERROR)
            {   l_i_status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
                break;
            }
            else if (l_i_status == DEVICE_READ_TIMEOUT)
            {   l_i_status = POWERMGT_DEVICE_READ_TIMEOUT;
                break;
            }
            else if ((l_i_status != 0) && (l_ui_respLen <= 0))
            {   break;
            }

        }
        else if(i_e_switch == OFF)
        {   snprintf((char *) l_i8_command, sizeof(l_i8_command), "sfpoff");

            /** Call the setget command*/
            l_i_status =  (*sfppowertransportReadWrite)(sfp_powercomm_handle,
                          (uint8_t *) l_i8_command, strlen((const char*) l_i8_command),
                          (uint8_t *) l_i8_Resp, &l_ui_respLen, l_ui32_timeout);

            if (l_i_status == DEVICE_COMMUNICATION_ERROR)
            {   l_i_status = POWERMGT_DEVICE_COMMUNICATION_ERROR;
                break;
            }
            else if (l_i_status == DEVICE_READ_TIMEOUT)
            {   l_i_status = POWERMGT_DEVICE_READ_TIMEOUT;
                break;
            }
            else if ((l_i_status != 0) && (l_ui_respLen <= 0))
            {   break;
            }
        }
        else
        {   l_i_status = POWERMGT_INVALID_INPUT;
            break;
        }
    }
    while(0);

    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from: %s. Return Value: %d", __func__, l_i_status);

    return l_i_status;
}


/**************************************************************************************************
 *
 * @brief   This function sets the SFP tone generator mode.
 *
 * @param   mode indicates the tone generator mode to enable. SFP_MODE returns the SFP to the default operation mode.
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPSetToneGeneratorMode(SFP_TONEMODE mode)
{
    int l_i_status = SFP_SUCCESS;
    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s. Input: %d]", __func__, mode);
    do
    {   
        if(mode >= SFP_INVALID_MODE || mode < SFP_STEADY_OFF)
        {
            l_i_status = SFP_INVALID_INPUT;
            break;
        }
        else if(mode == SFP_MODE)
        {
            g_sfp_tonemode = SFP_STEADY_OFF;
            usleep(100000);  
            g_sfp_tonemode = SFP_MODE;
            LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[TONE MODE SET to: SFP_MODE");
            l_i_status = SFP_SUCCESS;
            break;
        }
        else
        {
            g_sfp_tonemode = mode;
            LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[TONE MODE SET to: %s", mode == SFP_STEADY_OFF ? "STEADY OFF" : mode == SFP_STEADY_ON ? "STEADY ON" : mode == SFP_TONE_270HZ ? "270Hz" : "SFP");
            l_i_status = SFP_SUCCESS;
        }
    }
    while(0);
    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from: %s. Return Value: %d", __func__, l_i_status);
    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function read the current SFP tone generator mode.
 *
 * @param   mode will be updated with a enum value indicating the current tone generator mode. SFP_MODE indicates sfp default operation mode.
 *
 * @return  On success, returns SFP_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int SFPGetToneGeneratorMode(SFP_TONEMODE *mode)
{
    int l_i_status = SFP_SUCCESS;
    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Entering into: %s]", __func__);
    do
    {  
        if(g_sfp_tonemode < 0)
        {
            *mode = SFP_MODE;
            l_i_status = SFP_SUCCESS;
            break;
        }
        else
        {
            *mode = g_sfp_tonemode;
            LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[CURRENT MODE: %s", 
                    g_sfp_tonemode == SFP_STEADY_OFF ? "STEADY OFF" : 
                    g_sfp_tonemode == SFP_STEADY_ON ? "STEADY ON" : 
                    g_sfp_tonemode == SFP_TONE_270HZ ? "270Hz" : "SFP");
            l_i_status = SFP_SUCCESS;
        }
    }
    while(0);
    LOG_INFO(&g_px_SfplgrFileptr, NULL, 2, "SFP:[Exiting from: %s. Return Value: %d", __func__, l_i_status);
    return l_i_status;
}
