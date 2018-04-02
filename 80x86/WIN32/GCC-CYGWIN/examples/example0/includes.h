/*
*********************************************************************************************************
*                                                uC/OS-II
*                                The Real-Time Kernel (by Jean J. Labrosse)
*
*					WIN32 PORT & LINUX PORT
*
*                          (c) Copyright 2004, Werner.Zimmermann@fht-esslingen.de
*                                           All Rights Reserved
*
* File : INCLUDES.H
* By   : Werner Zimmermann
*********************************************************************************************************
*/


#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#ifndef __GNUC__
#include <conio.h>
#endif

#ifdef __WIN32__
#include <windows.h>
#endif

#ifdef __GNUC__
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#endif

#if (OS_VERSION <= 270)
#include    "os_cpu.h"
#include    "os_cfg.h"
#include    "ucos_ii.h"
#else
#include    "ucos_ii.h"
#endif

#include "pc.h"


