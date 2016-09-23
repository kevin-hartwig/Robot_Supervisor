/*
 *
 * 		COMMS.c
 *
 * 		Source file for the COMMS serial communication manager.
 * 		Last updated: 		April 10, 2016
 * 		Written by:			Kevin Hartwig
 *
 * 			COMMS Serial Communications Manager - for use with the second year,
 * 		second semester project robot control.  The COMM manager acts as an
 * 		intermediary between a Project Supervisory Program and the HCS12-based
 * 		Robot controller board.
 *
 * 		Kevin Hartwig, Ovi Ofrim, James Sonnenberg
 *
 */

// LIBRARIES / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include "myUtil.h"
#include <string.h>

// SETTINGS // / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
#define BAUDRATE 		B9600						// Baud rate
#define COMMPORT 		"/dev/ttyUSB0"
//
#define RXPSP			"./IPCFiles/COMMTRANSMIT"	//
#define MAXREAD			255							// Buffer length maximums
#define MSGLEN			6							// Number of SIGIO receives
													// permitted before a serial
													// read is activated

// GLOBAL DEFINES, MACROS, VARS, ETC / / / / / / / / / / / / / / / / / / / / / / /
#define clear() printf("\033[H\033[J")				/* Clear screen				*/

static struct termios oldtio, newtio;

static unsigned int 	PORTfd;						/* Serial file descriptor 	*/
static unsigned char	ReadPortBuffer[MAXREAD];	/* Buffer for serial read	*/
static unsigned char 	ReadPSPBuffer[MAXREAD];		/* Buffer, Rx from PSP		*/
static useconds_t 		alarmtime = 10000;
static unsigned int 	NOTERM = FALSE;				/* No terminal? Block stdout*/
static unsigned char	lastCommand[100] = {0};
static unsigned char	*VerificationString = "TESTRC";


// SIGNAL STUFF / / / / //
// Signal handler flags // - - - - - - - - - - - - - - - - - - - - - - -
static volatile sig_atomic_t	READ = FALSE;				/* COMM is reading a
															   character		*/
static volatile sig_atomic_t	INREADINGMODE = FALSE;		/* COMM is waiting for
															   a set # characters
															   to be read		*/
static volatile sig_atomic_t	TRANSMIT = FALSE;			/* COMM is transmitting
															   over serial port	*/
static volatile sig_atomic_t 	STOP = FALSE;				/* PSP terminated	*/
static volatile sig_atomic_t	TIMEOUT = FALSE;			/* Alarm timed out	*/
static volatile sig_atomic_t	count = 0;					/* Characters read
 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	   in the current serial
 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	   readin sequence	*/
// Flag Reference // // //
static enum 	FLAGS {								 /* For easy flag reference */
					READING = 1,					 /* - - keep updated - - -  */
					TRANSMITTING,
					STOPPING,
					TIMEDOUT
				} flagReferences;

// Signal Handlers // // //						/* LINKING *// // // // // // // /
static
void signal_handler_PSPTRANSMIT (int status);	/*  - - Linked to SIGUSR1		*/
												// Receiving this signal means
												// PSP is transmitting a mssg
// SIGIO is a special signal....				//
static
void signal_handler_SIGIO (int status, 		    /*  - - Linked to SIGIO 		*/
						   siginfo_t info);   	/* Currently, the second argument
						   						   to the SIGIO handler does not
						   						   work as expected (to pass info
						   						   on the type of IO that generated
						   						   that signal)  This would be
						   						   awesome if it did.  does not
						   						   affect performance currently,
						   						   as SIGIO is ignored if we are
						   						   transmitting (and, this is the
						   						   only time that it would generate
						   						   an unwanted SIGIO signal)*/
static void signal_handler_EXIT (int status);	/*  - - Linked to SIGUSR2		 */
												// Receiving this signal means PSP
												// has requested we terminate
static void signal_handler_ALRM (int status);	/* linked to SIGALRM 			 */

// Local Functions
static int openPort(void);
static int initPort(int fd);
static int WriteCommPid(void);
static int ClearCommPid(void);
static void initSigHandlers(void);
static int CheckFlags(void);
static int ProcessCommand(int flag);
static int	GetMessagePSP(char *s);
static int	BlockSIGIO(int);

static pid_t	PSP;							/* PSP pid 						 */

int main(int argc, char *argv[])
{
	uint flag;			//For use locally, to track the flag being attended to

	if (argc == 3){
		if (!strcmp(argv[2], "NOTERM")){
			fclose(stdout);
		}
	}
	printf("COMMS MANAGER - - - - - - - - - - - - - - - - -\n\n");

	/* Write our PID, for use by PSP */
	if(WriteCommPid()) {
		printf("Failed to write to COMMPID...\n");
		sleep(5);
		return(0);
	}

	/* Get PSP PID					 */
	if(!(PSP = (pid_t)atoi(argv[1]))) {
		printf("Failed to pass PSPPID...\n");
		sleep(5);
		return(0);
	}

	// These functions will exit upon failure. No need to check returns //
	initSigHandlers();		// Initialize signal handlers
	PORTfd = openPort();	// Open port, and get file descriptor
	initPort(PORTfd);		// Initialize port settings

	// Forever Loop, Waiting for Signals (including STOP)
	while (!STOP) {

		if ((flag = CheckFlags()))		//Loop until CheckFlags returns non-zero
			ProcessCommand(flag);		//Process the flag returned
	}
	printf("Leaving COMMS now\n"); fflush(stdout);
	ClearCommPid();
	close(PORTfd);
	/* restore old port settings */
	tcsetattr(PORTfd,TCSANOW,&oldtio);
	exit(1);
}

/*/ / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
 *
 * 	int CheckFlags(void)
 *
 * 		-> Checks all the flags that require action.  These flags are
 * 		atomic access variables and thus, mutual exclusion is ensured.
 *
 * 		The first flag that is set is returned, via its enum reference
 * 		(this is done globally throughout the program to ensure consistent
 * 		access and maintainability)
 *
 * 		Inherently, the order of checking dictates priority.
 *
 *  */
static int CheckFlags(void) {
	if (READ) {return(READING);} else
	if (STOP) {return(STOPPING);} else
	if (TRANSMIT) {return(TRANSMITTING);} else
	if (TIMEOUT) {return(TIMEDOUT); } else
		return(0);
}

/*
 *  LOCAL FUNCTION DEFINITIONS / 	/	/	/	/	/	/	/	/	/	/
 */

/*/ / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
 *
 * 	int GetMessagePSP(char *s)
 *
 * 		-> Reads message from a text file, specified by the global variable
 * 		RXPSP (this is the path name, and needs to be the same as that
 * 		written to by the PSP program before it transmits the signal to
 * 		COMMS to indicate a transmission request.
 *
 * 		Message is read into character buffer pointed to by s, up to
 * 		MAXREAD characters (Buffer passed should be MAXREAD char length)
 *
 * 		Returns 0 upon successful read of a line.  Otherwise, upon both
 * 		failure to open the file at the RXPSP directory path and an empty
 * 		text file or failure to read, returns 1.
 *
 *  */
static int GetMessagePSP(char *s) {
	FILE * fp = fopen(RXPSP, "r");
	if (fp) {
		fgets(s, MAXREAD, fp);
		fclose(fp);
		if (*s) {
			return(0);
		}
		else
			return(1);
	} else
		return(1);
}
/*/ / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
 *
 * 	int initPort(int fd)
 *
 * 	Initiailize the port.  BAUDRATE is applied here.  fd of serial pot
 * 	is passed as the argument and should be checked for void before
 * 	being passed.
 *
 *  */
static int initPort(int fd) {
	tcgetattr(fd,&oldtio); /* save current port settings */
	/* set new port settings for canonical input processing */
	newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON | ~ECHO;
	newtio.c_cc[VMIN]=10;
	newtio.c_cc[VTIME]=10;
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd,TCSANOW,&newtio);

	return(1);
}

/*/ / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
 *
 *	int openPort(void)
 *
 * 		-> opens the port located at the directory path contained in
 * 		globally defined COMMPORT.
 *
 * 		This function will exit upon failure.  Otherwise, one is returned,
 * 		but this does not really need to be checked.  Error messages are
 * 		printed and the process sleeps to allow recognition before
 * 		exiting.
 *
 * 		Port is opened in asynchronous mode using the O_ASYNC flag, which
 * 		also allows this process to receive the SIGIO signal whenever
 * 		a change of status occurs on the port.  This is generically
 * 		every time a new character is ready to be read from the serial
 * 		port and whenever a new character can fit into the transmit buffer
 * 		of the serial port.
 *
 * 		Since we only need the receive buffer currently, all we must do
 * 		is ignore SIGIO whenever we are transmitting, and this will reduce
 * 		SIGIOs function to signal that a character is ready to read.
 *
 * 		This inherently removes the ability to asynchronously read and
 * 		write data, as SIGIO read availables will be lost while transmitting.
 * 		This could be easily fixed if instead they were COUNTED while transmitting
 * 		(IDIOT)
 *
 *  */
static int openPort(void) {

	/* open the device to be non-blocking (read will return immediately) */
	int fd = open(COMMPORT, O_RDWR | O_NOCTTY ); //TOOK OUT NONBLOCK
	if (fd == -1) {
		perror(COMMPORT);
		printf("\nExiting...."); fflush(stdout)	;
		sleep(2);
		exit(0);
	}
	fcntl(fd, F_SETFL, 0);
	/* allow the process to receive SIGIO */
	fcntl(fd, F_SETOWN, getpid());
	/* Make the file descriptor asynchronous (the manual page says only
	   O_APPEND and O_NONBLOCK, will work with F_SETFL...) */
	int oflags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, oflags | O_ASYNC );		//TOOK OUT NONBLOCK O_ASYNC

	return(fd);
}
/*/ / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
 *
 * 	void initSigHandlers(void)
 *
 * 	Initializes all signal handlers and signal routing.
 *
 * 	SIGIO is connected to signal_handler_SIGIO with the SA_SIGINFO flag
 * 	but this does not seem to work in passing additional info to the
 * 	signal handler....
 *
 *  */
static void initSigHandlers(void) {
	struct sigaction saio;           /* definition of signal action */
	struct sigaction frompsp;
	struct sigaction exit;

	/* install the signal handler before making the device asynchronous */
	saio.sa_sigaction = (void*)signal_handler_SIGIO;
//	saio.sa_mask = 0;
	saio.sa_flags = SA_SIGINFO;
	saio.sa_restorer = NULL;
	sigaction(SIGIO,&saio,NULL);

	/* install signal handler for transmit request from PSP */
	frompsp.sa_handler = signal_handler_PSPTRANSMIT;
	frompsp.sa_flags = 0;
	frompsp.sa_restorer = NULL;
	sigaction(SIGUSR1,&frompsp,NULL);

	/* install signal handler for EXIT */
	exit.sa_handler = signal_handler_EXIT;
	exit.sa_flags = 0;
	exit.sa_restorer = NULL;
	sigaction(SIGUSR2,&exit,NULL);

}

static int ClearCommPid(void) {
	char buffer[100] = {0};
	FILE * fp = fopen("./IPCFiles/COMMPID", "w");
	if (fp) {
		fputs(buffer, fp);
		fclose(fp);
		return(0);
	} else
		return(1);
}

/*/ / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
 *
 * 	int WriteCommPid(void)
 *
 * 		Write our PIDD to a text file so that the PSP can read it
 *
 * 		Returns 0 on success, 1 on failure
 *
 *  */
static int WriteCommPid(void) {
	char buffer[100];
	FILE * fp = fopen("./IPCFiles/COMMPID", "w");
	if (fp) {
		sprintf(buffer, "%i", getpid());
		fputs(buffer, fp);
		fclose(fp);
		return(0);
	} else
		return(1);
}
/*/ / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
 *
 * 	int ProcessCommand(int flag)
 *
 * 		-> Processes the command associated with the designated flag,
 * 		passed as an integer argument.  All of these flags should be
 * 		referenced via the enum FLAGS, to ensure consistency and easy
 * 		referencing throughout the program.
 *
 * 		This is where all the work of the COMMS process is done.
 *
 *  */
int ProcessCommand(int flag) {
	int bytes = 1,len;
	char * p = ReadPortBuffer;
	switch(flag) {
		case READING:
			// Clear the buffer
			while (*p) {
				*p = 0;
				p++;
			}
			// Read the port
			if ((bytes = read(PORTfd,ReadPortBuffer,MAXREAD)) == -1){
				//perror("read");

				READ = FALSE;
				INREADINGMODE = FALSE;
				return(0);
			}
			if (bytes == 0) {
				printf("Tried to read....No bytes read...");
			}
			ReadPortBuffer[bytes] = 0;			/* Insert string terminator */
			printf("\n<ECHO> %s", ReadPortBuffer); fflush(stdout);
			if (!(strcmp(ReadPortBuffer,VerificationString))) {
				printf("LAST COMMAND VERIFIED"); fflush(stdout);
			}
			READ = FALSE;       /* reset flag */
			INREADINGMODE = FALSE;
			break;
		case TRANSMITTING:
			//BlockSIGIO(TRUE);
			// Verify we have received transmit request
			kill(PSP,SIGUSR1);

			// Attempt to read from the file, COMMTRANSMIT
			if (GetMessagePSP(ReadPSPBuffer)){
				printf("\nFailed to read from COMMTRANSMIT");
			}
			else {
				// Check for quit
				if(strcmp(ReadPSPBuffer, "quit") == 0) break;
				strcpy(lastCommand, ReadPSPBuffer);
				len = strlen(ReadPSPBuffer);
				bytes = write(PORTfd, ReadPSPBuffer, len);
				if (bytes != len) {
					printf("Failed to write entire string?");
					fflush(stdout);
				}else {
					printf("\n<SENT> %s", ReadPSPBuffer);
					fflush(stdout);
				}
				TIMEOUT = FALSE;
				//ualarm(alarmtime, 0);
				alarm(1);
				signal(SIGALRM, signal_handler_ALRM);
				while(!TIMEOUT);
				TIMEOUT = FALSE;
				//BlockSIGIO(FALSE);
				//sleep(3);
				//nanosleep(&delay);
			}
			TRANSMIT = FALSE;
			break;
		case TIMEDOUT:
			count = 0;
			READ = TRUE;
			TIMEOUT = FALSE;
			printf("\nPCP attempted to send a message (TIMED OUT)\nIncomplete/Corrupt Message:"); fflush(stdout);
			break;
		case STOPPING:
			/* No action needs to be taken, as our while loop
			 * in main will exit when STOP is TRUE! */
			break;
		default:
			printf("\nEntered ProcessCommand() but switch defaulted.");
			return(1);
			break;
	}
	return 0;

}


/*/ / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
 *
 * 	static int BlockSIGIO(int block)
 *
 * 	Blocks the SIGIO signal from being received from this process. While
 * 	blocked, the signal is pending and should be received immediately
 * 	upon unblocking.
 *
 * 	Passing in TRUE or 1 will block SIGIO.  Passing in FALSE or 0 will
 * 	unblock SIGIO.
 *
 * 	Returns 0 on success, else 1 on failure to apply sigprocmask.
 *
 * 	Not sure if this is working.
 *
 *  */
static int BlockSIGIO(int block){
	static sigset_t newMask, oldMask;

	if (block){
		sigemptyset(&newMask);
		sigemptyset(&oldMask);

		//Blocks the SIGHUP signal (by adding SIGHUP to newMask)
		sigaddset(&newMask, SIGIO);
		if (sigprocmask(SIG_BLOCK, &newMask, &oldMask)){
			perror("sigprocmask");
			return 1;
		}
	} else {
		// Restores the old mask
		if (sigprocmask(SIG_SETMASK, &oldMask, NULL)) {
			perror("sigprocmask");
			return 1;
		}
	}

	return 0;
}

/*
 *  SIGNAL HANDLERS	/	/	/	/	/	/	/	/	/	/	/	/	/	/
 */
/***************************************************************************
* Sets TRANSMIT flag, indicating PSP has requested a transmission          *
***************************************************************************/
static void signal_handler_PSPTRANSMIT(int status) {
	//signal(SIGIO, signalDump);
	TRANSMIT = TRUE;
}
/***************************************************************************
* Sets STOP flag, indicating PSP has requested termination		           *
***************************************************************************/
static void signal_handler_EXIT(int status) {
	STOP = TRUE;
}
/***************************************************************************
* Sets READ flag, and starts a count of SIGIOs receive, up to MSGLEN chars
*
* 		NOTE: This handler will result in SIGIO signals being ignored
* 		whenever a transmit is in effect. This is not good, but currently
* 		there is no way to differentiate between the SIGIOs generated in
* 		O/P ready vs I/P available.  Documenation states that siginfo_t
* 		argument should pass this information, if sigaction() is called
* 		with the flag SA_SIGINFO, but the if statements below currently
* 		do not trigger. TO DO!!!
***************************************************************************/
static void signal_handler_SIGIO (int status, siginfo_t info)
{
	/*
	if (info.si_band == POLL_IN){
		printf("\nRECEIVED SIGIO, POLL_IN"); fflush(stdout);
	}
	if (info.si_code == POLL_MSG) {
		printf("\nRECEIVED SIGIO, POLL_MSG"); fflush(stdout);
	}
	if (info.si_band == POLL_MSG) {
		printf("\nRECEIVED SIGIO, POLL_MSG(si_band)"); fflush(stdout);
	}
	*/

	if (!TRANSMIT){
		if(count == 0){
			TIMEOUT = FALSE;
			signal(SIGALRM, signal_handler_ALRM);
			INREADINGMODE = TRUE;
			alarm(2);
		}
		//printf("\n<received SIGIO signal>\n"); fflush(stdout);
		count++;
		//printf("Count = %i", count);
		if (count == MSGLEN) {
			READ = TRUE;
			count = 0;
			alarm(0);
			//printf("Count reset");
		}
	}
}
/***************************************************************************
* Sets TIMEOUT, indicating alarm timed out						           *
***************************************************************************/
static void signal_handler_ALRM(int status) {
	TIMEOUT = TRUE;
}
