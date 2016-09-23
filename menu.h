/*
 * menu.h
 *
 *  Created on: Apr 4, 2016
 *      Author: Kevin Hartwig
 */

#ifndef _MENU_H_
#define _MENU_H_

#include <termios.h>
#include <stdio.h>
#include <termcap.h>
#include <stdlib.h>
#include "myUtil.h"

#define ESC		27 - '1'

// MAIN MENU // // /// /// //// //// ///// ///// ////// ///////
char *menu[] = {
    "1] Command Mode",
	"2] Module Testing",
    "3] Direct Terminal",
	NULL,
};
enum MainOptions {COMMAND,MODULE,TERMINAL};

// COMMAND MENU // // /// //// ///// /////// //////// /////////
char *CommandMenu[] = {
			"________________________________________________________________________________",
			"                                                              					 ",
			"MODULE\t\tCOMMAND STRUCTURE\t\tARGUMENTS",
			"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ",
			"DC Motor\tD (Direction) (Speed)\t\tDirection: \t1 (FW), 2 (BW)",
			"\t\t\t\t\t\tSpeed: \t\t0-100%",
			"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ",
			"Stepper\t\tS (Speed) (Rotation)\t\tSpeed: \t\t1-3 (slow-fast)",
			"\t\t\t\t\t\tRotation: \t0-180 degrees",
			"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ",
			"RC Servo\tR (Rotation)\t\t\tRotation: \t65-170 degrees",
			"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ",
			"LCD\t\tP (String)\t\t\tString: \tCharacter string",
			"________________________________________________________________________________",
			NULL,
	};

// TESTING MENU // //// ///// //// // // ///// //// // // ////
char *TestingMenu[] = {
		"1] SERVO",
		"2] STEPPER",
		"3] DC MOTOR",
		NULL
};
enum TestingOptions {SERVO,STEPPER,DCMOTOR};

int getchoice(char *greet, char *choices[]);
/* Returns a choice associated with the menu passed in the second argument,
 * char *choices[].  A character string greeting can be passed, pointed to
 * by the first argument.  getChoice() will return the enum associated with
 * selected menu option.  This selection can be tested in a switch statement
 * in a foreground process using the same enums.
 */
int getchoice(char *greet, char *choices[])
{
	int chosen = 0;
	char selected;
	char **option;
    do {
		  printf("\n%s\n",greet);		// print the leading message
          option = choices;
          while(*option) {						// print each of the menu options
              printf("\t%s\n",*option);
              option++;
          }
          selected = (int)getch();					// wait for a user character
          option = choices;
          while (*option) {			// search for a menu option that matches the input
              if ( selected == *option[0] ) {
                  chosen = 1;
                  break;
              }
              option++;
          }
          if (selected == 27){
        	  chosen = 1;
        	  break;
          }
          if (!chosen) {		   // check if no match was found and berate the user
        	  clear();
          }
    } while(!chosen);				// keep waiting for input until a valid one is given
    clear();
    return selected;				// return the user’s selection from the menu
}


#endif /* MENU_H_ */

