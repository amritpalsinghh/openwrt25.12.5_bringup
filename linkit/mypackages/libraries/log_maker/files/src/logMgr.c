#include "logMgr.h"
#include <sys/stat.h>
#include <pthread.h>

LogLevel _logLevel = LOG_LEVEL_INFO;
int _logOutput = 0;

static const char *const _LogTypeStr[] =
{   "SERIAL",           // LOG_TYPE_SERIAL
    "SOCKET",          	//LOG_TYPE_SOCKET
    "I2C",              //LOG_TYPE_I2C
    "OTHER",          	//LOG_TYPE_OTHER
    "***",            	//LOG_TYPE_LAST
};

static const char *const _LogLevelStr[] =
{   "NOLOG", // LOG_LEVEL_NOLOG
    "ERROR", //LOG_LEVEL_ERR
    "WARN",  //LOG_LEVEL_WARN
    "INFO"   // LOG_LEVEL_INFO
};

void rotateLog(FILE **fp, char *i_filename);

void logMgrClose(FILE **i_fp)
{   if ((*i_fp))
    {   fflush((*i_fp));
        fclose((*i_fp));
        (*i_fp) = NULL;
    }
}

void Log(FILE **i_fp,void *s, LogLevel level, LogType type, const char *format, va_list *args)
{   char lgr_filename[2048] = {0};

    if (_logLevel < level)
        return;

    if((*i_fp) == NULL)
        return;
    /*
      * Output to file.
      */
    if (_logOutput & LOG_OUTPUT_FILE)
    {   struct stat buf = {0};
        size_t l_lgr_fsize = 0;  /* current log file size */

        if(strstr(format, "OTDR"))
        {   strcpy(lgr_filename, "/var/log/OTDR.log");
            l_lgr_fsize = (stat(lgr_filename, &buf) == 0) ? buf.st_size : 0;
        }
        else if(strstr(format, "OCM"))
        {   strcpy(lgr_filename, "/var/log/OCM.log");
            l_lgr_fsize = (stat(lgr_filename, &buf) == 0) ? buf.st_size : 0;
        }
        else if(strstr(format, "SWITCH"))
        {   strcpy(lgr_filename, "/var/log/SWITCH.log");
            l_lgr_fsize = (stat(lgr_filename, &buf) == 0) ? buf.st_size : 0;
        }
        else if(strstr(format, "BATTERY"))
        {   strcpy(lgr_filename, "/var/log/BATTERY.log");
            l_lgr_fsize = (stat(lgr_filename, &buf) == 0) ? buf.st_size : 0;
        }
        else if(strstr(format, "LED"))
        {   strcpy(lgr_filename, "/var/log/LED.log");
            l_lgr_fsize = (stat(lgr_filename, &buf) == 0) ? buf.st_size : 0;
        }
        else if(strstr(format, "POWER"))
        {   strcpy(lgr_filename, "/var/log/POWER.log");
            l_lgr_fsize = (stat(lgr_filename, &buf) == 0) ? buf.st_size : 0;
        }
        else if(strstr(format, "SFP"))
        {   strcpy(lgr_filename, "/var/log/SFP.log");
            l_lgr_fsize = (stat(lgr_filename, &buf) == 0) ? buf.st_size : 0;
        }
        else if(strstr(format, "HWEVENT"))
        {   strcpy(lgr_filename, "/var/log/HWEVENT.log");
            l_lgr_fsize = (stat(lgr_filename, &buf) == 0) ? buf.st_size : 0;
        }
        else if(strstr(format, "OSACAL"))
        {   strcpy(lgr_filename, "/var/log/OSACAL.log");
            l_lgr_fsize = (stat(lgr_filename, &buf) == 0) ? buf.st_size : 0;
        }
        else
        {   return;
        }


        printTime(i_fp, &l_lgr_fsize);
        l_lgr_fsize += fprintf((*i_fp), "%s: [%s]", _LogLevelStr[level], _LogTypeStr[type]);
        l_lgr_fsize += vfprintf((*i_fp), format, args[LOG_OUTPUT_FILE]);
        l_lgr_fsize += fprintf((*i_fp), "\n");
        fflush((*i_fp));

        /* if log file size exceeds the maximum, zip current file and open a new one */

        if (l_lgr_fsize >= LOG_FILE_SIZE_MAX)
            rotateLog(i_fp,lgr_filename);
    }
}

void rotateLog(FILE **fp, char *i_filename)
{   char newname[2048] = {0};
    if(*fp)
    {   fflush((*fp));
        fclose((*fp));
        (*fp) = NULL;

        snprintf(newname, sizeof(newname), "%s.old", i_filename);
        rename(i_filename, newname);

        (*fp) = fopen(i_filename, "w");

        if ((*fp) == NULL)
        {   printf("ERROR: Can't open the file\n");
            return;
        }
    }
}

void setLogLevel(LogLevel level)
{   _logLevel = level;
}

void setLogOutput(int output)
{   _logOutput |= output;
}

void setFileLogInfo(FILE **fp,const char *path, const char *mode)
{   if ((path == NULL) || ((fp) == NULL))
    {   return;
    }

    (*fp) = fopen(path, (mode ? mode : "a+"));

    if ((*fp) == NULL)
    {   printf("ERROR: File Not Found\n");
        return ;
    }

    return;
}

void printTime(FILE **fd,size_t *fsize)
{   int ret;
    char buf[LOG_NAME_LEN] = {0};
    struct tm *tm = NULL;
    struct timeval tv = {0};
    struct timezone tz = {0};
    int ms;

    if ((*fd) == NULL)
        return;

    gettimeofday(&tv, &tz);
    tm = localtime(&tv.tv_sec);

    if(tm != NULL)
    {   ret = strftime(buf, TIME_BUF_LEN, "%Y/%m/%d %H:%M:%S", tm);
        if (ret == 0)
        {   printf("ERROR: Can't get the File\n");
        }

        /* add print million-second */
        ms = tv.tv_usec / 1000;
        char msbuf[16] = {0};
        sprintf(msbuf, ".%03d", ms);

        strcat(buf, msbuf);

        (*fsize) += fprintf((*fd), "%s ", buf);
    }
}

const char * logLevelToChar(unsigned short level)
{   switch (level)
    {   case LOG_LEVEL_ERR:
            return ("error");
        case LOG_LEVEL_WARN:
            return ("warning");
        case LOG_LEVEL_INFO:
            return ("info");
        default:
            return ("");
    }
}

void LOG_ERR(FILE **fp, void *s, LogType type, const char *format, ...)
{   if(format && (*fp))
    {   va_list args[LOG_OUTPUT_LAST];
        int index;

        for (index = 0; index < LOG_OUTPUT_LAST; index++)
            va_start(args[index], format);

        Log(fp,s, LOG_LEVEL_ERR, type, format, args);

        for (index = 0; index < LOG_OUTPUT_LAST; index++)
            va_end(args[index]);
    }
}

void LOG_WARN(FILE **fp,void *s, LogType type, const char *format, ...)
{   if(format && (*fp))
    {   va_list args[LOG_OUTPUT_LAST];
        int index;

        for (index = 0; index < LOG_OUTPUT_LAST; index++)
            va_start(args[index], format);

        Log(fp,s, LOG_LEVEL_WARN, type, format, args);

        for (index = 0; index < LOG_OUTPUT_LAST; index++)
            va_end(args[index]);
    }
}

void LOG_INFO(FILE **fp,void *s, LogType type, const char *format, ...)
{   if(format && (*fp))
    {   va_list args[LOG_OUTPUT_LAST];
        int index;

        for (index = 0; index < LOG_OUTPUT_LAST; index++)
            va_start(args[index], format);

        Log(fp,s, LOG_LEVEL_INFO, type, format, args);

        for (index = 0; index < LOG_OUTPUT_LAST; index++)
            va_end(args[index]);
    }
}
