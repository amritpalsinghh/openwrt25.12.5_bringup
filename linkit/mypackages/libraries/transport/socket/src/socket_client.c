/**
 *  @file 	socket_client.c
 *  @brief 	Socket communication Source file
 *
 *  	This file contains Init, DeInit and read write functions
 *  	for communication through Socket as a client
 *
 */

#include <sys/un.h>
#include "socket_client.h"
#include "otdr_interface.h"
#include "logMgr.h"

#define LOCK_FILE "/var/lock/ii_vi_otdr_emu.LCK"
#define SOCKET_NAME "/tmp/ii_vi_otdr_emu"
#define PORT 8080
#define SA struct sockaddr

/* String used for safe read / write operations */
uint8_t str_print1[128] = { };
uint8_t g_ui8_debug_socket = 0;
ssize_t safe_write(int fd, const void *buf, size_t count);

/**
 * @brief  Initialize Socket
 *		   This function open Unix socket and initialize parameters
 *
 *@param   sockfd  socket file descriptor
 *
 *@return  On success returns 0 and error code otherwise.
 *
 */
int socketInit(int * sockfd)
{   struct sockaddr_un addr;

    // ensure socket not already in use
    FILE *fp = fopen(LOCK_FILE, "r");
    if (fp != NULL)
    {   int lock_pid = -1;
        fscanf(fp, "%d", &lock_pid);
        fclose(fp);
        if (lock_pid < 0)
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s: error reading lock file. Return Value: %s]", __func__, "BUSY_ERROR");
            return (int)BUSY_ERROR; //error reading file
        }
        if (lock_pid != getpid())
        {   char lock_pid_path[32];
            sprintf(lock_pid_path, "/proc/%d", lock_pid);
            if (access(lock_pid_path, F_OK) == 0) // process exists
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s: socket already in use by pid %d. Return Value: %s]", __func__, lock_pid, "BUSY_ERROR");
                return (int)BUSY_ERROR;
            }
            if (remove(LOCK_FILE) != 0)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s: error removing lock file. Return Value: %s]", __func__, "BUSY_ERROR");
                return (int)BUSY_ERROR;
            }
            fp = NULL;
        }
    }
    if (fp == NULL)
    {   fp = fopen(LOCK_FILE, "w");
        if (fp == NULL)
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s: error creating lock file. Return Value: %s]", __func__, "BUSY_ERROR");
            return (int)BUSY_ERROR;
        }
        const int n = fprintf(fp, "%d", getpid());
        if ((fclose(fp) != 0) || (n < 0))
        {   remove(LOCK_FILE);
            LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s: error writing lock file. Return Value: %s]", __func__, "BUSY_ERROR");
            return (int)BUSY_ERROR;
        }
    }

    // socket create and varification
    *sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*sockfd == -1)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s:socket creation failed. Return Value: %s]", __func__, "SOCKET_INIT_FAILED");
        remove(LOCK_FILE);
        return (int)SOCKET_INIT_FAILED;
    }
    bzero(&addr, sizeof(addr));
    /* Connect socket to socket address */
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_NAME, sizeof(addr.sun_path) - 1);

    // connect the client socket to server socket
    if (connect(*sockfd, (const struct sockaddr *) &addr,
                sizeof(addr)) != 0)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[%s: Connection with the server failed. Return Value: %s]", __func__, "SOCKET_INIT_FAILED");
        remove(LOCK_FILE);
        close(*sockfd);
        *sockfd = -1;
        return (int)SOCKET_INIT_FAILED;
    }
    return 0;
}

/**
 *
 *  @brief   Close socket
 *  		 This function closes the socket from client side
 *
 *
 *  @return  On success, returns 0, error code otherwise.
 *
 */
int socketDeInit(int sockfd)
{   remove(LOCK_FILE);

    if (close(sockfd) < 0)						//close node
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[socketDeInit: Deinit failed. Return Value: %s]", "SOCKET_DEINIT_FAILED");
        return SOCKET_DEINIT_FAILED;
    }

    if (g_ui8_debug_socket)
    {   memset(str_print1, 0, sizeof(str_print1));
        strcpy((char*)str_print1, "Serial DeInitialization Done \n");
        safe_write(STDOUT_FILENO, str_print1, strlen((const char *)str_print1));
    }
    return 0;
}

/**
 *
 * @brief	Read and write command through socket for OTDR.
 *			Write command on socket and get relevant response from the OTDR.
 *
 * @param	sockfd					filehandler for socket
 * @param	i_puc_cmd				input command
 * @param	i_ui16_CmdLen   		input command length
 * @param	o_puc_response  		response
 * @param	o_pui16_ResponseLen 	response length
 * @param	timeout				 	timeout specific to command
 *
 * @return  On success returns 0, error code otherwise.
 *
 */
int socketOtdrReadWrite(int sockfd, uint8_t *i_puc_cmd,
                        uint16_t i_ui16_CmdLen, uint8_t *o_puc_response,
                        uint32_t *o_pui16_ResponseLen, uint32_t timeout)
{   uint16_t l_ui16_count = 0;
    int l_i_retval = 0;
    int read_bytes = 0;
    uint32_t read_chunk = 0;
    struct timeval l_x_time = { 0 };
    fd_set l_x_rfds;

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:Entering function: %s: timeout:%d]", __func__, timeout);

    if (i_puc_cmd == NULL || o_puc_response == NULL
            || o_pui16_ResponseLen == NULL)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[socketOtdrReadWrite: NULL Pointer Error. Return Value: %d]", OTDR_NULL_POINTER);
        return OTDR_NULL_POINTER;
    }

    if (*o_pui16_ResponseLen == 0)
    {   read_chunk = DEFAULT_SERIAL_READ_SIZE;
    }
    else
    {   read_chunk = *o_pui16_ResponseLen;
        *o_pui16_ResponseLen = 0;
    }

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:Before Write operation]");
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:Socket FD: %d, Command: %s, Command Length: %d]", sockfd, i_puc_cmd, i_ui16_CmdLen);

    if ((l_i_retval = write(sockfd, &i_puc_cmd[l_ui16_count], i_ui16_CmdLen))
            < 1)
    {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:Write function Failed (returned: %d). Return Value: %d]", l_i_retval, OTDR_COMMUNICATION_ERROR);
        return OTDR_COMMUNICATION_ERROR;
    }

    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:Write function successful. Returned: %d]", l_i_retval);
    
    if(strstr(i_puc_cmd,"reinit") != NULL)
    {
        l_i_retval = 0;
        goto end_func; 
    }
	/*** For the External OTDR(XMF-HD) the socket timeout value should be increased to 
	* synchronise with the UART(240kbps) speed ****/  

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
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[socketOtdrReadWrite: Select failed (returned %d). Return Value: %d]", l_i_retval, OTDR_COMMUNICATION_ERROR);
            return OTDR_COMMUNICATION_ERROR;
        }
        /* l_i_retval is positive, data is available for read */
        else if (l_i_retval > 0)     // Data is available now
        {   if ((read_bytes = read(sockfd,
                                   &o_puc_response[*o_pui16_ResponseLen], read_chunk)) < 1)
            {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[socketOtdrReadWrite: Read failed (Response Length: %d) (returned %d), Return Value: %d]", *o_pui16_ResponseLen, read_bytes, OTDR_COMMUNICATION_ERROR);
                return OTDR_COMMUNICATION_ERROR;
            }
            else
            {   /* Read operation successful */
                *o_pui16_ResponseLen += read_bytes;

                if (g_ui8_debug_socket)
                {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:In READ loop: read byte: %d, total byte: %d]"
                             , read_bytes, *o_pui16_ResponseLen);
                    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:Received data: %s]", o_puc_response);
                }

                /* Check for end of response from OTDR */
                if (o_puc_response[*o_pui16_ResponseLen - 1] == '>') //here end of the response is '>'
                {   o_puc_response[*o_pui16_ResponseLen - 1] = '\0';

                    // Delay of 10ms in order to allow socket communication smoothly
                    usleep(10000);
                    break;
                }
            }
        }
        else     /* within timeout, there is no data is available on socket */
        {   LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[socketOtdrReadWrite: No data received till timeout(%d msec elapsed). Return Value: %d]", timeout/1000,OTDR_SOCKET_READ_TIMEOUT);
            o_puc_response[0] = '\0';
            *o_pui16_ResponseLen = 0;
            return OTDR_SOCKET_READ_TIMEOUT;
        }
    }
end_func:
    LOG_INFO(&g_px_lgrFileptr,NULL, g_i_lgrOtdrMode, "OTDR:[Socket:Exiting function: %s: Return Value: %d]", __func__, l_i_retval);

    return 0;
}
