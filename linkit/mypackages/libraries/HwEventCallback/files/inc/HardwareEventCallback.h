/**
 *  @file 	HardwareEventCallback.h
 *  @brief 	Event callback API Library
 *
 *  This header file contains Event callback  APIs declarations
 *
 */

#ifndef HARDWARE_EVENT_CALLBACK_H
#define HARDWARE_EVENT_CALLBACK_H

/** HARDWARE EVENTS */
/** Each bit represent one event */
typedef enum
{   NO_EVENT					= 0x00,
    EVENT_MF_BUTTON_PRESSED 	= 0x001,//This is for purely custom purpose as per additional application need.
    EVENT_SFP_IN 				= 0x002,
    EVENT_SFP_OUT				= 0x004,
    EVENT_PWR_BTN_PRESSED       = 0x008,
    EVENT_PWR_BTN_RELEASED      = 0x010,
    EVENT_SFP_ADP_IN            = 0x020,
    EVENT_SFP_ADP_OUT           = 0x040,
    EVENT_HD_IN                 = 0x080,
    EVENT_HD_OUT                = 0x100,
    MAX_EVENT_NUMBER 			= 0x200 //DEFINE the max number of event carefully to iterate bit checking loop necessary no. of times

} E_EVENTS_TYPE;

/** HARDWARE EVENT STATUS CODE */
#define HARDWARE_EVENT_SUCCESS	0
#define HARDWARE_EVENT_FAIL		1
#define HARDWARE_EVENT_REINIT_ERROR 3000
#define HARDWARE_EVENT_LOCKFILE_OPEN_ERROR 3001
#define HARDWARE_EVENT_LOCKFILE_REMOVE_ERROR 3002
#define HARDWARE_EVENT_LOCKFILE_WRITE_ERROR   3003


/**
 * @brief This function opens common node and starts the threads to monitor events.
 * @return On success, returns HARDWARE_EVENT_SUCCESS, error code otherwise.
*/
int HardwareEventCallbackInit(void (*EventCB)(E_EVENTS_TYPE i_e_eventName));

/**
 * @brief This function closes common node and stops the threads.
 *        It clears the registered callbacks.
 */
void HardwareEventCallbackDeInit(void);


/**
 * @brief This function will enable debug logs for HW events.
 */
void enableHwEventDebugLog(bool flag);
#endif
