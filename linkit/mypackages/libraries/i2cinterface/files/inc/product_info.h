/**************************************************************************************************
 *  @file   product_info.h
 *  @brief  Product information (H/W Version and Inventory) APIs
 *
 *  This header file contains product information API declaration.
 *
**************************************************************************************************/

#ifndef PRODUCT_INFO_H
#define PRODUCT_INFO_H

/* Standard Header */
#include <stdio.h>

/* Response Codes */
#define PRODUCT_INFO_SUCCESS        0
#define PRODUCT_INFO_FAIL           1201

/*******************************************************************************************************
 * @brief This function returns product information
 *
 * @param i_c_info A string containing data in JSON format of hardware revision and firmware version.
 *
 * @return  On success returns 0 and error code otherwise.
*******************************************************************************************************/
int ProductInfo(char *i_c_info);
#endif
