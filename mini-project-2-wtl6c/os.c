// Modified by Mustafa Hotaki, July 2018
// MODIFIED BY SILE SHU 2017.6
// os.c
// Runs on LM4F120/TM4C123
// A very simple real time operating system with minimal features.
// Daniel Valvano
// January 29, 2015

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2015
   Programs 4.4 through 4.12, section 4.2
 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

#include <stdint.h>
#include "os.h"
#include "PLL.h"
#include "tm4c123gh6pm.h"
#include "PORTE.h"

// Functions implemented in assembly files
void OS_DisableInterrupts(void);	// Disable interrupts
void OS_EnableInterrupts(void);  	// Enable interrupts
long StartCritical(void);    			// previous I bit, disable interrupts
void EndCritical(long sr);    		// restore I bit to previous value
void WaitForInterrupt(void);  		// low power mode
void StartOS(void);

extern long x, TIMESLICE, temp;

// Periodic task function pointers
void (*PeriodicTask1)(void);
void (*PeriodicTask2)(void);

#define NUMTHREADS  20       // maximum number of threads
#define STACKSIZE   100      // number of 32-bit words in stack

// TCB Data Structure 
struct tcb {
	int32_t *sp;						// Pointer to stack (valid for threads not running)
  struct tcb *next;				// Linked list pointer
  uint32_t id;						// Thread Identifier
  uint32_t available;			// Used to indicate if this tcb is available or not
};
typedef struct tcb tcbType;

tcbType *RunPt;
tcbType tcbs[NUMTHREADS];
int32_t Stacks[NUMTHREADS][STACKSIZE];

// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: systick, 80 MHz PLL
// input:  none
// output: none
void OS_Init(void){int i;
  OS_DisableInterrupts();
  PLL_Init(Bus80MHz);                 // set processor clock to 80 MHz
	for(i=0;i<NUMTHREADS;i++){tcbs[i].available = 1;}  // initial available
	InitTimer2A(TIME_1MS);
	InitTimer3A();
  OS_ClearMsTime();
  
  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
															// lowest PRI so only foreground interrupted
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000; // priority 7
}

void SetInitialStack(int i){
  tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer
  Stacks[i][STACKSIZE-1] = 0x01000000;   // thumb bit
  Stacks[i][STACKSIZE-3] = 0x14141414;   // R14
  Stacks[i][STACKSIZE-4] = 0x12121212;   // R12
  Stacks[i][STACKSIZE-5] = 0x03030303;   // R3
  Stacks[i][STACKSIZE-6] = 0x02020202;   // R2
  Stacks[i][STACKSIZE-7] = 0x01010101;   // R1
  Stacks[i][STACKSIZE-8] = 0x00000000;   // R0
  Stacks[i][STACKSIZE-9] = 0x11111111;   // R11
  Stacks[i][STACKSIZE-10] = 0x10101010;  // R10
  Stacks[i][STACKSIZE-11] = 0x09090909;  // R9
  Stacks[i][STACKSIZE-12] = 0x08080808;  // R8
  Stacks[i][STACKSIZE-13] = 0x07070707;  // R7
  Stacks[i][STACKSIZE-14] = 0x06060606;  // R6
  Stacks[i][STACKSIZE-15] = 0x05050505;  // R5
  Stacks[i][STACKSIZE-16] = 0x04040404;  // R4
}

///******** OS_Launch ***************
// start the scheduler, enable interrupts
// Inputs: number of 20ns clock cycles for each time slice
//         (maximum of 24 bits)
// Outputs: none (does not return)
void OS_Launch(unsigned long theTimeSlice){
	NVIC_ST_RELOAD_R = theTimeSlice - 1; // reload value
  NVIC_ST_CTRL_R = 0x00000007; // enable, core clock and interrupt arm
  StartOS();                   // start on the first task
}

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void) { 
	NVIC_ST_CURRENT_R = 0;							// Pg.81 - 82 (24 bit down counter - same Hz as bus)
	NVIC_INT_CTRL_R = 0x04000000;				// Triggers SysTick Interrupt
}

//******** OS_AddThreads ***************
// Test function that adds three foregound threads to the scheduler
// Inputs: three pointers to a void/void foreground tasks
// Outputs: 1 if successful, 0 if this thread can not be added
int OS_AddThreads(void(*task0)(void), void(*task1)(void), void(*task2)(void)){ 
	int32_t status;
  status = StartCritical();
  tcbs[0].next = &tcbs[1]; // 0 points to 1
  tcbs[1].next = &tcbs[2]; // 1 points to 2
  tcbs[2].next = &tcbs[0]; // 2 points to 0
  SetInitialStack(0); Stacks[0][STACKSIZE-2] = (int32_t)(task0); // PC
  SetInitialStack(1); Stacks[1][STACKSIZE-2] = (int32_t)(task1); // PC
  SetInitialStack(2); Stacks[2][STACKSIZE-2] = (int32_t)(task2); // PC
  RunPt = &tcbs[0];       // Thread 0 will run first
  EndCritical(status);
  return 1;               // Successful
}

//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)

static uint32_t ThreadNum = 0;
static int tail = 0;

int OS_AddThread(void(*task)(void), unsigned long stackSize, unsigned long priority) {
	
	int x = 0; 
	int status;
	
	status = StartCritical();
	
	if(ThreadNum < NUMTHREADS) {
		
		while(tcbs[x].available != 1){x++;}
		
		if(ThreadNum == 0){
			tcbs[0].next = tcbs;
			RunPt = tcbs;
		}
		
		else {
			tcbs[x].next = tcbs[tail].next;
			tcbs[tail].next = &tcbs[x];
		}
		
		tcbs[x].available = 0;
		tcbs[x].id = x;
		tail = x;
			
		SetInitialStack(x);
		Stacks[x][STACKSIZE - 2] = (int)(task);
		ThreadNum++;
		EndCritical(status);
		
		return 1;
	}
	return 0;
}
	 
	
//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
unsigned long OS_Id(void) { 
	return RunPt->id;
}
	 
// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(unsigned long sleepTime){
  // Your code here.
}

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
	
	tcbType * prev = &tcbs[tail];
	
	RunPt -> available = 1;
	
	while((prev -> next) != (RunPt)){
		prev = prev -> next;
	}
	
	prev -> next = RunPt -> next;
	ThreadNum--;
	OS_Suspend();	
}	

//******** OS_AddPeriodicThread *************** 
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
int OS_AddPeriodicThread(void(*task)(void), 
   unsigned long period, unsigned long priority) { 
	static uint16_t PeriodTaskCt;
	if (PeriodTaskCt == 0){
		PeriodicTask1 = task;
		InitTimer1A(period,priority);
	}
	else {
		PeriodicTask2 = task;
		InitTimer4A(period, priority);
	}
	PeriodTaskCt++;
  return 1;
}

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
unsigned long OS_Time(void) {
	return TIMER3_TAILR_R - TIMER3_TAV_R;
}

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
unsigned long OS_TimeDifference(unsigned long start, unsigned long stop) {
	return stop - start;
}

// Ms time system
static uint32_t MSTime;
// ******** OS_ClearMsTime ************
// sets the system time to zero
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void) {
	MSTime = 0;
}

// ******** OS_MsTime ************
// reads the current time in msec
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// It is ok to make the resolution to match the first call to OS_AddPeriodicThread
unsigned long OS_MsTime(void) {
	return MSTime;
}

void Scheduler(void){
	RunPt = RunPt->next;
}

//---------------------Timers-----------------------

void InitTimer1A(unsigned long period, uint32_t priority) {
	long sr;
	volatile unsigned long delay;
	
	sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x02;
	
  while((SYSCTL_RCGCTIMER_R & 0x02) == 0){} // allow time for clock to stabilize
	
  TIMER1_CTL_R &= ~TIMER_CTL_TAEN; // 1) disable timer1A during setup
                                   // 2) configure for 32-bit timer mode
  TIMER1_CFG_R = TIMER_CFG_32_BIT_TIMER;
                                   // 3) configure for periodic mode, default down-count settings
  TIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD;
  TIMER1_TAILR_R = period - 1;     // 4) reload value
                                   // 5) clear timer1A timeout flag
  TIMER1_ICR_R = TIMER_ICR_TATOCINT;
  TIMER1_IMR_R |= TIMER_IMR_TATOIM;// 6) arm timeout interrupt
								   // 7) priority shifted to bits 15-13 for timer1A
  NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF00FF)|(priority << 13);	//3
  NVIC_EN0_R = NVIC_EN0_INT21;     // 8) enable interrupt 21 in NVIC
  TIMER1_TAPR_R = 0;
  TIMER1_CTL_R |= TIMER_CTL_TAEN;  // 9) enable timer1A
	
  EndCritical(sr);
}

void Timer1A_Handler(void){ 
  TIMER1_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer1A timeout
	(*PeriodicTask1)();
}

void InitTimer2A(unsigned long period) {
	long sr;
	volatile unsigned long delay;
	
	sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x04;
  while((SYSCTL_RCGCTIMER_R & 0x04) == 0){} // allow time for clock to stabilize
	
  TIMER2_CTL_R &= ~TIMER_CTL_TAEN; 				// 1) disable timer2A during setup
																					// 2) configure for 32-bit timer mode
  TIMER2_CFG_R = TIMER_CFG_32_BIT_TIMER;
																					// 3) configure for periodic mode, default down-count settings
  TIMER2_TAMR_R = TIMER_TAMR_TAMR_PERIOD;
  TIMER2_TAILR_R = period - 1;   				  // 4) reload value
																					// 5) clear timer2A timeout flag
  TIMER2_ICR_R = TIMER_ICR_TATOCINT;
  TIMER2_IMR_R |= TIMER_IMR_TATOIM;				// 6) arm timeout interrupt
																					// 7) priority shifted to bits 31-29 for timer2A
  NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF)|(2 << 29);	
  NVIC_EN0_R = NVIC_EN0_INT23;     				// 8) enable interrupt 23 in NVIC
  TIMER2_TAPR_R = 0;
  TIMER2_CTL_R |= TIMER_CTL_TAEN; 				 // 9) enable timer2A
	
  EndCritical(sr);
}

void Timer2A_Handler(void){ 	
	TIMER2_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer2A timeout
	MSTime++;
}

void InitTimer3A(void){
	long sr;

	sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x08;
  while((SYSCTL_RCGCTIMER_R & 0x08) == 0){} // allow time for clock to stabilize
	
  TIMER3_CTL_R &= ~TIMER_CTL_TAEN; // 1) disable timer3A during setup
                                   // 2) configure for 32-bit timer mode
  TIMER3_CFG_R = TIMER_CFG_32_BIT_TIMER;
                                   // 3) configure for periodic mode, default down-count settings
  TIMER3_TAMR_R = TIMER_TAMR_TAMR_PERIOD;
  TIMER3_TAILR_R = 0xFFFFFFFF - 1;     // 4) reload value
                                   // 5) clear timer3A timeout flag
  TIMER3_ICR_R = TIMER_ICR_TATOCINT;
  TIMER3_IMR_R |= TIMER_IMR_TATOIM;// 6) arm timeout interrupt
								   // 7) priority shifted to bits for timer3A
  NVIC_PRI8_R = (NVIC_PRI8_R&0x00FFFFFF)|(1 << 29);	//1
  NVIC_EN1_R = NVIC_EN1_INT35;     // 8) enable interrupt 35 in NVIC
  TIMER3_TAPR_R = 0;
  TIMER3_CTL_R |= TIMER_CTL_TAEN;  // 9) enable timer3A
	
  EndCritical(sr);
}

void Timer3A_Handler(void){ 
  TIMER3_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer1A timeout
}

void InitTimer4A(uint32_t period, uint32_t priority) {
	long sr;
	
	sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x10;
	
  while((SYSCTL_RCGCTIMER_R & 0x10) == 0){} // allow time for clock to stabilize
	
  TIMER4_CTL_R &= ~TIMER_CTL_TAEN; // 1) disable timer4A during setup
                                   // 2) configure for 32-bit timer mode
  TIMER4_CFG_R = TIMER_CFG_32_BIT_TIMER;
                                   // 3) configure for periodic mode, default down-count settings
  TIMER4_TAMR_R = TIMER_TAMR_TAMR_PERIOD;
  TIMER4_TAILR_R = period - 1;     // 4) reload value
                                   // 5) clear timer4A timeout flag
  TIMER4_ICR_R = TIMER_ICR_TATOCINT;
  TIMER4_IMR_R |= TIMER_IMR_TATOIM;// 6) arm timeout interrupt
								   // 7) priority shifted to bits 15-13 for timer1A
  NVIC_PRI17_R = (NVIC_PRI17_R&0xFF00FFFF)|(priority << 21);	//3
  NVIC_EN2_R = NVIC_EN2_INT70;     // 8) enable interrupt 70 in NVIC
  TIMER4_TAPR_R = 0;
  TIMER4_CTL_R |= TIMER_CTL_TAEN;  // 9) enable timer4A
	
  EndCritical(sr);
}

void Timer4A_Handler(void){ 
  TIMER4_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer4A timeout
	(*PeriodicTask2)();
}
