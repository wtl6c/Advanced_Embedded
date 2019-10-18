#include <stdint.h>
// filename **********OS.H***********
// Real Time Operating System for Labs 2 and 3 
// Jonathan W. Valvano 2/20/17, valvano@mail.utexas.edu
// EE445M/EE380L.6 
// You may use, edit, run or distribute this file 
// You are free to change the syntax/organization of this file
// You are required to implement the spirit of this OS

 
#ifndef _OS_H_
#define _OS_H_

// edit these depending on your clock        
#define TIME_1MS    80000          
#define TIME_2MS    (2*TIME_1MS)  
#define TIME_500US  (TIME_1MS/2)  
#define TIME_250US  (TIME_1MS/5) 

//******** OS_AddPeriodicThread *************** 
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
int OS_AddPeriodicThread(void(*task)(void),unsigned long period, unsigned long priority);

#endif
