/*
*********************************************************************************************************
*                                                uC/OS-II
*                                The Real-Time Kernel (by Jean J. Labrosse)
*
*                                             WIN32 PORT
*
*                          (c) Copyright 2004-2005, Werner.Zimmermann@fht-esslingen.de
*                                           All Rights Reserved
*
* File : OS_CPU_C.C
* By   : Werner Zimmermann
*
* Changes:  1.40 Fixed a bug in OS_log(), thanks to Hu JW. 13 Jul 2004
*	    1.50 Substituted DWORD by INT32U, BOOL by BOOLEAN
*	    1.60 Changes in the PC_Elapsed... functions in the Cygwin and Linux port. Use direct reads 
*		 for the Pentium time stamp counter. No change in the Visual C++ port.
*           1.70 Changes to OSScheduleThread for detecting tasks deleted by OSTaskDel in the WIN32 port. 
*                Suggested by Hu JW.
*	    2.00 Modifications in the Linux port, no changes in the WIN32 ports, 18 Mar 2005
*	    2.10 Include paths modified to work with uCOS-II 2.70
*
*********************************************************************************************************
*/
#define OS_PORT_VERSION 200					//Version number of the uCOS-II WIN32 port

/*
*********************************************************************************************************
   Includes
*********************************************************************************************************
*/
#define _WIN32_WINNT  0x0400
#include    <windows.h>
#include    <winbase.h>
#include    <mmsystem.h>

#include    <stdio.h>
#include    <stdlib.h>

#if (OS_VERSION <= 270)
#include    "os_cpu.h"
#include    "os_cfg.h"
#include    "ucos_ii.h"
#else
#include    "ucos_ii.h"
#endif
/*
*********************************************************************************************************
   Global variables
*********************************************************************************************************
*/
HANDLE  hScheduleEvent, hScheduleThread;			//Scheduler thread variables
HANDLE  hTaskThread[OS_LOWEST_PRIO + 2], hDelTask;		//Map uCOS tasks to WIN32 threads
INT8S   taskSuspended[OS_LOWEST_PRIO + 2];			//Suspend status of mapped tasks/threads

#define NINTERRUPTS 8						//Number of interrupt events (DO NOT CHANGE)
HANDLE  hInterruptEvent[NINTERRUPTS], hInterruptThread;		//Interrupt handling thread variables
void (*interruptTable[NINTERRUPTS])();
BOOLEAN virtualInterruptFlag=TRUE;
INT32U   interruptThreadId = 0;

CRITICAL_SECTION criticalSection;

BOOLEAN idleTrigger = TRUE;					//Trigger a message, when the idle task is
								//invoked (used for debugging purposes)

#define NLOG	16						//Log last scheduled tasks (used for
INT16U taskLog[NLOG];						//... debugging purposes)

/*
*********************************************************************************************************
   Port-specific functions
*********************************************************************************************************
*/

// DBGPRINT ******************************************************************
// Debug output
void DBGPRINT(INT32U debugLevel, const char *fmt,...)
{   va_list argptr;
    FILE *fd;

    if ((debugLevel & DEBUGLEVEL) == 0)				//Debug output selection
        return;
    if (DEBUGLEVEL < 0x10000000UL)                              //Screen output (does influence real-time performance!)
    {   va_start(argptr, fmt);
        vprintf(fmt, argptr);
        va_end(argptr);
    } else                                                      //File output (does influence real-time performance!)
    {   va_start(argptr, fmt);
        if ((fd = fopen("ucos.log","a+"))!=NULL)
        {   vfprintf(fd, fmt, argptr);
            fclose(fd);
        }
        va_end(argptr);
    }
}


// OSLog *********************************************************************
// Log the last NLOG scheduled tasks in taskLog (with taskLog[0] = last task)
void OSLog(INT16U prio)
{   int i;

    for (i=NLOG-1; i > 0; i--)					//Shift the previous logged values by one
        taskLog[i]=taskLog[i-1];
    taskLog[0]=prio;						//Log the last one into taskLog[0]
}

// OSPortVersion *************************************************************
// Return the version number of the uCOS-II WIN32 port
INT16U OSPortVersion(void)
{    return OS_PORT_VERSION;
}

BOOLEAN CtrlBreakHandler(INT32U ctrl)
{   if (ctrl==CTRL_C_EVENT)					//Handler if CTRL-C is pressed
    {	printf("---Exiting OSPrioCur=%u-------------\n", OSPrioCur);	//---Display current task priority
    } else if (ctrl==CTRL_BREAK_EVENT)				//Handler if CTRL-BREAK is pressed
    {	printf("---Exiting OSPrioCur=%u-------------\n", OSPrioCur);	//---Display current task priority and exit
 	exit(0);
    }
    return TRUE;
}


// OSEnableInterruptFlag ****************************************************
// Enable the interrupt flag
void OSEnableInterruptFlag(void)
{
    if (virtualInterruptFlag==FALSE)				//If the timer interrupt previously was disabled,
    {   virtualInterruptFlag=TRUE;
        if (GetCurrentThreadId()!=interruptThreadId)
            ResumeThread(hInterruptThread);			//... resume the interrupt thread
    }
    DBGPRINT(0x00000080, ">>> ODEnableInterruptFlag %2d\n", virtualInterruptFlag);
}

// OSDisableInterruptFlag ****************************************************
// Disable the Interrupt Flag
void OSDisableInterruptFlag(void)
{   
    if (virtualInterruptFlag==TRUE)				//If the timer interrupt previously was enabled,
    {   if (GetCurrentThreadId()!=interruptThreadId)		//... suspend the interrupt thread ...
            SuspendThread(hInterruptThread);
        virtualInterruptFlag=FALSE;
    }
    DBGPRINT(0x00000080, ">>> OSDisableInterrupts   %2d\n", virtualInterruptFlag);
}

void OSDummyISR(void)
{   MessageBox(NULL, "Got unsupported interrupt", "uCOS-II WIN32", MB_OK | MB_SETFOREGROUND | MB_ICONERROR);
}

// OSTickTickCallback ********************************************************
// Callback function for time tick 'interrupt', acts as a relay to OSTickISR
void CALLBACK OSTimeTickCallback(INT32U hTimer, INT32U message, INT32U dwUser, INT32U lparam2, INT32U lparam3)
{   DBGPRINT(0x00000008, "*** OSTimeTickCallback\n");
    SetEvent(hInterruptEvent[0]);				//Trigger the interrupt thread
}

// OSScheduleThread ***********************************************************
// Start tasks, triggered by hScheduleEvent
void OSScheduleThread(INT32U param)
{   char temp[256];
    INT16U oldPrio, nextPrio;

    DBGPRINT(0x00000001, "*** OSScheduleThread First Call\n");

    while (1)
    {   if (WaitForSingleObject(hScheduleEvent, OS_SCHEDULER_TIMEOUT) == WAIT_TIMEOUT)	//Wait for a scheduler event (with timeout)
        {   sprintf(temp, "ERROR: Scheduler timed out in OSScheduleThread  %u --> %u   IF=%u  <-%u<%u<%u<%u<%u<%u<%u<%u<-\n",
                                                OSPrioCur, OSPrioHighRdy, virtualInterruptFlag,
                                                taskLog[0], taskLog[1], taskLog[2], taskLog[3],
                                                taskLog[4], taskLog[5], taskLog[6], taskLog[7]
                                                );
            DBGPRINT(0x00000040, temp);
            MessageBox(NULL, temp, "UCOS-II WIN32", MB_OK | MB_SETFOREGROUND | MB_ICONERROR);		//In case of timeout, display an error message ...
            OSRunning=0;

            SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlBreakHandler, FALSE);
            exit(-1);									//... and exit
        }
        EnterCriticalSection(&criticalSection);
        oldPrio =OSPrioCur;
        nextPrio=OSPrioHighRdy;
        OSTCBCur  = OSTCBHighRdy;
        OSPrioCur = OSPrioHighRdy;

        idleTrigger = TRUE;
        DBGPRINT(0x00000001, "*** OSScheduleThread from %2u to %2u\n", oldPrio, nextPrio);
        hDelTask=NULL;
        
        if (OSTCBPrioTbl[oldPrio] == NULL)
        {   hDelTask = hTaskThread[oldPrio];
            hTaskThread[oldPrio] = NULL;
            taskSuspended[oldPrio]=0;
        } else if (oldPrio != nextPrio && taskSuspended[oldPrio]==0)			//If switching context to a new task ...
        {   SuspendThread(hTaskThread[oldPrio]);					//... suspend the thread associated with the current task
            taskSuspended[oldPrio]++;
        }

        if (taskSuspended[nextPrio])
        {   taskSuspended[nextPrio]--;
            if (taskSuspended[nextPrio] < 0)
                taskSuspended[nextPrio]=0;
            if (virtualInterruptFlag==FALSE)
                OSEnableInterruptFlag();
            OSLog(nextPrio);
            ResumeThread(hTaskThread[nextPrio]);					//... and resume the thread associated with the new task
        } else
        {   if (virtualInterruptFlag==FALSE)
                OSEnableInterruptFlag();
        }
        LeaveCriticalSection(&criticalSection);
        if (hDelTask)
        {   CloseHandle(hDelTask);
        }
    }
}

// OSInterruptThread **********************************************************
// Time tick interrupt processing
void OSInterruptThread(INT32U param)
{   char temp[256];
    INT32U eventType;

    DBGPRINT(0x00000001, "*** OSInterruptThread First Call\n");

    while (1)
    {   //if (WaitForSingleObject(hInterruptEvent, OS_INTERRUPT_TIMEOUT) == WAIT_TIMEOUT)	//Wait for a timer interrupt event
    	eventType=WaitForMultipleObjects(NINTERRUPTS, hInterruptEvent, FALSE, OS_INTERRUPT_TIMEOUT);
    	if (eventType == WAIT_TIMEOUT)	//Wait for a timer interrupt event
        {   sprintf(temp, "ERROR: Interrupt timed out in OSInterruptThread   IF=%u\n", virtualInterruptFlag);
            DBGPRINT(0x00000040, temp);
            MessageBox(NULL, temp, "UCOS-II WIN32", MB_OK | MB_SETFOREGROUND | MB_ICONERROR);		//In case of timeout, display an error message ...
            OSRunning=0;
            SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlBreakHandler, FALSE);
            exit(-1);									//... and exit
        }
        EnterCriticalSection(&criticalSection);
        DBGPRINT(0x00000001, "*** OSInterruptThread\n");
        if (virtualInterruptFlag==FALSE)
            DBGPRINT(0x00000001, "*** virtualInteruptFlag disabled when calling OSInterruptThread XXX %d\n", virtualInterruptFlag);
        DBGPRINT(0x00000001, "--- OSIntEnter\n");
        OSIntEnter();

        DBGPRINT(0x00000001, "--- OSTimeTick\n");

        if (eventType==WAIT_OBJECT_0)							//Call uCOS' interrupt processing
        {   ResetEvent(hInterruptEvent[0]);
            interruptTable[0]();							//IRQ0 reserved vor time tick: OSTimeTick();
        } else if (eventType==WAIT_OBJECT_0+1)
        {   ResetEvent(hInterruptEvent[1]);
            interruptTable[1]();
        } else if (eventType==WAIT_OBJECT_0+2)
        {   ResetEvent(hInterruptEvent[2]);
            interruptTable[2]();
        } else if (eventType==WAIT_OBJECT_0+3)
        {   ResetEvent(hInterruptEvent[3]);
            interruptTable[3]();
        } else if (eventType==WAIT_OBJECT_0+4)
        {   ResetEvent(hInterruptEvent[4]);
            interruptTable[4]();
        } else if (eventType==WAIT_OBJECT_0+5)
        {   ResetEvent(hInterruptEvent[5]);
            interruptTable[5]();
        } else if (eventType==WAIT_OBJECT_0+6)
        {   ResetEvent(hInterruptEvent[6]);
            interruptTable[6]();
        } else if (eventType==WAIT_OBJECT_0+7)
        {   ResetEvent(hInterruptEvent[7]);
            interruptTable[7]();
        } else
        {   MessageBox(NULL, temp, "UCOS-II WIN32", MB_OK | MB_SETFOREGROUND | MB_ICONERROR);		//In case of timeout, display an error message ...
        }
        DBGPRINT(0x00000001, "--- OSIntExit\n");
        OSIntExit();
        LeaveCriticalSection(&criticalSection);
    }
}


/*
*********************************************************************************************************
   uCOS-II Functions
*********************************************************************************************************
*/

// OSTimeTickInit ************************************************************
// Initialize the WIN32 multimedia timer to simulate time tick interrupts
void OSTimeTickInit()
{   TIMECAPS timecaps;

    DBGPRINT(0x00000008, "*** OSTimeTickInit\n");

    if (timeGetDevCaps((LPTIMECAPS) & timecaps, sizeof(timecaps)) != TIMERR_NOERROR)
    {   printf("uCOS-II ERROR: Timer could not be installed 1\n");
        exit(-1);
    }
    if (timeBeginPeriod(timecaps.wPeriodMin) != TIMERR_NOERROR)
    {   printf("uCOS-II ERROR: Timer could not be installed 2\n");
        exit(-1);
    }
    if (timeSetEvent(1000 / OS_TICKS_PER_SEC, 0, (LPTIMECALLBACK) OSTimeTickCallback, 0, TIME_PERIODIC) == 0)
    {   printf("uCOS-II ERROR: Timer could not be installed 3\n");
        exit(-1);
    }
}

// OSTaskStkInit *************************************************************
// This function does initialize the stack of a task (this is only a dummy function
// for compatibility reasons, because this stack is not really used (except to refer
// to the task parameter *pdata)
OS_STK *OSTaskStkInit(void (*task) (void *pd), void *pdata, OS_STK *ptos, INT16U opt)
{   OS_STK *stk;

    DBGPRINT(0x00000010, "*** OSTaskStkInit\n");

    stk = (OS_STK *) ptos;                                      // Load stack pointer
    *--stk = (INT32U) pdata;					// Push pdata on the stack
    *--stk = (INT32U) task;					// Push the task start address on the stack

    return stk;
}


/*
*******************************************************************************
   Internal Hook functions (used by the WIN32 port, not user hookable)
*******************************************************************************
*/

// OSInitHookBegin ***********************************************************
// This hook is invoked at the beginning of the OS initialization. MUST NOT BE DEFINED BY THE APPLICATION.
void OSInitHookBegin()
{   int i;
    char temp[256];
    DBGPRINT(0x00000010, "*** OSInitHookBegin\n");

    hScheduleEvent  = CreateEvent(NULL, FALSE, FALSE, NULL);	//Create the scheduler and interrupt event

    for (i=0; i< NINTERRUPTS; i++)
    {	sprintf(temp,"OSirq%u",i);
    	hInterruptEvent[i] = CreateEvent(NULL, TRUE, FALSE, temp);
        interruptTable[i]  = OSDummyISR;
    }

    InitializeCriticalSection(&criticalSection);
    hScheduleThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) OSScheduleThread,  NULL, 0, (INT32U*) &i);	//Create the scheduler and interrupt thread
    hInterruptThread= CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) OSInterruptThread, NULL, 0, &interruptThreadId);
    interruptTable[0] = OSTimeTick;

//  SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
    SetThreadPriority(hScheduleThread,  THREAD_PRIORITY_HIGHEST);//Set the scheduler and interrupt threads to maximum WIN32 priority
    SetThreadPriority(hInterruptThread, THREAD_PRIORITY_HIGHEST);

    SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlBreakHandler, TRUE);
    Sleep(0);							//Give Windows a chance to start the scheduler and interrupt thread

    //ADD YOUR OWN HOOK CODE HERE IF NECESSARY
}

// OSTaskCreateHook **********************************************************
// This hook is invoked during task creation. MUST NOT BE DEFINED BY THE APPLICATION.
void OSTaskCreateHook(OS_TCB * pTcb)
{   int i;

    DBGPRINT(0x00000004, "*** OSTaskCreateHook %u\n", pTcb->OSTCBPrio);

    hTaskThread[pTcb->OSTCBPrio] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) *(pTcb->OSTCBStkPtr),	//Map uCOS-II task to WIN32 thread
                                                (void *) *(pTcb->OSTCBStkPtr + 1), CREATE_SUSPENDED, (INT32U*) &i);
    taskSuspended[pTcb->OSTCBPrio]=1;									//Create thread in WIN32 suspended state

    if (OS_BOOST_WIN32_PRIORITY && (pTcb->OSTCBPrio!=OS_LOWEST_PRIO))           //Boost WIN32 thread priorites (except idle thread)
        SetThreadPriority(hTaskThread[pTcb->OSTCBPrio], THREAD_PRIORITY_ABOVE_NORMAL);

    //ADD YOUR OWN HOOK CODE HERE IF NECESSARY
}

// OSTaskIdleHook ************************************************************
// This hook is invoked from the idle task. MUST NOT BE DEFINED BY THE APPLICATION.
void OSTaskIdleHook()
{   if (idleTrigger)						//Issue a debug message, each time the idle task is reinvoked
    {   DBGPRINT(0x00000020, "*** OSTaskIdleHook\n");
        idleTrigger = FALSE;
    }

    //ADD YOUR OWN HOOK CODE HERE IF NECESSARY

    Sleep(0);							//Give Windows a chance to run other applications to when uCOS-II idles
}

/*
*******************************************************************************
   Hook functions (user hookable)
*******************************************************************************
*/

#if OS_CPU_HOOKS_EN > 0

/*
void OSInitHookBegin()                  MUST NOT BE DEFINED BY THE APPLICATION, see above.
*/

// OSInitHookEnd *************************************************************
// This hook is invoked at the end of the OS initialization.
void OSInitHookEnd()
{
}

/*
void OSTaskCreateHook(OS_TCB * pTcb)    MUST NOT BE DEFINED BY THE APPLICATION, see above.
*/

// OSTaskDelHook *************************************************************
// This hook is invoked during task deletion.
void OSTaskDelHook(OS_TCB * pTcb)
{
}

// OSTCBInitHook
// This hook is invoked during TCB initialization
void OSTCBInitHook(OS_TCB * ptcb)
{
}

/*
void OSTaskIdleHook()                   MUST NOT BE DEFINED BY THE APPLICATION, see above.
*/

// OSTaskStatHook ************************************************************
// This hook is invoked by the statistical task every second.
void OSTaskStatHook()
{
}

// OSTimeTickHook ************************************************************
// This hook is invoked during a time tick.
void OSTimeTickHook()
{
}

// OSTaskSwHook **************************************************************
// This hook is invoked during a task switch.
// OSTCBCur points to the current task (being switched out).
// OSTCBHighRdy points on the new task (being switched in).
void OSTaskSwHook()
{
}

#else
#pragma message("INFO: Hook functions must be defined in the application")
#endif

/*
*******************************************************************************
   Internal Task switch functions
*******************************************************************************
*/

// OSStartHighRdy *************************************************************
// Start first task
void OSStartHighRdy(void)
{   OSTaskSwHook();						//Call the task switch hook function

    OSTimeTickInit();                                           //Initialize time ticks

    // Increment OSRunning by 1
    OSRunning++;

    DBGPRINT(0x00000002, "*** OSStartHighRdy   from %2u to %2u\n", OSPrioCur, OSPrioHighRdy);

    SetEvent(hScheduleEvent);                                   //Trigger scheduling thread

    while(OSRunning)						//A redundant idle thread, in case the uCOS
    {   Sleep(0);						//scheduler does not invoke the uCOS idle task
//    	DBGPRINT(0x00000002, "*** OSStartHighRdy   Idle loop\n");
    }
    printf("INFO: Primary thread killed - exiting\n");
}

// OSCtxSw ********************************************************************
// Task context switch
void OSCtxSw(void)
{   OSTaskSwHook();						//Call the task switch hook function

    DBGPRINT(0x00000002, "*** OSCtxSw          from %2u to %2u\n", OSPrioCur, OSPrioHighRdy);

    SetEvent(hScheduleEvent);                                   //Trigger scheduling thread
    Sleep(0);							//... and give Windows a chance to invoke it
}

// OSIntCtxSw *****************************************************************
// Interrupt context switch
void OSIntCtxSw(void)
{   OSTaskSwHook();						//Call the task switch hook function

    DBGPRINT(0x00000002, "*** OSCIntCtxSw      from %2u to %2u\n", OSPrioCur, OSPrioHighRdy);

    SetEvent(hScheduleEvent);                                   //Trigger scheduling thread
    Sleep(0);							//... and give Windows a chance to invoke it
}

