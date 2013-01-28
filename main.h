/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    (c) 2010 by Hans-Peter Deifel
    (c) 2010 by Thomas Adam
    (c) 2013 Daniel Carl
    see LICENSE file
*/

typedef struct {
    GtkWindow           *window;
    GtkWidget           *viewport;
    GtkBox              *box;
    GtkScrollbar        *scroll_h;
    GtkScrollbar        *scroll_v;
    GtkAdjustment       *adjust_h;
    GtkAdjustment       *adjust_v;
    GtkWidget           *inputbox;
    GtkWidget           *eventbox;
    GtkWidget           *pane;
    GtkBox              *statusbar;
    GtkWidget           *status_url;
    GtkWidget           *status_state;
    WebKitWebView       *webview;
    WebKitWebInspector  *inspector;
} Gui;

typedef struct {
    unsigned int    mode;
    unsigned int    count;
    char            current_modkey;
    char            *search_handle;
    gboolean        search_direction;
    GdkNativeWindow embed; /* TODO seems to be redundant to winid */
    char            *error_msg;
    GList           *activeDownloads;
    GList           *commandhistory;
    int             commandpointer;
    char            rememberedURI[BUF_SIZE];
    GtkClipboard    *clipboards[2];
    gboolean        manual_focus;
    gboolean        is_inspecting;
    GdkKeymap       *keymap;
} State;

typedef struct {
    float   zoomstep;
    char    *modkeys;
    char    *config_base;
    char    *configfile;
    KeyList *keylistroot;
    GList   *colon_aliases;
    time_t  cookie_timeout;
} Config;

typedef struct {
    SoupSession     *session;
    SoupCookieJar   *session_cookie_jar;
    SoupCookieJar   *file_cookie_jar;
    char            *cookie_store;
} Network;

typedef struct {
    Gui     gui;
    State   state;
    Config  config;
    Network net;
} Client;


/* functions */
void update_state(void);
gboolean process_line(char *line);
gboolean echo(const Arg *arg);
char * search_word(int whichword);
