#include <stdint.h>
#include "PLL.h"
#include "LCD.h"
#include "os.h"
#include "joystick.h"
#include "FIFO.h"
#include "PORTE.h"
#include "tm4c123gh6pm.h"

// Constants
#define BGCOLOR     					LCD_BLACK
#define CROSSSIZE            			5

//------------------Defines and Variables-------------------
uint16_t origin[2]; // the original ADC value of x,y if the joystick is not touched
int16_t x = 63;  // horizontal position of the crosshair, initially 63
int16_t y = 63;  // vertical position of the crosshair, initially 63
int16_t prevx = 63;
int16_t	prevy = 63;
uint8_t select;  // joystick push

//---------------------User debugging-----------------------

#define TEST_TIMER 	0		     		  // Change to 1 if testing the timer
#define TEST_PERIOD 3999999  		  // Defined by user
#define PERIOD 			3999999  	 	  // Defined by user

unsigned long Count;   		// number of times thread loops


//--------------------------------------------------------------
void CrossHair_Init(void){
	BSP_LCD_FillScreen(LCD_BLACK);	// Draw a black screen
	BSP_Joystick_Input(&origin[0], &origin[1], &select); // Initial values of the joystick, used as reference
}

//******** Producer *************** 
void Producer(void){
#if TEST_TIMER
	PE1 ^= 0x02;	// heartbeat
	Count++;	// Increment dummy variable			
#else
	// Variable to hold updated x and y values
	int16_t newX;
	int16_t newY;
	int16_t deltaX = 0;
	int16_t deltaY = 0;
	int16_t count = 0;
	
	uint16_t rawX, rawY; 			// To hold raw adc values
	uint8_t select;						// To hold pushbutton status
	rxDataType data;
	
	while(count < 5) {
		BSP_Joystick_Input(&rawX, &rawY, &select);
		deltaX += rawX;																	// sample 5 values to insure that the joystick ADC isn't just jumping around
		deltaY += rawY;
		count++;
	}
	
	newX = (deltaX / 5);															// Get the average of the 5 samples
	newY = (deltaY / 5);
	
	if(newX > 2500){x += 2;}													// If the ADC reads larger than 2500 move the x coordinate to the right twice
	else if(newX < 1000){x -= 2;}											// If the ADC reads less than 1000 move the x coordinate to the left twice
	
	if(newY > 2500) {y -= 2;}													// If the ADC reads larger than 2500 move the y coordinate up twice
	else if(newY < 1000){y += 2;}											// If the ADC reads less than 1000 move the x coordinate down twice
	
	if(x > 127){x = 127;}															// Dont allow the crosshairs to move outside the frame of the screen (+X)
	else if(x < 0){x = 0;}														// Dont allow the crosshairs to move outside the frame of the screen (-X)
	
	if(y > 115){y = 115;}															// Dont allow the crosshairs to move outside the frame of the screen (-Y)
	else if(y < 0){y = 0;}														// Dont allow the crosshairs to move outside the frame of the screen (+Y)
	
	data.x = x;																				// Store new updated value of x into the data struct
	data.y = y;																				// Store new updated value of y into the data struct
	
	RxFifo_Put(data);																	// Send the data struct to the stack (FIFO)
	
#endif
}

//******** Consumer *************** 
void Consumer(void){
	
	rxDataType data;
	char x_array[] = "X: ";
	char y_array[] = "Y: ";
	char* x_ptr = x_array;
	char* y_ptr = y_array;
	
	prevx = data.x;																				// Before reading in a new value from the data struct store the old ones in the prevx global
	prevy = data.y;																				// Before reading in a new value from the data struct store the old ones in the prevy global
		
	RxFifo_Get(&data);																		// Read top value from stack and place it into the data struct
	
	x = data.x;																						// Store values from the data struct into the x global
	y = data.y;																						// Store values from the data struct into the y global
	
	BSP_LCD_DrawCrosshair(prevx, prevy, LCD_BLACK);				// Erase old crossgair using prevx and prevy in prep for new crosshair
	BSP_LCD_DrawCrosshair(x, y, LCD_GREEN);								// Draw new crosshair using x and y
	
	BSP_LCD_Message(1, 12, 4, x_ptr, x);									// Write out message X: value of global x
	BSP_LCD_Message(1, 12, 12, y_ptr, y);									// Write out message Y: value of global y
}

//******** Main *************** 
int main(void){
  PLL_Init(Bus80MHz);       // set system clock to 80 MHz
#if TEST_TIMER
	PortE_Init();       // profile user threads
	Count = 0;
	OS_AddPeriodicThread(&Producer, TEST_PERIOD, 1);
	while(1){}
#else
  BSP_LCD_Init();        // initialize LCD
	BSP_Joystick_Init();   // initialize Joystick
  CrossHair_Init();      
 	RxFifo_Init();
	OS_AddPeriodicThread(&Producer,PERIOD, 1);
	while(1){
		Consumer();
	}
#endif
} 
