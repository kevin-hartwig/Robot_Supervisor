/*
 * 		myUtil.h
 *
 * 		General utilities, macros, defines, and so on
 *
 */// / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
#ifndef _myutil_h
#define _myutil_h

#define FALSE 0
#define TRUE 1

#define clear() printf("\033[H\033[J")			// Clear the screen

void flushstdin (void);
void probe(char *s);
char getch(void);
char getche(void);
void rmvNewline(char*);

static struct termios old, new;

void flushstdin (void)
{
	while ( getchar() != '\n' );
}

void probe(char *s){
	printf("\nPROBE:(%s)", s); fflush(stdout);
}

// Replace the first newline (if found before a null terminator) with a null terminator
// in the character string pointed to by *track
void rmvNewline(char *track){
	while(*track) {
		if (*track == '\n') {
			*track = '\0';
			break;
		}else
			track++;
	}
}

















/* GETCH AND GETCHE IMPLEMENTATION FROM
 * STACK OVERFLOW!
 */
/* Initialize new terminal i/o settings */
static void initTermios(int echo)
{
  tcgetattr(0, &old); 						/* grab old terminal i/o settings */
  new = old; 								/* make new settings same as old settings */
  new.c_lflag &= ~ICANON; 					/* disable buffered i/o */
  new.c_lflag &= echo ? ECHO : ~ECHO; 		/* set echo mode */
  tcsetattr(0, TCSANOW, &new); 				/* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
static void resetTermios(void)
{
  tcsetattr(0, TCSANOW, &old);
}

/* Read 1 character - echo defines echo mode */
static char getch_(int echo)
{
  char ch;
  initTermios(echo);
  ch = getchar();
  resetTermios();
  return ch;
}

/* Read 1 character without echo */
char getch(void)
{
  return getch_(0);
}

/* Read 1 character with echo */
char getche(void)
{
  return getch_(1);
}


#endif			/* _myutil_h */
