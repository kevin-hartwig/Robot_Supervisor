/*
 *
 * 		PSP.c
 *
 * 		Source file for thePSP (Parent Supervisory Process).
 * 		Last updated: 		April 10, 2016
 * 		Written by:			Kevin Hartwig
 *
 * 			PSP is used for controlling and testing for use with the second year,
 * 		second semester project robot control board.  This process is a menu based
 * 		controllin system with testing options, a direct terminal for communication
 * 		to the board, and a command interface.
 *
 * 		Eventually, it would be great to have real time streaming data over the
 * 		RS-232 link, to enable video-game/joy-stick based control, but through the
 * 		terminal using the keypad and control characters.
 *
 * 		Kevin Hartwig, Ovi Ofrim, James Sonnenberg
 *
 */

// LIBRARIES / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
#include "menu.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <unistd.h>
#include <string.h>
#include <ftw.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include "myUtil.h"

// SETTINGS / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
#define	MAXREAD			255			//Maximum input buffer length
#define NUMBER_TIMEOUTS 2			//Number of alarm TIMEOUTS before aborting a
									//verification from the COMMS manager
static unsigned char *COMMS_PATH = 	"/home/k3v/Desktop/eclipse"
									"/Projects/LinuxSupervisor/COMMS";
static unsigned char lastCommand[100] = {0};
static unsigned char CommandCheck[5] = {0};

enum COMMStartupOptions {
		TERM = 1,			//Used to specify whether COMMS should start in a new
		NOTERM,				//terminal (passed as argument to startCOMMS)
};

// PROTOTYPES / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
static int startCOMMS(int option);
static int servoModuleTest(void);
static int VerifyMessage(pid_t pid);
static int GetCommPid(void);
static void pCommandMenu(void);

// SIGNAL STUFF / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
// Signal Handlers - - -  //
static void signal_handler_VERIFY(int status);		/* Linked to SIGUSR1 		*/
static void signal_handler_ALRM(int status); 		/* Linked to SIGARLM 		*/

// Signal Flags - - - - - //
volatile static sig_atomic_t VERIFIED = FALSE;		/* COMMS verification flag  */
volatile static sig_atomic_t TIMEOUT = FALSE;		/* Alarm timeout flag		*/

// MAINLINE / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
int main(void) {
	char 	choice;			/* Track user menu choice   */
	pid_t	pid;			/* For general forking use	*/
	char	input[MAXREAD];	/* Buffer to read to		*/
	int 	loop;
	int		check;
	char **p;

	struct 	sigaction	commverify;
	commverify.sa_handler = signal_handler_VERIFY;
	commverify.sa_flags = SA_RESTART;
	commverify.sa_restorer = NULL;
	sigaction(SIGUSR1, &commverify, NULL);

	do {
		clear();
		choice = getchoice("What's up!? (Press ESC at any time to quit)", menu) - '1';
		switch(choice) {
			case COMMAND:		/* COMMAND MODE */

					// Start COMM manager with NOTERM selected
					if (startCOMMS(NOTERM)) {
						printf("Failed to start COMMS"); fflush(stdout);
						return(1);
					}
					/* Give COMMS a second to start */
					sleep(1);
					/* Retrieve child process ID */
					while (!(pid = GetCommPid()));
					/* Ensure child exists */		//THIS DOESNT WORK RIGHT NOW
					if ((kill(pid, 0)) == -1) {
						printf("COMMS exited...."); fflush(stdout);
						break;
					}

					do {
						pCommandMenu();
						fgets(input, sizeof(input), stdin);
						rmvNewline(input);
						strcpy(lastCommand, input);
						if (!strcmp(input, "quit")) break;
						if (TransmitMessage(input)){
							printf("\nError writing to COMMTRANSMIT IPC file...");
							fflush(stdout);
						}else {
							printf("\t\t>> SENT"); fflush(stdout); VERIFIED = FALSE;
							loop = NUMBER_TIMEOUTS;
							while (loop){
								kill(pid, SIGUSR1);
								signal(SIGALRM, signal_handler_ALRM);
								TIMEOUT=FALSE;
								alarm(1);
								while(!VERIFIED && !TIMEOUT);
								if (VERIFIED) {
									printf("\n\t\t>> RECEIVED BY COMM MANAGER\n");
									VERIFIED = FALSE;
									TIMEOUT = FALSE;
									break;
								}
								else {
									printf("\n\n\t>> VERIFICATION TIMED OUT (%i)\n",loop);
									loop--;
								}
							}
						}
					} while(strcmp(input,"quit") != 0);
				kill(pid, SIGUSR2);
				waitpid(pid, NULL, NULL);
				choice = 0;
				break;
			case MODULE:		/* MODULE TESTING */
				do {
					choice = getchoice("What module? (Press ESC to go back)\n", TestingMenu) - '1';
					switch(choice) {
						case SERVO:	/* SERVO */
								servoModuleTest();
							break;

						case STEPPER:	/* STEPPER */

							break;

						case DCMOTOR: 	/* DC MOTOR */

							break;
					}
				} while (choice != ESC);
				choice = 0;
				break;

			case TERMINAL:
				/* Start COMMS manager in new terminal */
				if (startCOMMS(TERM)) {
					/* Failed to start COMMS*/;
					printf("Failed to start COMMS");
					break;
				}
				/* Give child a second to start */
				sleep(1);
				/* Retrieve child process ID */
				pid = GetCommPid();
				/* Infinite loop until "quit" */
				clear(); printf("SUPERVISOR TRANSMIT TERMINAL ('quit' to go back)\n\n");
				do {
					fgets(input, sizeof(input), stdin);
					rmvNewline(input);
					if (!strcmp(input, "quit")) break;
					if (TransmitMessage(input)){
						printf("\nError writing to COMMTRANSMIT IPC file...");
						fflush(stdout);
					}else {
						printf("\t\t>> SENT"); fflush(stdout); VERIFIED = FALSE;
						loop = NUMBER_TIMEOUTS;
						while (loop){
							kill(pid, SIGUSR1);			//Tell COMM there is a message to read
							signal(SIGALRM, signal_handler_ALRM);
							TIMEOUT=FALSE;				//Reset TIMEOUT flag
							alarm(1);					//Set timeout for COMM verification that msg rx'd
							while(!VERIFIED && !TIMEOUT);
							if (VERIFIED) {				//Verified!
								printf("\n\t\t>> RECEIVED BY COMM MANAGER\n");
								VERIFIED = FALSE;
								TIMEOUT = FALSE;
								break;
							}
							else {						//Timed out!
								printf("\n\n\t>> VERIFICATION TIMED OUT (%i)\n",loop);
								loop--;
							}
						}
					}
				} while(strcmp(input,"quit") != 0);
				kill(pid, SIGUSR2);						//Terminate COMMS
				while ((kill(pid, 0)) != -1);			//Wait for COMMS to exit
				break;

			case ESC:
				return(0);
				break;
		}


	} while (choice != ESC);
}
/*/ / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
 *
 * 	int servoModuleTest(void)
 *
 * 	Turns on the RC motor for 7 seconds then halts it.
 *
 *  */
int servoModuleTest(void) {
	int loop;
	pid_t pid;

	// Start COMM manager with NOTERM selected
	if (startCOMMS(NOTERM)) {
		printf("Failed to start COMMS"); fflush(stdout);
		return(1);
	}
	/* Give COMMS a second to start */
	sleep(5);
	/* Retrieve child process ID */
	pid = GetCommPid();

	/* Send the TESTRC command */
	if (TransmitMessage("<R 180>")){
		printf("\nError writing to COMMTRANSMIT IPC file...");
		fflush(stdout);
	}else{
		printf("\t\t>> SENT"); fflush(stdout); VERIFIED = FALSE;
		VerifyMessage(pid);
	}
	signal(SIGALRM, signal_handler_ALRM);
	TIMEOUT = 0;
	alarm(5);
	/* Send the HALTRC command */
	if (TransmitMessage("<R 0>")){
		printf("\nError writing to COMMTRANSMIT IPC file...");
		fflush(stdout);
	}else{
		printf("\t\t>> SENT"); fflush(stdout); VERIFIED = FALSE;
		VerifyMessage(pid);
	}
	sleep(3);
	/* Kill COMM manager */
	kill(pid, SIGKILL);
	return(0);

}

static int VerifyMessage(pid_t pid) {
	int loop = NUMBER_TIMEOUTS;
	while (loop){
		kill(pid, SIGUSR1);
		signal(SIGALRM, signal_handler_ALRM);
		TIMEOUT=FALSE;
		alarm(1);
		while(!VERIFIED && !TIMEOUT);
		if (VERIFIED) {
			printf("\n\t\t>> RECEIVED BY COMM MANAGER\n");
			VERIFIED = FALSE;
			TIMEOUT = FALSE;
			return 0;
		}
		else {
			printf("\n\n\t>> VERIFICATION TIMED OUT (%i)\n",loop);
			return 1;
		}
	}
	return(1);
}

int startCOMMS(int option) {
	pid_t pid;
	char PSPPid[10];

	//Parse PSP PID into char buffer to be passed to COMM manager
	pid = getpid();
	sprintf(PSPPid, "%i", (int)pid);

	//Launch according to int option (TERM or NOTERM?)
	if (option == TERM) {		//open COMMs in separate terminal

		if ((pid = fork()) < 0) {printf("Error forking!"); return(1); }
		else if (!pid) { //CHILD
			execlp("gnome-terminal", "gnome-terminal",
					"-x",
					COMMS_PATH,
					PSPPid,
					NULL
					);
			printf("Child returned from execlp (ERROR)...");
			exit(-1);
		} else {
			return(0);
		}
	}
	else if (option == NOTERM){	// open COMMs in PSP Terminal
		if ((pid = fork()) < 0) {printf("Error forking!"); return(1); }
			else if (!pid) { //CHILD
				execlp(	COMMS_PATH,
						"COMMS",
						PSPPid,
						"NOTERM",
						NULL
						);
				printf("Child returned from execlp (ERROR)..."); fflush(stdout);
				exit(-1);
			} else {
				return(0);
			}
	}
	return(1); 	// option passed as argument was invalid
}

int TransmitMessage(char *buffer){
	FILE * fp;
	fp = fopen("./IPCFiles/COMMTRANSMIT", "w");
	if (fp){
		fputs(buffer,fp);
		fclose(fp);
		return(0);
	} else
		return(1);
}

pid_t GetCommPid(void) {
	FILE 	*fp;
	pid_t	pid;
	char	buf[100];

	fp = fopen("./IPCFiles/COMMPID", "r");
	if (fp){
		pid = atoi(fgets(buf, sizeof(buf), fp));
		fclose(fp);
		if(pid) { return(pid); } else { return(0); };
	} else
		return(0);
}

static void pCommandMenu(void) {
	// Print the command menu
	char **p;
	clear();
	p = CommandMenu;
	while (*p) {
		printf("%s\n", *p);
		p++;
	}
	printf("\t\t\t\t\t\t> Last: \t%s\n", lastCommand);
	//printf("\t\t\t\t\t\t> Confirmed: \t%s\n\n", CommandCheck);
}

static void signal_handler_VERIFY(int status){
	VERIFIED = TRUE;
}

static void signal_handler_ALRM(int status){
	TIMEOUT = TRUE;
}

