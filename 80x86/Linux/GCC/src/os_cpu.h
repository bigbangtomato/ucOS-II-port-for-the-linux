/*
*********************************************************************************************************
*                                                uC/OS-II
*                                The Real-Time Kernel (by Jean J. Labrosse)
*
*
*                          (c) Copyright 2004-2005, Werner.Zimmermann@fht-esslingen.de
*                                           All Rights Reserved
*
* File : OS_CPU.H
* By   : Werner Zimmermann
*********************************************************************************************************
*/

/*
*********************************************************************************************************
   uCOS-II standard definitions and declarations.
*********************************************************************************************************
*/

typedef unsigned char         	BOOLEAN;	//already defined in windows.h
typedef unsigned char           INT8U;
typedef char                    INT8S;
typedef unsigned short          INT16U;
typedef short                   INT16S;
typedef unsigned long           INT32U;
typedef long                    INT32S;
typedef unsigned long long      INT64U;
typedef long long               INT64S;
typedef float                   FP32;
typedef double                  FP64;

typedef INT32U                  OS_STK;

#define  OS_ENTER_CRITICAL()    OSDisableInterruptFlag()
#define  OS_EXIT_CRITICAL()     OSEnableInterruptFlag()

#define  OS_STK_GROWTH          1
#define  OS_TASK_SW()           OSCtxSw()

#define  OS_CRITICAL_METHOD     1

/*
*********************************************************************************************************
   Port-specific definitions and declarations
*********************************************************************************************************
*/

INT16U OSPortVersion(void);

void OSEnableInterruptFlag(void);
void OSDisableInterruptFlag(void);

/* DEBUGLEVEL	These values can be logically ored to set the debug level for uCOS-II LINUX port debugging
                Please note, that debugging will create a lot of screen messages and thus may affect
                the real-time performance of your application
   0x00000001   Scheduler
   0x00000002   Task switch
   0x00000004   Task creation
   0x00000008   Timer
   0x00000010   Initialization
   0x00000020   Idle and stat task
   0x00000040   Scheduler and Time Tick Interrupt Timeouts
   0x00000080   Interrupt-Enable/Disable
 */
#ifndef DEBUGLEVEL
#define DEBUGLEVEL 0	//0x07F
#endif

/* Timeout value in milliseconds for the scheduler - used to detect deadlocks. Set to INFINITE for "slow" applications*/
#define OS_SCHEDULER_TIMEOUT    10000                           //INFINITE

/* Timeout value in milliseconds for the time tick interrupt */
#define OS_INTERRUPT_TIMEOUT    10000                           //INFINITE

/* If this define is set, uCOS-II runs with elevated priority to ensure better (soft)-real time behaviour.
   This may decrease the performace of other applications and reduce the responsiveness to user inputs,
   if your uCOS-II generates a high CPU load.
*/
#define OS_BOOST_LINUX_PRIORITY TRUE

#define TASKSTACKSIZE	8192

#define gettid()   syscall(__NR_gettid)
#define getpid()   syscall(__NR_gettid)
#define kill(tid,sig)  syscall(__NR_tkill, tid, sig)
