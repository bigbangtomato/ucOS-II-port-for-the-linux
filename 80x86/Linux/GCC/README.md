uCOS-II - Port for the LINUX Operating System
=============================================

### Copyright (C) 2004-... Werner.Zimmermann|AT|fht-esslingen.de

* * *

Note for Linux Kernel 2.6.x Users: Beginning with uCOS-II Linux Port V2.0 should be compatible with Linux kernel 2.6.x. In Linux kernel 2.6.x there were some incompatible modifications to the kernel's threading implementation which broke uCOS-II Linux Port V1.x.
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

* * *

Overview
--------

uCOS-II is a small, yet powerful real-time operating system developed by Jean J. Labrosse and described in his book "MicroC/OS-II - The Real-Time-Kernel", Second Edition, published by CMP Books. The operating system is highly CPU independent and has been ported to numerous microprocessor platforms (see [www.ucos-ii.com](http://www.ucos-ii.com)). The source code as provided together with the book and via the web site may be freely used for non-commercial applications and educational purposes. Licenses for commercial use are available too.

This document describes the LINUX port of uCOS-II, which allows to run uCOS-II applications under LINUX. As the port relies on LINUX' process and thread management features, the uCOS-II LINUX port does \*\*\*NOT\*\*\* turn LINUX into a real-time operating system! It was developed for didactic purposes to teach students at the Esslingen University of Applied Sciences, Esslingen, Germany the basics of real-time operating systems with an insight to OS internal implementation. This approach was chosen, because students do have much easier access to PCs at home or at university computing centers rather than to proprietary embedded systems. Powerful PC-based development tools like GCC are available free of charge as compared to costly embedded system cross-compilers and debuggers. Thus real-time system mechanisms can be studied with uCOS-II under LINUX providing a soft real-time-environment on PCs at home or in the university computer center. If the basic functionality of the application is working correctly, the application can be easily ported to an embedded system later in the lab.

*   **[Get the latest version of uCOS-II LINUX and WIN32 ports](http://www.fht-esslingen.de/~zimmerma/software/index_uk.html)**
*   [Release Notes](#Release)
*   [Installation and Running the Samples](#Installation)
*   [LINUX Port Internals and Limitations](#Internals)
*   [Disclaimer and Copyright Information](#Disclaimer)

  

Release Notes
-------------

Only the port specific files of uCOS-II (and some examples) are provided here. It is assumed, that you do have the uCOS-II platform-independent source code as available together with the book or via www.ucos-ii.com.

The port was tested with the uCOS-II sources V2.52, which are distributed together with the second edition of Jean J. Labrosse's book. You will need GNU GCC C-compiler to compile the LINUX samples provided here. The port has been developed using GCC 3.1. The LINUX port is based on the WIN32 port of the same author, so in many points a solution, which allowed to make the WIN32 and LINUX port match as closely as possible instead of a more elegant LINUX solution was chosen.

V1.5 is the first published version of the LINUX port, as I chose to have the same version number for WIN32 and Linux.

In V1.6 the resolution of the time measurement PC_Elapsed... in the Cygwin GCC and LINUX version was improved. Now these ports also use the Pentium time stamp counter.

In V2.0 signal handling was modified to adapt it to Linux kernel version 2.6.x. Older versions did not run with that kernel, because the Linux thread implementation saw some changes, which were not backward compatible. In V2.0 instead of getpid() now gettid() and instead of kill() now tkill() is used. Real code changes were kept small by using #define in os_cpu.h.

V2.1 some include paths were modified to work with uCOS-II 2.70.

  

Installation, Running and Recompiling the Samples
-------------------------------------------------

You should **unpack** the files of the LINUX port into the uCOS-II installation directory (normally "/software/uCOS-II/"), keeping the directory structure of the LINUX port intact. If you installed uCOS-II in "/software/uCOS-II" with uCOS-II source files in "/software/uCOS-II/source", you will find the LINUX port for GCC in "/software/uCOS-II/Ports/80x86/Linux/GCC/" with the source code of the port files in subdirectory "./src", some examples under "./examples/" and some template files as starting point for your own applications in "./templates/".

To **run** one of the samples, CD to the appropriate sample's subdirectory, e.g. ".\\examples\\example1" and run the file "./test". All sample programs can be stopped by hitting the "ESC" key. The samples "example1", ..., "example4" are described in Jean J.Labrosse's book in chapter 1.

To **recompile** one of the samples, go to the appropriate sample's subdirectory and type "make". If you did not install uCOS-II or the LINUX port as recommended, you may need to edit the sample's "makefile", where paths are set in the makefile variables UCOS\_SRC, UCOS\_PORT\_SRC and UCOS\_PORT_EX.

If you get some 'file not found' errors, check your path and file names, especially watch out for upper/lower case letter bugs and for '\\' - '/' bugs . This especially happens, when copying files from Windows to LINUX, because LINUX is case-sensitive, whereas Windows is not and uses a different path delimiter.

As a starting point for **your own applications** you may use the template files in subdirectory "./templates". To get familiar with uCOS-II read Jean J. Labrosse's book. To learn about the limitations of the LINUX port, you should read the section about [LINUX port internals and limitations](#Internals) in this document.

There are several support functions for displaying characters or strings and measuring execution times and installing (simulated) "interrupt" service routines on the PC platform, see PC.c and PC.h in the "./src" subdirectory. Where possible, these functions were modeled according to the functions provided in Jean J. Labrosse's 80x86 real mode port.

  

* * *

LINUX Port Internals and Limitations
------------------------------------

*   **Task Scheduling**

As LINUX does not allow application programs to bypass its own scheduler or provide their own timers and interrupt handlers, uCOS-II scheduling and task managment functions must be remapped to LINUX functions.

The LINUX port is based on the WIN32 port of the same author, so in many points a solution, which allowed to make the WIN32 and LINUX port match as closely as possible instead of a more elegant LINUX solution was chosen. The WIN32 port uses WIN32 threads, which are suspended and resumed via WIN32 API functions and scheduling is triggered by WIN32 events. The LINUX port uses the same mechanisms with the system call clone() to create threads and LINUX signals instead of WIN32 events.

uCOS-II tasks are mapped to LINUX threads. The mapping is done, when a task is created in OSTaskCreateHook(). Additionally two special LINUX threads OSScheduleThread() for thread scheduling handling and OSInterruptThread() for interrupt handling are created in OSInitHookBegin(). The scheduling and the interrupt thread are created with the a higher LINUX process priority. The threads associated with uCOS-II tasks are created with a lower priority and set to suspended state (via signal SIGSTOP).

When the uCOS-II scheduler preempts a task and wants to switch context to a new uCOS-II task, the uCOS-II context switch function OSCtxSw() or OSIntCtxSw() triggers the scheduler thread via a signal hScheduleEvent (=SIGCONT). Because the scheduling thread OSScheduleThread() has a higher LINUX priority than all the WIN32 threads associated with uCOS-II tasks, LINUX will start the scheduling thread immediately. This thread will suspend the LINUX thread associated with the preempted uCOS-II task and resume the LINUX thread associated with the new uCOS-II task. Then the scheduler thread will wait for the next scheduler event (via system call pause()). So always exactly one LINUX thread associated with a uCOS-II task will be ready to run, all other uCOS-II task associated threads will be in LINUX suspended state. Thus this LINUX thread will be invoked by the LINUX operating system's internal scheduler, once the OSScheduleThread() waits for the next scheduling event, and will remain running until the uCOS-II scheduler is invoked the next time and retriggers the OSScheduleThread().

*   **Time Tick Interrupts**

To generate the uCOS-II time tick, a LINUX timer is used. Theoretically this timer provides a 10ms resolution (LINUX internal time tick). Therefore your application should set OS\_TICKS\_PER_SECOND to not more than 100 ticks per second (i.e. to a tick period of 10ms min.). The timer will be started automatically via OSStartHighRdy() and OSTimeTickInit(), once your application starts uCOS-II multitasking by calling OSStart().

LINUX periodically invokes a the signal handler OSTimeTickCallback() for the timer signal SIGALARM, which triggers the signal hInterruptEvent. Due to this signal, LINUX immediately invokes the interrupt thread OSInterruptThread(), because this thread has a higher process priority than all uCOS-II task related threads. As expected by uCOS-II, the interrupt thread calls OSTimeTick(), which may reschedule another uCOS-II task. Then the interrupt thread waits for the next interrupt event.

uCOS-II's critical sections normally disable interrupts. LINUX application programs, however, cannot disable interrupts. This issue is resolved by LINUX suspending the interrupt thread instead, once a uCOS-II critical section is entered by OS\_ENTER\_CRITICAL().

*   **Limitations**

uCOS-II provides several **hook functions** to allow applications to modify the uCOS internal behaviour. Some of these hook functions are used to interact with the LINUX operating system in the LINUX port. Thus the following hooks MUST NOT be used by your application:

	    OSInitHookBegin(), OSTaskCreateHook(), OSTaskIdleHook()
	    

If your application must execute own code when uCOS-II calls these hooks, you should add this code in file os\_cpu\_c.c. The appropriate points are marked in the code by

	    //ADD YOUR OWN HOOK CODE HERE IF NECESSARY
	    

All other hook functions can be redefined by your application directly if you set OS\_HOOKS\_EN = 0, as described in the uCOS-II documentation.

uCOS-II tasks are mapped to LINUX threads. These threads use their own LINUX **stacks** rather than the 'normal' uCOS-II task stacks. Stack size and stack management during context switches are automatically handled by LINUX. Nevertheless for compatibility reasons, when a uCOS-II task is created, a uCOS-II stack must be provided. Only a few bytes of this stack will be initialized, but this stack will not really be used by the running tasks. If you plan to test your application under LINUX and then port it to an embedded system, you \*\*\*CANNOT\*\*\* use the LINUX simulation to find out the necessary stack size. Your uCOS-II LINUX application should not rely on the uCOS-II stack checking and clearing functions and should not use any stack related programming tricks.

  

* * *

Disclaimer and Copyright Information
------------------------------------

All files are provided in the hope, that they might be useful, but **without any warranty or support**. You may **use them on your own risk in non-commercial applications**. Even if this software is provided free of charge, it is **no freeware**. It is copyrighted by Werner.Zimmermann|AT|fht- esslingen.de, but may be freely used for non-commercial applications. If you do modify the source code and/or implement additional features, you are expected to provide the author with a copy of your source code and a description of the changes.

If you want **to use this software in commercial applications, contact the author**.

In case you have problems, you may email a detailed description of the problem to Werner.Zimmermann|AT|fht-esslingen.de. But the author does not guarantee, that he will answer your mail or solve the problem.

uCOS-II, Windows , LINUX and other product names mentioned here may be trademarks or registered trademarks of their respective owners.

* * *
