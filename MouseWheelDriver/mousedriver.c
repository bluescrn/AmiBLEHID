#include <exec/interrupts.h>
#include <hardware/intbits.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <clib/exec_protos.h>
#include <clib/input_protos.h>
#include <clib/dos_protos.h>
#include <clib/potgo_protos.h>
#include <dos/dos.h>
#include <stdio.h>

#include "newmouse.h"

//#define DEBUG

// avoid definition of any of the following pairs to disable reporting them to OS
#define MM_WHEEL_UP   0x0A
#define MM_WHEEL_DOWN 0x09

// Define these to enable MMB
#define MM_MIDDLEMOUSE_UP   0x0D
#define MM_MIDDLEMOUSE_DOWN 0x0E

#define TEMPLATE "M=NOMMB/S,V=VREV/S"
LONG myargs[2];
BOOL vreverse, mmb_use; 

struct RDArgs *myrda;

#define MM_NOTHING 0

#define BUTTON_WHEELUP 1L<<5
#define BUTTON_WHEELDN 1L<<4
#define BUTTON_MMB	   1L<<2

extern void VertBServer();  /* proto for asm interrupt server */

struct {
    UBYTE head;		        // 0 message counter head
    UBYTE tail;		        // 1 message counter tail
    ULONG sigbit;         	// 2
    struct Task *task;		// 6
    APTR potgoResource;		// 10
    UBYTE codes[256];     	// 14
} mousedata;

struct Interrupt vertblankint = {
									// struct  Node is_Node;
	{	0,							// struct  Node *ln_Succ;		/* Pointer to next (successor) */
		0,							// struct  Node *ln_Pred;		/* Pointer to previous (predecessor) */
		NT_INTERRUPT,				// UBYTE   ln_Type;
		32,   						// BYTE    ln_Pri;				/* Priority, for sorting */
		"AmiBLEHID Mouse"		    // char    *ln_Name;			/* ID string, null terminated */
	},
	&mousedata,						// APTR    is_Data;		    	/* server data segment  */
	VertBServer						// VOID    (*is_Code)();	    /* server code entry    */
};

void CreateMouseEvents(int t);
int  AllocResources();
void FreeResources();

extern struct CIA ciaa, ciab;

struct IOStdReq   *InputIO;
struct MsgPort    *InputMP;
struct InputEvent *MouseEvent;
struct InputBase  *InputBase;
struct PotgoBase  *PotgoBase;
BYTE   intsignal;
UBYTE  msgdata, prevmsgdata;
ULONG  potbits;
UBYTE  button_state_mmb;
int    max_length;

const char version[] = "$VER: AmiBLEHID MouseWheel driver 0.1 (" __DATE__ ")";


int main(void)
{
	mousedata.head   = 0;
	mousedata.tail   = 0;
	max_length       = 0;
	button_state_mmb = 0;
	
	if (AllocResources())
	{
		mousedata.task = FindTask(0);
		mousedata.sigbit = 1 << intsignal;
		AddIntServer(INTB_VERTB, &vertblankint);
		SetTaskPri(mousedata.task, 20); /* same as input.device */

		printf("AmiBLEHID wheel driver installed.\n");
		printf("Expecting CD32 button protocol for wheel/MMB\n");
		printf(__DATE__ "; " __TIME__ "\ngcc: " __VERSION__);
		printf("\n\n");
		
		mmb_use  = TRUE;
		vreverse = FALSE;

		// Parse command line
		if(myrda = (struct RDArgs *)AllocDosObject(DOS_RDARGS, NULL))
		{	
			ReadArgs(TEMPLATE, myargs, myrda);
			if(myargs[0]) mmb_use = FALSE;
			if(myargs[1]) vreverse = TRUE;
		}
		
		printf("Reverse vertical axis  : %s\n", vreverse?"TRUE":"FALSE");
		printf("Use MMB                : %s\n", mmb_use?"TRUE":"FALSE");
		printf("To stop press CTRL-C.\n");
		

		while (1)
		{
			ULONG signals = Wait (mousedata.sigbit | SIGBREAKF_CTRL_C);
			
			if (signals & mousedata.sigbit)
			{
				while(mousedata.head != mousedata.tail)
				{
					msgdata = mousedata.codes[mousedata.tail];					

					if((mousedata.head - mousedata.tail) > max_length)
					{
						max_length = mousedata.head - mousedata.tail;
					}	

#ifdef DEBUG
					if(prevmsgdata != msgdata) 
					{
						int data = (int)msgdata;
						printf("CD32Buttons:0x%02X\n", data);
					}
#endif

					// Wheel events - Every sample read is a wheel tick, so what looks like
					// a 'held button' is a very rapid wheel movement.
					if(msgdata & BUTTON_WHEELDN)
					{
						CreateMouseEvents(MM_WHEEL_DOWN);
					}
					else if(msgdata & BUTTON_WHEELUP)
					{
						CreateMouseEvents(MM_WHEEL_UP);
					}

					// MMB events - check for a state change
					if(!button_state_mmb)
					{
						if(msgdata & BUTTON_MMB)
						{
							button_state_mmb = 1;
#ifdef MM_MIDDLEMOUSE_UP
							if(mmb_use)
							{
								CreateMouseEvents(MM_MIDDLEMOUSE_DOWN);
							}							
#endif
						}						
					}
					else if(button_state_mmb)
					{
						if (!(msgdata & BUTTON_MMB))
						{
							button_state_mmb = 0;
#ifdef MM_MIDDLEMOUSE_DOWN
							if(mmb_use)
							{
								CreateMouseEvents(MM_MIDDLEMOUSE_UP);
							}
#endif
						}						
					}

					prevmsgdata = msgdata;
					mousedata.codes[mousedata.tail] = 0;
					++mousedata.tail;
				}
			}
			
			if (signals & SIGBREAKF_CTRL_C)
			{
#ifdef DEBUG
				printf("head: %d.\n", mousedata.head);
				printf("tail: %d.\n", mousedata.tail);
#endif
				printf("Maximum FIFO used depth was: %d\n", max_length);				
				break;
			}
		}
	} 
	else 
	{
		return -1;
	}
	
	FreeResources();
	PutStr("Exited\n");
	
	return 0;
}



int AllocResources()
{
	if (intsignal = AllocSignal (-1))
	{
#ifdef DEBUG
		PutStr("Debug: Signal allocated.\n");
#endif
		if (InputMP = CreateMsgPort())
		{
#ifdef DEBUG
			PutStr("Debug: Message port created.\n");
#endif
			if (MouseEvent = AllocMem(sizeof(struct InputEvent),MEMF_PUBLIC))
			{
#ifdef DEBUG
				PutStr("Debug: Memory for MouseEvent allocated.\n");
#endif
				if (InputIO = CreateIORequest(InputMP,sizeof(struct IOStdReq)))
				{
#ifdef DEBUG
					PutStr("Debug: CIORequest created.\n");
#endif
					if (!OpenDevice("input.device",0UL,(struct IORequest *)InputIO,0UL))
					{
#ifdef DEBUG
						PutStr("Debug: input.device opened.\n");
#endif
						InputBase = (struct InputBase *)InputIO->io_Device;
						
						return 1;
					}
					else
						PutStr("Error: Could not open input.device\n");
				}
				else
					PutStr("Error: Could not create IORequest\n");
			}
			else
				PutStr("Error: Could not allocate memory for MouseEvent\n");
		}
		else
			PutStr("Error: Could not create message port\n");
	}
	else
		PutStr("Error: Could not allocate signal\n");
	
	return 0;
}



void FreeResources()
{	
	if (InputIO)
	{
		CloseDevice((struct IORequest *)InputIO);
		DeleteIORequest(InputIO);
	}
	if (MouseEvent) FreeMem(MouseEvent, sizeof(struct InputEvent));
	if (InputMP) DeleteMsgPort(InputMP);
	if (intsignal) FreeSignal(intsignal);
	RemIntServer(INTB_VERTB, &vertblankint);
}



void CreateMouseEvents(int t)
{
	if(t == MM_NOTHING)
	{
		return;
	}
	
	if(vreverse && ((t == MM_WHEEL_DOWN) || (t == MM_WHEEL_UP)))
	{
		if(t == MM_WHEEL_DOWN)
		{
			t = MM_WHEEL_UP;
		}
		else
		{
			t = MM_WHEEL_DOWN;
		}
	}

	MouseEvent->ie_EventAddress = NULL;
	MouseEvent->ie_NextEvent    = NULL;
	MouseEvent->ie_Class        = IECLASS_RAWKEY;
	MouseEvent->ie_SubClass     = 0;
	MouseEvent->ie_Qualifier    = PeekQualifier();
	
	switch (t)
	{
		case MM_WHEEL_DOWN:
			MouseEvent->ie_Code = NM_WHEEL_DOWN;
#ifdef DEBUG
			printf("Debug: Wheel Down.\n");
#endif
		break;
		case MM_WHEEL_UP:
			MouseEvent->ie_Code = NM_WHEEL_UP;
#ifdef DEBUG
			printf("Debug: Wheel Up.\n");
#endif			
		break;

#ifdef MM_MIDDLEMOUSE_DOWN
		case MM_MIDDLEMOUSE_DOWN:
 			MouseEvent->ie_Class = IECLASS_RAWMOUSE;
			MouseEvent->ie_Code = IECODE_MBUTTON;
			MouseEvent->ie_Qualifier = IEQUALIFIER_MIDBUTTON | IEQUALIFIER_RELATIVEMOUSE;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
 #ifdef DEBUG
			printf("Debug: MMB down.\n");
 #endif			
		break;
#endif

#ifdef MM_MIDDLEMOUSE_UP
		case MM_MIDDLEMOUSE_UP: 
			MouseEvent->ie_Class = IECLASS_RAWMOUSE;
			MouseEvent->ie_Code = IECODE_MBUTTON | IECODE_UP_PREFIX;
			MouseEvent->ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
 #ifdef DEBUG
			printf("Debug: MMB up.\n");
 #endif			
			break;
#endif
    default:
#ifdef DEBUG
			printf("Debug: unrecognized code 0x%02X.\n", t);
#endif
			return;
    break;
	}

	InputIO->io_Data = (APTR)MouseEvent;
	InputIO->io_Length = sizeof(struct InputEvent);
	InputIO->io_Command = IND_WRITEEVENT;

	DoIO((struct IORequest *)InputIO);
}
