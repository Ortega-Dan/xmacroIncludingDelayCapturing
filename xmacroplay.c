/***************************************************************************** 
 * Includes
 ****************************************************************************/
#include <stdio.h>		
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <X11/Xlibint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysymdef.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <sys/types.h>
#include <sys/wait.h>


#include "chartbl.h"
#include "FieldNames.h"
/* Strings */
char **string_split(char *path);
void string_split_free(char **retv);

/* Config object */
config_obj *cfg = NULL;
/**
 * Playback speed
 */
float speed = 1.0;
FILE *fdin = NULL;
/***************************************************************************** 
 * Globals...
 ****************************************************************************/
int   Delay = 10;
float Scale = 1.0;
char * Remote;

/****************************************************************************/
/*! Prints the usage, i.e. how the program is used. Exits the application with
    the passed exit-code.

	\arg const int ExitCode - the exitcode to use for exiting.
*/
/****************************************************************************/
void usage (const int exitCode) 
{
	// print the usage
	fprintf (stderr, "%s %s\n", PROG, VERSION);
	fprintf (stderr, "Usage: %s [options]\n", PROG);
	fprintf (stderr, "Options:\n");
	fprintf (stderr,
			"  -d DELAY   		delay in milliseconds for events sent to remote display. (default 10ms)\n"\
			"  --speed SPEED	Playback speed. 0.5 is twice as fast. 2 is 1/2 speed"\
			"  -s  	FACTOR  	scalefactor for coordinates. Default: 1.0.\n"\
			"  -v          		show version. \n"\
			"  -h          		this help. \n\n"\
			);

	// we're done
	exit ( EXIT_SUCCESS );
}


/****************************************************************************/
/*! Prints the version of the application and exits.
*/
/****************************************************************************/
void version () {

	/* print the version  */
	fprintf (stderr, "%s %s\n", PROG, VERSION);

	/* we're done */
	exit ( EXIT_SUCCESS );
}


/****************************************************************************/
/*! Parses the commandline and stores all data in globals (shudder). Exits
  the application with a failed exitcode if a parameter is illegal.

  \arg int argc - number of commandline arguments.
  \arg char * argv[] - vector of the commandline argument strings.
  */
/****************************************************************************/
void parseCommandLine (int argc, char * argv[]) {

	int Index = 1;

	// check the number of arguments
	if ( argc < 1 ) {
		// oops, too few arguments, go away
		usage ( EXIT_FAILURE );
	}

	// loop through all arguments except the last, which is assumed to be the
	// name of the display
	while ( Index < argc ) {

		// is this '-v'?
		if ( strcmp (argv[Index], "-v" ) == 0 ) {
			// yep, show version and exit
			version ();
		}

		// is this '-h'?
		if ( strcmp (argv[Index], "-h" ) == 0 ) {
			// yep, show usage and exit
			usage ( EXIT_SUCCESS );
		}
		else if ( strcmp (argv[Index], "-i") == 0 && Index+1 < argc ) {
			FILE *fp = fopen(argv[Index+1], "r");
			if(!fp) {
				fprintf(stderr, "Error opening file: %s:%s\n", argv[Index+1], strerror(errno));

			}
			if(fdin != NULL) {
				/* override config file */
				fclose(fdin);
			}
			fdin = fp;
			Index++;
		}
		// is this '-d'?
		else if ( strcmp (argv[Index], "-d" ) == 0 && Index + 1 < argc ) {
			// yep, and there seems to be a parameter too, interpret it as a
			// number
			if ( sscanf ( argv[Index + 1], "%d", &Delay ) != 1 ) {
				// oops, not a valid intereger
				fprintf(stdout, "Invalid parameter for '-d'.\n");
				usage ( EXIT_FAILURE );
			}

			Index++;
		}
		// is this '--speed'?
		else if ( strcmp (argv[Index], "--speed" ) == 0 && Index + 1 < argc ) {
			// yep, and there seems to be a parameter too, interpret it as a
			// floating point number
			if ( sscanf ( argv[Index + 1], "%f", &speed) != 1 ) {
				// oops, not a valid intereger
				fprintf(stdout, "Invalid parameter for '--speed'.\n");
				usage ( EXIT_FAILURE );
			}

			Index++;
		}


		// is this '-s'?
		else if ( strcmp (argv[Index], "-s" ) == 0 && Index + 1 < argc ) {
			// yep, and there seems to be a parameter too, interpret it as a
			// floating point number
			if ( sscanf ( argv[Index + 1], "%f", &Scale ) != 1 ) {
				// oops, not a valid intereger
				fprintf(stdout, "Invalid parameter for '-s'.\n");
				usage ( EXIT_FAILURE );
			}

			Index++;
		}

		// is this the last parameter?
		else if ( Index == argc - 1 ) {
			// yep, we assume it's the display, store it
			Remote = argv [ Index ];
		}

		else {
			// we got this far, the parameter is no good...
			fprintf (stderr, "Invalid parameter '%s'\n", argv[Index]);
			usage ( EXIT_FAILURE );
		}

		// next value
		Index++;
	}
}

/****************************************************************************/
/*! Connects to the desired display. Returns the \c Display or \c 0 if
  no display could be obtained.

  \arg const char * DisplayName - name of the remote display.
  */
/****************************************************************************/
Display * remoteDisplay (const char * DisplayName) {

	int Event, Error;
	int Major, Minor;  

	// open the display
	Display * D = XOpenDisplay ( DisplayName );

	// did we get it?
	if ( ! D ) {
		// nope, so show error and abort
		fprintf (stderr, "%s: could not open display \"%s\", aborting.\n",
				PROG,XDisplayName (DisplayName));
		exit ( EXIT_FAILURE );
	}

	// does the remote display have the Xtest-extension?
	if ( ! XTestQueryExtension (D, &Event, &Error, &Major, &Minor ) ) {
		// nope, extension not supported
		fprintf(stderr, "%s: XTest extension not supported on server \"%s\"\n",PROG,DisplayString(D));

		// close the display and go away
		XCloseDisplay ( D );
		exit ( EXIT_FAILURE );
	}

	// print some information
	fprintf(stderr, "XTest for server \"%s\" is version %i.%i\n",DisplayString(D), Major,Minor);

	// execute requests even if server is grabbed 
	XTestGrabControl ( D, True ); 

	// sync the server
	XSync ( D,True ); 

	// return the display
	return D;
}

/****************************************************************************/
/*! Scales the passed coordinate with the given saling factor. the factor is
  either given as a commandline argument or it is 1.0.
  */
/****************************************************************************/
int scale (const int Coordinate) {

	// perform the scaling, all in one ugly line
	return (int)( (float)Coordinate * Scale );
}

/****************************************************************************/
/*! Sends a \a character to the remote display \a RemoteDpy. The character is
  converted to a \c KeySym based on a character table and then reconverted to
  a \c KeyCode on the remote display. Seems to work quite ok, apart from
  something weird with the Alt key.

  \arg Display * RemoteDpy - used display.
  \arg char c - character to send.
  */
/****************************************************************************/
void sendChar(Display *RemoteDpy, char c)
{
	KeySym ks, sks, *kss, ksl, ksu;
	KeyCode kc, skc;
	int syms;

	sks=XK_Shift_L;

	ks=XStringToKeysym(chartbl[0][(unsigned char)c]);
	if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
	{
		fprintf(stderr, "No keycode on remote display found for char: %c\n", c);
		
		return;
	}
	if ( ( skc = XKeysymToKeycode ( RemoteDpy, sks ) ) == 0 )
	{
		fprintf(stderr, "No keycode on remote display found for char: XK_Shift_L!\n");
		return;
	}

	kss=XGetKeyboardMapping(RemoteDpy, kc, 1, &syms);
	if (!kss)
	{
		fprintf(stderr, "XGetKeyboardMapping failed on the remote display (keycode: %i)\n", kc);
		return;
	}
	for (; syms && (!kss[syms-1]); syms--);
	if (!syms)
	{
		fprintf(stderr, "XGetKeyboardMapping failed on the remote display (no syms) (keycode: %i)\n", kc);
		XFree(kss);
		return;
	}
	XConvertCase(ks,&ksl,&ksu);
	if (ks==kss[0] && (ks==ksl && ks==ksu)) sks=NoSymbol;
	if (ks==ksl && ks!=ksu) sks=NoSymbol;
	if (sks!=NoSymbol) XTestFakeKeyEvent ( RemoteDpy, skc, True, Delay );
	XTestFakeKeyEvent ( RemoteDpy, kc, True, Delay );
	XFlush ( RemoteDpy );
	XTestFakeKeyEvent ( RemoteDpy, kc, False, Delay );
	if (sks!=NoSymbol) XTestFakeKeyEvent ( RemoteDpy, skc, False, Delay );
	XFlush ( RemoteDpy );
	XFree(kss);
}

/****************************************************************************/
/*! Main event-loop of the application. Loops until a key with the keycode
  \a QuitKey is pressed. Sends all mouse- and key-events to the remote
  display.

  \arg Display * RemoteDpy - used display.
  \arg int RemoteScreen - the used screen.
  */
/****************************************************************************/
void eventLoop (Display * RemoteDpy, int RemoteScreen) {

	char ev[200];
	char *str=NULL;
	int x, y;
	unsigned int b;
	int ks;
	int	kc;


	while ( fgets(ev,200,fdin)) {
		/* terminate the string */
		if(ev[strlen(ev)-1] == '\n')
			ev[strlen(ev)-1] = '\0';
		if (ev[0]=='#')
		{
			continue;
		}
		if (!strncasecmp(DELAY,ev,strlen(DELAY)))
		{
			b = atoi(&ev[strlen(DELAY)+1]);
			usleep ( (int)(speed*b*1000) );
		}
		else if (!strncasecmp(BUTTON_PRESS, ev,strlen(BUTTON_PRESS)))
		{
			b = atoi(&ev[strlen(BUTTON_PRESS)+1]);
			XTestFakeButtonEvent ( RemoteDpy, b, True, Delay );
		}
		else if (!strncasecmp(BUTTON_RELEASE,ev,strlen(BUTTON_RELEASE)))
		{
			b = atoi(&ev[strlen(BUTTON_RELEASE)+1]);
			XTestFakeButtonEvent ( RemoteDpy, b, False, Delay );
		}
		else if (!strncasecmp(MOTION_NOTIFY,ev, strlen(MOTION_NOTIFY)))
		{
			sscanf(ev,MOTION_NOTIFY" %i %i", &x, &y);
			XTestFakeMotionEvent ( RemoteDpy, RemoteScreen , scale ( x ), scale ( y ), Delay ); 
		}
		else if (!strncasecmp(KEY_CODE_PRESS,ev,strlen(KEY_CODE_PRESS)))
		{
			kc = atoi(&ev[strlen(KEY_CODE_PRESS)+1]);
			XTestFakeKeyEvent ( RemoteDpy, kc, True, Delay );
		}
		else if (!strncasecmp(KEY_CODE_RELEASE,ev,strlen(KEY_CODE_RELEASE)))
		{
			kc = atoi(&ev[strlen(KEY_CODE_RELEASE)+1]);
			XTestFakeKeyEvent ( RemoteDpy, kc, False, Delay );
		}
		else if (!strcasecmp(SCREENSHOT, ev))
		{
			int pid;
			fprintf (stderr, "Taking screenshot\n");
			if ((pid=fork()) == 0)
			{ 
				/* This should be fine, even with the fork, it's just reading.. */
				char *filename = cfg_get_single_value_as_string_with_default(cfg, "Programs", "Screenshot","import -window root");
				char *file = cfg_get_single_value_as_string_with_default(cfg, "Programs", "filename","%T-%d-%m-%Y.png");
				char **argvs=NULL;
				int ind=1,iter;
				char buffer[64];
				time_t tim = time(NULL);
				struct tm *lt = gmtime(&tim); 

				strftime(buffer, 64, file,lt);
				free(file);



				/*Parse the filename */
				argvs = realloc(argvs,(ind+1)*sizeof(char*));
				argvs[ind-1] = filename;
				for(iter=0;filename[iter] != '\0';iter++)
				{
					if(filename[iter] == ' '&&filename[iter+1] != '\0')
					{
						ind++;
						argvs = realloc(argvs,(ind+1)*sizeof(char*));
						argvs[ind-1] = &filename[iter+1];
						filename[iter] = '\0';
					}		
				}
				ind++;
				argvs = realloc(argvs,(ind+1)*sizeof(char*));
				argvs[ind-1] = buffer;
				/* Close the array */
				argvs[ind] = NULL;

				execvp(argvs[0],argvs);
				perror("child");
				free(filename);
				free(argvs);
				exit(1);
			}                                                              	
			if(cfg_get_single_value_as_int_with_default(cfg, "Programs","Block on screenshot",true))
			{
				printf("Waiting on screenshot\n");
				wait(NULL);
				printf("Done\n");
			}
		}
		else if (!strncmp(EXEC_BLOCK, ev, strlen(EXEC_BLOCK)))
		{
			char *script = strdup(&ev[strlen(EXEC_BLOCK)+1]);
			int pid;
			fprintf (stderr, "Exec '%s' blocking\n",script);			
			if ((pid=fork()) == 0)
			{
				char **argv= string_split(script); 
				execvp(argv[0], argv);
				string_split_free(argv);
				free(script);
				exit(1);
			}
			/* Wait for execvp to be done */
			wait(NULL);
		}
		else if (!strncmp(EXEC_NO_BLOCK, ev, strlen(EXEC_NO_BLOCK)))
		{
			char *script = strdup(&ev[strlen(EXEC_NO_BLOCK)+1]);
			int pid;
			fprintf (stderr, "Exec '%s' non-blocking\n",script);			
			if ((pid=fork()) == 0)
			{
				char **argv= string_split(script); 
				execvp(argv[0], argv);
				string_split_free(argv);
				free(script);
				exit(1);
			}
		}
		else if (!strncasecmp(KEY_SYM_PRESS,ev, strlen(KEY_SYM_PRESS)))
		{
			ks = atoi(&ev[strlen(KEY_SYM_PRESS)+1]);
			if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
			{
				continue;
			}
			XTestFakeKeyEvent ( RemoteDpy, kc, True, Delay );
		}
		else if (!strncasecmp(KEY_SYM_RELEASE,ev,strlen(KEY_SYM_RELEASE)))
		{
			ks = atoi(&ev[strlen(KEY_SYM_RELEASE)+1]);
			if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
			{
				continue;
			}
			XTestFakeKeyEvent ( RemoteDpy, kc, False, Delay );
		}
		else if (!strncasecmp(KEY_SYM,ev, strlen(KEY_SYM)))
		{
			ks = atoi(&ev[strlen(KEY_SYM)+1]);
			if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
			{
				fprintf(stderr, "No keycode on remote display found for keysym: %i",ks);
				continue;
			}
			XTestFakeKeyEvent ( RemoteDpy, kc, True, Delay );
			XFlush ( RemoteDpy );
			XTestFakeKeyEvent ( RemoteDpy, kc, False, Delay );
		}
		else if (!strncasecmp(KEY_STR_PRESS,ev,strlen(KEY_STR_PRESS)))
		{
			ks=XStringToKeysym(&(ev[strlen(KEY_STR_PRESS)+1]));
			if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
			{
				//	cerr << "No keycode on remote display found for '" << ev << "': " << ks << endl;
				fprintf(stderr, "No keycode on remote display found for '%s':%i\n", ev, ks);
				continue;
			}
			XTestFakeKeyEvent ( RemoteDpy, (KeyCode)kc, True, Delay );
		}
		else if (!strncasecmp(KEY_STR_RELEASE,ev,strlen(KEY_STR_RELEASE)))
		{
			ks=XStringToKeysym(&(ev[strlen(KEY_STR_RELEASE)+1]));

			if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
			{
				fprintf(stderr, "No keycode on remote display found for '%s':%i\n", ev, ks);
				continue;
			}
			XTestFakeKeyEvent ( RemoteDpy, (KeyCode)kc, False, Delay );
		}
		else if (!strncasecmp(KEY_STR,ev,strlen(KEY_STR)))
		{
			ks=XStringToKeysym(&(ev[strlen(KEY_STR)+1]));
			if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
			{
				fprintf(stderr, "No keycode on remote display found for '%s':%i\n", ev, ks);
				continue;
			}
			XTestFakeKeyEvent ( RemoteDpy, kc, True, Delay );
			XFlush ( RemoteDpy );
			XTestFakeKeyEvent ( RemoteDpy, kc, False, Delay );
		}
		else if (!strncasecmp(STRING,ev,strlen(STRING)))
		{
			str = &(ev[strlen(STRING)+1]);
			b=0;
			while(str[b]) sendChar(RemoteDpy, str[b++]);
		}
		else{
			fprintf(stderr, "error: %s\n", ev);
		}

		// sync the remote server
		XFlush ( RemoteDpy );
	} 
}


/****************************************************************************/
/*! Main function of the application. It expects no commandline arguments.

  \arg int argc - number of commandline arguments.
  \arg char * argv[] - vector of the commandline argument strings.
 */
/****************************************************************************/
int main (int argc, char * argv[]) 
{
	char *filename = NULL;
	/* Open the configuration file */
	cfg = cfg_open("xmacro.ini"); 

	/* Load default values from config */
	speed = cfg_get_single_value_as_float_with_default(cfg, "Playback", "speed",1.0);
	Scale = cfg_get_single_value_as_float_with_default(cfg, "Playback", "scale",1.0);
	Delay = cfg_get_single_value_as_int_with_default(cfg, "Playback", "EventDuration",10);
	filename = cfg_get_single_value_as_string(cfg, "Playback", "InputFile");
	if(filename)
	{
		fdin = fopen(filename,"r");
		if(fdin == NULL)
		{
			fprintf(stderr, "Error opening file: %s:%s\n", filename, strerror(errno));	
			exit(EXIT_FAILURE);
		}
		free(filename);
	}

	// parse commandline arguments
	parseCommandLine ( argc, argv );
	/* If no input file is opened, then open stdin */
	if(fdin == NULL)
	{
		fdin = stdin;
	}

	// open the remote display or abort
	Display * RemoteDpy = remoteDisplay ( Remote );

	// get the screens too
	int RemoteScreen = DefaultScreen ( RemoteDpy );

	XTestDiscard ( RemoteDpy );

	// start the main event loop
	eventLoop ( RemoteDpy, RemoteScreen );

	// discard and even flush all events on the remote display
	XTestDiscard ( RemoteDpy );
	XFlush ( RemoteDpy ); 

	// we're done with the display
	XCloseDisplay ( RemoteDpy );

	fprintf(stderr, "%s:  pointer and keyboard released. \n", PROG);
	/* Close the config system */
	cfg_close(cfg);
	// go away
	exit ( EXIT_SUCCESS );
}

/**
 * String splitting
 */

char ** string_split(char *path)
{
	char *ptr;
	char **retv = NULL;
	int items = 0;
	if(path == NULL)
		return NULL;

	ptr = strtok(path," ");
	while(ptr != NULL )
	{
		items++;
		/* make space */
		retv = realloc(retv,(items+1)*sizeof(char *));
		retv[items] = NULL;
		retv[items-1] = strdup(ptr);
		/* get next token */
		ptr = strtok(NULL, " ");
	}

	return retv;
}


void string_split_free(char **retv)
{
	int i=0;
	/* look all strings */
	for(i=0;retv[i] != NULL;i++)free(retv[i]);
	/* free the array */
	free(retv);
}
