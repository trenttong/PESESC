#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <string>
#include <cassert>
#include <cstdarg>

using namespace std;

#define MY_FOPEN  fopen
#define MY_FCLOSE fclose

#define XLOG_LOGLEVELS      6
#define XLOG_LOG_MAX_LENGTH 256

/// @ -------------------------------------------------- @ ///
//  @ log system                                         @ ///
/// @ -------------------------------------------------- @ ///

/// XLog - log system that can adjust what is printed based
/// on level.
class XLog
{
public:
    enum
    {
        FORCELOG=0,   /* force log */
        NOLOG,        /* do not log */
        CRITICAL,     /* only log critical msgs */
        WARNING,      /* log critical and warning msgs */
        VERBOSE,      /* log verbose and upper levels */
        SUPERVERBOSE, /* log superverbose and upper levels */
    };

private:
    /* logged */
    unsigned long long logcount;
    /* level of printing and file to print to */
    int level;
    /* file to print to */
    FILE* file;
    /* name of the logger */
    string logname;
    /* print to file as well */
    bool logtofile;
    /* print to stdout as well */
    bool logtostdout;
    /* enable sampling ? */
    bool sampling;
    /* sample frequency */
    int frequency;
    /* level names in strings */
    const char* levelname[XLOG_LOGLEVELS];

public:
    /// XLog - constructor/destructor.
    XLog(int level            ,
         const char* filename , 
         const string logname ,  
         bool logtofile       , 
         bool logtostdout     , 
         bool sampling        , 
         int frequency) :
         logcount(0)          ,
         level(level)         , 
         file(file)           , 
         logname(logname)     , 
         logtofile(logtofile) , 
         logtostdout(logtostdout), 
         sampling(sampling)   , 
         frequency(frequency)
    {
        levelname[NOLOG]        = "[NOLOG]";
        levelname[CRITICAL]     = "[CRITICAL]";
        levelname[WARNING]      = "[WARNING] ";
        levelname[VERBOSE]      = "[VERBOSE]";
        levelname[FORCELOG]     = "[FORCELOG]";
        levelname[SUPERVERBOSE] = "[SUPERVERBOSE]";

        if (logtofile)
        {
            file = MY_FOPEN(filename, "w+");
            assert(file);
        }
    }

    /// ~XLog - destructor.
    virtual ~XLog() { if (logtofile) MY_FCLOSE(file); }

    /// logme - log the msg based on the level of the logger and the msg.
    void logme(int loglevel, const string fmt, ...)
    {
        if (sampling) 
        {
           if (++logcount % frequency) return;
        }

        if (loglevel <= level)
        {
            char buffer[XLOG_LOG_MAX_LENGTH];
            va_list args;

            va_start(args, fmt);
            vsnprintf(buffer, sizeof(buffer), fmt.c_str(), args);
            va_end(args);

            string afmt = "[" + logname +  "]" + " " + buffer;

            /* where to log */
            if (logtostdout) fprintf(stdout, "%s\n", afmt.c_str());
            if (logtofile)   fprintf(file, "%s\n", afmt.c_str());
        }
    }
};

#endif // LOGGER_H
