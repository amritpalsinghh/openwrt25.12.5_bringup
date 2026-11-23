/**
 *  @file 	socket_client.c
 *  @brief 	Socket communication Source file
 *
 *  	This file contains Init, DeInit and read write functions
 *  	for communication through Socket as a client
 *
 */

#include "power_socket_client.h"
#include <sys/un.h>

#define POWER_SOCKET_NAME "/tmp/powermgt_daemon"
#define SA struct sockaddr

/* String used for safe read / write operations */
uint8_t str_print2[128] = { };
uint8_t g_ui8_debug_power_socket = 0;

/* Function for safe write operation */
ssize_t power_safe_write(int fd, const void *buf, size_t count)
{   ssize_t n;

    do
    {   n = write(fd, buf, count);
    }
    while (n < 0 && errno == EINTR);

    return n;
}


/**************************************************************************************************
 * @brief   Initialize Socket. This function open Unix socket and initialize parameters
 *
 * @param   sockfd  socket file descriptor
 *
 * @return  On success returns 0 and error code otherwise.
 *
 **************************************************************************************************/
int powerMgtSocketInit(int * sockfd)
{   struct sockaddr_un addr;

    // socket create and varification
    *sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*sockfd == -1)
    {   return (int)POWER_SOCKET_INIT_FAILED;
    }
    bzero(&addr, sizeof(addr));
    /* Connect socket to socket address */
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, POWER_SOCKET_NAME, sizeof(addr.sun_path) - 1);

    // connect the client socket to server socket
    if (connect(*sockfd, (const struct sockaddr *) &addr,
                sizeof(addr)) != 0)
    {   close(*sockfd);
        *sockfd = -1;
        return (int)POWER_SOCKET_INIT_FAILED;
    }
    return 0;
}

/**************************************************************************************************
 * @brief   Close socket. This function closes the socket from client side
 *
 * @return  On success, returns 0, error code otherwise.
 *
 **************************************************************************************************/
int powerMgtsocketDeInit(int sockfd)
{   if (close(sockfd) < 0)						//close node
    {   return POWER_SOCKET_DEINIT_FAILED;
    }

    if (g_ui8_debug_power_socket)
    {   memset(str_print2, 0, sizeof(str_print2));
        strcpy((char*)str_print2, "Power management socket deInitialization Done \n");
        power_safe_write(STDOUT_FILENO, str_print2, strlen((const char *)str_print2));
    }
    return 0;
}

/**************************************************************************************************
 * @brief	Read and write command through socket. Write command on socket and get relevant response from the instruments.
 *
 * @param	sockfd					filehandler for socket
 * @param	i_puc_cmd				input command
 * @param	i_ui16_CmdLen   		input command length
 * @param	o_puc_response  		response
 * @param	o_pui16_ResponseLen 	response length
 * @param	timeout				 	timeout specific to command
 *
 * @return  On success returns 0 and error code otherwise.
 *
 **************************************************************************************************/
int socketPowerMgtReadWrite(int sockfd, uint8_t *i_puc_cmd,
                            uint16_t i_ui16_CmdLen, uint8_t *o_puc_response,
                            uint32_t *o_pui16_ResponseLen, uint32_t timeout)
{   uint16_t l_ui16_count = 0;
    int l_i_retval = 0;
    int read_bytes = 0;
    uint32_t read_chunk = 0;
    struct timeval l_x_time = { 0 };
    fd_set l_x_rfds;

    if (i_puc_cmd == NULL || o_puc_response == NULL
            || o_pui16_ResponseLen == NULL)
    {   return DEVICE_NULL_POINTER;
    }

    if (*o_pui16_ResponseLen == 0)
    {   read_chunk = DEFAULT_SERIAL_READ_SIZE;
    }
    else
    {   read_chunk = *o_pui16_ResponseLen;
        *o_pui16_ResponseLen = 0;
    }

    if ((l_i_retval = write(sockfd, &i_puc_cmd[l_ui16_count], i_ui16_CmdLen))
            < 1)
    {   return DEVICE_COMMUNICATION_ERROR;
    }


    l_i_retval = -1;
    while (1)
    {   /* Watch stdin (filehandler 0) to see when it has input. */
        FD_ZERO(&l_x_rfds);
        FD_SET(sockfd, &l_x_rfds);

        /* Wait up to 100 ms. */
        l_x_time.tv_sec = 0;
        l_x_time.tv_usec = timeout;

        /* select observe the filehandler get any data or not */
        do
            l_i_retval = select(sockfd + 1, &l_x_rfds, NULL, NULL,
                                &l_x_time);
        while ((l_i_retval == -1) && (errno == EINTR));

        /* system error if select fails return -1 */
        if (l_i_retval == -1)
        {   return DEVICE_COMMUNICATION_ERROR;
        }
        /* l_i_retval is positive, data is available for read */
        else if (l_i_retval > 0)     // Data is available now
        {   if ((read_bytes = read(sockfd,
                                   &o_puc_response[*o_pui16_ResponseLen], read_chunk)) < 1)
            {   return DEVICE_COMMUNICATION_ERROR;
            }
            else
            {   /* Read operation successful */
                *o_pui16_ResponseLen += read_bytes;

                // Delay of 10ms in order to allow socket communication smoothly/
                usleep(10000);
                break;
            }
        }
        else     /* within timeout, there is no data is available on socket */
        {   return DEVICE_READ_TIMEOUT;
        }
    }

    return 0;
}
