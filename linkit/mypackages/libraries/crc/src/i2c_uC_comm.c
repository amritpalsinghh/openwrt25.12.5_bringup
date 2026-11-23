// i2c_uC_comm.c -- (c)2020-2021 II-VI Incorporated
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "calc_crc8.h"
#include "xmfh_err.h"
#include <errno.h>

/* mraa header */
#include "mraa/i2c.h"

#define I2C_IDLE 		            2700
#define I2C_IN_PROGRESS_ERROR       2701
#define I2C_OK				        2702
#define I2C_PROTOCOL_ERROR 	        2703
#define I2C_NACK_ERROR              2704
#define I2C_ARBL_ERROR              2705
#define I2C_STATUS_TIMEOUT_ERROR    2706
#define I2C_SKIPPED_ERROR        	2707
#define I2C_UNEXPECTED_ERROR        2708
#define I2C_TIMEOUT		        	2709
#define I2C_MRAA_WRITE_ERROR		2710
#define I2C_MRAA_READ_ERROR	    	2711
#define I2C_CRC_MISMATCH_ERROR		2712
#define I2C_SEQID_MISMATCH_ERROR	2713
#define I2C_INVALID_LENGTH_ERROR	2714
#define I2C_INPROGRESS_TIMEOUT	    2715
#define I2C_DATA_NOT_READY          2716

#define I2C_READ_TIMEOUT_CONFIG  "/tmp/i2cReadTimeoutConfig"
#define I2C_INPROGRESS_TIMEOUT_CONFIG  "/tmp/i2cInProgressTimeoutConfig"

#define I2C_READ_TIMEOUT_MS 100
#define I2C_INPROGRESS_TIMEOUT_MS 10
#define I2C_IN_PROGRESS_STATUS 1

#define PSEUDO_SLAVE_ADDRESS    0x66
uint8_t g_ui8_seq_id = 0;

unsigned int g_ui_readTimeout = I2C_READ_TIMEOUT_MS;
unsigned int g_ui_InProgressTimeout = I2C_INPROGRESS_TIMEOUT_MS;

int g_i_timeout_en = 1;

void i2c_inprogress_timeout_configure( )
{   if(access( I2C_INPROGRESS_TIMEOUT_CONFIG, F_OK ) == 0)
    {   FILE *fp = fopen(I2C_INPROGRESS_TIMEOUT_CONFIG,"r");
        fscanf(fp,"%u %d", &g_ui_InProgressTimeout, &g_i_timeout_en);
        fclose(fp);
    }
    else
    {   g_i_timeout_en = true;
        g_ui_InProgressTimeout = I2C_INPROGRESS_TIMEOUT_MS;
    }

}

void i2c_read_timeout_configure( )
{   if(access( I2C_READ_TIMEOUT_CONFIG, F_OK ) == 0)
    {   FILE *fp = fopen(I2C_READ_TIMEOUT_CONFIG,"r");
        fscanf(fp,"%u",&g_ui_readTimeout);
        fclose(fp);
    }
    else
    {   g_ui_readTimeout = I2C_READ_TIMEOUT_MS;
    }

}

long long time_diff_ms ( struct timespec *ts0, struct timespec *ts1 )
{   return ((ts1->tv_sec * 1000000000LL + ts1->tv_nsec) - (ts0->tv_sec * 1000000000LL + ts0->tv_nsec)) / 1000000LL;
}

int i2c_read (mraa_i2c_context i2c, unsigned char *out_payload, unsigned char nDataLength, unsigned char *sequenceID, bool is_cached)
{   /*  response format from slave as following:
     byte[0] = total length of whole response
     byte[1] = sequence id of the command
     byte[2] = status of the command process, 0 = idle, 1 = busy, 2 = ok.
    	protocol error (3) [illeagal len /crc in Tx request packet - transmission or programer err],
    	nack (4), bus / Arbitration error (5), timed out (6), and skipped (7) commands.
    	(For MCU internal commands, only status 2 and 3 are possible.)
     byte[3] to byte[length - 2] = payload data (e.g. ascii response)
     byte[length - 1] = crc.

     After send the command to slave, master can just poll the first 3 bytes to check the status byte, which is byte[2],
     Until the byte[2] is not 1, then get the whole response according to length in the byte[0]
    */
    unsigned char responseBuf[nDataLength+4];
    unsigned char responseBufOld[3] = { 0, 0, 0 };
    struct timespec ts0, ts1, ts2;
    int rtn = 0;
    int cacheCnt = 0;

    clock_gettime(CLOCK_MONOTONIC, &ts0);
    memset(responseBuf, 0, nDataLength+4);
    i2c_inprogress_timeout_configure();
    i2c_read_timeout_configure();
    do
    {   const size_t len = mraa_i2c_read(i2c, responseBuf, 3);
        if(len == 0 && errno == 6)
        {   return DEVICE_I2C_NACK_ERROR;
        }
        else if(len != 3)
        {   return I2C_MRAA_READ_ERROR;
        }
        for (int i = 0; i < 3; i++)
            responseBufOld[i] = responseBuf[i];
        clock_gettime(CLOCK_MONOTONIC, &ts1);
    }
    while (((responseBuf[0] < 3) || (responseBuf[2] < 2)) && (time_diff_ms(&ts0, &ts1) < g_ui_readTimeout));
    const long long t_ms = time_diff_ms(&ts0, &ts1);

    if (t_ms >= g_ui_readTimeout)
        return I2C_MRAA_READ_ERROR;

    for ( ; ; )
    {   const size_t len = mraa_i2c_read(i2c, responseBuf, nDataLength+4);
        if(len == 0 && errno == 6)
        {   return DEVICE_I2C_NACK_ERROR;
        }
        else if(len != nDataLength+4)
        {   return I2C_INVALID_LENGTH_ERROR;
        }
        clock_gettime(CLOCK_MONOTONIC, &ts2);
        const long long t_ms = time_diff_ms(&ts1, &ts2); // ((ts2.tv_sec - ts1.tv_sec)*1e9 + (ts2.tv_nsec - ts1.tv_nsec)) * 1e-6;

        if(rtn == I2C_IN_PROGRESS_ERROR && t_ms > g_ui_InProgressTimeout)
            return I2C_INPROGRESS_TIMEOUT;

        if (t_ms  > g_ui_readTimeout)
            return I2C_TIMEOUT;

        if ((responseBuf[0] <= nDataLength+4) && (calc_crc8_msbfirst(0, responseBuf, responseBuf[0]) != 0))
        {   rtn = I2C_CRC_MISMATCH_ERROR;
            break;
        }

        if (responseBuf[1] != *sequenceID)
        {   rtn = I2C_SEQID_MISMATCH_ERROR;
            break;
        }

        switch(responseBuf[2])
        {   case 0:
                rtn = I2C_IDLE;
                break;
            case 1:
                rtn = I2C_IN_PROGRESS_ERROR;
                if(g_i_timeout_en)
                    continue;
                else
                    break;
            case 2:
                rtn = 0;
                break;
            case 3:
                rtn = I2C_PROTOCOL_ERROR;
                break;
            case 4:
                rtn = I2C_NACK_ERROR;
                break;
            case 5:
                rtn = I2C_ARBL_ERROR;
                break;
            case 6:
                rtn = I2C_STATUS_TIMEOUT_ERROR;
                break;
            case 7:
                rtn = I2C_SKIPPED_ERROR;
                break;
            default:
                rtn = I2C_UNEXPECTED_ERROR;
                break;
        }

        break;
    }

    /*Checking for Cache Count of freescale*/
    if((!rtn) && (is_cached))
    {   cacheCnt |= responseBuf[responseBuf[0]-5];
        cacheCnt |= (responseBuf[responseBuf[0]-4] & 255) << 8;
        cacheCnt |= (responseBuf[responseBuf[0]-3] & 255) << 16;
        cacheCnt |= (responseBuf[responseBuf[0]-2] & 255) << 24;
        if(!cacheCnt)
            rtn = I2C_DATA_NOT_READY;

    }
    int i = 0;

    if ((out_payload != NULL) && (!rtn))
    {   for (i = 0; i < (responseBuf[0] - 4); i++)
        {   out_payload[i] = responseBuf[i+3];
        }
        out_payload[i]	 = '\0';
    }

    return rtn;
}


/*
* device_read: read bytes from gauge (must be implemented for a specific system)
* i2c: handle to communications adapater
* request: request frame
* requestLength: request frame length
* out_payload: pointer to a data buffer; output
* sequenceID : sequence ID
* return value: number of bytes read (0 if error)
*/
int device_read(mraa_i2c_context i2c, unsigned char *request,size_t requestLength,unsigned char *out_payload, unsigned char *sequenceID)
{   int l_i_status = 0;
    *sequenceID = ++g_ui8_seq_id;
    size_t len = 0;
    bool is_cached = 0;

    if((request) && (request[0] == PSEUDO_SLAVE_ADDRESS))
        is_cached = 1;

    /**Check the response length */
    if (request[requestLength - 1] < 1) return I2C_INVALID_LENGTH_ERROR;

    unsigned char buf[4+requestLength];
    memset(buf, 0x00, 4+requestLength);
    for (int i = 0; i < request[requestLength - 1]; i++)
        out_payload[i] = 0;

    buf[len++] = 4+requestLength;
    buf[len++] = *sequenceID;
    buf[len++] = 'r'; // 0x72
    for (size_t i = 0; i < requestLength; i++)
    {   buf[len++] = request[i];
    }
    buf[len] = calc_crc8_msbfirst(0, buf, buf[0]-1);

    l_i_status  = mraa_i2c_write( i2c, buf, buf[0]);
    if(l_i_status != MRAA_SUCCESS && errno == 6)
    {   return DEVICE_I2C_NACK_ERROR;
    }
    else if(l_i_status != MRAA_SUCCESS)
    {   return  I2C_MRAA_WRITE_ERROR;
    }
    l_i_status = i2c_read(i2c, out_payload,buf[len - 1], sequenceID, is_cached);

    return l_i_status;
}

/*
* device_write: Write [nLength] bytes to the gauge starting at [nRegister].
* i2c: handle to communications adapater
* nRegister: first register (=standard command) to write to
* pData: pointer to a data buffer
* nDataLength: number of data bytes
* return value: number of bytes written (0 if error)
*/
int device_write(mraa_i2c_context i2c, unsigned char nDevice, unsigned char nRegister, unsigned char *pData, unsigned char nDataLength, unsigned char *sequenceID)
{   unsigned char buf[64] = {0};
    int l_i_status = 0;

    if (nDataLength < 1) return I2C_INVALID_LENGTH_ERROR;

    *sequenceID = ++g_ui8_seq_id;

    buf[0] = nDataLength+6;
    buf[1] = *sequenceID;
    buf[2] = 'w'; // 0x77
    buf[3] = nDevice;
    buf[4] = nRegister;

    for (int i = 0; i < nDataLength; i++)
        buf[i+5] = pData[i];
    buf[nDataLength+5] = calc_crc8_msbfirst(0, buf, buf[0]-1);


    l_i_status  = mraa_i2c_write(i2c, buf, nDataLength+6);
    if(l_i_status != MRAA_SUCCESS && errno == 6)
    {   return DEVICE_I2C_NACK_ERROR;
    }
    else if(l_i_status != MRAA_SUCCESS)
    {   return  I2C_MRAA_WRITE_ERROR;
    }

    return i2c_read(i2c, 0, 0, sequenceID, 0);
}
