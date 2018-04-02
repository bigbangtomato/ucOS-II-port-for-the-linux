/*
   *********************************************************************************************************
   *                            PC SUPPORT FUNCTIONS for __GNUC__ (WIN32 and LINUX)
   *
   *                          (c) Copyright 2004-2005, Werner.Zimmermann@fht-esslingen.de
   *                      (Functions similar to the 80x86 Real Mode port by Jean J. Labrosse)
   *                                           All Rights Reserved
   *
   * File : PC.H
   * By   : Werner Zimmermann
   *********************************************************************************************************
 */

#define PC_CHECK_RECURSIVE_CALLS        FALSE           //Set to TRUE, if you want to check whether the keyboard
                                                        //and display functions are called recursively, i.e.
                                                        //the previous call could not be finished before the
                                                        //functions are called the next time

//#define DEBUG_PC					//Uncomment, if you want to debug the display functions

/*
   *********************************************************************************************************
   *                                               CONSTANTS
   *                                    COLOR ATTRIBUTES FOR VGA MONITOR
   *
   * Description: These #defines are used in the PC_Disp???() functions.  The 'color' argument in these
   *              function MUST specify a 'foreground' color and a 'background'.
   *              If you don't specify a background color, BLACK is assumed.  You would
   *              specify a color combination as follows:
   *
   *              PC_DispChar(0, 0, 'A', DISP_FGND_WHITE + DISP_BGND_BLUE);
   *
   *              To have the ASCII character 'A' with a white letter on a blue background.
   *********************************************************************************************************
 */

#define DISP_FGND_BLACK           0x10
#define DISP_FGND_BLUE            0x20
#define DISP_FGND_GREEN           0x30
#define DISP_FGND_RED             0x40
#define DISP_FGND_CYAN            0x50
#define DISP_FGND_YELLOW          0x60
#define DISP_FGND_WHITE           0x70
#define DISP_FGND_GRAY            0x80
#define DISP_FGND_MAGENTA	  0x90

#define DISP_BGND_BLACK           0x01
#define DISP_BGND_BLUE            0x02
#define DISP_BGND_GREEN           0x03
#define DISP_BGND_RED             0x04
#define DISP_BGND_CYAN            0x05
#define DISP_BGND_YELLOW          0x06
#define DISP_BGND_WHITE           0x07
#define DISP_BGND_GRAY            0x08
#define DISP_BGND_MAGENTA	  0x09
#define DISP_BGND_LIGHT_GRAY      0x0A

/*$PAGE */
/*
   *********************************************************************************************************
   *                                           FUNCTION PROTOTYPES
   *********************************************************************************************************
 */

void PC_DispChar(INT8U x, INT8U y, INT8U c, INT8U color);
void PC_DispClrScr(INT8U color);
void PC_DispStr(INT8U x, INT8U y, INT8U *s, INT8U color);

void PC_ElapsedInit(void);
void PC_ElapsedStart(INT8U n);
INT32U PC_ElapsedStop(INT8U n);

void PC_GetDateTime(char *s);
BOOLEAN PC_GetKey(INT16S * c);

void  PC_IntVectSet(INT8U irq, void (*isr)(void));
void *PC_IntVectGet(INT8U irq);
