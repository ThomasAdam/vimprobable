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

/* Structure definitions. */
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
	GtkNativeWindow *embed;
	char *winid;
	SoupSession *session;
	GtkClipboard *clipboards[CLIPBOARDS];

	GUIActions *gui_actions;
} GUI;

typedef struct _webkit_data
{
	WebKitWebView *webview;
	WebKitInspector *inspector;
	WebKitDownliad *active_download;
	float zoomstep;
} WebkitData;

typedef struct _cookie_handling
{
	SoupSession *session;
} Cookies;

typedef struct _gui_actions
{
	char rememberedURI[URI_LEN] = "";
	char inputKey[INPUTKEY_LEN];
	char inputBuffer[INPUTBUF_LEN] = "";
	char followTarget[TARGET_LEN] = "";
} GUIActions;

/* functions */
void update_state(void);
gboolean process_line(char *line);
gboolean echo(const Arg *arg);
