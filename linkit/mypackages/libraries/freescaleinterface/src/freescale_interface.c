/**************************************************************************************************
 *  @file 	freescale_interface.c
 *
 *  @brief 	Freescale Interface source file
 *
 *  This file contains Freescale interface APIs implementation.
 *
**************************************************************************************************/
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

/* mraa header */
#include "mraa/i2c.h"
#include "i2c_uC_comm.h"
#include "freescale_interface.h"

#define I2C_BUS 0

/* Response Codes */
#define FSCALE_SUCCESS         0
#define FSCALE_FAIL            2250
#define FSCALE_INIT_ERROR      2251
#define FSCALE_SET_ADDR_ERROR  2252
#define FSCALE_FREQUENCY_ERROR 2253
#define FSCALE_WRITE_ERROR     2254
#define FSCALE_READ_ERROR      2255
#define FSCALE_DEINIT_ERROR    2256

/** GPIO Pin */
#define FREESCALE_RESET_GPIO 15

int8_t g_ui8_fsl_seq_id = 0;

/* Freescale I2C init node */
mraa_i2c_context g_p_fscale_init = NULL;

int FreescaleInit(void);
int FreescaleDeInit(void);
int FslVerCmd(char *o_c_ver);
int FslPackage(char *o_c_package);
/**************************************************************************************************
 *
 * @brief   This function initialize the global i2c context
 *
 * @return  On success, returns FSCALE_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int FreescaleInit(void)
{   int l_i_status = FSCALE_SUCCESS;

    do
    {   /** Freescale API Init**/
        g_p_fscale_init = mraa_i2c_init(I2C_BUS);
        if (NULL == g_p_fscale_init)
        {   l_i_status = FSCALE_INIT_ERROR;
            break;
        }

        /* Switch I2C address */
        if (mraa_i2c_address(g_p_fscale_init, 0x21))
        {   l_i_status = FSCALE_SET_ADDR_ERROR;
            break;
        }

        /* Run the device at 100kHz */
        if(mraa_i2c_frequency(g_p_fscale_init, MRAA_I2C_STD))
        {   l_i_status = FSCALE_FREQUENCY_ERROR;
            break;
        }
    }
    while(0);

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function deinitialize the global i2c context
 *
 * @return  On success, returns FSCALE_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int FreescaleDeInit(void)
{   int l_i_status = FSCALE_SUCCESS;

    /** Freescale API Deinit*/
    if (g_p_fscale_init != NULL)
    {   l_i_status = mraa_i2c_stop(g_p_fscale_init);

        if (l_i_status != FSCALE_SUCCESS)
            l_i_status = FSCALE_DEINIT_ERROR;
    }

    return l_i_status;
}

/**************************************************************************************************
 * @brief   This function gets version of Freescale
 *
 * @param   o_c_ver return Freescale version as a string
 *
 * @return  On success, returns FSCALE_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int FslVerCmd(char *o_c_ver)
{   int l_i_status = FSCALE_SUCCESS;
    uint8_t l_ui8_deviceAddress = 0x66;
    uint8_t l_ui8_registerAddress = 0x01;
    uint8_t l_ui8_readData[100] = {0};
    uint8_t l_ui8_requestFrame[4] = {0};
    char l_c_temp[2] = {0};

    do
    {   if (l_i_status = FreescaleInit())
        {   break;
        }
        l_ui8_requestFrame[0] = l_ui8_deviceAddress;
        l_ui8_requestFrame[1] = l_ui8_registerAddress;
        l_ui8_requestFrame[2] = 0x33;

        l_i_status = device_read(g_p_fscale_init, l_ui8_requestFrame, 3,l_ui8_readData, &g_ui8_fsl_seq_id);
        if (!l_i_status)
        {   for (int i = 34; i < (l_ui8_readData[0] ); i++)
            {   if (l_ui8_readData[i] == 0xd || l_ui8_readData[i] == 0xa)
                {   break;
                }
                sprintf(l_c_temp, "%c", l_ui8_readData[i]);
                if (isalnum(l_c_temp[0]) || l_c_temp[0] == '.')
                {   strncat(o_c_ver, l_c_temp, 1);
                }
            }
        }
    }
    while(0);

    FreescaleDeInit();

    return l_i_status;
}
/**************************************************************************************************
 * @brief   This function gets the package of Freescale used
 *
 * @param   o_c_package return Freescale version as a string
 *
 * @return  On success, returns FSCALE_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int FslPackage(char *o_c_package)
{   int l_i_status = FSCALE_SUCCESS;
    uint8_t l_ui8_deviceAddress = 0x66;
    uint8_t l_ui8_registerAddress = 0x02;
    uint8_t l_ui8_readData[100] = {0};
    uint8_t l_ui8_requestFrame[4] = {0};
    char l_c_temp[2] = {0};

    do
    {   if (l_i_status = FreescaleInit())
        {   break;
        }
        l_ui8_requestFrame[0] = l_ui8_deviceAddress;
        l_ui8_requestFrame[1] = l_ui8_registerAddress;
        l_ui8_requestFrame[2] = 0x33;

        l_i_status = device_read(g_p_fscale_init, l_ui8_requestFrame, 3,l_ui8_readData, &g_ui8_fsl_seq_id);
        if (!l_i_status)
        {   for (int i = 9; i < (l_ui8_readData[0] ); i++)
            {   if (l_ui8_readData[i] == 0xd || l_ui8_readData[i] == 0xa)
                {   break;
                }
                sprintf(l_c_temp, "%c", l_ui8_readData[i]);
                if (isalnum(l_c_temp[0]) || l_c_temp[0] == '.')
                {   strncat(o_c_package, l_c_temp, 1);
                }
            }
        }
    }
    while(0);

    FreescaleDeInit();

    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief This function checks Freescale I2C communication errors
 *
 * @param i_puc_responseBuf     input response buffer
 *
 * @return On success, returns SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int FslErrChk(unsigned char *i_puc_responseBuf)
{   int l_i_retVal = SUCCESS;

    /** I2C Bus IN_PROGRESS Error */
    if (i_puc_responseBuf[2] == 1)
    {   l_i_retVal = FSL_I2C_IN_PROGRESS_ERROR;
    }

    /** I2C Bus OK Response*/
    else if (i_puc_responseBuf[2] == 2)
    {   l_i_retVal = SUCCESS;
    }

    /** I2C Bus CRC mismatch Error */
    else if (i_puc_responseBuf[2] == 3)
    {   l_i_retVal = FSL_I2C_CRC_MISMATCH_ERROR;
    }

    /** I2C Bus nack’d Error */
    else if (i_puc_responseBuf[2] == 4)
    {   l_i_retVal = FSL_I2C_NACK_ERROR;
    }

    /** I2C Bus Error */
    else if (i_puc_responseBuf[2] == 5)
    {   l_i_retVal = FSL_I2C_BUS_ERROR;
    }

    /** I2C Bus Timeout Error */
    else if (i_puc_responseBuf[2] == 6)
    {   l_i_retVal = FSL_I2C_TIMEOUT_ERROR;
    }

    /** I2C Bus Busy Error */
    else if (i_puc_responseBuf[2] == 7)
    {   l_i_retVal = FSL_I2C_BUSY_SKIPPED_ERROR;
    }

    /** I2C Bus Undefined behaviour*/
    else
    {   l_i_retVal = FSL_I2C_UNEXPECTED_ERROR;
    }

    return l_i_retVal;
}

/**
 * @brief This function toggle reset pin of the Freescale processor
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int ResetPowerContolLogic()
{   mraa_gpio_context l_mraa_rstGpio = NULL;
    int l_i_returnStatus = FSL_RST_SUCCESS;

    do
    {   /** initialize GPIO pin */
        l_mraa_rstGpio = mraa_gpio_init(FREESCALE_RESET_GPIO);
        if (l_mraa_rstGpio == NULL)
        {   l_i_returnStatus = FSL_GPIO_INIT_ERROR;
            break;
        }

        /** set GPIO to output */
        l_i_returnStatus = mraa_gpio_dir(l_mraa_rstGpio, MRAA_GPIO_OUT);
        if (l_i_returnStatus != FSL_RST_SUCCESS)
        {   l_i_returnStatus = FSL_GPIO_SET_DIR_ERROR;
            break;
        }

        /** Set GPIO Value to the 0 */
        l_i_returnStatus = mraa_gpio_write(l_mraa_rstGpio, 0);
        if (l_i_returnStatus != FSL_RST_SUCCESS)
        {   l_i_returnStatus = FSL_GPIO_SET_VAL_ERROR;
            break;
        }

        usleep(2000);

        /** Set GPIO Value to the 1 */
        l_i_returnStatus = mraa_gpio_write(l_mraa_rstGpio, 1);
        if (l_i_returnStatus != FSL_RST_SUCCESS)
        {   l_i_returnStatus = FSL_GPIO_SET_VAL_ERROR;
            break;
        }
    }
    while(0);

    if(l_mraa_rstGpio)
    {   /** release gpio's */
        mraa_gpio_close(l_mraa_rstGpio);
        l_mraa_rstGpio = NULL;
    }

    return l_i_returnStatus;
}
/**************************************************************************************************
 * @brief   This function requests the FuelGauge Temprature(0x06)
 *          to check the far side I2C Slave device's behaviour
 *
 * @return  On success, returns FSCALE_SUCCESS, error code otherwise
 *
 **************************************************************************************************/
int FslSlaveCheckCmd()
{   uint8_t l_ui8_readData[16] = {0};
    uint8_t l_ui8_requestFrame[4] = {0};
    int l_i_status = FSCALE_SUCCESS;


    do
    {   if (l_i_status = FreescaleInit())
        {   break;
        }

        /** Getting Data from I2C */
        l_ui8_requestFrame[0] = 0x55;
        l_ui8_requestFrame[1] = 0x06;
        l_ui8_requestFrame[2] = 0x02;

        l_i_status = device_read(g_p_fscale_init, l_ui8_requestFrame, 3,l_ui8_readData, &g_ui8_fsl_seq_id);
    }
    while(0);

    FreescaleDeInit();

    return l_i_status;
}
