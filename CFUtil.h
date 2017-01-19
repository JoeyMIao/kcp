//
// Created by joeymiao on 17-1-19.
//

#ifndef KCPNET_CFUTIL_H
#define KCPNET_CFUTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <cstdint>


#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#include <windows.h>
#elif !defined(__unix)
#define __unix
#endif

#ifdef __unix

#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>

#endif
namespace cf
{
    class CFUtil
    {
        /* get system time */
        static inline void itimeofday(long *sec, long *usec)
        {
#if defined(__unix)
            struct timeval time;
            gettimeofday(&time, NULL);
            if (sec) *sec = time.tv_sec;
            if (usec) *usec = time.tv_usec;
#else
            static long mode = 0, addsec = 0;
            BOOL retval;
            static IINT64 freq = 1;
            IINT64 qpc;
            if (mode == 0) {
                retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
                freq = (freq == 0)? 1 : freq;
                retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
                addsec = (long)time(NULL);
                addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
                mode = 1;
            }
            retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
            retval = retval * 2;
            if (sec) *sec = (long)(qpc / freq) + addsec;
            if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
#endif
        }

/* get clock in millisecond 64 */
        static inline int64_t iclock64(void)
        {
            long s, u;
            int64_t value;
            itimeofday(&s, &u);
            value = (int64_t) (((int64_t) s) * 1000 + (u / 1000));
            return value;
        }

        static inline uint32_t iclock()
        {
            return (uint32_t) (iclock64() & 0xfffffffful);
        }

/* sleep in millisecond */
        static inline void isleep(unsigned long millisecond)
        {
#ifdef __unix    /* usleep( time * 1000 ); */
            struct timespec ts;
            ts.tv_sec = (time_t) (millisecond / 1000);
            ts.tv_nsec = (long) ((millisecond % 1000) * 1000000);
            /*nanosleep(&ts, NULL);*/
            usleep((__useconds_t) ((millisecond << 10) - (millisecond << 4) - (millisecond << 3)));
#elif defined(_WIN32)
            Sleep(millisecond);
#endif
        }
    };
}


#endif //KCPNET_CFUTIL_H
