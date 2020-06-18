#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/X.h>
#include <X11/Xproto.h>
#include <X11/extensions/secur.h>

#include <scrnintstr.h>
#include <windowstr.h>
#include <misc.h>
#include <extnsionst.h>
#include <gcstruct.h>
#include <privates.h>
#include <registry.h>
#include <xace.h>
#include <xacestr.h>
#include <xf86.h>
#include <resource.h>
#include <inputstr.h>
#include <dixstruct.h>

#include <systemd/sd-journal.h>
#include <json-c/json.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <time.h>
#include <dbus/dbus.h>


#define XORG_VERSION_CURRENT (((1) * 10000000) + ((19) * 100000) + ((2) * 1000) + 0)
#define SECURITY_AUDIT_LEVEL 4

#define POLICY_BUF_SIZE	1024
#define DEFAULT_POLICY_PATH "/etc/xsm/default.rules"

#define XSM_ALLOW	1
#define XSM_DISALLOW 0

#define XSM_SCREENSHOT	0
#define XSM_SCREENCAST	1
#define XSM_XRECORD		3
#define XSM_CLIPBOARD	4

#define LOG_ID "XSM-LOG"

#define LOG_SCREENSHOT	0
#define LOG_SCREENCAST	1
#define LOG_XRECORD		3
#define LOG_CLIPBOARD	4

#define LOG_SCREENSHOT_BODY	"Screenshot restricted.\n"
#define LOG_SCREENCAST_BODY	"Screencast restricted.\n"
#define LOG_XRECORD_BODY	"Xsession record or replay restricted.\n"
#define LOG_CLIPBOARD_BODY	"Clipboard restricted.\n"
#define LOG_UNKNOWN_BODY	"Unknown action restricted.\n"

#define NOTIFY_MSG_SCR	"screenshot and screencast detected"
#define NOTIFY_MSG_CLP	"clipboard detected"

#define INOTIFY_MAX_EVENTS	1024 /* Max. number of events to process at one go */
#define INOTIFY_LEN_NAME 	16 /* Assuming that the length of the filename won't exceed 16 bytes */
#define INOTIFY_EVENT_SIZE  ( sizeof (struct inotify_event) ) /*size of one event */
#define INOTIFY_BUF_LEN     ( INOTIFY_MAX_EVENTS * ( INOTIFY_EVENT_SIZE + INOTIFY_LEN_NAME )) /* buffer to store the data of events */

#define POLICY_DIR_PATH		"/etc/xsm/"
#define POLICY_SCRS_ATTR	"screenshot"
#define POLICY_SCRT_ATTR	"screencast"
#define POLICY_XRER_ATTR	"xrecord"
#define POLICY_CLIP_ATTR	"clipboard"

#define POLICY_ALLOW_STR	"allow"
#define POLICY_DISALLOW_STR	"disallow"

#define PID_SAVE_TIMEOUT	5


static MODULESETUPPROTO(XsmSetup);

Bool noXsmExtension = FALSE;

/* Extension stuff */
static int XsmErrorBase;   /* first Security error number */                                   
static int XsmEventBase;   /* first Security event number */


//CallbackListPtr ClientStateCallback;

/* This structure is expected to be returned by the initfunc */
// typedef struct {
//   const char *modname;        /* name of module, e.g. "foo" */
// 	 const char *vendor;         /* vendor specific string */
//	 CARD32 _modinfo1_;          /* constant MODINFOSTRING1/2 to find */
//	 CARD32 _modinfo2_;          /* infoarea with a binary editor or sign tool */
//	 CARD32 xf86version;         /* contains XF86_VERSION_CURRENT */
//	 CARD8 majorversion;         /* module-specific major version */
//	 CARD8 minorversion;         /* module-specific minor version */
//	 CARD16 patchlevel;          /* module-specific patch level */
//	 const char *abiclass;       /* ABI class that the module uses */
//	 CARD32 abiversion;          /* ABI version */
//	 const char *moduleclass;    /* module class description */
//	 CARD32 checksum[4];         /* contains a digital signature of the */
	 /* version info structure */
//} XF86ModuleVersionInfo;   


static XF86ModuleVersionInfo VersRec = { 
	"Xsm",
	"ultract@nsr.re.kr",	/* MODULEVENDORSTRING */
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	1, 0, 0,
	ABI_CLASS_EXTENSION,
	ABI_EXTENSION_VERSION,                                                                               
	MOD_CLASS_NONE,
	{0, 0, 0, 0}
};


/*
 * Access modes
 */

static const Mask XsmResourceMask = 
	DixGetAttrAccess | DixReceiveAccess | DixListPropAccess |
	DixGetPropAccess | DixListAccess;

static const Mask XsmWindowExtraMask = DixRemoveAccess;

static const Mask XsmRootWindowExtraMask =
    DixReceiveAccess | DixSendAccess | DixAddAccess | DixRemoveAccess;


_X_EXPORT XF86ModuleData xsmModuleData = { &VersRec, XsmSetup, NULL };

extern void XsmExtensionInit(void); 

static const ExtensionModule XsmExt[] = {
{ XsmExtensionInit, "Xsm", &noXsmExtension },
};


static _X_INLINE const char *
XsmLookupRequestName(ClientPtr client)
{
	return LookupRequestName(client->majorOp, client->minorOp);
}


static void
_X_ATTRIBUTE_PRINTF(1, 2)
XsmAudit(const char *format, ...)
{
	va_list args;

	if (auditTrailLevel < SECURITY_AUDIT_LEVEL)
		return;
	va_start(args, format);
	VAuditF(format, args);
	va_end(args);
}


void dbus_notify_signal(const char *noti_msg)
{

	DBusError err;
	dbus_error_init(&err);

	DBusConnection* conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (conn == NULL || dbus_error_is_set(&err)) { 
		dbus_error_free(&err); 
		exit(1); 
	}

	DBusMessage* dbus_msg = dbus_message_new_signal(
                            "dbus path",
                            "dbus interface", 
                            "signal name"); 
	if (NULL == dbus_msg) { 
		exit(1); 
	}

    dbus_message_append_args(dbus_msg, DBUS_TYPE_STRING, &noti_msg, DBUS_TYPE_INVALID);

	dbus_uint32_t serial = 0;
	if (!dbus_connection_send(conn, dbus_msg, &serial)) {
		exit(1);
	}
	printf("sent (serial=%d)\n", serial);
	dbus_message_unref(dbus_msg);
}



/* LOG_EMERG	0	system is unusable */
/* ALERT		1	action must be taken immediately */
/* LOG_CRIT		2	critical conditions */
/* LOG_ERR		3	error conditions */
/* LOG_WARNING	4	warning conditions */
/* LOG_NOTICE	5	normal but significant condition */
/* LOG_INFO		6	informational */
/* LOG_DEBUG	7	debug-level messages */

static void write_journal_log(int priority, const char *logmsg, const char *custom_fields)
{
	/*
	openlog(LOG_ID, LOG_NDELAY, LOG_DAEMON);
	syslog(priority,logmsg);
	closelog();
	*/
	sd_journal_send("SYSLOG_IDENTIFIER=%s", LOG_ID,
					"PRIORITY=%d",priority,
					"MESSAGE=%s", logmsg,
					NULL);
}

/* 
 *	Default rule set -> disallow 
 */
static int screenshot_allow = XSM_ALLOW;
static int screencast_allow = XSM_ALLOW;
static int xrecord_allow = XSM_ALLOW;
static int clipboard_allow = XSM_ALLOW;

static int policy_check(int idx)
{
	if(idx == XSM_SCREENSHOT)
		return screenshot_allow;
	if(idx == XSM_SCREENCAST)
		return screencast_allow;
	if(idx == XSM_XRECORD)
		return xrecord_allow;
	if(idx == XSM_CLIPBOARD)
		return clipboard_allow;
}


/*
 *	Read policy
 *	- User policy has higher priority than the default policy.
 */
static void read_policy(void)
{
	FILE *fp;
	char policy_buf[POLICY_BUF_SIZE];
	struct json_object *parsed_json;
	struct json_object *clipboard;
	struct json_object *screenshot;
	struct json_object *screencast;
	struct json_object *xrecord;
	const char *screencapt_policy, *clipboard_policy;
	const char *screenshot_policy, *screencast_policy, *xrecord_policy;

	fp = fopen(DEFAULT_POLICY_PATH, "r");
	if(fp == NULL)
	{
		write_journal_log(LOG_WARNING, "Default-policy file: Not exist!!", "");
		write_journal_log(LOG_WARNING, "Screen-capture & clipboard: No restrict.", "");
		screenshot_allow = XSM_ALLOW;
		screencast_allow = XSM_ALLOW;
		xrecord_allow = XSM_ALLOW;
		clipboard_allow = XSM_ALLOW;
		return;
	}
	else
		write_journal_log(LOG_NOTICE, "Default-policy file: Loaded", "");
	
	fread(policy_buf, POLICY_BUF_SIZE, 1, fp);
	fclose(fp);

	/* Parse json policy */
	parsed_json = json_tokener_parse(policy_buf);
	if(parsed_json == NULL)
	{
		write_journal_log(LOG_WARNING, "Policy-file: Json parsing error!", "");
		return;	
	}

	/* Get object in json policy */
	json_object_object_get_ex(parsed_json, POLICY_SCRS_ATTR, &screenshot);
	json_object_object_get_ex(parsed_json, POLICY_SCRT_ATTR, &screencast);
	json_object_object_get_ex(parsed_json, POLICY_XRER_ATTR, &xrecord);
	json_object_object_get_ex(parsed_json, POLICY_CLIP_ATTR, &clipboard);

	/* Json object to string */
	screenshot_policy = json_object_get_string(screenshot);
	screencast_policy = json_object_get_string(screencast);
	xrecord_policy = json_object_get_string(xrecord);
	clipboard_policy = json_object_get_string(clipboard);

	/* Set screen capture policy e.g. screenshot,screencast, xrecord&replay */
	if(screenshot_policy) 
	{
		if(!strcmp(screenshot_policy, POLICY_ALLOW_STR))
		{
			screenshot_allow = XSM_ALLOW;
			write_journal_log(LOG_NOTICE, "Screenshot: Allow", "");
		}
		else if(!strcmp(screenshot_policy, POLICY_DISALLOW_STR))
		{
			screenshot_allow = XSM_DISALLOW;
			write_journal_log(LOG_NOTICE, "Screen-capture: Disallow", "");
		}
	}
	else
	{
		screenshot_allow = XSM_ALLOW;
		write_journal_log(LOG_WARNING, "Policy-file: Screenshot rule not exist!", "");
		write_journal_log(LOG_WARNING, "Screenshot: No restrict", "");
	}

	if(screencast_policy)
	{
		if(!strcmp(screencast_policy, POLICY_ALLOW_STR))
		{
			screencast_allow = XSM_ALLOW;
			write_journal_log(LOG_NOTICE, "Screencast: Allow", "");
		}
		else if(!strcmp(screencast_policy, POLICY_DISALLOW_STR))
		{
			screencast_allow = XSM_DISALLOW;
			write_journal_log(LOG_NOTICE, "Screencast: Disallow", "");
		}
	}
	else
	{
		screencast_allow = XSM_ALLOW;
		write_journal_log(LOG_WARNING, "Policy-file: Screencast rule not exist!", "");
		write_journal_log(LOG_WARNING, "Screencast: No restrict", "");
	}

	if(xrecord_policy != NULL)
	{
		if(!strcmp(xrecord_policy, POLICY_ALLOW_STR))
		{
			xrecord_allow = XSM_ALLOW;
			write_journal_log(LOG_NOTICE, "Xrecord: Allow", "");
		}
		else if(!strcmp(xrecord_policy, POLICY_DISALLOW_STR))
		{
			xrecord_allow = XSM_DISALLOW;
			write_journal_log(LOG_NOTICE, "Xrecord: Disallow", "");
		}
	}
	else
	{
		xrecord_allow = XSM_ALLOW;
		write_journal_log(LOG_WARNING, "Policy-file: Xrecord rule not exist!", "");
		write_journal_log(LOG_WARNING, "Xrecord: No restrict", "");
	}
	
	if(clipboard_policy != NULL) /* Set clipboard policy */
	{
		if(!strcmp(clipboard_policy, POLICY_ALLOW_STR))
		{
			clipboard_allow = XSM_ALLOW;
			write_journal_log(LOG_NOTICE, "Clipboard: Allow", "");
		}
		else if(!strcmp(clipboard_policy, POLICY_DISALLOW_STR))
		{
			clipboard_allow = XSM_DISALLOW;
			write_journal_log(LOG_NOTICE, "Clipboard: Disallow", "");
		}
	}
	else
	{
		clipboard_allow = XSM_ALLOW;
		write_journal_log(LOG_WARNING, "Policy-file: Clibboard value not exist!", "");
		write_journal_log(LOG_WARNING, "Clipboard: No restrict", "");
	}
}


/*
 * pthread for reading policy file
 *
 */

static pthread_t inotify_pthread;
static int inotify_fd; /* inotify file descriptor */

static void *inotify_policy_thread(void *param)
{
	int length, i = 0, wd;
	char buffer[INOTIFY_BUF_LEN];

	while(1)
	{
		i = 0;
		length = read(inotify_fd, buffer, INOTIFY_BUF_LEN );  
		
		if (length < 0)	/* If this occur then check file descriptor !! */
			LogMessage(X_INFO, "inotify_policy_thread : inotify policy read error!\n");

		while (i < length) {
			struct inotify_event *event = ( struct inotify_event * ) &buffer[i];
			
			//LogMessage(X_INFO, "inotify_policy_thread : event->len (%d)\n", event->len);
			if ( event->len ) {
				if (event->mask & IN_CREATE || event->mask & IN_MODIFY || event->mask & IN_DELETE ||
					event->mask & IN_MOVED_FROM || event->mask & IN_MOVED_TO) {
					
					if(event->name == NULL)
						continue;
					/*
					LogMessage(X_INFO, "inotify_policy_thread : event->mask & IN_CREATE "
										"event->name : %s \n", event->name); 
					*/

					/* "event->name" monitored file name via inotify */
					if(!strcmp(event->name, "user.rules") || !strcmp(event->name, "default.rules"))
						read_policy();
						//printf( "The file %s was Created with WD %d\n", event->name, event->wd );
				}
				 
				i += INOTIFY_EVENT_SIZE + event->len;
			}
		}
	}
}

/*
 * Read policy file via inotify event
 *
 */
void inotify_policy(void)
{
	int length, i = 0, wd;
	char buffer[INOTIFY_BUF_LEN];
	int ret;

	/* Initialize Inotify */
	inotify_fd = inotify_init();
	if (inotify_fd < 0)
		LogMessage(X_INFO, "inotify_policy : Inotify policy load failed!\n");

	LogMessage(X_INFO, "inotify_policy : called\n");

	/* add watch to starting directory */
	wd = inotify_add_watch(inotify_fd, POLICY_DIR_PATH, 
				IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO); 

	if (wd == -1)
		LogMessage(X_INFO, "inotify_policy : Couldn't add watch to %s\n",POLICY_DIR_PATH);
	else
		LogMessage(X_INFO, "inotify_policy : Watching policy dir: %s\n", POLICY_DIR_PATH);

	/* Run thread */
	ret = pthread_create(&inotify_pthread, NULL, inotify_policy_thread, &inotify_fd);
	pthread_detach(inotify_pthread);
}


/* Initialize pid */
static int screenshot_pid = 0;
static int screencast_pid = 0;
static int xrecord_pid = 0;
static int clipboard_pid = 0;

/* Timer variables */
static int timer = 0;
struct timespec before, after;
long elapsed_secs = 0;

static void renew_pid()
{
	screenshot_pid = 0;
	screencast_pid = 0;
	xrecord_pid = 0;
	clipboard_pid = 0;
}

static void make_log(int idx, pid_t cmdpid)
{
	if(timer == 0)
	{
		clock_gettime(CLOCK_REALTIME, &before);
		timer = 1;
	}

	if(idx == LOG_SCREENSHOT && cmdpid != screenshot_pid)
	{
		screenshot_pid = cmdpid;
		//dbus_notify_signal(NOTIFY_MSG_SCR);
		write_journal_log(LOG_CRIT, LOG_SCREENSHOT_BODY, "");
		LogMessage(X_INFO, LOG_SCREENSHOT_BODY);
	}
	else if(idx == LOG_SCREENCAST && cmdpid != screencast_pid)
	{
		screencast_pid = cmdpid;
		//dbus_notify_signal(NOTIFY_MSG_SCR);
		write_journal_log(LOG_CRIT, LOG_SCREENCAST_BODY, "");
		LogMessage(X_INFO, LOG_SCREENCAST_BODY);
	}
	else if(idx == LOG_XRECORD && cmdpid != xrecord_pid)
	{
		xrecord_pid = cmdpid;
		//dbus_notify_signal(NOTIFY_MSG_SCR);
		write_journal_log(LOG_CRIT, LOG_XRECORD_BODY, "");
		LogMessage(X_INFO, LOG_XRECORD_BODY);
	}
	else if(idx == LOG_CLIPBOARD && cmdpid != clipboard_pid)
	{
		clipboard_pid = cmdpid;
		//dbus_notify_signal(NOTIFY_MSG_CLP);
		write_journal_log(LOG_CRIT, LOG_CLIPBOARD_BODY, "");
		LogMessage(X_INFO, LOG_CLIPBOARD_BODY);
	}
	
	clock_gettime(CLOCK_REALTIME, &after);
	elapsed_secs = after.tv_sec - before.tv_sec;
	if(elapsed_secs > PID_SAVE_TIMEOUT)
	{
		renew_pid();
		timer = 0;
	}
}


/*
 * Check whitelist of application
 *
 */
static int is_whitelist(const char *cmdname)
{
	if(!strcmp(cmdname, "/usr/bin/gnome-shell"))
		return 1;
	else if(!strcmp(cmdname, "xfce4-session"))
		return 1;
	else if(!strcmp(cmdname, "cinnamon"))
		return 1;
	else if(!strcmp(cmdname, "/usr/lib/at-spi2-core/at-spi2-registryd"))
		return 1;
	else if(!strcmp(cmdname, "/usr/lib/vmware-tools/sbin64/vmtoolsd"))
		return 1;

	return 0;
}


static void *
XsmSetup(void *module, void *opts, int *errmaj, int *errmin)
{
	LoadExtensionList(XsmExt, ARRAY_SIZE(XsmExt), FALSE);
	LogMessage(X_INFO, "XsmSetup Called\n");
	return module;
}


static void 
XsmResource(CallbackListPtr *pcbl, void *unused, void *calldata){
	XaceResourceAccessRec *rec = calldata;
	int cid = CLIENT_ID(rec->id);
	Mask requested = rec->access_mode;
	Mask allowed = XsmResourceMask;
	const char *requestName, *resourceName;
	const char *cmdname, *cmdargs;
	pid_t cmdpid;


	requestName = XsmLookupRequestName(rec->client);
	resourceName = LookupResourceName(rec->rtype);

	cmdname = GetClientCmdName(rec->client);
	cmdargs = GetClientCmdArgs(rec->client);
	cmdpid = GetClientPid(rec->client);

	if(requestName == NULL || cmdname == NULL)
		return;

	/* Check APIs of DixReadAccess */
	if(requested & DixReadAccess)
	{
		/* Restrict Screencast e.g. recordmydesktop, kazam, scrot? */
		if(!strcmp(requestName, "MIT-SHM:GetImage") && rec->rtype == RT_WINDOW){

			XsmAudit("XsmResource: client(%d) access(%lx) to resource(0x%lx) "
				"of client(%d) on request(%s) resource(%s) "
				"cmdname: %s(%d) args: %s\n", 
				rec->client->index,	(unsigned long)requested, 
				(unsigned long)rec->id, cid, requestName, resourceName,
				cmdname, cmdpid, cmdargs);
			
			//if(!policy_check(XSM_SCREENSHOT) && !is_whitelist(cmdname))
			if(!policy_check(XSM_SCREENCAST) && !is_whitelist(cmdname))
			{
				make_log(LOG_SCREENCAST, cmdpid);
				rec->status = BadAccess;
			}
			return;
		}

		/* Restrict Screenshot via XGetImage (WINDOW) e.g. gtk2, gtk3, */
		if(rec->rtype == RT_WINDOW && !strcmp(requestName, "X11:GetImage"))
		{
			XsmAudit("XsmResource: client(%d) access(%lx) to resource(0x%lx) "
				"of client(%d) on request(%s) resource(%s) "
				"cmdname: %s(%d) args: %s\n", 
				rec->client->index,	(unsigned long)requested, 
				(unsigned long)rec->id, cid, requestName, resourceName,
				cmdname, cmdpid, cmdargs);
			
			if(!policy_check(XSM_SCREENSHOT))
			{
				make_log(LOG_SCREENSHOT, cmdpid);
				rec->status = BadAccess;
			}
			return;
		}
		
		/* Restrict Screenshot via XGetImage (PIXMAP) e.g. screencloud, gnome-shell */
		if(rec->rtype == RT_PIXMAP && !strcmp(requestName, "X11:GetImage"))
		{
			XsmAudit("XsmResource: client(%d) access(%lx) to resource(0x%lx) "
				"of client(%d) on request(%s) resource(%s,%p) "
				"cmdname: %s(%d) args: %s\n", 
				rec->client->index,	(unsigned long)requested, 
				(unsigned long)rec->id, cid, requestName, resourceName, rec->rtype,
				cmdname, cmdpid, cmdargs);

			return;
		}


		/* Restrict Screenshot via gtk libraries and so on... */
		if((rec->rtype == RT_WINDOW) && !strcmp(requestName, "X11:CopyArea"))
		{
			XsmAudit("XsmResource: client(%d) access(%lx) to resource(0x%lx) "
				"of client(%d) on request(%s) resource(%s,%p) "
				"cmdname: %s(%d) args: %s\n", 
				rec->client->index,	(unsigned long)requested, 
				(unsigned long)rec->id, cid, requestName, resourceName, rec->rtype,
				cmdname, cmdpid, cmdargs);
			
			if(!policy_check(XSM_SCREENSHOT) && !is_whitelist(cmdname))
			{
				make_log(LOG_SCREENSHOT, cmdpid);
				rec->status = BadAccess;
			}
			return;
		}
	}
}

/*
 * Control Xrecord and Xreplay
 *
 */

static void 
XsmExtension(CallbackListPtr *pcbl, void *unused, void *calldata)
{
	XaceExtAccessRec *rec = calldata;
	int i = 0;
	const char *requestName;
	const char *cmdname, *cmdargs;
	pid_t cmdpid;

	requestName = XsmLookupRequestName(rec->client);
	cmdname = GetClientCmdName(rec->client);
	cmdargs = GetClientCmdArgs(rec->client);
	cmdpid = GetClientPid(rec->client);

	if(requestName == NULL || cmdname == NULL)
		return;

	if(!strncmp(requestName, "RECORD:", 7)) {
		XsmAudit("XsmExtension: client %d access to extension "
		 		 "%s on request %s cmdname %s(%d) args %s\n",
				 rec->client->index, rec->ext->name,
				 requestName,
				 cmdname, cmdpid, cmdargs);
	}

	/* Restrict Xsession Record & Replay */
	if((!strcmp(requestName, "XTEST:GrabControl") || 
		!strcmp(requestName, "XTEST:FakeInput") ||
		!strcmp(requestName, "RECORD:CreateContext") || 
		!strcmp(requestName, "RECORD:EnableContext"))){

		XsmAudit("XsmExtension: client %d access to extension "
				"%s on request %s cmdname %s(%d) args %s\n",
				rec->client->index, rec->ext->name,
				requestName,
				cmdname, cmdpid, cmdargs);
		
		if(!policy_check(XSM_XRECORD) && !is_whitelist(cmdname))
		{
			make_log(LOG_XRECORD, cmdpid);
			rec->status = BadAccess;
		}
		return;
	}
	/* Restrict Screencast (Cursor Image) e.g. recordmydesktop, kazam */
	else if(!strcmp(requestName, "XFIXES:GetCursorImageAndName"))
	{
		XsmAudit("XsmExtension: client %d access to extension "
				"%s on request %s cmdname %s(%d) args %s\n",
				rec->client->index, rec->ext->name,
				requestName,
				cmdname, cmdpid, cmdargs);
		
		if(!policy_check(XSM_SCREENCAST) && !is_whitelist(cmdname))
		{
			make_log(LOG_SCREENCAST, cmdpid);
			rec->status = BadAccess;
		}
		return;
	}

}


/*
 * Control clipboard behaviors
 * 
 */

/*
 
"include/selection.h" 

typedef struct _Selection {
	Atom selection;
	TimeStamp lastTimeChanged;
	Window window;
	WindowPtr pWin;
	ClientPtr client;
	struct _Selection *next;
	PrivateRec *devPrivates;
} Selection;
*/

static void
XsmSelection(CallbackListPtr *pcbl, void *unused, void *calldata)
{
	XaceSelectionAccessRec *rec = calldata;
	Selection *pSel = *rec->ppSel;
	Atom name = pSel->selection;
	Mask access_mode = rec->access_mode;
	const char *cmdname, *cmdargs;
	pid_t cmdpid;
	const char *atomname, *requestName;

	cmdname = GetClientCmdName(rec->client);
	cmdargs = GetClientCmdArgs(rec->client);
	cmdpid = GetClientPid(rec->client);

	requestName = XsmLookupRequestName(rec->client);
	atomname = NameForAtom(name);

	if(cmdname == NULL || atomname == NULL || requestName == NULL)
		return;

	if(!strcmp(atomname, "CLIPBOARD") && (pSel->window == 0x0))
	{
		/*
		XsmAudit("XsmSelection: client %d access to server configuration request %s "
				"atom %s(%p) window(%p)"
				"cmdname %s(%d) args %s\n", 
				rec->client->index, requestName,
				NameForAtom(name), name, pSel->window,
				cmdname, cmdpid, cmdargs);
		*/
		
		/* Kill clipboard application */
		if(!strcmp(cmdname, "clipit") || !strcmp(cmdname, "xclip"))
		{
			XsmAudit("XsmSelection: client %d cmdname %s(%d) killed\n"
					,rec->client->index, cmdname, cmdpid);

			rec->status = BadAccess;
			kill(cmdpid, SIGKILL);
		}
	}
	
	/* Restrict Clipboard on Window (GUI Client) */
	if(!strcmp(atomname, "CLIPBOARD") && !strcmp(requestName, "X11:GetSelectionOwner") &&
		(pSel->window != 0x0)){

		XsmAudit("XsmSelection: client %d access to server configuration request %s "
				"atom %s(%p) window(%p) "
				"cmdname %s(%d) args %s\n", 
				rec->client->index, requestName,
				NameForAtom(name), name, pSel->window,
				cmdname, cmdpid, cmdargs);

		if(!policy_check(XSM_CLIPBOARD))
		{
			make_log(LOG_CLIPBOARD, cmdpid);
		
			/* Delete Any Selections */
			DeleteWindowFromAnySelections(pSel->pWin);
			DeleteClientFromAnySelections(rec->client);
			
			/* Kill clipboard application */
			if(!strcmp(cmdname, "clipit") || !strcmp(cmdname, "xclip"))
			{
				XsmAudit("XsmSelection: client %d cmdname %s(%d) killed\n"
						,rec->client->index, cmdname, cmdpid);

				rec->status = BadAccess;
				kill(cmdpid, SIGKILL);
			}
		}

	}
}

static void 
XsmResetProc(ExtensionEntry * extEntry)
{
	/* Unregister callbacks */
	XaceDeleteCallback(XACE_EXT_DISPATCH, XsmExtension, NULL);
	XaceDeleteCallback(XACE_RESOURCE_ACCESS, XsmResource, NULL);
	XaceDeleteCallback(XACE_EXT_ACCESS, XsmExtension, NULL);
	XaceDeleteCallback(XACE_SELECTION_ACCESS, XsmSelection, NULL);
}



/* extension function called from client */
static int
ProcXsm(ClientPtr client){
	const char *cmdname, *cmdargs;
	pid_t cmdpid;

	cmdname = GetClientCmdName(client);
	cmdargs = GetClientCmdArgs(client);
	cmdpid = GetClientPid(client);

	LogMessage(X_INFO, 
				"ProcXsm call from %s(%d) args:%s\n"
				, cmdname, cmdpid, cmdargs);
	return Success;
}


void
XsmExtensionInit(void)
{
	ExtensionEntry *extEntry;
	int ret = TRUE;

	LogMessage(X_INFO, "XsmExtensionInit() Called\n");

	/* Read Xsm policy file */
	read_policy();

	/* Run inotify policy loader */
	inotify_policy();

	/* 
		X Access Control Extension Security hooks
		Constants used to identify the available security hooks

		#define XACE_CORE_DISPATCH		0
		#define XACE_EXT_DISPATCH       1
		#define XACE_RESOURCE_ACCESS    2
		#define XACE_DEVICE_ACCESS      3
		#define XACE_PROPERTY_ACCESS    4
		#define XACE_SEND_ACCESS        5
		#define XACE_RECEIVE_ACCESS     6
		#define XACE_CLIENT_ACCESS      7
		#define XACE_EXT_ACCESS         8
		#define XACE_SERVER_ACCESS      9
		#define XACE_SELECTION_ACCESS   10
		#define XACE_SCREEN_ACCESS      11
		#define XACE_SCREENSAVER_ACCESS 12
		#define XACE_AUTH_AVAIL         13
		#define XACE_KEY_AVAIL          14
		#define XACE_NUM_HOOKS          15
	*/
	
	ret &= XaceRegisterCallback(XACE_EXT_DISPATCH, XsmExtension, NULL);
	ret &= XaceRegisterCallback(XACE_RESOURCE_ACCESS, XsmResource, NULL);
	ret &= XaceRegisterCallback(XACE_EXT_ACCESS, XsmExtension, NULL);
	ret &= XaceRegisterCallback(XACE_SELECTION_ACCESS, XsmSelection, NULL);

	if (!ret)
		FatalError("XsmExtensionInit: Failed to register callbacks\n");
	
	extEntry = AddExtension("xsm", 
							1, 2,
							ProcXsm, ProcXsm,
							XsmResetProc, StandardMinorOpcode);
	
	XsmErrorBase = extEntry->errorBase;
    XsmEventBase = extEntry->eventBase;
	
} 
