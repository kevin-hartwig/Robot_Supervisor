/*
 * testing.c
 *
 *  Created on: Apr 13, 2016
 *      Author: k3v
 */


#include <stdio.h>
#include <stdlib.h>

void main() {

	char s[10];
	char *CommandMenu[] = {
			"- - - - Command Menu - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -",
			"MODULE\t\tCOMMAND STRUCTURE\t\tARGUMENTS",
			"\nDC Motor\tD <Dir> <Speed>\t\t\tDir: Left=1, Right=2",
			"\nStepper\t\tS <Speed> <StepSz> <# Steps>\tSpeed=1F-7F",
			"\t\t\t\t\t\tStepSz: Half=1, Full=2",
			"\t\t\t\t\t\t(StepSz sign controls direction)",
			"\nRC Servo\tR <Rotation>\t\t\tRotation: 0-180 degrees",
			"\nLCD\t\tP <String>\t\t\tString: character string",
			"\n* Wrap command in < >, ie. to set RC at 180 degrees, use <R 180> *",
			"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ",
			NULL,
	};

	char **p = CommandMenu;

	while (*p) {
		printf("\n%s", *p);
		p++;
	}

	gets(s)	;

}
