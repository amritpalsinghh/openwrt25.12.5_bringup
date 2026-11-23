/**************************************************************************************************
 *
 * @file   power_socket_client.h
 *
 * @brief   socket client header file.
 *			This file contains all socket communication related function, global variable, macros and enum for power management.
 *
 **************************************************************************************************/


#ifndef POWER_SOCKET_CLIENT_H
#define POWER_SOCKET_CLIENT_H

#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <time.h>
#include <signal.h>

//Response and Error buffer size
#define MAX_ERR_BUFFER_SIZE			255                                        //maximum error buffer size
#define FINAL_BUFFER_SIZE			8192                                       //response buffer size
#define MAX_RESP_BUFFER_SIZE		255
#define DEFAULT_SERIAL_READ_SIZE 	8192

#define DEVICE_READ_TIMEOUT           1051
#define DEVICE_COMMUNICATION_ERROR    1052
#define DEVICE_NULL_POINTER           1053

#define POWER_BUSY_ERROR              1054
#define POWER_SOCKET_INIT_FAILED      1055
#define POWER_SOCKET_DEINIT_FAILED    1056


/**************************************************************************************************
 * @brief   Initialize Socket. This function open Unix socket and initialize parameters
 *
 * @param   sockfd  socket file descriptor
 *
 * @return  On success returns 0 and error code otherwise.
 *
 **************************************************************************************************/
int powerMgtSocketInit(int * o_ui16_filehandler);

/**************************************************************************************************
 * @brief   Close socket. This function closes the socket from client side
 *
 * @return  On success, returns 0, error code otherwise.
 *
 **************************************************************************************************/
int powerMgtsocketDeInit(int filehandler);

/**************************************************************************************************
 * @brief	Read and write command through socket for OTDR, OCM and SFP.
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
int socketPowerMgtReadWrite(int filehandler, uint8_t *i_puc_cmd, uint16_t i_ui16_CmdLen, uint8_t *o_puc_response, uint32_t *o_pui16_ResponseLen, uint32_t timeout);

#endif
