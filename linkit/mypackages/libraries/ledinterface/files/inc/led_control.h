/**
 *  @file 	led_control.h
 *  @brief 	LED Control  API Library
 *
 *  	This file contains LED Control , Freescale setup
 *  	and  Freescale processor reset APIs declaration.
 *
 */

#ifndef LEDCONTROL_H_
#define LEDCONTROL_H_

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdbool.h>

/* mraa header */
#include "mraa/i2c.h"
#include "mraa/gpio.h"
#include "calc_crc8.h"
/* Success fail error */
#define LED_SUCCESS 0
#define LED_ERROR   1

#define FSL_RST_SUCCESS 0
#define FSL_RST_ERROR 	1

/**
 * LED ID Numbers
 */

typedef enum { LED_ID_ALM=0x01, LED_ID_WIFI=0x02, LED_ID_PWR=0x04,
               LED_ID_BAT=0x08, LED_ID_ALL=0x0F
             } E_LEDID;

/**
 * LED Errors
 */
#define LED_INIT_ERROR                          1801
#define LED_SET_ADDR_ERROR                      1802
#define LED_NULL_POINTER_ERROR                  1803
#define LED_MALLOC_NULL_POINTER_ERROR           1804
#define LED_INVALID_ID_ERROR                    1805
#define LED_READ_RESP_ERROR                     1806
#define LED_RED_COLORVAL_OUTOF_RANGE_ERROR      1807
#define LED_GREEN_COLORVAL_OUTOF_RANGE_ERROR    1808
#define LED_BLUE_COLORVAL_OUTOF_RANGE_ERROR     1809
#define LED_WRITE_ERROR                         1810
#define LED_FREQUENCY_ERROR                     1811
#define LED_READ_ERROR                          1812
#define LED_NACK_ERROR                          1813


/**
 *
 * LED color value
 */
#define LED_MIN_COLOR_VALUE  0
#define LED_MAX_COLOR_VALUE  255

/**
* FSL Setup Errors
*/
#define FSL_INIT_ERROR                  1871

/**
 * @brief This function Initialize  LED related global i2c context and default values
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int LEDInit(void);

/**
 * @brief This function DeInitialize  LED related global i2c context and default values
 * @return no returns
 */
void LEDDeInit(void);



/**
 * @brief This functions Enables the LED Controller
 * @param i_b_state LED State. Pass 1 to emable and 0 to disable.
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int LEDEnableController(bool i_b_state);

/**
 * @brief This function sets the LED Brightness
 * @param i_e_ledId        LED ID [Valid values: as per E_LEDID enum]
 * @param i_ui8_colorRed   Read color value    [Valid range: 0-255]
 * @param i_ui8_colorGreen Green color value   [Valid range: 0-255]
 * @param i_ui8_colorBlue  Blue color value    [Valid range: 0-255]
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int LEDBrightness(E_LEDID i_e_ledId, uint8_t i_ui8_colorRed,
                  uint8_t i_ui8_colorGreen, uint8_t i_ui8_colorBlue);

/**
 * @brief This function toggle reset pin of the Freescale processor
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int ResetPowerContolLogic(void);

/**
 * @brief This function make iic slave enter to the correct mode
 *  only intended to be used in initialization sequence during boot up, applications are not expected to use it.
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int FreescaleSetup(void);

/**
 * @brief   This function enable or disable automatic mirroring for wifi LEDs
 * @param   i_b_value Input TRUE or FALSE as a bool
 * @return  On success, returns FSCALE_SUCCESS, error code otherwise
 *
 */
int LEDWifiMirroring(bool i_b_value);
/**
 * @brief This function is used for led flags control ex: stop led animation
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int SetLedControlFlag(uint8_t flag);
/**
 * @brief This function runs default led animation.
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int RunDefaultLedAnimation();
/**
 * @brief This function runs default led animation by Amrit its a dummy code only.
 * @return On success, returns EXIT_SUCCESS, error code otherwise
 */
int RunDefaultLedAnimation2();

#endif /* LEDCONTROL_H_ */
