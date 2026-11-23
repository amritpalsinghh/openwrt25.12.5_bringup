/**************************************************************************************************
 *  @file       sfp_fw_upgrade.c
 *  @brief      SFP Firmware Upgrade API
 *
 *  The file contains SFP Firmware Upgrade API implementation.
 *
**************************************************************************************************/

/* Standard Header */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

/* Custom Header */
#include "sfp_interface.h"
#include "sfp_fw_upgrade_internal.h"
#include "sfp_fw_upgrade.h"

#define SFP_FW_LOCK_FILEPATH "/var/lock/ttyS0.LCK"
#define SERIAL_NODE "/dev/ttyS0"

#define DEVICE_BUSY_ERROR        1640
#define CREATE_LOCK_ERROR        1641
#define REMOVE_LOCK_ERROR        1642
#define SERIAL_INIT_ERROR        1643
#define SERIAL_DEINIT_ERROR      1644
#define SET_TERMINAL_PARAM_ERROR 1645
#define SFP_COMMUNICATION_ERROR  1646
#define SFP_FILE_NOT_FOUND       1649
#define GPIO_INIT_ERROR          1654
#define GPIO_DIR_ERROR           1655
#define GPIO_WRITE_ERROR         1656

int isDeviceBusy(void);
int createDeviceLockFile(void);
int RemoveDeviceLockFile(void);
int setSterminoParam(int i_serialPortFp);
int SFPserialPortInit(int *o_serialPortFp);
int SFPserialPortDeInit(int o_serialPortFp);
int SFPBootModeEnter(int o_serialPort);
int SFPFirmwareFileSend(int o_serialPort, uint8_t *i_ui8_fileName);


/**************************************************************************************************
 * @brief The function reads Lock file to analyze device state.
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int isDeviceBusy(void)
{   int l_i_status = SFP_SUCCESS;
    #if 0    
    FILE *fp = NULL;
    pid_t proc_pid = 0;

    /* Read lock file. */
    if(fp = fopen(SFP_FW_LOCK_FILEPATH,"r"))
    {   fscanf(fp, "%d", &proc_pid);
        fclose(fp);
        fp = NULL;
        if(proc_pid != getpid())
        {   l_i_status = DEVICE_BUSY_ERROR;
        }
    }
    else
    {   l_i_status = CREATE_LOCK_ERROR;
    }
    #endif

    return l_i_status;
}

/**************************************************************************************************
 * @brief The function creates Lock file for the device.
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int createDeviceLockFile(void)
{   int l_i_status = SFP_SUCCESS;
    #if 0 
    FILE *fp = NULL;
    pid_t pid = 0;

    do 	/* Check device is accessible. */
    {   if(!isDeviceBusy())
        {   l_i_status = DEVICE_BUSY_ERROR;
            break;
        }
        /* Create lock file. */
        fp = fopen(SFP_FW_LOCK_FILEPATH,"w");
        if(NULL == fp)
        {   l_i_status = CREATE_LOCK_ERROR;
            break;
        }
        pid = getpid();
        fprintf(fp,"%d",pid);
        fclose(fp);
        fp = NULL;
    }
    while(0);
    #endif
    return l_i_status;
}

/**************************************************************************************************
 * @brief The function removes Lock file which is created to lock the device.
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int RemoveDeviceLockFile(void)
{   int l_i_status = SFP_SUCCESS;
    #if 0
    int l_i_del = 0;

    /* Remove lock file. */
    l_i_del = remove(SFP_FW_LOCK_FILEPATH);
    if (l_i_del)
        l_i_status = REMOVE_LOCK_ERROR;
    #endif
    remove(SFP_FW_LOCK_FILEPATH); //remove lock file if exists
    return l_i_status;
}

/**************************************************************************************************
 * @brief The function configure terminal parameters for serial port.
 *
 * @param  i_serialPortFp serial port file pointer which is connected with SFP.
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int setSterminoParam(int i_serialPortFp)
{   int l_i_status = SFP_SUCCESS;
    struct termios stermio = {0} ;	// Structure to set serial port parameters

    do
    {   memset(&stermio, 0, (sizeof(stermio)));

        /* Get the parameters associated with the terminal */
        if (tcgetattr(i_serialPortFp, &stermio) < 0)
        {   l_i_status = SERIAL_INIT_ERROR;
            break;
        }

        /* Set terminal configuration. */
        stermio.c_cflag &= ~CRTSCTS;                                // Turn off hardware based flow control (RTS/CTS).
        /* Turn on the receiver of the serial port (CREAD), other wise reading from the serial port will not work. */
        stermio.c_cflag |= CREAD | CLOCAL;
        stermio.c_iflag &= ~(IXON | IXOFF | IXANY);                 //Turn off software based flow control (XON/XOFF).
        stermio.c_cflag &= ~(PARENB | PARODD);                      //no parity
        stermio.c_cflag &= ~CSTOPB;                                 //1 stop bits
        stermio.c_cflag = (stermio.c_cflag & ~CSIZE) | CS8;
        stermio.c_lflag = 0;
        stermio.c_iflag &= ~IGNBRK;                                 // disable break processing
        stermio.c_oflag = 0;
        /* Flag prevent the alteration of 0x0d to 0x0a. */
        stermio.c_iflag &= ~INLCR;
        stermio.c_iflag &= ~ICRNL;

        /* Set Baud Rate. */
        if (cfsetispeed(&stermio, B9600) < 0 && cfsetospeed(&stermio, B9600) < 0)
        {   l_i_status = SERIAL_INIT_ERROR;
            break;
        }

        /* Set the parameters associated with the terminal. */
        if (tcsetattr(i_serialPortFp, TCSANOW, &stermio) < 0)
        {   l_i_status = SERIAL_INIT_ERROR;
            break;
        }

        /* Flush both the input and output queues. */
        if (tcflush(i_serialPortFp, TCIOFLUSH) < 0)
        {   l_i_status = SERIAL_INIT_ERROR;
            break;
        }
    }
    while(0);

    return l_i_status;
}

/**************************************************************************************************
 * @brief The function initializes serial port.
 *
 * @param  o_serialPortFp serial port handler.
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int SFPserialPortInit(int *o_serialPortFp)
{   int l_i_status = SFP_SUCCESS;

#ifdef DEBUG
    printf("Entering into %s.\n", __func__);
#endif

    do
    {   /* Create lock file. */
        if(createDeviceLockFile())
        {   l_i_status = CREATE_LOCK_ERROR;
            break;
        }

        /* Open SFP serial node with read and write permission. */
        *o_serialPortFp = open(SERIAL_NODE, O_RDWR | O_NOCTTY | O_SYNC);
        if (*o_serialPortFp == -1)
        {   l_i_status = SERIAL_INIT_ERROR;
            /* Remove lock file*/
            l_i_status = RemoveDeviceLockFile();
            break;
        }

        /* Set terminal parameters. */
        if (setSterminoParam(*o_serialPortFp))
            l_i_status = SET_TERMINAL_PARAM_ERROR;
    }
    while(0);

#ifdef DEBUG
    printf("Exiting from %s. Returned:%d\n", __func__, l_i_status);
#endif

    return l_i_status;
}

/**************************************************************************************************
 * @brief The function deinitializes serial port.
 *
 * @param  o_serialPortFp serial port handler.
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int SFPserialPortDeInit(int o_serialPortFp)
{   int l_i_status = SFP_SUCCESS;

#ifdef DEBUG
    printf("Entering into %s.\n", __func__);
#endif

    /* Remove lock file. */
    l_i_status = RemoveDeviceLockFile();

    /* Close device node. */
    if (close(o_serialPortFp) < 0)
        l_i_status = SERIAL_DEINIT_ERROR;

#ifdef DEBUG
    printf("Exiting from %s. Returned:%d\n", __func__, l_i_status);
#endif

    return l_i_status;
}

/**************************************************************************************************
 * @brief The function enters SFP into boot mode.
 *
 * @param  o_serialPortFp serial port handler.
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int SFPBootModeEnter(int o_serialPort)
{   int l_i_status = SFP_SUCCESS;
    uint8_t l_ui8_command[4] = {0x03,0x03,0x03,0x03};
    uint8_t l_ui8_resp[1] = {0};
    uint8_t l_ui8_respLen = 1;
    int l_i_attempts = 500;
    int read_chunk = 1;
    int l_i_loop = 0;
    int l_i_enteredBootMode = 0;

    /* Flush both the input and output queues. */
    if (tcflush(o_serialPort, TCIOFLUSH) < 0)
    {   return SFP_COMMUNICATION_ERROR;
    }

    for(l_i_loop=1; l_i_loop<=l_i_attempts; l_i_loop++)
    {
#ifdef DEBUG
        printf("Entering into %s attempt:%d:", __func__, l_i_loop);
        for(int i = 0; i < sizeof(l_ui8_command)/sizeof(l_ui8_command[0]); i++)
        {   printf("%02x", l_ui8_command[i]);
        }
        printf("\n");
#endif
        /* Write boot mode command. */
        if ((write(o_serialPort, l_ui8_command, sizeof(l_ui8_command)/sizeof(l_ui8_command[0]))) < 1)
        {   l_i_status = SFP_COMMUNICATION_ERROR;
        }
        usleep(10000);
        for(;;)
        {   /* Read boot mode response. */
            if ((read(o_serialPort, l_ui8_resp, read_chunk)) < 1)
            {   l_i_status = SFP_COMMUNICATION_ERROR;
                break;
#ifdef DEBUG
                printf("Enter Boot Mode Read Fail\n");
#endif
            }
            else
            {
#ifdef DEBUG
                printf("Enter Boot Mode Response:%02x\n", l_ui8_resp[0]);
#endif
                if (l_ui8_resp[0] == 0x79)
                {   l_ui8_command[0] = 0x78;
                    /* Send "x"c to abort any entered command*/
                    if ((write(o_serialPort, l_ui8_command, 1)) < 1)
                    {   l_i_status = SFP_COMMUNICATION_ERROR;
                    }
                    else
                    {   l_i_enteredBootMode = 1;
                        break;
                    }
                }
                usleep(1000);
            }
        }
        if (l_i_enteredBootMode)
        {   break;
        }
    }

#ifdef DEBUG
    printf("Exiting from %s Returned:%d\n", __func__, l_i_status);
#endif

    return l_i_status;
}

/**************************************************************************************************
 * @brief The function sets serial port baud rate to 9600.
 *
 * @param  o_serialPortFp serial port handler.
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int SFPSetBaudRate(int o_serialPort)
{   int l_i_status = SFP_SUCCESS;
    uint8_t l_ui8_command[11] = {48, 48, 48, 48, 48, 48, 48, 98, 98, 48,48};
    struct termios stermio = {0} ;	// Structure to set serial port parameters

#ifdef DEBUG
    printf("Entering into %s\n", __func__);
#endif

    do
    {   /* Write Baud Rate Command. */
        if ((write(o_serialPort, l_ui8_command, sizeof(l_ui8_command)/sizeof(l_ui8_command[0]))) < 1)
        {   l_i_status = SFP_COMMUNICATION_ERROR;
            break;
        }
        usleep(100000);

        /* Get the parameters associated with the terminal */
        if (tcgetattr(o_serialPort, &stermio) < 0)
        {   l_i_status = SERIAL_INIT_ERROR;
            break;
        }

        /* Set Baud Rate. */
        if (cfsetispeed(&stermio, B9600) < 0 && cfsetospeed(&stermio, B9600) < 0)
        {   l_i_status = SERIAL_INIT_ERROR;
            break;
        }

        /* Set the parameters associated with the terminal. */
        if (tcsetattr(o_serialPort, TCSANOW, &stermio) < 0)
            l_i_status = SERIAL_INIT_ERROR;
    }
    while(0);

#ifdef DEBUG
    printf("Exiting from %s Returned:%d\n", __func__, l_i_status);
#endif

    return l_i_status;
}

/**************************************************************************************************
 * @brief The function sends firmware file to serial port.
 *
 * @param  o_serialPort serial port on which file needs to be sent.
 *
 * @param  i_ui8_fileName file name needs to be sent on serial port
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int SFPFirmwareFileSend(int o_serialPort, uint8_t *i_ui8_fileName)
{   int l_i_status = SFP_SUCCESS;
    uint8_t l_i8_command[512] = { 0 };
    uint8_t l_ui8_resp[512] = { 0 };
    FILE *l_p_file = NULL;
    int l_i_lineNum = 0;
    int l_i_counter = 0;
    int l_i_stop = 0;
    int l_i_command_length = 0;

    do
    {
#ifdef DEBUG
        printf("Entering into %s.FW File to be opened:%s\n", __func__, i_ui8_fileName);
#endif

        /* Check the file exist or not */
        if (access((char *) i_ui8_fileName, F_OK) == -1)
        {   l_i_status = SFP_FILE_NOT_FOUND;
            break;
        }
        if(l_p_file = fopen ( i_ui8_fileName, "r" ))
        {   tcflush(o_serialPort, TCIOFLUSH);
            /* Read Firmware file line by line */
            while ( fgets ( l_i8_command, sizeof(l_i8_command) - 1, l_p_file ) != NULL ) /* read a line */
            {
#ifdef DEBUG
                l_i_lineNum++;
                printf("\nLine No:%d. Function Name:%s. Line(%d)\n",l_i_lineNum, __func__, __LINE__);
#endif

                if (strlen(l_i8_command) > sizeof(l_i8_command) - 2)
                {   l_i_stop = 1;
                    break;
                }

                l_i8_command[strlen(l_i8_command)] = '\0';

                /* Remove newline (\n) character */
                l_i_command_length = strlen(l_i8_command);

                /* Write Line command. */
                if ((write(o_serialPort, l_i8_command, l_i_command_length - 1)) < 0)
                {
#ifdef DEBUG
                    printf("Fw line Write Error\n");
#endif
                    l_i_status = SFP_COMMUNICATION_ERROR;
                    break;
                }

                //usleep(20000);

                fd_set l_x_rfds;
                struct timeval l_x_time = {0};
                l_i_counter = 0;
                memset(l_ui8_resp, 0x00, sizeof(l_ui8_resp));
                for(;;)
                {   /* Watch stdin (filehandler 0) to see when it has input. */
                    FD_ZERO(&l_x_rfds);
                    FD_SET(o_serialPort, &l_x_rfds);
                    /* Wait up to 5 s. */
                    l_x_time.tv_sec = 5;
                    l_x_time.tv_usec = 0;

                    select(o_serialPort + 1, &l_x_rfds, NULL, NULL, &l_x_time);

                    /* Read line response. */
                    if ((read(o_serialPort, &l_ui8_resp[l_i_counter], 1)) < 0)
                    {
#ifdef DEBUG
                        printf("Read Fail\n");
#endif
                        l_i_status = SFP_COMMUNICATION_ERROR;
                        l_i_stop = 1;
                        break;
                    }
                    else
                    {
#ifdef DEBUG
                        printf("Line Response:%d. Function Name:%s. Line(%d)\n", l_ui8_resp[l_i_counter],  __func__, __LINE__);
#endif
                        if ((l_ui8_resp[l_i_counter] == 0x03) || (l_ui8_resp[l_i_counter] == 0x00))
                            break;
                    }
                    l_i_counter++;
                }
                if (l_i_stop)
                {   break;
                }
                memset(l_i8_command, 0x00, sizeof(l_i8_command));
                if (fgets ( l_i8_command, sizeof l_i8_command, l_p_file ) != NULL )
                {   for(int i = 0; i< strlen(l_i8_command) - 1; i++)
                    {   if (l_i8_command[i] != l_ui8_resp[i])
                        {
#ifdef DEBUG
                            printf("INVALID RESPONSE RECEIVED. Length of l_ui8_resp:%d. Command:%d.. Command:%d. Resp:%d.\n", strlen(l_ui8_resp), strlen(l_i8_command), l_i8_command[i], l_ui8_resp[i]);
#endif
                            l_i_status = SFP_COMMUNICATION_ERROR;
                            break;
                        }
                    }
                }
                memset(l_i8_command, 0x00, sizeof(l_i8_command));
            }
            fclose (l_p_file);
        }
        else
        {
#ifdef DEBUG
            printf("FW File is not opened.\n");
#endif
        }
    }
    while(0);

#ifdef DEBUG
    printf("Exiting from %s. Returned:%d\n", __func__, l_i_status);
#endif

    return l_i_status;
}

/**************************************************************************************************
 * @brief The function changes the value of GPIO 17 to input value.
 *
 * @param  i_i_value GPIO value to set.
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int SFPGPIOWrite(int i_i_value)

{   int l_i_status = SFP_SUCCESS;
    mraa_gpio_context l_gpio_17 = NULL;

#ifdef DEBUG
    printf("Entering into %s GPIO value:%d\n", __func__, i_i_value);
#endif

    do
    {   /* initialize mraa for the platform (not needed most of the times) */
        mraa_init();

        /* initialize GPIO pin */
        l_gpio_17 = mraa_gpio_init(17);
        if (l_gpio_17 == NULL)

        {
#ifdef DEBUG
            printf("Failed to initialize GPIO 17\n");
#endif
            mraa_deinit();
            l_i_status = GPIO_INIT_ERROR;
            break;
        }

        /* set GPIO to output */
        l_i_status = mraa_gpio_dir(l_gpio_17, MRAA_GPIO_OUT);
        if (l_i_status != SFP_SUCCESS)
        {
#ifdef DEBUG
            printf("Failed to set GPIO direction\n");
#endif
            l_i_status = GPIO_DIR_ERROR;
            break;
        }

        l_i_status = mraa_gpio_write(l_gpio_17, i_i_value);
        if (l_i_status != SFP_SUCCESS)
        {
#ifdef DEBUG
            printf("Failed to set GPIO value\n");
#endif
            l_i_status = GPIO_WRITE_ERROR;
            break;
        }
        usleep(250000);
    }
    while(0);
#if 0
    if (l_gpio_17 != NULL)
    {   /* close GPIO */
        l_i_status = mraa_gpio_close(l_gpio_17);
        if (l_i_status != SFP_SUCCESS)
        {
#ifdef DEBUG
            printf(" GPIO close Returned:%d\n", l_i_status);
#endif
        }
        /* deinitialize mraa for the platform (not needed most of the times) */
        mraa_deinit();
    }
#endif

#ifdef DEBUG
    printf("Exiting from %s GPIO Returned:%d\n", __func__, l_i_status);
#endif

    return l_i_status;
}

/**************************************************************************************************
 * @brief The function writes 01 to enable communication to SFP.
 *
 * @return  On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int SFPPreFWUpgrade()
{   int l_i_status = SFP_SUCCESS;

#ifdef DEBUG
    printf("Entering into %s\n", __func__);
#endif
    do
    {   if (l_i_status = SFPGPIOWrite(0))
        {
#ifdef DEBUG
            printf("SFPGPIOWrite(1) failed. Error:%d\n", l_i_status);
#endif
            break;
        }
        usleep(250000);

        if (l_i_status = SFPPower(OFF))
        {
#ifdef DEBUG
            printf("SFPPower(ON) failed. Error:%d\n", l_i_status);
#endif
            break;
        }
        if (l_i_status = SFPPower(ON))
        {
#ifdef DEBUG
            printf("SFPPower(ON) failed. Error:%d\n", l_i_status);
#endif
            break;
        }
        /* Check device status is on */
        for(;;)
        {   l_i_status = getSFPStatus();
            if(POWERMGT_DEVICE_ON == l_i_status)
            {
#ifdef DEBUG
                printf("Power ON\n");
#endif
                break;
            }
            if(POWERMGT_DEVICE_OFF == l_i_status)
            {
#ifdef DEBUG
                printf("Power OFF\n");
#endif
                break;
            }
            usleep(100000);
        }

        sleep(1);
        l_i_status = SFPWriteByte(0xA2, 0x7F, 0x00);
        if (l_i_status)
        {
#ifdef DEBUG
            printf("ERROR: SFPWriteByte Error no: %d\n", l_i_status);
#endif
        }
        else
        {
#ifdef DEBUG
            printf("SFP. API:SFPWriteByte Success \n");
#endif
        }
        usleep(200000);
        l_i_status = SFPWriteByte(0xA2, 0x77, 0x01);
        if (l_i_status)
        {
#ifdef DEBUG
            printf("ERROR: SFPWriteByte Error no: %d\n", l_i_status);
#endif
        }
        else
        {
#ifdef DEBUG
            printf("SFP. API:SFPWriteByte Success \n");
#endif
        }
        usleep(200000);
    }
    while(0);

#ifdef DEBUG
    printf("Exiting from %s Returned:%d\n", __func__, l_i_status);
#endif
    return l_i_status;
}

/**************************************************************************************************
 * @brief The function Switch UART to OTDR and power ON the SFP.
 *
 * @return  On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int SFPPostFWUpgrade()
{   int l_i_status = SFP_SUCCESS;

#ifdef DEBUG
    printf("Entering into %s\n", __func__);
#endif

    do
    {   if (l_i_status = SFPPower(OFF))
        {
#ifdef DEBUG
            printf("SFPPower(OFF) failed. Error:%d\n", l_i_status);
#endif
            break;
        }
        usleep(250000);
        if (l_i_status = SFPGPIOWrite(1))
        {
#ifdef DEBUG
            printf("SFPGPIOWrite(1) failed. Error:%d\n", l_i_status);
#endif
            break;
        }
        usleep(250000);

        if (l_i_status = SFPPower(ON))
        {
#ifdef DEBUG
            printf("SFPPower(ON) failed. Error:%d\n", l_i_status);
#endif
            break;
        }
        sleep(1);
    }
    while(0);

#ifdef DEBUG
    printf("Exiting from %s Returned:%d\n", __func__, l_i_status);
#endif

    return l_i_status;
}

/**************************************************************************************************
 * @brief The function Upgrades the SFP Firmware.
 *
 * @param  i_ui8_fileName SFP firmware file name, which is expected at /tmp/ location.
 *         (Note: Remote firmware upgrade procedure places files in /tmp/ directory of XMF-H unit,
 *                which can be utilized here to initiate further procedure using this API.)
 *
 * @return On success returns SFP_SUCCESS and error code otherwise.
 **************************************************************************************************/
int SFPFWUpgrade(uint8_t *i_ui8_fileName)
{   int l_i_status = SFP_SUCCESS;
    int o_serialPortFp =  -1;       //UART Linux device handler

#ifdef DEBUG
    printf("Entering into %s\n", __func__);
#endif

    do
    {   /* Switch UART to SFP */
        if (SFPPreFWUpgrade())
        {   l_i_status = SFP_FW_PRE_SETTING_ERROR;
            break;
        }

        /* Initialize serial communication */
        if (l_i_status = SFPserialPortInit(&o_serialPortFp))
        {   l_i_status = SFP_FW_INIT_ERROR;
            break;
        }

        sleep(1); // Sleep required before entering into boot mode

        /* Enter SFP into boot mode */
        if (SFPBootModeEnter(o_serialPortFp))
        {   l_i_status = SFP_FW_BOOT_MODE_ERROR;
            break;
        }

        /* Set serial baud rate */
        if (SFPSetBaudRate(o_serialPortFp))
        {   l_i_status = SFP_FW_BAUD_RATE_ERROR;
            break;
        }

        /* Sent SFP FW file to SFP */
        if (SFPFirmwareFileSend(o_serialPortFp, i_ui8_fileName))
        {   l_i_status = SFP_FW_SEND_FILE_ERROR;
            break;
        }

        /* Switch back UART to OTDR */
        if (SFPPostFWUpgrade())
        {   l_i_status = SFP_FW_POST_SETTING_ERROR;
            break;
        }
    }
    while(0);

    /* Deinitialize Serial port */
    if (SFPserialPortDeInit(o_serialPortFp))
    {   l_i_status = SFP_FW_DEINIT_ERROR;
    }

#ifdef DEBUG
    printf("Exiting from %s Returned:%d\n", __func__, l_i_status);
#endif

    return l_i_status;
}
