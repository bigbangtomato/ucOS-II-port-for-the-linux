/*
   *********************************************************************************************************
   *                               PC SUPPORT FUNCTIONS for __GNUC__ (WIN32 and LINUX)
   *
   *                          (c) Copyright 2004-2005, Werner.Zimmermann@fht-esslingen.de
   *                      (Functions similar to the 80x86 Real Mode port by Jean J. Labrosse)
   *                                           All Rights Reserved
   *
   * File : PC.C
   * By   : Werner Zimmermann
   *********************************************************************************************************
 */

#include "includes.h"

BOOLEAN lock=FALSE;
extern void (*interruptTable[])();


/*
   *********************************************************************************************************
   *                                        CHECK IF A KEY WAS HIT
   *********************************************************************************************************
*/
struct termios oldKbdMode;

void keyboardCooked(void)
{   tcsetattr(STDIN_FILENO, TCSANOW, &oldKbdMode);
}

void keyboardRaw(void)
{   static char init=0;
    struct termios newKbdMode;
    if (init)
    	return;
    tcgetattr(0, &oldKbdMode);
    memcpy(&newKbdMode, &oldKbdMode, sizeof(struct termios));
    newKbdMode.c_lflag &= ~(ICANON | ECHO);
    newKbdMode.c_cc[VTIME]=0;
    newKbdMode.c_cc[VMIN]=1;
    tcsetattr(0, TCSANOW, &newKbdMode);
    atexit(keyboardCooked);
    init=1;
}

int kbhit(void)
{   struct timeval tv;
    fd_set readFd;

    keyboardRaw();

    tv.tv_sec=0;
    tv.tv_usec=0;
    FD_ZERO(&readFd);
    FD_SET(STDIN_FILENO, &readFd);

    select(1, &readFd, NULL, NULL, &tv);

    if (FD_ISSET(STDIN_FILENO, &readFd))
    	return 1;
    else
    	return 0;
}

/*
   *********************************************************************************************************
   *                                        Translate colors
   *********************************************************************************************************
*/
void setColor(INT8U color)
{   switch (color & 0xF0)				//Foreground colors
    {  	case DISP_FGND_BLACK:	printf("\033[30m"); break;
	case DISP_FGND_RED:     printf("\033[31m"); break;
	case DISP_FGND_GREEN:   printf("\033[32m"); break;
	case DISP_FGND_YELLOW:  printf("\033[33m"); break;
	case DISP_FGND_BLUE: 	printf("\033[34m"); break;
	case DISP_FGND_MAGENTA: printf("\033[35m"); break;
	case DISP_FGND_CYAN:    printf("\033[36m"); break;
	case DISP_FGND_WHITE:   printf("\033[37m"); break;
	case DISP_FGND_GRAY:    printf("\033[30m"); break; //black, gray not implemented
    	default:		printf("\033[30m"); break;	//black
    }
    switch (color & 0x0F)				//Background colors
    {	case DISP_BGND_BLACK:   printf("\033[40m"); break;
	case DISP_BGND_RED:     printf("\033[41m"); break;
	case DISP_BGND_GREEN:   printf("\033[42m"); break;
	case DISP_BGND_YELLOW:  printf("\033[43m"); break;
	case DISP_BGND_BLUE:    printf("\033[44m"); break;
        case DISP_BGND_MAGENTA: printf("\033[45m"); break;
	case DISP_BGND_CYAN:    printf("\033[46m"); break;
	case DISP_BGND_WHITE:   printf("\033[47m"); break;
	case DISP_BGND_GRAY:    printf("\033[47m"); break;	//white, gray not implemented
	case DISP_BGND_LIGHT_GRAY:printf("\033[47m"); break;	//white, gray not implemented
	default:		printf("\033[47m"); break;	//white
    }
}

/*
   *********************************************************************************************************
   *                                        Move cursor
   *********************************************************************************************************
*/
void moveCursor(INT8U x, INT8U y)
{
    printf("\033[%u;%uH", y+1, x+1);
}


/*
   *********************************************************************************************************
   *                                        CHECK AND GET KEYBOARD KEY
   *
   * Description: This function checks to see if a key has been pressed at the keyboard and returns TRUE if
   *              so.  Also, if a key is pressed, the key is read and copied where the argument is pointing
   *              to.
   *
   * Arguments  : c     is a pointer to where the read key will be stored.
   *
   * Returns    : TRUE  if a key was pressed
   *              FALSE otherwise
   *********************************************************************************************************
 */
BOOLEAN PC_GetKey(INT16S *c)
{
    if (PC_CHECK_RECURSIVE_CALLS && lock)			// Check and avoid recursive calls
    {   perror("Recursive call in PC_GetKey");
        exit(-1);
    } else if (lock)
    {	return FALSE;
    } else
    {   lock = TRUE;
    }
    if (kbhit())						// See if a key has been pressed
    {   *c = (INT16S) getchar();                                // Get key pressed
        lock = FALSE;
        return (TRUE);
    } else
    {   *c = 0x00;                                              // No key pressed
        lock = FALSE;
        return (FALSE);
    }
}

/*
   *********************************************************************************************************
   *                                 DISPLAY A STRING  AT 'X' & 'Y' COORDINATE
   *
   * Description : This function writes an ASCII string anywhere on the PC's screen.  This function writes
   *               directly to video RAM instead of using the BIOS for speed reasons.  It assumed that the
   *               video adapter is VGA compatible.  Video RAM starts at absolute address 0x000B8000.  Each
   *               character on the screen is composed of two bytes: the ASCII character to appear on the
   *               screen followed by a video attribute.  An attribute of 0x07 displays the character in
   *               WHITE with a black background.
   *
   * Arguments   : x      corresponds to the desired column on the screen.  Valid columns numbers are from
   *                      0 to 79.  Column 0 corresponds to the leftmost column.
   *               y      corresponds to the desired row on the screen.  Valid row numbers are from 0 to 24.
   *                      Line 0 corresponds to the topmost row.
   *               s      Is the ASCII string to display.  You can also specify a string containing
   *                      characters with numeric values higher than 128.  In this case, special character
   *                      based graphics will be displayed.
   *               color  specifies the foreground/background color to use (see PC.H for available choices)
   *
   * Returns     : None
   *********************************************************************************************************
 */
void PC_DispStr(INT8U x, INT8U y, INT8U *s, INT8U color)
{
#ifdef DEBUG_PC
    printf("PC_DispStr: %s\n", s);
    return;
#endif

    if (PC_CHECK_RECURSIVE_CALLS && lock)			// Check and avoid recursive calls
    {   perror("Recursive call in PC_DispStr");
        exit(-1);
    } else if (lock)
    {	return;
    } else
    {   lock = TRUE;
    }

    if ((x>79) || (y>24))					// Check for valid cursor position
        perror("Invalid screen position in PC_DispStr()");

    moveCursor(x, y);						// Move cursor to x, y
    setColor(color);						// Set foreground/background color
    puts(s);							// Display text string
    lock = FALSE;
}

/*
   *********************************************************************************************************
   *                           DISPLAY A SINGLE CHARACTER AT 'X' & 'Y' COORDINATE
   *
   * Description : This function writes a single character anywhere on the PC's screen.  This function
   *               writes directly to video RAM instead of using the BIOS for speed reasons.  It assumed
   *               that the video adapter is VGA compatible.  Video RAM starts at absolute address
   *               0x000B8000.  Each character on the screen is composed of two bytes: the ASCII character
   *               to appear on the screen followed by a video attribute.  An attribute of 0x07 displays
   *               the character in WHITE with a black background.
   *
   * Arguments   : x      corresponds to the desired column on the screen.  Valid columns numbers are from
   *                      0 to 79.  Column 0 corresponds to the leftmost column.
   *               y      corresponds to the desired row on the screen.  Valid row numbers are from 0 to 24.
   *                      Line 0 corresponds to the topmost row.
   *               c      Is the ASCII character to display.  You can also specify a character with a
   *                      numeric value higher than 128.  In this case, special character based graphics
   *                      will be displayed.
   *               color  specifies the foreground/background color to use (see PC.H for available choices)
   *
   * Returns     : None
   *********************************************************************************************************
 */
void PC_DispChar(INT8U x, INT8U y, INT8U c, INT8U color)
{
#ifdef DEBUG_PC
    printf("PC_DispChar: %c\n", c);
    return;
#endif

    if (PC_CHECK_RECURSIVE_CALLS && lock)			// Check and avoid recursive calls
    {   perror("Recursive call in PC_DispChar");
        exit(-1);
    } else if (lock)
    {	return;
    } else
    {   lock = TRUE;
    }

    if ((x>79) || (y>24))					// Check for valid cursor position
        perror("Invalid screen position in PC_DispChar()");

    moveCursor(x, y);						// Move cursor to x, y
    setColor(color);						// Set foreground/background color
    putchar(c);							// Display character
    lock = FALSE;
}


/*
   *********************************************************************************************************
   *                                              CLEAR SCREEN
   *
   * Description : This function clears the PC's screen by directly accessing video RAM instead of using
   *               the BIOS.  It assumed that the video adapter is VGA compatible.  Video RAM starts at
   *               absolute address 0x000B8000.  Each character on the screen is composed of two bytes:
   *               the ASCII character to appear on the screen followed by a video attribute.  An attribute
   *               of 0x07 displays the character in WHITE with a black background.
   *

   * Arguments   : color   specifies the foreground/background color combination to use
   *                       (see PC.H for available choices)
   *
   * Returns     : None
   *********************************************************************************************************
 */
void PC_DispClrScr(INT8U color)
{
#ifdef DEBUG_PC
    printf("PC_DispClrScr\n");
    return;
#endif

    if (PC_CHECK_RECURSIVE_CALLS && lock)			// Check and avoid recursive calls
    {   perror("Recursive call in PC_DispClrScr");
        exit(-1);
    } else if (lock)
    {	return;
    } else
    {   lock = TRUE;
    }

    printf("\033[2J");						// Clear screen and move cursor to 1,1
    setColor(color);						// Set foreground/background color

    lock = FALSE;
}


/*
   *********************************************************************************************************
   * 	Helper functions for time measurement
   *********************************************************************************************************
*/

#define NTIMERS 16
static INT64U PC_ElapsedOverhead=0;
static INT64U PC_startTime[NTIMERS], PC_stopTime[NTIMERS], PC_frequency=0;

//Read the x86 CPU time stamp counter
INT64U readPentiumTimeStampCounter(void)
{   unsigned int hi, lo;
    asm("rdtsc; movl %%edx,%0; movl %%eax,%1" 	//read and copy
	 	: "=r" (hi), "=r" (lo) 		//output
 		: 				//input
 		: "%edx", "%eax");		//registers destroyed (globbered)
    return (((INT64U) hi)<<32) | ((INT64U) lo);
}

//Read the x86 CPU time stamp frequency (aka: time stamp counter counts per second)
INT64U countsPerSecond(void)
{  INT64U start = readPentiumTimeStampCounter();
#ifdef __WIN32__
   sleep(1000);
#else
   sleep(1);
#endif
   return readPentiumTimeStampCounter() - start;
}

/*
   *********************************************************************************************************
   *                                       ELAPSED TIME INITIALIZATION
   *
   * Description : This function initialize the elapsed time module by determining how long the START and
   *               STOP functions take to execute.  In other words, this function calibrates this module
   *               to account for the processing time of the START and STOP functions.
   *               Needs to be called only once before any of the timers is started with PC_ElapsedStart().
   *
   * Arguments   : None.
   *
   * Returns     : None.
   *********************************************************************************************************
 */
void PC_ElapsedInit(void)
{   static BOOLEAN initDone=FALSE;
    int i;
    
    if (initDone)
    	return;

    for (i=0; i < 4; i++)
    {    PC_frequency += countsPerSecond();
    }
    PC_frequency = PC_frequency >> 2;
    

    PC_ElapsedOverhead = 0;					// Measure the overhead of PC_ElapsedStart
    PC_ElapsedStart(0);						// ... and PC_ElapsedStop
    PC_ElapsedOverhead = (INT16U) PC_ElapsedStop(0);
    initDone=TRUE;
}

/*
   *********************************************************************************************************
   *                            START A TIMER FOR EXECUTION TIME MEASUREMENT
   *
   * Description : Trigger the timer to be used to measure the time between events.
   *               Timer will be running when the function returns.
   *		   Time measurement needs to be initalized before by calling PC_ElapsedInit, only
   *               needed once for all timers together.
   *
   * Arguments   : n=0...NTIMERS-1 number of timer
   *
   * Returns     : None.
   *********************************************************************************************************
 */
void PC_ElapsedStart(INT8U n)
{   if (n >= NTIMERS)
        return;

     PC_startTime[n]=readPentiumTimeStampCounter();		// Read the counter and store it
}

/*
   *********************************************************************************************************
   *                             STOP A TIMER FOR EXECUTION TIMER MEASUREMENT
   *
   * Description : This function stops the the timer for execution time measurement and computes the
   *               time in microseconds since the timer was started with PC_ElapsedStart.
   *
   * Arguments   : n=0...NTIMERS-1 number of timer.
   *
   * Returns     : The number of micro-seconds since the timer was last started.
   *
   * Notes       : - The returned time accounts for the processing time of the START and STOP functions.
   *
   *********************************************************************************************************
 */
INT32U PC_ElapsedStop(INT8U n)
{
    INT64U PC_diffTime;
    FP64 PC_diffTime_usec;

    if (n >= NTIMERS)
        return 0;
    PC_stopTime[n]=readPentiumTimeStampCounter();		// Read the counter and store it
 
    PC_diffTime = PC_stopTime[n] - PC_startTime[n]; 		//Compute the difference and
								// ... convert it into microseconds
																
    PC_diffTime_usec = PC_diffTime;
    PC_diffTime_usec = PC_diffTime * 1000000.0 / PC_frequency - PC_ElapsedOverhead;
    
//  printf("val=%8u   %f   %u\n", (INT32U)PC_diffTime, PC_diffTime_usec, PC_frequency);

    return (INT32U) PC_diffTime_usec;
}

/*
   *********************************************************************************************************
   *                                       GET THE CURRENT DATE AND TIME
   *
   * Description: This function obtains the current date and time from the PC.
   *
   * Arguments  : s     is a pointer to where the ASCII string of the current date and time will be stored.
   *                    You must allocate at least 21 bytes (includes the NUL) of storage in the return
   *                    string.  The date and time will be formatted as follows:
   *
   *                        "YYYY-MM-DD  HH:MM:SS"
   *
   * Returns    : none
   *********************************************************************************************************
 */
void PC_GetDateTime(char *s)
{   struct tm *now;
    time_t nowT;
    char *pS;
    
    nowT= time(NULL);
    now = (struct tm*) localtime(&nowT);			// Get the local time

    sprintf(s, "%04d-%02d-%02d  %02d:%02d:%02d",		// Convert into a string
            now->tm_year + 1900,
            now->tm_mon + 1,
            now->tm_mday,
            now->tm_hour,
            now->tm_min,
            now->tm_sec);
}



/*
*********************************************************************************************************
*                                        INSTALL INTERRUPT VECTOR
*
* Description: This function sets an interrupt vector in the "simulated" interrupt vector table.
*
* Arguments  : vect  is the desired interrupt vector number, a number between 1 and 7.
*              isr   is a pointer to a function to execute when the interrupt or exception occurs.
*
*              Interrupt 0 cannot be set, because it is reserved for the timer interrupt.
*
* Returns    : none
*********************************************************************************************************
*/
void  PC_IntVectSet(INT8U irq, void (*isr)(void))
{   if ((irq >0) && (irq < 8))
       interruptTable[irq]=isr;
}

/*
*********************************************************************************************************
*                                        OBTAIN INTERRUPT VECTOR
*
* Description: This function reads the pointer stored at the specified vector.
*
* Arguments  : vect  is the desired interrupt vector number, a number between 0 and 7.
*
* Returns    : The address of the Interrupt handler stored at the desired vector location.
*********************************************************************************************************
*/
void *PC_IntVectGet(INT8U irq)
{   if (irq < 8)
       return interruptTable[irq];
    else
       return NULL;
}
