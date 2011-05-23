/*
    (c) 2009 by Leon Winter
    (c) 2009-2010 by Hannes Schueller
    (c) 2009-2010 by Matto Fransen
    (c) 2010 by Hans-Peter Deifel
    (c) 2010 by Thomas Adam
    see LICENSE file
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/file.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit/webkit.h>
#include <libsoup/soup.h>
#include <JavaScriptCore/JavaScript.h>
#include <X11/Xlib.h>
#include "vimprobable.h"
#include "javascript.h"

/* the CLEAN_MOD_*_MASK defines have all the bits set that will be stripped from the modifier bit field */
#define CLEAN_MOD_NUMLOCK_MASK (GDK_MOD2_MASK)
#define CLEAN_MOD_BUTTON_MASK (GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK|GDK_BUTTON4_MASK|GDK_BUTTON5_MASK)

/* remove unused bits, numlock symbol and buttons from keymask */
#define CLEAN(mask) (mask & (GDK_MODIFIER_MASK) & ~(CLEAN_MOD_NUMLOCK_MASK) & ~(CLEAN_MOD_BUTTON_MASK))

/* callbacks here */
static gboolean inputbox_keypress_cb(GtkEntry *entry, GdkEventKey *event);
static gboolean inputbox_keyrelease_cb(GtkEntry *entry, GdkEventKey *event);
static gboolean inputbox_changed_cb(GtkEditable *entry, gpointer user_data);
static gboolean notify_event_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static gboolean webview_console_cb(WebKitWebView *webview, char *message, int line, char *source, gpointer user_data);
static gboolean webview_download_cb(WebKitWebView *webview, WebKitDownload *download, gpointer user_data);
static void webview_hoverlink_cb(WebKitWebView *webview, char *title, char *link, gpointer data);
static gboolean webview_keypress_cb(WebKitWebView *webview, GdkEventKey *event);
static void webview_load_committed_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data);
static void webview_load_finished_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data);
static gboolean webview_mimetype_cb(WebKitWebView *webview, WebKitWebFrame *frame, WebKitNetworkRequest *request,
                        char *mime_type, WebKitWebPolicyDecision *decision, gpointer user_data);
static gboolean webview_navigation_cb(WebKitWebView *webview, WebKitWebFrame *frame, WebKitNetworkRequest *request,
                        WebKitWebPolicyDecision *decision, gpointer user_data);
static gboolean webview_new_window_cb(WebKitWebView *webview, WebKitWebFrame *frame, WebKitNetworkRequest *request,
                        WebKitWebNavigationAction *action, WebKitWebPolicyDecision *decision, gpointer user_data);
static gboolean webview_open_in_new_window_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data);
static void webview_progress_changed_cb(WebKitWebView *webview, int progress, gpointer user_data);
static void webview_scroll_cb(GtkAdjustment *adjustment, gpointer user_data);
static void webview_title_changed_cb(WebKitWebView *webview, WebKitWebFrame *frame, char *title, gpointer user_data);
static void webview_title_changed_cb(WebKitWebView *webview, WebKitWebFrame *frame, char *title, gpointer user_data);
static void window_destroyed_cb(GtkWidget *window, gpointer func_data);

/* functions */
static gboolean bookmark(const Arg *arg);
static gboolean complete(const Arg *arg);
static gboolean descend(const Arg *arg);
static gboolean echo(const Arg *arg);
static gboolean focus_input(const Arg *arg);
static gboolean input(const Arg *arg);
static gboolean navigate(const Arg *arg);
static gboolean number(const Arg *arg);
static gboolean open_arg(const Arg *arg);
static gboolean paste(const Arg *arg);
static gboolean quit(const Arg *arg);
static gboolean revive(const Arg *arg);
static gboolean print_frame(const Arg *arg);
static gboolean search(const Arg *arg);
static gboolean set(const Arg *arg);
static gboolean script(const Arg *arg);
static gboolean scroll(const Arg *arg);
static gboolean search_tag(const Arg *arg);
static gboolean toggle_images(const Arg *arg);
static gboolean toggle_plugins(const Arg *arg);
static gboolean yank(const Arg *arg);
static gboolean view_source(const Arg * arg);
static gboolean zoom(const Arg *arg);
static gboolean quickmark(const Arg *arg);
static gboolean fake_key_event(const Arg *arg);

static void update_url(const char *uri);
static void update_state(void);
static void setup_modkeys(void);
static void setup_gui(void);
static void setup_settings(void);
static void setup_signals(void);
static void ascii_bar(int total, int state, char *string);
static gchar *jsapi_ref_to_string(JSContextRef context, JSValueRef ref);
static void jsapi_evaluate_script(const gchar *script, gchar **value, gchar **message);
static void download_progress(WebKitDownload *d, GParamSpec *pspec);
gboolean build_taglist(const Arg *arg, FILE *f);
void set_error(const char *error);
void give_feedback(const char *feedback);
Listelement * complete_list(const char *searchfor, const int mode, Listelement *elementlist);
Listelement * add_list(const char *element, Listelement *elementlist);
int count_list(Listelement *elementlist);
void free_list(Listelement *elementlist);
void fill_suggline(char * suggline, const char * command, const char *fill_with);
GtkWidget * fill_eventbox(const char * completion_line);

static void history(void);
static void mop_up(void);

/* variables */
static GtkWindow *window;
static GtkBox *box;
static GtkAdjustment *adjust_h;
static GtkAdjustment *adjust_v;
static GtkWidget *inputbox;
static GtkWidget *eventbox;
static GtkWidget *status_url;
static GtkWidget *status_state;
static WebKitWebView *webview;
static SoupSession *session;
static GtkClipboard *clipboards[2];

static char **args;
static unsigned int mode = ModeNormal;
static unsigned int count = 0;
static float zoomstep;
static char *modkeys;
static char current_modkey;
static char *search_handle;
static gboolean search_direction;
static gboolean echo_active = TRUE;

static GdkNativeWindow embed = 0;
static char *winid = NULL;

static char rememberedURI[128] = "";
static char followTarget[8] = "";
char *error_msg = NULL;

GList *activeDownloads;

#include "config.h"

/* Cookie support. */
#ifdef ENABLE_COOKIE_SUPPORT
static SoupCookieJar *session_cookie_jar = NULL;
static SoupCookieJar *file_cookie_jar = NULL;
static time_t cookie_timeout = 4800;
static char *cookie_store;
static void setup_cookies(void);
static const char *get_cookies(SoupURI *soup_uri);
static void load_all_cookies(void);
static void new_generic_request(SoupSession *soup_ses, SoupMessage *soup_msg, gpointer unused);
static void update_cookie_jar(SoupCookieJar *jar, SoupCookie *old, SoupCookie *new);
static void handle_cookie_request(SoupMessage *soup_msg, gpointer unused);
#endif
/* callbacks */
void
window_destroyed_cb(GtkWidget *window, gpointer func_data) {
    quit(NULL);
}

void
webview_title_changed_cb(WebKitWebView *webview, WebKitWebFrame *frame, char *title, gpointer user_data) {
    gtk_window_set_title(window, title);
}

void
webview_progress_changed_cb(WebKitWebView *webview, int progress, gpointer user_data) {
#ifdef ENABLE_GTK_PROGRESS_BAR
    gtk_entry_set_progress_fraction(GTK_ENTRY(inputbox), progress == 100 ? 0 : (double)progress/100);
#endif
    update_state();
}

#ifdef ENABLE_WGET_PROGRESS_BAR
void
ascii_bar(int total, int state, char *string) {
    int i;

    for (i = 0; i < state; i++)
        string[i] = progressbartickchar;
    string[i++] = progressbarcurrent;
    for (; i < total; i++)
        string[i] = progressbarspacer;
    string[i] = '\0';
}
#endif

void
webview_load_committed_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data) {
    Arg a = { .i = Silent, .s = g_strdup(JS_SETUP_HINTS) };
    const char *uri = webkit_web_view_get_uri(webview);

    update_url(uri);
    script(&a);
}

void
webview_load_finished_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data) {
    Arg a = { .i = Silent, .s = g_strdup(JS_SETUP_INPUT_FOCUS) };

    if (HISTORY_MAX_ENTRIES > 0)
        history();
    update_state();
    script(&a);
}

gboolean
webview_navigation_cb(WebKitWebView *webview, WebKitWebFrame *frame, WebKitNetworkRequest *request,
                        WebKitWebPolicyDecision *decision, gpointer user_data) {
    return FALSE;
}

static gboolean
webview_open_in_new_window_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data) {
    Arg a = { .i = TargetNew, .s = (char*)webkit_web_view_get_uri(webview) };
    if (strlen(rememberedURI) > 0) {
        a.s = rememberedURI;
    }
    open_arg(&a);
    return FALSE;
}

gboolean
webview_new_window_cb(WebKitWebView *webview, WebKitWebFrame *frame, WebKitNetworkRequest *request,
                        WebKitWebNavigationAction *action, WebKitWebPolicyDecision *decision, gpointer user_data) {
    Arg a = { .i = TargetNew, .s = (char*)webkit_network_request_get_uri(request) };
    open_arg(&a);
    webkit_web_policy_decision_ignore(decision);
    return TRUE;
}

gboolean
webview_mimetype_cb(WebKitWebView *webview, WebKitWebFrame *frame, WebKitNetworkRequest *request,
                        char *mime_type, WebKitWebPolicyDecision *decision, gpointer user_data) {
    if (webkit_web_view_can_show_mime_type(webview, mime_type) == FALSE) {
        webkit_web_policy_decision_download(decision);
        return TRUE;
    } else {
        return FALSE;
    }
}

gboolean
webview_download_cb(WebKitWebView *webview, WebKitDownload *download, gpointer user_data) {
    const gchar *filename;
    gchar *uri, *path;
    uint32_t size;
    Arg a;

    filename = webkit_download_get_suggested_filename(download);
    if (filename == NULL || strlen(filename) == 0) {
        filename = "vimprobable_download";
    }
    path = g_build_filename(g_strdup_printf(DOWNLOADS_PATH), filename, NULL);
    uri = g_strconcat("file://", path, NULL);
    webkit_download_set_destination_uri(download, uri);
    g_free(uri);
    size = (uint32_t)webkit_download_get_total_size(download);
    a.i = Info;
    if (size > 0)
        a.s = g_strdup_printf("Download %s started (expected size: %u bytes)...", filename, size);
    else
        a.s = g_strdup_printf("Download %s started (unknown size)...", filename);
    echo(&a);
    activeDownloads = g_list_prepend(activeDownloads, download);
    g_signal_connect(download, "notify::progress", G_CALLBACK(download_progress), NULL);
    g_signal_connect(download, "notify::status", G_CALLBACK(download_progress), NULL);
    update_state();
    return TRUE;
}

void
download_progress(WebKitDownload *d, GParamSpec *pspec) {
    Arg a;
    WebKitDownloadStatus status = webkit_download_get_status(d);

    if (status != WEBKIT_DOWNLOAD_STATUS_STARTED && status != WEBKIT_DOWNLOAD_STATUS_CREATED) {
        if (status != WEBKIT_DOWNLOAD_STATUS_FINISHED) {
            a.i = Error;
            a.s = g_strdup_printf("Error while downloading %s", webkit_download_get_suggested_filename(d));
            echo(&a);
        } else {
            a.i = Info;
            a.s = g_strdup_printf("Download %s finished", webkit_download_get_suggested_filename(d));
            echo(&a);
        }
        activeDownloads = g_list_remove(activeDownloads, d);
    }
    update_state();
}

gboolean
webview_keypress_cb(WebKitWebView *webview, GdkEventKey *event) {
    unsigned int i;
    Arg a = { .i = ModeNormal, .s = NULL };

    switch (mode) {
    case ModeNormal:
        if (CLEAN(event->state) == 0) {
            if (event->keyval == GDK_Escape) {
                a.i = Info;
                a.s = g_strdup("");
                echo(&a);
            } else if (current_modkey == 0 && ((event->keyval >= GDK_1 && event->keyval <= GDK_9)
                    || (event->keyval == GDK_0 && count))) {
                count = (count ? count * 10 : 0) + (event->keyval - GDK_0);
                update_state();
                return TRUE;
            } else if (strchr(modkeys, event->keyval) && current_modkey != event->keyval) {
                current_modkey = event->keyval;
                update_state();
                return TRUE;
            }
        }
        /* keybindings */
        for (i = 0; i < LENGTH(keys); i++)
            if (keys[i].mask == CLEAN(event->state)
            && (keys[i].modkey == current_modkey
                || (!keys[i].modkey && !current_modkey)
                || keys[i].modkey == GDK_VoidSymbol)    /* wildcard */
            && keys[i].key == event->keyval
            && keys[i].func)
                if (keys[i].func(&keys[i].arg)) {
                    current_modkey = count = 0;
                    update_state();
                    return TRUE;
                }
        break;
    case ModeInsert:
        if (CLEAN(event->state) == 0 && event->keyval == GDK_Escape) {
            a.i = Silent;
            a.s = g_strdup("vimprobable_clearfocus()");
            script(&a);
            a.i = ModeNormal;
            return set(&a);
        }
    case ModePassThrough:
        if (CLEAN(event->state) == 0 && event->keyval == GDK_Escape) {
            echo(&a);
            set(&a);
            return TRUE;
        }
        break;
    case ModeSendKey:
        echo(&a);
        set(&a);
        break;
    }
    return FALSE;
}

void
webview_hoverlink_cb(WebKitWebView *webview, char *title, char *link, gpointer data) {
    const char *uri = webkit_web_view_get_uri(webview);

    memset(rememberedURI, 0, 128);
    if (link) {
        gtk_label_set_markup(GTK_LABEL(status_url), g_markup_printf_escaped("<span font=\"%s\">Link: %s</span>", statusfont, link));
        strncpy(rememberedURI, link, 128);
    } else
        update_url(uri);
}

gboolean
webview_console_cb(WebKitWebView *webview, char *message, int line, char *source, gpointer user_data) {
    Arg a;

    /* Don't change internal mode if the browser doesn't have focus to prevent inconsistent states */
    if (gtk_window_has_toplevel_focus(window)) {
        if (!strcmp(message, "hintmode_off") || !strcmp(message, "insertmode_off")) {
            a.i = ModeNormal;
            return set(&a);
        } else if (!strcmp(message, "insertmode_on")) {
            a.i = ModeInsert;
            return set(&a);
        }
    }
    return FALSE;
}

void
webview_scroll_cb(GtkAdjustment *adjustment, gpointer user_data) {
    update_state();
}

void
inputbox_activate_cb(GtkEntry *entry, gpointer user_data) {
    char *text;
    guint16 length = gtk_entry_get_text_length(entry);
    Arg a;
    int i;
    size_t len;
    gboolean success = FALSE, forward = FALSE, found = FALSE;

    a.i = HideCompletion;
    complete(&a);
    if (length == 0)
        return;
    text = (char*)gtk_entry_get_text(entry);
    if (length > 1 && text[0] == ':') {
        for (i = 0; i < LENGTH(commands); i++) {
            len = strlen(commands[i].cmd);
            if (length >= len && !strncmp(&text[1], commands[i].cmd, len) && (text[len + 1] == ' ' || !text[len + 1])) {
                found = TRUE;
                a.i = commands[i].arg.i;
                a.s = length > len + 2 ? &text[len + 2] : commands[i].arg.s;
                success = commands[i].func(&a);
                break;
            }
        }
        if (!found) {
            a.i = Error;
            a.s = g_strdup_printf("Not a browser command: %s", &text[1]);
            echo(&a);
        } else if (!success) {
            a.i = Error;
            if (error_msg != NULL) {
                a.s = g_strdup_printf("%s", error_msg);
                g_free(error_msg);
                error_msg = NULL;
            } else {
                a.s = g_strdup_printf("Unknown error. Please file a bug report!");
            }
            echo(&a);
        }
    } else if (length > 1 && ((forward = text[0] == '/') || text[0] == '?')) {
        webkit_web_view_unmark_text_matches(webview);
#ifdef ENABLE_MATCH_HIGHLITING
        webkit_web_view_mark_text_matches(webview, &text[1], FALSE, 0);
        webkit_web_view_set_highlight_text_matches(webview, TRUE);
#endif
        count = 0;
#ifndef ENABLE_INCREMENTAL_SEARCH
        a.s =& text[1];
        a.i = searchoptions | (forward ? DirectionForward : DirectionBackwards);
        search(&a);
#else
        search_direction = forward;
        search_handle = g_strdup(&text[1]);
#endif
    } else if (count && (text[0] == '`' || text[0] == '~')) {
        a.i = Silent;
        a.s = g_strdup_printf("vimprobable_fire(%d)", count);
        script(&a);
        update_state();
    } else
        return;
    if (!echo_active)
        gtk_entry_set_text(entry, "");
    gtk_widget_grab_focus(GTK_WIDGET(webview));
}

gboolean
inputbox_keypress_cb(GtkEntry *entry, GdkEventKey *event) {
    Arg a;
    int numval;

     switch (event->keyval) {
         case GDK_Escape:
             a.i = HideCompletion;
             complete(&a);
             a.i = ModeNormal;
             return set(&a);
         break;
         case GDK_Tab:
             a.i = DirectionNext;
             return complete(&a);
         break;
         case GDK_ISO_Left_Tab:
             a.i = DirectionPrev;
             return complete(&a);
         break;
    }

    numval = g_unichar_digit_value((gunichar) gdk_keyval_to_unicode(event->keyval));
    if (followTarget[0] && ((numval >= 1 && numval <= 9) || (numval == 0 && count))) {
        /* allow a zero as non-first number */
        count = (count ? count * 10 : 0) + numval;
        a.i = Silent;
        a.s = g_strdup_printf("vimprobable_update_hints(%d)", count);
        script(&a);
        update_state();
        return TRUE;
    }

    return FALSE;
}

gboolean
notify_event_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    int i;
     if (mode == ModeNormal && event->type == GDK_BUTTON_RELEASE) {
        /* handle mouse click events */
        for (i = 0; i < LENGTH(mouse); i++) {
            if (mouse[i].mask == CLEAN(event->button.state)
                    && (mouse[i].modkey == current_modkey
                        || (!mouse[i].modkey && !current_modkey)
                        || mouse[i].modkey == GDK_VoidSymbol)    /* wildcard */
                    && mouse[i].button == event->button.button
                    && mouse[i].func) {
                if (mouse[i].func(&mouse[i].arg)) {
                    current_modkey = count = 0;
                    update_state();
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

static gboolean inputbox_keyrelease_cb(GtkEntry *entry, GdkEventKey *event) {
    Arg a;
    guint16 length = gtk_entry_get_text_length(entry);

    if (!length) {
        a.i = HideCompletion;
        complete(&a);
        a.i = ModeNormal;
        return set(&a);
    }
    return FALSE;
}

static gboolean inputbox_changed_cb(GtkEditable *entry, gpointer user_data) {
    Arg a;
    char *text = (char*)gtk_entry_get_text(GTK_ENTRY(entry));
    guint16 length = gtk_entry_get_text_length(GTK_ENTRY(entry));
    gboolean forward = FALSE;

    /* Update incremental search if the user changes the search text.
     *
     * Note: gtk_widget_is_focus() is a poor way to check if the change comes
     *       from the user. But if the entry is focused and the text is set
     *       through gtk_entry_set_text() in some asyncrounous operation,
     *       I would consider that a bug.
     */

    if (gtk_widget_is_focus(GTK_WIDGET(entry)) && length > 1 && ((forward = text[0] == '/') || text[0] == '?')) {
        webkit_web_view_unmark_text_matches(webview);
        webkit_web_view_search_text(webview, &text[1], searchoptions & CaseSensitive, forward, searchoptions & Wrapping);
        return TRUE;
    } else if (gtk_widget_is_focus(GTK_WIDGET(entry)) && length >= 1 &&
            (text[0] == '`' || text[0] == '~')) {
        a.i = Silent;
        a.s = g_strdup("vimprobable_cleanup()");
        script(&a);

        a.i = Silent;
        a.s = g_strconcat("vimprobable_show_hints('", text + 1, "')", NULL);
        script(&a);

        return TRUE;
    } else if (length == 0 && followTarget[0]) {
        memset(followTarget, 0, 8);
        a.i = Silent;
        a.s = g_strdup("vimprobable_clear()");
        script(&a);
        count = 0;
        update_state();
    }

    return FALSE;
}

/* funcs here */
void fill_suggline(char * suggline, const char * command,  const char *fill_with) {
    memset(suggline, 0, 512);
    strncpy(suggline, command, 512);
    strncat(suggline, " ", 1);
    strncat(suggline, fill_with, 512 - strlen(suggline) - 1);
}

GtkWidget * fill_eventbox(const char * completion_line) {
    GtkBox    * row;
    GtkWidget *row_eventbox, *el;
    GdkColor  color;
    char      * markup;

    row = GTK_BOX(gtk_hbox_new(FALSE, 0));
    row_eventbox = gtk_event_box_new();
    gdk_color_parse(completionbgcolor[0], &color);
    gtk_widget_modify_bg(row_eventbox, GTK_STATE_NORMAL, &color);
    el = gtk_label_new(NULL);
    markup = g_strconcat("<span font=\"", completionfont[0], "\" color=\"", completioncolor[0], "\">",
        g_markup_escape_text(completion_line, strlen(completion_line)), "</span>", NULL);
    gtk_label_set_markup(GTK_LABEL(el), markup);
    g_free(markup);
    gtk_misc_set_alignment(GTK_MISC(el), 0, 0);
    gtk_box_pack_start(row, el, TRUE, TRUE, 2);
    gtk_container_add(GTK_CONTAINER(row_eventbox), GTK_WIDGET(row));
    return row_eventbox;
}

gboolean
complete(const Arg *arg) {
    char *str, *p, *s, *markup, *entry, *searchfor, command[32] = "", suggline[512] = "", **suggurls = NULL;
    size_t listlen, len, cmdlen;
    int i, spacepos;
    Listelement *elementlist = NULL, *elementpointer;
    gboolean highlight = FALSE;
    GtkBox *row;
    GtkWidget *row_eventbox, *el;
    GtkBox *_table;
    GdkColor color;
    static GtkWidget *table, **widgets, *top_border;
    static char **suggestions, *prefix;
    static int n = 0, m, current = -1;

    str = (char*)gtk_entry_get_text(GTK_ENTRY(inputbox));
    len = strlen(str);
    if ((len == 0 || str[0] != ':') && arg->i != HideCompletion)
        return TRUE;
    if (prefix) {
        if (arg->i != HideCompletion && widgets && current != -1 && !strcmp(&str[1], suggestions[current])) {
            gdk_color_parse(completionbgcolor[0], &color);
            gtk_widget_modify_bg(widgets[current], GTK_STATE_NORMAL, &color);
            current = (n + current + (arg->i == DirectionPrev ? -1 : 1)) % n;
            if ((arg->i == DirectionNext && current == 0)
            || (arg->i == DirectionPrev && current == n - 1))
                current = -1;
        } else {
            free(widgets);
            free(suggestions);
            free(prefix);
            gtk_widget_destroy(GTK_WIDGET(table));
            gtk_widget_destroy(GTK_WIDGET(top_border));
            table = NULL;
            widgets = NULL;
            suggestions = NULL;
            prefix = NULL;
            n = 0;
            current = -1;
            if (arg->i == HideCompletion)
                return TRUE;
        }
    } else if (arg->i == HideCompletion)
        return TRUE;
    if (!widgets) {
        prefix = g_strdup_printf(str);
        widgets = malloc(sizeof(GtkWidget*) * MAX_LIST_SIZE);
        suggestions = malloc(sizeof(char*) * MAX_LIST_SIZE);
        top_border = gtk_event_box_new();
        gtk_widget_set_size_request(GTK_WIDGET(top_border), 0, 1);
        gdk_color_parse(completioncolor[2], &color);
        gtk_widget_modify_bg(top_border, GTK_STATE_NORMAL, &color);
        table = gtk_event_box_new();
        gdk_color_parse(completionbgcolor[0], &color);
        _table = GTK_BOX(gtk_vbox_new(FALSE, 0));
        highlight = len > 1;
        if (strchr(str, ' ') == NULL) {
            /* command completion */
            listlen = LENGTH(commands);
            for (i = 0; i < listlen; i++) {
                cmdlen = strlen(commands[i].cmd);
                if (!highlight || (n < MAX_LIST_SIZE && len - 1 <= cmdlen && !strncmp(&str[1], commands[i].cmd, len - 1))) {
                    p = s = malloc(sizeof(char*) * (highlight ? sizeof(COMPLETION_TAG_OPEN) + sizeof(COMPLETION_TAG_CLOSE) - 1 : 1) + cmdlen);
                    if (highlight) {
                        memcpy(p, COMPLETION_TAG_OPEN, sizeof(COMPLETION_TAG_OPEN) - 1);
                        memcpy((p += sizeof(COMPLETION_TAG_OPEN) - 1), &str[1], len - 1);
                        memcpy((p += len - 1), COMPLETION_TAG_CLOSE, sizeof(COMPLETION_TAG_CLOSE) - 1);
                        p += sizeof(COMPLETION_TAG_CLOSE) - 1;
                    }
                    memcpy(p, &commands[i].cmd[len - 1], cmdlen - len + 2);
                    row = GTK_BOX(gtk_hbox_new(FALSE, 0));
                    row_eventbox = gtk_event_box_new();
                    gtk_widget_modify_bg(row_eventbox, GTK_STATE_NORMAL, &color);
                    el = gtk_label_new(NULL);
                    markup = g_strconcat("<span font=\"", completionfont[0], "\" color=\"", completioncolor[0], "\">", s, "</span>", NULL);
                    free(s);
                    gtk_label_set_markup(GTK_LABEL(el), markup);
                    g_free(markup);
                    gtk_misc_set_alignment(GTK_MISC(el), 0, 0);
                    gtk_box_pack_start(row, el, TRUE, TRUE, 2);
                    gtk_container_add(GTK_CONTAINER(row_eventbox), GTK_WIDGET(row));
                    gtk_box_pack_start(_table, GTK_WIDGET(row_eventbox), FALSE, FALSE, 0);
                    suggestions[n] = commands[i].cmd;
                    widgets[n++] = row_eventbox;
                }
            }
        } else {
            entry = (char *)malloc(512 * sizeof(char));
            if (entry == NULL) {
                return FALSE;
            }
            memset(entry, 0, 512);
            suggurls = malloc(sizeof(char*) * MAX_LIST_SIZE);
            if (suggurls == NULL) {
                return FALSE;
            }
            spacepos = strcspn(str, " ");
            searchfor = (str + spacepos + 1);
            strncpy(command, (str + 1), spacepos - 1);
            if (strlen(command) == 2 && strncmp(command, "qt", 2) == 0) {
                /* completion on tags */
                spacepos = strcspn(str, " ");
                searchfor = (str + spacepos + 1);
                elementlist = complete_list(searchfor, 1, elementlist);
            } else {
                /* URL completion: bookmarks */
                elementlist = complete_list(searchfor, 0, elementlist);
                m = count_list(elementlist);
                if (m < MAX_LIST_SIZE) {
                    /* URL completion: history */
                    elementlist = complete_list(searchfor, 2, elementlist);
                }
            }
            elementpointer = elementlist;
            while (elementpointer != NULL) {
                fill_suggline(suggline, command, elementpointer->element);
                suggurls[n] = (char *)malloc(sizeof(char) * 512 + 1);
                strncpy(suggurls[n], suggline, 512);
                suggestions[n] = suggurls[n];
                row_eventbox = fill_eventbox(suggline);
                gtk_box_pack_start(_table, GTK_WIDGET(row_eventbox), FALSE, FALSE, 0);
                widgets[n++] = row_eventbox;
                elementpointer = elementpointer->next;
                if (n >= MAX_LIST_SIZE)
                    break;
            }
            free_list(elementlist);
            if (suggurls != NULL) {
                free(suggurls);
                suggurls = NULL;
            }
            if (entry != NULL) {
                free(entry);
                entry = NULL;
            }
        }
        widgets = realloc(widgets, sizeof(GtkWidget*) * n);
        suggestions = realloc(suggestions, sizeof(char*) * n);
        if (!n) {
            gdk_color_parse(completionbgcolor[1], &color);
            gtk_widget_modify_bg(table, GTK_STATE_NORMAL, &color);
            el = gtk_label_new(NULL);
            gtk_misc_set_alignment(GTK_MISC(el), 0, 0);
            markup = g_strconcat("<span font=\"", completionfont[1], "\" color=\"", completioncolor[1], "\">No Completions</span>", NULL);
            gtk_label_set_markup(GTK_LABEL(el), markup);
            g_free(markup);
            gtk_box_pack_start(_table, GTK_WIDGET(el), FALSE, FALSE, 0);
        }
        gtk_box_pack_start(box, GTK_WIDGET(top_border), FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(table), GTK_WIDGET(_table));
        gtk_box_pack_start(box, GTK_WIDGET(table), FALSE, FALSE, 0);
        gtk_widget_show_all(GTK_WIDGET(window));
        if (!n)
            return TRUE;
        current = arg->i == DirectionPrev ? n - 1 : 0;
    }
    if (current != -1) {
        gdk_color_parse(completionbgcolor[2], &color);
        gtk_widget_modify_bg(GTK_WIDGET(widgets[current]), GTK_STATE_NORMAL, &color);
        s = g_strconcat(":", suggestions[current], NULL);
        gtk_entry_set_text(GTK_ENTRY(inputbox), s);
        g_free(s);
    } else
        gtk_entry_set_text(GTK_ENTRY(inputbox), prefix);
    gtk_editable_set_position(GTK_EDITABLE(inputbox), -1);
    return TRUE;
}

gboolean
descend(const Arg *arg) {
    char *source = (char*)webkit_web_view_get_uri(webview), *p = &source[0], *new;
    int i, len;
    count = count ? count : 1;

    if (!source)
        return TRUE;
    if (arg->i == Rootdir) {
        for(i = 0; i < 3; i++)                  /* get to the third slash */
            if (!(p = strchr(++p, '/')))
                return TRUE;                    /* if we cannot find it quit */
    } else {
        len = strlen(source);
        if (!len)                                /* if string is empty quit */
            return TRUE;
        p = source + len;                       /* start at the end */
        if (*(p - 1) == '/')                     /* /\/$/ is not an additional level */
            ++count;
        for(i = 0; i < count; i++)
            while(*(p--) != '/' || *p == '/')   /* count /\/+/ as one slash */
                if (p == source)                 /* if we reach the first char pointer quit */
                    return TRUE;
        ++p;                                    /* since we do p-- in the while, we are pointing at
                                                   the char before the slash, so +1  */
    }
    len =  p - source + 1;                      /* new length = end - start + 1 */
    new = malloc(len + 1);
    memcpy(new, source, len);
    new[len] = '\0';
    webkit_web_view_load_uri(webview, new);
    free(new);
    return TRUE;
}

gboolean
echo(const Arg *arg) {
    PangoFontDescription *font;
    GdkColor color;
    int index = !arg->s ? 0 : arg->i & (~NoAutoHide);

    if (index < Info || index > Error)
        return TRUE;
    font = pango_font_description_from_string(urlboxfont[index]);
    gtk_widget_modify_font(inputbox, font);
    pango_font_description_free(font);
    if (urlboxcolor[index])
        gdk_color_parse(urlboxcolor[index], &color);
    gtk_widget_modify_text(inputbox, GTK_STATE_NORMAL, urlboxcolor[index] ? &color : NULL);
    if (urlboxbgcolor[index])
        gdk_color_parse(urlboxbgcolor[index], &color);
    gtk_widget_modify_base(inputbox, GTK_STATE_NORMAL, urlboxbgcolor[index] ? &color : NULL);
    gtk_entry_set_text(GTK_ENTRY(inputbox), !arg->s ? "" : arg->s);
	/* TA:  Always free arg->s here, rather than relying on the caller to do
	 * this.
	 */
    if (arg->s)
        g_free(arg->s);
    return TRUE;
}

gboolean
input(const Arg *arg) {
    int pos = 0;
    count = 0;
    const char *url;
    Arg a;

    update_state();

    /* Set the colour and font back to the default, so that we don't still
     * maintain a red colour from a warning from an end of search indicator,
     * etc.
     *
     * XXX - unify this with echo() at some point.
     */
    {
        GdkColor ibox_fg_color;
        GdkColor ibox_bg_color;
        PangoFontDescription *font;
        int index = Info;

        font = pango_font_description_from_string(urlboxfont[index]);
        gtk_widget_modify_font(inputbox, font);
        pango_font_description_free(font);

        if (urlboxcolor[index])
            gdk_color_parse(urlboxcolor[index], &ibox_fg_color);
        if (urlboxbgcolor[index])
            gdk_color_parse(urlboxbgcolor[index], &ibox_bg_color);

        gtk_widget_modify_text(inputbox, GTK_STATE_NORMAL, urlboxcolor[index] ? &ibox_fg_color : NULL);
        gtk_widget_modify_base(inputbox, GTK_STATE_NORMAL, urlboxbgcolor[index] ? &ibox_bg_color : NULL);
    }

    if (arg->s[0] == '`' || arg->s[0] == '~') {
        memset(followTarget, 0, 0);
        strncpy(followTarget, arg->s[0] == '`' ? "current" : "new", 8);
        a.i = Silent;
        a.s = g_strdup("vimprobable_show_hints()");
        script(&a);
    }

    /* to avoid things like :open URL :open URL2  or :open :open URL */
    gtk_entry_set_text(GTK_ENTRY(inputbox), "");
    gtk_editable_insert_text(GTK_EDITABLE(inputbox), arg->s, -1, &pos);
    if (arg->i & InsertCurrentURL && (url = webkit_web_view_get_uri(webview)))
        gtk_editable_insert_text(GTK_EDITABLE(inputbox), url, -1, &pos);
    gtk_widget_grab_focus(inputbox);
    gtk_editable_set_position(GTK_EDITABLE(inputbox), -1);

    return TRUE;
}

gboolean
navigate(const Arg *arg) {
    if (arg->i & NavigationForwardBack)
        webkit_web_view_go_back_or_forward(webview, (arg->i == NavigationBack ? -1 : 1) * (count ? count : 1));
    else if (arg->i & NavigationReloadActions)
        (arg->i == NavigationReload ? webkit_web_view_reload : webkit_web_view_reload_bypass_cache)(webview);
    else
        webkit_web_view_stop_loading(webview);
    return TRUE;
}

gboolean
number(const Arg *arg) {
    const char *source = webkit_web_view_get_uri(webview);
    char *uri, *p, *new;
    int number, diff = (count ? count : 1) * (arg->i == Increment ? 1 : -1);

    if (!source)
        return TRUE;
    uri = g_strdup_printf(source); /* copy string */
    p =& uri[0];
    while(*p != '\0') /* goto the end of the string */
        ++p;
    --p;
    while(*p >= '0' && *p <= '9') /* go back until non number char is reached */
        --p;
    if (*(++p) == '\0') { /* if no numbers were found abort */
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
open_arg(const Arg *arg) {
    char *argv[6];
    char *s = arg->s, *p, *new;
    Arg a = { .i = NavigationReload };
    int len, i;

    if (embed) {
        argv[0] = *args;
        argv[1] = "-e";
        argv[2] = winid;
        argv[3] = arg->s;
        argv[4] = NULL;
    } else {
        argv[0] = *args;
        argv[1] = arg->s;
        argv[2] = NULL;
    }

    if (!arg->s)
        navigate(&a);
    else if (arg->i == TargetCurrent) {
        len = strlen(arg->s);
        new = NULL, p = strchr(arg->s, ' ');
        if (p)                                                           /* check for search engines */
            for(i = 0; i < LENGTH(searchengines); i++)
                if (!strncmp(arg->s, searchengines[i].handle, p - arg->s)) {
                    p = soup_uri_encode(++p, "&");
                    new = g_strdup_printf(searchengines[i].uri, p);
                    g_free(p);
                    break;
                }
        if (!new) {
            if (len > 3 && strstr(arg->s, "://")) {                      /* valid url? */
                p = new = g_malloc(len + 1);
                while(*s != '\0') {                                     /* strip whitespaces */
                    if (*s != ' ')
                        *(p++) = *s;
                    ++s;
                }
                *p = '\0';
            } else if (strcspn(arg->s, "/") == 0 || strcspn(arg->s, "./") == 0) {  /* prepend "file://" */
                new = g_malloc(sizeof("file://") + len);
                strcpy(new, "file://");
                memcpy(&new[sizeof("file://") - 1], arg->s, len + 1);
            } else if (p || !strchr(arg->s, '.')) {                      /* whitespaces or no dot? */
                p = soup_uri_encode(arg->s, "&");
                new = g_strdup_printf(defsearch->uri, p);
                g_free(p);
            } else {                                                    /* prepend "http://" */
                new = g_malloc(sizeof("http://") + len);
                strcpy(new, "http://");
                memcpy(&new[sizeof("http://") - 1], arg->s, len + 1);
            }
        }
        webkit_web_view_load_uri(webview, new);
        g_free(new);
    } else
        g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
    return TRUE;
}

gboolean
yank(const Arg *arg) {
    const char *url, *feedback;

    if (arg->i & SourceURL) {
        url = webkit_web_view_get_uri(webview);
        if (!url)
            return TRUE;
        feedback = g_strconcat("Yanked ", url, NULL);
        give_feedback(feedback);
        if (arg->i & ClipboardPrimary)
            gtk_clipboard_set_text(clipboards[0], url, -1);
        if (arg->i & ClipboardGTK)
            gtk_clipboard_set_text(clipboards[1], url, -1);
    } else
        webkit_web_view_copy_clipboard(webview);
    return TRUE;
}

gboolean
paste(const Arg *arg) {
    Arg a = { .i = arg->i & TargetNew, .s = NULL };

    /* If we're over a link, open it in a new target. */
    if (strlen(rememberedURI) > 0) {
        Arg new_target = { .i = TargetNew, .s = arg->s };
        open_arg(&new_target);
        return TRUE;
    }

    if (arg->i & ClipboardPrimary)
        a.s = gtk_clipboard_wait_for_text(clipboards[0]);
    if (!a.s && arg->i & ClipboardGTK)
        a.s = gtk_clipboard_wait_for_text(clipboards[1]);
    if (a.s)
        open_arg(&a);
    return TRUE;
}

gboolean
quit(const Arg *arg) {
    FILE *f;
    const char *filename;
    const char *uri = webkit_web_view_get_uri(webview);
    if (uri != NULL) {
        /* write last URL into status file for recreation with "u" */
        filename = g_strdup_printf(CLOSED_URL_FILENAME);
        f = fopen(filename, "w");
        if (f != NULL) {
            fprintf(f, "%s", uri);
            fclose(f);
        }
    }
    gtk_main_quit();
    return TRUE;
}

gboolean
revive(const Arg *arg) {
    FILE *f;
    const char *filename;
    char buffer[512] = "";
    Arg a = { .i = TargetNew, .s = NULL };
    /* get the URL of the window which has been closed last */
    filename = g_strdup_printf(CLOSED_URL_FILENAME);
    f = fopen(filename, "r");
    if (f != NULL) {
        fgets(buffer, 512, f);
        fclose(f);
    }
    if (strlen(buffer) > 0) {
        a.s = buffer;
        open_arg(&a);
        return TRUE;
    }
    return FALSE;
}

static 
gboolean print_frame(const Arg *arg)
{
    WebKitWebFrame *frame = webkit_web_view_get_main_frame(webview);
    webkit_web_frame_print (frame);
    return TRUE;
}

gboolean
search(const Arg *arg) {
    count = count ? count : 1;
    gboolean success, direction = arg->i & DirectionPrev;
    Arg a;

    if (arg->s) {
        free(search_handle);
        search_handle = g_strdup_printf(arg->s);
    }
    if (!search_handle)
        return TRUE;
    if (arg->i & DirectionAbsolute)
        search_direction = direction;
    else
        direction ^= search_direction;
    do {
        success = webkit_web_view_search_text(webview, search_handle, arg->i & CaseSensitive, direction, FALSE);
        if (!success) {
            if (arg->i & Wrapping) {
                success = webkit_web_view_search_text(webview, search_handle, arg->i & CaseSensitive, direction, TRUE);
                if (success) {
                    a.i = Warning;
                    a.s = g_strdup_printf("search hit %s, continuing at %s",
                            direction ? "BOTTOM" : "TOP",
                            direction ? "TOP" : "BOTTOM");
                    echo(&a);
                } else
                    break;
            } else
                break;
        }
    } while(--count);
    if (!success) {
        a.i = Error;
        a.s = g_strdup_printf("Pattern not found: %s", search_handle);
        echo(&a);
    }
    return TRUE;
}

gboolean
set(const Arg *arg) {
    Arg a = { .i = Info | NoAutoHide };

    switch (arg->i) {
    case ModeNormal:
        if (search_handle) {
            search_handle = NULL;
            webkit_web_view_unmark_text_matches(webview);
        }
        gtk_entry_set_text(GTK_ENTRY(inputbox), "");
        gtk_widget_grab_focus(GTK_WIDGET(webview));
        break;
    case ModePassThrough:
        a.s = g_strdup("-- PASS THROUGH --");
        echo(&a);
        break;
    case ModeSendKey:
        a.s = g_strdup("-- PASS TROUGH (next) --");
        echo(&a);
        break;
    case ModeInsert: /* should not be called manually but automatically */
        a.s = g_strdup("-- INSERT --");
        echo(&a);
        break;
    default:
        return TRUE;
    }
    mode = arg->i;
    return TRUE;
}

gchar*
jsapi_ref_to_string(JSContextRef context, JSValueRef ref) {
    JSStringRef string_ref;
    gchar *string;
    size_t length;

    string_ref = JSValueToStringCopy(context, ref, NULL);
    length = JSStringGetMaximumUTF8CStringSize(string_ref);
    string = g_new(gchar, length);
    JSStringGetUTF8CString(string_ref, string, length);
    JSStringRelease(string_ref);
    return string;
}

void
jsapi_evaluate_script(const gchar *script, gchar **value, gchar **message) {
    WebKitWebFrame *frame = webkit_web_view_get_main_frame(webview);
    JSGlobalContextRef context = webkit_web_frame_get_global_context(frame);
    JSStringRef str;
    JSValueRef val, exception;

    str = JSStringCreateWithUTF8CString(script);
    val = JSEvaluateScript(context, str, JSContextGetGlobalObject(context), NULL, 0, &exception);
    JSStringRelease(str);
    if (!val)
        *message = jsapi_ref_to_string(context, exception);
    else
        *value = jsapi_ref_to_string(context, val);
}

gboolean
script(const Arg *arg) {
    gchar *value = NULL, *message = NULL;
    Arg a;

    if (!arg->s) {
        set_error("Missing argument.");
        return FALSE;
    }
    jsapi_evaluate_script(arg->s, &value, &message);
    if (message) {
        set_error(message);
	if (arg->s)
	    g_free(arg->s);
        return FALSE;
    }
    if (arg->i != Silent && value) {
        a.i = arg->i;
        a.s = g_strdup(value);
        echo(&a);
    }
    if (value) {
        if (strncmp(value, "fire;", 5) == 0) {
        	count = 0;
            a.s = g_strconcat("vimprobable_fire(", (value + 5), ")", NULL);
            a.i = Silent;
            script(&a);
        } else if (strncmp(value, "open;", 5) == 0) {
            count = 0;
            a.i = ModeNormal;
            set(&a);
            if (strncmp(followTarget, "new", 3) == 0)
                a.i = TargetNew;
            else
                a.i = TargetCurrent;
            memset(followTarget, 0, 8);
            a.s = (value + 5);
            open_arg(&a);
        }
    }
    if (arg->s)
        g_free(arg->s);
    g_free(value);
    return TRUE;
}

gboolean
scroll(const Arg *arg) {
    GtkAdjustment *adjust = (arg->i & OrientationHoriz) ? adjust_h : adjust_v;
    int max = gtk_adjustment_get_upper(adjust) - gtk_adjustment_get_page_size(adjust);
    float val = gtk_adjustment_get_value(adjust) / max * 100;
    int direction = (arg->i & (1 << 2)) ? 1 : -1;
    
    if ((direction == 1 && val < 100) || (direction == -1 && val > 0)) {
        if (arg->i & ScrollMove)
            gtk_adjustment_set_value(adjust, gtk_adjustment_get_value(adjust) +
                direction *      /* direction */
                ((arg->i & UnitLine || (arg->i & UnitBuffer && count)) ? (scrollstep * (count ? count : 1)) : (
                    arg->i & UnitBuffer ? gtk_adjustment_get_page_size(adjust) / 2 :
                    (count ? count : 1) * (gtk_adjustment_get_page_size(adjust) -
                        (gtk_adjustment_get_page_size(adjust) > pagingkeep ? pagingkeep : 0)))));
        else
            gtk_adjustment_set_value(adjust,
                ((direction == 1) ?  gtk_adjustment_get_upper : gtk_adjustment_get_lower)(adjust));
        update_state();
    }
    return TRUE;
}

gboolean
zoom(const Arg *arg) {
    webkit_web_view_set_full_content_zoom(webview, (arg->i & ZoomFullContent) > 0);
    webkit_web_view_set_zoom_level(webview, (arg->i & ZoomOut) ?
        webkit_web_view_get_zoom_level(webview) +
            (((float)(count ? count : 1)) * (arg->i & (1 << 1) ? 1.0 : -1.0) * zoomstep) :
        (count ? (float)count / 100.0 : 1.0));
    return TRUE;
}

gboolean 
quickmark(const Arg *a) {
    int i, b;
    b = atoi(a->s);
    char *fn = strcat(QUICKMARK_FILE);
    FILE *fp;
    fp = fopen(fn, "r");
    char buf[100];
 
    if (fp != NULL && b < 10) {
       for( i=0; i < b; ++i ) {
           if (feof(fp)) {
               break;
           }
           fgets(buf, 100, fp);
       }
       char *ptr = strrchr(buf, '\n');
       *ptr = '\0';
       Arg x = { .s = buf };
       return open_arg(&x);
    }
    else { return false; }
}

gboolean 
fake_key_event(const Arg *a) {
    if(!embed) {
        return FALSE;
    }
    Arg err;
    err.i = Error;
    Display *xdpy;
    if ( (xdpy = XOpenDisplay(NULL)) == NULL ) {
        err.s = g_strdup("Couldn't find the XDisplay.");
        echo(&err);
        return FALSE;
    }
       
    XKeyEvent xk;
    xk.display = xdpy;
    xk.subwindow = None;
    xk.time = CurrentTime;
    xk.same_screen = True;
    xk.x = xk.y = xk.x_root = xk.y_root = 1;
    xk.window = embed;
    xk.state =  a->i;

    if( ! a->s ) {
        err.s = g_strdup("Zero pointer as argument! Check your config.h");
        echo(&err);
        return FALSE;
    }

    KeySym keysym;
    if( (keysym = XStringToKeysym(a->s)) == NoSymbol ) {
        err.s = g_strdup_printf("Couldn't translate %s to keysym", a->s );
        echo(&err);
        return FALSE;
    }
    
    if( (xk.keycode = XKeysymToKeycode(xdpy, keysym)) == NoSymbol ) {
        err.s = g_strdup("Couldn't translate keysym to keycode");
        echo(&err);
        return FALSE;
    }
   
    xk.type = KeyPress;
    if( !XSendEvent(xdpy, embed, True, KeyPressMask, (XEvent *)&xk) ) {
        err.s = g_strdup("XSendEvent failed");
        echo(&err);
        return FALSE;
    }
    XFlush(xdpy);

    return TRUE;
}

gboolean
toggle_plugins(const Arg *arg) {
    static gboolean plugins;
    WebKitWebSettings *settings;
    settings = webkit_web_view_get_settings(webview);
    plugins = !plugins;
    g_object_set((GObject*)settings, "enable-plugins", plugins, NULL);
    g_object_set((GObject*)settings, "enable-scripts", plugins, NULL);
    webkit_web_view_set_settings(webview, settings);
    webkit_web_view_reload(webview);
    return TRUE;
}

gboolean
toggle_images(const Arg *arg) {
    static gboolean images;
    WebKitWebSettings *settings;
    settings = webkit_web_view_get_settings(webview);
    images = !images;
    g_object_set((GObject*)settings, "auto-load-images", images, NULL);
    webkit_web_view_set_settings(webview, settings);
    webkit_web_view_reload(webview);
    return TRUE;
}

gboolean
bookmark(const Arg *arg) {
    FILE *f;
    const char *filename;
    const char *uri = webkit_web_view_get_uri(webview);
    const char *title = webkit_web_view_get_title(webview);
    filename = g_strdup_printf(BOOKMARKS_STORAGE_FILENAME);
    f = fopen(filename, "a");
    if (uri == NULL || strlen(uri) == 0) {
        set_error("No URI found to bookmark.");
        return FALSE;
    }
    if (f != NULL) {
        fprintf(f, "%s", uri);
        if (title != NULL) {
            fprintf(f, "%s", " ");
            fprintf(f, "%s", title);
        }
        if (arg->s && strlen(arg->s)) {
            build_taglist(arg, f);
        }
        fprintf(f, "%s", "\n");
        fclose(f);
        give_feedback( "Bookmark saved" );
        return TRUE;
    } else {
    	set_error("Bookmarks file not found.");
        return FALSE;
    }
}

void history() {
    FILE *f;
    const char *filename;
    const char *uri = webkit_web_view_get_uri(webview);
    const char *title = webkit_web_view_get_title(webview);
    char *entry, buffer[512], *new;
    int n, i = 0;
    gboolean finished = FALSE;
    if (uri != NULL) {
        if (title != NULL) {
            entry = malloc((strlen(uri) + strlen(title) + 2) * sizeof(char));
            memset(entry, 0, strlen(uri) + strlen(title) + 2);
        } else {
            entry = malloc((strlen(uri) + 1) * sizeof(char));
            memset(entry, 0, strlen(uri) + 1);
        }
        if (entry != NULL) {
            strncpy(entry, uri, strlen(uri));
            if (title != NULL) {
                strncat(entry, " ", 1);
                strncat(entry, title, strlen(title));
            }
            n = strlen(entry);
            filename = g_strdup_printf(HISTORY_STORAGE_FILENAME);
            f = fopen(filename, "r");
            if (f != NULL) {
                new = (char *)malloc(HISTORY_MAX_ENTRIES * 512 * sizeof(char) + 1);
                if (new != NULL) {
                    memset(new, 0, HISTORY_MAX_ENTRIES * 512 * sizeof(char) + 1);
                    /* newest entries go on top */
                    strncpy(new, entry, strlen(entry));
                    strncat(new, "\n", 1);
                    /* retain at most HISTORY_MAX_ENTIRES - 1 old entries */
                    while (finished != TRUE) {
                        if ((char *)NULL == fgets(buffer, 512, f)) {
                            /* check if end of file was reached / error occured */
                            if (!feof(f)) {
                                break;
                            }
                            /* end of file reached */
                            finished = TRUE;
                            continue;
                        }
                        /* compare line (-1 because of newline character) */
                        if (n != strlen(buffer) - 1 || strncmp(entry, buffer, n) != 0) {
                            /* if the URI is already in history; we put it on top and skip it here */
                            strncat(new, buffer, 512);
                            i++;
                        }
                        if ((i + 1) >= HISTORY_MAX_ENTRIES) {
                            break;
                        }
                    }
                    fclose(f);
                }
                f = fopen(filename, "w");
                if (f != NULL) {
                    fprintf(f, "%s", new);
                    fclose(f);
                }
                if (new != NULL) {
                    free(new);
                    new = NULL;
                }
            }
        }
        if (entry != NULL) {
            free(entry);
            entry = NULL;
        }
    }
}

static gboolean
view_source(const Arg * arg) {
    gboolean current_mode = webkit_web_view_get_view_source_mode (webview);
    webkit_web_view_set_view_source_mode (webview, !current_mode);
    webkit_web_view_reload (webview);
    return TRUE;
}

static gboolean
focus_input(const Arg *arg) {
    static Arg a;

    a.s = g_strconcat("vimprobable_focus_input()", NULL);
    a.i = Silent;
    script(&a);
    update_state();
    return TRUE;
}

static gboolean
search_tag(const Arg * a) {
    FILE *f;
    const char *filename;
    const char *tag = a->s;
    char s[BUFFERSIZE], foundtag[40], url[BUFFERSIZE];
    int t, i, intag, k;

    if (!tag) {
	    /* The user must give us something to load up. */
	    set_error("Bookmark tag required with this option.");
	    return FALSE;
    }

    if (strlen(tag) > MAXTAGSIZE) {
        set_error("Tag too long.");
        return FALSE;
    }

    filename = g_strdup_printf(BOOKMARKS_STORAGE_FILENAME);
    f = fopen(filename, "r");
    if (f == NULL) {
        set_error("Couldn't open bookmarks file.");
        return FALSE;
    }
    while (fgets(s, BUFFERSIZE-1, f)) {
        intag = 0;
        t = strlen(s) - 1;
        while (isspace(s[t]))
            t--;
        if (s[t] != ']') continue;      
        while (t > 0) {
            if (s[t] == ']') {
                if (!intag)
                    intag = t;
                else
                    intag = 0;
            } else {
                if (s[t] == '[') {
                    if (intag) {
                        i = 0;
                        k = t + 1;
                        while (k < intag)
                            foundtag[i++] = s[k++];
                        foundtag[i] = '\0';
                        /* foundtag now contains the tag */	
                        if (strlen(foundtag) < MAXTAGSIZE && strcmp(tag, foundtag) == 0) {
                            i = 0;
                            while (isspace(s[i])) i++;
                            k = 0;
                            while (s[i] && !isspace(s[i])) url[k++] = s[i++];
                            url[k] = '\0';
                            Arg x = { .i = TargetNew, .s = url };
                           open_arg(&x);
                        }
                    }
                    intag = 0;
                }
            }
            t--;
        }
    }
    return TRUE;
}

gboolean
build_taglist(const Arg *arg, FILE *f) {
    int k = 0, in_tag = 0;
    int t = 0, marker = 0;
    char foundtab[MAXTAGSIZE+1];
    while (arg->s[k]) {
        if (!isspace(arg->s[k]) && !in_tag) {
            in_tag = 1;
            marker = k;
        }
        if (isspace(arg->s[k]) && in_tag) {
            /* found a tag */
            t = 0;
            while (marker < k && t < MAXTAGSIZE) foundtab[t++] = arg->s[marker++];
            foundtab[t] = '\0';
            fprintf(f, " [%s]", foundtab);
            in_tag = 0;
        }
        k++;
    }
    if (in_tag) {
        t = 0;
        while (marker < strlen(arg->s) && t < MAXTAGSIZE) foundtab[t++] = arg->s[marker++];
        foundtab[t] = '\0';
        fprintf(f, " [%s]", foundtab );
    }
    return TRUE;
}

void
set_error(const char *error) {
    /* it should never happen that set_error is called more than once, 
     * but to avoid any potential memory leaks, we ignore any subsequent 
     * error if the current one has not been shown */
    if (error_msg == NULL) {
        error_msg = g_strdup_printf("%s", error);
    }
}

void 
give_feedback(const char *feedback) { 
    Arg a = { .i = Info };

    a.s = g_strdup_printf(feedback);
    echo(&a);
}

void
update_url(const char *uri) {
    gboolean ssl = g_str_has_prefix(uri, "https://");
    GdkColor color;
#ifdef ENABLE_HISTORY_INDICATOR
    char before[] = " [";
    char after[] = "]";
    gboolean back = webkit_web_view_can_go_back(webview);
    gboolean fwd = webkit_web_view_can_go_forward(webview);

    if (!back && !fwd)
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
    gtk_widget_modify_fg(GTK_WIDGET(status_url), GTK_STATE_NORMAL, &color);
    gtk_widget_modify_fg(GTK_WIDGET(status_state), GTK_STATE_NORMAL, &color);
}

void
update_state() {
    char *markup;
    int download_count = g_list_length(activeDownloads);
    GString *status = g_string_new("");

    /* construct the status line */

    /* count, modkey and input buffer */
    g_string_append_printf(status, "%.0d", count);
    if (current_modkey) g_string_append_c(status, current_modkey);

    /* the number of active downloads */
    if (activeDownloads) {
        g_string_append_printf(status, " %d active %s", download_count,
                (download_count == 1) ? "download" : "downloads");
    }

#ifdef ENABLE_WGET_PROGRESS_BAR
    /* the progressbar */
    {
        int progress = -1;
        char progressbar[progressbartick + 1];

        if (activeDownloads) {
            progress = 0;
            GList *ptr;

            for (ptr = activeDownloads; ptr; ptr = g_list_next(ptr)) {
                progress += 100 * webkit_download_get_progress(ptr->data);
            }

            progress /= download_count;

        } else if (webkit_web_view_get_load_status(webview) != WEBKIT_LOAD_FINISHED
                && webkit_web_view_get_load_status(webview) != WEBKIT_LOAD_FAILED) {

            progress = webkit_web_view_get_progress(webview) * 100;
        }

        if (progress >= 0) {
            ascii_bar(progressbartick, progress * progressbartick / 100, progressbar);
            g_string_append_printf(status, " %c%s%c",
                    progressborderleft, progressbar, progressborderright);
        }
    }
#endif

    /* and the current scroll position */
    {
        int max = gtk_adjustment_get_upper(adjust_v) - gtk_adjustment_get_page_size(adjust_v);
        int val = (int)(gtk_adjustment_get_value(adjust_v) / max * 100);

        if (max == 0)
            g_string_append(status, " All");
        else if (val == 0)
            g_string_append(status, " Top");
        else if (val == 100)
            g_string_append(status, " Bot");
        else
            g_string_append_printf(status, " %d%%", val);
    }


    markup = g_markup_printf_escaped("<span font=\"%s\">%s</span>", statusfont, status->str);
    gtk_label_set_markup(GTK_LABEL(status_state), markup);

    g_string_free(status, TRUE);
}

void
setup_modkeys() {
    unsigned int i;
    modkeys = calloc(LENGTH(keys) + 1, sizeof(char));
    char *ptr = modkeys;

    for(i = 0; i < LENGTH(keys); i++)
        if (keys[i].modkey && !strchr(modkeys, keys[i].modkey))
            *(ptr++) = keys[i].modkey;
    modkeys = realloc(modkeys, &ptr[0] - &modkeys[0] + 1);
}

void
setup_gui() {
    GtkScrollbar *scroll_h = GTK_SCROLLBAR(gtk_hscrollbar_new(NULL));
    GtkScrollbar *scroll_v = GTK_SCROLLBAR(gtk_vscrollbar_new(NULL));
    adjust_h = gtk_range_get_adjustment(GTK_RANGE(scroll_h));
    adjust_v = gtk_range_get_adjustment(GTK_RANGE(scroll_v));
    if (embed) {
        window = (GtkWindow *)gtk_plug_new(embed);
    } else {
        window = (GtkWindow *)gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_wmclass(window, "vimprobable", "Vimprobable");
    }
    gtk_window_set_default_size(window, 640, 480);
    box = GTK_BOX(gtk_vbox_new(FALSE, 0));
    inputbox = gtk_entry_new();
    webview = (WebKitWebView*)webkit_web_view_new();
    GtkBox *statusbar = GTK_BOX(gtk_hbox_new(FALSE, 0));
    eventbox = gtk_event_box_new();
    status_url = gtk_label_new(NULL);
    status_state = gtk_label_new(NULL);
    GdkColor bg;
    PangoFontDescription *font;
    GdkGeometry hints = { 1, 1 };

    clipboards[0] = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    clipboards[1] = gtk_clipboard_get(GDK_NONE);
    setup_settings();
    gdk_color_parse(statusbgcolor, &bg);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &bg);
    gtk_widget_set_name(GTK_WIDGET(window), "Vimprobable");
    gtk_window_set_geometry_hints(window, NULL, &hints, GDK_HINT_MIN_SIZE);

#ifdef DISABLE_SCROLLBAR
    GtkWidget *viewport = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(viewport), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
#else
    /* Ensure we still see scrollbars. */
    GtkWidget *viewport = gtk_scrolled_window_new(adjust_h, adjust_v);
#endif

    setup_signals();
    gtk_container_add(GTK_CONTAINER(viewport), GTK_WIDGET(webview));

    /* Ensure we set the scroll adjustments now, so that if we're not drawing
     * titlebars, we can still scroll.
     */
    gtk_widget_set_scroll_adjustments(GTK_WIDGET(webview), adjust_h, adjust_v);

    font = pango_font_description_from_string(urlboxfont[0]);
    gtk_widget_modify_font(GTK_WIDGET(inputbox), font);
    pango_font_description_free(font);
    gtk_entry_set_inner_border(GTK_ENTRY(inputbox), NULL);
    gtk_misc_set_alignment(GTK_MISC(status_url), 0.0, 0.0);
    gtk_misc_set_alignment(GTK_MISC(status_state), 1.0, 0.0);
    gtk_box_pack_start(statusbar, status_url, TRUE, TRUE, 2);
    gtk_box_pack_start(statusbar, status_state, FALSE, FALSE, 2);
    gtk_container_add(GTK_CONTAINER(eventbox), GTK_WIDGET(statusbar));
    gtk_box_pack_start(box, viewport, TRUE, TRUE, 0);
    gtk_box_pack_start(box, eventbox, FALSE, FALSE, 0);
    gtk_entry_set_has_frame(GTK_ENTRY(inputbox), FALSE);
    gtk_box_pack_end(box, inputbox, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(box));
    gtk_widget_grab_focus(GTK_WIDGET(webview));
    gtk_widget_show_all(GTK_WIDGET(window));
}

void
setup_settings() {
    WebKitWebSettings *settings = (WebKitWebSettings*)webkit_web_settings_new();
    SoupURI *proxy_uri;
    char *filename, *new;
    int  len;

    session = webkit_get_default_session();
    g_object_set((GObject*)settings, "default-font-size", DEFAULT_FONT_SIZE, NULL);
    g_object_set((GObject*)settings, "enable-scripts", enablePlugins, NULL);
    g_object_set((GObject*)settings, "enable-plugins", enablePlugins, NULL);
    filename = g_strdup_printf(USER_STYLES_FILENAME);
    filename = g_strdup_printf("file://%s", filename);
    g_object_set((GObject*)settings, "user-stylesheet-uri", filename, NULL);
    g_object_set((GObject*)settings, "user-agent", USER_AGENT, NULL);
    g_object_get((GObject*)settings, "zoom-step", &zoomstep, NULL);
    webkit_web_view_set_settings(webview, settings);

    /* proxy */
    filename = (char *)g_getenv("http_proxy");
    /* Fallthrough to checking HTTP_PROXY as well, since this can also be
     * defined.
     */
    if (filename == NULL)
        filename = (char *)g_getenv("HTTP_PROXY");
    if (filename != NULL && 0 < (len = strlen(filename))) {
        if (strstr(filename, "://") == NULL) {
            /* prepend http:// */
            new = g_malloc(sizeof("http://") + len);
            strcpy(new, "http://");
            memcpy(&new[sizeof("http://") - 1], filename, len + 1);
            proxy_uri = soup_uri_new(new);
        } else {
            proxy_uri = soup_uri_new(filename);
        }
        g_object_set(session, "proxy-uri", proxy_uri, NULL);
    }
}

void
setup_signals() {
#ifdef ENABLE_COOKIE_SUPPORT
    /* Headers. */
    g_signal_connect_after((GObject*)session, "request-started", (GCallback)new_generic_request, NULL);
#endif
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
        "signal::create-web-view",                      (GCallback)webview_open_in_new_window_cb,   NULL,
        "signal::event",                                (GCallback)notify_event_cb,                 NULL,
    NULL);
    /* webview adjustment */
    g_object_connect((GObject*)adjust_v,
        "signal::value-changed",                        (GCallback)webview_scroll_cb,               NULL,
    NULL);
    /* inputbox */
    g_object_connect((GObject*)inputbox,
        "signal::activate",                             (GCallback)inputbox_activate_cb,            NULL,
        "signal::key-press-event",                      (GCallback)inputbox_keypress_cb,            NULL,
        "signal::key-release-event",                    (GCallback)inputbox_keyrelease_cb,          NULL,
#ifdef ENABLE_INCREMENTAL_SEARCH
        "signal::changed",                              (GCallback)inputbox_changed_cb,             NULL,
#endif
    NULL);
}

#ifdef ENABLE_COOKIE_SUPPORT
void
setup_cookies()
{
	if (file_cookie_jar)
		g_object_unref(file_cookie_jar);

	if (session_cookie_jar)
		g_object_unref(session_cookie_jar);

	session_cookie_jar = soup_cookie_jar_new();
	cookie_store = g_strdup_printf(COOKIES_STORAGE_FILENAME);

	load_all_cookies();

	g_signal_connect((GObject *)file_cookie_jar, "changed",
			G_CALLBACK(update_cookie_jar), NULL);

	return;
}

/* TA:  XXX - we should be using this callback for any header-requests we
 *      receive (hence the name "new_generic_request" -- but for now, its use
 *      is limited to handling cookies.
 */
void
new_generic_request(SoupSession *session, SoupMessage *soup_msg, gpointer unused) {
	SoupMessageHeaders *soup_msg_h;
	SoupURI *uri;
	const char *cookie_str;

	soup_msg_h = soup_msg->request_headers;
	soup_message_headers_remove(soup_msg_h, "Cookie");
	uri = soup_message_get_uri(soup_msg);
	if( (cookie_str = get_cookies(uri)) )
		soup_message_headers_append(soup_msg_h, "Cookie", cookie_str);

	g_signal_connect_after(G_OBJECT(soup_msg), "got-headers", G_CALLBACK(handle_cookie_request), NULL);

	return;
}

const char *
get_cookies(SoupURI *soup_uri) {
	const char *cookie_str;

	cookie_str = soup_cookie_jar_get_cookies(file_cookie_jar, soup_uri, TRUE);

	return cookie_str;
}

void
handle_cookie_request(SoupMessage *soup_msg, gpointer unused)
{
	GSList *resp_cookie = NULL;
	SoupCookie *cookie;

	for(resp_cookie = soup_cookies_from_response(soup_msg);
		resp_cookie;
		resp_cookie = g_slist_next(resp_cookie))
	{
		SoupDate *soup_date;
		cookie = soup_cookie_copy((SoupCookie *)resp_cookie->data);

		if (cookie_timeout && cookie->expires == NULL) {
			soup_date = soup_date_new_from_time_t(time(NULL) + cookie_timeout * 10);
			soup_cookie_set_expires(cookie, soup_date);
		}
		soup_cookie_jar_add_cookie(file_cookie_jar, cookie);
	}

	return;
}

void
update_cookie_jar(SoupCookieJar *jar, SoupCookie *old, SoupCookie *new)
{
	if (!new) {
		/* Nothing to do. */
		return;
	}

	SoupCookie *copy;
	copy = soup_cookie_copy(new);

	soup_cookie_jar_add_cookie(session_cookie_jar, copy);

	return;
}

void
load_all_cookies(void)
{
	file_cookie_jar = soup_cookie_jar_text_new(cookie_store, COOKIES_STORAGE_READONLY);

	/* Put them back in the session store. */
	GSList *cookies_from_file = soup_cookie_jar_all_cookies(file_cookie_jar);

	for (; cookies_from_file;
	       cookies_from_file = cookies_from_file->next)
	{
		soup_cookie_jar_add_cookie(session_cookie_jar, cookies_from_file->data);
	}

	soup_cookies_free(cookies_from_file);

	return;
}

#endif

void
mop_up(void) {
	/* Free up any nasty globals before exiting. */
#ifdef ENABLE_COOKIE_SUPPORT
	if (cookie_store)
		g_free(cookie_store);
#endif
	return;
}

Listelement *
complete_list(const char *searchfor, const int mode, Listelement *elementlist)
{
    FILE *f;
    const char *filename;
    Listelement *candidatelist = NULL, *candidatepointer = NULL;
    char s[255] = "", readelement[MAXTAGSIZE + 1] = "";
    int i, t, n = 0;

    if (mode == 2) {
        /* open in history file */
        filename = g_strdup_printf(HISTORY_STORAGE_FILENAME);
    } else {
        /* open in bookmark file (for tags and bookmarks) */
        filename = g_strdup_printf(BOOKMARKS_STORAGE_FILENAME);
    }
    f = fopen(filename, "r");
    if (f == NULL) {
        g_free((gpointer)filename);
        return (elementlist);
    }

    while (fgets(s, 254, f)) {
        if (mode == 1) {
            /* just tags (could be more than one per line) */
            i = 0;
            while (s[i] && i < 254) {
                while (s[i] != '[' && s[i])
                    i++;
                if (s[i] != '[')
                    continue;
                i++;
                t = 0;
                while (s[i] != ']' && s[i] && t < MAXTAGSIZE)
                    readelement[t++] = s[i++];
                readelement[t] = '\0';
                candidatelist = add_list(readelement, candidatelist);
                i++;
            }
        } else {
            /* complete string (bookmarks & history) */
            candidatelist = add_list(s, candidatelist);
        }
        candidatepointer = candidatelist;
        while (candidatepointer != NULL) {
            if (!complete_case_sensitive) {
               g_strdown(candidatepointer->element);
            }
            if (!strlen(searchfor) || strstr(candidatepointer->element, searchfor) != NULL) {
                /* only use string up to the first space */
                memset(readelement, 0, MAXTAGSIZE + 1);
                if (strchr(candidatepointer->element, ' ') != NULL) {
                    i = strcspn(candidatepointer->element, " ");
                    strncpy(readelement, candidatepointer->element, i);
                } else {
                    strncpy(readelement, candidatepointer->element, MAXTAGSIZE);
                }
                /* for URLs without title, strip newline character */
                if (readelement[strlen(readelement) - 1] == '\n') {
                    readelement[strlen(readelement) - 1] = '\0';
                }
                elementlist = add_list(readelement, elementlist);
                n = count_list(elementlist);
            }
            if (n >= MAX_LIST_SIZE)
                break;
            candidatepointer = candidatepointer->next;
        }
        free_list(candidatelist);
        candidatelist = NULL;
        if (n >= MAX_LIST_SIZE)
            break;
    }
    g_free((gpointer)filename);
    return (elementlist);
}

Listelement *
add_list(const char *element, Listelement *elementlist)
{
    int n, i = 0;
    Listelement *newelement, *elementpointer, *lastelement;

    if (elementlist == NULL) { /* first element */
        newelement = malloc(sizeof(Listelement));
        if (newelement == NULL) 
            return (elementlist);
        strncpy(newelement->element, element, 254);
        newelement->next = NULL;
        return newelement;
    }
    elementpointer = elementlist;
    n = strlen(element);

    /* check if element is already in list */
    while (elementpointer != NULL) {
        if (strlen(elementpointer->element) == n && 
                strncmp(elementpointer->element, element, n) == 0)
            return (elementlist);
        lastelement = elementpointer;
        elementpointer = elementpointer->next;
        i++;
    }
    /* add to list */
    newelement = malloc(sizeof(Listelement));
    if (newelement == NULL)
        return (elementlist);
    lastelement->next = newelement;
    strncpy(newelement->element, element, 254);
    newelement->next = NULL;
    return elementlist;
}

void
free_list(Listelement *elementlist)
{
    Listelement *elementpointer;

    while (elementlist != NULL) {
        elementpointer = elementlist->next;
        free(elementlist);
        elementlist = elementpointer;
    }
}

int
count_list(Listelement *elementlist)
{
    Listelement *elementpointer = elementlist;
    int n = 0;

    while (elementpointer != NULL) {
        n++;
        elementpointer = elementpointer->next;
    }
    
    return n;
}

int
main(int argc, char *argv[]) {
    static Arg a;
    static char url[256] = "";
    static gboolean ver = false;
    static GOptionEntry opts[] = {
            { "version", 'v', 0, G_OPTION_ARG_NONE, &ver, "print version", NULL },
            { "embed", 'e', 0, G_OPTION_ARG_STRING, &winid, "embeded", NULL },
            { NULL }
    };
    static GError *err;
    args = argv;

    if (!gtk_init_with_args(&argc, &argv, "[<uri>]", opts, NULL, &err)) {
        g_printerr("can't init gtk: %s\n", err->message);
        g_error_free(err);
        return EXIT_FAILURE;
    }

    if (ver) {
        printf("%s\n", INTERNAL_VERSION);
        return EXIT_SUCCESS;
    }

    if (!g_thread_supported())
        g_thread_init(NULL);

    if (winid)
        embed = atoi(winid);

    if (argc >= 2) {
        strncpy(url, argv[1], 255);
    } else {
        strncpy(url, startpage, 255);
    }

    setup_modkeys();
    setup_gui();
#ifdef ENABLE_COOKIE_SUPPORT
    setup_cookies();
#endif
    a.i = TargetCurrent;
    a.s = url;
    open_arg(&a);
    gtk_main();

    mop_up();

    return EXIT_SUCCESS;
}
