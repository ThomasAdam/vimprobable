/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    see LICENSE file
*/

/* Defines. */
#define CLIPBOARDS 2
#define URI_LEN 128
#define INPUTKEY_LEN 5
#define INPUTBUF_LEN 65
#define TARGET_LEN 8
#define SCROLL_STATE 4
#define COMMANDHISTSIZE 50

/* Structure definitions. */

typedef struct _gui_actions
{
	char rememberedURI[URI_LEN];
	char inputKey[INPUTKEY_LEN];
	char inputBuffer[INPUTBUF_LEN];
	char followTarget[TARGET_LEN];
	char scroll_state[SCROLL_STATE];
	char commandhistory[COMMANDHISTSIZE][255];
	int  lastcommand;
	int  maxcommands;
	int  commandpointer;
	KeyList *keylistroot;

} GUIActions;

typedef struct _cookie_handling
{
	SoupSession *session;
} Cookies;

typedef struct _webkit_data
{
	WebKitWebView *webview;
	WebKitWebInspector *inspector;
	WebKitDownload *active_download;
	GtkWidget* inspector_window;
	GtkWidget* inspector_view;

	float zoomstep;

	Cookies *cookie_data;
} WebKitData;

typedef struct _gui
{
	GtkWindow *window;
	GtkWidget *viewport;
	GtkBox *box;
	GtkScrollbar *scroll_h;
	GtkScrollbar *scroll_v;
	GtkAdjustment *adjust_h;
	GtkAdjustment *adjust_v;
	GtkWidget *inputbox;
	GtkWidget *eventbox;
	GtkWidget *status_url;
	GtkWidget *status_state;
	GdkNativeWindow *embed;
	char *winid;
	SoupSession *session;
	GtkClipboard *clipboards[CLIPBOARDS];

	GUIActions *gui_actions;
} GUI;

typedef struct _vimprobable
{
	GUI *gui;
	WebKitData *webkit_data;
} Vimprobable;

/* functions */
void update_state(void);
gboolean process_line(char *line);
gboolean echo(const Arg *arg);
