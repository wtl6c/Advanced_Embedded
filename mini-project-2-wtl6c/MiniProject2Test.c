// Lab2Test.c
// Runs on LM4F120/TM4C123
// You may use, edit, run or distribute this file 
// You are free to change the syntax/organization of this file

// Jonathan W. Valvano 2/20/17, valvano@mail.utexas.edu
// Modified by Sile Shu 10/4/17, ss5de@virginia.edu
// Modified by Mustafa Hotaki 8/1/2018, mkh3cf@virginia.edu

#include <stdint.h>
#include "OS.h"
#include "tm4c123gh6pm.h"
#include <string.h> 
#include "PLL.h"
#include "PORTE.h"

unsigned long NumCreated;   // Number of foreground threads created
unsigned long TIMESLICE;
unsigned long CONTEXTSWITCHTIME;
unsigned long temp;

//******************* Measurement of context switch time **********
// Run this to measure the time it takes to perform a task switch
void Thread8(void){       // only thread running
  while(1){
    PE0 ^= 0x01;      // debugging profile  
  }
}

int main(void){       // Testmain0
  PortE_Init();
  OS_Init();           // initialize, disable interrupts
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Thread8, 128, 2); 
  OS_Launch(TIME_1MS/10); // 100us, doesn't return, interrupts enabled in here
  return 0;             // this never executes
}

//******************* Initial TEST **********
// Cooperative thread scheduler
unsigned long Count1;   // number of times thread1 loops
unsigned long Count2;   // number of times thread2 loops
unsigned long Count3;   // number of times thread3 loops
unsigned long Count4;   // number of times thread4 loops
unsigned long Count5;   // number of times thread5 loops

void Thread1(void){
  Count1 = 0;          
  for(;;){
    PE0 ^= 0x01;       // heartbeat
    Count1++;
    OS_Suspend();      // cooperative multitasking
  }
}
void Thread2(void){
  Count2 = 0;          
  for(;;){
		//temp = OS_Time();
    PE1 ^= 0x02;       // heartbeat
    Count2++;
		//TIMESLICE = OS_TimeDifference(temp, OS_Time());
		//temp = OS_Time();
    OS_Suspend();      // cooperative multitasking
  }
}
void Thread3(void){
  Count3 = 0;          
  for(;;){
		//CONTEXTSWITCHTIME = OS_TimeDifference(temp, OS_Time());
    PE2 ^= 0x04;       // heartbeat
    Count3++;
    OS_Suspend();      // cooperative multitasking
  }
}

int Testmain1(void){  // Testmain1
  OS_Init();          // initialize, disable interrupts
  PortE_Init();       // profile user threads
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Thread1, 128, 1); 
  NumCreated += OS_AddThread(&Thread2, 128, 2); 
  NumCreated += OS_AddThread(&Thread3, 128, 3); 
  // Count1 Count2 Count3 should be equal or off by one at all times
  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}

//*******************Second TEST**********
// Preemptive thread scheduler
void Thread1b(void){
  Count1 = 0;          
  for(;;){
    PE0 ^= 0x01;       // heartbeat
    Count1++;
  }
}
void Thread2b(void){
  Count2 = 0;          
  for(;;){						// x cleared in the interrupt for timer3A. 
		//temp = OS_Time();
    PE1 ^= 0x02;       // heartbeat
    Count2++;
  }
}
void Thread3b(void){
  Count3 = 0;     
  //CONTEXTSWITCHTIME = OS_TimeDifference(temp, OS_Time());
	//temp = OS_Time();	
  for(;;){
    PE2 ^= 0x04;       // heartbeat
    Count3++;
	//TIMESLICE = OS_TimeDifference(temp, OS_Time());
  }
}
int Testmain2(void){    // Testmain2
  OS_Init();            // initialize, disable interrupts
  PortE_Init();         // profile user threads

  NumCreated = 0;
  NumCreated += OS_AddThread(&Thread1b, 128, 1); 
  NumCreated += OS_AddThread(&Thread2b, 128, 2); 
  NumCreated += OS_AddThread(&Thread3b, 128, 3); 

  OS_Launch(TIME_2MS);  // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}
