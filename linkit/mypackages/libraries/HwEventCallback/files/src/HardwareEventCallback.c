/**
 *  @file 	HardwareEventCallback.c
 *
 *  @brief 	Event callback API Library. The Library will call
 *		Respective registered callback function based on the event received
 *		from the hardware.
 *
 *  This file contains Event callback  APIs definition.
 *
 */

/** HEADER FILES SECTION */
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <stdbool.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include "HardwareEventCallback.h"
#include "otdr_interface.h"
#include <mraa.h>
#include "logMgr.h"

/** DEFINE MACRO */
#define PCF8575DEVNODE				"/dev/pcf857x"
#define SYSFS_GPIO14_VAL_FILE		"/sys/class/gpio/gpio14/value"
#define SYSFS_GPIO2_VAL_FILE		"/sys/class/gpio/gpio2/value"
#define CHANGESBYTE(x,y)	((x >> y) & 0x01)
#define MF_BUTTON_BIT				12
#define MOD_ABS_BIT					6
#define PCF_TX_FAULT_BIT			4
#define GPIO_I2C_INT                2
#define ENABLE_HWEVENT_DEBUG            "/tmp/HwEventDebugFlag"

/** For Logger */
#define HWEVENT_LOGGER_FILE_PATH   "/var/log/HWEVENT.log"
FILE * g_px_HwEventlgrFileptr = NULL;

//#define __HWEVENTDEBUG

#ifdef __HWEVENTDEBUG
#define HWDEBUG(x...)  \
        printf("HwEventCallback : File: [%s] Func: [%s] line: [%d]: ",__FILE__, __FUNCTION__, __LINE__);\
        printf(x);
#else
# define HWDEBUG(x...) do {} while (0)
#endif

/** GLOBAL DECLARATIONS */
void (*EventCallPtr)(E_EVENTS_TYPE i_e_eventName); 	//callback global pointer
int g_fd_devNode = -1;		//device node
int g_fd_pwrStatus = -1; //gpio14 file-descriptor
int g_fd_intI2CStatus = -1; //gpio2 file-descriptor
bool g_b_stopThreads;        // This variable stops PCF threads
bool g_b_stopPwrThread;        // This variable stops the thread
bool g_b_stopI2CInitThread;        // This variable stops I2C init thread
struct timespec g_x_ts_prev = {0};
struct timespec g_x_ts_curr = {0};
extern volatile uint8_t g_b_pcf_data[2];
extern bool g_f_pcf_read;

/** FUNCTIONS DEFINITIONS */
int EventDataProcess(uint8_t *l_ui8_buff, unsigned int l_ui_ChangedByte, unsigned int l_ui_CurrentByte);
/**************************************************************************************************
 * @brief Initialiize SFP logger
 *
 **************************************************************************************************/
void initHWEventLogger(void)
{   /** Initialize Logger */
    setLogOutput(LOG_OUTPUT_FILE);
    if(!g_px_HwEventlgrFileptr)
        setFileLogInfo(&g_px_HwEventlgrFileptr, HWEVENT_LOGGER_FILE_PATH, NULL);
    setLogLevel(LOG_LEVEL_INFO);
}

/**************************************************************************************************
 * @brief deinitialiize SFP logger
 *
 **************************************************************************************************/
void deInitHWEventLogger(void)
{   if(g_px_HwEventlgrFileptr)
        logMgrClose(&g_px_HwEventlgrFileptr);
}
long long time_diff_ms ( struct timespec *ts0, struct timespec *ts1 )
{   return ((ts1->tv_sec * 1000000000LL + ts1->tv_nsec) - (ts0->tv_sec * 1000000000LL + ts0->tv_nsec)) / 1000000LL;
}


/**
 * @brief	The thread will check the gpio2 status and read the PCF
 *          if it stays low more than 50ms.
 *
 */
void *get_i2c_int_sts(void* arg)
{   int l_i_sts = 0;
    uint8_t l_ui8_pcf_data[2] = {0};
    mraa_i2c_context l_p_eventi2c_init = NULL;
    int l_i_length = 0;
    uint8_t l_ui8_intI2CBuff = 0;
    uint32_t l_i_correctionCount = 0;
    bool prev_pcf_data_captured = false;
    uint8_t l_ui8_buff[4] = {0x00000000};
    uint8_t prev_pcf_data[2] = {0};
    uint8_t curr_pcf_data[2] = {0};
    unsigned int l_ui_ChangedByte = 0;	//pcf change byte variable
    unsigned int l_ui_CurrentByte = 0;	//pcf current byte variable

    if(g_fd_intI2CStatus < 0)
    {   printf("ERROR!! %s File is not open\n", SYSFS_GPIO2_VAL_FILE);
        return NULL;
    }

    /**Initialize the I2C */
    l_p_eventi2c_init = mraa_i2c_init(0);
    if (!l_p_eventi2c_init)
    {   printf("ERROR!! Failed to intialize mraa i2c\n");
        return NULL;
    }

    /* 0x27 valid device address */
    l_i_sts = mraa_i2c_address(l_p_eventi2c_init, 0x27);
    if (l_i_sts)
    {   printf("ERROR!!Failed to set the I2C address\n");
        return NULL;
    }

    while(false == g_b_stopI2CInitThread)
    {   if(g_fd_intI2CStatus < 0)
        {   printf("ERROR!! %s File is not open\n", SYSFS_GPIO2_VAL_FILE);
            break;
        }

        l_i_sts = lseek(g_fd_intI2CStatus, 0, SEEK_SET);
        if (l_i_sts == -1)
        {   printf("ERROR!! lseek error\n");
            break;
        }

        if(read(g_fd_intI2CStatus, &l_ui8_intI2CBuff,1) < 0)
        {   printf("ERROR!! Can't read GPIO2 value\n");
            break;
        }

        if(l_ui8_intI2CBuff == '0')
        {   usleep(50000);
            l_i_sts = lseek(g_fd_intI2CStatus, 0, SEEK_SET);
            if (l_i_sts == -1)
            {   printf("ERROR!! lseek error\n");
                break;
            }

            if(read(g_fd_intI2CStatus, &l_ui8_intI2CBuff,1) < 0)
            {   printf("ERROR!! Can't read GPIO2 value\n");
                break;
            }

            if(l_ui8_intI2CBuff == '0')
            {   ++l_i_correctionCount;
                if(!access(ENABLE_HWEVENT_DEBUG,F_OK))
                    printf("Correction count:%d\n",l_i_correctionCount);
                g_f_pcf_read = false;
                /* Read the data */
                l_i_length = mraa_i2c_read(l_p_eventi2c_init,  g_b_pcf_data, 2);
                if (l_i_length  == 0 && errno == 6)
                {   LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:NACK Received");
                    break;
                }
                else if(l_i_length == 0)
                {   LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:Failed to read the data");
                    break;
                }
                g_f_pcf_read = true;

                if(prev_pcf_data_captured == false)
                {
                    prev_pcf_data_captured = true;
                    prev_pcf_data[0] = g_b_pcf_data[0];
                    prev_pcf_data[1] = g_b_pcf_data[1];
                    continue;
                }
                // Updating 4 byte buffer with change byte and current byte as required.
                l_ui8_buff[1] = prev_pcf_data[0] ^ g_b_pcf_data[0]; // Sequence is reversed as its read from the node that way
                l_ui8_buff[0] = prev_pcf_data[1] ^ g_b_pcf_data[1];
                l_ui8_buff[3] = g_b_pcf_data[0];
                l_ui8_buff[2] = g_b_pcf_data[1];

                // Updating previous buffer to current buffer
                prev_pcf_data[0] = g_b_pcf_data[0];
                prev_pcf_data[1] = g_b_pcf_data[1];
                /** Changed Byte Value*/
                l_ui_ChangedByte = l_ui8_buff[0] << 8 | l_ui8_buff[1];
                HWDEBUG("Changed Byte After Read: %x\n", l_ui_ChangedByte);

                /** Current Byte Value*/
                l_ui_CurrentByte = l_ui8_buff[2] << 8 | l_ui8_buff[3];
                HWDEBUG("Current Byte After Read: %x\n", l_ui_CurrentByte);

                // Check the function  pointer is NULL
                if(NULL == EventCallPtr)
                    continue;

                if(EventDataProcess(l_ui8_buff, l_ui_ChangedByte, l_ui_CurrentByte))
                    continue;

                //LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:Correction count:%d, g_b_pcfdata[0]: %x, g_b_pcfdata[1]: %x",l_i_correctionCount,g_b_pcf_data[0], g_b_pcf_data[1]);
                
            }
        }
        sleep(1);
    }
    l_i_sts = mraa_i2c_stop(l_p_eventi2c_init);

    return NULL;
}

/**
 * @brief This function will enable debug logs for HW events.
 */
void enableHwEventDebugLog(bool flag)
{   if(flag)
    {   FILE *fp =fopen(ENABLE_HWEVENT_DEBUG,"w");
        if(fp)
        {   fclose(fp);
            fp=NULL;
        }
    }
    else
    {   if(!access(ENABLE_HWEVENT_DEBUG,F_OK))
            remove(ENABLE_HWEVENT_DEBUG);
    }
}


/**
 * @brief	The thread block on read until any event received.
 *		On event, thread reads the data and callback the event handler accordingly.
 */
void* PwrEventProcess(void* arg)
{   uint8_t l_ui8_prev_pwBuff = 0;
    uint8_t l_ui8_pwBuff[2] = {0};
    fd_set l_fdset_exceptfds;
    int l_i_status = 0;
    long long t_ms = 0;
    bool is_pressed = 0;

    /** Check the Global file descriptor */
    if(g_fd_pwrStatus < 0)
    {   printf("ERROR!! %s File is not open Ret:%d\n", SYSFS_GPIO14_VAL_FILE,g_fd_devNode);
        return NULL;
    }

    //Monitor the GPIO14 value after first read operation
    if(read(g_fd_pwrStatus, &l_ui8_pwBuff,1) < 0)
    {   printf("ERROR!! Can't read GPIO14 value\n");
        return NULL;
    }
    clock_gettime(CLOCK_MONOTONIC, &g_x_ts_prev);

    while(false == g_b_stopPwrThread)
    {   /** Check the Global file descriptor */
        if(g_fd_pwrStatus < 0)
        {   printf("ERROR!! %s File is not open Ret:%d\n", SYSFS_GPIO14_VAL_FILE,g_fd_devNode);
            break;
        }

        l_i_status = lseek(g_fd_pwrStatus, 0, SEEK_SET);
        if (l_i_status == -1)
        {   printf("lseek error\n");
            break;
        }

        E_EVENTS_TYPE l_e_eventType = NO_EVENT;

        FD_ZERO(&l_fdset_exceptfds);
        FD_SET(g_fd_pwrStatus, &l_fdset_exceptfds);

        //Thread remains blocked here until Power Button Event(Pressed, released).
        l_i_status = select(g_fd_pwrStatus + 1, NULL, NULL, &l_fdset_exceptfds, NULL);
        if ((l_i_status < 0) && (errno!=EINTR))
        {   printf("ERROR!! select error \n");
            break;
        }

        if(read(g_fd_pwrStatus, &l_ui8_prev_pwBuff,1) < 0)
        {   printf("ERROR!! Can't read GPIO14 value\n");
            break;
        }
        usleep(15000);

        l_i_status = lseek(g_fd_pwrStatus, 0, SEEK_SET);
        if (l_i_status == -1)
        {   printf("lseek error\n");
            break;
        }

        if(read(g_fd_pwrStatus, &l_ui8_pwBuff,1) < 0)
        {   printf("ERROR!! Can't read GPIO14 value\n");
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &g_x_ts_curr);
        t_ms = time_diff_ms(&g_x_ts_prev, &g_x_ts_curr);


        if(l_ui8_prev_pwBuff != l_ui8_pwBuff[0])
        {

            LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: Glitch detected on Power Button");
            continue;
        }
        LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT:EVENT CAME AFTER %lld ms",t_ms);
        g_x_ts_prev = g_x_ts_curr;

        // Check for the button pressed event
        if(l_ui8_pwBuff[0] == '0')
        {   l_e_eventType = EVENT_PWR_BTN_PRESSED;
            is_pressed = 1;
            HWDEBUG("Button is Pressed\n");
            LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_PWR_BTN_PRESSED");
        }
        else if((l_ui8_pwBuff[0] == '1') && (is_pressed))  //Check for the button release event
        {   l_e_eventType = EVENT_PWR_BTN_RELEASED;
            is_pressed = 0;
            HWDEBUG("Button is released\n");
            LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_PWR_BTN_RELEASED");
        }
        else
        {   LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: Glitch detected on Power Button!!!");
        }

        /** Call back function with specific event */
        if(NULL != EventCallPtr && (l_e_eventType != NO_EVENT))
        {   EventCallPtr(l_e_eventType);
        }
    }

    return NULL;
}


/**
 * @brief	The thread  calls the respective
 *		register callback function.
 */
void* ProcessEventData(void *arg)
{   EventCallPtr(*((E_EVENTS_TYPE *) arg));
    pthread_exit(NULL);
}

/**
 * @brief	The thread block on read until any event received.
 *		On event, thread reads the data and copies the data to
 *		Global variable for further processing.
 */
void* EventDataRead(void* arg)
{   uint8_t l_ui8_buff[4] = {0x00000000};
    uint8_t l_ui8_buff_temp[2] = {0x0000};
    unsigned int l_ui_ChangedByte = 0;	//pcf change byte variable
    unsigned int l_ui_CurrentByte = 0;	//pcf current byte variable
    pthread_t processThread[10] = {0};
    uint8_t prev_pcf_data[2] = {0};
    uint8_t curr_pcf_data[2] = {0};
    bool i2cInitialized = false;
    mraa_i2c_context h_w_eventi2c_init = NULL;
    int l_i_length = 0;

    /**Initialize the I2C */
    h_w_eventi2c_init = mraa_i2c_init(0);
    if (!h_w_eventi2c_init)
    {   printf("ERROR!! Failed to intialize mraa i2c\n");
        return NULL;
    }
    /*Run the device at 100kHz */
    if (mraa_i2c_frequency(h_w_eventi2c_init, MRAA_I2C_STD) != 0)
    {   return NULL;
    }
    /* 0x27 valid device address */    
    if (mraa_i2c_address(h_w_eventi2c_init, 0x27))
    {   printf("ERROR!!Failed to set the I2C address\n");
        return NULL;
    }
    

    while(false == g_b_stopThreads)
    {   /** Check the Global file descriptor */
        if(g_fd_devNode < 0)
        {   printf("ERROR!! PCF Node is not open  %d\n", g_fd_devNode);
            return NULL;
        }

        //Thread remains blocked here until data is available from HW.
        if(read(g_fd_devNode, &l_ui8_buff,4) < 0)
        {   printf("ERROR!! PCF NODE read\n");
            return NULL;
        }
        //Check for the MOD_ABS and TX_FAULT bit changes
        if(CHANGESBYTE(l_ui8_buff[1],MOD_ABS_BIT) != 0 || CHANGESBYTE(l_ui8_buff[1],PCF_TX_FAULT_BIT) != 0)
        {   //Wait for SFP generic assertion time ~2-4ms and read again to get the actual data if anything missed in the first read
            usleep(50000);
            /* Read the data */
            if(mraa_i2c_read(h_w_eventi2c_init, &l_ui8_buff_temp, 2) == 0)
            {   break;
            }
            /*Compare the PCF data*/
            if(l_ui8_buff[3] != l_ui8_buff_temp[0])
            {
                if((l_ui8_buff[3] & (1<<PCF_TX_FAULT_BIT)) != (l_ui8_buff_temp[0] & (1<<PCF_TX_FAULT_BIT)))
                {
                    l_ui8_buff[1] = l_ui8_buff[1] | (1<<PCF_TX_FAULT_BIT);
                    l_ui8_buff[3] &= ~(1<<PCF_TX_FAULT_BIT);
                    l_ui8_buff[3] |= l_ui8_buff_temp[0] & (1<<PCF_TX_FAULT_BIT);
                }
                if((l_ui8_buff[3] & (1<<MOD_ABS_BIT)) != (l_ui8_buff_temp[0] & (1<<MOD_ABS_BIT)))
                {
                    l_ui8_buff[1] = l_ui8_buff[1] | (1<<MOD_ABS_BIT);
                    l_ui8_buff[3] &= ~(1<<MOD_ABS_BIT);
                    l_ui8_buff[3] |= l_ui8_buff_temp[0] & (1<<MOD_ABS_BIT);
                }
            }
        }
        /**
        * Update global buffer with latest data */        
        g_b_pcf_data[0] = l_ui8_buff[3];
        g_b_pcf_data[1] = l_ui8_buff[2];  
            

        /** Changed Byte Value*/
        l_ui_ChangedByte = l_ui8_buff[0] << 8 | l_ui8_buff[1];
        HWDEBUG("Changed Byte After Read: %x\n", l_ui_ChangedByte);

        /** Current Byte Value*/
        l_ui_CurrentByte = l_ui8_buff[2] << 8 | l_ui8_buff[3];
        HWDEBUG("Current Byte After Read: %x\n", l_ui_CurrentByte);

        // Check the function  pointer is NULL
        if(NULL == EventCallPtr)
            continue;

        if(EventDataProcess(l_ui8_buff, l_ui_ChangedByte, l_ui_CurrentByte))
            continue;

        //Work around to clear MF Event, In case Failed to clear on earlier transaction due to conversion to GPIO
        if(l_ui_CurrentByte & 0x1000)
        {   //Buffer data will be ignored driver will perform custom data write operation
            write(g_fd_devNode, l_ui8_buff, 4);
        }
    }

    return NULL;
}

/**
 * @brief	Process the data to log as per hw event occur.
 */
int EventDataProcess(uint8_t *l_ui8_buff, unsigned int l_ui_ChangedByte, unsigned int l_ui_CurrentByte)
{    
    uint32_t l_i_eventCount = 0;
    pthread_t processThread[10] = {0};
    E_EVENTS_TYPE l_e_eventName = NO_EVENT;
    unsigned int l_ui_count = 12;
    while(l_ui_count)
    {   // Check if the Byte is Changed
            if(CHANGESBYTE(l_ui_ChangedByte,l_ui_count) != 0 || CHANGESBYTE(l_ui_ChangedByte,PCF_TX_FAULT_BIT) != 0)
            {   // PCF - P14(12th Bit) - MF_BTN Event
                if(l_ui_count == MF_BUTTON_BIT)
                {   // Check If MF Button is Pressed
                    if(CHANGESBYTE(l_ui_CurrentByte,l_ui_count) != 0)
                    {   //Buffer data will be ignored driver will perform custom data write operation
                        write(g_fd_devNode, l_ui8_buff, 4);
                        l_e_eventName  = EVENT_MF_BUTTON_PRESSED;
                        HWDEBUG("12 Bit is 1\n");
                        LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_MF_BUTTON_PRESSED");
                        //LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:Event count:%d data: %x, event: %d",l_i_eventCount, l_ui_CurrentByte, l_e_eventName);
                    }

                }
                else if(l_ui_count == MOD_ABS_BIT)   // PCF - P6(6th Bit) - Mod_ABS Event, P4(4th Bit) - Tx_fault (SFP adapter for xmf-hd will make only tx_fault line low when it is inserted)
                {   // Check is SFP Inserted or Removed
                    if(CHANGESBYTE(l_ui_CurrentByte,l_ui_count) == 0 || (CHANGESBYTE(l_ui_CurrentByte,PCF_TX_FAULT_BIT) == 0 && CHANGESBYTE(l_ui_ChangedByte,l_ui_count) == 0))
                    {  
                        HWDEBUG("6th 0r 4th Bit is 0\n");
                        //LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_SFP_IN");
                        if( ((((l_ui_CurrentByte >> 4) & 0x01) !=1 ) && (((l_ui_CurrentByte >> 6) & 0x01) == 1)) &&
                            ((((l_ui_ChangedByte >> 4) & 0x01) ==1 ) && (((l_ui_ChangedByte >> 6) & 0x01) != 1)) )
                        {   LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_SFP_ADP_IN");
                            l_e_eventName  = EVENT_SFP_ADP_IN;
                        }
                        else if(
                            ((((l_ui_CurrentByte >> 4) & 0x01) !=1 ) && (((l_ui_CurrentByte >> 6) & 0x01) != 1)) &&
                            ((((l_ui_ChangedByte >> 4) & 0x01) !=1 ) && (((l_ui_ChangedByte >> 6) & 0x01) == 1)) )
                        {   LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_HD_IN");
                            l_e_eventName  = EVENT_HD_IN;
                        }
                        else if(
                            ((((l_ui_CurrentByte >> 4) & 0x01) !=1 ) && (((l_ui_CurrentByte >> 6) & 0x01) != 1)) &&
                             ((((l_ui_ChangedByte >> 4) & 0x01) ==1 ) && (((l_ui_ChangedByte >> 6) & 0x01) == 1)) )
                        {   LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_SFP_IN");
                            l_e_eventName  = EVENT_SFP_IN;
                        }
                        else /* Set SFP IN as default case */ 
                        {   LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_SFP_IN");
                            l_e_eventName  = EVENT_SFP_IN;
                        }
                    }
                    else
                    {   
                        HWDEBUG("6 Bit is 1\n");
                        //LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_SFP_OUT");
                        if(
                            ((((l_ui_CurrentByte >> 4) & 0x01) ==1 ) && (((l_ui_CurrentByte >> 6) & 0x01) == 1)) &&
                            ((((l_ui_ChangedByte >> 4) & 0x01) ==1 ) && (((l_ui_ChangedByte >> 6) & 0x01) != 1)) )
                        {   LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_SFP_ADP_OUT");
                            l_e_eventName  = EVENT_SFP_ADP_OUT;
                        }
                        else if( ((((l_ui_CurrentByte >> 4) & 0x01) !=1 ) && (((l_ui_CurrentByte >> 6) & 0x01) == 1)) &&
                            ((((l_ui_ChangedByte >> 4) & 0x01) !=1 ) && (((l_ui_ChangedByte >> 6) & 0x01) == 1)) )
                        {   LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_HD_OUT");
                            l_e_eventName  = EVENT_HD_OUT;
                        }
                        else if(
                            ((((l_ui_CurrentByte >> 4) & 0x01) ==1 ) && (((l_ui_CurrentByte >> 6) & 0x01) == 1)) &&
                            ((((l_ui_ChangedByte >> 4) & 0x01) ==1 ) && (((l_ui_ChangedByte >> 6) & 0x01) == 1)) )
                        {   LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_SFP_OUT");
                            l_e_eventName  = EVENT_SFP_OUT;
                        }
                        else /* Set SFP OUT as default case */
                        {   LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:EVENT: EVENT_SFP_OUT");
                            l_e_eventName  = EVENT_SFP_OUT;
                        }
                    }
                    //LOG_INFO(&g_px_HwEventlgrFileptr, NULL, 3, "HWEVENT:Event count:%d,changed data:%x, current data: %x, event:%d",l_i_eventCount, l_ui_ChangedByte, l_ui_CurrentByte, l_e_eventName);
                }
            }
            l_ui_count -= 6;
    }
    /** Call back function with specific event */
    if(NULL != EventCallPtr && (l_e_eventName != NO_EVENT))
    {   EventCallPtr(l_e_eventName);
    }

    return 0;
}

/**
 * @brief This function opens common node and starts the threads.
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int HardwareEventCallbackInit(void (*EventCB)(E_EVENTS_TYPE i_e_eventName))
{   pthread_t readThread = 0, gpio_i2cThread = 0, PwrStatusThr = 0;
    int l_i_retStatus = HARDWARE_EVENT_SUCCESS;
    do
    {   if(g_fd_devNode > 0)
        {   l_i_retStatus = HARDWARE_EVENT_REINIT_ERROR;
            break;
        }
        /* Initialize Logger */
        initHWEventLogger();
		#if 0
        //** Process locking mechanism*/
        FILE *l_px_HwEventLockFile = fopen(HW_EVENT_LOCK_FILE, "r");
        if (l_px_HwEventLockFile != NULL)
        {   int l_i_hwEventLockPid = -1;
            fscanf(l_px_HwEventLockFile, "%d", &l_i_hwEventLockPid);
            fclose(l_px_HwEventLockFile);
            if (l_i_hwEventLockPid < 0)
            {   l_i_retStatus = HARDWARE_EVENT_FAIL;    //error reading file
                break;
            }
            if (l_i_hwEventLockPid != getpid())
            {   char l_c_hwEventLockPidPath[32] = {0};
                sprintf(l_c_hwEventLockPidPath, "/proc/%d", l_i_hwEventLockPid);
                if (access(l_c_hwEventLockPidPath, F_OK) == 0) // process exists
                {   l_i_retStatus = HARDWARE_EVENT_REINIT_ERROR;
                    break;
                }
                if (remove(HW_EVENT_LOCK_FILE) != 0)
                {   l_i_retStatus = HARDWARE_EVENT_LOCKFILE_REMOVE_ERROR;
                    break;
                }
                l_px_HwEventLockFile = NULL;
            }
        }
        if (l_px_HwEventLockFile == NULL)
        {   l_px_HwEventLockFile = fopen(HW_EVENT_LOCK_FILE, "w");
            if (l_px_HwEventLockFile == NULL)
            {   l_i_retStatus = HARDWARE_EVENT_LOCKFILE_OPEN_ERROR;
                break;
            }
            const int l_ci_Bytecount = fprintf(l_px_HwEventLockFile, "%d", getpid());
            if ((fclose(l_px_HwEventLockFile) != 0) || (l_ci_Bytecount < 0))
            {   remove(HW_EVENT_LOCK_FILE);
                l_i_retStatus = HARDWARE_EVENT_LOCKFILE_WRITE_ERROR;
                break;
            }
        }
		#endif

        //Open GPIO14's value file
        g_fd_pwrStatus = open(SYSFS_GPIO14_VAL_FILE, O_RDONLY);
        if(g_fd_pwrStatus < 0)
        {   printf("ERROR!! Can't open :%s Ret:%d\n", SYSFS_GPIO14_VAL_FILE,g_fd_devNode);
            l_i_retStatus = HARDWARE_EVENT_FAIL;
            break;
        }

        //Open GPIO2's value file
        g_fd_intI2CStatus = open(SYSFS_GPIO2_VAL_FILE, O_RDONLY);
        if(g_fd_intI2CStatus < 0)
        {   printf("ERROR!! Can't open :%s \n", SYSFS_GPIO2_VAL_FILE);
            l_i_retStatus = HARDWARE_EVENT_FAIL;
            break;
        }

        //Open PCF857x Device node.
        g_fd_devNode = open(PCF8575DEVNODE, O_RDWR);
        if(g_fd_devNode < 0)
        {   l_i_retStatus = HARDWARE_EVENT_FAIL;
            break;
        }

        //start the PCF thread loop as soon as the thread is created
        g_b_stopThreads = false;

        //start the Power thread loop as soon as the thread is created
        g_b_stopPwrThread = false;

        //start the I2C Interrupt thread loop as soon as the thread is created
        g_b_stopI2CInitThread = false;

        /* Create the threads
        1. Thread block the call on read function.
        2. Thread will call the respected PCF callback function.
        3. Thread will monitor the GPIO14's value file
        */
        if(pthread_create(&readThread,NULL,&EventDataRead,NULL))
        {   printf("ERROR!! FAIL to create Event Read Thread\n");
            l_i_retStatus = HARDWARE_EVENT_FAIL;
            break;
        }
        pthread_detach(readThread);

        if(pthread_create(&PwrStatusThr,NULL,&PwrEventProcess,NULL))
        {   printf("ERROR!! FAIL to create Event Read Thread\n");
            l_i_retStatus = HARDWARE_EVENT_FAIL;
            break;
        }

        pthread_detach(PwrStatusThr);

        if(pthread_create(&gpio_i2cThread,NULL,&get_i2c_int_sts,NULL))
        {   printf("ERROR!! FAIL to create gpio i2c Thread\n");
            l_i_retStatus = HARDWARE_EVENT_FAIL;
            break;
        }
        pthread_detach(gpio_i2cThread);

        /**Assigned callback*/
        EventCallPtr = EventCB;
    }
    while(0);

    if(l_i_retStatus != HARDWARE_EVENT_SUCCESS)
    {   HardwareEventCallbackDeInit();

    }

    return l_i_retStatus;
}

/**
 * @brief This function closes common node and stops the threads.
 * @return no returns
 */
void HardwareEventCallbackDeInit(void)
{   //Stop PCF event threads
    g_b_stopThreads = true;

    //Stop PowerEvent thread
    g_b_stopPwrThread = true;

    //Stop I2C Interrupt thread
    g_b_stopI2CInitThread = true;

    //Close pcf device node
    if(g_fd_devNode > 0)
    {   close(g_fd_devNode);
        g_fd_devNode = -1;
        //Remove Hw Event Lock file
        //remove(HW_EVENT_LOCK_FILE);
    }

    //Close gpio14 device node
    if(g_fd_pwrStatus > 0)
    {   close(g_fd_pwrStatus);
        g_fd_pwrStatus = -1;
    }

    //Close gpio2 device node
    if(g_fd_intI2CStatus > 0)
    {   close(g_fd_intI2CStatus);
        g_fd_intI2CStatus = -1;
    }

    /* Deinitialize Logger */
    deInitHWEventLogger();
    //Deregister callback
    EventCallPtr = NULL;

    return;
}
