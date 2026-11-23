/**
 *  @file 	led_control.c
 *  @brief 	LED Control  API Library
 *
 *  	This file contains LED Control , Freescale setup
 *  	and  Freescale processor reset APIs implementations.
 *
 */

/* standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include "led_control.h"
#include "freescale_interface.h"
#include "logMgr.h"
#include "i2c_uC_comm.h"
#include "xmfh_err.h"

/** Logger File Handler */
int g_i_lgrLedMode = 2;
FILE *g_px_ledLgrFileptr = NULL;

/** Logger File Path */
#define LED_LOG_FILE_PATH   "/var/log/LED.log"


#define LED_I2C_BUS                      0
#define LED_WRITE_FRAME_LENGTH           0x09
#define LED_WRITE_BYTE_FRAME_LENGTH      0x07
#define LED_WRITE_BYTE_DEVICE_ADDR       0x77
#define LED_CARRIAGE_CHAR                0x77
#define LED_ENDOF_FRAME                  0x00
#define FSL_I2C_SLAVE_ADDRESS            0x21
#define LED_READ_RESP_LEN                3
#define LED_I2C_ADDRESS                  0x2b
#define LED_MAX_COUNT                    4

/** MRAA i2c init node */
mraa_i2c_context g_p_led_init = NULL;

int  g_i_DebugFlag = 0;
uint8_t g_ui8_sequenceID;

int LEDreadWrite(uint8_t* i_ui8_data, uint8_t* o_ui8_data, int i_i_n);
/**
 *
 * Public APIs Used by User
 */

/**
 * @brief This functions Enables the LED Controller
 * @param i_b_state LED State. Pass 1 to enable and 0 to disable.
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int LEDEnableController(bool i_b_state)
{   int l_i_returnStatus = LED_SUCCESS;
    uint8_t l_ui8_Data = 0x00;


    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[Entering into [%s]", __func__);

    do
    {   if(i_b_state == 1)
        {   /** Write 0x40 data to the register address 0x00 */

            l_ui8_Data = 0x40;
            l_i_returnStatus = device_write(g_p_led_init, LED_I2C_ADDRESS, 0x00, &l_ui8_Data, 1, &g_ui8_sequenceID);
            if(l_i_returnStatus)
            {   if(l_i_returnStatus == DEVICE_I2C_NACK_ERROR)
                {   l_i_returnStatus = LED_NACK_ERROR;
                    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device write operation failed Error: %d]", __func__, l_i_returnStatus);
                    break;
                }
                LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device write operation failed Error: %d]", __func__, l_i_returnStatus);
                l_i_returnStatus = LED_WRITE_ERROR;
                break;
            }

            /** Write 0x00 data to the register address 0x02 */
            l_ui8_Data = 0x00;
            l_i_returnStatus = device_write(g_p_led_init, LED_I2C_ADDRESS, 0x02, &l_ui8_Data, 1, &g_ui8_sequenceID);
            if(l_i_returnStatus)
            {   if(l_i_returnStatus == DEVICE_I2C_NACK_ERROR)
                {   l_i_returnStatus = LED_NACK_ERROR;
                    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device write operation failed Error: %d]", __func__, l_i_returnStatus);
                    break;
                }
                LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device write operation failed Error: %d]", __func__, l_i_returnStatus);
                l_i_returnStatus = LED_WRITE_ERROR;
                break;
            }

        }
        else
        {   /** Write 0x00 data to the register address 0x00 */
            l_ui8_Data = 0x00;
            l_i_returnStatus = device_write(g_p_led_init, LED_I2C_ADDRESS, 0x00, &l_ui8_Data, 1, &g_ui8_sequenceID);
            if(l_i_returnStatus)
            {   if(l_i_returnStatus == DEVICE_I2C_NACK_ERROR)
                {   l_i_returnStatus = LED_NACK_ERROR;
                    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device write operation failed Error: %d]", __func__, l_i_returnStatus);
                    break;
                }
                LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device write operation failed Error: %d]", __func__, l_i_returnStatus);
                l_i_returnStatus = LED_WRITE_ERROR;
                break;
            }

        }


    }
    while(0);
    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[Exiting function: %s: Return Value: %d]", __func__, l_i_returnStatus);

    return l_i_returnStatus;
}

/**
 * @brief This function sets the LED Brightness
 * @param i_e_ledId        LED ID [Valid values: as per E_LEDID enum]
 * @param i_ui8_colorRed   Read color value    [Valid range: 0-255]
 * @param i_ui8_colorGreen Green color value   [Valid range: 0-255]
 * @param i_ui8_colorBlue  Blue color value    [Valid range: 0-255]
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int LEDBrightness(E_LEDID i_e_ledId, uint8_t i_ui8_colorRed,
                  uint8_t i_ui8_colorGreen, uint8_t i_ui8_colorBlue)
{

    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[Entering into [%s] LED:%d, Red:%d, Green:%d, Blue:%d", __func__, i_e_ledId, i_ui8_colorRed, i_ui8_colorGreen, i_ui8_colorBlue);

    uint8_t l_ui8_ledOpr = 0x08;
    uint8_t l_ui8_Data[3] = {0};
    int l_i_count  = 0;
    int l_i_returnStatus = LED_SUCCESS;

    do
    {   if(!g_p_led_init)
        {   l_i_returnStatus = LED_INIT_ERROR;
            break;
        }

        /** LED ID Checking */
        if (!((i_e_ledId == LED_ID_BAT ) || (i_e_ledId == LED_ID_PWR) ||
                (i_e_ledId == LED_ID_WIFI) || (i_e_ledId == LED_ID_ALM) ||
                (i_e_ledId == LED_ID_ALL)))
        {   l_i_returnStatus = LED_INVALID_ID_ERROR;
            break;
        }

        l_ui8_Data[0] = i_ui8_colorBlue;
        l_ui8_Data[1] = i_ui8_colorGreen;
        l_ui8_Data[2] = i_ui8_colorRed;

        int l_i_ret = 0;
        /** Rotate the loop 4 times Because of the LED count*/
        for(l_i_count = 0; l_i_count < LED_MAX_COUNT; l_i_count++)
        {   if(i_e_ledId & l_ui8_ledOpr)
            {   l_i_returnStatus = device_write(g_p_led_init, LED_I2C_ADDRESS, (0x0F + 9 - (l_i_count*3)), l_ui8_Data, 3,  &g_ui8_sequenceID);
                if(l_i_returnStatus)
                {   if(l_i_returnStatus == DEVICE_I2C_NACK_ERROR)
                    {   l_i_returnStatus = LED_NACK_ERROR;
                        LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device write operation failed Error: %d]", __func__, l_i_returnStatus);
                        break;
                    }
                    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device write operation failed Error: %d]", __func__, l_i_returnStatus);
                    l_i_returnStatus = LED_WRITE_ERROR;
                    break;
                }
            }
            l_ui8_ledOpr = l_ui8_ledOpr >> 1;
        }
    }
    while(0);

    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[Exiting function: %s: Return Value: %d]", __func__, l_i_returnStatus);

    return l_i_returnStatus;
}
/**
 * @brief This function runs default led animation.
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int RunDefaultLedAnimation()
{   uint8_t l_ui8_request[4] = {0x66,0x1E,0xEE,0x01};
    uint8_t l_ui8_response[32] = {0};
    int l_i_ret = 0;
    do
    {   if(!g_p_led_init)
        {   l_i_ret = LED_INIT_ERROR;
            break;
        }

        l_i_ret = device_read(g_p_led_init, l_ui8_request, sizeof(l_ui8_request),l_ui8_response, &g_ui8_sequenceID);
        if(l_i_ret)
        {   if(l_i_ret == DEVICE_I2C_NACK_ERROR)
            {   l_i_ret = LED_NACK_ERROR;
                LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device read operation failed Error: %d]", __func__, l_i_ret);
                break;
            }
            LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device read operation failed Error: %d]", __func__, l_i_ret);
            l_i_ret = LED_READ_ERROR;
            break;
        }
    }
    while(0);

    return l_i_ret;
}
/**
 * @brief This function runs default led animation.
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int RunDefaultLedAnimation2()
{   printf("RunDefaultLedAnimation2 is run by AMRIT : Starts\n");
	uint8_t l_ui8_request[4] = {0x66,0x1E,0xEE,0x01};
    uint8_t l_ui8_response[32] = {0};
    int l_i_ret = 0;
    do
    {   if(!g_p_led_init)
        {   l_i_ret = LED_INIT_ERROR;
            break;
        }

        l_i_ret = device_read(g_p_led_init, l_ui8_request, sizeof(l_ui8_request),l_ui8_response, &g_ui8_sequenceID);
        if(l_i_ret)
        {   if(l_i_ret == DEVICE_I2C_NACK_ERROR)
            {   l_i_ret = LED_NACK_ERROR;
                LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device read operation failed Error: %d]", __func__, l_i_ret);
                break;
            }
            LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device read operation failed Error: %d]", __func__, l_i_ret);
            l_i_ret = LED_READ_ERROR;
            break;
        }
    }
    while(0);
	printf("RunDefaultLedAnimation2 is run by AMRIT : Ends\n");
    return l_i_ret;
}

/**
 * @brief This function is used for led flags control ex: stop led animation
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int SetLedControlFlag(uint8_t flag)
{   uint8_t l_ui8_request[5] = {0};
    uint8_t l_ui8_response[32] = {0};
    int l_i_ret = 0;

    l_ui8_request[0] = 0x66;
    l_ui8_request[1] = 0x1E;
    l_ui8_request[2] = 0x40;
    l_ui8_request[3] = flag;
    l_ui8_request[4] = 0x01;

    do
    {   if(!g_p_led_init)
        {   l_i_ret = LED_INIT_ERROR;
            break;
        }

        l_i_ret = device_read(g_p_led_init, l_ui8_request, sizeof(l_ui8_request),l_ui8_response, &g_ui8_sequenceID);
        if(l_i_ret)
        {   if(l_i_ret == DEVICE_I2C_NACK_ERROR)
            {   l_i_ret = LED_NACK_ERROR;
                LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device read operation failed Error: %d]", __func__, l_i_ret);
                break;
            }
            LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[<%s>: device read operation failed Error: %d]", __func__, l_i_ret);
            l_i_ret = LED_READ_ERROR;
            break;
        }
    }
    while(0);

    return l_i_ret;
}



/**
 * @brief This function Initialize  LED related global i2c context and default values
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int LEDInit()
{   int l_i_returnStatus = LED_SUCCESS;

    /* Initialize Logger */
    setLogOutput(LOG_OUTPUT_FILE);
    if (!g_px_ledLgrFileptr)
        setFileLogInfo(&g_px_ledLgrFileptr, LED_LOG_FILE_PATH, NULL);
    setLogLevel(LOG_LEVEL_INFO);
    l_i_returnStatus = LED_SUCCESS;

    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[Entering into [%s]", __func__);

    do
    {   /** LED Init*/
        g_p_led_init = mraa_i2c_init(LED_I2C_BUS);
        if (g_p_led_init == NULL)
        {   l_i_returnStatus = LED_INIT_ERROR;
            break;
        }

        /** Set the slave Address*/
        l_i_returnStatus = mraa_i2c_address(g_p_led_init, FSL_I2C_SLAVE_ADDRESS);
        if (l_i_returnStatus != LED_SUCCESS)
        {   l_i_returnStatus = LED_SET_ADDR_ERROR;
            break;
        }
    }
    while(0);

    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[Exiting function: %s: Return Value: %d]", __func__, l_i_returnStatus);

    /* Deinit Logger in case LED init fails */
    if (l_i_returnStatus && g_px_ledLgrFileptr)
        logMgrClose(&g_px_ledLgrFileptr);

    return l_i_returnStatus;
}

/**
 * @brief This function DeInitialize  LED related global i2c context and default values
 * @return no returns
 */
void LEDDeInit()
{   LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[Entering into [%s]", __func__);

    if(g_p_led_init)
    {   mraa_i2c_stop(g_p_led_init);
        g_p_led_init = NULL;
    }

    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[Exiting function: %s]", __func__);

    if (g_px_ledLgrFileptr)
        logMgrClose(&g_px_ledLgrFileptr);

    return;
}


/**
 * @brief This function make iic slave enter to the correct mode
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int FreescaleSetup()
{   uint8_t *l_pui8_transmitBuffer = NULL;
    uint8_t l_pui8_respBuffer[3] = {0};
    int l_i_returnStatus = LED_SUCCESS;

    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[Entering into [%s]", __func__);

    do
    {   if(!g_p_led_init)
        {   l_i_returnStatus = FSL_INIT_ERROR;
            break;
        }

        /** Increase the sequence ID */
        g_ui8_sequenceID = g_ui8_sequenceID + 1 ;

        l_pui8_transmitBuffer = (unsigned char*) malloc(7 * sizeof(char));

        if (l_pui8_transmitBuffer == NULL)
        {   l_i_returnStatus = LED_MALLOC_NULL_POINTER_ERROR;
            break;
        }

        l_pui8_transmitBuffer[0] = 0x06;				//length, remove length of start byte
        l_pui8_transmitBuffer[1] = g_ui8_sequenceID;    // Sequence ID
        l_pui8_transmitBuffer[2] = 0x25;				// register address
        l_pui8_transmitBuffer[3] = 0x0d;				// 	\r hex value
        l_pui8_transmitBuffer[4] = 0x0a;				//	\n hex value
        l_pui8_transmitBuffer[5] = calc_crc8_msbfirst(0, l_pui8_transmitBuffer, ((l_pui8_transmitBuffer[0]) - 1)); // END OF FRAME

        /** Debug print */
        if(g_i_DebugFlag)
        {   printf("-----------------------------------------------------------------\nFunc:[%s]\n", __func__);
            int l_i_localCount = 0;
            for(l_i_localCount = 0; l_i_localCount < 6; l_i_localCount++)
            {   printf("[Tx] :data: [%d] Hex Format:%02x \n", l_i_localCount,
                       l_pui8_transmitBuffer[l_i_localCount]);
            }
        }

        l_i_returnStatus = mraa_i2c_write(g_p_led_init, l_pui8_transmitBuffer, 0x06);
        if(l_i_returnStatus != LED_SUCCESS && errno == 6)
        {   l_i_returnStatus = LED_NACK_ERROR;
            break;
        }
        else if (l_i_returnStatus != LED_SUCCESS)
        {   l_i_returnStatus = LED_WRITE_ERROR;
            break;
        }
        usleep(500000);

        l_i_returnStatus = mraa_i2c_read(g_p_led_init, l_pui8_respBuffer, LED_READ_RESP_LEN);
        if(l_i_returnStatus == 0 && errno == 6)
        {   l_i_returnStatus = LED_NACK_ERROR;
            break;
        }
        else if(l_i_returnStatus == 0)
        {   l_i_returnStatus =  LED_READ_RESP_ERROR;
            break;
        }

        /** Debug print */
        if(g_i_DebugFlag)
        {   int l_i_localCount = 0;
            for(l_i_localCount = 0; l_i_localCount < LED_READ_RESP_LEN; l_i_localCount++)
            {   printf("[Rx]:data: [%d] Hex Format:%02x \n", l_i_localCount,
                       l_pui8_respBuffer[l_i_localCount]);
            }
            printf("-----------------------------------------------------------------\n");
        }
        l_i_returnStatus = LED_SUCCESS;
    }
    while(0);

    if(l_pui8_transmitBuffer)
    {   free(l_pui8_transmitBuffer);
        l_pui8_transmitBuffer = NULL;
    }

    LOG_INFO(&g_px_ledLgrFileptr, NULL, g_i_lgrLedMode, "LED:[Exiting function: %s: Return Value: %d]", __func__, l_i_returnStatus);

    return l_i_returnStatus;
}

/**
 * @brief   This function enable or disable automatic mirroring for wifi LEDs
 * @param   i_b_value Input TRUE or FALSE as a bool
 * @return  On success, returns SUCCESS, error code otherwise
 */
int LEDWifiMirroring(bool i_b_value)
{   int l_i_status = SUCCESS;
    uint8_t l_ui8_mirror_off[20] = {0x14, 0x0, 0x54, 0x45, 0x53, 0x54, 0x20, 0x4d, 0x49, 0x52, 0x52, 0x4f, 0x52, 0x20, 0x6f, 0x66, 0x66, 0xd, 0xa, 0x0};
    uint8_t l_ui8_mirror_on[19] = {0x13, 0x0, 0x54, 0x45, 0x53, 0x54, 0x20, 0x4d, 0x49, 0x52, 0x52, 0x4f, 0x52, 0x20, 0x6f, 0x6e, 0xd, 0xa, 0x0};
    uint8_t l_ui8_per[6] = {0x6, 0x0, 0x25, 0xd, 0xa, 0x0};
    uint8_t l_ui8_readData[100] = {0};

    do
    {   if(!g_p_led_init)
        {   l_i_status = FSL_INIT_ERROR;
            break;
        }

        l_i_status = LEDreadWrite(l_ui8_per, l_ui8_readData, 6);
        if (i_b_value)
        {   l_i_status = LEDreadWrite(l_ui8_mirror_on, l_ui8_readData, 19);
        }
        else
        {   l_i_status = LEDreadWrite(l_ui8_mirror_off, l_ui8_readData, 20);
        }
    }
    while(0);

    return l_i_status;
}

/**
 * @brief   This function read and write data to Freescale
 * @param   i_ui8_data Input array of hexadecimal data
 * @param   i_ui8_data Output array of hexadecimal data
 * @param   i_i_n Number of data to write
 * @return  On success, returns SUCCESS, error code otherwise
 */
int LEDreadWrite(uint8_t* i_ui8_data, uint8_t* o_ui8_data, int i_i_n)
{   int l_i_status = -1;
    mraa_i2c_context fscale_init = NULL;

    do
    {   /* Freescale Init*/
        fscale_init = mraa_i2c_init(LED_I2C_BUS);
        if (NULL == fscale_init)
        {   l_i_status = LED_INIT_ERROR;
            break;
        }

        /* Switch I2C address */
        if (mraa_i2c_address(fscale_init, FSL_I2C_SLAVE_ADDRESS))
        {   l_i_status = LED_SET_ADDR_ERROR;
            break;
        }

        /* Run the device at 100kHz */
        if(mraa_i2c_frequency(fscale_init, MRAA_I2C_STD))
        {   l_i_status = LED_FREQUENCY_ERROR;
            break;
        }

        i_ui8_data[i_i_n-1] = calc_crc8_msbfirst(0, i_ui8_data, i_ui8_data[0]-1); // Add CRC

#ifdef DEBUG
        printf("\nSize:%d\n", i_i_n);
        for(int i = 0; i<i_i_n; i++)
        {   printf("%x ", i_ui8_data[i]);
        }
#endif
        /* write to register */
        l_i_status = mraa_i2c_write(fscale_init, i_ui8_data, i_i_n);
        if(l_i_status != LED_SUCCESS && l_i_status == 6)
        {   l_i_status = LED_NACK_ERROR;
            break;
        }
        else if (l_i_status != LED_SUCCESS)
        {   l_i_status = LED_WRITE_ERROR;
            break;
        }

        usleep(200000);

        /* Read the register */
        l_i_status = mraa_i2c_read(fscale_init, (uint8_t*)o_ui8_data, 4);
        if(l_i_status == 0 && errno == 6)
        {   l_i_status = LED_NACK_ERROR;
            break;
        }
        else if(l_i_status == 0)
        {   l_i_status =  LED_READ_RESP_ERROR;
            break;
        }

#ifdef DEBUG
        printf("\n");
        for(int i = 0; i<4; i++)
        {   printf("%x ", o_ui8_data[i]);
        }
        int prev_len = o_ui8_data[0];
#endif
        /* Read the register */
        l_i_status = mraa_i2c_read(fscale_init, (uint8_t*)o_ui8_data, o_ui8_data[0]);
        if(l_i_status == 0 && errno == 6)
        {   l_i_status = LED_NACK_ERROR;
            break;
        }
        else if(l_i_status == 0)
        {   l_i_status =  LED_READ_RESP_ERROR;
            break;
        }

#ifdef DEBUG
        printf("\n");
        for(int i = 0; i<prev_len; i++)
        {   printf("%x ", o_ui8_data[i]);
        }
#endif
        l_i_status = LED_SUCCESS;
    }
    while(0);
    return l_i_status;
}

