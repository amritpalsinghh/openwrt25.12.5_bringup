/**
 *  @file 	serial.h
 *  @brief 	Serial header file
 *  		This file contains all serial communication related functions, global variables and macros
 *
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <termios.h>
#include <stdio.h>
#include <unistd.h>
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
#define MAX_ERR_BUFFER_SIZE             255    //maximum error buffer size
#define FINAL_BUFFER_SIZE               8192   //response buffer size
#define MAX_RESP_BUFFER_SIZE            255
#define DEFAULT_SERIAL_READ_SIZE        8192

#define OTDR_SERIAL_READ_TIMEOUT        1032
#define OTDR_COMMUNICATION_ERROR        1033

#define SWITCH_READ_TIMEOUT             1156
#define SWITCH_COMMUNICATION_ERROR      1157

#define OCM_COMMUNICATION_ERROR         1250
#define OCM_READ_TIMEOUT                1251

#define SERIAL_INIT_FAILED              1475
#define SERIAL_DEINIT_FAILED            1476
#define BUSY_ERROR                      1375
#define UNKWN_ERROR                     1076

#define OTDR_NULL_POINTER               1034
#define OCM_NULL_POINTER                1252
#define SWITCH_NULL_POINTER             1158

/************************ Library functions ************************/

/**
 * @brief  Initialize Serial Interface
 *		   This function open serial port and initialize parameters(BaudRate,parity,stop-bit)
 *
 *@param   i_pc_SerialNode  serial node
 *@param   o_filehandler file handler for init operation
 *
 *@return  On success returns 0 and error code otherwise.
 *
 */
int serialPortInit(char *i_puc_SerialNode, int * o_filehandler);

/**
 *
 *  @brief   Close serial node
 *  		 This function closes the serial node
 *
 *  @return  On success, returns 0, error code otherwise.
 *
 */
int serialPortDeInit(char *i_puc_SerialNode, int  o_filehandler);


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
int serialOtdrReadWrite(int filehandler, uint8_t *i_puc_cmd, uint16_t i_ui16_CmdLen, uint8_t *o_puc_response, uint32_t *o_pui16_ResponseLen, uint32_t timeout);

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
int serialSwitchReadWrite(int filehandler, uint8_t *i_puc_cmd, uint16_t i_ui16_CmdLen, uint8_t *o_puc_response, uint32_t *o_pui16_ResponseLen, uint32_t timeout);

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
int serialOcmReadWrite(int filehandler, uint8_t *i_puc_cmd, uint16_t i_ui16_CmdLen,
                       uint8_t *o_puc_response, uint32_t *o_pui16_ResponseLen, uint32_t timeout);

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
                      uint32_t *o_pui16_ResponseLen, uint32_t timeout);
#endif
