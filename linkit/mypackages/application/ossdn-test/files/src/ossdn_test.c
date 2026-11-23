/**
* @file ossdn_test.c
*
* @brief Test application for OTDR, OCMtest
*
*/

#include <ocm_interface.h>
#include <otdr_interface.h>
#include <power_management.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include <stdbool.h>
#include <occ_interface.h>
#include <occ_interface_internal.h>
#include <sfp_interface.h>
#include <sfp_fw_upgrade.h>
#include <led_control.h>
#include <freescale_interface.h>
#include <battery_management.h>
#include <common_i2c_operation.h>
#include <sys/time.h>
#include <HardwareEventCallback.h>
#include "mraa/i2c.h"
#include <sys/wait.h>
//#include <charger.h>
#include <osa_calibration.h>

/* Enable MT_TEST for multi-thread test */
//#define MT_THREAD_COUNT 1
//#define MT_TEST
#define ADD_DELAY

/* OTDR Communication Mode */
#define OTDR_NORMAL_MODE	0
#define OTDR_FAST_MODE		1

/* OTDR Dump trace msg */
#define OTDR_DUMP_MSG_DISABLE 0
#define OTDR_DUMP_MSG_ENABLE  1

/* OTDR trace Function Call */
#define OTDR_TRACE_FUNC_ONLY 1
#define OTDR_ALL_FUNC        0

/* Hw event check macros */
#define GETNEXTEVENTTYPE(x) (x << 1)
#define EVENT(x,y)	(x & y)
#define CHANGESBYTE(x,y)	((x >> y) & 0x01)

#define HARDWARE_REVISION_1_1 "/tmp/hwrev1.1"
#define HARDWARE_REVISION_1_0 "/tmp/hwrev1.0"
#define OTDR_FW_HWREV1        "/root/OTDR/hwrev1"
#define OTDR_FW_HWREV11       "/root/OTDR/hwrev1.1"

void PwrMcuOp(void);
int power_off_otdr(bool flag);
int power_off_all_Ins();
int FslVerCmd(char *o_c_ver);
int FslPackage(char *o_c_package);
int power_off_sfp();
int enable_op_sw(void);
int initialProductInfo(char *info);
int ProductInfo(char *o_c_info);
int FslSlaveCheckCmd();
int OTDRPowerForce(E_POWERSWITCH i_e_switch);
 
/* Hw event global declarations */
struct timeval  g_x_startTime, g_x_endTime;
int g_i_pwrStopTimer;
bool g_b_addDelay = 1;

int device_deinit_flag = 0;    // Use for deinit in signal_handler
int option = 0;                // Use for OTDR Communication mode
int otdrMsgVal=0;              // Use for OTDR dump msg Print
int otdrFuncSelect = 0;              // Use for OTDR Function Selection

/* Supporting function to convert response from OCM APIs */
float q8_converter(uint16_t power)
{   float result = 0;
    int j = 1;
    uint8_t first_byte = power & 0x00FF;
    uint8_t last_byte = (power >> 8) & 0x00FF;
    for(int i = 7; i >= 0; i--)
    {   result += (float)((first_byte >> i) & 0x01) /(pow(2,j++));
    }

    if((last_byte >> 7) & 0x01)
    {   result =  last_byte - 256 + result;
    }
    else
    {   result = last_byte + result;
    }

    return result;
}

/* This fucntion handles required deinit in case of app termination */
void signal_handler(int32_t i_i32_SigNum)
{   int ret_fn = 0;
    if (i_i32_SigNum)
    {   printf("OSSDN Test Application stopped by signal %d <%d>\n",i_i32_SigNum, device_deinit_flag);
    }
    if(device_deinit_flag == 2)
    {   if (0 != (ret_fn = OcmInterfaceDeInit()))
            printf("ERROR: ocm DeInit Failed \n");
    }
    else if(device_deinit_flag == 3)
    {   if (0 != (ret_fn = OtdrInterfaceDeInit()))
            printf("ERROR: Otdr DeInit Failed \n");
    }
    else if(device_deinit_flag == 4)
    {   if (0 != (ret_fn = OtdrInterfaceDeInit()))
            printf("ERROR: Otdr DeInit Failed \n");
        if (0 != (ret_fn = OcmInterfaceDeInit()))
            printf("ERROR: ocm DeInit Failed \n");
        if (0 != (ret_fn = SFPInterfaceDeInit()))
            printf("ERROR: SFP DeInit Failed \n");
    }
    else if(device_deinit_flag == 5)
    {   if (0 != (ret_fn = SFPInterfaceDeInit()))
            printf("ERROR: SFP DeInit Failed \n");
    }
    else if(device_deinit_flag == 6)
    {   OCCInterfaceDeInit();
    }
    else if(device_deinit_flag == 7)
    {   LEDDeInit();
    }
    else if(device_deinit_flag == 8)
    {   HardwareEventCallbackDeInit();
    }
    else if (device_deinit_flag == 9)
    {   if (0 != BatteryAPIDeInit())
            printf("ERROR: BATTERY DeInit Failed \n");
    }
    else if (device_deinit_flag == 10)
    {   if (0 != BatteryAPIDeInit())
            printf("ERROR: BATTERY DeInit Failed \n");
        else
            printf("BATTERY DeInit Success \n");

        LEDDeInit();
        printf("LED DeInit Success \n");

        if (0 != (ret_fn = SFPInterfaceDeInit()))
            printf("ERROR: SFP DeInit Failed \n");
        else
            printf("SFP DeInit Success \n");
    }
    else
    {   printf("ERROR: Signal Handling Failed!!!\n");
    }
    exit(1);
}

/**************************************************************************************************
 *
 * @brief   This function remove alphabets from string.
 *
 * @param   str input/output string
 *
 * @return  On success, returns removed alphabets string.
 *
 **************************************************************************************************/
char* removeAlphabets(char * str)
{   int i, j;
    for(i = 0; str[i] != '\0'; ++i)
    {   while (!( (str[i] >= '0' && str[i] <= '9') || (str[i] == '.') || str[i] == '\0') )
        {   for(j = i; str[j] != '\0'; ++j)
            {   str[j] = str[j+1];
            }
            str[j] = '\0';
        }
    }
    return str;
}

/**************************************************************************************************
 *
 * @brief   This function creates a /tmp/hwrev1.1 flag file.
 *
 **************************************************************************************************/
void createHardRevFlag(float i_f_hardRev, int8_t i_i8_RevChk)
{   FILE *fp = NULL;
    switch(i_i8_RevChk)
    {   case 1:
            fp = fopen(HARDWARE_REVISION_1_1, "w");
            break;
        case 2:
            fp = fopen(HARDWARE_REVISION_1_0, "w");
            break;
    }

    if(fp)
    {   fprintf(fp,"%f", i_f_hardRev);
        fclose(fp);
    }
}

/**************************************************************************************************
 *
 * @brief   This function read hardware revision from EEPORM and create a flag.
 *
 **************************************************************************************************/
void checkHardwareRevision()
{   int l_i_status = 0;
    uint32_t l_ui_checkByte = 0;
    uint8_t i_c_hardRev[17] = {0};
    float l_f_hardRev = 0;
    float value = 0;

    if( access(HARDWARE_REVISION_1_1, F_OK ) == 0 )
    {   // file exists
        if (remove(HARDWARE_REVISION_1_1) == 0)
        {   printf("The file %s is deleted successfully.\n", HARDWARE_REVISION_1_1);
        }
        else
        {   printf("The file %s is not deleted.", HARDWARE_REVISION_1_1);
        }
    }
    else if( access(HARDWARE_REVISION_1_0, F_OK ) == 0 )
    {   // file exists
        if (remove(HARDWARE_REVISION_1_0) == 0)
        {   printf("The file %s is deleted successfully.\n", HARDWARE_REVISION_1_0);
        }
        else
        {   printf("The file %s is not deleted.", HARDWARE_REVISION_1_0);
        }
    }

    l_i_status = getEEPROMHardRev(i_c_hardRev);
    if (l_i_status)
    {   printf("EEPROM Hardware Revision Failed. Return Value: %d\n", l_i_status);
    }
    else
    {   printf("Hardware Revision: %s\n", i_c_hardRev);
        if(i_c_hardRev[0] == 0xff || i_c_hardRev[0] == 'P')
        {   createHardRevFlag(value, 1);
        }
        else
        {   value = atof(removeAlphabets(i_c_hardRev));
            printf("Reivision: %f\n", value);
            if (value == 1.1f)
            {   createHardRevFlag(value, 1);
            }
            else if (value == 1.0f)
            {   createHardRevFlag(value, 2);
            }
        }
    }
}

/**************************************************************************************************
 *
 * @brief   This function read APD Vendor details from EEPORM .
 *
 **************************************************************************************************/
void checkAPDVendor()
{   int l_i_status = 0;
    uint8_t i_c_apdVendor[17] = {0};
    l_i_status = getEEPROMApdVendor(i_c_apdVendor);
    if (l_i_status)
    {   printf("EEPROM APD Vendor details Failed. Return Value: %d\n", l_i_status);
    }
    else
    {   
        int num = atoi(i_c_apdVendor);
        char *APD_COMP_VEN[] = {"Juyi", "San-U", "Eugenlight"}; //component vendors list
        char *APD_CHIP_VEN [] = {"D-tech", "Gofoton", "CETC"};//chip vendors list
        switch (num)
        {
        case 1:
            printf("APD component vendor : %s\n",APD_COMP_VEN[0]);
            printf("APD chip vendor : %s\n",APD_CHIP_VEN[0]);
            break;
        case 2:
            printf("APD component vendor : %s\n",APD_COMP_VEN[0]);
            printf("APD chip vendor : %s\n",APD_CHIP_VEN[1]);
            break;
        case 3:
            printf("APD component vendor : %s\n",APD_COMP_VEN[0]);
            printf("APD chip vendor : %s\n",APD_CHIP_VEN[2]);
            break;
        case 4:
            printf("APD component vendor : %s\n",APD_COMP_VEN[1]);
            printf("APD chip vendor : %s\n",APD_CHIP_VEN[0]);
            break;
        case 5:
            printf("APD component vendor : %s\n",APD_COMP_VEN[1]);
            printf("APD chip vendor : %s\n",APD_CHIP_VEN[1]);
            break;
        case 6:
            printf("APD component vendor : %s\n",APD_COMP_VEN[1]);
            printf("APD chip vendor : %s\n",APD_CHIP_VEN[2]);
            break;
        case 7:
            printf("APD component vendor : %s\n",APD_COMP_VEN[2]);
            printf("APD chip vendor : %s\n",APD_CHIP_VEN[0]);
            break;
        case 8:
            printf("APD component vendor : %s\n",APD_COMP_VEN[2]);
            printf("APD chip vendor : %s\n",APD_CHIP_VEN[1]);
            break;
        case 9:
            printf("APD component vendor : %s\n",APD_COMP_VEN[2]);
            printf("APD chip vendor : %s\n",APD_CHIP_VEN[2]);
            break;
        
        default:
            printf("Apd Details not provided for earlier releases\n");
            break;
        }
    }
}

/**************************************************************************************************
 *
 * @brief   This function reads OTDR firmware/hardware version as per given file extension.
 *
 * @param   fileName Input file name
 * @param   ext input file extension
 * @param   version output version string
 *
 * @return  On success, returns OTDR firmware-/hardware version string.
 *
 **************************************************************************************************/
int getStringBetweenPatterns(char fileName[], int ext, char *version)
{   char PATTERN1[32] = {0};
    char PATTERN2[32] = {0};

    if (ext == 1) // for OHW
    {   snprintf((char *) PATTERN2, 8, "-IIVI");
    }
    else // for BIN
    {   snprintf((char *) PATTERN1, 8, ".X");
        snprintf((char *) PATTERN2, 8, " ");
    }

    char *target = NULL;
    char *start, *end;

    if ( start = strstr( fileName, PATTERN1 ) )
    {   start += strlen( PATTERN1 );
        if ( end = strstr( start, PATTERN2 ) )
        {   target = ( char * )malloc( end - start + 1 );
            memcpy( target, start, end - start );
            target[end - start] = '\0';
        }
    }

    if ( target )
    {   snprintf(version, 32, "%s.", target);
    }

    free( target );

    return 0;
}

/**************************************************************************************************
 *
 * @brief   This function reads OTDR firmware/hardware file name as per given file extension.
 *
 * @param   ptr output file name
 * @param   ext input file extension
 * @param   hwrev input hardware revision
 *
 * @return  On success, returns OTDR version string.
 *
 **************************************************************************************************/
int getOTDRFWFileName(char *ptr, char *ext, int hwrev)
{   int l_i_status = 2;
    DIR *d;
    struct dirent *dir;
    int i = 0;
    char fwPath[32] = {0};
    if (hwrev == 2)
    {   snprintf(fwPath, 31, "%s", OTDR_FW_HWREV11);
    }
    else
    {   snprintf(fwPath, 31, "%s", OTDR_FW_HWREV1);
    }

    d = opendir(fwPath);
    if (d)
    {   while ((dir = readdir(d)) != NULL)
        {   //printf("%s\n", dir->d_name);
            if (strstr(dir->d_name, ext) != NULL)
            {   snprintf(ptr, 64, "%s/%s", fwPath, dir->d_name);
                l_i_status = 0;
            }
        }
        closedir(d);
    }
    else
    {   l_i_status = 1;
    }
    return l_i_status;
}

/**************************************************************************************************
 *
 * @brief   This function upgrades OTDR firmware/hardware at boot time.
 *
 **************************************************************************************************/
void _boot_otdr_fw()
{   int ret_fn = -1;
    do
    {   /** OTDR Initialization **/
        if ((ret_fn = OtdrInterfaceInit(OTDR_NORMAL_MODE)) != 0)
        {   printf("ERROR: otdr Init Error: %d\n", ret_fn);
            break;
        }
        else
        {   printf("OTDR Initialized Successfully.\n");
        }

        char i_c_OHWfilename[128] = {0};
        char i_c_BINfilename[128] = {0};
        char l_c_OHW_ext[8] = "ohw";
        char l_c_BIN_ext[8] = "bin";
        int isOHWFound = 0;
        int isBINFound = 0;

        /* obtain the file name (OHW, BIN) based on the hardware revision */
        if( access(HARDWARE_REVISION_1_1, F_OK ) == 0 )
        {   printf("Hardware revision 1.1 found\n");
            ret_fn = getOTDRFWFileName(i_c_OHWfilename, l_c_OHW_ext, 2);
            if (ret_fn == 0)
            {   isOHWFound = 1;
            }
            else if (ret_fn == 1)
            {   printf("OTDR firmware directory not found\n");
            }
            else if (ret_fn == 2)
            {   printf("OTDR firmware OHW file not found\n");
            }

            ret_fn = getOTDRFWFileName(i_c_BINfilename, l_c_BIN_ext, 2);
            if (ret_fn == 0)
            {   isBINFound = 1;
            }
            else if (ret_fn == 1)
            {   printf("OTDR firmware directory not found\n");
            }
            else if (ret_fn == 2)
            {   printf("OTDR firmware BIN file not found\n");
            }
        }
        else
        {   printf("Hardware revision 1.1 doesn't found\n");
            ret_fn = getOTDRFWFileName(i_c_OHWfilename, l_c_OHW_ext, 1);
            if (ret_fn == 0)
            {   isOHWFound = 1;
            }
            else if (ret_fn == 1)
            {   printf("OTDR firmware directory not found\n");
            }
            else if (ret_fn == 2)
            {   printf("OTDR firmware OHW file not found\n");
            }
            ret_fn = getOTDRFWFileName(i_c_BINfilename, l_c_BIN_ext, 1);
            if (ret_fn == 0)
            {   isBINFound = 1;
            }
            else if (ret_fn == 1)
            {   printf("OTDR firmware directory not found\n");
            }
            else if (ret_fn == 2)
            {   printf("OTDR firmware BIN file not found\n");
            }
        }

        printf("OHW file name: %s\n", i_c_OHWfilename);
        printf("BIN file name: %s\n", i_c_BINfilename);

        T_VER_FULL ver_full = {0};
        int l_i_isOHW = 0;
        int l_i_isBIN = 0;
        int checkOTDRCorruption = 0;
        ret_fn = OtdrGetVerFull(&ver_full);
        if (ret_fn)
        {   if (OTDR_CORRUPTION_ERROR == ret_fn)
            {   l_i_isOHW = 1;
                checkOTDRCorruption = 1;
                printf("OTDR is corrupted:%d\n", ret_fn);
            }
            else
            {   printf("OtdrGetVerFull failed. Error:%d\n", ret_fn);
                break;
            }
        }

        printf("OtdrGetVerFull\n");
        printf("version :\t<%s>\n", ver_full.version);
        printf("designversion :\t<%s>\n", ver_full.designversion);

        if (isOHWFound == 1)
        {   char currentOHWversion[32] = {0};
            getStringBetweenPatterns(ver_full.designversion, 1, currentOHWversion);

            printf("currentOHWversion:%s\n", currentOHWversion);

            if ((strstr(i_c_OHWfilename, currentOHWversion) != NULL) && (OTDR_CORRUPTION_ERROR != ret_fn))
            {   printf("Same OHW Version\n");
            }
            else
            {   printf("Versions of OHW that are not the same. Starting the Upgrade. It will take a few minutes.\n");
                if ((ret_fn = OtdrFirmwareUpgrade(i_c_OHWfilename)) != 0)
                {   printf("ERROR: OtdrFirmwareUpgrade Error: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Firmware Upgraded Successfully\n");
                }
            }
        }
        else
        {   printf("OTDR Hardware version is empty or file not found\n");
        }

        if (isBINFound == 1)
        {   char currentBINversion[32] = {0};

            if (checkOTDRCorruption)
            {   ret_fn = OtdrGetVerFull(&ver_full);
                if (ret_fn)
                {   printf("OtdrGetVerFull failed. Error:%d\n", ret_fn);
                    break;
                }
                printf("OtdrGetVerFull\n");
                printf("version :\t<%s>\n", ver_full.version);
                printf("designversion :\t<%s>\n", ver_full.designversion);
            }

            getStringBetweenPatterns(ver_full.version, 2, currentBINversion);

            if ((strstr(i_c_BINfilename, currentBINversion) != NULL) && (NULL != ver_full.version))
            {   printf("Same BIN Version\n");
            }
            else
            {   printf("Versions of BIN that are not the same. Starting the Upgrade. It will take a few minutes.\n");
                if ((ret_fn = OtdrFirmwareUpgrade(i_c_BINfilename)) != 0)
                {   printf("ERROR: OtdrFirmwareUpgrade Error: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Firmware Upgraded Successfully\n");
                }
            }
        }
        else
        {   printf("OTDR version is empty or file not found\n");
        }
    }
    while(0);

    /** Otdr De-Initialization **/
    if(ret_fn != OTDR_INIT_FAILED)
    {   if (0 != (ret_fn = OtdrInterfaceDeInit()))
        {   printf("ERROR: Otdr DeInit Failed \n");
        }
    }
}

/* This function calls OTDR Interface Library APIs */
void _otdr_fw(char *i_c_filename)
{   int ret_fn = -1;
    do
    {   system("rm -f /tmp/ttyS0*"); /* Remove serial port lock files before initializing the otdr */
		/** OTDR Initialization **/
        if ((ret_fn = OtdrInterfaceInit(OTDR_NORMAL_MODE)) != 0)
        {   printf("ERROR: otdr Init Error: %d\n", ret_fn);
            break;
        }
        else
        {   printf("OTDR Initialized Successfully.\n");
        }
#ifdef ADD_DELAY
        usleep(10000);
#endif
		printf("Please do not close the application...!\nOTDR firmware upgrade is in progresss...!\n");
        if ((ret_fn = OtdrFirmwareUpgrade(i_c_filename)) != 0)
        {   printf("ERROR: OtdrFirmwareUpgrade Error: %d\n", ret_fn);
            system("rm -f /tmp/lock/otdr_fw_upgrade.lock");
            system("ossdn-test led fw_upgrade_fail &");
            system("/etc/init.d/xmfhwxx-boot-daemon start");
            system("/etc/init.d/xmfhwss-daemon start");
            break;
        }
        else
        {   printf("OTDR Firmware Upgraded Successfully\n");
            system("reboot -f &");
        }
#ifdef ADD_DELAY
        usleep(10000);
#endif
    }
    while(0);
    /** Otdr De-Initialization **/
    if(ret_fn != OTDR_INIT_FAILED)
    {   if (0 != (ret_fn = OtdrInterfaceDeInit()))
        {   printf("ERROR: Otdr DeInit Failed \n");
        }
    }
}

/* This function calls OCM Interface Library APIs */
void _ocm_fw(char *i_c_filename)
{   int 		ret_fn = -1;
    int choice = 0;
    do
    {   /** OCM Initialization **/
        if ((ret_fn = OcmInterfaceInit()) != 0)
        {   printf("ERROR: OcmIniterfaceInit Error: %d\n", ret_fn);
            break;
        }
        else
        {   printf("OCM Initialized Successfully\n");
        }
#ifdef ADD_DELAY
        usleep(10000);
#endif
        if ((ret_fn = OcmFirmwareUpgrade(i_c_filename)) != 0)
        {   printf("ERROR: OcmFirmwareUpgrade Error: %d\n", ret_fn);
            break;
        }
        else
        {   printf("OCM Firmware Upgraded Successfully\n");
        }
#ifdef ADD_DELAY
        usleep(10000);
#endif
    }
    while(0);
    /** Ocm De-Initialization **/
    if (ret_fn != OCM_INIT_ERROR)
    {   if (0 != (ret_fn=OcmInterfaceDeInit()))
        {   printf("ERROR: Ocm DeInit Failed %d \n", ret_fn);
        }
    }
}

/* This function calls OCM Interface Library APIs */
void _ocm(int count, int apiNo)
{   int 		ret_fn = -1;
    int 		times = 0;
    char 		idCode[12] = {0};
    char 		mfd[12] = {0};
    int 		buildNumber = 0;
    uint16_t 	l_u16_strsize = 0;
    int 		l_u16_resLen=0;
    uint32_t 	channalPlan = 0;
    uint32_t    l_ui32_StatusWord = 0;
    float 		l_f_startFreq = 0;
    do
    {   /** OCM Initialization **/
        if ((ret_fn = OcmInterfaceInit()) != 0)
        {   printf("ERROR: OcmIniterfaceInit Error: %d\n", ret_fn);
            break;
        }
        else
        {   printf("OCM Initialized Successfully\n");
        }

#ifdef ADD_DELAY
        usleep(10000);
#endif

        while(--count >= 0)
        {   if (apiNo == 1 )
            {   /** OCM Get Serial Number API*/
                ret_fn = OcmGetSNUM(idCode,mfd,&buildNumber);
                if (!ret_fn)
                {   printf("Ocm. API:OcmGetSNUM. API Response: \n idCode : <%s>\n mfd : <%s>\n buildNumber : <%d>\n",idCode,mfd,buildNumber);
                }
                else
                {   printf("ERROR: OcmGetSNUM. Error no: %d\n", ret_fn);
                }

#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            if (apiNo == 2 )
            {   char * revision = NULL;
                revision = (char *)malloc(128*sizeof(char));
                ret_fn = OcmGetDREV(revision);
                if (!ret_fn)
                {   printf("Ocm. API:OcmGetDREV. API Response: <%s>\n", revision);
                }
                else
                {   printf("ERROR: OcmGetDREV. Error no: %d\n", ret_fn);
                }
                if(revision)
                {   free(revision);
                    revision = 0;
                }
#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            if (apiNo == 3 )
            {   channalPlan = 0;
                /** OCM Get CSSP API*/
                ret_fn = OcmGetCSSP(&channalPlan,&l_f_startFreq,&l_ui32_StatusWord);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   printf("Ocm. API:OcmGetCSSP. API Response: CSSP Channel: <%u> \t  "
                           "CSSP Freq: <%f> Status Word: %u\n", channalPlan,
                           l_f_startFreq, l_ui32_StatusWord);
                }
                else
                {   printf("ERROR: OcmGetCSSP. Error no: %d\n", ret_fn);
                }
                l_ui32_StatusWord = 0;

#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            if (apiNo == 4 )
            {   channalPlan = 1;
                l_f_startFreq = 0;
                /** OCM Set CSSP2 API*/
                ret_fn = OcmSetCSSP(&channalPlan,&l_f_startFreq, &l_ui32_StatusWord);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   printf("Ocm. API:OcmSetCSSP. API Response: CSSP Channel: <%u> \t  "
                           "CSSP Freq: <%f> Status Word:%u\n",  channalPlan,
                           l_f_startFreq);
                }
                else
                {   printf("ERROR: OcmSetCSSP. Error no: %d\n", ret_fn);
                }
                l_ui32_StatusWord = 0;
                sleep(2);
#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            /** OCM Get FwVr API*/
            char ocmGetFw[12] = {0};
            if (apiNo == 5 )
            {   ret_fn = OcmGetFWVR(ocmGetFw);
                if (!ret_fn)
                {   printf("Ocm. API:OcmGetFWVR. API Response: <%s>\n", ocmGetFw);
                }
                else
                {   printf("ERROR: OcmGetFWVR. Error no: %d\n", ret_fn);
                }

#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            /** OCM Get HwVr API*/
            char ocmGetHw[12] = {0};
            if (apiNo == 6 )
            {   ret_fn = OcmGetHWVR(ocmGetHw);
                if (!ret_fn)
                {   printf("Ocm. API:OcmGetHWVR. API Response: <%s>\n", ocmGetHw);
                }
                else
                {   printf("ERROR: OcmGetHWVR. Error no: %d\n", ret_fn);
                }

#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            /** OCM Get csta1 API*/
            T_CSTA0 l_x_csta0 = {0};
            if (apiNo == 7 )
            {   ret_fn = OcmGetCSTA0(&l_x_csta0);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   printf("Ocm. API:OcmGetCSTA0. API Response: Device Temp: <%.2lf> Status Word: <%u>, Error Status Word: %u\n", l_x_csta0.m_f_deviceTemperature,
                           l_x_csta0.m_ui32_statusWord, l_x_csta0.m_ui32_ChkErrorStatus);
                }
                else
                {   printf("ERROR: OcmGetCSTA0. Error no: %d\n", ret_fn);
                }
#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            /** OCM Get csta1 API*/
            float totPower = 0;
            if (apiNo == 8)
            {   ret_fn = OcmGetCTPR1(&totPower, &l_ui32_StatusWord);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   printf("Ocm. API:OcmGetCTPR1. API Response: totPower: <%f> Status Word:%u \n", totPower, l_ui32_StatusWord);
                }
                else
                {   printf("ERROR: OcmGetCTPR1. Error no: %d\n", ret_fn);
                }
                l_ui32_StatusWord = 0;
#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            /** OCM Get csta1 API*/
            T_TOFSCAN_MODE scanMode = 2;
            uint32_t scanSetting = 0;
            if (apiNo == 9)
            {   ret_fn = OcmDoCENB1(scanMode,&scanSetting, &l_ui32_StatusWord);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   printf("Ocm. API:OcmDoCENB1. API Response: scanMode: <%d> scanSetting : <%d> Status Word:%u \n", scanMode,scanSetting, l_ui32_StatusWord);
                }
                else
                {   printf("ERROR: OcmDoCENB1. Error no: %d\n", ret_fn);
                }
                l_ui32_StatusWord = 0;
#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            /** OCM Get csta1 API*/
            T_CFCR cfcr = {0};
            if (apiNo == 10 )
            {   ret_fn = OcmGetCFCR(&cfcr);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   printf("Ocm. API:OcmGetCFCR. API Response: \n");
                    printf("totalPower : <%f>\n",cfcr.m_totalPower);
                    printf("m_channels : <%d>\n",cfcr.m_channels);
                    printf("m_ui32_ChkErrorStatus : <%u>\n",cfcr.m_ui32_ChkErrorStatus);
                    for(int i = 0; i<3; i++)
                        printf("m_peakDetectionFlags[%d] : <%u>\n",i,cfcr.m_peakDetectionFlags[i]);
                    for(int i = 0; i<3; i++)
                        printf("m_peakTypeFlags[%d] : <%u>\n",i,cfcr.m_peakTypeFlags[i]);
                    for (int i = 0; i<cfcr.m_channels; i++)
                    {   printf("channelPowerITU : <%f>\t\t channelPowerPeak : <%f>\t\t channelFrequency : <%f>\n", q8_converter(cfcr.m_channel[i].channelPowerITU), q8_converter(cfcr.m_channel[i].channelPowerPeak),q8_converter(cfcr.m_channel[i].channelFrequency));
                    }
                }
                else
                {   printf("ERROR: OcmGetCFCR. Error no: %d\n", ret_fn);
                }
#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            uint32_t ctmStatus = 0;
            if (apiNo == 11 )
            {   ret_fn = OcmGetCSTS0(&ctmStatus);
                if (!ret_fn)
                {   printf("Ocm. API:OcmGetCSTS0. API Response: ctmStatus : <%u>\n",ctmStatus);
                }
                else
                {   printf("ERROR: OcmGetCSTS0. Error no: %d\n", ret_fn);
                }
            }
            /** OCM Get CGTP1 API*/
            T_CGTP1B l_x_cgtp1 = {0};
            T_INTEGRATE l_e_nm = 0;
            int l_i_count = 0;
            uint32_t l_ui32_peak[96] = {0};
            uint32_t l_ui32_sat[96] = {0};
            if (apiNo == 12)
            {   ret_fn = OcmGetCGTP1B(l_e_nm,&l_x_cgtp1);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((l_x_cgtp1.m_peakDetectionFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_peak[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_peak[i + 64] = 1;
                                }
                                else
                                {   l_ui32_peak[i] = 1;
                                }
                            }
                        }
                    }

                    for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((l_x_cgtp1.m_channelSaturationFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_sat[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_sat[i + 64] = 1;
                                }
                                else
                                {   l_ui32_sat[i] = 1;
                                }
                            }
                        }
                    }

                    printf("Ocm. API: OcmGetCGTP1%dB. API Response: Total Pow: <%f>\n", l_e_nm,l_x_cgtp1.m_totalPower);
                    for (int l_i_count = 0; l_i_count < l_x_cgtp1.m_channels;
                            l_i_count++)
                    {   printf("Freq: [%f], Pow: [%f], peak: [%d], sat: [%d]\n",
                               l_x_cgtp1.m_channel[l_i_count].channelFrequency,
                               l_x_cgtp1.m_channel[l_i_count].channelPower, l_ui32_peak[l_i_count], l_ui32_sat[l_i_count]);
                    }
                    printf("Status Word: %u\n", l_x_cgtp1.m_ui32_ChkErrorStatus);
                }
                else
                {   printf("ERROR: OcmGetCGTP1%dB. Error no: %d\n", l_e_nm,ret_fn);
                }

                l_e_nm = 1;
                l_i_count = 0;
                memset(l_ui32_peak,0,sizeof(l_ui32_peak));
                memset(l_ui32_sat,0,sizeof(l_ui32_sat));
                ret_fn = OcmGetCGTP1B(l_e_nm,&l_x_cgtp1);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((l_x_cgtp1.m_peakDetectionFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_peak[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_peak[i + 64] = 1;
                                }
                                else
                                {   l_ui32_peak[i] = 1;
                                }
                            }
                        }
                    }

                    for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((l_x_cgtp1.m_channelSaturationFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_sat[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_sat[i + 64] = 1;
                                }
                                else
                                {   l_ui32_sat[i] = 1;
                                }
                            }
                        }
                    }

                    printf("Ocm. API: OcmGetCGTP1%dB. API Response: Total Pow: <%f>\n", l_e_nm,l_x_cgtp1.m_totalPower);
                    for (int l_i_count = 0; l_i_count < l_x_cgtp1.m_channels;
                            l_i_count++)
                    {   printf("Freq: [%f], Pow: [%f], peak: [%d], sat: [%d]\n",
                               l_x_cgtp1.m_channel[l_i_count].channelFrequency,
                               l_x_cgtp1.m_channel[l_i_count].channelPower, l_ui32_peak[l_i_count], l_ui32_sat[l_i_count]);
                    }
                    printf("Status Word: %u\n", l_x_cgtp1.m_ui32_ChkErrorStatus);
                }
                else
                {   printf("ERROR: OcmGetCGTP1%dB. Error no: %d\n", l_e_nm,ret_fn);
                }

                l_e_nm = 2;
                l_i_count = 0;
                memset(l_ui32_peak,0,sizeof(l_ui32_peak));
                memset(l_ui32_sat,0,sizeof(l_ui32_sat));
                ret_fn = OcmGetCGTP1B(l_e_nm,&l_x_cgtp1);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((l_x_cgtp1.m_peakDetectionFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_peak[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_peak[i + 64] = 1;
                                }
                                else
                                {   l_ui32_peak[i] = 1;
                                }
                            }
                        }
                    }

                    for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((l_x_cgtp1.m_channelSaturationFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_sat[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_sat[i + 64] = 1;
                                }
                                else
                                {   l_ui32_sat[i] = 1;
                                }
                            }
                        }
                    }

                    printf("Ocm. API: OcmGetCGTP1%dB. API Response: Total Pow: <%f>\n", l_e_nm,l_x_cgtp1.m_totalPower);
                    for (int l_i_count = 0; l_i_count < l_x_cgtp1.m_channels;
                            l_i_count++)
                    {   printf("Freq: [%f], Pow: [%f], peak: [%d], sat: [%d]\n",
                               l_x_cgtp1.m_channel[l_i_count].channelFrequency,
                               l_x_cgtp1.m_channel[l_i_count].channelPower, l_ui32_peak[l_i_count], l_ui32_sat[l_i_count]);
                    }
                    printf("Status Word: %u\n", l_x_cgtp1.m_ui32_ChkErrorStatus);

                }
                else
                {   printf("ERROR: OcmGetCGTP1%dB. Error no: %d\n", l_e_nm,ret_fn);
                }
            }
            /** OCM Get cbdi API*/
            T_CBDI l_x_cbdi = {0};
            if (apiNo == 13)
            {   ret_fn = OcmGetCBDI(&l_x_cbdi);
                if (!ret_fn)
                {   printf("Ocm. API: OcmGetCBDI API Response: \nCTM Model No : %u\n"
                           "PCB Serial Number : %d\n"
                           "CTM Revision : %u\n"
                           "CTM Firmware Version : %u\n"
                           "CTM data Code : %d\n"
                           "tof Serial Number : %d\n"
                           "tof Revision : %u\n"
                           "tof start Frequency : %f THz\n"
                           "tof end Frequency : %f THz\n"
                           "tof Insertion loss : %f\n"
                           "tof peak detection thresold : %f\n"
                           "tof apllied SBOC : %f GHz\n"
                           "tof Applied SBTC : %f GHz\n"
                           "tof Minimum Power: %f\n"
                           "tof Reserved1 : %f\n"
                           "Reserved 1 : %f\n"
                           "Reserved 2 : %f\n"
                           "CTM Vendor : %s\n"
                           "CTM CTM Serial Number : %s\n"
                           "crc16 : %02x\n", l_x_cbdi.m_ui32_CTMModelNumber,
                           l_x_cbdi.m_i32_PCBSerialNumber, l_x_cbdi.m_ui32_CTMRevision,
                           l_x_cbdi.m_ui32_CTMFirmwareVersion, l_x_cbdi.m_ui32_CTMDataCode,
                           l_x_cbdi.m_ui32_tofSerialNumber, l_x_cbdi.m_ui32_tofRevision,
                           l_x_cbdi.m_f_tofStartFrequency, l_x_cbdi.m_f_tofEndFrequency,
                           l_x_cbdi.m_f_tofInsertionLoss,
                           l_x_cbdi.m_f_tofPeakDetectionThresold,
                           l_x_cbdi.m_f_tofAppliedSBOC, l_x_cbdi.m_f_tofAppliedSBTC,
                           l_x_cbdi.m_f_tofMinimumPower, l_x_cbdi.m_f_tofReserved1,
                           l_x_cbdi.m_f_reserved1, l_x_cbdi.m_f_reserved2,
                           l_x_cbdi.m_aui8_CTMVendor, l_x_cbdi.m_aui8_CTMSerialNumber,
                           l_x_cbdi.m_ui16_crc16);
                }
                else
                    printf("ERROR: OcmGetCBDI. Error no: %d\n", ret_fn);

#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            T_CDDFB cddfb = {0};
            l_i_count = 0;
            uint32_t l_ui32_peakDet[96] = {0};
            uint32_t l_ui32_peakFlg[96] = {0};
            uint32_t l_ui32_saturation[96] = {0};
            if (apiNo == 14)
            {   ret_fn = OcmGetCDDFB(&cddfb);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((cddfb.m_peakDetectionFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_peakDet[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_peakDet[i + 64] = 1;
                                }
                                else
                                {   l_ui32_peakDet[i] = 1;
                                }
                            }
                        }
                    }

                    for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((cddfb.m_peakTypeFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_peakFlg[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_peakFlg[i + 64] = 1;
                                }
                                else
                                {   l_ui32_peakFlg[i] = 1;
                                }
                            }
                        }
                    }

                    for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((cddfb.m_channelSaturationFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_saturation[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_saturation[i + 64] = 1;
                                }
                                else
                                {   l_ui32_saturation[i] = 1;
                                }
                            }
                        }
                    }

                    printf("Ocm. API: OcmGetCDDFB. API Response: \n");
                    printf("m_totalPower : <%f>\n",cddfb.m_totalPower);
                    printf("m_channels : <%d>\n",cddfb.m_channels);
                    printf("m_ui32_ChkErrorStatus : <%u>\n",cddfb.m_ui32_ChkErrorStatus);
                    for (int l_i_count = 0; l_i_count < cddfb.m_channels;
                            l_i_count++)
                    {   printf("Freq: [%f]\t,pwr2nm: [%u]\t, pwr4nm: [%u]\t, PeakDetct: [%d]\t, peakTyp: [%d]\t, SatFlg: [%d]\n",
                               cddfb.m_channel[l_i_count].channelFrequency,
                               cddfb.m_channel[l_i_count].channelPower2nm,cddfb.m_channel[l_i_count].channelPower4nm, l_ui32_peakDet[l_i_count],l_ui32_peakFlg[l_i_count],l_ui32_saturation[l_i_count]);
                    }
                }
                else
                {   printf("ERROR: OcmGetCDDFB. Error no: %d\n",ret_fn);
                }
            }

            /** OCM Get CGET1 API*/
            T_CGET1 l_x_cget1 = {0};
            T_INTEGRATE l_e_nm1 = 0;
            l_i_count  = 0;
            uint32_t l_ui32_peak_Det[96] = {0};
            if (apiNo == 15)
            {   ret_fn = OcmGetCGET1(l_e_nm1,&l_x_cget1);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((l_x_cget1.m_peakDetectionFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_peak_Det[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_peak_Det[i + 64] = 1;
                                }
                                else
                                {   l_ui32_peak_Det[i] = 1;
                                }
                            }
                        }
                    }

                    printf("Ocm. API: OcmGetCGET1%d. API Response: \n", l_e_nm1);
                    for (int l_i_count = 0; l_i_count < l_x_cget1.m_channels;
                            l_i_count++)
                    {   printf("Freq: [%f], Pow: [%f], peak: [%d]\n",
                               l_x_cget1.m_channel[l_i_count].channelFrequency,
                               l_x_cget1.m_channel[l_i_count].channelPower, l_ui32_peak_Det[l_i_count]);
                    }
                    printf("Status Word: %u\n", l_x_cget1.m_ui32_ChkErrorStatus);
                }
                else
                {   printf("ERROR: OcmGetCGET1%d. Error no: %d\n", l_e_nm1,ret_fn);
                }

                l_e_nm1 = 1;
                l_i_count  = 0;
                memset(l_ui32_peak_Det,0,sizeof(l_ui32_peak_Det));
                ret_fn = OcmGetCGET1(l_e_nm1,&l_x_cget1);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((l_x_cget1.m_peakDetectionFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_peak_Det[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_peak_Det[i + 64] = 1;
                                }
                                else
                                {   l_ui32_peak_Det[i] = 1;
                                }
                            }
                        }
                    }

                    printf("Ocm. API: OcmGetCGET1%d. API Response: \n", l_e_nm1);
                    for (int l_i_count = 0; l_i_count < l_x_cget1.m_channels;
                            l_i_count++)
                    {   printf("Freq: [%f], Pow: [%f], peak: [%d]\n",
                               l_x_cget1.m_channel[l_i_count].channelFrequency,
                               l_x_cget1.m_channel[l_i_count].channelPower, l_ui32_peak_Det[l_i_count]);
                    }
                    printf("Status Word: %u\n", l_x_cget1.m_ui32_ChkErrorStatus);
                }
                else
                {   printf("ERROR: OcmGetCGET1%d. Error no: %d\n", l_e_nm1,ret_fn);
                }

                l_e_nm1 = 2;
                l_i_count  = 0;
                memset(l_ui32_peak_Det,0,sizeof(l_ui32_peak_Det));
                ret_fn = OcmGetCGET1(l_e_nm1,&l_x_cget1);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   for (l_i_count = 0; l_i_count < 3; l_i_count++)
                    {   for (int i = 31; i >= 0; i--)
                        {   if ((l_x_cget1.m_peakDetectionFlags[l_i_count] >> i) & 0x01)
                            {   if (l_i_count == 1)
                                {   l_ui32_peak_Det[i + 32] = 1;
                                }
                                else if (l_i_count == 2)
                                {   l_ui32_peak_Det[i + 64] = 1;
                                }
                                else
                                {   l_ui32_peak_Det[i] = 1;
                                }
                            }
                        }
                    }

                    printf("Ocm. API: OcmGetCGET1%d. API Response: \n", l_e_nm1);
                    for (int l_i_count = 0; l_i_count < l_x_cget1.m_channels;
                            l_i_count++)
                    {   printf("Freq: [%f], Pow: [%f], peak: [%d]\n",
                               l_x_cget1.m_channel[l_i_count].channelFrequency,
                               l_x_cget1.m_channel[l_i_count].channelPower, l_ui32_peak_Det[l_i_count]);
                    }
                    printf("Status Word: %u\n", l_x_cget1.m_ui32_ChkErrorStatus);
                }
                else
                {   printf("ERROR: OcmGetCGET1%d. Error no: %d\n", l_e_nm1,ret_fn);
                }
            }
            bool enableDemoMode = 0;
            if (apiNo == 16)
            {   /** OCM Set CDMO API*/
                ret_fn = OcmSetCDMO(enableDemoMode);
                if (!ret_fn)
                {   printf("Ocm. API:OcmSetCDMO. API Response: %d\n",enableDemoMode);
                }
                else
                {   printf("ERROR: OcmSetCDMO. Error no: %d\n", ret_fn);
                }
            }

            bool highGain = 1;
            if (apiNo == 17)
            {   /** OCM Set Gain API*/
                ret_fn = OcmSetGain(highGain);
                if (!ret_fn)
                {   printf("Ocm. API:OcmSetGain. API Response: %d\n",highGain);
                }
                else
                {   printf("ERROR: OcmSetGain. Error no: %d\n", ret_fn);
                }
            }

            float minimumPeakPower = 0.00002;
            if (apiNo == 18)
            {   /** OCM Set CMPP1 API*/
                ret_fn = OcmSetCMPP1(minimumPeakPower);
                if (!ret_fn)
                {   printf("Ocm. API:OcmSetCMPP1. API Response: %f\n",minimumPeakPower);
                }
                else
                {   printf("ERROR: OcmSetCMPP1. Error no: %d\n", ret_fn);
                }
            }

            float inserttionLoss = 2.0;
            if (apiNo == 19)
            {   /** OCM Set CSIL1 API*/
                ret_fn = OcmSetCSIL1(inserttionLoss);
                if (!ret_fn)
                {   printf("Ocm. API:OcmSetCSIL1. API Response: %f\n",inserttionLoss);
                }
                else
                {   printf("ERROR: OcmSetCSIL1. Error no: %d\n", ret_fn);
                }
            }

            float peakDetectThreshold = 0.002;
            if (apiNo == 20)
            {   /** OCM Set CMPP1 API*/
                ret_fn = OcmSetCSPT1(peakDetectThreshold);
                if (!ret_fn)
                {   printf("Ocm. API:OcmSetCSPT1. API Response: %f\n",peakDetectThreshold);
                }
                else
                {   printf("ERROR: OcmSetCSPT1. Error no: %d\n", ret_fn);
                }

#ifdef ADD_DELAY
                usleep(10000);
#endif
            }

            T_SLICE_WIDTH sliceWidth = 1;
            T_SPECTRUM_FORMAT spectrumFormat = 1;
            T_EDGE_BINS edgeBins = 3;
            T_CGSP1 cgsp1 = {0};
            ret_fn = 0;
            if (apiNo == 21 )
            {   ret_fn = OcmGetCGSP1(sliceWidth,spectrumFormat,edgeBins,&cgsp1);
                if (ret_fn == 0 || ret_fn == OCM_INTERNAL_STATUS_ERROR)
                {   printf("Ocm. API:OcmGetCGSP1. API Response(for CGSP1T3):\n");
                    printf("cgsp1->m_totalPower : <%f>\n",cgsp1.m_totalPower);
                    printf("cgsp1->m_slices : <%u>\n",cgsp1.m_slices);
                    printf("cgsp1->m_startFreq : <%f>\n",cgsp1.m_startFreq);
                    printf("cgsp1->m_sliceWidth : <%f>\n",cgsp1.m_sliceWidth);
                    printf("cgsp1->m_scanCounter : <%u>\n",cgsp1.m_scanCounter);
                    printf("cgsp1->m_ui32_ChkErrorStatus : <%u>\n",cgsp1.m_ui32_ChkErrorStatus);
                    for(int i = 0; i<cgsp1.m_slices; i++)
                    {   printf("slicePower[%d]: %f\n",i,cgsp1.m_slicePower[i]);
                    }

                }
                else
                {   printf("ERROR: OcmGetCGSP1T3. Error no: %d\n", ret_fn);
                }
                if(cgsp1.m_slicePower !=NULL)
                    free(cgsp1.m_slicePower);

            }

            sleep(1);
        }
    }
    while(0);
    /** Ocm De-Initialization **/
    if (ret_fn != OCM_INIT_ERROR)
    {   if (0 != (ret_fn=OcmInterfaceDeInit()))
        {   printf("ERROR: Ocm DeInit Failed %d \n", ret_fn);
        }
    }
}


/* This function calls OTDR Interface Library APIs */
void _otdr(int count, int apiNo)
{   double get_thr = -1;
    double thr = 0.30;
    int ret_fn = -1;
    int times = 0;
    int l_i_retry_count = 0;
    /** otdr Initialization **/
    if ((ret_fn = OtdrInterfaceInit(option)) != 0)
    {   printf("ERROR: otdr Init Error: %d\n", ret_fn);
        return;
    }
    else
    {   printf("OTDR Initialized Successfully.\n");
    }
    do
    {   T_OTDRSTATUS get_status= {0};
        if(!otdrFuncSelect)
        {

            if(g_b_addDelay)
                usleep(10000);


            T_VER ver = {0};
            if (apiNo == 1 )
            {   ret_fn = OtdrGetVer(&ver);
                if (ret_fn)
                {   printf("OtdrGetVer failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrGetVer\n");
                    printf("partnumber :\t<%s>\n", ver.partnumber);
                    printf("version :\t<%s>\n", ver.version);
                    printf("serialnumber :\t<%s>\n", ver.serialnumber);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            T_VER_FULL ver_full = {0};
            if (apiNo == 2 )
            {   ret_fn = OtdrGetVerFull(&ver_full);
                if (ret_fn)
                {   printf("OtdrGetVerFull failed. Error:%d\n", ret_fn);
                }
                else
                {
					printf("TEST APP: OtdrGetVerFull\n");
					printf("partnumber :\t<%s>\n", ver_full.partnumber);
					printf("version :\t<%s>\n", ver_full.version);
					printf("serialnumber :\t<%s>\n", ver_full.serialnumber);
					printf("designversion :\t<%s>\n", ver_full.designversion);
					printf("loader : \t<%s>\n", ver_full.loader);
					printf("spiPeripheral :<%s>\n", ver_full.spiPeripheral);
					printf("sysClock : \t<%s>\n", ver_full.sysClock);
					printf("schedulePeriod:<%s>\n", ver_full.schedulePeriod);
					printf("dataCache :\t<%s>\n", ver_full.dataCache);
					printf("instCache :\t<%s>\n", ver_full.instCache);
					printf("platform :\t<%d>\n", ver_full.platform);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            int mode = 0;
            if (apiNo == 3 )
            {   ret_fn = OtdrGetAstm(&mode);
                if (ret_fn)
                {   printf("OtdrGetAstm failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrGetAstm : <%d>\n", mode);
                }
                printf("\n");

                if(g_b_addDelay)
                    usleep(10000);
            }

            T_OTDRRESULT result = {0};
            if (apiNo == 4)
            {   ret_fn = OtdrResult(&result);
                if (ret_fn)
                {   printf("OtdrResult failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrResult : <%d>\n", result);
                }
                printf("\n");

                if(g_b_addDelay)
                    usleep(10000);
            }

            T_AST ast = {0};
            if (apiNo == 5 )
            {   ret_fn = OtdrGetAst(&ast);
                if (ret_fn)
                {   printf("OtdrGetAst failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrGetAst : <%d>\n", ret_fn);
                    printf("TEST APP: ast.Mth : <%d>\n", ast.Mth);
                    printf("TEST APP: ast.Mtl : <%d>\n", ast.Mtl);
                    printf("TEST APP: ast.Mtx : <%d>\n", ast.Mtx);
                    printf("TEST APP: ast.Tec1tmp : <%d>\n", ast.Tec1tmp);
                }
                printf("\n");

                if(g_b_addDelay)
                    usleep(10000);
            }

            if (apiNo == 6 )
            {  
            
                    ret_fn = OtdrStatus(&get_status);
                    if (ret_fn)
                    {   printf("OtdrStatus failed. Error:%d\n", ret_fn);
                    }
                    else
                    {   printf("TEST APP: OtdrStatus :: <%d>\n", get_status.currentState);
                        if (get_status.currentState == OTDR_STATE_IDLE)
                        {   
                            printf("IDLE\n");
                        }
                        else if (get_status.currentState == OTDR_STATE_CAPTURE)
                        {   printf("TEST APP: OtdrStatus Capture Percentage: <%f>\n", get_status.percentage);
                             printf("CAPTURE\n");
                        }
                       
                        else if(get_status.currentState == OTDR_STATE_LIVE_CAPTURE)
                        {
                            printf("LIVE CAPTURE\n");
                        }
                        else if(get_status.currentState == OTDR_STATE_PROCESS)
                        {
                            printf("PROCESS\n");
                        }
                         else if(get_status.currentState == OTDR_STATE_ANALYZE)
                        {
                            printf("ANALYZE\n");
                        }
                          else if(get_status.currentState == OTDR_STATE_TRANSFER)
                        {
                            printf("TRANSFER\n");
                        }
						else if(get_status.currentState == OTDR_STATE_SETUP)
                        {
                            printf("SETUP\n");
                        }
                        else
                        {
                            printf("UNKNOWN\n");
                        }
    
                    }
                    
                printf("\n");
            }
        }

        if(g_b_addDelay)
            usleep(10000);

        if(option == OTDR_NORMAL_MODE)
        {   float thr = 0.3;

            if (apiNo == 7 )
            {  
               printf("Set EventLossThreshold (default val : 0.3) : \n");
               scanf("%f",&thr); 
                 ret_fn = OtdrSetEventLossThreshold(thr);
                if (ret_fn)
                {   printf("OtdrSetEventLossThresold failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetEventLossThresold : <%f>\n\n", thr);
                }

                if(g_b_addDelay)
                    usleep(10000);
            }

            float pw = 0.1;
            if (apiNo == 8 )
            {   
                printf("SetPulseWidth (default val : 0.1) : \n");
                scanf("%f",&pw);
                ret_fn = OtdrSetPulseWidth(pw);
                if (ret_fn)
                {   printf("OtdrSetPulseWidth failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetPulseWidth : <%f>\n\n", pw);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            float ms = 1;
            if (apiNo == 9 )
            {   
                printf("SetMeasureTime (default : 1) : \n");
                scanf("%d",&ms);
                ret_fn = OtdrSetMeasureTime(ms);
                if (ret_fn)
                {   printf("OtdrSetMeasureTime failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetMeasureTime : <%f>\n\n", ms);
                }

                if(g_b_addDelay)
                    usleep(10000);
            }

            float ws = 45;
            if (apiNo == 10)
            {   
                printf("set WindowSize (default : 45) : \n");
                scanf("%f",&ws);
                ret_fn = OtdrSetWindowSize(ws);
                if (ret_fn)
                {   printf("OtdrSetWindowSize failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetWindowSize : <%f>\n\n", ws);
                }

                if(g_b_addDelay)
                    usleep(10000);
            }

            float res = 1.6;
            if (apiNo == 11 )
            {   
                printf("Set Resolution (default : 1.6) : \n");
                scanf("%f",&res);
                ret_fn = OtdrSetResolution(res);
                if (ret_fn)
                {   printf("OtdrSetResolution failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetResolution : <%f>\n\n", res);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            float sl = 45;
            if (apiNo == 12)
            {   printf("Set SpanLength (default 45 ): \n");
                scanf("%f",&sl);
                ret_fn = OtdrSetSpanLength(sl);
                if (ret_fn)
                {   printf("OtdrSetSpanLength failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetSpanLength : <%f>\n\n", sl);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            float wo = 0;
            if (apiNo == 13 )
            {
                printf("set Window Offset (default : 0) : \n");
                scanf("%f",&wo); 
                ret_fn = OtdrSetWindowOffset(wo);
                if (ret_fn)
                {   printf("OtdrSetWindowOffset failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetWindowOffset : <%f>\n", wo);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            double refractiveIndex = 1.4989623;
            if (apiNo == 14 )
            {   
                printf("set RefractiveIndex (default 1.4989623) : \n");
                scanf("%lf",&refractiveIndex);
                ret_fn = OtdrSetRefractiveIndex(refractiveIndex);
                if (ret_fn)
                {   printf("OtdrSetRefractiveIndex failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetRefractiveIndex : <%.7lf>\n\n", refractiveIndex);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }
        }

        if(option == OTDR_FAST_MODE)
        {   float thr = 0.3;
            if (apiNo == 7 )
            {   
                printf("OtdrSetEventLossThreshold (default : 0.3) : \n");
                scanf("%f",&thr);
                ret_fn = OtdrSetEventLossThreshold(thr);
                if (ret_fn)
                {   printf("OtdrSetEventLossThresold failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetEventLossThresold : <%f>\n", thr);
                }

                if(g_b_addDelay)
                    usleep(10000);

            }

            float ms = 1;
            if (apiNo == 9 )
            {   
                printf("OtdrSetMeasureTime (default : 1) : \n");
                scanf("%f",&ms);
                ret_fn = OtdrSetMeasureTime(ms);
                if (ret_fn)
                {   printf("OtdrSetMeasureTime failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetMeasureTime : <%f>\n", ms);
                }

                if(g_b_addDelay)
                    usleep(10000);
            }

            float pw = 2;
            if (apiNo == 8 )
            {   
                printf("OtdrSetPulseWidth (default : 2) : \n");
                scanf("%f",&pw);
                ret_fn = OtdrSetPulseWidth(pw);
                if (ret_fn)
                {   printf("OtdrSetPulseWidth failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetPulseWidth : <%f>\n", pw);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            float res = 8;
            if (apiNo == 11 )
            {   
                printf("OtdrSetResolution (default : 8) : \n");
                scanf("%f",&res);
                ret_fn = OtdrSetResolution(res);
                if (ret_fn)
                {   printf("OtdrSetResolution failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetResolution : <%f>\n", res);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            float sl = 30;
            if (apiNo == 12 )
            {   
                printf("OtdrSetSpanLength (default : 30) : \n");
                scanf("%f",&sl);
                ret_fn = OtdrSetSpanLength(sl);
                if (ret_fn)
                {   printf("OtdrSetSpanLength failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetSpanLength : <%f>\n", sl);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            float ws = 35;
            if (apiNo == 10 )
            {   
                printf("OtdrSetWindowSize (default : 35) : \n");
                scanf("%f",&ws);
                ret_fn = OtdrSetWindowSize(ws);
                if (ret_fn)
                {   printf("OtdrSetWindowSize failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetWindowSize : <%f>\n", ws);
                }

                if(g_b_addDelay)
                    usleep(10000);
            }

            float wo = 0;
            if (apiNo == 13 )
            {   
                printf("OtdrSetWindowOffset (default : 0) : \n");
                scanf("%f",&wo);
                ret_fn = OtdrSetWindowOffset(wo);
                if (ret_fn)
                {   printf("OtdrSetWindowOffset failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetWindowOffset : <%f>\n", wo);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            double refractiveIndex = 1.4989623;
            if (apiNo == 14 )
            {   
                printf("OtdrSetRefractiveIndex (default : 1.4989623) : \n");
                scanf("%lf",&refractiveIndex);
                ret_fn = OtdrSetRefractiveIndex(refractiveIndex);
                if (ret_fn)
                {   printf("OtdrSetRefractiveIndex failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetRefractiveIndex : <%.7lf>\n\n", refractiveIndex);
                }
            }
        }

        if(g_b_addDelay)
            usleep(1000000);


        if(!otdrFuncSelect)
        {   T_OTDRCONFIG params = {0};
            if (apiNo == 15)
            {   ret_fn = OtdrGetParameters(&params);
                if (ret_fn)
                {   printf("OtdrGetParameters failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrGetParameters lossThreshold : <%f>\n", params.lossThreshold);
                    printf("TEST APP: OtdrGetParameters measureTime : <%f>\n", params.measureTime);
                    printf("TEST APP: OtdrGetParameters pulseWidth : <%f>\n", params.pulseWidth);
                    printf("TEST APP: OtdrGetParameters resolution : <%f>\n", params.resolution);
                    printf("TEST APP: OtdrGetParameters spanLength : <%f>\n", params.spanLength);
                    printf("TEST APP: OtdrGetParameters windowSize : <%f>\n", params.windowSize);
                    printf("TEST APP: OtdrGetParameters refractiveIndex : <%.7lf>\n", params.refractiveIndex);
                    printf("TEST APP: OtdrGetParameters windowOffset : <%f>\n", params.windowOffset);
                    printf("TEST APP: OtdrGetParameters reflectionThreshold : <%.7f>\n", params.reflectionThreshold);

                }
                printf("\n");

                if(g_b_addDelay)
                    usleep(10000);
            }

            float get_mt = 0;

            if (apiNo == 16)
            {   ret_fn = OtdrGetMt(&get_mt);
                if (ret_fn)
                {   printf("OtdrGetMt failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrGetMt	 : <%f>\n\n", get_mt);
                }

                if(g_b_addDelay)
                    usleep(10000);
            }

            if (apiNo == 17)
            {   ret_fn = OtdrAnalyze();
                if (ret_fn)
                {   printf("OtdrAnalyze failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrAnalyze: <%d>\n\n", ret_fn);
                }
                /* Wait for analyze operation to complete */
                sleep(2);
            }
        }

        l_i_retry_count = 0;
        if (apiNo == 18 )
        {   
                  ret_fn = OtdrRun();
                if (0 != ret_fn)
                {   printf("OtdrRun failed or busy. Error:%d\n", ret_fn);
                    sleep(2);
                }
                else
                {   printf("TEST APP: OtdrRun: <%d>\n\n", ret_fn);
                    break;
                }
               
        }

        l_i_retry_count = 0;
            if (apiNo == 19)
            {  T_DUMPTRACE trace = { 0 };
                int ret_fn = OtdrDumpTrace(&trace);
                if (ret_fn)
                {   if (ret_fn == OTDR_NOT_DEFINED)
                    {   printf("OtdrDumpTrace(trace not defined:%d\n", ret_fn);
                    }
                    else
                    {   printf("OtdrDumpTrace(trace failed. Error:%d\n", ret_fn);
                    }
                }
                else
                {   printf("\nTEST APP: OtdrDumpTrace(trace : <%d>\n", ret_fn);
                    printf("trace.points:%u \n", trace.points);
                    printf("trace.dx:%f \n", trace.dx);
                    printf("trace.windowOffset:%f \n", trace.windowOffset);
                    int traceCount = 0;
                          for (traceCount = 0; traceCount < trace.points; ++traceCount)
                        {   printf("%e\n", trace.power[traceCount]);
                        }
                }
                if (NULL != trace.power)
                {   free(trace.power);
                    trace.power = NULL;
                }
            }
        
        
        if (apiNo == 20 )
        {   T_DUMPTRACE ltrace = { 0 };
            ret_fn = OtdrDumpTraceLinear(&ltrace);
            if (ret_fn)
            {   if (ret_fn == OTDR_NOT_DEFINED)
                {   printf("OtdrDumpTraceLinear trace not defined:%d\n", ret_fn);
                }
                else
                {   printf("OtdrDumpTraceLinear trace failed. Error:%d\n", ret_fn);
                }
            }
            else
            {   printf("\nTEST APP: OtdrDumpTraceLinear trace : <%d>\n", ret_fn);
                printf("trace.points:%u \n", ltrace.points);
                printf("trace.dx:%f \n", ltrace.dx);
                printf("trace.windowOffset:%f \n", ltrace.windowOffset);
                int ltraceCount = 0;
                if(otdrMsgVal == OTDR_DUMP_MSG_ENABLE)
                {   for (ltraceCount = 0; ltraceCount < ltrace.points; ++ltraceCount)
                    {   printf("%e\n", ltrace.power[ltraceCount]);
                    }
                }
            }
            if (NULL != ltrace.power)
            {   free(ltrace.power);
                ltrace.power = NULL;
            }
        }

        
        if (apiNo == 21 )
        {  
            T_DUMPEVENT event_table = {0};
             int itr = 0;
            ret_fn = OtdrDumpEvent(&event_table);
            if (ret_fn)
            {   if (ret_fn == OTDR_NOT_DEFINED)
                {   printf("OtdrDumpEvent(event not defined:%d\n", ret_fn);
                }
                else
                {   printf("OtdrDumpEvent(event failed. Error:%d\n", ret_fn);
                }
            }
            else
            {   printf("\n\nTEST APP: OtdrDumpEvent(event : <%d>\n", ret_fn);
                printf("TEST APP: OtdrDumpEventLength(length : <%u>\n", event_table.n);

                for (itr = 0; itr < event_table.n; ++itr)
                {   printf("Event%d:\n", itr + 1);
                    printf("\nevent_table.e->type:%u  ", ((event_table.e) + itr)->type);
                    printf("\nevent_table.e->x_km:%f  ", ((event_table.e) + itr)->x_km);
                    printf("\nevent_table.e->loss_u_km:%f  ",
                           ((event_table.e) + itr)->loss_u_km);
                    printf("\nevent_table.e->loss_dB:%f  ",
                           ((event_table.e) + itr)->loss_dB);
                    printf("\nevent_table.e->c_loss_db:%f  ",
                           ((event_table.e) + itr)->c_loss_dB);
                    printf("\nevent_table.e->attn_dB_km:%f  ",
                           ((event_table.e) + itr)->attn_dB_km);
                    printf("\nevent_table.e->refl_dB:%f  ",
                           ((event_table.e) + itr)->refl_dB);
                    printf("\nevent_table.e->refl_u_km:%f  ",
                           ((event_table.e) + itr)->refl_u_km);
                    printf("\n");
                }
            }
            if (NULL != event_table.e)
            {   free(event_table.e);
                event_table.e = NULL;
            }
        }

        if(!otdrFuncSelect)
        {   if(g_b_addDelay)
                usleep(10000);

            float pd[2] = { 0 };
            if (apiNo == 22)
            {   ret_fn = OtdrGetPd(pd);
                if (ret_fn)
                {   printf("OtdrGetPd failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrGetPd	 : <pd1:%f  pd2:%f>\n\n", pd[0], pd[1]);
                }

                if(g_b_addDelay)
                    usleep(10000);
            }

            T_HWVER ver_hw = {0};
            if (apiNo == 23 )
            {   ret_fn = OtdrGetHwVer(&ver_hw);
                if (ret_fn)
                {   printf("OtdrGetHwVer failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("\n");
                    printf("TEST APP: OtdrGetVerHw\n");
                    printf("Designversion : %s\n", ver_hw.design);
                    printf("Loaderversion : %s\n", ver_hw.loader);
                    printf("SPIperipheralversion : %s\n", ver_hw.spiPeripheral);
                    printf("Systemclock : %s\n", ver_hw.sysClock);
                    printf("Schedulingperiod : %s\n", ver_hw.schedulePeriod);
                    printf("Datacache : %s\n", ver_hw.dataCache);
                    printf("Instcache : %s\n\n", ver_hw.instCache);
                    printf("OtdrGetHwVer Completed\n");
                }

                if(g_b_addDelay)
                    usleep(10000);
            }

            char time[] = "14:00:00";
            char date[] = "2019-08-22";
            if (apiNo == 24 )
            {   ret_fn = OtdrSetRtc(time, date);
                if (ret_fn)
                {   printf("OtdrSetRtc failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetRtc: <%d>\n\n", ret_fn);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            char *get_date= NULL;
            char *get_time=NULL;
            if (apiNo == 25)
            {   get_date = (char *) malloc(32 * sizeof(char));
                get_time = (char *) malloc(32 * sizeof(char));
                ret_fn = OtdrGetRtc(get_time, get_date);
                if (ret_fn)
                {   printf("OtdrGetRtc failed. Error:%d", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrGetRtc: <%d>\n", ret_fn);
                    printf("TEST APP: Date: <%s>\n", get_date);
                    printf("TEST APP: Time: <%s>\n", get_time);
                }

                if (NULL != get_date)
                {   free(get_date);
                    get_date = NULL;
                }

                if (NULL != get_time)
                {   free(get_time);
                    get_time = NULL;
                }
                printf("\n");

                if(g_b_addDelay)
                    usleep(10000);
            }

            T_SUPPLY supply = {0};
            if (apiNo == 26)
            {   ret_fn = OtdrGetSupply(&supply);
                if (ret_fn)
                {   printf("OtdrGetSupply failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrGetSupply : <%d>\n", ret_fn);
                    printf("TEST APP: supply.current : <current:%f>\n", supply.current);
                    printf("TEST APP: supply.power : <power:%f>\n", supply.power);
                    printf("TEST APP: supply.voltage : <voltage:%f>\n", supply.voltage);
                }
                printf("\n");

                if(g_b_addDelay)
                    usleep(10000);
            }
#if 0
            T_OTDRALARMS alarm = 0;
            float hysteresis = 5;
            ret_fn =  OtdrSetAlrmHysteresis(alarm, hysteresis);
            if (ret_fn)
            {   printf("OtdrSetAlrmHysteresis failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrSetAlrmHysteresis : <%f>\n", hysteresis);
            }
#endif
            if(g_b_addDelay)
                usleep(10000);

            T_OTDRALARMS alarm_latch = 0;

            if (apiNo == 27 )
            {   ret_fn = OtdrClearAlarmLatch(alarm_latch);

                if (ret_fn)
                {   printf("OtdrClearAlarmLatch failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrClearAlarmLatch ret_fn:%d\n", ret_fn);
                }
                printf("\n");
                if(g_b_addDelay)
                    usleep(10000);
            }

            if (apiNo == 28 )
            {   ret_fn = OtdrSetAstmNormal();
                if (ret_fn)
                {   printf("OtdrSetAstmNormal failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetAstmNormal : <%d>\n", ret_fn);
                }
                printf("\n");
                if(g_b_addDelay)
                    usleep(10000);
            }

            if (apiNo == 29)
            {   ret_fn = OtdrSetAstmSticky();
                if (ret_fn)
                {   printf("OtdrSetAstmSticky failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetAstmSticky : <%d>\n", ret_fn);
                }
                printf("\n");
                if(g_b_addDelay)
                    usleep(10000);
            }

            float set_thr1 = -50;
            if (apiNo == 30)
            {   ret_fn = OtdrSetEventReflectionThreshold(set_thr1);
                if (ret_fn)
                {   printf("OtdrSetEventReflThresold failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrSetEventReflThresold : <%f>\n", set_thr1);
                }
                printf("\n");
                if(g_b_addDelay)
                    usleep(10000);
            }

           
#if 0
            T_OTDRALARMS l_alrm = 3;
            float threshold = 3;
            ret_fn = OtdrSetAlrmThreshold(l_alrm,threshold);
            if (ret_fn)
            {   printf("OtdrSetAlrmThreshold failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrSetAlrmThreshold : <%d>\n\n", ret_fn);
            }

            sleep(3);
#endif
            if(g_b_addDelay)
                usleep(10000);


            T_ALRM alrm1= {0};
            if (apiNo == 31)
            {   ret_fn = OtdrGetALRM(&alrm1);
                if (ret_fn)
                {   printf("OtdrGetALRM failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrGetALRM : <%d>\n", ret_fn);
                    printf("alrm.m_d_MtxSta:%d\n", alrm1.MtxSta);
                    printf("alrm.MtxSst:%d\n", alrm1.MtxSst);
                    printf("alrm.MtxThr:%f\n", alrm1.MtxThr);
                    printf("alrm.MtxHys:%f\n", alrm1.MtxHys);
                    printf("alrm.MthSta:%d\n", alrm1.MthSta);
                    printf("alrm.MthSst:%d\n", alrm1.MthSst);
                    printf("alrm.MthThr:%f\n", alrm1.MthThr);
                    printf("alrm.MthHys:%f\n", alrm1.MthHys);
                    printf("alrm.MtlSta:%d\n", alrm1.MtlSta);
                    printf("alrm.MtlSst:%d\n", alrm1.MtlSst);
                    printf("alrm.MtlThr:%f\n", alrm1.MtlThr);
                    printf("alrm.MtlHys:%f\n", alrm1.MtlHys);
                    printf("alrm.Tec1tmpSta:%d\n", alrm1.Tec1tmpSta);
                    printf("alrm.Tec1tmpSst:%d\n", alrm1.Tec1tmpSst);
                    printf("alrm.Tec1tmpThr:%f\n", alrm1.Tec1tmpThr);
                    printf("alrm.Tec1tmpHys:%f\n", alrm1.Tec1tmpHys);
                    printf("alrm.Tec2tmpSta:%d\n", alrm1.Tec2tmpSta);
                    printf("alrm.Tec2tmpSst:%d\n", alrm1.Tec2tmpSst);
                    printf("alrm.Tec2tmpThr:%f\n", alrm1.Tec2tmpThr);
                    printf("alrm.Tec2tmpHys:%f\n", alrm1.Tec2tmpHys);
                }
                if(g_b_addDelay)
                    usleep(10000);
            }

            if(apiNo == 32)
            { 
                ret_fn = OtdrLiveRun();
                 printf("Otdr live run: %d\n",ret_fn);                  
            }

            if(apiNo == 33 )
            {   
                ret_fn = OtdrLiveStop();
                printf("otdr live stop :%d\n",ret_fn);
            }


            if(apiNo == 34 )
            {   
                T_LIVETRACECOUNT cnt = {0};
                ret_fn = OtdrLiveTraceCount(&cnt);
                if (ret_fn)
                {   printf("OtdrGetLiveCounts failed. Error:%d\n", ret_fn);
                    
                }
                else
                {
                    printf("LiveCaptureCount : <%d>\n", cnt.liveScanCaptureCnt);
                    printf("LiveTraceCount : <%d>\n", cnt.liveScanProcessCnt);
                    printf("LiveAvgTraceCount : <%d>\n", cnt.liveScanAvgProcessCnt);
                    printf("LiveAnalyzeTraceCount : <%d>\n", cnt.analyzeTraceindex);
                    printf("LiveAvgAnalyzeTraceCount : <%d>\n", cnt.avgAnalyzeTraceindex);

                }
            }

            if(apiNo == 35)
            {   printf("otdr live AvgAnalyze : %d\n",OtdrLiveAvgAnalyze());
            }



            if(apiNo == 36)
            {  T_DUMPEVENT event_table = {0};
                int itr = 0;
                int ret_fn = OtdrLiveEvent(&event_table);
                if (ret_fn)
                {   if (ret_fn == OTDR_NOT_DEFINED)
                    {   printf("OtdrLiveEvent(Event not defined:%d\n", ret_fn);
                    }
                    else if(ret_fn == OTDR_LIVE_INVALID_DATA)
                    {printf("OtdrLiveEvent(Event not defined:%d\n", ret_fn);
                    }
                    else
                    {   printf("OtdrLiveEvent(Event failed. Error:%d\n", ret_fn);
                    }
                }
                else
                {   printf("\n\nTEST APP: OtdrLiveEvent(event : <%d>\n", ret_fn);
                    printf("TEST APP: OtdrLiveEventLength(length : <%u>\n", event_table.n);

                    for (itr = 0; itr < event_table.n; ++itr)
                    {   printf("Event%d:\n", itr + 1);
                        printf("\nevent_table.e->type:%u  ", ((event_table.e) + itr)->type);
                        printf("\nevent_table.e->x_km:%f  ", ((event_table.e) + itr)->x_km);
                        printf("\nevent_table.e->loss_u_km:%f  ",
                               ((event_table.e) + itr)->loss_u_km);
                        printf("\nevent_table.e->loss_dB:%f  ",
                               ((event_table.e) + itr)->loss_dB);
                        printf("\nevent_table.e->c_loss_db:%f  ",
                               ((event_table.e) + itr)->c_loss_dB);
                        printf("\nevent_table.e->attn_dB_km:%f  ",
                               ((event_table.e) + itr)->attn_dB_km);
                        printf("\nevent_table.e->refl_dB:%f  ",
                               ((event_table.e) + itr)->refl_dB);
                        printf("\nevent_table.e->refl_u_km:%f  ",
                               ((event_table.e) + itr)->refl_u_km);
                        printf("\n");
                    }
                }
                if (NULL != event_table.e)
                {   free(event_table.e);
                    event_table.e = NULL;
                } 
            }


            if(apiNo == 37)
            {   printf("otdr live analyze : %d\n",OtdrLiveAnalyze());
            }


            if(apiNo == 38)
            {   T_DUMPTRACE liveTrace = {0};
                int ret_fn = OtdrLiveTrace(&liveTrace);
                printf("otdr live Trace : %d\n",ret_fn);
                if (ret_fn)
                {   if (ret_fn == OTDR_NOT_DEFINED)
                    {   printf("OtdrLiveTrace(trace not defined:%d\n", ret_fn);
                    }
                    else if(ret_fn == OTDR_LIVE_INVALID_DATA)
                    {printf("OtdrLiveTrace(trace not defined:%d\n", ret_fn);
                    }
                    else
                    {   printf("OtdrLiveTrace(trace failed. Error:%d\n", ret_fn);
                    }
                }
                else
                {   printf("\nTEST APP: OtdrLiveTrace(trace : <%d>\n", ret_fn);
                    printf("livetrace.points:%u \n", liveTrace.points);
                    printf("livetrace.dx:%f \n", liveTrace.dx);
                    printf("livetrace.windowOffset:%f \n", liveTrace.windowOffset);
                    int traceCount = 0;
                    for (traceCount = 0; traceCount < liveTrace.points; ++traceCount)
                    {   printf("%e\n", liveTrace.power[traceCount]);
                    }
                }
                if (NULL != liveTrace.power)
                {   free(liveTrace.power);
                    liveTrace.power = NULL;
                }
            }


            if(apiNo == 39)
            {   T_DUMPEVENT event_table = {0};
                int itr = 0;
                int ret_fn = OtdrLiveAvgEvent(&event_table);
                if (ret_fn)
                {   if (ret_fn == OTDR_NOT_DEFINED)
                    {   printf("OtdrLiveAvgEvent(Event not defined:%d\n", ret_fn);
                    }
                    else if(ret_fn == OTDR_LIVE_INVALID_DATA)
                    {printf("OtdrLiveAvgEvent(Event not defined:%d\n", ret_fn);
                    }
                    else
                    {   printf("OtdrLiveAvgEvent(Event failed. Error:%d\n", ret_fn);
                    }
                }
                else
                {   printf("\n\nTEST APP: OtdrLiveAvgEvent(event : <%d>\n", ret_fn);
                    printf("TEST APP: OtdrLiveAvgEventLength(length : <%u>\n", event_table.n);

                    for (itr = 0; itr < event_table.n; ++itr)
                    {   printf("Event%d:\n", itr + 1);
                        printf("\nevent_table.e->type:%u  ", ((event_table.e) + itr)->type);
                        printf("\nevent_table.e->x_km:%f  ", ((event_table.e) + itr)->x_km);
                        printf("\nevent_table.e->loss_u_km:%f  ",
                               ((event_table.e) + itr)->loss_u_km);
                        printf("\nevent_table.e->loss_dB:%f  ",
                               ((event_table.e) + itr)->loss_dB);
                        printf("\nevent_table.e->c_loss_db:%f  ",
                               ((event_table.e) + itr)->c_loss_dB);
                        printf("\nevent_table.e->attn_dB_km:%f  ",
                               ((event_table.e) + itr)->attn_dB_km);
                        printf("\nevent_table.e->refl_dB:%f  ",
                               ((event_table.e) + itr)->refl_dB);
                        printf("\nevent_table.e->refl_u_km:%f  ",
                               ((event_table.e) + itr)->refl_u_km);
                        printf("\n");
                    }
                }
                if (NULL != event_table.e)
                {   free(event_table.e);
                    event_table.e = NULL;
                }
            }

            if(apiNo == 40 )
            {    T_LIVEANALYZESTATUS live_analyze_status ={0};
            ret_fn = OtdrLiveAnalyzeStatus(&live_analyze_status);
            if (ret_fn)
            {   printf("OtdrLiveAnalyzeStatus failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrLiveAnalyzeStatus : <%d>\n\n", ret_fn);
                 if(live_analyze_status.analyzestatus == LIVE_ANALYZESTA_ANALYZE)
                    printf("ANALYZE\n");
                else if(live_analyze_status.analyzestatus == LIVE_ANALYZESTA_IDLE)
                    printf("IDLE\n");
            }
            
            }

            if(apiNo == 41 )
            {   int max_ne;//1 to 4
            printf("Enter max_ne (1 - 4)");
            scanf("%d",&max_ne);
            ret_fn = OtdrSetMaxNE(max_ne);
            if(ret_fn)
            printf("OtdrSetMAxNE failed. Error:%d\n", ret_fn);
            }

            if(apiNo == 42 )
            {   ret_fn = OtdrStop();
                if (ret_fn)
                {   printf("OtdrStop failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrStop: <%d>\n\n", ret_fn);
                }
            }
            if(apiNo == 43 )
            {   int max_ne = 0;
                ret_fn = OtdrGetMaxNE(&max_ne);
                if (ret_fn)
                {   printf("OtdrGetMaxNE failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: Otdr Max NE <%d>\n\n", max_ne);
                }
            }
            if(apiNo == 44 )
            {
                   T_DUMPTRACE liveTrace = {0};
                int ret_fn = OtdrLiveAnalyzeTrace(&liveTrace);
                printf("otdr live AnalyzeTrace : %d\n",ret_fn);
                if (ret_fn)
                {   if (ret_fn == OTDR_NOT_DEFINED)
                    {   printf("OtdrLiveAnalyzeTrace(trace not defined:%d\n", ret_fn);
                    }
                    else if(ret_fn == OTDR_LIVE_INVALID_DATA)
                    {printf("OtdrLiveAnalyzeTrace(trace not defined:%d\n", ret_fn);
                    }
                    else
                    {   printf("OtdrLiveAnalyzeTrace(trace failed. Error:%d\n", ret_fn);
                    }
                }
                else
                {   printf("\nTEST APP: OtdrLiveAnalyzeTrace(trace : <%d>\n", ret_fn);
                    printf("OtdrLiveAnalyzeTrace.points:%u \n", liveTrace.points);
                    printf("OtdrLiveAnalyzeTrace.dx:%f \n", liveTrace.dx);
                    printf("OtdrLiveAnalyzeTrace.windowOffset:%f \n", liveTrace.windowOffset);
                    int traceCount = 0;
                    for (traceCount = 0; traceCount < liveTrace.points; ++traceCount)
                    {   printf("%e\n", liveTrace.power[traceCount]);
                    }
                }
                if (NULL != liveTrace.power)
                {   free(liveTrace.power);
                    liveTrace.power = NULL;
                }
            }
            if(apiNo == 45)
            {
                T_DUMPTRACE liveTrace = {0};
                int ret_fn = OtdrLiveAvgAnalyzeTrace(&liveTrace);
                printf("otdr live AvgAnalyzeTrace : %d\n",ret_fn);
                if (ret_fn)
                {   if (ret_fn == OTDR_NOT_DEFINED)
                    {   printf("OtdrLiveAvgAnalyzeTrace(trace not defined:%d\n", ret_fn);
                    }
                    else if(ret_fn == OTDR_LIVE_INVALID_DATA)
                    {printf("OtdrLiveAvgAnalyzeTrace(trace not defined:%d\n", ret_fn);
                    }
                    else
                    {   printf("OtdrLiveAvgAnalyzeTrace(trace failed. Error:%d\n", ret_fn);
                    }
                }
                else
                {   printf("\nTEST APP: OtdrLiveAvgAnalyzeTrace(trace : <%d>\n", ret_fn);
                    printf("OtdrLiveAvgAnalyzeTrace.points:%u \n", liveTrace.points);
                    printf("OtdrLiveAvgAnalyzeTrace.dx:%f \n", liveTrace.dx);
                    printf("OtdrLiveAvgAnalyzeTrace.windowOffset:%f \n", liveTrace.windowOffset);
                    int traceCount = 0;
                    for (traceCount = 0; traceCount < liveTrace.points; ++traceCount)
                    {   printf("%e\n", liveTrace.power[traceCount]);
                    }
                }
                if (NULL != liveTrace.power)
                {   free(liveTrace.power);
                    liveTrace.power = NULL;
                }
            }

        }


            if(apiNo == 46 )
            {   T_DUMPTRACE liveTrace = {0};
                int ret_fn = OtdrLiveAvgTrace(&liveTrace);
                printf("otdr live AvgTrace : %d\n",ret_fn);
                if (ret_fn)
                {   if (ret_fn == OTDR_NOT_DEFINED)
                    {   printf("OtdrLiveAvgTrace(trace not defined:%d\n", ret_fn);
                    }
                    else if(ret_fn == OTDR_LIVE_INVALID_DATA)
                    {printf("OtdrLiveAvgTrace(trace not defined:%d\n", ret_fn);
                    }
                    else
                    {   printf("OtdrLiveAvgTrace(trace failed. Error:%d\n", ret_fn);
                    }
                }
                else
                {   printf("\nTEST APP: OtdrLiveAvgTrace(trace : <%d>\n", ret_fn);
                    printf("OtdrLiveAvgTrace.points:%u \n", liveTrace.points);
                    printf("OtdrLiveAvgTrace.dx:%f \n", liveTrace.dx);
                    printf("OtdrLiveAvgTrace.windowOffset:%f \n", liveTrace.windowOffset);
                    int traceCount = 0;
                    for (traceCount = 0; traceCount < liveTrace.points; ++traceCount)
                    {   printf("%e\n", liveTrace.power[traceCount]);
                    }
                }
                if (NULL != liveTrace.power)
                {   free(liveTrace.power);
                    liveTrace.power = NULL;
                }
            }

            if(apiNo == 47)
            {
                    ret_fn = OtdrReboot();
                    if (ret_fn)
                    {   printf("OtdrReboot failed. Error:%d\n", ret_fn);
                    }
                    else
                    {   printf("TEST APP: OtdrReboot: <%d>\n\n", ret_fn);
                    }
            }
            


    }
    while(--count > 0);

    /** Otdr De-Initialization **/
    if(ret_fn != OTDR_INIT_FAILED)
    {   if (0 != (ret_fn = OtdrInterfaceDeInit()))
        {   printf("ERROR: Otdr DeInit Failed \n");
        }
        else
            printf("Otdr DeInitialized Successfully \n");
    }

}

void _occ(void)
{

    unsigned char *data;

    /* Initialize OCC */
    int status = OCCInterfaceInit();
    if(status != OCC_SUCCESS)
    {   printf("Failed to Intialize Error <%d>\n", status);

    }

    float val;
    int pdval = 0;
    int ret = 0;

    for(pdval = PD_1577; pdval <= PD_NONE; pdval++)
    {   ret = OCCReadPWR(pdval, &val);
        printf("OCCReadPWR : Return Status: <%d>\n", ret);
        printf("PD<%d>: <%e mW> \n", pdval,val);
        val = 0;
    }
    printf("\n\n");

    T_POWER power = {0};

    for(pdval = PD_1577; pdval <= PD_NONE; pdval++)
    {   ret = OCCReadCurrPWR(pdval, &power);
        printf("OCCReadCurrPWR : Return Status: <%d>\n", ret);
        printf("PD<%d>: <%e mW> \n", pdval, power.f_pwrVal);
        printf("Current <%e> \n", power.f_current);
        val = 0;
    }
    /** Deinit the OCC*/
    OCCInterfaceDeInit();

    return ;
}

void _sfpseq(void)
{   uint8_t data = 0;
    uint8_t device_address = 0xA0;
    uint8_t device_address1 = 0xA2;
    uint8_t page_reg_add_vn = 0x14;
    uint8_t page_reg_add_pn = 0x28;
    uint8_t page_reg_add_sn = 0x44;
    uint8_t page_data[17] = {0};
    uint8_t page_read[2] = {0};
    uint16_t data1 = 0;
    int status;
    int n = 16;
    float vcc = 0;
    float TxPWR = 0;
    float RxPWR = 0;

    /* Commented, As used same function in stressTest without delay */
    //printf("Once SFP is ON,Please wait for 10 seconds to get the proper response\n");
    //sleep(10);

    status = SFPReadWord(device_address1, 96, &data1);// Internally measured module temperature. Temp MSB=96, LSB=97
    float T = ((int16_t)((data1 & 0xFF) << 8 | (data1 & 0xFF00) >> 8)) / 256.0;
    if (status != 0)
        printf("ERROR: SFPReadWord Error no: %d\n", status);
    else
        printf("SFP. API:SFPReadWord. API Response Temp: <%fC> (0x%04x)\n", T, data1);

    status = SFPReadWord(device_address1, 98, &data1);
    long int tmp_vcc = (data1 & 0xFF)*256 + (data1&0xFF00)/256;
    vcc  = tmp_vcc*100e-6;
    if (status != 0)
        printf("ERROR: SFPReadWord Error no: %d\n", status);
    else
        printf("SFP. API:SFPReadWord. API Response Vcc: <%fC> (0x%02x)\n",vcc,tmp_vcc);

    uint16_t tx_data = 0;
    status = SFPReadWord(device_address1, 102, &tx_data);
    long int tmp_tx = (tx_data & 0xFF)*256 + (tx_data&0xFF00)/256;
    TxPWR = tmp_tx*1e-4;
    if (status != 0)
        printf("ERROR: SFPReadWord Error no: %d\n", status);
    else
    {   if (TxPWR>0)
            printf("SFP. API:SFPReadWord. API Response TxPWR: <%fC> (0x%02x)\n",10*log10(TxPWR), tmp_tx);
        else
            printf("SFP. API:SFPReadWord. API Response TxPWR: <%fC> (0x%02x)\n", TxPWR, tmp_tx);

    }

    uint16_t rx_data = 0;
    status = SFPReadWord(device_address1, 104, &rx_data);
    long int tmp_rx = (rx_data & 0xFF)*256 + (rx_data&0xFF00)/256;
    RxPWR  = tmp_rx*1e-4;
    if (status != 0)
        printf("ERROR: SFPReadWord Error no: %d\n", status);
    else
    {   if (RxPWR>0)
            printf("SFP. API:SFPReadWord. API Response RxPWR: <%fC> (0x%02x)\n",10*log10(RxPWR), tmp_rx);
        else
            printf("SFP. API:SFPReadWord. API Response RxPWR: <%fC> (0x%02x)\n", RxPWR, tmp_rx);
    }

    usleep(50000);
    status = SFPPageRead(device_address, page_reg_add_vn, n, page_data);
    page_data[17] = '\0';
    if (status != 0)
        printf("ERROR: SFPPageRead Error no: %d\n", status);
    else
        printf("SFP. API:SFPPageRead. API Response Vendor Name: <%s> \n", page_data);
    usleep(50000);
    status = SFPPageRead(device_address, page_reg_add_pn, n, page_data);
    page_data[17] = '\0';
    if (status != 0)
        printf("ERROR: SFPPageRead Error no: %d\n", status);
    else
        printf("SFP. API:SFPPageRead. API Response Part Number: <%s> \n", page_data);
    usleep(50000);
    status = SFPPageRead(device_address, page_reg_add_sn, n, page_data);
    page_data[17] = '\0';
    if (status != 0)
        printf("ERROR: SFPPageRead Error no: %d\n", status);
    else
        printf("SFP. API:SFPPageRead. API Response Serial Number: <%s> \n", page_data);
    usleep(50000);

    return;
}
void _fsl_ver()
{   int status = -1;
    char ver[32] = {0};
    /**********Freescale Version()**********/
    status = FslVerCmd(ver);
    if (status != 0)
        printf("ERROR: Freescale communication. Error no: %d\n", status);
    else
        printf("Freescale Version: %s\n", ver);

    return ;
}
void _fsl_package()
{   int status = -1;
    char package[32] = {0};
    /**********Freescale Version()**********/
    status = FslPackage(package);
    if (status != 0)
        printf("ERROR: Freescale communication. Error no: %d\n", status);
    else
        printf("Freescale Package: %s\n", package);

    return ;
}

void _sfp(int count)
{   int status;
    uint8_t data = 0;
    uint16_t data1 = 0;
    uint8_t device_address = 0xA0;
    uint8_t device_address1 = 0xA2;
    uint8_t page_reg_add_vn = 0x14;
    uint8_t page_reg_add_pn = 0x28;
    uint8_t page_reg_add_sn = 0x44;
    uint8_t page_write_add = 0x80;
    float vcc;
    float TxPWR;
    float RxPWR;

    /* password */
    uint8_t byte123 = 0x7B;
    uint8_t byte124 = 0x7C;
    uint8_t byte125 = 0x7D;
    uint8_t byte126 = 0x7E;
    /* enable page */
    uint8_t byte127 = 0x7F;
    /* writting address */
    uint8_t byte128 = 0x80;
    uint8_t byte129 = 0x81;

    int n = 16;
    int i;
    uint8_t page_data[17] = {0};
    uint8_t page_read[2] = {0};
    int page_write_n = 3;
    uint8_t page_write_data[3] = {0xba,0xdc,0xfe};
    uint8_t page_write_read_data[3];
    uint8_t page_write_read_data1[150];

    g_b_addDelay = 1; //SFP must need proper delay we can't ignore in stress testing also.

    /**********SFPInterfaceInit()**********/
    status = SFPInterfaceInit();
    if (status != 0)
        printf("ERROR: SFPinit Error no: %d\n", status);
    else
        printf("SFP Initialized Successfully\n");

    while(--count >= 0)
    {   /**********SFPReadByte()**********/
        status = SFPReadByte(device_address, 0, &data);
        if (status != 0)
            printf("ERROR: SFPReadByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadByte. API Response Identifier: <%u>\n", data);
        status = SFPReadByte(device_address, 2, &data);
        if (status != 0)
            printf("ERROR: SFPReadByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadByte. API Response Connector: <%u>\n", data);

        /**********SFPReadWord()**********/

        printf("Negitive testing with invalid register address 255 in SFPReadWord\n");
        status = SFPReadWord(device_address1, 255, &data1);
        if (status != 0)
            printf("ERROR: SFPReadWord Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadWord. API Response Word: (0x%02x)\n",data1);
        status = SFPReadWord(device_address1, 96, &data1); // Internally measured module temperature. Temp MSB=96, LSB=97
        float T = ((int16_t)((data1 & 0xFF) << 8 | (data1 & 0xFF00) >> 8)) / 256.0;
        if (status != 0)
            printf("ERROR: SFPReadWord Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadWord. API Response Temp: <%fC> (0x%04x)\n", T, data1);
        status = SFPReadWord(device_address1, 98, &data1);
        long int tmp_vcc = (data1 & 0xFF)*256 + (data1&0xFF00)/256;
        vcc  = tmp_vcc*100e-6;
        if (status != 0)
            printf("ERROR: SFPReadWord Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadWord. API Response Vcc: <%fC> (0x%02x)\n",vcc,tmp_vcc);
        uint16_t tx_data;
        status = SFPReadWord(device_address1, 102, &tx_data);
        long int tmp_tx = (tx_data & 0xFF)*256 + (tx_data&0xFF00)/256;
        TxPWR = tmp_tx*1e-4;
        if (status != 0)
            printf("ERROR: SFPReadWord Error no: %d\n", status);
        else
        {   if (TxPWR>0)
                printf("SFP. API:SFPReadWord. API Response TxPWR: <%fC> (0x%02x)\n",10*log10(TxPWR), tmp_tx);
            else
                printf("SFP. API:SFPReadWord. API Response TxPWR: <%fC> (0x%02x)\n", TxPWR, tmp_tx);

        }
        uint16_t rx_data;
        status = SFPReadWord(device_address1, 104, &rx_data);
        long int tmp_rx = (rx_data & 0xFF)*256 + (rx_data&0xFF00)/256;
        RxPWR  = tmp_rx*1e-4;
        if (status != 0)
            printf("ERROR: SFPReadWord Error no: %d\n", status);
        else
        {   if (RxPWR>0)
                printf("SFP. API:SFPReadWord. API Response RxPWR: <%fC> (0x%02x)\n",10*log10(RxPWR), tmp_rx);
            else
                printf("SFP. API:SFPReadWord. API Response RxPWR: <%fC> (0x%02x)\n", RxPWR, tmp_rx);
        }

        /**********SFPPageWrite()**********/
        /*for (i = 0; i< 8; i++)
        {   page_write_data2[i] = 0xcd;
        }*/
        int page_write_n1 = 8;
        uint8_t page_write_data2[8] = {0xaa, 0xcd, 0xcc, 0xdd, 0xfa, 0xfe, 0xee, 0xbb};
        uint8_t page_write_read_data2[63];

        status = SFPPageWrite(device_address1, page_write_add, page_write_n1, page_write_data2);
        if (status == 0)
        {   printf("SFP. API:SFPPageWrite. API Response %d\n", status);
            for(i = 0; i<page_write_n1; i++)
            {   printf("Page Write element [%d]: %x>\n", i+1, page_write_data2[i]);
            }
        }
        else
            printf("ERROR: SFPPageWrite. Error no: %d\n", status);
        if(g_b_addDelay)
            usleep(100000);
        status = SFPPageRead(device_address1, page_write_add, 63, page_write_read_data2);
        if (status == 0)
        {   printf("SFP. API:SFPPageRead. API Response %d\n", status);
            for(i = 0; i<63; i++)
            {   printf("Page Read Element [%d]: %x>\n", i+1, page_write_read_data2[i]);
            }
        }
        else
            printf("ERROR: SFPPageRead. Error no: %d\n", status);

        status = SFPPageWrite(device_address1, page_write_add, page_write_n, page_write_data);
        printf("SFP. API:SFPPageWrite. API Response %d\n", status);
        for(i = 0; i<page_write_n; i++)
        {   printf("Page Write element [%d]: %x>\n", i+1, page_write_data[i]);
        }
        if(g_b_addDelay)
            usleep(100000);
        status = SFPPageRead(device_address1, page_write_add, page_write_n, page_write_read_data);
        printf("SFP. API:SFPPageRead. API Response %d\n", status);
        for(i = 0; i<page_write_n; i++)
        {   printf("Page Read Element [%d]: %x>\n", i+1, page_write_read_data[i]);
        }

        /**********SFPPageRead()**********/
        printf("Negitive Testing with invalid page length 256 in SFPPageRead\n");
        status = SFPPageRead(device_address, page_reg_add_vn, 256, page_data);
        page_data[17] = '\0';
        if (status != 0)
            printf("ERROR: SFPPageRead Error no: %d\n", status);
        else
            printf("SFP. API:SFPPageRead. API Response Vendor Name: <%s> \n", page_data);

        if(g_b_addDelay)
            usleep(50000);

        status = SFPPageRead(device_address, page_reg_add_vn, n, page_data);
        page_data[17] = '\0';
        if (status != 0)
            printf("ERROR: SFPPageRead Error no: %d\n", status);
        else
            printf("SFP. API:SFPPageRead. API Response Vendor Name: <%s> \n", page_data);

        if(g_b_addDelay)
            usleep(50000);

        status = SFPPageRead(device_address, page_reg_add_pn, n, page_data);
        page_data[17] = '\0';
        if (status != 0)
            printf("ERROR: SFPPageRead Error no: %d\n", status);
        else
            printf("SFP. API:SFPPageRead. API Response Part Number: <%s> \n", page_data);

        if(g_b_addDelay)
            usleep(50000);

        status = SFPPageRead(device_address, page_reg_add_sn, n, page_data);
        page_data[17] = '\0';
        if (status != 0)
            printf("ERROR: SFPPageRead Error no: %d\n", status);
        else
            printf("SFP. API:SFPPageRead. API Response Serial Number: <%s> \n", page_data);
        if(g_b_addDelay)
            usleep(50000);

        /**********SFPWriteByte()**********/

        status = SFPWriteByte(device_address1, byte123, 0x00);
        if (status != 0)
            printf("ERROR: SFPWriteByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPWriteByte. API Response writting password byte at %d: <0x%02x> \n", byte123, 0x00);

        status = SFPWriteByte(device_address1, byte124, 0x00);
        if (status != 0)
            printf("ERROR: SFPWriteByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPWriteByte. API Response writting password byte at %d: <0x%02x> \n", byte124, 0x00);
        status = SFPWriteByte(device_address1, byte125, 0x00);
        if (status != 0)
            printf("ERROR: SFPWriteByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPWriteByte. API Response writting password byte at %d: <0x%02x> \n", byte125, 0x00);
        status = SFPWriteByte(device_address1, byte126, 0x00);
        if (status != 0)
            printf("ERROR: SFPWriteByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPWriteByte. API Response writting password byte at %d: <0x%02x> \n", byte126, 0x00);

        status = SFPWriteByte(device_address1, byte127, 0x00);
        if (status != 0)
            printf("ERROR: SFPWriteByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPWriteByte. API Response writting page select byte at %d: <0x%02x> \n", byte127, 0x00);

        if(g_b_addDelay)
            usleep(100000);

        printf("**** writting data to EEPROM **** \n");
        uint8_t byte_val = 0x87;
        status = SFPWriteByte(device_address1, byte128, byte_val);
        if (status != 0)
            printf("ERROR: SFPWriteByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPWriteByte. API Response writting byte at %d: <0x%02x> \n", byte128, byte_val);

        if(g_b_addDelay)
            usleep(100000);

        status = SFPReadByte(device_address1, byte128, &data);
        if (status != 0)
            printf("ERROR: SFPReadByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadByte. API Response reading byte at %d: <0x%02x> \n", byte128, data);

        /**********SFPWriteWord**********/
        printf("Negitive Testing with invalid register address 255 in SFPWriteWord\n");
        uint16_t word_data = 0x5467;
        status = SFPWriteWord(device_address1, 0xFF, word_data);
        if (status != 0)
            printf("ERROR: SFPWriteWord Error no: %d\n", status);
        else
            printf("SFP. API:SFPWriteWord. API Response writting word at %d: <0x%02x> \n", 0xFF, word_data);

        if(g_b_addDelay)
            usleep(100000);

        uint8_t byte254 = 0x34;
        status = SFPReadByte(device_address1, byte254, &data);
        if (status != 0)
            printf("ERROR: SFPReadByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadByte. API Response reading byte at %d: <0x%02x> \n", byte254, data);
        uint8_t byte255 = 0xFE;
        status = SFPReadByte(device_address1, byte255, &data);
        if (status != 0)
            printf("ERROR: SFPReadByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadByte. API Response reading byte at %d: <0x%02x> \n", byte255, data);


        uint16_t word_val = 0x1234;
        status = SFPWriteWord(device_address1, byte128, word_val);
        if (status != 0)
            printf("ERROR: SFPWriteWord Error no: %d\n", status);
        else
            printf("SFP. API:SFPWriteWord. API Response writting word at %d: <0x%02x> \n", byte128, word_val);

        if(g_b_addDelay)
            usleep(100000);

        status = SFPReadWord(device_address1, byte128, &data1);
        if (status != 0)
            printf("ERROR: SFPReadWord Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadWord. API Response reading word at %d: <0x%02x> \n", byte128, data1);

        status = SFPReadByte(device_address1, byte128, &data);
        if (status != 0)
            printf("ERROR: SFPReadByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadByte. API Response reading byte at %d: <0x%02x> \n", byte128, data);
        status = SFPReadByte(device_address1, byte129, &data);
        if (status != 0)
            printf("ERROR: SFPReadByte Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadByte. API Response reading byte at %d: <0x%02x> \n", byte129, data);


        bool mode_abs = 0;
        status = SFPReadModABS(&mode_abs);
        if (status != 0)
            printf("ERROR: SFPReadModABS Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadModABS. Response Mode ABS: %d\n", mode_abs);

        bool tx_fault = 0;
        status = SFPReadTxFault(&tx_fault);
        if (status != 0)
            printf("ERROR: SFPReadTxFault Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadTxFault. Response Tx fault: %d\n", tx_fault);

        bool los = 0;
        status = SFPReadLOS(&los);
        if (status != 0)
            printf("ERROR: SFPReadLOS Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadLOS. Response LOS: %d\n", los);

        bool tx_disable = 0;
        status = SFPReadTxDisable(&tx_disable);
        if (status != 0)
            printf("ERROR: SFPReadTxDisable Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadTxDisable. Response Tx Disable: %d\n", tx_disable);

        bool tx_status = 1;
        status = SFPSetTxDisable(tx_status);
        if (status != 0)
            printf("ERROR: SFPSetTxDisable Error no: %d\n", status);
        else
            printf("SFP. API:SFPSetTxDisable. Response Tx status: %d\n", tx_status);

        bool tx_disable1 = 0;
        status = SFPReadTxDisable(&tx_disable1);
        if (status != 0)
            printf("ERROR: SFPReadTxDisable Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadTxDisable. Response Tx Disable: %d\n", tx_disable1);

        tx_status = 0;
        status = SFPSetTxDisable(tx_status);
        if (status != 0)
            printf("ERROR: SFPSetTxDisable Error no: %d\n", status);
        else
            printf("SFP. API:SFPSetTxDisable. Response Tx status: %d\n", tx_status);

        bool tx_disable2 = 0;
        status = SFPReadTxDisable(&tx_disable2);
        if (status != 0)
            printf("ERROR: SFPReadTxDisable Error no: %d\n", status);
        else
            printf("SFP. API:SFPReadTxDisable. Response Tx Disable: %d\n", tx_disable2);
    }
    /**********SFPInterfaceDeInit()**********/
    status = SFPInterfaceDeInit();
    if (status != 0)
        printf("ERROR: SFPdeinit Error no: %d\n", status);
    else
        printf("SFP Deinitialized Successfully\n");
    return;
}

/* This function calls i2cinterface Library API */
void _sfp_fw(char *i_c_filename)
{   int status = 0;

    /**********SFPInterfaceInit()**********/
    status = SFPInterfaceInit();
    if (status != 0)
        printf("ERROR: SFPinit Error no: %d\n", status);
    else
        printf("SFP Initialized Successfully\n");

    printf("TSFP FW Upgrade Process Started. it will take approx. ~6 Minutes.\n");
    printf("Filename:%s\n", i_c_filename);
    time_t begin = time(NULL);
    status = SFPFWUpgrade(i_c_filename);
    time_t end = time(NULL);
    printf("Time elpased is %d seconds\n", (end - begin));
    printf("SFPFWUpgrade status:%d\n", status);

    /**********SFPInterfaceDeInit()**********/
    status = SFPInterfaceDeInit();
    if (status != 0)
        printf("ERROR: SFPdeinit Error no: %d\n", status);
    else
        printf("SFP Deinitialized Successfully\n");
}

void _op_sw_en(void)
{   int status = -1;

    status = enable_op_sw();
    if(status)
    {   printf("Optical Switch Enable Failed. Returned:%d", status);
    }
    else
    {   printf("Optical switch Enabled");
    }
}

void _diagdata(void)
{   int status = -1;
    T_DIAG diag;
    int loop;
    char * channel_des[8] = {"TIA OUT    ", "Ref Voltage", "Current_PL ", "+3v3       ", "VAdaptor   ", "Pack_Load  ", "+5V        ", "12V_CTM    "};

    status = GetDiagnosticsData(&diag);
    if (status == 0)
    {   printf("API GetDiagnostics:%d\n", status);
        printf("Conf Reg: %x\n", diag.conf_reg);
        printf("Mfg ID: %x\n", diag.mfg_id);
        printf("Rev ID: %x\n", diag.rev_id);
        printf("Conv Rate Reg: %x\n", diag.conv_rate);
        printf("Ch Disable Reg: %x\n", diag.ch_disable_reg);
        printf("Status Reg: %x\n", diag.status_reg);
        printf("Temp: %fC\n", diag.temp);
        for (loop = 0; loop < 8; loop++)
        {   printf("Channel[%d]: %s: %f\n", loop, channel_des[loop], diag.channel[loop]);
        }
    }
    else
    {   printf("Error: GetDiagnosticsData Error No: %d\n", status);
    }

    return ;
}
void _led(int count)
{   int status = LEDInit();
    if(status != LED_SUCCESS)
    {   printf("Failed to Initialize Error <%d>\n", status);

    }

    while(--count >= 0)
    {   /** LED Controller*/
        status = LEDEnableController(1);
        if(status != LED_SUCCESS)
        {   printf("Failed to Enable the LED Controller Error <%d>\n", status);

        }
        else
        {   printf("LED Controller Enabled\n");
        }

        //Turn on all LEDs white
        printf("LED :Turn on all LEDs white \n");
        status = LEDBrightness(LED_ID_ALL, 255, 255, 255);
        if(status != LED_SUCCESS)
        {   printf("Failed to set LED Brightness Error <%d>\n", status);

        }
        if(g_b_addDelay)
            usleep(500000);

        //Turn on all LEDs RED
        printf("LED :Turn on all LEDs RED \n");
        status = LEDBrightness(LED_ID_ALL, 255, 0, 0);
        if(status != LED_SUCCESS)
        {   printf("Failed to set LED Brightness Error <%d>\n", status);

        }
        if(g_b_addDelay)
            usleep(500000);

        //Turn on all LEDs BLUE
        printf("LED :Turn on all LEDs BLUE \n");
        status = LEDBrightness(LED_ID_ALL, 0, 0, 255);
        if(status != LED_SUCCESS)
        {   printf("Failed to set LED Brightness Error <%d>\n", status);

        }
        if(g_b_addDelay)
            usleep(500000);

        //Turn on all LEDs GREEN
        printf("LED :Turn on all LEDs GREEN \n");
        status = LEDBrightness(LED_ID_ALL, 0, 255, 0);
        if(status != LED_SUCCESS)
        {   printf("Failed to set LED Brightness Error <%d>\n", status);

        }
        if(g_b_addDelay)
            usleep(500000);

        //Turn on all LEDs YELLOW
        printf("LED :Turn on all LEDs YELLOW \n");
        status = LEDBrightness(LED_ID_ALL, 255, 255, 0);
        if(status != LED_SUCCESS)
        {   printf("Failed to set LED Brightness Error <%d>\n", status);

        }
        if(g_b_addDelay)
            usleep(500000);

        //Turn off all LEDs
        printf("LED :Turn off all LEDs \n");
        status = LEDBrightness(LED_ID_ALL, 0, 0, 0);
        if(status != LED_SUCCESS)
        {   printf("Failed to set LED Brightness Error <%d>\n", status);

        }
        if(g_b_addDelay)
            usleep(500000);

        //Turn Battery LED YELLOW
        printf("LED :Turn battery LED YELLOW \n");
        status = LEDBrightness(LED_ID_BAT, 255, 255, 0);
        if(status != LED_SUCCESS)
        {   printf("Failed to set LED Brightness Error <%d>\n", status);

        }
        if(g_b_addDelay)
            usleep(500000);

        //Turn power LED GREEN
        printf("LED :Turn power LED GREEN \n");
        status = LEDBrightness(LED_ID_PWR, 0, 255, 0);
        if(status != LED_SUCCESS)
        {   printf("Failed to set LED Brightness Error <%d>\n", status);

        }
        if(g_b_addDelay)
            usleep(500000);

        //Turn power WIFI LED BLUE
        printf("LED :Turn power WIFI LED BLUE \n");
        status = LEDBrightness(LED_ID_WIFI, 0, 0, 255);
        if(status != LED_SUCCESS)
        {   printf("Failed to set LED Brightness Error <%d>\n", status);

        }
        if(g_b_addDelay)
            usleep(500000);

        //Turn ALM LED RED
        printf("LED :Turn ALM LED RED \n");
        status = LEDBrightness(LED_ID_ALM, 255, 0, 0);
        if(status != LED_SUCCESS)
        {   printf("Failed to set LED Brightness Error <%d>\n", status);

        }
        if(g_b_addDelay)
            usleep(500000);
    }
    /** Deinit the LED*/
    LEDDeInit();

    return ;
}

void _fslsetup(void)
{   /* Initialize LED */
    int status = LEDInit();
    if(status != LED_SUCCESS)
    {   printf("Failed to Initialize Error <%d>\n", status);

    }

    status = FreescaleSetup();
    if(status != LED_SUCCESS)
    {   printf("Failed to make iic slave enter to the correct mode <%d>\n", status);

    }

    /* Deinitialize LED */
    LEDDeInit();

    return ;
}

void _fslrst(bool i_b_verCheck)
{   int status = 0;

    /* Check Freescale Communication is not broken */
    if (i_b_verCheck)
    {   char ver[32] = {0};

        /**********Freescale Version()**********/
        status = FslVerCmd(ver);
        if (status != 0)
        {   printf("ERROR: Freescale version checking Error no: %d\n", status);
        }
        else
        {   printf("Freescale version checking... Ver:<%s>\n", ver);
        }
        if (ver[0] != '\0')
        {   return ;
        }
    }

    /* Initialize LED */
    status = LEDInit();
    if(status != LED_SUCCESS)
    {   printf("Failed to Initialize Error <%d>\n", status);

    }

    /* Reset Freescale */
    status = ResetPowerContolLogic();
    if(status != FSL_RST_SUCCESS)
    {   printf("Failed Reset GPIO <%d>\n", status);

    }
    else
    {   printf("Freescale Reset Sucessfully\n");
    }

    /* Deinitialize LED */
    LEDDeInit();

    return ;
}

void _osa_cal()
{   int l_i_status =0;
    int type = 1;
    float sboc = 0;
    float sbtc = 0;
    do
    {   /** OCM Initialization **/
        l_i_status = OcmInterfaceInit();
        if (l_i_status != 0)
        {   printf("ERROR: OCMinit Error no: %d\n", l_i_status);
            break ;
        }
        else
        {   printf("OCM Initialized Successfully\n");
        }

        /** otdr Initialization **/
        if ((l_i_status = OtdrInterfaceInit(option)) != 0)
        {   printf("ERROR: otdr Init Error: %d\n", l_i_status);
            break ;
        }
        else
        {   printf("OTDR Initialized Successfully\n");
        }

        /** sfp Initialization **/
        l_i_status = SFPInterfaceInit();
        if (l_i_status != 0)
        {   printf("ERROR: SFPinit Error no: %d\n", l_i_status);
            break ;
        }
        else
        {   printf("SFP Initialized Successfully\n");
        }


        if (l_i_status = osaCalibration(type,&sboc,&sbtc))
        {   printf("osaCalibration Failed. Return Value:%d\n", l_i_status);
        }
        else
        {   printf("osaCalibration Success: New SBOC: %f New SBTC : %f\n",sboc,sbtc);
        }

    }
    while(0);
    /** OCM De-Initialization **/
    l_i_status = OcmInterfaceDeInit();
    if (l_i_status != 0)
        printf("ERROR: OCMdeinit Error no: %d\n", l_i_status);
    else
        printf("OCM Deinitialized Successfully\n");

    /** Otdr De-Initialization **/
    l_i_status = l_i_status = OtdrInterfaceDeInit();
    if (l_i_status != 0)
        printf("ERROR: OTDRdeinit Error no: %d\n", l_i_status);
    else
        printf("OTDR Deinitialized Successfully\n");

    /** SFP De-Initialization **/
    l_i_status = SFPInterfaceDeInit();
    if (l_i_status != 0)
        printf("ERROR: SFPdeinit Error no: %d\n", l_i_status);
    else
        printf("SFP Deinitialized Successfully\n");
}


void _batterymanagement(int count, int api_no)
{   int l_i_status;
    float l_f_tmp = 0;
    T_DEEP_SLEEP_CONFIG cfg;

    /**********BatteryAPIInit()**********/
    l_i_status = BatteryAPIInit();
    if (l_i_status != 0)
    {   printf("ERROR: BatteryAPIInit Error no: %d\n", l_i_status);
        return;
    }
    else
        printf("BatteryAPI Initialized Successfully\n");

    device_deinit_flag = 9;

    while(--count >= 0)
    {

        /**********GetBatteryChargeCycle()**********/
        uint16_t l_ui16_tmp = 0;
        if((api_no == 1) || (api_no < 1) ||  (api_no > 8))
        {   l_i_status = GetBatteryChargeCycle(&l_ui16_tmp);
            if (l_i_status != 0)
                printf("ERROR: GetBatteryChargeCycle Error no: %d\n", l_i_status);
            else
                printf("BatteryAPI GetBatteryChargeCycle: <%hu>\n", l_ui16_tmp);
        }

        if((api_no == 2) || (api_no < 1) ||  (api_no > 8))
        {   /**********GetBatteryChargerTemp()**********/
            l_f_tmp = 0;
            l_i_status = GetBatteryChargerTemp(&l_f_tmp);
            if (l_i_status != 0)
                printf("ERROR: GetBatteryChargerTemp Error no: %d\n", l_i_status);
            else
                printf("BatteryAPI GetBatteryChargerTemp: <%.2fC>\n", l_f_tmp);
        }

        if((api_no == 3) || (api_no < 1) ||  (api_no > 8))
        {   /**********GetBatteryVoltage()**********/
            l_f_tmp = 0;
            l_i_status = GetBatteryVoltage(&l_f_tmp);
            if (l_i_status != 0)
                printf("ERROR: GetBatteryVoltage Error no: %d\n", l_i_status);
            else
                printf("BatteryAPI GetBatteryVoltage: <%.2fV>\n", l_f_tmp);
        }

        if((api_no == 4) || (api_no < 1) ||  (api_no > 8))
        {   /**********GetBatteryCurrent()**********/
            l_f_tmp = 0;
            l_i_status = GetBatteryCurrent(&l_f_tmp);
            if (l_i_status != 0)
                printf("ERROR: GetBatteryCurrent Error no: %d\n", l_i_status);
            else
                printf("BatteryAPI GetBatteryCurrent: <%.2fA>\n", l_f_tmp);
        }

        if((api_no == 5) || (api_no < 1) ||  (api_no > 8))
        {   /**********GetBatteryChargePercentage()**********/
            l_f_tmp = 0;
            l_i_status = GetBatteryChargePercentage(&l_f_tmp);
            if (l_i_status != 0)
                printf("ERROR: GetBatteryChargePercentage Error no: %d\n", l_i_status);
            else
                printf("BatteryAPI GetBatteryChargePercentage: <%.2f%%>\n", l_f_tmp);
        }

        if((api_no == 6) || (api_no < 1) ||  (api_no > 8))
        {   /**********GetBatteryChargerVoltage()**********/
            l_f_tmp = 0;
            l_i_status = GetBatteryChargerVoltage(&l_f_tmp);
            if (l_i_status != 0)
                printf("ERROR: GetBatteryChargerVoltage Error no: %d\n", l_i_status);
            else
                printf("BatteryAPI GetBatteryChargerVoltage: <%.2fV>\n", l_f_tmp);
        }

        if((api_no == 7) || (api_no < 1) ||  (api_no > 8))
        {   /**********GetBatteryDeepSleepConfig()**********/
            memset(&cfg, 0, sizeof(cfg));            
            l_i_status = GetBatteryDeepSleepConfig(&cfg);
            if (l_i_status != 0)
                printf("ERROR: GetBatteryDeepSleepConfig Error no: %d\n", l_i_status);
            else
                printf("BatteryAPI GetBatteryDeepSleepConfig: <mode: %s, days: %d, percentage: %d>\n", cfg.mode == DEEP_SLEEP_ENABLED ? "Enabled" : "Disabled", cfg.days, cfg.percentage);
        }

        if((api_no == 8))
        {   /**********SetBatteryDeepSleepConfig()**********/
            do{
                memset(&cfg, 0, sizeof(cfg));
                printf("\nEnable Shipping Mode?\n");
                printf("0 = Disable\n");
                printf("1 = Enable\n");
                printf("Enter Mode : ");
                scanf("%d", &cfg.mode);

                if (cfg.mode == DEEP_SLEEP_ENABLED)
                {
                    printf("\nEnter Days (1-30): ");
                    scanf("%d", &cfg.days);              

                    printf("Enter Percentage (0-80): ");
                    scanf("%d", &cfg.percentage);
                }

                l_i_status = SetBatteryDeepSleepConfig(&cfg);

                if (l_i_status == BATTERYAPI_SUCCESS)
                    printf("\nShipping Mode Configuration Updated Successfully.\n");
                else
                    printf("\nSetBatteryDeepSleepConfig Failed (%d)\n", l_i_status);
            }while(0);
        }
    }
    /**********BatteryAPIDeInit()**********/
    l_i_status = BatteryAPIDeInit();
    if (l_i_status != 0)
        printf("ERROR: BatteryAPIDeInit Error no: %d\n", l_i_status);
    else
        printf("BatteryAPI Deinitialized Successfully\n");

    return;
}

/****************************************************************** POWER MANAGEMENT SECTION : START **********************************************************/
void SFPMenu()
{   char l_i_option = -1;
    int l_i_Status = -1;

    do
    {   printf("\nselect option :\n\n");
        printf("\t1 SFP ON/OFF status (get)\n");
        printf("\t2 SFP OFF\n");
        printf("\t3 SFP ON\n");
        printf("\t0 EXIT \n");


        scanf(" %c", &l_i_option);
        switch(l_i_option)
        {   case '0':
                break;

            case '1':
                l_i_Status = getSFPStatus();
                switch(l_i_Status)
                {   case POWERMGT_DEVICE_ON:
                        printf("SFP is POWER ON\n");
                        break;
                    case POWERMGT_DEVICE_OFF:
                        printf("SFP is POWER OFF\n");
                        break;
                    case POWERMGT_DEVICE_POWERING_UP:
                        printf("SFP is POWERRING UP\n");
                        break;

                    case FREESCALE_FW_UPGRADE_INPROGRESS:
                        printf("Freescale Upgrade in Progress, %d\n", l_i_Status);
                        break;

                    case POWERMGT_DEVICE_FAILED_TO_INITIALIZE:
                        printf("SFP failed Initialize, %d\n", l_i_Status);
                        break;

                    default :
                        printf("ERROR: SFP invalid state %d\n", l_i_Status);
                }
                break;

            case '2':
                printf("sfp status %d\n",SFPPower(OFF));
                break;

            case '3':
                printf("sfp status %d\n", SFPPower(ON));
                break;
            default:
                printf("select valid option\n");
                continue;

        }
    }
    while(l_i_option != '0');
}

void CTMMenu()
{   char l_i_option = -1;
    int l_i_Status = -1;
    bool io_b_otdrstatus = 0;

    do
    {   printf("\nselect option :\n\n");
        printf("\t1 CTM ON/OFF status (get)\n");
        printf("\t2 CTM OFF\n");
        printf("\t3 CTM ON\n");
        printf("\t0 EXIT \n");

        scanf(" %c", &l_i_option);
        switch(l_i_option)
        {

            case '0':
                break;

            case '1':
                l_i_Status = getOCMStatus();
                switch(l_i_Status)
                {   case POWERMGT_DEVICE_ON:
                        printf("CTM is POWER ON\n");
                        break;
                    case POWERMGT_DEVICE_OFF:
                        printf("CTM is POWER OFF\n");
                        break;
                    case POWERMGT_DEVICE_POWERING_UP:
                        printf("CTM is POWERRING UP\n");
                        break;
                    default :
                        printf("ERROR : CTM invalid State %d\n", l_i_Status);
                }
                break;

            case '2':
                printf("OCM status %d\n", OCMPower(OFF));
                break;

            case '3':
                printf("OCM status %d\n", OCMPower(ON));
                break;
            default:
                printf("select valid option\n");
                continue;
        }
    }
    while(l_i_option != '0');
}

void OTDRMenu()
{   char l_i_option = -1;
    int l_i_Status = -1;
    bool o_b_otdrstatus = 0;

    do
    {   printf("\nselect option :\n\n");
        printf("\t1 OTDR ON/OFF status (get)\n");
        printf("\t2 OTDR OFF\n");
        printf("\t3 OTDR ON\n");
        printf("\t4 OTDR FORCE ON\n");
        printf("\t0 EXIT \n");

        scanf(" %c", &l_i_option);
        switch(l_i_option)
        {   case '0':
                break;

            case '1':
                l_i_Status = getOTDRStatus();
                switch(l_i_Status)
                {   case POWERMGT_DEVICE_ON:
                        printf("OTDR is POWER ON, %d\n", l_i_Status);
                        break;
                    case POWERMGT_DEVICE_OFF:
                        printf("OTDR is POWER OFF, %d\n", l_i_Status);
                        break;
                    case POWERMGT_DEVICE_POWERING_UP:
                        printf("OTDR is POWERRING UP, %d\n", l_i_Status);
                        break;
                    case FREESCALE_FW_UPGRADE_INPROGRESS:
                        printf("Freescale Upgrade in Progress, %d\n", l_i_Status);
                        break;
                    case POWERMGT_DEVICE_FAILED_TO_INITIALIZE:
                        printf("OTDR failed Initialize, %d\n", l_i_Status);
                        break;

                    default :
                        printf("ERROR: OTDR invalid state %d\n", l_i_Status);
                }
                break;

            case '2':
                printf( "OTDR OFF Status %d\n", OTDRPower(OFF));
                break;

            case '3':
                printf( "OTDR ON Status %d\n", OTDRPower(ON));
                break;
            case '4':
                printf( "OTDR FORCE ON Status %d\n", OTDRPowerForce(ON));
                break;
            default:
                printf("select valid option\n");
                continue;
        }

    }
    while(l_i_option != '0');
}

void powermanagementMenu()
{   char l_i_option = -1;
    int l_i_Status = -1;

    /** otdr Initialization **/
    if ((l_i_Status = OtdrInterfaceInit(option)) != 0)
    {   printf("ERROR: otdr Init Error: %d\n", l_i_Status);
        return ;
    }
    else
    {   printf("OTDR Initialized Successfully\n");
    }

    /** sfp Initialization **/
    l_i_Status = SFPInterfaceInit();
    if (l_i_Status != 0)
    {   printf("ERROR: SFPinit Error no: %d\n", l_i_Status);
        return ;
    }
    else
    {   printf("SFP Initialized Successfully\n");
    }
    /** OCM Initialization **/
    l_i_Status = OcmInterfaceInit();
    if (l_i_Status != 0)
    {   printf("ERROR: OCMinit Error no: %d\n", l_i_Status);
        return ;
    }
    else
    {   printf("OCM Initialized Successfully\n");
    }

    do
    {   printf("**************************");
        printf("\nselcet the device\n");
        printf("**************************\n");
        printf("\t1.OTDR\n");
        printf("\t2.CTM\n");
        printf("\t3.SFP\n");
        printf("\t0.EXIT\n");

        scanf(" %c", &l_i_option);
        switch(l_i_option)
        {   case '0':
                break;

            case '1' :
                OTDRMenu();
                break;

            case '2':
                CTMMenu();
                break;

            case '3':
                SFPMenu();
                break;

            default:
                printf("select valid option\n");
                continue;
        }
    }
    while(l_i_option != '0');

    /** Otdr De-Initialization **/
    l_i_Status = l_i_Status = OtdrInterfaceDeInit();
    if (l_i_Status != 0)
        printf("ERROR: OTDRdeinit Error no: %d\n", l_i_Status);
    else
        printf("OTDR Deinitialized Successfully\n");

    /** SFP De-Initialization **/
    l_i_Status = SFPInterfaceDeInit();
    if (l_i_Status != 0)
        printf("ERROR: SFPdeinit Error no: %d\n", l_i_Status);
    else
        printf("SFP Deinitialized Successfully\n");

    /** OCM De-Initialization **/
    l_i_Status = OcmInterfaceDeInit();
    if (l_i_Status != 0)
        printf("ERROR: OCMdeinit Error no: %d\n", l_i_Status);
    else
        printf("OCM Deinitialized Successfully\n");
}
//POWER MANAGEMENT MAIN FUNCTION:
void _powermgt()
{   powermanagementMenu();
}
/****************************************************************** POWER MANAGEMENT SECTION : END **********************************************************/

void _powerOtdrSeq()
{   double get_thr = -1;
    double thr = 0.30;
    int ret_fn = -1;
    int times = 0;
    T_OTDRSTATUS get_status= {0};
    struct timeval  l_x_tStampFirst = {0};
    struct timeval  l_x_tStampFinal = {0};

    do
    {   /** otdr Initialization **/
        if ((ret_fn = OtdrInterfaceInit(option)) != 0)
        {   printf("ERROR: otdr Init Error: %d\n", ret_fn);
            break;
        }
        else
        {   printf("OTDR Initialized Successfully.\n");
        }

#ifdef ADD_DELAY
        usleep(10000);
#endif

        printf("Powering up the OTDR...\n");
        /** Get the First Timestamp */
        gettimeofday(&l_x_tStampFirst, NULL);
        ret_fn = OTDRPower(ON);
        if (ret_fn)
        {   printf("OTDRPower(ON) failed. Error:%d\n", ret_fn);
            break;
        }

        while(1)
        {   ret_fn = getOTDRStatus();
            if(POWERMGT_DEVICE_ON == ret_fn)
            {   /** Get the Final Timestamp */
                gettimeofday(&l_x_tStampFinal, NULL);
                printf("OTDR Power Up Successfully :%d, in: [%f]Sec \n", ret_fn,
                       (double) (l_x_tStampFinal.tv_usec - l_x_tStampFirst.tv_usec) / 1000000 +
                       (double) (l_x_tStampFinal.tv_sec - l_x_tStampFirst.tv_sec));
                break;
            }
            else if (POWERMGT_DEVICE_OFF == ret_fn)
            {   /** Get the Final Timestamp */
                gettimeofday(&l_x_tStampFinal, NULL);
                printf("Issue in Powering on the OTDR:%d. Terminating the execution in :[%f]Sec\n", ret_fn,
                       (double) (l_x_tStampFinal.tv_usec - l_x_tStampFirst.tv_usec) / 1000000 +
                       (double) (l_x_tStampFinal.tv_sec - l_x_tStampFirst.tv_sec));
                if (0 != (ret_fn = OtdrInterfaceDeInit()))
                {   printf("ERROR: Otdr DeInit Failed \n");
                }
                return;
            }
            usleep(100000);
        }

        /** Clear the both structure */
        memset(&l_x_tStampFirst, 0x00, sizeof(struct timeval));
        memset(&l_x_tStampFinal, 0x00, sizeof(struct timeval));

#ifdef ADD_DELAY
        usleep(10000);
#endif
        while (1)
        {   ret_fn = OtdrStatus(&get_status);
            if (ret_fn)
            {   printf("OtdrStatus failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrStatus: <%d>\n", get_status.currentState);
                if (get_status.currentState == OTDR_STATE_CAPTURE)
                {   printf("TEST APP: OtdrStatus Capture Percentage: <%f>\n", get_status.percentage);
                }
                if (get_status.currentState == OTDR_STATE_IDLE)
                {   break;
                }
            }
            sleep(1);
        }

        float thr = 0.3;
        ret_fn = OtdrSetEventLossThreshold(thr);
        if (ret_fn)
        {   printf("OtdrSetEventLossThresold failed. Error:%d\n", ret_fn);
        }
        else
        {   printf("TEST APP: OtdrSetEventLossThresold : <%f>\n", thr);
        }

#ifdef ADD_DELAY
        usleep(10000);
#endif

        float ms = 1;
        ret_fn = OtdrSetMeasureTime(ms);
        if (ret_fn)
        {   printf("OtdrSetMeasureTime failed. Error:%d\n", ret_fn);
        }
        else
        {   printf("TEST APP: OtdrSetMeasureTime : <%f>\n", ms);
        }

#ifdef ADD_DELAY
        usleep(10000);
#endif

        float pw = 2;
        ret_fn = OtdrSetPulseWidth(pw);
        if (ret_fn)
        {   printf("OtdrSetPulseWidth failed. Error:%d\n", ret_fn);
        }
        else
        {   printf("TEST APP: OtdrSetPulseWidth : <%f>\n", pw);
        }
#ifdef ADD_DELAY
        usleep(10000);
#endif
        float res = 8;
        ret_fn = OtdrSetResolution(res);
        if (ret_fn)
        {   printf("OtdrSetResolution failed. Error:%d\n", ret_fn);
        }
        else
        {   printf("TEST APP: OtdrSetResolution : <%f>\n", res);
        }
#ifdef ADD_DELAY
        usleep(10000);
#endif
        float sl = 30;
        ret_fn = OtdrSetSpanLength(sl);
        if (ret_fn)
        {   printf("OtdrSetSpanLength failed. Error:%d\n", ret_fn);
        }
        else
        {   printf("TEST APP: OtdrSetSpanLength : <%f>\n", sl);
        }
#ifdef ADD_DELAY
        usleep(10000);
#endif
        float ws = 35;
        ret_fn = OtdrSetWindowSize(ws);
        if (ret_fn)
        {   printf("OtdrSetWindowSize failed. Error:%d\n", ret_fn);
        }
        else
        {   printf("TEST APP: OtdrSetWindowSize : <%f>\n", ws);
        }

#ifdef ADD_DELAY
        usleep(10000);
#endif
        float wo = 0;
        ret_fn = OtdrSetWindowOffset(wo);
        if (ret_fn)
        {   printf("OtdrSetWindowOffset failed. Error:%d\n", ret_fn);
        }
        else
        {   printf("TEST APP: OtdrSetWindowOffset : <%f>\n", wo);
        }

        double refractiveIndex = 1.4989623;
        ret_fn = OtdrSetRefractiveIndex(refractiveIndex);
        if (ret_fn)
        {   printf("OtdrSetRefractiveIndex failed. Error:%d\n", ret_fn);
        }
        else
        {   printf("TEST APP: OtdrSetRefractiveIndex : <%.7lf>\n\n", refractiveIndex);
        }

#ifdef ADD_DELAY
        usleep(1000000);
#endif

        while(1)
        {   ret_fn = OtdrRun();
            if (0 != ret_fn)
            {   printf("OtdrRun failed or busy. Error:%d\n", ret_fn);
                sleep(2);
            }
            else
            {   printf("TEST APP: OtdrRun: <%d>\n\n", ret_fn);
                break;
            }
        }

#ifdef ADD_DELAY
        usleep(10000);
#endif

        while (1)
        {   ret_fn = OtdrStatus(&get_status);
            if (ret_fn)
            {   printf("OtdrStatus failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrStatus: <%d>\n", get_status.currentState);
                if (get_status.currentState == OTDR_STATE_CAPTURE)
                {   printf("TEST APP: OtdrStatus Capture Percentage: <%f>\n", get_status.percentage);
                }
                if (get_status.currentState == OTDR_STATE_IDLE)
                {   break;
                }
            }
            usleep(10000);
        }

#ifdef ADD_DELAY
        usleep(10000);
#endif

        T_DUMPTRACE ltrace = { 0 };
        ret_fn = OtdrDumpTraceLinear(&ltrace);
        if (ret_fn)
        {   if (ret_fn == OTDR_NOT_DEFINED)
            {   printf("OtdrDumpTraceLinear trace not defined:%d\n", ret_fn);
            }
            else
            {   printf("OtdrDumpTraceLinear trace failed. Error:%d\n", ret_fn);
            }
        }
        else
        {   printf("\nTEST APP: OtdrDumpTraceLinear trace : <%d>\n", ret_fn);
            printf("trace.points:%u \n", ltrace.points);
            printf("trace.dx:%f \n", ltrace.dx);
            printf("trace.windowOffset:%f \n", ltrace.windowOffset);
            int ltraceCount = 0;
            if(otdrMsgVal == OTDR_DUMP_MSG_ENABLE)
            {   for (ltraceCount = 0; ltraceCount < ltrace.points; ++ltraceCount)
                {   printf("%e\n", ltrace.power[ltraceCount]);
                }
            }
        }
        if (NULL != ltrace.power)
        {   free(ltrace.power);
            ltrace.power = NULL;
        }

#ifdef ADD_DELAY
        usleep(100000);
#endif
        T_DUMPEVENT event_table = {0};
        int itr = 0;
        ret_fn = OtdrDumpEvent(&event_table);
        if (ret_fn)
        {   if (ret_fn == OTDR_NOT_DEFINED)
            {   printf("OtdrDumpEvent(trace not defined:%d\n", ret_fn);
            }
            else
            {   printf("OtdrDumpEvent(trace failed. Error:%d\n", ret_fn);
            }
            printf("\nOtdrDumpEvent(event failed. Error:%d\n", ret_fn);
        }
        else
        {   printf("\n\nTEST APP: OtdrDumpEvent(event : <%d>\n", ret_fn);
            printf("TEST APP: OtdrDumpEventLength(length : <%u>\n\n", event_table.n);

            for (itr = 0; itr < event_table.n; ++itr)
            {   printf("Event%d:\n", itr + 1);
                printf("event_table.e->type:%u  ", ((event_table.e) + itr)->type);
                printf("\nevent_table.e->x_km:%f  ", ((event_table.e) + itr)->x_km);
                printf("\nevent_table.e->loss_u_km:%f  ",
                       ((event_table.e) + itr)->loss_u_km);
                printf("\nevent_table.e->loss_dB:%f  ",
                       ((event_table.e) + itr)->loss_dB);
                printf("\nevent_table.e->c_loss_db:%f  ",
                       ((event_table.e) + itr)->c_loss_dB);
                printf("\nevent_table.e->attn_dB_km:%f  ",
                       ((event_table.e) + itr)->attn_dB_km);
                printf("\nevent_table.e->refl_dB:%f  ",
                       ((event_table.e) + itr)->refl_dB);
                printf("\nevent_table.e->refl_u_km:%f  ",
                       ((event_table.e) + itr)->refl_u_km);
                printf("\n");
            }
        }
        if (NULL != event_table.e)
        {   free(event_table.e);
            event_table.e = NULL;
        }

        ret_fn = OTDRPower(OFF);
        if (ret_fn)
        {   printf("OTDRPower(OFF) failed. Error:%d\n", ret_fn);
            break;
        }

        printf("Starting next iteration in 5 seconds. Press CTRL+C if you want to stop the execution.\n");
        sleep(5);

        while(1)
        {   ret_fn = getOTDRStatus();
            if(POWERMGT_DEVICE_OFF == ret_fn)
            {   printf("OTDR is POWER OFF\n");
                printf("Powering up the OTDR...\n");
                /** Get the First Timestamp */
                gettimeofday(&l_x_tStampFirst, NULL);
                ret_fn = OTDRPower(ON);
                if (ret_fn)
                {   printf("OTDRPower(ON) failed. Error:%d\n", ret_fn);
                    break;
                }
            }
            usleep(10000);

            while(1)
            {   ret_fn = getOTDRStatus();
                if(POWERMGT_DEVICE_ON == ret_fn)
                {   /** Get the Final Timestamp */
                    gettimeofday(&l_x_tStampFinal, NULL);
                    printf("OTDR Power Up Successfully :%d, in: [%f]Sec \n", ret_fn,
                           (double) (l_x_tStampFinal.tv_usec - l_x_tStampFirst.tv_usec) / 1000000 +
                           (double) (l_x_tStampFinal.tv_sec - l_x_tStampFirst.tv_sec));
                    break;
                }
                else if (POWERMGT_DEVICE_OFF == ret_fn)
                {   /** Get the Final Timestamp */
                    gettimeofday(&l_x_tStampFinal, NULL);
                    printf("Issue in Powering on the OTDR:%d. Terminating the execution in :[%f]Sec\n", ret_fn,
                           (double) (l_x_tStampFinal.tv_usec - l_x_tStampFirst.tv_usec) / 1000000 +
                           (double) (l_x_tStampFinal.tv_sec - l_x_tStampFirst.tv_sec));
                    if (0 != (ret_fn = OtdrInterfaceDeInit()))
                    {   printf("ERROR: Otdr DeInit Failed \n");
                    }
                    return;
                }
                usleep(100000);
            }

            /** Clear the both structure */
            memset(&l_x_tStampFirst, 0x00, sizeof(struct timeval));
            memset(&l_x_tStampFinal, 0x00, sizeof(struct timeval));

#ifdef ADD_DELAY
            usleep(10000);
#endif
            memset(&get_status, 0x00, sizeof(T_OTDRSTATUS));
            while (1)
            {   ret_fn = OtdrStatus(&get_status);
                if (ret_fn)
                {   printf("OtdrStatus failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrStatus: <%d>\n", get_status.currentState);
                    if (get_status.currentState == OTDR_STATE_CAPTURE)
                    {   printf("TEST APP: OtdrStatus Capture Percentage: <%f>\n", get_status.percentage);
                    }
                    if (get_status.currentState == OTDR_STATE_IDLE)
                    {   break;
                    }
                }
                sleep(1);
            }

            float thr = 0.3;
            ret_fn = OtdrSetEventLossThreshold(thr);
            if (ret_fn)
            {   printf("OtdrSetEventLossThresold failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrSetEventLossThresold : <%f>\n", thr);
            }

#ifdef ADD_DELAY
            usleep(10000);
#endif

            float ms = 1;
            ret_fn = OtdrSetMeasureTime(ms);
            if (ret_fn)
            {   printf("OtdrSetMeasureTime failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrSetMeasureTime : <%f>\n", ms);
            }

#ifdef ADD_DELAY
            usleep(10000);
#endif

            float pw = 2;
            ret_fn = OtdrSetPulseWidth(pw);
            if (ret_fn)
            {   printf("OtdrSetPulseWidth failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrSetPulseWidth : <%f>\n", pw);
            }
#ifdef ADD_DELAY
            usleep(10000);
#endif
            float res = 8;
            ret_fn = OtdrSetResolution(res);
            if (ret_fn)
            {   printf("OtdrSetResolution failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrSetResolution : <%f>\n", res);
            }
#ifdef ADD_DELAY
            usleep(10000);
#endif
            float sl = 30;
            ret_fn = OtdrSetSpanLength(sl);
            if (ret_fn)
            {   printf("OtdrSetSpanLength failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrSetSpanLength : <%f>\n", sl);
            }
#ifdef ADD_DELAY
            usleep(10000);
#endif
            float ws = 35;
            ret_fn = OtdrSetWindowSize(ws);
            if (ret_fn)
            {   printf("OtdrSetWindowSize failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrSetWindowSize : <%f>\n", ws);
            }

#ifdef ADD_DELAY
            usleep(10000);
#endif
            float wo = 0;
            ret_fn = OtdrSetWindowOffset(wo);
            if (ret_fn)
            {   printf("OtdrSetWindowOffset failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrSetWindowOffset : <%f>\n", wo);
            }

            double refractiveIndex = 1.4989623;
            ret_fn = OtdrSetRefractiveIndex(refractiveIndex);
            if (ret_fn)
            {   printf("OtdrSetRefractiveIndex failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("TEST APP: OtdrSetRefractiveIndex : <%.7lf>\n\n", refractiveIndex);
            }


#ifdef ADD_DELAY
            usleep(1000000);
#endif

            while(1)
            {   ret_fn = OtdrRun();
                if (0 != ret_fn)
                {   printf("OtdrRun failed or busy. Error:%d\n", ret_fn);
                    sleep(2);
                }
                else
                {   printf("TEST APP: OtdrRun: <%d>\n\n", ret_fn);
                    break;
                }
            }

#ifdef ADD_DELAY
            usleep(10000);
#endif

            memset(&get_status, 0x00, sizeof(T_OTDRSTATUS));
            while (1)
            {   ret_fn = OtdrStatus(&get_status);
                if (ret_fn)
                {   printf("OtdrStatus failed. Error:%d\n", ret_fn);
                }
                else
                {   printf("TEST APP: OtdrStatus: <%d>\n", get_status.currentState);
                    if (get_status.currentState == OTDR_STATE_CAPTURE)
                    {   printf("TEST APP: OtdrStatus Capture Percentage: <%f>\n", get_status.percentage);
                    }
                    if (get_status.currentState == OTDR_STATE_IDLE)
                    {   break;
                    }
                }
                usleep(10000);
            }

#ifdef ADD_DELAY
            usleep(10000);
#endif

            memset(&ltrace, 0x00, sizeof(T_DUMPTRACE));
            ret_fn = OtdrDumpTraceLinear(&ltrace);
            if (ret_fn)
            {   if (ret_fn == OTDR_NOT_DEFINED)
                {   printf("OtdrDumpTraceLinear trace not defined:%d\n", ret_fn);
                }
                else
                {   printf("OtdrDumpTraceLinear trace failed. Error:%d\n", ret_fn);
                }
            }
            else
            {   printf("\nTEST APP: OtdrDumpTraceLinear trace : <%d>\n", ret_fn);
                printf("trace.points:%u \n", ltrace.points);
                printf("trace.dx:%f \n", ltrace.dx);
                printf("trace.windowOffset:%f \n", ltrace.windowOffset);
                int ltraceCount = 0;
                if(otdrMsgVal == OTDR_DUMP_MSG_ENABLE)
                {   for (ltraceCount = 0; ltraceCount < ltrace.points; ++ltraceCount)
                    {   printf("%e\n", ltrace.power[ltraceCount]);
                    }
                }
            }
            if (NULL != ltrace.power)
            {   free(ltrace.power);
                ltrace.power = NULL;
            }

#ifdef ADD_DELAY
            usleep(100000);
#endif

            memset(&event_table, 0x00, sizeof(T_DUMPEVENT));
            itr = 0;
            ret_fn = OtdrDumpEvent(&event_table);
            if (ret_fn)
            {   if (ret_fn == OTDR_NOT_DEFINED)
                {   printf("OtdrDumpEvent(trace not defined:%d\n", ret_fn);
                }
                else
                {   printf("OtdrDumpEvent(trace failed. Error:%d\n", ret_fn);
                }
                printf("\nOtdrDumpEvent(event failed. Error:%d\n", ret_fn);
            }
            else
            {   printf("\n\nTEST APP: OtdrDumpEvent(event : <%d>\n", ret_fn);
                printf("TEST APP: OtdrDumpEventLength(length : <%u>\n", event_table.n);

                for (itr = 0; itr < event_table.n; ++itr)
                {   printf("Event%d:\n", itr + 1);
                    printf("\nevent_table.e->type:%u  ", ((event_table.e) + itr)->type);
                    printf("\nevent_table.e->x_km:%f  ", ((event_table.e) + itr)->x_km);
                    printf("\nevent_table.e->loss_u_km:%f  ",
                           ((event_table.e) + itr)->loss_u_km);
                    printf("\nevent_table.e->loss_dB:%f  ",
                           ((event_table.e) + itr)->loss_dB);
                    printf("\nevent_table.e->c_loss_db:%f  ",
                           ((event_table.e) + itr)->c_loss_dB);
                    printf("\nevent_table.e->attn_dB_km:%f  ",
                           ((event_table.e) + itr)->attn_dB_km);
                    printf("\nevent_table.e->refl_dB:%f  ",
                           ((event_table.e) + itr)->refl_dB);
                    printf("\nevent_table.e->refl_u_km:%f  ",
                           ((event_table.e) + itr)->refl_u_km);
                    printf("\n");
                }
            }
            if (NULL != event_table.e)
            {   free(event_table.e);
                event_table.e = NULL;
            }

            ret_fn = OTDRPower(OFF);
            if (ret_fn)
            {   printf("OTDRPower(OFF) failed. Error:%d\n", ret_fn);
                break;
            }

            printf("Starting next iteration in 10 seconds. Press CTRL+C if you want to stop the execution.\n");
            sleep(10);
        }
    }
    while(0);

    /** Otdr De-Initialization **/
    if(ret_fn != OTDR_INIT_FAILED)
    {   if (0 != (ret_fn = OtdrInterfaceDeInit()))
        {   printf("ERROR: Otdr DeInit Failed \n");
        }
    }

#ifdef ADD_DELAY
    usleep(10000);
#endif
}

void _powerSfpSeq()
{   int status = 0;
    struct timeval  l_x_tStampFirst = {0};
    struct timeval  l_x_tStampFinal = {0};

    do
    {   /**********SFPInterfaceInit()**********/
        status = SFPInterfaceInit();
        if (status != 0)
        {   printf("ERROR: SFPinit Error no: %d\n", status);
            break;
        }
        else
            printf("SFP Initialized Successfully\n");

        printf("Powering up the SFP...\n");
        /** Get the First Timestamp */
        gettimeofday(&l_x_tStampFirst, NULL);
        status = SFPPower(ON);
        if (status)
        {   printf("SFPPower(ON) failed. Error:%d\n", status);
            break;
        }

        while(1)
        {   status = getSFPStatus();
            if(POWERMGT_DEVICE_ON == status)
            {   /** Get the Final Timestamp */
                gettimeofday(&l_x_tStampFinal, NULL);
                printf("SFP Power Up Successfully :%d , in: [%f]Sec\n", status,
                       (double) (l_x_tStampFinal.tv_usec - l_x_tStampFirst.tv_usec) / 1000000 +
                       (double) (l_x_tStampFinal.tv_sec - l_x_tStampFirst.tv_sec));
                break;
            }
            else if(POWERMGT_DEVICE_OFF == status)
            {   /** Get the Final Timestamp */
                gettimeofday(&l_x_tStampFinal, NULL);
                printf("Issue in Powering on the SFP:%d. Terminating the execution in :[%f]Sec\n", status,
                       (double) (l_x_tStampFinal.tv_usec - l_x_tStampFirst.tv_usec) / 1000000 +
                       (double) (l_x_tStampFinal.tv_sec - l_x_tStampFirst.tv_sec));

                status = SFPInterfaceDeInit();
                if (status != 0)
                    printf("ERROR: SFPdeinit Error no: %d\n", status);
                else
                    printf("SFP Deinitialized Successfully\n");

                return;
            }
            usleep(10000);
        }

        /** Clear the both structure */
        memset(&l_x_tStampFirst, 0x00, sizeof(struct timeval));
        memset(&l_x_tStampFinal, 0x00, sizeof(struct timeval));

        _sfpseq();

        status = SFPPower(OFF);
        if (status)
        {   printf("SFPPower(OFF) failed. Error:%d\n", status);
            break;
        }

        printf("Starting next iteration in 5 seconds. Press CTRL+C if you want to stop the execution.\n");
        sleep(5);
        while(1)
        {   status = getSFPStatus();
            if(POWERMGT_DEVICE_OFF == status)
            {   printf("SFP is POWER OFF\n");
                printf("Powering up the SFP...\n");
                /** Get the First Timestamp */
                gettimeofday(&l_x_tStampFirst, NULL);
                status = SFPPower(ON);
                if (status)
                {   printf("SFPPower(ON) failed. Error:%d\n", status);
                    break;
                }
            }

            while(1)
            {   status = getSFPStatus();
                if(POWERMGT_DEVICE_ON == status)
                {   /** Get the Final Timestamp */
                    gettimeofday(&l_x_tStampFinal, NULL);
                    printf("SFP Power Up Successfully :%d , in: [%f]Sec\n", status,
                           (double) (l_x_tStampFinal.tv_usec - l_x_tStampFirst.tv_usec) / 1000000 +
                           (double) (l_x_tStampFinal.tv_sec - l_x_tStampFirst.tv_sec));

                    break;
                }
                else if(POWERMGT_DEVICE_OFF == status)
                {   /** Get the Final Timestamp */
                    gettimeofday(&l_x_tStampFinal, NULL);
                    printf("Issue in Powering on the SFP:%d. Terminating the execution in :[%f]Sec\n", status,
                           (double) (l_x_tStampFinal.tv_usec - l_x_tStampFirst.tv_usec) / 1000000 +
                           (double) (l_x_tStampFinal.tv_sec - l_x_tStampFirst.tv_sec));

                    status = SFPInterfaceDeInit();
                    if (status != 0)
                        printf("ERROR: SFPdeinit Error no: %d\n", status);
                    else
                        printf("SFP Deinitialized Successfully\n");

                    return;
                }
                usleep(10000);
            }

            /** Clear the both structure */
            memset(&l_x_tStampFirst, 0x00, sizeof(struct timeval));
            memset(&l_x_tStampFinal, 0x00, sizeof(struct timeval));

            _sfpseq();

            status = SFPPower(OFF);
            if (status)
            {   printf("SFPPower(OFF) failed. Error:%d\n", status);
                break;
            }

            printf("Starting next iteration in 10 seconds. Press CTRL+C if you want to stop the execution.\n");
            sleep(10);
        }
    }
    while(0);
    /**********SFPInterfaceDeInit()**********/
    status = SFPInterfaceDeInit();
    if (status != 0)
        printf("ERROR: SFPdeinit Error no: %d\n", status);
    else
        printf("SFP Deinitialized Successfully\n");
    return;
}
void _bootTimeProductInfo()
{   int ret = 0;
    char buffer[8192] = {0};

    if((ret = initialProductInfo(buffer)))
    {   printf("Boottime Productinfo error: %d\n and Error Message: %s\n", ret, buffer);
    }
    else
    {   printf("Boottime Productinfo API: Intial_ProductInfo Returned:%d, Response:\n%s\n", ret, buffer);
    }

    return;
}
void _productInfo()
{   int ret = 0;
    char buffer[512] = {0};

    if((ret = ProductInfo(buffer)))
    {   printf("productinfo error: %d\n and msg: %s\n", ret, buffer);
    }
    else
    {   printf("ProductInfo response:\n%s\n", buffer);
    }

    return;
}

/****************************************************************** EventCallback SECTION : START **********************************************************/
void* pwrElapsedTimer(void *arg)
{   int l_i_retry = 30;
    gettimeofday(&g_x_startTime, NULL);
    while(g_i_pwrStopTimer)
    {   if(!(l_i_retry--))
        {   //Board is going to shutdown in 2 seconds
            printf("XMFH Board is going to Shutdown in 2 seconds\n");
        }
        usleep(100000);
    }
    return NULL;
}

/**
 * @brief	MF Button Pressed event handler.
*/
void MF_Btn_Pressed_Event()
{   printf("MF Button Pressed event received\n");
}


/**
 * @brief	Power Button Pressed event handler.
*/
void Pwr_Btn_Pressed_Event()
{   pthread_t pwrTimerThread = 0;
    g_i_pwrStopTimer = 1;
    if(pthread_create(&pwrTimerThread,NULL,&pwrElapsedTimer,NULL))
    {   printf("ERROR!! FAIL to create Power Timer  Thread\n");
        return;
    }

    pthread_detach(pwrTimerThread);
    printf("Power Button Pressed event received\n");
}

/**
 * @brief	Power Button Released event handler.
*/
void Pwr_Btn_Released_Event()
{   g_i_pwrStopTimer = 0;
    gettimeofday(&g_x_endTime, NULL);
    printf("Power Button Released In %f seconds. 3 Sec Press = Notification message. 5 Sec Press = Hard Shutdown\n",
           (double) (g_x_endTime.tv_usec - g_x_startTime.tv_usec) / 1000000 +
           (double) (g_x_endTime.tv_sec - g_x_startTime.tv_sec));

    //Clear the structures
    memset(&g_x_startTime, 0x00, sizeof(struct timeval));
    memset(&g_x_endTime, 0x00, sizeof(struct timeval));
}


/**
 * @brief	SFP Adapter Out event handler.
*/
void Sfp_Adapter_Out_Event(void)
{    printf("SFP Adapter OUT event received\n");
}
/**
 * @brief	HD Out event handler.
*/
void HD_Out_Event(void)
{    printf("HD OUT event received\n");
}
/**
 * @brief	SFP Out event handler.
*/
void Sfp_Out_Event(void)
{     printf("SFP OUT event received\n");
}

/**
 * @brief	SFP Adapter In event handler.
*/  
void Sfp_Adapter_In_Event(void)
{    printf("SFP Adapter IN event received\n");
}

/**
 * @brief	HD In event handler.
*/
void HD_In_Event(void)
{    printf("HD IN event received\n");
}

/**
 * @brief	SFP In event handler.
*/
void Sfp_In_Event(void)
{    printf("SFP IN event received\n");
}

/**
 * @brief	This Function is callback function for events.
*/
void EventCallback(E_EVENTS_TYPE i_e_eventName)
{   /** Check the bit change for all possible events. Perform necessary actions accordingly. */
    /** Parse the event type & proceed */
    switch(i_e_eventName)
    {   case EVENT_MF_BUTTON_PRESSED:
            MF_Btn_Pressed_Event();
            break;

        case EVENT_SFP_IN:
            Sfp_In_Event();
            break;

        case EVENT_SFP_OUT:
            Sfp_Out_Event();
            break;
        case EVENT_SFP_ADP_IN:
            Sfp_Adapter_In_Event();
            break;
        
        case EVENT_SFP_ADP_OUT: 
            Sfp_Adapter_Out_Event();
            break;  

        case EVENT_HD_IN:   
            HD_In_Event();
            break;

        case EVENT_HD_OUT:  
            HD_Out_Event();
            break;  

        case EVENT_PWR_BTN_PRESSED:
            Pwr_Btn_Pressed_Event();
            break;

        case EVENT_PWR_BTN_RELEASED:
            Pwr_Btn_Released_Event();
            break;

        default:
            printf("Unknown event received\n");
    }
}

void _eventTest()
{   int status = HARDWARE_EVENT_FAIL;

    /* Register callback function (EventCallback) to be called in case of any event occurrence. */
    status = HardwareEventCallbackInit(EventCallback);
    if(status)
    {   printf("Hardware Event Init Failed [%d]\n", status);
    }

    while(1)
    {   sleep(2);
    }

}

void PwrMcuInit()
{   int status = 0;
    /**********SFPInterfaceInit()**********/
    status = SFPInterfaceInit();
    if (status != 0)
        printf("ERROR: SFPinit Error no: %d\n", status);
    else
        printf("SFP Initialized Successfully\n");

    status = LEDInit();
    if(status != LED_SUCCESS)
    {   printf("Failed to Initialize LED Error <%d>\n", status);

    }
    else
    {   printf("LED Initialized Successfully\n");
    }

    /**********BatteryAPIInit()**********/
    status = BatteryAPIInit();
    if (status != 0)
    {   printf("ERROR: BatteryAPIInit Error no: %d\n", status);
        return;
    }
    else
        printf("BatteryAPI Initialized Successfully\n");

    /** LED Controller*/
    status = LEDEnableController(1);
    if(status != LED_SUCCESS)
    {   printf("Failed to Enable the LED Controller Error <%d>\n", status);

    }
}

void PwrMcuDeInit()
{   int status = 0;
    /**********SFPInterfaceDeInit()**********/
    status = SFPInterfaceDeInit();
    if (status != 0)
        printf("ERROR: SFPdeinit Error no: %d\n", status);
    else
        printf("SFP Deinitialized Successfully\n");

    /**********BatteryAPIDeInit()**********/
    status = BatteryAPIDeInit();
    if (status != 0)
        printf("ERROR: BatteryAPIDeInit Error no: %d\n", status);
    else
        printf("Battery Deinitialized Successfully\n");

    /** Deinit the LED*/
    LEDDeInit();
    printf("LED Deinitialized Successfully\n");
}

void _i2cStressTest_PwrMcu(int loop)
{   int count = 0;

    PwrMcuInit();

    device_deinit_flag = 10;

    do
    {   printf("\n-----------------------Iteration %d--------------------\n",++count);
        PwrMcuOp();
    }
    while ((loop == -1) || (--loop > 0));
    PwrMcuDeInit();
}

void _ledSeq()
{   int status;

    //Turn on all LEDs white
    printf("LED :Turn on all LEDs white \n");
    status = LEDBrightness(LED_ID_ALL, 255, 255, 255);
    if(status != LED_SUCCESS)
    {   printf("Failed to set LED Brightness Error <%d>\n", status);

    }
    //Turn ALM LED RED
    printf("LED :Turn ALM LED RED \n");
    status = LEDBrightness(LED_ID_ALM, 255, 0, 0);
    if(status != LED_SUCCESS)
    {   printf("Failed to set LED Brightness Error <%d>\n", status);

    }

    return ;
}

void _battery()
{   int l_i_status;
    float l_f_tmp = 0;

    /**********GetBatteryChargePercentage()**********/
    l_f_tmp = 0;
    l_i_status = GetBatteryChargePercentage(&l_f_tmp);
    if (l_i_status != 0)
        printf("ERROR: GetBatteryChargePercentage init Error no: %d\n", l_i_status);
    else
        printf("BatteryAPI GetBatteryChargePercentage: <%.2f%%>\n", l_f_tmp);

    return;
}

void PwrMcuOp()
{   int status = 0;

    //STEP: A
    printf("Power ON sfp status %d\n", SFPPower(ON));
    for(int i =0; i<10; i++)
    {   status = getSFPStatus();
        if(POWERMGT_DEVICE_ON == status)
        {   printf("SFP Power Up Successfully :%d\n", status);
            break;
        }
        else if(POWERMGT_DEVICE_OFF == status)
        {   printf("Issue in Powering on the SFP:%d. Terminating the execution\n", status);
            break;
        }
        usleep(10000);
    }

    //STEP: B
    bool tx_status = 1;
    status = SFPSetTxDisable(tx_status);
    if (status != 0)
        printf("ERROR: SFPSetTxDisable Error no: %d\n", status);
    else
        printf("SFP. API:SFPSetTxDisable. Response Tx status: %d\n", tx_status);

    //STEP: C
    status = getSFPStatus();
    printf("SFP getSFPStatus Return Value: %d\n", status);

    //STEP: D
    _sfpseq();

    //STEP: E
    tx_status = 0;
    status = SFPSetTxDisable(tx_status);
    if (status != 0)
        printf("ERROR: SFPSetTxDisable Error no: %d\n", status);
    else
        printf("SFP. API:SFPSetTxDisable. Response Tx status: %d\n", tx_status);

    //STEP: F
    status = getSFPStatus();
    printf("SFP getSFPStatus Return Value: %d\n", status);

    _sfpseq();

    //STEP: H
    tx_status = 1;
    status = SFPSetTxDisable(tx_status);
    if (status != 0)
        printf("ERROR: SFPSetTxDisable Error no: %d\n", status);
    else
        printf("SFP. API:SFPSetTxDisable. Response Tx status: %d\n", tx_status);

    status = getSFPStatus();
    printf("SFP getSFPStatus Return Value: %d\n", status);

    _battery();

    _ledSeq();

    _sfpseq();

    //STEP: G
    printf("sfp OFF status %d\n",SFPPower(OFF));
}

void _fsl_i2cStressTest(int loop)
{   int count = 0;
    do
    {   device_deinit_flag =  7;
        _led(1);
        device_deinit_flag = 9;
        _batterymanagement(1,0);
        printf("-------------------------------------------count:%d\n\n",++count);
    }
    while ((loop == -1) || (--loop > 0));
}

void _linkit_i2cStressTest(int loop)
{   int count = 0;
    do
    {   device_deinit_flag = 5;
        _sfp(1);
        device_deinit_flag = 3;
        _otdr(-1,-1);
        device_deinit_flag = 6;
        _occ();
        printf("-------------------------------------------count:%d\n\n",++count);
    }
    while ((loop == -1) || (--loop > 0));
}
/****************************************************************** EventCallback SECTION : END **********************************************************/
void printUsageMsg()
{   printf("* ossdn-test app Usage: *****************************************************************************************\n"
           "*\n"
           "* OTDR APIs with Normal mode  : ossdn-test otdr normal_scan <trace_point_print - 1/0> <limited_execution - 1/0> loop_count Api_no\n"
           "* OTDR APIs with Fast mode    : ossdn-test otdr fast_scan   <trace_point_print - 1/0> <limited_execution - 1/0> loop_count Api_no\n"
           "*                               Arg <trace_point_print> : Value - 1 / 0 :: Printing trace data point - Enable (1) or Disable (0)\n"
           "*                               Arg <limited_execution> : Value - 1 / 0 :: if 1, Only run, status, ltrace, events functions\n"
           "*                               (loop_count) - count for interation\n"
           "*                               (Api_no) - No which mapped with API need to execute\n"
           "*                               1 - OtdrGetVer, 2 - OtdrGetVerFull, 3 - OtdrGetAstm, 4 - OtdrResult, 5 - OtdrGetAst, 6 - OtdrStatus\n"
           "*                               7 - OtdrSetEventLossThreshold, 8 - OtdrSetPulseWidth, 9 - OtdrSetMeasureTime, 10 - OtdrSetWindowSize\n"
           "*                               11 - OtdrSetResolution, 12 - OtdrSetSpanLength, 13 - OtdrSetWindowOffset, 14 - OtdrSetRefractiveIndex\n"
           "*                               15 - OtdrGetParameters, 16 - OtdrGetMt, 17 - OtdrAnalyze, 18 - OtdrRun, 19 - OtdrDumpTrace\n"
           "*                               20 - OtdrDumpTraceLinear, 21 - OtdrDumpEvent, 22 - OtdrGetPd, 23 - OtdrGetHwVer, 24 - OtdrSetRtc\n"
           "*                               25 - OtdrGetRtc, 26 - OtdrGetSupply, 27 - OtdrClearAlarmLatch, 28 - OtdrSetAstmNormal, 29 - OtdrSetAstmSticky\n"
           "*                               30 - OtdrSetEventReflectionThreshold, 31 - OtdrGetALRM, 32 - OtdrLiveRun, 33 - OtdrLiveStop\n"
           "*                               34 - OtdrLiveTraceCount, 35 - OtdrLiveAvgAnalyze, 36 - OtdrLiveEvent, 37 - OtdrLiveAnalyze, 38 - OtdrLiveTrace\n"
           "*                               39 - OtdrLiveAvgEvent, 40 - OtdrLiveAnalyzeStatus, 41 - OtdrSetMaxNE, 42 - OtdrStop, 43 - OtdrGetMaxNE\n"
           "*                               44 - OtdrLiveAnalyzeTrace, 45 - OtdrLiveAvgAnalyzeTrace, 46 - OtdrLiveAvgTrace, 47 - OtdrReboot\n"
           "* OCM APIs                    : ossdn-test ocm loop_count Api_no\n"
           "*                               1 - OcmGetSNUM, 2 - OcmGetDREV, 3 - OcmGetCSSP, 4 - OcmSetCSSP, 5 - OcmGetFWVR, 6 - OcmGetHWVR, 7 - OcmGetCSTA\n"
           "*                               8 - OcmGetCTPR1, 9 - OcmDoCENB1, 10 - OcmGetCFCR, 11 - OcmGetCSTS0, 12 - OcmGetCGTP1B, 13 - OcmGetCBDI\n"
           "*                               14 - OcmGetCDDFB, 15 - OcmGetCGET1, 16 - OcmSetCDMO, 17 - OcmSetGain, 18 - OcmSetCMPP1, 19 - OcmSetCSIL1\n"
           "*                               20 - OcmSetCSPT1, 21 - OcmGetCGSP1\n"
           "* OCC APIs                    : ossdn-test occ\n"
           "* SFP APIs                    : ossdn-test sfp loop_count \n"
           "* OTDR FW upgrade API         : ossdn-test otdr_fw fw_file_name (Filename From XMFH /tmp/ dir)\n"
           "* OCM FW upgrade API          : ossdn-test ocm_fw fw_file_name (Filename From XMFH /tmp/ dir)\n"
           "* SFP FW Upgrade API          : ossdn-test sfp_fw filename (Filename from XMFH /tmp/ dir)\n"
           "* LED APIs                    : ossdn-test led loop_count \n"
           "* Freescale reset API         : ossdn-test fslrst 1/0 (Toggle reset pin of the Freescale processor)\n"
           "*                               (0) - Reset Freescale irrespective of current Freescale state.\n"
           "*                               (1) - Reset Freescale in case of Freescale is stuck (I2C communication issue).\n"
           "* BATTERY MANAGEMENT APIs     : ossdn-test batterymanagement loop_count Api_no\n"
           "*                               1 - GetBatteryChargeCycle, 2 - GetBatteryChargerTemp, 3 - GetBatteryVoltage, 4 - GetBatteryCurrent\n"
           "*                               5 - GetBatteryChargePercentage, 6 - GetBatteryChargerVoltage, 7 - GetBatteryDeepSleepConfig\n"
           "*                               8 - SetBatteryDeepSleepConfig, 9 - Get all battery values \n"   
           "* Hw Event Debug Log 	  : ossdn-test enable_hw_debug on/off\n"
           "* Timeout Configuration       : ossdn-test i2c_inprogress_timeout config(0/1) InProgressTimeout\n"
           "*                             : ossdn-test i2c_read_timeout readTimeout\n"
           "* POWER MANAGEMENT APIs       : ossdn-test powermgt\n"
           "* Diagnostics Data APIs       : ossdn-test diagdata\n"
           "* OTDR power  sequence        : ossdn-test power_otdr_seq\n"
           "* SFP  power  sequence        : ossdn-test power_sfp_seq\n"
           "* Product Information         : ossdn-test productinfo\n"
           "* OSA Calibration             : ossdn-test osa_cal\n"
           "* OSA Set Cal                 : ossdn-test osa_set_cal\n"
           "* OSA Reset Cal               : ossdn-test osa_reset_cal\n"
           "* Mirroring WIFI LED          : ossdn-test mirror 1/0 (ON/OFF) \n"
           "* Hardware EventTest          : ossdn-test eventTest\n"
           "                                (Supported Events: SFP In/Out, Power Button Pressed/Released\n"
           "                                  and MF Button Pressed. Press CTRL+C to deregister test Callback and Exit.)\n"
           "* I2C Stress testing          : ossdn-test i2cstresstest fsl/linkit/mcu loopcount (for infinite loop enter '-1' in loop count) \n"
           "* Switch UART                 : get_uart_status switch_uart_int switch_uart_ext \n"
           "*\n"
           "*****************************************************************************************************************\n");
    return;
}

/**************************************************************************************************
 *
 * @brief   This function Checks if Hw is 1.1, then Set the Mux Setting according to the i_Hw_Option
 *	    Option
 *
 * @param   i_Hw_Option Hardware Selection Option
 *
 * @return  On success, returns 0
 *	    While in case of failure returns -1.
 *
 **************************************************************************************************/
int ioExp2Check(int i_Hw_Option)
{   int l_i_status = 0;
    uint8_t l_ui8_data[2] = {0};
    mraa_i2c_context  l_mraa_i2cTMS = NULL;

    switch(i_Hw_Option)
    {   case 1:
            l_ui8_data[1] = 0x5F;       //For kl02
            break;

        case 2:
            l_ui8_data[1] = 0x9F;       //For otdr
            break;

        case 3:
            l_ui8_data[1] = 0xDF;       //For Powr607
            break;

        case 4:
            l_ui8_data[1] = 0x3F;       //For deinit
            break;


        default:
            printf("Unknown Hw option Received received\n");
    }
    do
    {   /**Check the Hw Rev file*/
        if( access(HARDWARE_REVISION_1_1, F_OK ) != 0 )
        {   l_i_status = 1;
            break;
        }

        /* I2C 0 line Init*/
        l_mraa_i2cTMS = mraa_i2c_init(0);
        if (NULL == l_mraa_i2cTMS)
        {   break;
        }

        if ((mraa_i2c_address(l_mraa_i2cTMS, 0x26)) != 0)
        {   break;
        }

        /* make sure that inputs are set high independent of current status */
        l_ui8_data[0] = l_ui8_data[0] | 0xFF;

        l_i_status = mraa_i2c_write(l_mraa_i2cTMS, l_ui8_data, 2);
        if(l_i_status != 0 && errno == 6)
        {   l_i_status = -1;
            break;
        }
        else if(l_i_status != 0)
        {   l_i_status = -1;
            break;
        }


        l_i_status = 0;
    }
    while(0);
    if(l_mraa_i2cTMS)
    {   mraa_i2c_stop(l_mraa_i2cTMS);
        l_mraa_i2cTMS = NULL;
    }

    return l_i_status;
}


/**************************************************************************************************
 *
 * @brief   This function Checks if Reset_Latch_EXIO is low, then toggle the Reset_Clr_EXIO Pin of
 *	    IO_EXP2
 *
 * @param   i_Hw_Option Hardware Selection Option
 *
 **************************************************************************************************/
void chk_i2c_latchReset(void)
{   int l_i_sts = 0;
    uint8_t l_ui8_pcf_data[2] = {0};
    mraa_i2c_context l_p_eventi2c_init = NULL;
    int l_i_length = 0;
    char l_ac_cmd[16] = "passwd root";
    FILE *l_p_passfp = NULL;

    /**If hwrev1.1 file dose not exist then stop the latch reset operaion*/
    if( access(HARDWARE_REVISION_1_1, F_OK ) != 0 )
    {   return;
    }


    /**Initialize the I2C */
    l_p_eventi2c_init = mraa_i2c_init(0);
    if (!l_p_eventi2c_init)
    {   printf("ERROR!! Failed to intialize mraa i2c\n");
        return;
    }

    /* 0x26 valid device address */
    l_i_sts = mraa_i2c_address(l_p_eventi2c_init, 0x26);
    if (l_i_sts)
    {   printf("ERROR!!Failed to set the I2C address\n");
        return;
    }

    do
    {   l_i_length = mraa_i2c_read(l_p_eventi2c_init,  l_ui8_pcf_data, 2);
        if (l_i_length  == 0 && errno == 6)
        {   printf("NACK Received\n");
            break;
        }
        else if(l_i_length == 0)
        {   printf("Failed to read the data\n");
            break;
        }
        else
        {   if(CHANGESBYTE(l_ui8_pcf_data[0], 2) == 0)      /**Check the P2 Pin of the IO_Expander2*/
            {   /**Clear the P12 Pin*/
                l_ui8_pcf_data[1] &= 0xFB;
                mraa_i2c_write(l_p_eventi2c_init,  l_ui8_pcf_data, 2);
                usleep(100000);
                /**Set the P12 Pin*/
                l_ui8_pcf_data[1] |= 0x04;
                mraa_i2c_write(l_p_eventi2c_init,  l_ui8_pcf_data, 2);
                printf("Resetting the password...\n");
                l_p_passfp= popen(l_ac_cmd, "w");
                if(l_p_passfp)
                {   fprintf(l_p_passfp, "fiberscan\n");
                    fprintf(l_p_passfp, "fiberscan\n");
                    printf("Return value: %i\n", WEXITSTATUS(pclose(l_p_passfp)));
                }
                printf("Setting WiFi Mode to the AP...\n");
                system("wifi_mode ap && wifi");
            }
        }
    }
    while(0);

    l_i_sts = mraa_i2c_stop(l_p_eventi2c_init);

    return;
}
/**
 * @brief This functions checks the far side I2C slave communication,
 *        If Slave devices can't able to communicate and returns the bus
 *        Error then reset the PwrMgtMCU.
 *
 */
void _fslSlaveChk()
{   int l_i_status = 0;

    l_i_status = FslSlaveCheckCmd();
    if(l_i_status != 0)
    {   if(l_i_status == 2705)
        {   printf("\nBus Error Detected. Resetting the PwrMgtMCU...\n");
            _fslrst(0);
        }
        else
        {   printf("\nERROR: Failed to communicate with PwrMgtMCU's Slave Device , Status: %d\n", l_i_status);
        }
    }
    else
    {   printf("\nSuccessfully able to communicate with Fuelgauge\n");
    }
}

/* Main function tests instrument APIs as per passed argument */
int main(int argc, char **argv)
{   int loop=0;
    int l_i_status =0;
    int8_t l_i8_ioExp2Hwoption = 0;
    /* Signal handling for graceful deinit and termination */
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGHUP,  signal_handler);
    signal(SIGTSTP, signal_handler);
    signal(SIGKILL, signal_handler);
    signal(SIGABRT, signal_handler);
#ifdef MT_TEST
    pthread_t thread_id[MT_THREAD_COUNT];
#endif
    if (argc > 1)
    {   if(!strcmp("ocm",argv[1]))
        {   device_deinit_flag = 2;
#ifdef MT_TEST
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_create(&thread_id[loop], NULL, _ocm,NULL);
                usleep(200);
            }
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_join(thread_id[loop], NULL);
                usleep(200);
            }
#else
            if (argc == 3)
                _ocm((int)atoi(argv[2]),-1);
            else if (argc == 4)
                _ocm((int)atoi(argv[2]), (int)atoi(argv[3]) );
            else
                _ocm(-1,-1);
#endif
        }
        else if(!strcmp("switch_uart_int",argv[1]))
        {   
            int ret_fn = 0;
            do
            {   /** OTDR Initialization **/
                if ((ret_fn = OtdrInterfaceInit(OTDR_FAST_MODE)) != 0)
                {   printf("ERROR: otdr Init Error: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Initialized Successfully.\n");
                }
                /** sfp Initialization **/
                if (ret_fn = SFPInterfaceInit() != 0)
                {   printf("ERROR: SFPinit Error no: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("SFP Initialized Successfully\n");
                }

                ret_fn = getSFPStatus();
                switch(ret_fn)
                {   case POWERMGT_DEVICE_ON:
                        printf("SFP is POWER ON\n");
                        printf("External OTDR power off status %d\n",SFPPower(OFF));
                        break;
                    case POWERMGT_DEVICE_OFF:
                        printf("SFP is POWER OFF\n");
                        break;
                }

                if(ret_fn = OTDRPowerForce(ON)!= 0)
                {   printf("ERROR: OTDR FORCE ON Status: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Powered ON Successfully.\n");
                }

                if(ret_fn = OtdrSwitchUart(OFF)!= 0)
                {   printf("ERROR: OTDR Switch UART failed status: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Switched to internal Successfully.\n");
                }

                usleep(1000000);                
                OtdrEmuReinit();

                /** OTDR Deinitialization **/
                if (ret_fn = OtdrInterfaceDeInit() != 0)
                {   printf("ERROR: Otdr DeInit Failed: %d\n", ret_fn);
                }
                else
                {   printf("OTDR Deinitialized Successfully.\n");
                }
                /** SFP Deinitialization **/
                if (ret_fn = SFPInterfaceDeInit() != 0)
                {   printf("ERROR: SFP DeInit Failed: %d\n", ret_fn);
                }
                else
                {   printf("SFP Deinitialized Successfully.\n");
                }
            }while(0);

        }
        else if(!strcmp("switch_uart_ext",argv[1]))
        {   
            int ret_fn = 0;
            do
            {   /** OTDR Initialization **/
                if ((ret_fn = OtdrInterfaceInit(OTDR_FAST_MODE)) != 0)
                {   printf("ERROR: otdr Init Error: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Initialized Successfully.\n");
                }
                /** sfp Initialization **/
                if (ret_fn = SFPInterfaceInit() != 0)
                {   printf("ERROR: SFPinit Error no: %d\n", ret_fn);
                    break; ;
                }
                else
                {   printf("SFP Initialized Successfully\n");
                }

                ret_fn = getOTDRStatus();
                switch(ret_fn)
                {   case POWERMGT_DEVICE_ON:
                        printf("Internal OTDR POWER is ON\n");
                        printf("Inter OTDR POWER OFF status %d\n",OTDRPower(OFF));
                        break;
                    case POWERMGT_DEVICE_OFF:
                        printf("Internal OTDR is POWER OFF\n");
                        break;
                }

                ret_fn = getSFPStatus();
                switch(ret_fn)
                {   case POWERMGT_DEVICE_ON:
                        printf("External OTDR POWER is ON\n");
                        break;
                    case POWERMGT_DEVICE_OFF:
                        printf("External OTDR is POWER OFF\n");
                        printf("External OTDR POWER ON status %d\n",SFPPower(ON));
                        break;
                }

                if(ret_fn = OtdrSwitchUart(ON)!= 0)
                {   printf("ERROR: OTDR Switch UART failed status: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Switched to external Successfully.\n");
                }

                /*** Check for boot mode****/
                int l_i_maxEmuSeconds = 10;
                do{
                    sleep(1);    
                    l_i_maxEmuSeconds--;
                }while(OtdrBootModeStatus(OTDR_BOOT_MODE) && l_i_maxEmuSeconds > 0);
                if (l_i_maxEmuSeconds <= 0)
                {
                    printf("ERROR: OTDR_EMU $ (Boot) mode failed\n");
                    break;
                }

                if(OtdrB())
                {printf("ERROR: Otdr Boot command Failed\n");
                    break;
                }
                /*** Check for active mode****/
                l_i_maxEmuSeconds = 10;
                do{
                    sleep(1);    
                    l_i_maxEmuSeconds--;
                }while(OtdrBootModeStatus(OTDR_ACTIVE_MODE) && l_i_maxEmuSeconds);
                if (l_i_maxEmuSeconds <= 0)
                {
                    printf("ERROR: OTDR_EMU > (Active) mode failed\n");
                    break;
                }                
                OtdrEmuReinit();
                /** OTDR Deinitialization **/
                if (ret_fn = OtdrInterfaceDeInit() != 0)
                {   printf("ERROR: Otdr DeInit Failed: %d\n", ret_fn);
                }
                else
                {   printf("OTDR Deinitialized Successfully.\n");
                }
                /** SFP Deinitialization **/
                if (ret_fn = SFPInterfaceDeInit() != 0)
                {   printf("ERROR: SFP DeInit Failed: %d\n", ret_fn);
                }
                else
                {   printf("SFP Deinitialized Successfully.\n");
                }
            }while(0);
        }
		else if(!strcmp("SFPtest",argv[1]))
		{
            int status = 0;
            g_b_addDelay = 1; //SFP must need proper delay we can't ignore in stress testing also.
            char l_i_option = -1;
            bool run_loop = true;
            bool mode_abs = 0;
            bool tx_fault = 0;
            bool tx_disable = 0;
            bool tx_status = 1;
            do{
                /**********Initialize SFP Interface **********/
                status = SFPInterfaceInit();
                if (status != 0){
                    printf("ERROR: SFPinit Error no: %d\n", status);
                    break;
                }
                else
                    printf("SFP Initialized Successfully\n");

                SFP_TEST_START_CASE:
                /**Print menu to get user input ***/
                printf("**************************");
                printf("\nselcet the sfp param to set\n");
                printf("**************************\n");
                printf("\t0.ReadModAbs\n");
                printf("\t1.ReadTxFault\n");
                printf("\t2.ReadTxDisable\n");
                printf("\t3.SetTxDisable\n");
                printf("\t4.ClearTxDisable\n");
                printf("\t5.Exit\n");   
                printf("**************************\n");
                /***********Print menu end ********/
                scanf(" %c", &l_i_option);
                switch(l_i_option)
                {
                    case '0':
                        
                        status = SFPReadModABS(&mode_abs);
                        if (status != 0)
                            printf("ERROR: SFPReadModABS Error no: %d\n", status);
                        else
                            printf("SFP. API:SFPReadModABS. Response Mode ABS: %d\n", mode_abs);
                        break;
                    case '1':
                        
                        status = SFPReadTxFault(&tx_fault);
                        if (status != 0)
                            printf("ERROR: SFPReadTxFault Error no: %d\n", status);
                        else
                            printf("SFP. API:SFPReadTxFault. Response Tx fault: %d\n", tx_fault);
                        break;
                    case '2':                        
                        status = SFPReadTxDisable(&tx_disable);
                        if (status != 0)
                            printf("ERROR: SFPReadTxDisable Error no: %d\n", status);
                        else
                            printf("SFP. API:SFPReadTxDisable. Response Tx Disable: %d\n", tx_disable);
                        break;
                    case '3':
                        tx_disable = 1;
                        status = SFPSetTxDisable(tx_disable);
                        if (status != 0)
                            printf("ERROR: SFPSetTxDisable Error no: %d\n", status);
                        else
                            printf("SFP. API:SFPSetTxDisable Set is successfull \n");
                        break;
                    case '4':
                        tx_disable = 0;
                        status = SFPSetTxDisable(tx_disable);
                        if (status != 0)
                            printf("ERROR: SFPSetTxDisable Error no: %d\n", status);
                        else
                            printf("SFP. API:SFPSetTxDisable Clear is successfull \n");
                        break;
                    case '5':
                        run_loop = false;
                        break;
                    default:
                        printf("select valid option\n");
                        goto SFP_TEST_START_CASE;
                }                

                /**********Deinitialize SFP Interface **********/
                status = SFPInterfaceDeInit();
                if (status != 0)
                    printf("ERROR: SFPdeinit Error no: %d\n", status);
                else
                    printf("SFP Deinitialized Successfully\n");
            }while(run_loop);	
            printf("SFP Test Completed...!\n");
		}
        else if (!strcmp("SFPtonemode",argv[1]))
        {   
            bool mode_abs = 0;
            int status = -1, ret_fn = 0;
            char l_i_option = -1;
            bool run_loop = true;
            SFP_TONEMODE mode;
            do{ /**********Initialize SFP Interface **********/
                status = SFPInterfaceInit();
                if (status != 0) {
                    printf("ERROR: SFPinit Error no: %d\n", status);
                    break;
                }
                /*SFP Power Off*/
                status = SFPPower(OFF);
                if(status!=0){
                    printf("ERROR! SFP Power status %d\n",status);
                    break;
                }
                usleep(1000000);
                status=SFPPower(ON);
                if(status!=0){
                    printf("ERROR! SFP Power Error no: %d\n",status);
                    break;
                }
                else{
                    printf("SFP Power ON Successfully\n");
                }            
                do{               
                    status = SFPReadModABS(&mode_abs);
                    if (status != 0){
                        printf("ERROR: SFPReadModABS Error no: %d\n", status);
                        break;
                    }
                    if (mode_abs == 0 )
                    {   /**Print menu to get user input ***/
                        printf("**************************");
                        printf("\nSelect the option to get or set the mode\n");
                        printf("**************************\n");
                        printf("\t0. SFP GET CURRENT MODE\n");
                        printf("\t1. SFP STEADY OFF\n");
                        printf("\t2. SFP STEADY ON\n");
                        printf("\t3. SFP MODE\n");
                        printf("\t4. SFP TONE 270HZ\n");
                        printf("\t5. Exit\n");
                        /***********Print menu end ********/
                        scanf(" %c", &l_i_option); 
                        /********Execute user option*******/
                        switch(l_i_option)
                        {   
                            case '0':
                                ret_fn = SFPGetToneGeneratorMode(&mode);
                                if(ret_fn == SFP_SUCCESS){
                                    printf("SFP Tone Mode: %s\n", mode==SFP_STEADY_OFF?"SFP_STEADY_OFF":mode==SFP_STEADY_ON?"SFP_STEADY_ON":mode==SFP_MODE?"SFP_MODE":mode==SFP_TONE_270HZ?"SFP_TONE_270HZ":"SFP_INVALID_MODE");
                                } 
                                else {
                                    printf("Failed to get SFP Tone Mode %d.\n",status);
                                }
                                break;
                            case '1':
                                ret_fn = SFPSetToneGeneratorMode(SFP_STEADY_OFF);
                                break;
                            case '2' :
                                ret_fn = SFPSetToneGeneratorMode(SFP_STEADY_ON);
                                break;                        
                            case '3' :
                                ret_fn = SFPSetToneGeneratorMode(SFP_MODE);
                                break;
                            case '4' :
                                ret_fn = SFPSetToneGeneratorMode(SFP_TONE_270HZ);
                                break;
                            default:
                                run_loop = false;
                                break;                   
                        }
                        if(ret_fn == SFP_SUCCESS){
                            printf("Command SFP Set Tone Sent Successfully.\n");
                        }
                        else{
                            printf("Command SFP Set Tone Failed. Error: %d\n", ret_fn);
                        }
                    }
                    else{
                        printf("ERROR! No SFP Module\n");
                        break;
                    }  
                }while(run_loop);

                /*Power off the SFP*/
                status = SFPPower(OFF);
                if(status!=0){
                    printf("ERROR! SFP Power status %d\n",status);                    
                }
				else{
					printf("SFP Power OFF Successfully\n");
				}
                /**********Deinitialize SFP Interface **********/
                status = SFPInterfaceDeInit();
                if (status != 0){
                    printf("ERROR: SFPdeinit Error no: %d\n", status);
                    break;
                }
                else{
                    printf("SFP Deinitialized Successfully\n");
                }
            }while(0);
            printf("SFP Tone Mode Test Completed...!\n");	
        }
        else if(!strcmp("OTDRsettone",argv[1]))
		{
            int ret_fn = 0;
            char l_i_option = -1;
			do
            {   /** OTDR Initialization **/
                if ((ret_fn = OtdrInterfaceInit(OTDR_FAST_MODE)) != 0)
                {   printf("ERROR: otdr Init Error: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Initialized Successfully.\n");
                }   

                printf("**************************");
                printf("\nSelcet the mode to set\n");
                printf("**************************\n");
                printf("\t0.OTDR Steady OFF\n");
                printf("\t1.OTDR Steady ON\n");
                printf("\t2.OTDR\n");
                printf("\t3.270HZ\n");
                printf("\t4.1KHZ\n");
                printf("\t5.2KHZ\n");

                scanf(" %c", &l_i_option);
                switch(l_i_option)
                {   case '0':
                        ret_fn = SetToneGeneratorMode(OTDR_STEADY_OFF);
                        break;

                    case '1' :
                        ret_fn = SetToneGeneratorMode(OTDR_STEADY_ON);
                        break;
                    
                    case '2' :
                        ret_fn = SetToneGeneratorMode(OTDR_MODE);
                        break;

                    case '3' :
                        ret_fn = SetToneGeneratorMode(OTDR_TONE_270HZ);
                        break;

                    case '4':
                        ret_fn = SetToneGeneratorMode(OTDR_TONE_1KHZ);
                        break;

                    case '5':
                        ret_fn = SetToneGeneratorMode(OTDR_TONE_2KHZ);
                        break;

                    default:
                        printf("select valid option\n");
                        continue;
                }

                if(ret_fn == OTDR_COMMAND_SUCCESS)
                    printf("Command set tone sent Successfully.\n");
                else
                    printf("Command set tone failed. Error: %d\n", ret_fn);
                /** OTDR Deinitialization **/
                if (ret_fn = OtdrInterfaceDeInit() != 0)
                {   printf("ERROR: Otdr DeInit Failed: %d\n", ret_fn);
                }
                else
                {   printf("OTDR Deinitialized Successfully.\n");
                }
                
            }while(0);
		}        
        else if(!strcmp("OTDRgettone",argv[1]))
		{
            int ret_fn = 0;
            T_TONEMODE l_tone_mode;
			do
            {   /** OTDR Initialization **/
                if ((ret_fn = OtdrInterfaceInit(OTDR_FAST_MODE)) != 0)
                {   printf("ERROR: otdr Init Error: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Initialized Successfully.\n");
                }                
                
                ret_fn = GetToneGeneratorMode(&l_tone_mode);
                if(ret_fn == OTDR_COMMAND_SUCCESS)
                    printf("Generator mode is: %s.\n", l_tone_mode == OTDR_STEADY_OFF ? "OTDR Steady OFF" : l_tone_mode == OTDR_STEADY_ON ? "OTDR Steady ON" : l_tone_mode == OTDR_MODE ? "OTDR" : 
                                                       l_tone_mode == OTDR_TONE_2KHZ ? "2KHZ" : l_tone_mode == OTDR_TONE_1KHZ ? "1KHZ" : l_tone_mode == OTDR_TONE_270HZ ? "270HZ" : "INVALID MODE");
                else
                    printf("Command set tone failed. Error: %d\n", ret_fn);

                /** OTDR Deinitialization **/
                if (ret_fn = OtdrInterfaceDeInit() != 0)
                {   printf("ERROR: Otdr DeInit Failed: %d\n", ret_fn);
                }
                else
                {   printf("OTDR Deinitialized Successfully.\n");
                }
                
            }while(0);
		}
        else if(!strcmp("seteof",argv[1]))
		{
            int ret_fn = 0;
            char l_i_option = -1;
            float eof_ltf = 0, eof_lt=0,eof_ltlz=0, eof_ptf = 0;
			do
            {   /** OTDR Initialization **/
                if ((ret_fn = OtdrInterfaceInit(OTDR_FAST_MODE)) != 0)
                {   printf("ERROR: otdr Init Error: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Initialized Successfully.\n");
                }   

                printf("**************************");
                printf("\nselcet threshold to set\n");
                printf("**************************\n");
                printf("\t0.EOF Loss Threshold \n");
                printf("\t1.EOF Pos Threshold \n");
                scanf(" %c", &l_i_option);
                switch(l_i_option)
                {   case '0':
                        printf("Enter the Value of EOF Loss Threshold - LTF, LT, LTLZ:\n");
                        //Read float value from the console
                        scanf("%f %f %f", &eof_ltf,&eof_lt,&eof_ltlz);
                        ret_fn = OtdrSetEofLossThreshold(eof_ltf, eof_lt, eof_ltlz);
                        break;

                    case '1' :
                        printf("Enter value of EOF Pos Threshold Front: \n");
                        //Read float value from the console
                        scanf("%f", &eof_ptf);  
                        ret_fn = OtdrSetEofPosThresholdFront(eof_ptf);
                        break;
                    default:
                        printf("select valid option\n");
                        continue;
                }

                if(ret_fn == OTDR_COMMAND_SUCCESS){
                    if(l_i_option == '0')
                        printf("EOF Loss Thresold LTF: %.2f, LT: %.2f, LTLZ: %.2f \nSet Successfully.\n",eof_ltf, eof_lt, eof_ltlz);
                    else
                        printf("EOF Pos Thresold Front: %.2f \nSet Successfully. \n",eof_ptf);
                }
                else
                    printf("Command set end of fiber failed. Error: %d\n", ret_fn);
                /** OTDR Deinitialization **/
                if (ret_fn = OtdrInterfaceDeInit() != 0)
                {   printf("ERROR: Otdr DeInit Failed: %d\n", ret_fn);
                }
                else
                {   printf("OTDR Deinitialized Successfully.\n");
                }
                
            }while(0);
		}
        else if(!strcmp("geteof",argv[1]))
		{
            int ret_fn = 0;
            float eof_ltf = 0, eof_lt=0, eof_ltlz=0, eof_ptf = 0;

			do
            {   /** OTDR Initialization **/
                if ((ret_fn = OtdrInterfaceInit(OTDR_FAST_MODE)) != 0)
                {   printf("ERROR: otdr Init Error: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Initialized Successfully.\n");
                }                
                
                ret_fn = OtdrGetEofLossThreshold(&eof_ltf, &eof_lt, &eof_ltlz);
                if(ret_fn == OTDR_COMMAND_SUCCESS)
                    printf("EOF Loss Threshold - LTF: %f, LT:%f, LTLZ:%f\n",eof_ltf, eof_lt, eof_ltlz);
                else
                    printf("EOF Loss Thresold Read failed. Error: %d\n", ret_fn);
                
                ret_fn = OtdrGetEofPosThresholdFront(&eof_ptf);
                if(ret_fn == OTDR_COMMAND_SUCCESS)
                    printf("EOF Pos Thresold Front: %f.\n", eof_ptf);
                else
                    printf("EOF Pos Thresold Front Read failed. Error: %d\n", ret_fn);

                /** OTDR Deinitialization **/
                if (ret_fn = OtdrInterfaceDeInit() != 0)
                {   printf("ERROR: Otdr DeInit Failed: %d\n", ret_fn);
                }
                else
                {   printf("OTDR Deinitialized Successfully.\n");
                }
                
            }while(0);
		}
        else if(!strcmp("emu_reinit",argv[1]))
		{
            int ret_fn = 0;
			do
            {   /** OTDR Initialization **/
                if ((ret_fn = OtdrInterfaceInit(OTDR_FAST_MODE)) != 0)
                {   printf("ERROR: otdr Init Error: %d\n", ret_fn);
                    break;
                }
                else
                {   printf("OTDR Initialized Successfully.\n");
                }                
                OtdrEmuReinit();
                printf("Command emu reinit sent Successfully.\n");
                /** OTDR Deinitialization **/
                if (ret_fn = OtdrInterfaceDeInit() != 0)
                {   printf("ERROR: Otdr DeInit Failed: %d\n", ret_fn);
                }
                else
                {   printf("OTDR Deinitialized Successfully.\n");
                }
                
            }while(0);
		}
        
		else if(!strcmp("emu_start",argv[1]))
		{
			system("/etc/init.d/otdr_emu.sh start");
		}
		else if(!strcmp("emu_stop",argv[1]))
        {
            system("/etc/init.d/otdr_emu.sh stop");
        }
        else if(!strcmp("get_uart_status",argv[1]))
        {
            int ext_uart_st = 0;
            isExternalOTDR(&ext_uart_st);  
            printf("UART status: %s \n", ext_uart_st ? "External" :"Internal" );
        }
		else if(!strcmp("otdrB",argv[1]))
        {
			system("echo 'B' >/dev/ttyS0");
            printf("Boot command sent over uart \n");
        }	
		else if(!strcmp("ocm_fw",argv[1]))
        {   char l_c_filename[128] = {0};
            device_deinit_flag = 2;
            if(argc >= 3)
            {   snprintf((char *) l_c_filename, 128, "%s", argv[2]);
#ifdef MT_TEST
                for (loop=0; loop < MT_THREAD_COUNT; loop++)
                {   pthread_create(&thread_id[loop], NULL, _ocm_fw, l_c_filename);
                    usleep(200);
                }
                for (loop=0; loop < MT_THREAD_COUNT; loop++)
                {   pthread_join(thread_id[loop], NULL);
                    usleep(200);
                }
#else
                _ocm_fw(l_c_filename);
#endif
            }
        }
        else if(!strcmp("otdr_fw",argv[1]))
        {   char l_c_filename[128] = {0};
            device_deinit_flag = 3;
            if(argc >= 3)
            {   snprintf((char *) l_c_filename, 128, "%s", argv[2]);
#ifdef MT_TEST
                for (loop=0; loop < MT_THREAD_COUNT; loop++)
                {   pthread_create(&thread_id[loop], NULL, _otdr_fw, l_c_filename);
                    usleep(200);
                }
                for (loop=0; loop < MT_THREAD_COUNT; loop++)
                {   pthread_join(thread_id[loop], NULL);
                    usleep(200);
                }
#else
                _otdr_fw(l_c_filename);
#endif
            }
        }
        else if(!strcmp("otdr",argv[1]))
        {   device_deinit_flag = 3;
            int count = -1;
            int apiNo = -1;

#if 0
            if(argc >= 3)
            {   if(!strcmp("fast_scan",argv[2]))
                    option=OTDR_FAST_MODE;
                else if(!strcmp("normal_scan",argv[2]))
                    option=OTDR_NORMAL_MODE;
                else
                {   printUsageMsg();
                    return 0;
                }
                if(argc >= 4)
                {   if(!strcmp("1",argv[3]))
                    {   otdrMsgVal=OTDR_DUMP_MSG_ENABLE;
                    }
                    else if(!strcmp("0",argv[3]))
                    {   otdrMsgVal=OTDR_DUMP_MSG_DISABLE;
                    }
                    else
                    {   printUsageMsg();
                        return 0;
                    }

                    if(argc >= 5)
                    {   if(!strcmp("1",argv[4]))
                        {   otdrFuncSelect=OTDR_TRACE_FUNC_ONLY;
                        }
                        else if(!strcmp("0",argv[4]))
                        {   otdrFuncSelect=OTDR_ALL_FUNC;
                        }
                        else
                        {   printUsageMsg();
                            return 0;
                        }

                        if(argc == 6)
                        {   count = (int)atoi(argv[5]);
                        }
                        elseif (argc == 7)
                        {   count = (int)atoi(argv[5]);
                            apiNo = (int)atoi(argv[6]);
                        }

                    }

                }
            }

#endif

#ifdef MT_TEST
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_create(&thread_id[loop], NULL, _otdr, NULL);
                usleep(200);
            }
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_join(thread_id[loop], NULL);
                usleep(200);
            }
#else

            if(argc == 7)
            {   if(!strcmp("fast_scan",argv[2]))
                    option=OTDR_FAST_MODE;
                else if(!strcmp("normal_scan",argv[2]))
                    option=OTDR_NORMAL_MODE;
                else
                {   printUsageMsg();
                    return 0;
                }

                if(!strcmp("1",argv[3]))
                {   otdrMsgVal=OTDR_DUMP_MSG_ENABLE;
                }
                else if(!strcmp("0",argv[3]))
                {   otdrMsgVal=OTDR_DUMP_MSG_DISABLE;
                }
                else
                {   printUsageMsg();
                    return 0;
                }

                if(!strcmp("1",argv[4]))
                {   otdrFuncSelect=OTDR_TRACE_FUNC_ONLY;
                }
                else if(!strcmp("0",argv[4]))
                {   otdrFuncSelect=OTDR_ALL_FUNC;
                }
                else
                {   printUsageMsg();
                    return 0;
                }

                _otdr((int)atoi(argv[5]),(int)atoi(argv[6]));
            }

            else
            {   printUsageMsg();
                return 0;
            }

#endif

        }
        else if(!strcmp("occ",argv[1]))
        {   device_deinit_flag = 6;
#ifdef MT_TEST
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_create(&thread_id[loop], NULL, _occ,NULL);
                usleep(200);
            }
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_join(thread_id[loop], NULL);
                usleep(200);
            }
#else
            _occ();
#endif
        }
        else if(!strcmp("sfp",argv[1]))
        {   device_deinit_flag = 5;
#ifdef MT_TEST
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_create(&thread_id[loop], NULL, _sfp,NULL);
                usleep(200);
            }
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_join(thread_id[loop], NULL);
                usleep(200);
            }
#else
            if(argc == 3)
            {   _sfp((int)atoi(argv[2]));
            }
            else
                _sfp(1);

#endif
        }
        else if(!strcmp("sfp_fw",argv[1]))
        {   device_deinit_flag = 5;
            char l_c_filename[128] = {0};
            if(argc >= 3)
            {   snprintf((char *) l_c_filename, 128, "%s", argv[2]);
#ifdef MT_TEST
                for (loop=0; loop < MT_THREAD_COUNT; loop++)
                {   pthread_create(&thread_id[loop], NULL, _sfp_fw, argv[2]);
                    usleep(200);
                }
                for (loop=0; loop < MT_THREAD_COUNT; loop++)
                {   pthread_join(thread_id[loop], NULL);
                    usleep(200);
                }
#else
                _sfp_fw(l_c_filename);
#endif
            }
        }
        else if(!strcmp("led",argv[1]))
        {   device_deinit_flag = 7;
#ifdef MT_TEST
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_create(&thread_id[loop], NULL,  _led,NULL);
                usleep(200);
            }
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_join(thread_id[loop], NULL);
                usleep(200);
            }
#else
            if(argc != 3)
            {   printUsageMsg();
                return 0;
            } 
            if(!strcmp(argv[2],"run_animation"))
            {   int status = LEDInit();
                if(status != LED_SUCCESS)
                {   printf("Failed to Initialize Error <%d>\n", status);

                }

                status = RunDefaultLedAnimation();
                if(status != LED_SUCCESS)
                {   printf("Failed to run default ANIMATION <%d>\n", status);
                }

                LEDDeInit();
            }
            else if(!strcmp(argv[2],"stop_animation"))
            {   int status = LEDInit();
                if(status != LED_SUCCESS)
                {   printf("Failed to Initialize Error <%d>\n", status);

                }
                status = SetLedControlFlag(0);
                if(status != LED_SUCCESS)
                {   printf("Failed to stop ANIMATION <%d>\n", status);
                }
                LEDDeInit();

            }
			else if(!strcmp(argv[2],"fw_upgrade_start"))
            {   int status = LEDInit();
                if(status != LED_SUCCESS)
                {   printf("Failed to Initialize Error <%d>\n", status);

                }                
                //Turn off all LEDs
                printf("LED :Turn off all LEDs \n");
                status = LEDBrightness(LED_ID_ALL, 0, 0, 0);
                if(status != LED_SUCCESS)
                {   printf("Failed to set LED Brightness Error <%d>\n", status);

                }
				if(g_b_addDelay)
                    usleep(500000);
                
                printf("Turn on LED's to: ALM: Fuschia, WIFI: Purple, PWR: Green, BAT: Aqua \n");
                status = LEDBrightness(LED_ID_PWR, 0, 64, 0);
                if(status != LED_SUCCESS)
                {   printf("Failed to set LED Brightness Error <%d>\n", status);

                }
                status = LEDBrightness(LED_ID_BAT, 0, 100, 100);
                if(status != LED_SUCCESS)
                {   printf("Failed to set LED Brightness Error <%d>\n", status);

                }
                status = LEDBrightness(LED_ID_WIFI, 25, 0, 50);
                if(status != LED_SUCCESS)
                {   printf("Failed to set LED Brightness Error <%d>\n", status);

                }
                status = LEDBrightness(LED_ID_ALM, 100, 25, 0);
                if(status != LED_SUCCESS)
                {   printf("Failed to set LED Brightness Error <%d>\n", status);

                }
                
                LEDDeInit();
            }
			else if(!strcmp(argv[2],"fw_upgrade_fail"))
            {   int status = LEDInit();
                if(status != LED_SUCCESS)
                {   printf("Failed to Initialize Error <%d>\n", status);

                }
                //Turn off all LEDs
                printf("LED :Turn off all LEDs \n");
                status = LEDBrightness(LED_ID_ALL, 0, 0, 0);
                if(status != LED_SUCCESS)
                {   printf("Failed to set LED Brightness Error <%d>\n", status);

                }
                if(g_b_addDelay)
                    usleep(500000);
                //Turn on all LEDs Red
                printf("LED :Turn on all LEDs Red \n");
                status = LEDBrightness(LED_ID_ALL, 255, 0, 0);
                if(status != LED_SUCCESS)
                {   printf("Failed to set LED Brightness Error <%d>\n", status);

                }
                if(g_b_addDelay)
                    usleep(500000);

                LEDDeInit();
            }
			else if(!strcmp(argv[2],"fw_upgrade_done"))
            {   int status = LEDInit();
                if(status != LED_SUCCESS)
                {   printf("Failed to Initialize Error <%d>\n", status);

                }
                //Turn off all LEDs
                printf("LED :Turn off all LEDs \n");
                status = LEDBrightness(LED_ID_ALL, 0, 0, 0);
                if(status != LED_SUCCESS)
                {   printf("Failed to set LED Brightness Error <%d>\n", status);

                }
				if(g_b_addDelay)
                    usleep(500000);
                //Turn on all LEDs Green
                printf("LED :Turn on all LEDs Green \n");
                status = LEDBrightness(LED_ID_ALL, 0, 255, 0);
                if(status != LED_SUCCESS)
                {   printf("Failed to set LED Brightness Error <%d>\n", status);

                }
                if(g_b_addDelay)
                    usleep(500000);
                
                LEDDeInit();
            }
			else  if(!strcmp(argv[2],"run_animation2"))
            {   int status = LEDInit();
                if(status != LED_SUCCESS)
                {   printf("Failed to Initialize Error <%d>\n", status);

                }

                status = RunDefaultLedAnimation2();
                if(status != LED_SUCCESS)
                {   printf("Failed to run default ANIMATION <%d>\n", status);
                }

                LEDDeInit();
            }
            else
			{
               	_led((int)atoi(argv[2]));
			}
#endif
        }
        else if(!strcmp("batterymanagement",argv[1]))
        {
#ifdef MT_TEST
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_create(&thread_id[loop], NULL, _batterymanagement,NULL);
                usleep(200);
            }
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_join(thread_id[loop], NULL);
                usleep(200);
            }
#else
            if(argc != 4)
            {   printUsageMsg();
                return 0;
            }

            _batterymanagement((int)atoi(argv[2]), (int)atoi(argv[3]));
#endif
        }

        else if(!strcmp("osa_cal",argv[1]))
        {   device_deinit_flag = 4;
            _osa_cal();
        }
        else if(!strcmp("osa_set_cal",argv[1]))
        {   device_deinit_flag = 2;
            do
            {   /** OCM Initialization **/
                l_i_status = OcmInterfaceInit();
                if (l_i_status != 0)
                {   printf("ERROR: OCMinit Error no: %d\n", l_i_status);
                    break ;
                }

                printf("SetCTMCalibration: %d\n",SetCTMCalibration(-5.0f, 5.0f));

            }
            while(0);
            /** Ocm De-Initialization **/
            if (l_i_status != OCM_INIT_ERROR)
            {   if (0 != (l_i_status=OcmInterfaceDeInit()))
                {   printf("OSACAL: ERROR: Ocm DeInit Failed %d \n", l_i_status);
                }
            }
        }
        else if(!strcmp("osa_reset_cal",argv[1]))
        {   device_deinit_flag = 2;
            do
            {   /** OCM Initialization **/
                l_i_status = OcmInterfaceInit();
                if (l_i_status != 0)
                {   printf("ERROR: OCMinit Error no: %d\n", l_i_status);
                    break;
                }

                printf("ResetCTMCalibrationToMFGBaseline: %d\n",ResetCTMCalibrationToMFGBaseline());

            }
            while(0);

            /** Ocm De-Initialization **/
            if (l_i_status != OCM_INIT_ERROR)
            {   if (0 != (l_i_status=OcmInterfaceDeInit()))
                {   printf("OSACAL: ERROR: Ocm DeInit Failed %d \n", l_i_status);
                }
            }
        }
        else if(!strcmp("powermgt",argv[1]))
        {   device_deinit_flag = 4;
            option=OTDR_FAST_MODE;
#ifdef MT_TEST
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_create(&thread_id[loop], NULL, _powermgt,NULL);
                usleep(200);
            }
            for (loop=0; loop < MT_THREAD_COUNT; loop++)
            {   pthread_join(thread_id[loop], NULL);
                usleep(200);
            }
#else
            _powermgt();
#endif
        }
        else if(!strcmp("enable_hw_debug",argv[1]))
        {   if(argc == 3 && (!strcasecmp("on",argv[2])))
                enableHwEventDebugLog(true);
            else
                enableHwEventDebugLog(false);
        }
        else if(!strcmp("i2c_read_timeout",argv[1]))
        {   if(argc == 3)
                configure_mcu_read_timeout((int)atoi(argv[2]));
            else
            {   printUsageMsg();
                return 0;
            }
        }
        else if(!strcmp("i2c_inprogress_timeout",argv[1]))
        {   if(argc == 4)
                configure_mcu_inprogress_timeout((int)atoi(argv[2]), (int)atoi(argv[3]));
            else
            {   printUsageMsg();
                return 0;
            }
        }
        else if(!strcmp("diagdata",argv[1]))
        {   _diagdata();
        }
        else if(!strcmp("op_sw_en",argv[1]))
        {   _op_sw_en();
        }
        else if(!strcmp("power_off_otdr",argv[1]))
        {   int status = -1;
            status = power_off_otdr(false);
            if(status)
            {   printf("OTDR Power off failed. Returned:%d\n", status);
            }
            else
            {   printf("\nOTDR Powered off.\n");
            }
        }
        else if(!strcmp("power_off_sfp",argv[1]))
        {   int status = -1;
            status = power_off_sfp();
            if(status)
            {   printf("SFP Power off failed. Returned:%d\n", status);
            }
            else
            {   printf("\nSFP Powered off.\n");
            }
        }
        else if(!strcmp("fslsetup",argv[1]))
        {   _fslsetup();
        }
        else if(!strcmp("fslrst",argv[1]))
        {   if(argc == 3)
            {   if(!strcmp("1",argv[2]))
                {   _fslrst(1);
                }
                else if(!strcmp("0",argv[2]))
                {   _fslrst(0);
                }
                else
                {   printUsageMsg();
                    return 0;
                }
            }
            else
            {   printUsageMsg();
                return 0;
            }
        }
        else if(!strcmp("power_otdr_seq",argv[1]))
        {   device_deinit_flag = 3;
            option=OTDR_FAST_MODE;
            _powerOtdrSeq();
        }
        else if(!strcmp("power_sfp_seq",argv[1]))
        {   device_deinit_flag = 5;
            _powerSfpSeq();
        }
        else if(!strcmp("boottimeproductinfo",argv[1]))
        {   _bootTimeProductInfo();
        }
        else if(!strcmp("productinfo",argv[1]))
        {   _productInfo();
        }
        else if(!strcmp("eventTest",argv[1]))
        {   device_deinit_flag = 8;
            _eventTest();
        }
        else if(!strcmp("fslver",argv[1]))
        {   _fsl_ver();
        }
        else if(!strcmp("hwrev",argv[1]))
        {   checkHardwareRevision();
        }
        else if(!strcmp("apdvendor", argv[1]))
        {
            checkAPDVendor();
        }
        else if(!strcmp("boot_otdr_fw",argv[1]))
        {   _boot_otdr_fw();
        }
        else if(!strcmp("mirror",argv[1]))
        {   if(argc >= 3)
            {   int l_i_status = 0;
                bool value = atoi(argv[2]);

                l_i_status = LEDInit();
                if(l_i_status != LED_SUCCESS)
                {   printf("Failed to Initialize Error <%d>\n", l_i_status);

                }

                l_i_status = LEDWifiMirroring(value);
                printf("API: LEDWifiMirroring status:%d\n", l_i_status);

                /** Deinit the LED*/
                LEDDeInit();
            }
        }
        else if(!strcmp("i2cstresstest",argv[1]))
        {   g_b_addDelay=0;
            if(argc>=3)
            {   if(!strcmp("linkit",argv[2]))
                {   option=OTDR_FAST_MODE;
                    if(argc>3)
                        _linkit_i2cStressTest((int)atoi(argv[3]));
                    else
                        _linkit_i2cStressTest(1);

                }
                else if(!strcmp("fsl",argv[2]))
                {   if(argc>3)
                        _fsl_i2cStressTest((int)atoi(argv[3]));
                    else
                        _fsl_i2cStressTest(1);
                }
                else if(!strcmp("mcu",argv[2]))
                {   if(argc>3)
                        _i2cStressTest_PwrMcu((int)atoi(argv[3]));
                    else
                        _i2cStressTest_PwrMcu(1);
                }
                else
                    _fsl_i2cStressTest((int)atoi(argv[2]));
            }
            else
                _fsl_i2cStressTest(1);
        }
        else if(!strcmp("power_off_ins",argv[1]))
        {   int status = -1;
            status = power_off_all_Ins();
            if(status)
            {   printf("Power off All instruments failed. Returned:%d\n", status);
            }
            else
            {   printf("\nAll Instruments powered off successfully.\n");
            }
        }
        else if(!strcmp("IO_EXP2",argv[1]))
        {   int status = -1;
            if(argc == 3 && (!strcasecmp("kl02",argv[2])))
            {   l_i8_ioExp2Hwoption = 1;
            }
            else if(argc == 3 && (!strcasecmp("otdr",argv[2])))
            {   printf("\n otdr selected by IO_EXP2\n");
                l_i8_ioExp2Hwoption = 2;
            }
            else if(argc == 3 && (!strcasecmp("powr607",argv[2])))
            {   l_i8_ioExp2Hwoption = 3;
            }
            else if(argc == 3 && (!strcasecmp("deinit",argv[2])))
            {   l_i8_ioExp2Hwoption = 4;
            }

            status = ioExp2Check(l_i8_ioExp2Hwoption);
            printf("%d",status);

        }
        else if(!strcmp("sw3_check",argv[1]))
        {   chk_i2c_latchReset();
        }
        else if(!strcmp("fslslavechk",argv[1]))
        {   _fslSlaveChk();
        }
        else
        {   printUsageMsg();
        }
    }
    else
    {   printUsageMsg();
    }


    return 0;
}


