/*
*********************************************************************************************************
*                                                uC/OS-II
*                                The Real-Time Kernel (by Jean J. Labrosse)
*
*                                             LINUX PORT
*
*                          (c) Copyright 2004-2005, Werner.Zimmermann@fht-esslingen.de
*                                           All Rights Reserved
*
* File : OS_CPU_C.C
* By   : Werner Zimmermann
*
* Changes:  1.50 First Release of LINUX Port - same version number as WIN32 port
*           1.60 Increased resolution of PC_Elapsed... time measurement. Now uses the Pentium
*                time stamp counter.
*           2.00 Major change of signal generation and handling to adapt the Linux port to
*		 threading changes in Linux kernel 2.6.x. Use gettid() instead getpid() and
*		 tkill() instead of kill() because clone() changed.
*	    2.10 Include paths modified to work with uCOS-II 2.70
*
*********************************************************************************************************
*/
#define OS_PORT_VERSION 210					//Version number of the uCOS-II LINUX port

#define SHOWTID 1

/*
*********************************************************************************************************
   Includes
*********************************************************************************************************
*/

#include    <stdio.h>
#include    <stdlib.h>
#include    <stdarg.h>
#include    <sys/time.h>
#include    <sys/resource.h>
#include    <sys/sem.h>
#include    <signal.h>
#include    <sched.h>
#include    <assert.h>
#include    <linux/unistd.h>
#include    <asm/unistd.h>

#if (OS_VERSION <= 270)
#include    "os_cpu.h"
#include    "os_cfg.h"
#include    "ucos_ii.h"
#else
#include    "ucos_ii.h"
#endif


typedef struct itimerval ITIMERVAL;
typedef int (*LPTHREAD_START_ROUTINE)(void*);

/*
*********************************************************************************************************
   Global variables
*********************************************************************************************************
*/

INT32S  hMainThread;
INT32S  hScheduleEvent, hScheduleThread;			//Scheduler thread variables
INT32S  hTaskThread[OS_LOWEST_PRIO + 2];			//Map uCOS tasks to LINUX threads
INT8S   taskSuspended[OS_LOWEST_PRIO + 2];			//Suspend status of mapped tasks/threads

#define NINTERRUPTS 8						//Number of interrupt events (DO NOT CHANGE)
INT32S  hInterruptEvent[NINTERRUPTS], hInterruptThread;		//Interrupt handling thread variables

void (*interruptTable[NINTERRUPTS])();
BOOLEAN virtualInterruptFlag=TRUE;
INT32S eventType = 0;
sigset_t interruptSignalSet;


INT32S criticalSection;


BOOLEAN idleTrigger = TRUE;					//Trigger a message, when the idle task is
								//invoked (used for debugging purposes)

#define NLOG	16						//Log last scheduled tasks (used for
INT16U taskLog[NLOG];						//... debugging purposes)

/*
*********************************************************************************************************
   Port-specific functions
*********************************************************************************************************
*/


void InitializeCriticalSection(INT32S *sema)
{   *sema = semget(1, 1, IPC_CREAT | 0666);
    semctl(*sema, 0, SETVAL, 1);
}

void EnterCriticalSection(INT32S *sema)
{   struct sembuf op;
    op.sem_num=0;
    op.sem_flg=0;
    op.sem_op=-1;   
}

void LeaveCriticalSection(INT32S *sema)
{   struct sembuf op;
    op.sem_num=0;
    op.sem_flg=0;
    op.sem_op=+1;   
}

// DBGPRINT ******************************************************************
// Debug output
void DBGPRINT(INT32U debugLevel, const char *fmt,...)
{
    va_list argptr;
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
// Return the version number of the uCOS-II LINUX port
INT16U OSPortVersion(void)
{    return OS_PORT_VERSION;
}


void CtrlBreakHandler(int ctrl)
{
    if (ctrl==SIGTSTP)					//Handler if CTRL-D is pressed
    {	printf("---Exiting OSPrioCur=%u-------------\n", OSPrioCur);	//---Display current task priority
    } else if (ctrl==SIGINT)				//Handler if CTRL-C is pressed
    {	printf("---Exiting OSPrioCur=%u-------------\n", OSPrioCur);	//---Display current task priority and exit
 	exit(0);
    }
}


// OSEnableInterruptFlag ****************************************************
// Enable the interrupt flag
void OSEnableInterruptFlag(void)
{

    if (virtualInterruptFlag==FALSE)				//If the timer interrupt previously was disabled,
    {   virtualInterruptFlag=TRUE;
	if (getpid()!=hInterruptThread)
	    sigprocmask(SIG_UNBLOCK, &interruptSignalSet, NULL);//... resume the interrupt thread
    }

    DBGPRINT(0x00000080, ">>> ODEnableInterruptFlag %2d\n", virtualInterruptFlag);
}

// OSDisableInterruptFlag ****************************************************
// Disable the Interrupt Flag
void OSDisableInterruptFlag(void)
{
    if (virtualInterruptFlag==TRUE)				//If the timer interrupt previously was enabled,
    {   
    	if (getpid()!=hInterruptThread)				//... suspend the interrupt thread ...
	    sigprocmask(SIG_BLOCK, &interruptSignalSet, NULL);  
	virtualInterruptFlag=FALSE;
    }

    DBGPRINT(0x00000080, ">>> OSDisableInterrupts   %2d\n", virtualInterruptFlag);
}

void OSDummyISR(void)
{   perror("Got unsupported interrupt");
}

// OSTickTickCallback ********************************************************
// Callback function for time tick 'interrupt', acts as a relay to OSTickISR
void OSTimeTickCallback(int para)
{    DBGPRINT(0x00000008, "*** OSTimeTickCallback\n");
     DBGPRINT(0x00000008, "--- Sending signal to PID=%u\n", hInterruptThread);
     kill(hInterruptThread, hInterruptEvent[0]);		//Trigger the interrupt thread
}


// OSScheduleThread ***********************************************************
// Start tasks, triggered by hScheduleEvent
void OSScheduleEventHandler(int val)
{   //dummy handler, necessary for pause() in OSScheduleThread()
    DBGPRINT(0x00000001, "--- OSScheduleEventHandler ---\n");
}

void OSScheduleThread(int param)
{   char temp[256];
    INT16U oldPrio, nextPrio;

    DBGPRINT(0x00000001, "*** OSScheduleThread First Call %u  %u  PID=%u  TID=%u\n", oldPrio, nextPrio, getpid(), gettid());

    while (1)
    { 	DBGPRINT(0x00000001, "--- OSScheduleThread: Going to pause ---\n");
    	pause();
	DBGPRINT(0x00000001, "--- OSScheduleThread: Resume from pause ---\n");

	EnterCriticalSection(&criticalSection);

	oldPrio =OSPrioCur;
        nextPrio=OSPrioHighRdy;
        OSTCBCur  = OSTCBHighRdy;
        OSPrioCur = OSPrioHighRdy;

        idleTrigger = TRUE;
        DBGPRINT(0x00000001, "*** OSScheduleThread from %2u to %2u  PID=%u  TID=%u\n", oldPrio, nextPrio, getpid(), gettid());

        if (oldPrio != nextPrio && taskSuspended[oldPrio]==0)				//If switching context to a new task ...
        {   DBGPRINT(0x00000001, "--- Sending signal SIGSTOP to PID=%u\n", hTaskThread[oldPrio]);
	    kill(hTaskThread[oldPrio], SIGSTOP);					//... suspend the thread associated with the current task
	    taskSuspended[oldPrio]++;
        }

        if (taskSuspended[nextPrio])
        {   taskSuspended[nextPrio]--;
            if (taskSuspended[nextPrio] < 0)
                taskSuspended[nextPrio] = 0;
            if (virtualInterruptFlag==FALSE)
                OSEnableInterruptFlag();
            OSLog(nextPrio);

	    DBGPRINT(0x00000001, "--- Sending signal SIGCONT to PID=%u\n", hTaskThread[nextPrio]);
	    kill(hTaskThread[nextPrio], SIGCONT);					//... and resume the thread associated with the new task
	} else
        {   if (virtualInterruptFlag==FALSE)
                OSEnableInterruptFlag();
        }
	LeaveCriticalSection(&criticalSection);

    }
}



// OSInterruptThread **********************************************************
// Time tick interrupt processing
void OSInterruptEventHandler(int val)
{   eventType=val;
    //dummy handler, necessary for pause() in OSInterruptThread()
    DBGPRINT(0x00000001, "--- OSInterruptEventHandler ---\n");
}

void OSInterruptThread(int param)
{   char temp[256];

    DBGPRINT(0x00000001, "*** OSInterruptThread First Call  PID=%u  TID=%u\n", getpid(), gettid());

    while (1)
    {  	DBGPRINT(0x00000001, "--- OSInterruptThread: Going to pause ---\n");
    	pause();
	DBGPRINT(0x00000001, "--- OSInterruptThread: Resume from pause ---\n");

	EnterCriticalSection(&criticalSection);

	DBGPRINT(0x00000001, "*** OSInterruptThread  PDI=%x  TID=%u\n", getpid(), gettid());
        if (virtualInterruptFlag==FALSE)
            DBGPRINT(0x00000001, "*** virtualInteruptFlag disabled when calling OSInterruptThread XXX %d\n", virtualInterruptFlag);
        DBGPRINT(0x00000001, "--- OSIntEnter\n");
        OSIntEnter();

        DBGPRINT(0x00000001, "--- OSTimeTick\n");

        if (eventType==hInterruptEvent[0])						//Call uCOS' interrupt processing
        {   interruptTable[0]();							//IRQ0 reserved vor time tick: OSTimeTick();
 	} else if (eventType==hInterruptEvent[1])
        {   interruptTable[1]();
        } else if (eventType==hInterruptEvent[2])
        {   interruptTable[2]();
        } else if (eventType==hInterruptEvent[3])
        {   interruptTable[3]();
        } else if (eventType==hInterruptEvent[4])
        {   interruptTable[4]();
        } else if (eventType==hInterruptEvent[5])
        {   interruptTable[5]();
        } else if (eventType==hInterruptEvent[6])
        {   interruptTable[6]();
        } else if (eventType==hInterruptEvent[7])
        {   interruptTable[7]();
        } else
        {  perror("OSInterruptThread(): Unsupported interrupt\n");
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

// OSStopThreads *************************************************************
// Stop all threads before exiting the program
void OSStopThreads(void)
{   DBGPRINT(0x00000001, "--- Sending signal SIGKILL to PID=%u\n", hMainThread);
    kill(hMainThread, SIGKILL);
}

// OSTimeTickInit ************************************************************
// Initialize the LINUX multimedia timer to simulate time tick interrupts
void OSTimeTickInit()
{   ITIMERVAL timerVal;

    DBGPRINT(0x00000008, "*** OSTimeTickInit\n");

    signal(SIGALRM, OSTimeTickCallback);

    timerVal.it_interval.tv_sec = 0;
    timerVal.it_interval.tv_usec = 1000000/ OS_TICKS_PER_SEC;
    timerVal.it_value=timerVal.it_interval;
    if (setitimer(ITIMER_REAL,&timerVal, NULL))
    {	printf("uCOS-II ERROR: Timer could not be installed 1\n");
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

    DBGPRINT(0x00000010, "*** OSTaskStkInit end\n");

    return stk;
}


/*
*******************************************************************************
   Internal Hook functions (used by the LINUX port, not user hookable)
*******************************************************************************
*/

// OSInitHookBegin ***********************************************************
// This hook is invoked at the beginning of the OS initialization. MUST NOT BE DEFINED BY THE APPLICATION.
void OSInitHookBegin()
{   int i;
    char temp[256];
    void *p, *q;
    p=malloc(TASKSTACKSIZE);
    p=p+TASKSTACKSIZE;
    q=malloc(TASKSTACKSIZE);
    q=q+TASKSTACKSIZE;

    DBGPRINT(0x00000010, "*** OSInitHookBegin  PID=%u  TID=%u\n", getpid(), gettid());

    hMainThread=getpid();

    hScheduleEvent = SIGCONT;
    signal(hScheduleEvent, OSScheduleEventHandler);
    sigemptyset(&interruptSignalSet);
    sigaddset(&interruptSignalSet, hScheduleEvent);

    for (i=0; i< NINTERRUPTS; i++)
    {	interruptTable[i]  = OSDummyISR;
	if (i==0) 					//Seems that Linux kernel 2.6.x does not support
	    hInterruptEvent[i]=SIGUSR1;			//real time signals SIGRTMIN ... ???
	else if (i==1)
	    hInterruptEvent[i]=SIGUSR2;
	else
            hInterruptEvent[i] = SIGRTMIN+10+i;
	signal(hInterruptEvent[i], OSInterruptEventHandler);
	sigaddset(&interruptSignalSet, hInterruptEvent[i]);
    }
    InitializeCriticalSection(&criticalSection);

    hScheduleThread=clone((LPTHREAD_START_ROUTINE) OSScheduleThread, p,		//Start the schedule thread
  					CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_VM, NULL);
    hInterruptThread=clone((LPTHREAD_START_ROUTINE) OSInterruptThread, q,	//Start the Interrupt thread
  					CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_VM, NULL);
    interruptTable[0] = OSTimeTick;
    atexit(OSStopThreads);

    DBGPRINT(0x00000010, "--- PID schedule thread = %u  interrupt thread = %u\n", hScheduleThread, hInterruptThread);

    if (setpriority(PRIO_PROCESS, hScheduleThread,  -10))			//Set the scheduler and interrupt threads to maximum LINUX priority
    	DBGPRINT(0x00000010, "Setting hScheduleThread priority failed\n");
    if (setpriority(PRIO_PROCESS, hInterruptThread, -10))
    	DBGPRINT(0x00000010, "Setting hInterruptThread priority failed\n");

    signal(SIGINT,  CtrlBreakHandler);
    signal(SIGTSTP, CtrlBreakHandler);

    sched_yield();						//Give Linux a chance to start the scheduler and interrupt thread

    //ADD YOUR OWN HOOK CODE HERE IF NECESSARY
}

typedef struct { BOOLEAN flag; LPTHREAD_START_ROUTINE routine; void *param; INT16U prio;} TASKSTART;
TASKSTART taskStart;

// OSGenericTaskRoutine ******************************************************
// Used to suspend a task after creation
int OSGenericTaskRoutine(void *param)
{   TASKSTART localTaskStart;					//Flag, that the new task has been started
    memcpy(&localTaskStart, param, sizeof(TASKSTART));
    taskStart.flag=TRUE;
    DBGPRINT(0x00000004, "--- Sending signal SIGSTOP to self %u ---\n", taskStart.prio);
    kill(getpid(), SIGSTOP);   					//Suspend the task
    DBGPRINT(0x00000004, "--- Continuing task %u ---\n", localTaskStart.prio);
    localTaskStart.routine(localTaskStart.param);		//Continue the task
}

// OSTaskCreateHook **********************************************************
// This hook is invoked during task creation. MUST NOT BE DEFINED BY THE APPLICATION.
void OSTaskCreateHook(OS_TCB *pTcb)
{   int i;
    void *p;
    DBGPRINT(0x00000004, "*** OSTaskCreateHook %u\n", pTcb->OSTCBPrio);
    p=malloc(TASKSTACKSIZE);
    p=p+TASKSTACKSIZE;
    
    taskStart.flag=FALSE;
    taskStart.routine=(LPTHREAD_START_ROUTINE) *(pTcb->OSTCBStkPtr);
    taskStart.param  =(void *)                 *(pTcb->OSTCBStkPtr + 1);
    taskStart.prio   = pTcb->OSTCBPrio;
    
    hTaskThread[pTcb->OSTCBPrio]=clone((LPTHREAD_START_ROUTINE) OSGenericTaskRoutine, p,	//Map uCOS-II task to LINUX thread
    					CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_VM,
					(void *) &taskStart);
    DBGPRINT(0x00000004, "--- PID new Task = %u\n", hTaskThread[pTcb->OSTCBPrio]);

    while(taskStart.flag==FALSE)
    	sched_yield();
    taskSuspended[pTcb->OSTCBPrio]=1;								//Create thread in LINUX suspended state

/*  Does not work ???
    if (OS_BOOST_LINUX_PRIORITY && (pTcb->OSTCBPrio!=OS_LOWEST_PRIO))          			 //Boost LINUX thread priorites (except idle thread)
        setpriority(PRIO_PROCESS, hTaskThread[pTcb->OSTCBPrio], -5);
*/

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

    sched_yield();						//Give Linux a chance to run other applications to when uCOS-II idles
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

    DBGPRINT(0x00000002, "--- Sending signal hScheduleEvent to PID=%u\n", hScheduleThread);
    kill(hScheduleThread,hScheduleEvent);                      	//Trigger scheduling thread

    while(OSRunning)						//A redundant idle thread, in case the uCOS
    {   //usleep(1000);						//scheduler does not invoke the uCOS idle task
        sched_yield();
//   	DBGPRINT(0x00000002, "*** OSStartHighRdy   Idle loop\n");
    }

    printf("ERROR: Primary thread killed - exiting\n");
}

// OSCtxSw ********************************************************************
// Task context switch
void OSCtxSw(void)
{   OSTaskSwHook();						//Call the task switch hook function

    DBGPRINT(0x00000002, "*** OSCtxSw          from %2u to %2u\n", OSPrioCur, OSPrioHighRdy);

    DBGPRINT(0x00000002, "--- Sending signal hScheduleEvent to PID=%u\n", hScheduleThread);
    kill(hScheduleThread,hScheduleEvent);                     	//Trigger scheduling thread
    sched_yield();						//... and give Linux a chance to invoke it
}

// OSIntCtxSw *****************************************************************
// Interrupt context switch
void OSIntCtxSw(void)
{   OSTaskSwHook();						//Call the task switch hook function

    DBGPRINT(0x00000002, "*** OSCIntCtxSw      from %2u to %2u\n", OSPrioCur, OSPrioHighRdy);

    DBGPRINT(0x00000002, "--- Sending signal hScheduleEvent to PID=%u\n", hScheduleThread);
    kill(hScheduleThread,hScheduleEvent);                      	//Trigger scheduling thread
    sched_yield();						//... and give Linux a chance to invoke it
}

