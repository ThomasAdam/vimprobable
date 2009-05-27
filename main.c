/*
    (c) 2009 by Leon Winter, see LICENSE file
*/

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit/webkit.h>
#include <libsoup/soup.h>

/* macros */
#define LENGTH(x)                   (sizeof(x)/sizeof(x[0]))
#define ISCOMMAND(str, cmd, len)    (g_str_has_prefix(str, cmd) && len > sizeof(cmd) + 1)

/* enums */
enum { ModeNormal, ModeInsert, ModeSearch, ModeWebsearch, ModeHints };                  /* modes */
enum { TargetCurrent, TargetNew };                                                      /* target */
/* bitmask,
    1 << 0:  0 = jumpTo,            1 = scroll
    1 << 1:  0 = top/down,          1 = left/right
    1 << 2:  0 = top/left,          1 = bottom/right
    1 << 3:  0 = paging/halfpage,   1 = line
    1 << 4:  0 = paging,            1 = halfpage aka buffer
*/
enum { ScrollJumpTo, ScrollMove };
enum { OrientationVert, OrientationHoriz = (1 << 1) };
enum { DirectionTop,
        DirectionBottom = (1 << 2),
        DirectionLeft = OrientationHoriz,
        DirectionRight = OrientationHoriz | (1 << 2) };
enum { UnitPage,
        UnitLine = (1 << 3),
        UnitBuffer = (1 << 4) };
/* bitmask:
    1 << 0:  0 = Reload/Cancel      1 = Forward/Back
    Forward/Back:
    1 << 1:  0 = Forward            1 = Back
    Reload/Cancel:
    1 << 1:  0 = Cancel             1 = Force/Normal Reload
    1 << 2:  0 = Force Reload       1 = Reload
*/
enum { NavigationForwardBack = 1, NavigationReloadActions = (1 << 1) };
enum { NavigationCancel,
        NavigationBack = (NavigationForwardBack | 1 << 1),
        NavigationForward = (NavigationForwardBack),
        NavigationReload = (NavigationReloadActions | 1 << 2),
        NavigationForceReload = NavigationReloadActions };
/* bitmask:
    1 << 1:  ClipboardPrimary (X11)
    1 << 2:  ClipboardGTK
    1 << 3:  0 = SourceSelection    1 = SourceURL
*/
enum { ClipboardPrimary = 1 << 1, ClipboardGTK = 1 << 2 };
enum { SourceSelection, SourceURL = 1 << 3 };
/* bitmask:
    1 << 0:  0 = ZoomReset          1 = ZoomIn/Out
    1 << 1:  0 = ZoomOut            1 = ZoomIn
    1 << 2:  0 = TextZoom           1 = FullContentZoom
*/
enum { ZoomReset,
        ZoomOut,
        ZoomIn = ZoomOut | (1 << 1) };
enum { ZoomText, ZoomFullContent = (1 << 2) };

enum { NthSubdir, Rootdir };

enum { Increment, Decrement };
/* bitmask:
    1 << 0:  0 = DirectionForward   1 = DirectionBackwards
    1 << 1:  0 = CaseInsensitive    1 = CaseSensitive
    1 << 2:  0 = No Wrapping        1 = Wrapping
*/
enum { DirectionBackwards, DirectionForward };
enum { CaseInsensitive, CaseSensitive = 1 << 1 };
enum { Wrapping = 1 << 2 };

/* structs here */
typedef struct {
    int i;
    char *s;
} Arg;

typedef struct {
    guint mask;
    guint modkey;
    guint key;
    gboolean (*func)(const Arg *arg);
    const Arg arg;
} Key;

/* callbacks here */
static void window_destroyed_cb(GtkWidget* window, gpointer func_data);
static void webview_title_changed_cb(WebKitWebView* webview, WebKitWebFrame* frame, char* title, gpointer user_data);
static void webview_load_committed_cb(WebKitWebView* webview, WebKitWebFrame* frame, gpointer user_data);
static void webview_title_changed_cb(WebKitWebView* webview, WebKitWebFrame* frame, char* title, gpointer user_data);
static void webview_progress_changed_cb(WebKitWebView* webview, int progress, gpointer user_data);
static void webview_load_committed_cb(WebKitWebView* webview, WebKitWebFrame* frame, gpointer user_data);
static void webview_load_finished_cb(WebKitWebView* webview, WebKitWebFrame* frame, gpointer user_data);
static gboolean webview_navigation_cb(WebKitWebView* webview, WebKitWebFrame* frame, WebKitNetworkRequest* request,
                        WebKitWebPolicyDecision* decision, gpointer user_data);
static gboolean webview_new_window_cb(WebKitWebView* webview, WebKitWebFrame* frame, WebKitNetworkRequest* request,
                        WebKitWebNavigationAction *action, WebKitWebPolicyDecision *decision, gpointer user_data);
static gboolean webview_mimetype_cb(WebKitWebView* webview, WebKitWebFrame* frame, WebKitNetworkRequest* request,
                        char* mime_type, WebKitWebPolicyDecision* decision, gpointer user_data);
static gboolean webview_download_cb(WebKitWebView* webview, GObject* download, gpointer user_data);
static gboolean webview_keypress_cb(WebKitWebView* webview, GdkEventKey* event);
static void webview_hoverlink_cb(WebKitWebView* webview, char* title, char* link, gpointer data);
static gboolean webview_console_cb(WebKitWebView* webview, char* message, int line, char* source, gpointer user_data);
static gboolean webview_scroll_cb(WebKitWebView* webview, GtkMovementStep step, int count, gpointer user_data);
static void inputbox_activate_cb(GtkEntry* entry, gpointer user_data);
static gboolean inputbox_keypress_cb(GtkEntry* entry, GdkEventKey* event);

/* functions */
static gboolean descend(const Arg* arg);
static gboolean focus(const Arg* arg);
static gboolean input(const Arg* arg);
static gboolean navigate(const Arg* arg);
static gboolean number(const Arg* arg);
static gboolean open(const Arg* arg);
static gboolean paste(const Arg* arg);
static gboolean quit(const Arg* arg);
static gboolean search(const Arg* arg);
static gboolean scroll(const Arg* arg);
static gboolean yank(const Arg *arg);
static gboolean zoom(const Arg *arg);
static void update_url();
static void update_state();
static void setup_modkeys();
static void setup_gui();
static void setup_settings();
static void setup_signals();
static void ascii_bar(int total, int state, char* string);

/* variables */
static GtkWidget* window;
static GtkAdjustment* adjust_h;
static GtkAdjustment* adjust_v;
static GtkWidget* inputbox;
static GtkWidget* eventbox;
static GtkWidget* status_url;
static GtkWidget* status_state;
static WebKitWebView* webview;
static GtkClipboard* clipboards[2];

static char **args;
static unsigned int mode = ModeNormal;
static unsigned int count = 0;
static float zoomstep;
static char scroll_state[4] = "\0";
static char* modkeys;
static char current_modkey;
static char* search_handle;

#include "config.h"

/* callbacks */
void
window_destroyed_cb(GtkWidget* window, gpointer func_data) {
    quit(NULL);
}

void
webview_title_changed_cb(WebKitWebView* webview, WebKitWebFrame* frame, char* title, gpointer user_data) {
    gtk_window_set_title((GtkWindow*)window, title);
}

void
webview_progress_changed_cb(WebKitWebView* webview, int progress, gpointer user_data) {
#ifdef ENABLE_GTK_PROGRESS_BAR
    gtk_entry_set_progress_fraction((GtkEntry*)inputbox, progress == 100 ? 0 : (double)progress/100);
#endif
    update_state();
}

#ifdef ENABLE_WGET_PROGRESS_BAR
void
ascii_bar(int total, int state, char* string) {
    int i;

    for(i = 0; i < state; i++)
        string[i] = progressbartickchar;
    string[i++] = progressbarcurrent;
    for(; i < total; i++)
        string[i] = progressbarspacer;
    string[i] = '\0';
}
#endif

void
webview_load_committed_cb(WebKitWebView* webview, WebKitWebFrame* frame, gpointer user_data) {
    const char* uri = webkit_web_view_get_uri(webview);

    update_url(uri);
}

void
webview_load_finished_cb(WebKitWebView* webview, WebKitWebFrame* frame, gpointer user_data) {
    update_state();
}

gboolean
webview_navigation_cb(WebKitWebView* webview, WebKitWebFrame* frame, WebKitNetworkRequest* request,
                        WebKitWebPolicyDecision* decision, gpointer user_data) {
    return FALSE;
}

gboolean
webview_new_window_cb(WebKitWebView* webview, WebKitWebFrame* frame, WebKitNetworkRequest* request,
                        WebKitWebNavigationAction *action, WebKitWebPolicyDecision *decision, gpointer user_data) {
    return FALSE;
}

gboolean
webview_mimetype_cb(WebKitWebView* webview, WebKitWebFrame* frame, WebKitNetworkRequest* request,
                        char* mime_type, WebKitWebPolicyDecision* decision, gpointer user_data) {
    return FALSE;
}

gboolean
webview_download_cb(WebKitWebView* webview, GObject* download, gpointer user_data) {
    return FALSE;
}

gboolean
webview_keypress_cb(WebKitWebView* webview, GdkEventKey* event) {
    unsigned int i;

    switch (mode) {
    case ModeNormal:
        if(event->state == 0) {
            if((event->keyval >= GDK_1 && event->keyval <= GDK_9)
            ||  (event->keyval == GDK_0 && count)) {
                count = (count ? count * 10 : 0) + (event->keyval - GDK_0);
                update_state();
                return TRUE;
            } else if(strchr(modkeys, event->keyval) && current_modkey != event->keyval) {
                current_modkey = event->keyval;
                update_state();
                return TRUE;
            }
        }
        /* keybindings */
        for(i = 0; i < LENGTH(keys); i++)
            if(keys[i].mask == event->state
            && (keys[i].modkey == current_modkey || (!keys[i].modkey && !current_modkey))
            && keys[i].key == event->keyval
            && keys[i].func)
                if(keys[i].func(&keys[i].arg)) {
                    current_modkey = count = 0;
                    update_state();
                    return TRUE;
                }
        break;
    }
    return FALSE;
}

void
webview_hoverlink_cb(WebKitWebView* webview, char* title, char* link, gpointer data) {
    const char* uri = webkit_web_view_get_uri(webview);

    if(link)
        gtk_label_set_markup((GtkLabel*)status_url, g_markup_printf_escaped("<span font=\"%s\">Link: %s</span>", statusfont, link));
    else
        update_url(uri);
}

gboolean
webview_console_cb(WebKitWebView* webview, char* message, int line, char* source, gpointer user_data) {
    return FALSE;
}

gboolean
webview_scroll_cb(WebKitWebView* webview, GtkMovementStep step, int count, gpointer user_data) {
    update_state();
    return TRUE;
}

void
inputbox_activate_cb(GtkEntry* entry, gpointer user_data) {
    char* text;
    guint16 length = gtk_entry_get_text_length(entry);
    Arg a;

    if(length < 2)
        return;
    text = (char*)gtk_entry_get_text(entry);
    if(text[0] == ':') {
        if(ISCOMMAND(&text[1], "open", length)) {
            a.s =& text[sizeof("open") + 1];
            a.i = TargetCurrent;
            open(&a);
        } else if(ISCOMMAND(&text[1], "tabopen", length)) {
            a.s =& text[sizeof("tabopen") + 1];
            a.i = TargetNew;
            open(&a);
        } else
            return;
    } else if(text[0] == '/') {
        webkit_web_view_unmark_text_matches(webview);
#ifdef ENABLE_MATCH_HIGHLITING
        webkit_web_view_mark_text_matches(webview, &text[1], FALSE, 0);
        webkit_web_view_set_highlight_text_matches(webview, TRUE);
#endif
        count = 0;
        a.s =& text[1];
        a.i = searchoptions;
        search(&a);
    } else
        return;
    gtk_entry_set_text(entry, "");
    gtk_widget_grab_focus((GtkWidget*)webview);
}

static gboolean inputbox_keypress_cb(GtkEntry* entry, GdkEventKey* event) {
    if(event->type == GDK_KEY_PRESS && event->keyval == GDK_Escape)
        return focus(NULL);
    return FALSE;
}

/* funcs here */
gboolean
descend(const Arg* arg) {
    char *source = (char*)webkit_web_view_get_uri(webview), *p = &source[0], *new;
    int i, len;
    count = count ? count : 1;

    if(arg->i == Rootdir) {
        for(i = 0; i < 3; i++)                  /* get to the third slash */
            if(!(p = strchr(++p, '/')))
                return TRUE;                    /* if we cannot find it quit */
    } else {
        len = strlen(source);
        if(!len)                                /* if string is empty quit */
            return TRUE;
        p = source + len;                       /* start at the end */
        if(*(p - 1) == '/')                     /* /\/$/ is not an additional level */
            ++count;
        for(i = 0; i < count; i++)
            while(*(p--) != '/' || *p == '/')   /* count /\/+/ as one slash */
                if(p == source)                 /* if we reach the first char pointer quit */
                    return TRUE;
        ++p;                                    /* since we do p-- in the while, we are pointing at
                                                   the char before the slash, so +1  */
    }
    len =  p - source + 1;                      /* new length = end - start + 1 */
    new = malloc(len);
    memcpy(new, source, len);
    new[len] = '\0';
    webkit_web_view_load_uri(webview, new);
    free(new);
    return TRUE;
}

gboolean
focus(const Arg* arg) {
    webkit_web_view_unmark_text_matches(webview);
    gtk_entry_set_text((GtkEntry*)inputbox, "");
    gtk_widget_grab_focus((GtkWidget*)webview);
    return TRUE;
}

gboolean
input(const Arg* arg) {
    gtk_entry_set_text((GtkEntry*)inputbox, arg->s);
    gtk_widget_grab_focus(inputbox);
    gtk_editable_set_position((GtkEditable*)inputbox, -1);
    return TRUE;
}

gboolean
navigate(const Arg* arg) {
    if(arg->i & NavigationForwardBack)
        webkit_web_view_go_back_or_forward(webview, (arg->i == NavigationBack ? -1 : 1) * (count ? count : 1));
    else if(arg->i & NavigationReloadActions)
        (arg->i == NavigationReload ? webkit_web_view_reload : webkit_web_view_reload_bypass_cache)(webview);
    else
        webkit_web_view_stop_loading(webview);
    return TRUE;
}

gboolean
number(const Arg* arg) {
    const char* source = webkit_web_view_get_uri(webview);
    char *uri, *p, *new;
    int number, diff = (count ? count : 1) * (arg->i == Increment ? 1 : -1);

    uri = strdup(source); /* copy string */
    p =& uri[0];
    while(*p != '\0') /* goto the end of the string */
        ++p;
    --p;
    while(*p >= '0' && *p <= '9') /* go back until non number char is reached */
        --p;
    if(*(++p) == '\0') { /* if no numbers were found abort */
        free(uri);
        return TRUE;
    }
    number = atoi(p) + diff; /* apply diff on number */
    *p = '\0';
    new = g_strdup_printf("%s%d", uri, number); /* create new uri */
    webkit_web_view_load_uri(webview, new);
    g_free(new);
    free(uri);
    return TRUE;
}

gboolean
open(const Arg* arg) {
    char *argv[] = { *args, arg->s, NULL };

    if(arg->i == TargetCurrent)
        webkit_web_view_load_uri(webview, arg->s);
    else
        g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
    return TRUE;
}

gboolean
yank(const Arg *arg) {
    const char* url;

    if(arg->i & SourceURL) {
        url = webkit_web_view_get_uri(webview);
        if(arg->i & ClipboardPrimary)
            gtk_clipboard_set_text(clipboards[0], url, -1);
        if(arg->i & ClipboardGTK)
            gtk_clipboard_set_text(clipboards[1], url, -1);
    } else
        webkit_web_view_copy_clipboard(webview);
    return TRUE;
}

gboolean
paste(const Arg* arg) {
    Arg a = { .i = arg->i & TargetNew, .s = NULL };

    if(arg->i & ClipboardPrimary)
        a.s = gtk_clipboard_wait_for_text(clipboards[0]);
    if(!a.s && arg->i & ClipboardGTK)
        a.s = gtk_clipboard_wait_for_text(clipboards[1]);
    if(a.s)
        open(&a);
    return TRUE;
}

gboolean
quit(const Arg* arg) {
    gtk_main_quit();
    return TRUE;
}

gboolean
search(const Arg* arg) {
    count = count ? count : 1;

    if(arg->s) {
        free(search_handle);
        search_handle = strdup(arg->s);
    }
    if(!search_handle)
        return TRUE;
    do webkit_web_view_search_text(webview, search_handle, arg->i & CaseSensitive, arg->i & DirectionForward, arg->i & Wrapping);
    while(--count);
    return TRUE;
}

gboolean
scroll(const Arg* arg) {
    GtkAdjustment* adjust = (arg->i & OrientationHoriz) ? adjust_h : adjust_v;

    if(arg->i & ScrollMove)
        gtk_adjustment_set_value(adjust, gtk_adjustment_get_value(adjust) +
            (arg->i & (1 << 2) ? 1 : -1) *      /* direction */
            ((arg->i & UnitLine || (arg->i & UnitBuffer && count)) ? (scrollstep * (count ? count : 1)) : (
                arg->i & UnitBuffer ? gtk_adjustment_get_page_size(adjust) / 2 :
                (count ? count : 1) * (gtk_adjustment_get_page_size(adjust) -
                    (gtk_adjustment_get_page_size(adjust) > pagingkeep ? pagingkeep : 0)))));
    else
        gtk_adjustment_set_value(adjust,
            ((arg->i & (1 << 2)) ?  gtk_adjustment_get_upper : gtk_adjustment_get_lower)(adjust));
    update_state();
    return TRUE;
}

gboolean
zoom(const Arg* arg) {
    webkit_web_view_set_full_content_zoom(webview, (arg->i & ZoomFullContent) > 0);
    webkit_web_view_set_zoom_level(webview, (arg->i & ZoomOut) ?
        webkit_web_view_get_zoom_level(webview) +
            (((float)(count ? count : 1)) * (arg->i & (1 << 1) ? 1.0 : -1.0) * zoomstep) :
        (count ? (float)count / 100.0 : 1.0));
    return TRUE;
}

void
update_url(const char* uri) {
    gboolean ssl = g_str_has_prefix(uri, "https://");
    GdkColor color;
#ifdef ENABLE_HISTORY_INDICATOR
    char before[] = " [";
    char after[] = "]";
    gboolean back = webkit_web_view_can_go_back(webview);
    gboolean fwd = webkit_web_view_can_go_forward(webview);

    if(!back && !fwd)
        before[0] = after[0] = '\0';
#endif
    gtk_label_set_markup((GtkLabel*)status_url, g_markup_printf_escaped(
#ifdef ENABLE_HISTORY_INDICATOR
        "<span font=\"%s\">%s%s%s%s%s</span>", statusfont, uri,
        before, back ? "+" : "", fwd ? "-" : "", after
#else
        "<span font=\"%s\">%s</span>", statusfont, uri
#endif
    ));
    gdk_color_parse(ssl ? sslbgcolor : statusbgcolor, &color);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &color);
    gdk_color_parse(ssl ? sslcolor : statuscolor, &color);
    gtk_widget_modify_fg((GtkWidget*)status_url, GTK_STATE_NORMAL, &color);
    gtk_widget_modify_fg((GtkWidget*)status_state, GTK_STATE_NORMAL, &color);
}

void
update_state() {
    int max = gtk_adjustment_get_upper(adjust_v) - gtk_adjustment_get_page_size(adjust_v);
    int val = (int)(gtk_adjustment_get_value(adjust_v) / max * 100);
    char* markup;
#ifdef ENABLE_WGET_PROGRESS_BAR
    double progress;
    char progressbar[progressbartick + 1];

    g_object_get((GObject*)webview, "progress", &progress, NULL);
#endif

    if(max == 0)
        sprintf(&scroll_state[0], "All");
    else if(val == 0)
        sprintf(&scroll_state[0], "Top");
    else if(val == 100)
        sprintf(&scroll_state[0], "Bot");
    else
        sprintf(&scroll_state[0], "%d%%", val);
#ifdef ENABLE_WGET_PROGRESS_BAR
    if(webkit_web_view_get_load_status(webview) != WEBKIT_LOAD_FINISHED) {
        ascii_bar(progressbartick, (int)(progress * progressbartick / 100), (char*)progressbar);
        markup = (char*)g_markup_printf_escaped("<span font=\"%s\">%.0d%c %c%s%c %s</span>",
            statusfont, count, current_modkey, progressborderleft, progressbar, progressborderright, scroll_state);
    } else
#endif
    markup = (char*)g_markup_printf_escaped("<span font=\"%s\">%.0d%c %s</span>", statusfont, count, current_modkey, scroll_state);
    gtk_label_set_markup((GtkLabel*)status_state, markup);
}

void
setup_modkeys() {
    unsigned int i;
    modkeys = calloc(LENGTH(keys) + 1, sizeof(char));
    char* ptr = modkeys;

    for(i = 0; i < LENGTH(keys); i++)
        if(keys[i].modkey && !strchr(modkeys, keys[i].modkey))
            *(ptr++) = keys[i].modkey;
    modkeys = realloc(modkeys, &ptr[0] - &modkeys[0] + 1);
}

void
setup_gui() {
    GtkScrollbar* scroll_h = (GtkScrollbar*)gtk_hscrollbar_new(NULL);
    GtkScrollbar* scroll_v = (GtkScrollbar*)gtk_vscrollbar_new(NULL);
    adjust_h = gtk_range_get_adjustment((GtkRange*)scroll_h);
    adjust_v = gtk_range_get_adjustment((GtkRange*)scroll_v);
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkBox* box = (GtkBox*)gtk_vbox_new(FALSE, 0);
    inputbox = gtk_entry_new();
    GtkWidget* viewport = gtk_scrolled_window_new(adjust_h, adjust_v);
    webview = (WebKitWebView*)webkit_web_view_new();
    GtkBox* statusbar = (GtkBox*)gtk_hbox_new(FALSE, 0);
    eventbox = gtk_event_box_new();
    status_url = gtk_label_new(NULL);
    status_state = gtk_label_new(NULL);
    GdkColor bg;
    PangoFontDescription* font;

    clipboards[0] = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    clipboards[1] = gtk_clipboard_get(GDK_NONE);
    setup_settings();
    gdk_color_parse(statusbgcolor, &bg);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &bg);
    gtk_widget_set_name(window, "Vimpression");
#ifdef DISABLE_SCROLLBAR
    gtk_scrolled_window_set_policy((GtkScrolledWindow*)viewport, GTK_POLICY_NEVER, GTK_POLICY_NEVER);
#endif
    setup_signals();
    gtk_container_add((GtkContainer*)viewport, (GtkWidget*)webview);
    font = pango_font_description_from_string(urlboxfont);
    gtk_widget_modify_font((GtkWidget*)inputbox, font);
    pango_font_description_free(font);
    gtk_entry_set_inner_border((GtkEntry*)inputbox, NULL);
    gtk_misc_set_alignment((GtkMisc*)status_url, 0.0, 0.0);
    gtk_misc_set_alignment((GtkMisc*)status_state, 1.0, 0.0);
    gtk_box_pack_start(statusbar, status_url, TRUE, TRUE, 2);
    gtk_box_pack_start(statusbar, status_state, FALSE, FALSE, 2);
    gtk_container_add((GtkContainer*)eventbox, (GtkWidget*)statusbar);
    gtk_box_pack_start(box, viewport, TRUE, TRUE, 0);
    gtk_box_pack_start(box, eventbox, FALSE, FALSE, 0);
    gtk_entry_set_has_frame((GtkEntry*)inputbox, FALSE);
    gtk_box_pack_start(box, inputbox, FALSE, FALSE, 0);
    gtk_container_add((GtkContainer*)window, (GtkWidget*)box);
    gtk_widget_grab_focus((GtkWidget*)webview);
    gtk_widget_show_all(window);
}

void
setup_settings() {
    WebKitWebSettings* settings = (WebKitWebSettings*)webkit_web_settings_new();

#ifdef WEBKITSETTINGS
    g_object_set((GObject*)settings, WEBKITSETTINGS, NULL);
#endif
    g_object_get((GObject*)settings, "zoom-step", &zoomstep, NULL);
    webkit_web_view_set_settings(webview, settings);
}

void
setup_signals() {
    /* window */
    g_object_connect((GObject*)window,
        "signal::destroy",                              (GCallback)window_destroyed_cb,             NULL,
    NULL);
    /* webview */
    g_object_connect((GObject*)webview,
        "signal::title-changed",                        (GCallback)webview_title_changed_cb,        NULL,
        "signal::load-progress-changed",                (GCallback)webview_progress_changed_cb,     NULL,
        "signal::load-committed",                       (GCallback)webview_load_committed_cb,       NULL,
        "signal::load-finished",                        (GCallback)webview_load_finished_cb,        NULL,
        "signal::navigation-policy-decision-requested", (GCallback)webview_navigation_cb,           NULL,
        "signal::new-window-policy-decision-requested", (GCallback)webview_new_window_cb,           NULL,
        "signal::mime-type-policy-decision-requested",  (GCallback)webview_mimetype_cb,             NULL,
        "signal::download-requested",                   (GCallback)webview_download_cb,             NULL,
        "signal::key-press-event",                      (GCallback)webview_keypress_cb,             NULL,
        "signal::hovering-over-link",                   (GCallback)webview_hoverlink_cb,            NULL,
        "signal::console-message",                      (GCallback)webview_console_cb,              NULL,
        "signal-after::move-cursor",                    (GCallback)webview_scroll_cb,               NULL,
    NULL);
    /* inputbox */
    g_object_connect((GObject*)inputbox,
        "signal::activate",                             (GCallback)inputbox_activate_cb,            NULL,
        "signal::key-press-event",                      (GCallback)inputbox_keypress_cb,            NULL,
    NULL);
}

int
main(int argc, char* argv[]) {
    Arg a;
    args = argv;

    gtk_init(&argc, &argv);
    if(!g_thread_supported())
        g_thread_init(NULL);
    setup_modkeys();
    setup_gui();
    a.i = TargetCurrent;
    a.s = argc > 1 ? argv[1] : startpage;
    open(&a);
    gtk_main();

    return EXIT_SUCCESS;
}
