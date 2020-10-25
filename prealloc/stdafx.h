/* stdafx.h : include file for standard system include files,
     or project specific include files that are used frequently, but
     are changed infrequently */

#ifndef _stdafx_
    #define _stdafx_

    #ifdef _MSC_VER
        #pragma once

        #include <tchar.h>
        #include <windows.h>
        #include <wtypes.h>
        #include <io.h>
        #include <sys/stat.h>
        #include <math.h>
        #include "targetver.h"
    #else
        #include <alloca.h>
        #include <unistd.h>
        #include <syslog.h>
        #include <sys/times.h>
    #endif

    #include <stdio.h>
    #include <stdlib.h>
    #include <sys/types.h>
    #include <time.h>
    #include <fcntl.h>

/* TODO: reference additional headers your program requires here */
#endif
