#ifndef LOG_MGR_
#define LOG_MGR_

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>

#define LOG_NAME_LEN     256

#define TIME_BUF_LEN  27

#define LOG_FILE_SIZE_MAX 2 * 1024 * 1024 /* 2 MB */


typedef enum
{   LOG_TYPE_SERIAL = 0,
    LOG_TYPE_SOCKET,
    LOG_TYPE_I2C,
    LOG_TYPE_OTHER,
    LOG_LAST

} LogType;

typedef enum
{   LOG_LEVEL_NOLOG = 0,
    LOG_LEVEL_ERR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_LAST,

    LOG_LEVEL_DEFAULT = LOG_LEVEL_WARN
} LogLevel;

typedef enum
{   LOG_OUTPUT_CONSOLE = 1,
    LOG_OUTPUT_FILE,
    LOG_OUTPUT_SESSION,
    LOG_OUTPUT_SYSLOG,
    LOG_OUTPUT_LAST
} LogOutput;


void logMgrClose(FILE **fp);

void Log(FILE **fp,void* s, LogLevel level, LogType type, const char* format, va_list *args);

void setLogLevel(LogLevel level);
void setLogOutput(int output);
void setSysLogInfo(const char* ident, int flags, int facility);
//    FILE *setFileLogInfo(const char* path, const char* mode);
void setFileLogInfo(FILE** fp,const char* path, const char* mode);
void logRawStringToFile(const char* str);

extern LogLevel _logLevel;
extern int _logOutput;

void printTime(FILE** fd, size_t *fsize);


void LOG_ERR(FILE **fp,void* s, LogType type, const char* format, ...);
void LOG_WARN(FILE **fp,void* s, LogType type, const char* format, ...);
void LOG_INFO(FILE **fp,void* s, LogType type, const char* format, ...);

#endif /* LOG_MGR_ */


