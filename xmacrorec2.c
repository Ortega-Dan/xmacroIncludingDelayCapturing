/***************************************************************************** 
 * Includes
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <X11/Xlibint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysymdef.h>
#include <X11/keysym.h>
#include <X11/extensions/record.h>
#include "FieldNames.h"
#include "config1.h"
config_obj *cfg = NULL;

/***************************************************************************** 
 * Globals...
 ****************************************************************************/
/* delay */
int Delay = 10;
/* scaling of pixels */
float Scale = 1.0;
/* output dev */
FILE *fdout = 0;
/** the quit hotkey */
unsigned int QuitKey;
int HasQuitKey = false;
/** The screenshot hotkey */
unsigned int ScreenshotKey;
int HasScreenshotKey = false;

/** Value used to print delays interactions */
Time last_time = 0;
/***************************************************************************** 
 * Private data used in eventCallback.
 ****************************************************************************/
typedef struct
{
	int Status1, Status2, x, y, mmoved, doit;
	Display *LocalDpy, *RecDpy;
	XRecordContext rc;
} Priv;

/****************************************************************************/
/*! Prints the usage, i.e. how the program is used. Exits the application with
  the passed exit-code.

  \arg const int ExitCode - the exitcode to use for exiting.
  */
/****************************************************************************/
void usage (const int exitCode)
{
	/* print the usage */
	fprintf (stderr, "%s %s\n", PROG, VERSION);
	fprintf (stderr, "Usage: %s [options]\n", PROG);
	fprintf (stderr, "Options:\n");
	fprintf (stderr,
			"  -s  FACTOR  scalefactor for coordinates. Default: 1.0.\n"\
			"  -p  KEYCODE the keycode for the key used fo taking a screenshot\n"\
			"  -k  KEYCODE the keycode for the key used for quitting.\n"\
			"  -v  show version. \n"\
			"  -h  this help. \n"\
			"  -o  output file\n");

	/* we're done */
	exit (exitCode);
}


/****************************************************************************/
/*! Prints the version of the application and exits.
*/
/****************************************************************************/
void version (void)
{
	/* print the version */
	fprintf (stderr, "%s %s\n", PROG, VERSION);
	/* we're done */
	exit (EXIT_SUCCESS);
}

/****************************************************************************/
/*! Parses the commandline and stores all data in globals (shudder). Exits
  the application with a failed exitcode if a parameter is illegal.

  \arg int argc - number of commandline arguments.
  \arg char * argv[] - vector of the commandline argument strings.
  */
/****************************************************************************/
void parseCommandLine (int argc, char *argv[])
{
	int Index = 1;

	/* loop through all arguments except the last, which is assumed to be the 
	 name of the display */
	while (Index < argc)
	{
		/* is this '-v'? */
		if (strcmp (argv[Index], "-v") == 0)
		{
			/* yep, show version and exit */
			version ();
		}
		/* is this '-h'? */
		if (strcmp (argv[Index], "-h") == 0)
		{
			/* yep, show usage and exit */
			usage (EXIT_SUCCESS);
		}
		/* is this '-o' ? */
		if (strcmp(argv[Index], "-o") == 0 && Index +1 < argc)
		{
			FILE *fp = fopen(argv[Index+1], "w");
			if(fp == NULL)
			{
				fprintf(stderr, "Failed to open: '%s': %s\n", argv[Index+1], strerror(errno));
				exit(EXIT_FAILURE);
			}
			if(fdout != NULL) {
				fclose(fdout);
			}
			fdout = fp;
			Index++;
		}
		/* is this '-s'? */
		else if (strcmp (argv[Index], "-s") == 0 && Index + 1 < argc)
		{
			/* yep, and there seems to be a parameter too, interpret it as a
			 floating point number
			*/
			if (sscanf (argv[Index + 1], "%f", &Scale) != 1)
			{
				/* oops, not a valid intereger */
				fprintf (stderr, "Invalid parameter for '-s'.\n");
				usage (EXIT_FAILURE);
			}
			Index++;
		}
		/* is this '-k'? */
		else if (strcmp (argv[Index], "-k") == 0 && Index + 1 < argc)
		{
			/* yep, and there seems to be a parameter too, interpret it as a */
			if (sscanf (argv[Index + 1], "%u", &QuitKey) != 1)
			{
				/* oops, not a valid integer */
				fprintf (stderr, "Invalid parameter for '-k'. %i\n", QuitKey);
				usage (EXIT_FAILURE);
			}
			/* now we have a key for quitting */
			HasQuitKey = true;
			Index++;
		}
		/* is this '-p'? */
		else if (strcmp (argv[Index], "-p") == 0 && Index + 1 < argc)
		{
			/* yep, and there seems to be a parameter too, interpret it as a number	*/
			if (sscanf (argv[Index + 1], "%u", &ScreenshotKey) != 1)
			{
				/* oops, not a valid integer */
				fprintf (stderr, "Invalid parameter for '-k'. %i\n",ScreenshotKey );
				usage (EXIT_FAILURE);
			}
			HasScreenshotKey = true;
			Index++;
		}
		else
		{
			/* we got this far, the parameter is no good... */
			fprintf (stderr, "Invalid parameter '%s'\n", argv[Index]);
			usage (EXIT_FAILURE);
		}
		/* next value */
		Index++;
	}
}


/****************************************************************************/
/*! Connects to the local display. Returns the \c Display or \c 0 if
  no display could be obtained.
  */
/****************************************************************************/
Display * localDisplay ()
{
	/* open the display */
	Display *D = XOpenDisplay (0);
	/* did we get it? */
	if (!D)
	{
		/* nope, so show error and abort */
		fprintf (stderr, "%s: could not open display \"%s\", aborting.\n",
				PROG,XDisplayName (0));
		exit (EXIT_FAILURE);
	}
	/* return the display */
	return D;
}

/****************************************************************************/
/*! Function that finds out the key the user wishes to use for quitting the
  application. This must be configurable, as a suitable key is not always
  possible to determine in advance. By letting the user pick a key one that
  does not interfere with the needed applications can be chosen.

  The function grabs the keyboard and waits for a key to be pressed. Returns
  the keycode of the pressed key.

  \arg Display * Dpy - used display.
  \arg int Screen - the used screen.
  */
/****************************************************************************/
int findQuitKey (Display * Dpy, int Screen)
{

	XEvent Event;
	XKeyEvent EKey;
	Window Target, Root;
	int Loop = true;
	int Error;

	/* get the root window and set default target */
	Root = RootWindow (Dpy, Screen);
	Target = None;

	/* grab the keyboard */
	Error =
		XGrabKeyboard (Dpy, Root, False, GrabModeSync, GrabModeAsync,
				CurrentTime);

	/* did we succeed in grabbing the keyboard?*/
	if (Error != GrabSuccess)
	{
		/* nope, abort */
		fprintf (stderr, "Could not grab the keyboard, aborting.");
		exit (EXIT_FAILURE);
	}

	/* print a message to the user informing about what's going on */
	fprintf (stderr,"Press the key you want to use to end the application. \n"
			"This key can be any key, \n"
			"as long as you don't need it while working with the remote display.\n"
			"A good choice is Escape.\n\n");

	/* let the user select a window... */
	while (Loop)
	{
		/* allow one more event */
		XAllowEvents (Dpy, SyncPointer, CurrentTime);
		XWindowEvent (Dpy, Root, KeyPressMask, &Event);

		/* what did we get? */
		if (Event.type == KeyPress)
		{
			/* a key was pressed, don't loop more */
			EKey = Event.xkey;
			Loop = false;
		}
	}

	/* we're done with pointer and keyboard */
	XUngrabPointer (Dpy, CurrentTime);
	XUngrabKeyboard (Dpy, CurrentTime);

	/* show the user what was chosen */
	fprintf (stderr, "The chosen quit-key has the keycode: %i\n", EKey.keycode);
	/* return the found key */
	return EKey.keycode;
}


/****************************************************************************/
/*! Scales the passed coordinate with the given saling factor. the factor is
  either given as a commandline argument or it is 1.0.
  */
/****************************************************************************/
int scale (const int Coordinate)
{

	/* perform the scaling, all in one ugly line */
	return (int) ((float) Coordinate * Scale);
}

void eventCallback (XPointer priv, XRecordInterceptData * d)
{
	Priv *p = (Priv *) priv;
	unsigned int *ud4, tstamp, wroot, wevent, wchild, type, detail;
	unsigned char *ud1, type1, detail1, samescreen;
	unsigned short *ud2, seq;
	short *d2, rootx, rooty, eventx, eventy, kstate;

	if (d->category == XRecordStartOfData)
		fprintf (stderr, "Got Start Of Data\n");
	if (d->category == XRecordEndOfData)
		fprintf (stderr, "Got End Of Data\n");
	if (d->category != XRecordFromServer || p->doit == 0)
	{
		fprintf (stderr, "Skipping...\n");
		goto returning;
	}
	if (d->client_swapped == True)
		fprintf (stderr, "Client is swapped!!!\n");
	ud1 = (unsigned char *) d->data;
	ud2 = (unsigned short *) d->data;
	d2 = (short *) d->data;
	ud4 = (unsigned int *) d->data;

	type1 = ud1[0] & 0x7F;
	type = type1;
	detail1 = ud1[1];
	detail = detail1;
	seq = ud2[1];
	tstamp = ud4[1];
	wroot = ud4[2];
	wevent = ud4[3];
	wchild = ud4[4];
	rootx = d2[10];
	rooty = d2[11];
	eventx = d2[12];
	eventy = d2[13];
	kstate = d2[14];
	samescreen = ud1[30];

	if (p->Status1)
	{
		p->Status1--;
		if (type == KeyRelease)
		{
			fprintf (stderr, "- Skipping stale KeyRelease event. %i\n",
					p->Status1);
			goto returning;
		}
		else
			p->Status1 = 0;
	}
	if (p->x == -1 && p->y == -1 && p->mmoved == 0 && type != MotionNotify)
	{
		fprintf (stderr,
				"- Please move the mouse before any other event to synchronize pointer\n");
		fprintf (stderr, "  coordinates! This event is now ignored!\n");
		goto returning;
	}

	/* what did we get? */
	switch (type)
	{
		case ButtonPress:
			if (p->mmoved)
			{
				if (last_time != 0)
				{
					fprintf (fdout, "Delay %i\n",(int) (d->server_time - last_time));
				}
				fprintf (fdout, MOTION_NOTIFY" %i %i\n", p->x, p->y);
				p->mmoved = 0;
				last_time = d->server_time;
			}
			if (p->Status2 < 0)
				p->Status2 = 0;
			p->Status2++;
			fprintf (fdout, BUTTON_PRESS" %i\n", detail);
			break;

		case ButtonRelease:
			/* button released, create event */
			if (last_time != 0)
			{
				fprintf (fdout, "Delay %i\n",(int) (d->server_time - last_time));
			}
			last_time = d->server_time;
			if (p->mmoved)
			{
				fprintf (fdout, MOTION_NOTIFY" %i %i\n", p->x, p->y);
				p->mmoved = 0;
			}
			p->Status2--;
			if (p->Status2 < 0)
				p->Status2 = 0;
			fprintf (fdout, BUTTON_RELEASE" %i\n", detail);
			break;

		case MotionNotify:
			/* motion-event, create event */
			if (p->Status2 > 0)
			{
				fprintf (fdout, MOTION_NOTIFY" %i %i\n", rootx, rooty);
				p->mmoved = 0;
			}
			else
				p->mmoved = 1;
			p->x = rootx;
			p->y = rooty;
			break;

		case KeyPress:
			/* a key was pressed */
			/* should we stop looping, i.e. did the user press the quitkey? */
			if (HasQuitKey && detail == QuitKey)
			{
				/* yep, no more loops */
				fprintf (stderr, "Got QuitKey, so exiting...\n");
				p->doit = 0;
			}
			else if (HasScreenshotKey && detail == ScreenshotKey) 
			{
				fprintf(fdout, "Screenshot\n");
			}
			else
			{
				/* send the keycode to the remote server */
				if (p->mmoved)
				{
					fprintf (fdout, MOTION_NOTIFY" %i %i\n", p->x, p->y);
					p->mmoved = 0;
				}
				if (last_time != 0)
				{
					fprintf (fdout, DELAY" %i\n", (int)(d->server_time - last_time));
				}
				fprintf (fdout, KEY_STR_PRESS" %s\n", XKeysymToString (XKeycodeToKeysym(p->LocalDpy, detail, 0)));
				last_time = d->server_time;
			}
			break;

		case KeyRelease:
			/* a key was released */
			if (HasScreenshotKey && detail == ScreenshotKey) 
			{
				break;
			}
			if (p->mmoved)
			{
				fprintf (fdout, MOTION_NOTIFY" %i %i\n", p->x, p->y);
				p->mmoved = 0;
			}
			fprintf (fdout, KEY_STR_RELEASE" %s\n",
					XKeysymToString (XKeycodeToKeysym (p->LocalDpy, detail, 0)));
			break;
	}
returning:
	XRecordFreeData (d);

}

/****************************************************************************/
/*! Main event-loop of the application. Loops until a key with the keycode
  \a QuitKey is pressed. Sends all mouse- and key-events to the remote
  display.

  \arg Display * LocalDpy - used display.
  \arg int LocalScreen - the used screen.
  \arg Display * RecDpy - used display.
  \arg unsigned int QuitKey - the key when pressed that quits the eventloop.
  */
/****************************************************************************/
void eventLoop (Display * LocalDpy, int LocalScreen, Display * RecDpy, unsigned int QuitKey)
{
	Window Root, rRoot, rChild;
	XRecordContext rc;
	XRecordRange *rr;
	XRecordClientSpec rcs;
	Priv priv;
	int rootx, rooty, winx, winy;
	unsigned int mmask;
	Bool ret;
	Status sret;

	/* get the root window and set default target */
	Root = RootWindow (LocalDpy, LocalScreen);

	ret =
		XQueryPointer (LocalDpy, Root, &rRoot, &rChild, &rootx, &rooty, &winx,
				&winy, &mmask);
	fprintf (stderr, "XQueryPointer returned: %i\n", ret);
	rr = XRecordAllocRange ();
	if (!rr)
	{
		fprintf (stderr, "Could not alloc record range, aborting.\n");
		exit (EXIT_FAILURE);
	}
	rr->device_events.first = KeyPress;
	rr->device_events.last = MotionNotify;
	rcs = XRecordAllClients;
	rc = XRecordCreateContext (RecDpy, 0, &rcs, 1, &rr, 1);
	if (!rc)
	{
		fprintf (stderr, "Could not create a record context, aborting.\n");
		exit (EXIT_FAILURE);
	}
	priv.x = rootx;
	priv.y = rooty;
	priv.mmoved = 1;
	priv.Status2 = 0;
	priv.Status1 = 2;
	priv.doit = 1;
	priv.LocalDpy = LocalDpy;
	priv.RecDpy = RecDpy;
	priv.rc = rc;

	if (!XRecordEnableContextAsync
			(RecDpy, rc, eventCallback, (XPointer) & priv))
	{
		fprintf (stderr, "Could not enable the record context, aborting.\n");
		exit (EXIT_FAILURE);
	}

	while (priv.doit)
		XRecordProcessReplies (RecDpy);

	sret = XRecordDisableContext (LocalDpy, rc);
	if (!sret)
		fprintf (stderr, "XRecordDisableContext failed!\n");
	sret = XRecordFreeContext (LocalDpy, rc);
	if (!sret)
		fprintf (stderr, "XRecordFreeContext failed!\n");
	XFree (rr);
}


/****************************************************************************/
/*! Main function of the application. It expects no commandline arguments.

  \arg int argc - number of commandline arguments.
  \arg char * argv[] - vector of the commandline argument strings.
  */
/****************************************************************************/
int main (int argc, char *argv[])
{
	char *filename;
	int Major, Minor;
	cfg = cfg_open("xmacro.ini");
	/* Load some defaults */
	Scale = cfg_get_single_value_as_float_with_default(cfg, "Record", "scale",1.0);
	HasQuitKey = cfg_get_single_value_as_int_with_default(cfg, "Record", "hasQuitKey",false);
	/* Get quit key, default is F8*/
	QuitKey = cfg_get_single_value_as_int_with_default(cfg, "Record", "QuitKey",74);
	HasScreenshotKey = cfg_get_single_value_as_int_with_default(cfg, "Record", "hasScreenshotKey",false);
	/* get screenshot key, default is F6 */
	ScreenshotKey = cfg_get_single_value_as_int_with_default(cfg, "Record", "ScreenshotKey",72);	
	filename = cfg_get_single_value_as_string(cfg, "Record", "OutputFile");
	if(filename)
	{
		fdout = fopen(filename,"w");
		fprintf(stderr,"Outputfile: %s\n", filename);
		if(fdout == NULL)
		{
			fprintf(stderr, "Error opening file: %s:%s\n", filename, strerror(errno));		
			exit(EXIT_FAILURE);
		}
		free(filename);
	}                                                                                               	


	/* parse commandline arguments */
	parseCommandLine (argc, argv);
	/* set default output to stdout */
	if(fdout == NULL) {
		fdout = stdout;                   	
	}
	/* open the local display twice */
	Display *LocalDpy = localDisplay ();
	Display *RecDpy = localDisplay ();

	/* get the screens too */
	int LocalScreen = DefaultScreen (LocalDpy);

	fprintf (stderr, "Server VendorRelease: %i\n", VendorRelease (RecDpy));
	/* does the remote display have the Xrecord-extension? */
	if (!XRecordQueryVersion (RecDpy, &Major, &Minor))
	{
		/* nope, extension not supported */
		fprintf (stderr, "%s: XRecord extension not supported on server \"%s\"\n", PROG, DisplayString (RecDpy));

		/* close the display and go away */
		XCloseDisplay (RecDpy);
		exit (EXIT_FAILURE);
	}

	/* print some information */

	fprintf (stderr, "XRecord for server \"%s\" is version %i.%i.\n",
			DisplayString (RecDpy), Major, Minor);
	/* do we already have a quit key? If one was supplied as a commandline */
	if (!HasQuitKey)
	{
		/* nope, so find the key that quits the application */
		QuitKey = findQuitKey (LocalDpy, LocalScreen);
		HasQuitKey = true;
	}
	else
	{
		/* show the user which key will be used */
		fprintf (stderr, "The used quit-key has the keycode: %i\n", QuitKey);
	}

	/* start the main event loop */
	eventLoop (LocalDpy, LocalScreen, RecDpy, QuitKey);

	/* we're done with the display */
	/*  XCloseDisplay ( RecDpy ); */
	XCloseDisplay (LocalDpy);

	fprintf (stderr, "%s:  Exiting. \n", PROG);
	if(fdout != stdout)
	{
		fclose(fdout);
	}
	/* go away */
	exit (EXIT_SUCCESS);
}
