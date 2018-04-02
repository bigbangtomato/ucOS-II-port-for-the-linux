/*
*********************************************************************************************************
*                                          Generate interrupt events
*                                Used to simulate interrupts for uC/OS-II LINUX PORT
*
*                          (c) Copyright 2004-2005, Werner.Zimmermann@fht-esslingen.de
*                                           All Rights Reserved
*
* File : irqGenerator.C
* By   : Werner Zimmermann
*********************************************************************************************************
*/


#include <signal.h>

int irqSignal= 0;
int irqNum   = 0;
int irqPid   = 0;

int main(int argc, char *argv[])
{   char irqName[256];

    if (argc < 3)
    {   printf("Usage: irqGenerator <pid of thread> <number of irq 0 ... 7>\n");
        return -1;
    }

    irqPid=atoi(argv[1]);
    irqNum=atoi(argv[2]);
    
	
    if (irqNum==0) 	
        irqSignal=SIGUSR1;
    else if (irqNum==1) 
	irqSignal=SIGUSR2;
    else
        irqSignal = SIGRTMIN+10+irqNum;				
    
    if (kill(irqPid, irqSignal))				//Trigger the "interrupt"
    {   printf("Could not trigger thread %u with signal %u (IRQ %u) - exiting\n", irqPid, irqSignal, irqNum);
        return -2;
    }
    
    return 0;
}
