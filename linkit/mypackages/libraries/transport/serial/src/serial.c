/**
 *  @file 	serial.c
 *  @brief 	Serial (UART) communication Source file
 *
 *  	This file contains Init, DeInit and read write functions
 *  	for communication through UART
 *
 */

#include <stdio.h>
#include "serial.h"



//global variable
uint8_t g_ui8_debug_serial        =  0;              //debug flag

/* Function for safe read operation */
ssize_t safe_read(int fd, void *buf, size_t count)
{   ssize_t n;

    do
    {   n = read(fd, buf, count);
    }
    while (n < 0 && errno == EINTR);

    return n;
}

/* Function for safe write operation */
ssize_t safe_write(int fd, const void *buf, size_t count)
{   ssize_t n;

    do
    {   n = write(fd, buf, count);
    }
    while (n < 0 && errno == EINTR);

    return n;
}

/**
 * @brief  Initialize Serial Interface
 *		   This function open serial port and initialize parameters(BaudRate,parity,stop-bit)
 *
 *@param   i_pc_SerialNode  serial node
 *@param   o_ui16_filehandler file handler for init operation
 *
 *@return  On success returns 0 and error code otherwise.
 *
 */
int serialPortInit(char *i_pc_SerialNode, int * o_filehandler)
{   char filepath[100]= {0};
    int dev_handle = 0;
    int l_i_retStatus = 0;
    pid_t proc_pid = 0;
    FILE *fp = NULL;
    struct termios stermio = {0} ; 	     	 //termios structure variable to set serial port paramters


    if ((i_pc_SerialNode == NULL) || (o_filehandler == NULL))
    {   return SERIAL_INIT_FAILED;
    }
    do
    {   sprintf(filepath,"/var/lock/%s.LCK",i_pc_SerialNode);

        memset(&stermio, 0, (sizeof(stermio)));

        fp = fopen(filepath,"r");

        if(fp)
        {   fscanf(fp, "%d", &proc_pid);
            fclose(fp);
            fp = NULL;
            if(proc_pid != getpid())
            {   l_i_retStatus = BUSY_ERROR;
                break;
            }
        }
        else
        {   fp = fopen(filepath,"w");
            if(fp == NULL)
            {   l_i_retStatus = SERIAL_INIT_FAILED;
                break;
            }
            proc_pid = getpid();
            fprintf(fp,"%d",proc_pid);
            fclose(fp);
            fp = NULL;
        }
        sprintf(filepath,"/dev/%s",i_pc_SerialNode);
        /* open serial node with read & write permission */
        dev_handle = open(filepath, O_RDWR | O_NOCTTY | O_SYNC);
        if (dev_handle == -1)
        {   l_i_retStatus = SERIAL_INIT_FAILED;
            break;
        }
        else
        {   /* Provide handler value for library usage */
            *o_filehandler = dev_handle;

        }

        if (g_ui8_debug_serial)
        {   printf("Serial:serial port Opened Successfully\n");
        }

        if (tcgetattr(dev_handle, &stermio) < 0) //get the parameters associated with the terminal
        {   l_i_retStatus = SERIAL_INIT_FAILED;
            break;
        }

        if (g_ui8_debug_serial)
        {   printf("Serial:get attribute successfully\n");
        }

        stermio.c_cflag &= ~CRTSCTS; //Turn off hardware based flow control (RTS/CTS).
        /* Turn on the receiver of the serial port (CREAD), other wise reading from the serial port will not work. */
        stermio.c_cflag |= CREAD | CLOCAL;
        stermio.c_iflag &= ~(IXON | IXOFF | IXANY); //Turn off software based flow control (XON/XOFF).
        stermio.c_cflag &= ~(PARENB | PARODD);		//no parity
        stermio.c_cflag &= ~CSTOPB;				    //1 stop bits
        stermio.c_cflag = (stermio.c_cflag & ~CSIZE) | CS8;
        stermio.c_lflag = 0;
        stermio.c_iflag &= ~IGNBRK;					// disable break processing
        stermio.c_oflag = 0;

        /* this flag prevent the alteration of 0x0d to 0x0a*/
        stermio.c_iflag &= ~INLCR;
        stermio.c_iflag &= ~ICRNL;

        if (cfsetispeed(&stermio, B115200) < 0
                && cfsetospeed(&stermio, B115200) < 0)
        {   l_i_retStatus = SERIAL_INIT_FAILED;
            break;
        }
        if (g_ui8_debug_serial)
        {   printf("Serial:uart BaudRate set as 115200\n");
        }

        if (tcsetattr(dev_handle, TCSANOW, &stermio) < 0) //all terminal parameters reflect to termios structure
        {   l_i_retStatus = SERIAL_INIT_FAILED;
            break;
        }

        if (g_ui8_debug_serial)
        {   printf("Serial:set attribute successfully\n");
        }

        if (tcflush(dev_handle, TCIOFLUSH) < 0)//Flush Terminal Input and Output both
        {   l_i_retStatus = SERIAL_INIT_FAILED;
            break;
        }

        if (g_ui8_debug_serial)
        {   printf("Serial:Serial Initialization Done \n");
        }
    }
    while(0);
    if (l_i_retStatus == SERIAL_INIT_FAILED)
    {   remove(filepath);
    }
    return l_i_retStatus;
}

/**
 *
 *  @brief   Close serial node
 *  		 This function closes the serial node
 *
 *
 *  @return  On success, returns 0, error code otherwise.
 *
 */

int serialPortDeInit(char *i_pc_SerialNode, int filehandler)
{   char filepath[100]= {0};
    if (i_pc_SerialNode == NULL)
    {   return SERIAL_DEINIT_FAILED;
    }
    sprintf(filepath,"/var/lock/%s.LCK",i_pc_SerialNode);
    remove(filepath);

    if (close(filehandler) < 0)						//close node
        return SERIAL_DEINIT_FAILED;

    if (g_ui8_debug_serial)
    {   printf("Serial:Serial DeInitialization Done \n");
    }
    return 0;
}

/**
 *
 * @brief	Read and write command through serial for OTDR.
 *			Write command on serial and get relevant response from the Board.
 *
 * @param	filehandler				filehandler for serial node
 * @param	i_puc_cmd				input command
 * @param	i_ui16_CmdLen   		input command length
 * @param	o_puc_response  		response
 * @param	o_pui16_ResponseLen 	response length
 * @param	timeout				 	timeout specific to command
 *
 * @return  On success returns 0, error code otherwise.
 *
 */
int serialOtdrReadWrite(int filehandler, uint8_t *i_puc_cmd,
                        uint16_t i_ui16_CmdLen, uint8_t *o_puc_response,
                        uint32_t *o_pui16_ResponseLen, uint32_t timeout)
{   uint16_t l_ui16_count = 0;
    int16_t l_i16_retval = 0;
    uint32_t read_bytes = 0;
    uint32_t read_chunk = 0;
    struct timeval l_x_time =
    {   0
    };
    fd_set l_x_rfds;

    if (i_puc_cmd == NULL || o_puc_response == NULL
            || o_pui16_ResponseLen == NULL)
    {   return OTDR_NULL_POINTER;
    }

    if (*o_pui16_ResponseLen == 0)
    {   read_chunk = DEFAULT_SERIAL_READ_SIZE;
    }
    else
    {   read_chunk = *o_pui16_ResponseLen;
        *o_pui16_ResponseLen = 0;
    }

    if (tcflush(filehandler, TCIOFLUSH) < 0)//Flush Terminal Input and Output both
    {   return OTDR_COMMUNICATION_ERROR;
    }

    while (l_ui16_count < i_ui16_CmdLen)
    {   if ((l_i16_retval = write(filehandler, &i_puc_cmd[l_ui16_count], 1))
                < 1)
        {   return OTDR_COMMUNICATION_ERROR;
        }
        l_ui16_count++;
    }

    if (g_ui8_debug_serial)
    {   printf("Serial:command write on uart successfully\n");
    }

    l_i16_retval = -1;
    while (1)
    {   /* Watch stdin (filehandler 0) to see when it has input. */
        FD_ZERO(&l_x_rfds);
        FD_SET(filehandler, &l_x_rfds);

        /* Wait up to 100 ms. */
        l_x_time.tv_sec = 0;
        l_x_time.tv_usec = timeout;

        /* select observe the filehandler get any data or not */
        do
            l_i16_retval = select(filehandler + 1, &l_x_rfds, NULL, NULL,
                                  &l_x_time);
        while ((l_i16_retval == -1) && (errno == EINTR));

        /* system error if select fails return -1 */
        if (l_i16_retval == -1)
        {   return OTDR_COMMUNICATION_ERROR;
        }
        /* l_i16_retval is positive, data is available for read */
        else if (l_i16_retval > 0)
        {   // Data is available now
            if ((read_bytes = read(filehandler,
                                   &o_puc_response[*o_pui16_ResponseLen], read_chunk)) < 1)
            {   //if read fail
                if (g_ui8_debug_serial)
                {   printf("Serial:Read fails\n");
                }
                return OTDR_COMMUNICATION_ERROR;
            }
            else
            {   /* Read operation successful */
                *o_pui16_ResponseLen += read_bytes;

                if (g_ui8_debug_serial)
                {   printf(
                        "\n[SERIAL]In READ loop: read byte: %d, total byte: %d \n",
                        read_bytes, *o_pui16_ResponseLen);
                    printf("Serial:Received bytes: %s\n", o_puc_response);
                }

                /* Check for end of response from OTDR */
                if (o_puc_response[*o_pui16_ResponseLen - 1] == '>') //here end of the response is '>'
                {   o_puc_response[*o_pui16_ResponseLen - 1] = '\0';
                    if (g_ui8_debug_serial)
                    {   printf("Serial:\nFound > as end of OTDR Response\n");
                    }
                    // Delay of 10ms in order to allow UART functionality smoothly
                    usleep(10000);
                    break;
                }
            }
        }
        else
        {   /* within timeout, there is no data is available on UART */
            if (g_ui8_debug_serial)
            {   printf("Serial:No data received till timeout\n");
            }
            return OTDR_SERIAL_READ_TIMEOUT;
        }
    }
    return 0;
}
/**
 *
 * @brief	Read and write command through serial for OTDR.
 *			Write command on serial and get relevant response from the Board.
 *
 * @param	filehandler				filehandler for serial node
 * @param	i_puc_cmd				input command
 * @param	i_ui16_CmdLen   		input command length
 * @param	o_puc_response  		response
 * @param	o_pui16_ResponseLen 	response length
 * @param	timeout				 	timeout specific to command
 *
 * @return  On success returns 0, error code otherwise.
 *
 */
int uartOtdrReadWrite(int filehandler, uint8_t *i_puc_cmd,
                      uint16_t i_ui16_CmdLen, uint8_t *o_puc_response,
                      uint32_t *o_pui16_ResponseLen, uint32_t timeout)
{   uint16_t l_ui16_count = 0;
    int16_t l_i16_retval = 0;
    uint32_t read_bytes = 0;
    uint32_t read_chunk = 0;
    struct timeval l_x_time =
    {   0
    };
    fd_set l_x_rfds;

    if (i_puc_cmd == NULL || o_puc_response == NULL
            || o_pui16_ResponseLen == NULL)
    {   return OTDR_NULL_POINTER;
    }

    if (*o_pui16_ResponseLen == 0)
    {   read_chunk = DEFAULT_SERIAL_READ_SIZE;
    }
    else
    {   read_chunk = *o_pui16_ResponseLen;
        *o_pui16_ResponseLen = 0;
    }

    if (tcflush(filehandler, TCIOFLUSH) < 0)//Flush Terminal Input and Output both
    {   return OTDR_COMMUNICATION_ERROR;
    }

    while (l_ui16_count < i_ui16_CmdLen)
    {   if ((l_i16_retval = write(filehandler, &i_puc_cmd[l_ui16_count], 1))
                < 1)
        {   return OTDR_COMMUNICATION_ERROR;
        }
        l_ui16_count++;
    }

    if (g_ui8_debug_serial)
    {   printf("Serial:command write on uart successfully\n");
    }

    l_i16_retval = -1;
    /* Watch stdin (filehandler 0) to see when it has input. */
    FD_ZERO(&l_x_rfds);
    FD_SET(filehandler, &l_x_rfds);

    /* Wait up to 100 ms. */
    l_x_time.tv_sec = 0;
    l_x_time.tv_usec = 3000000;

    /* select observe the filehandler get any data or not */
    do
    {   l_i16_retval = select(filehandler + 1, &l_x_rfds, NULL, NULL,
                              &l_x_time);
    }
    while ((l_i16_retval == -1) && (errno == EINTR));

    /* system error if select fails return -1 */
    if (l_i16_retval == -1)
    {   return OTDR_COMMUNICATION_ERROR;
    }
    /* l_i16_retval is positive, data is available for read */
    else if (l_i16_retval > 0)
    {   // Data is available now
        if ((read_bytes = read(filehandler,
                               &o_puc_response[*o_pui16_ResponseLen], read_chunk)) < 1)
        {   //if read fail
            if (g_ui8_debug_serial)
            {   printf("Serial:Read fails\n");
            }
            return OTDR_COMMUNICATION_ERROR;
        }
    }
    else
    {   /* within timeout, there is no data is available on UART */
        if (g_ui8_debug_serial)
        {   printf("Serial:No data received till timeout\n");
        }
        return OTDR_SERIAL_READ_TIMEOUT;
    }

    return 0;
}
/**
 *
 * @brief	Read and write command through serial for Switch
 *			Write command on serial and get relevant response from the Board.
 *
 * @param	filehandler				filehandler for serial node
 * @param	i_puc_cmd				input command
 * @param	i_ui16_CmdLen   		input command length
 * @param	o_puc_response  		response
 * @param	o_pui16_ResponseLen 	response length
 * @param	timeout				 	timeout specific to command
 *
 * @return  On success returns 0, error code otherwise.
 *
 */
int serialSwitchReadWrite(int filehandler, uint8_t *i_puc_cmd,
                          uint16_t i_ui16_CmdLen, uint8_t *o_puc_response,
                          uint32_t *o_pui16_ResponseLen, uint32_t timeout)
{   uint16_t l_ui16_count = 0;
    int16_t l_i16_retval = 0;
    uint32_t read_bytes = 0;
    uint32_t read_chunk = 0;
    struct timeval l_x_time =
    {   0
    };
    fd_set l_x_rfds;

    if (i_puc_cmd == NULL || o_puc_response == NULL
            || o_pui16_ResponseLen == NULL)
    {   return SWITCH_NULL_POINTER;
    }

    if (*o_pui16_ResponseLen == 0)
    {   read_chunk = DEFAULT_SERIAL_READ_SIZE;
    }
    else
    {   read_chunk = *o_pui16_ResponseLen;
        *o_pui16_ResponseLen = 0;
    }

    if (tcflush(filehandler, TCIOFLUSH) < 0)//Flush Terminal Input and Output both
    {   return SWITCH_COMMUNICATION_ERROR;
    }

    while (l_ui16_count < i_ui16_CmdLen)
    {   if ((l_i16_retval = write(filehandler, &i_puc_cmd[l_ui16_count], 1))
                < 1)
        {   return SWITCH_COMMUNICATION_ERROR;
        }
        l_ui16_count++;
    }

    if (g_ui8_debug_serial)
    {   printf("Serial:command write on uart successfully\n");
    }

    l_i16_retval = -1;

    /* Watch stdin (filehandler 0) to see when it has input. */
    FD_ZERO(&l_x_rfds);
    FD_SET(filehandler, &l_x_rfds);

    /* Wait up to 100 ms. */
    l_x_time.tv_sec = 0;
    l_x_time.tv_usec = timeout;

    /* select observe the filehandler get any data or not */
    l_i16_retval = select(filehandler + 1, &l_x_rfds, NULL, NULL, &l_x_time);

    /* system error if select fails return -1 */
    if (l_i16_retval == -1)
    {   return SWITCH_COMMUNICATION_ERROR;
    }
    /* l_i16_retval is positive, data is available for read */
    else if (l_i16_retval > 0)
    {   // Data is available now
        if ((read_bytes = read(filehandler,
                               &o_puc_response[*o_pui16_ResponseLen], read_chunk)) < 1)
        {   //if read fail
            if (g_ui8_debug_serial)
            {   printf("Serial:Read fails\n");
            }
            return SWITCH_COMMUNICATION_ERROR;
        }
        else
        {   /* Read operation successful */
            *o_pui16_ResponseLen += read_bytes;

            if (g_ui8_debug_serial)
            {   printf(
                    "\n[SERIAL]In READ loop: read byte: %d, total byte: %d \n",
                    read_bytes, *o_pui16_ResponseLen);
                printf("Serial:Received bytes: %s\n", o_puc_response);
            }
        }
    }
    else
    {   /* within timeout, there is no data is available on UART */
        if (g_ui8_debug_serial)
        {   printf("Serial:No data received till timeout\n");
        }
        return SWITCH_READ_TIMEOUT;
    }

    return 0;
}

/**
 *
 * @brief	Read and write command through serial for OCM.
 *			Write command on serial and get relevant response from the Board.
 *
 * @param	filehandler				filehandler for serial node
 * @param	i_puc_cmd				input command
 * @param	i_ui16_CmdLen   		input command length
 * @param	o_puc_response  		response
 * @param	o_pui16_ResponseLen 	response length
 * @param	timeout				 	timeout specific to command
 *
 * @return  On success returns 0, error code otherwise.
 *
 */
int serialOcmReadWrite(int filehandler, uint8_t *i_puc_cmd,
                       uint16_t i_ui16_CmdLen, uint8_t *o_puc_response,
                       uint32_t *o_pui16_ResponseLen, uint32_t timeout)
{   uint16_t l_ui16_count = 0;
    int16_t l_i16_retval = 0;
    uint32_t read_bytes = 0;
    uint32_t read_chunk = 0;
    struct timeval l_x_time =
    {   0
    };
    fd_set l_x_rfds;

    if (i_puc_cmd == NULL || o_puc_response == NULL
            || o_pui16_ResponseLen == NULL)
    {   return OCM_NULL_POINTER;
    }

    if (*o_pui16_ResponseLen == 0)
    {   read_chunk = DEFAULT_SERIAL_READ_SIZE;
    }
    else
    {   read_chunk = *o_pui16_ResponseLen;
        *o_pui16_ResponseLen = 0;
    }

    if (tcflush(filehandler, TCIOFLUSH) < 0)//Flush Terminal Input and Output both
    {   return OCM_COMMUNICATION_ERROR;
    }

    while (l_ui16_count < i_ui16_CmdLen)
    {   if ((l_i16_retval = write(filehandler, &i_puc_cmd[l_ui16_count], 1))
                < 1)
        {   return OCM_COMMUNICATION_ERROR;
        }
        l_ui16_count++;
    }

    if (g_ui8_debug_serial)
    {   printf("Serial:command write on uart successfully\n");
    }

    l_i16_retval = -1;
    while(1)
    {   /* Watch stdin (filehandler 0) to see when it has input. */
        FD_ZERO(&l_x_rfds);
        FD_SET(filehandler, &l_x_rfds);

        /* Wait up to 100 ms. */
        l_x_time.tv_sec = 0;
        l_x_time.tv_usec = timeout;

        /* select observe the filehandler get any data or not */
        l_i16_retval = select(filehandler + 1, &l_x_rfds, NULL, NULL, &l_x_time);

        /* system error if select fails return -1 */
        if (l_i16_retval == -1)
        {   return (int)OCM_COMMUNICATION_ERROR;
        }
        /* l_i16_retval is positive, data is available for read */
        else if (l_i16_retval > 0)
        {   // Data is available now
            if ((read_bytes = read(filehandler,
                                   &o_puc_response[*o_pui16_ResponseLen], read_chunk)) < 1)
            {   //if read fail
                if (g_ui8_debug_serial)
                {   printf("Serial:Read fails\n");
                }
                return (int)OCM_COMMUNICATION_ERROR;
            }
            else
            {   /* Read operation successful */
                *o_pui16_ResponseLen += read_bytes;

                if (g_ui8_debug_serial)
                {   printf(
                        "\n[SERIAL]In READ loop: read byte: %d, total byte: %d \n",
                        read_bytes, *o_pui16_ResponseLen);
                    printf("Serial:Received bytes: %s\n", o_puc_response);
                }
            }
        }
        else
        {   if(o_pui16_ResponseLen != 0)
            {   break;
            }

            /* within timeout, there is no data is available on UART */
            if (g_ui8_debug_serial)
            {   printf("Serial:No data received till timeout\n");
            }
            return (int)OCM_READ_TIMEOUT;
        }

    }

    return 0;
}
