/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    see LICENSE file
*/

#include "includes.h"
#include "vimprobable.h"
#include "utilities.h"
#include "callbacks.h"
#include "hintingmode.h"

/* remove numlock symbol from keymask */
#define CLEAN(mask) (mask & ~(GDK_MOD2_MASK) & ~(GDK_BUTTON1_MASK) & ~(GDK_BUTTON2_MASK) & ~(GDK_BUTTON3_MASK) & ~(GDK_BUTTON4_MASK) & ~(GDK_BUTTON5_MASK))

/* callbacks here */
static void inputbox_activate_cb(GtkEntry *entry, gpointer user_data);
static gboolean inputbox_keypress_cb(GtkEntry *entry, GdkEventKey *event);
static gboolean inputbox_keyrelease_cb(GtkEntry *entry, GdkEventKey *event);
static WebKitWebView* inspector_inspect_web_view_cb(gpointer inspector, WebKitWebView* web_view);
static gboolean notify_event_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static gboolean webview_console_cb(WebKitWebView *webview, char *message, int line, char *source, gpointer user_data);
static gboolean webview_download_cb(WebKitWebView *webview, WebKitDownload *download, gpointer user_data);
static void webview_hoverlink_cb(WebKitWebView *webview, char *title, char *link, gpointer data);
static gboolean webview_keypress_cb(WebKitWebView *webview, GdkEventKey *event);
static void webview_load_committed_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data);
static void webview_load_finished_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data);
static gboolean webview_mimetype_cb(WebKitWebView *webview, WebKitWebFrame *frame, WebKitNetworkRequest *request,
                        char *mime_type, WebKitWebPolicyDecision *decision, gpointer user_data);
static gboolean webview_new_window_cb(WebKitWebView *webview, WebKitWebFrame *frame, WebKitNetworkRequest *request,
                        WebKitWebNavigationAction *action, WebKitWebPolicyDecision *decision, gpointer user_data);
static gboolean webview_open_in_new_window_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data);
static void webview_progress_changed_cb(WebKitWebView *webview, int progress, gpointer user_data);
static void webview_title_changed_cb(WebKitWebView *webview, WebKitWebFrame *frame, char *title, gpointer user_data);
static void window_destroyed_cb(GtkWidget *window, gpointer func_data);

/* functions */
static gboolean bookmark(const Arg *arg);
static gboolean browser_settings(const Arg *arg);
static gboolean commandhistoryfetch(const Arg *arg);
static gboolean complete(const Arg *arg);
static gboolean descend(const Arg *arg);
gboolean echo(const Arg *arg);
static gboolean focus_input(const Arg *arg);
static gboolean input(const Arg *arg);
static gboolean navigate(const Arg *arg);
static gboolean number(const Arg *arg);
static gboolean open(const Arg *arg);
static gboolean paste(const Arg *arg);
static gboolean quickmark(const Arg *arg);
static gboolean quit(const Arg *arg);
static gboolean revive(const Arg *arg);
static gboolean search(const Arg *arg);
static gboolean set(const Arg *arg);
static gboolean script(const Arg *arg);
static gboolean scroll(const Arg *arg);
static gboolean yank(const Arg *arg);
static gboolean view_source(const Arg * arg);
static gboolean zoom(const Arg *arg);

static void update_url(const char *uri);
static void setup_modkeys(void);
static void setup_gui(void);
static void setup_settings(void);
static void setup_signals(void);
static void ascii_bar(int total, int state, char *string);
static gchar *jsapi_ref_to_string(JSContextRef context, JSValueRef ref);
static void jsapi_evaluate_script(const gchar *script, gchar **value, gchar **message);
static void download_progress(WebKitDownload *d, GParamSpec *pspec);

static gboolean history(void);
static gboolean mappings(const Arg *arg);
char * search_word(int whichword);
static gboolean process_set_line(char *line);
static gboolean process_map_line(char *line);
gboolean parse_colour(char *color);
gboolean read_rcfile(void);
void save_command_history(char *line);
void toggle_proxy(gboolean onoff);

void make_keyslist(void);
gboolean process_keypress(GdkEventKey *event);
void fill_suggline(char * suggline, const char * command, const char *fill_with);
GtkWidget * fill_eventbox(const char * completion_line);


#include "main.h"

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
static char scroll_state[4] = "\0";
static char *modkeys;
static char current_modkey;
static char *search_handle;
static gboolean search_direction;
static gboolean echo_active = TRUE;
WebKitWebInspector *inspector;

static GdkNativeWindow embed = 0;
static char *winid = NULL;

static char rememberedURI[128] = "";
static char inputKey[5];
static char inputBuffer[65] = "";
static char followTarget[8] = "";
static WebKitDownload *activeDownload = NULL;

#include "config.h"

char commandhistory[COMMANDHISTSIZE][255];
int  lastcommand    = 0;
int  maxcommands    = 0;
int  commandpointer = 0;
KeyList *keylistroot = NULL;

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
    const char *uri = webkit_web_view_get_uri(webview);

    update_url(uri);
}

void
webview_load_finished_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data) {
    Arg a = { .i = Silent, .s = JS_SETUP };

    if (HISTORY_MAX_ENTRIES > 0)
        history();
    update_state();
    script(&a);
}

static gboolean
webview_open_in_new_window_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data) {
    Arg a = { .i = TargetNew, .s = (char*)webkit_web_view_get_uri(webview) };
    if (strlen(rememberedURI) > 0) {
        a.s = rememberedURI;
    }
    open(&a);
    return FALSE;
}

gboolean
webview_new_window_cb(WebKitWebView *webview, WebKitWebFrame *frame, WebKitNetworkRequest *request,
                        WebKitWebNavigationAction *action, WebKitWebPolicyDecision *decision, gpointer user_data) {
    Arg a = { .i = TargetNew, .s = (char*)webkit_network_request_get_uri(request) };
    open(&a);
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

static WebKitWebView*
inspector_inspect_web_view_cb(gpointer inspector, WebKitWebView* web_view) {
    gchar*     inspector_title;
    GtkWidget* inspector_window;
    GtkWidget* inspector_view;

    /* just enough code to show the inspector - no signal handling etc. */
    inspector_title = g_strdup_printf("Inspect page - %s - Vimprobable2", webkit_web_view_get_uri(web_view));
    if (embed) {
        inspector_window = gtk_plug_new(embed);
    } else {
        inspector_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_wmclass(window, "vimprobable2", "Vimprobable2");
    }
    gtk_window_set_title(GTK_WINDOW(inspector_window), inspector_title);
    g_free(inspector_title);
    inspector_view = webkit_web_view_new();
    gtk_container_add(GTK_CONTAINER(inspector_window), inspector_view);
    gtk_widget_show_all(inspector_window);
    return WEBKIT_WEB_VIEW(inspector_view);
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
    activeDownload = download;
    g_signal_connect(activeDownload, "notify::progress", G_CALLBACK(download_progress), NULL);
    g_signal_connect(activeDownload, "notify::status", G_CALLBACK(download_progress), NULL);
    update_state();
    return TRUE;
}

void
download_progress(WebKitDownload *d, GParamSpec *pspec) {
    WebKitDownloadStatus status;
    Arg a;

    if (activeDownload != NULL) {
        status = webkit_download_get_status(activeDownload);
        if (status != WEBKIT_DOWNLOAD_STATUS_STARTED && status != WEBKIT_DOWNLOAD_STATUS_CREATED) {
            if (status != WEBKIT_DOWNLOAD_STATUS_FINISHED) {
                a.i = Error;
                echo(&a);
            } else {
                a.i = Info;
                a.s = g_strdup_printf("Download finished");
                echo(&a);
            }
            activeDownload = NULL;
        }
        update_state();
    }
}


void
make_keyslist(void) {
    int i;
    KeyList *ptr, *current;

    ptr     = NULL;
    current = NULL;
    for (i = 0; i < LENGTH(keys); i++) {
        current = malloc(sizeof(KeyList));
        if (current == NULL) {
            printf("Not enough memory\n");
            exit(-1);
        }
        current->Element = keys[i];
        current->next = NULL;
        if (keylistroot == NULL) keylistroot = current;
        if (ptr != NULL) ptr->next = current;
        ptr = current;
    }
}

gboolean
process_keypress(GdkEventKey *event) {
    KeyList *current;

    current = keylistroot;
    while (current != NULL) {
        if (current->Element.mask == CLEAN(event->state)
                && (current->Element.modkey == current_modkey
                    || (!current->Element.modkey && !current_modkey)
                    || current->Element.modkey == GDK_VoidSymbol )    /* wildcard */
                && current->Element.key == event->keyval
                && current->Element.func)
            if (current->Element.func(&current->Element.arg)) {
                current_modkey = count = 0;
                update_state();
                return TRUE;
            }
        current = current->next;
    }
    return FALSE;
}

gboolean
webview_keypress_cb(WebKitWebView *webview, GdkEventKey *event) {
    Arg a = { .i = ModeNormal, .s = NULL };

    switch (mode) {
    case ModeNormal:
        if (CLEAN(event->state) == 0) {
            memset(inputBuffer, 0, 65);
            if (event->keyval == GDK_Escape) {
                a.i = Info;
                a.s = "";
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
        if (process_keypress(event) == TRUE) return TRUE;

        break;
    case ModeInsert:
        if (CLEAN(event->state) == 0 && event->keyval == GDK_Escape) {
            a.i = Silent;
            a.s = "clearfocus()";
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
    case ModeHints:
        if (CLEAN(event->state) == 0 && event->keyval == GDK_Escape) {
            a.i = Silent;
            a.s = "clear()";
            script(&a);
            a.i = ModeNormal;
            count = 0;
            return set(&a);
        } else if (CLEAN(event->state) == 0 && ((event->keyval >= GDK_1 && event->keyval <= GDK_9)
                || (event->keyval >= GDK_KP_1 && event->keyval <= GDK_KP_9)
                || ((event->keyval == GDK_0 || event->keyval == GDK_KP_0) && count))) {
            /* allow a zero as non-first number */
            if (event->keyval >= GDK_KP_1 && event->keyval <= GDK_KP_9)
                count = (count ? count * 10 : 0) + (event->keyval - GDK_KP_0);
            else
                count = (count ? count * 10 : 0) + (event->keyval - GDK_0);
            memset(inputBuffer, 0, 65);
            sprintf(inputBuffer, "%d", count);
            a.s = g_strconcat("update_hints(", inputBuffer, ")", NULL);
            a.i = Silent;
            script(&a);
            update_state();
	    return TRUE;
        } else if ((CLEAN(event->state) == 0 && (event->keyval >= GDK_a && event->keyval <= GDK_z))
                || (CLEAN(event->state) == GDK_SHIFT_MASK && (event->keyval >= GDK_A && event->keyval <= GDK_Z))) {
            /* update hints by link text */
            if (strlen(inputBuffer) < 65) {
                count = 0;
                memset(inputKey, 0, 5);
                sprintf(inputKey, "%c", event->keyval);
                strncat(inputBuffer, inputKey, 1);
                a.i = Silent;
                a.s = "cleanup()";
                script(&a);
                a.s = g_strconcat("show_hints('", inputBuffer, "')", NULL);
                a.i = Silent;
                script(&a);
                update_state();
            }
	    return TRUE;
        } else if (CLEAN(event->state) == 0 && event->keyval == GDK_Return && count) {
            memset(inputBuffer, 0, 65);
            sprintf(inputBuffer, "%d", count);
            a.s = g_strconcat("fire(", inputBuffer, ")", NULL);
            a.i = Silent;
            script(&a);
            memset(inputBuffer, 0, 65);
            count = 0;
            update_state();
	    return TRUE;
        } else if (CLEAN(event->state) == 0 && event->keyval == GDK_BackSpace) {
            if (count > 0) {
                count = ((count >= 10) ? count/10 : 0);
                memset(inputBuffer, 0, 65);
                sprintf(inputBuffer, "%d", count);
                a.s = g_strconcat("update_hints(", inputBuffer, ")", NULL);
                a.i = Silent;
                script(&a);
                update_state();
            } else if (strlen(inputBuffer) > 0) {
                a.i = Silent;
                a.s = "cleanup()";
                script(&a);
                strncpy((inputBuffer + strlen(inputBuffer) - 1), "\0", 1);
                a.s = g_strconcat("show_hints('", inputBuffer, "')", NULL);
                a.i = Silent;
                script(&a);
                update_state();
            }
	    return TRUE;
        }
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
inputbox_activate_cb(GtkEntry *entry, gpointer user_data) {
    char *text;
    guint16 length = gtk_entry_get_text_length(entry);
    Arg a;
    int i;
    size_t len;
    gboolean success = FALSE, forward = FALSE;

    a.i = HideCompletion;
    complete(&a);
    if (length < 2)
        return;
    text = (char*)gtk_entry_get_text(entry);
    if (text[0] == ':') {
        for (i = 0; i < LENGTH(commands); i++) {
            len = strlen(commands[i].cmd);
            if (length >= len && !strncmp(&text[1], commands[i].cmd, len) && (text[len + 1] == ' ' || !text[len + 1])) {
                a.i = commands[i].arg.i;
                a.s = length > len + 2 ? &text[len + 2] : commands[i].arg.s;
                if ((success = commands[i].func(&a)))
                    break;
            }
        }

        save_command_history(text);

        if (!success) {
            a.i = Error;
            a.s = g_strdup_printf("Not a browser command: %s", &text[1]);
            echo(&a);
            g_free(a.s);
        }
    } else if ((forward = text[0] == '/') || text[0] == '?') {
        webkit_web_view_unmark_text_matches(webview);
#ifdef ENABLE_MATCH_HIGHLITING
        webkit_web_view_mark_text_matches(webview, &text[1], FALSE, 0);
        webkit_web_view_set_highlight_text_matches(webview, TRUE);
#endif
        count = 0;
        a.s =& text[1];
        a.i = searchoptions | (forward ? DirectionForward : DirectionBackwards);
        search(&a);
    } else
        return;
    if (!echo_active)
        gtk_entry_set_text(entry, "");
    gtk_widget_grab_focus(GTK_WIDGET(webview));
}

gboolean
inputbox_keypress_cb(GtkEntry *entry, GdkEventKey *event) {
    Arg a;

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
        case GDK_Up:
            a.i = DirectionPrev;
            return commandhistoryfetch(&a);
        break;
        case GDK_Down:
            a.i = DirectionNext;
            return commandhistoryfetch(&a);
        break;
        case GDK_ISO_Left_Tab:
            a.i = DirectionPrev;
            return complete(&a);
        break;
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
#ifdef ENABLE_INCREMENTAL_SEARCH
    char *text = (char*)gtk_entry_get_text(entry);
    gboolean forward = FALSE;
#endif
    if (!length) {
        a.i = HideCompletion;
        complete(&a);
        a.i = ModeNormal;
        return set(&a);
    }
#ifdef ENABLE_INCREMENTAL_SEARCH
    else if (length > 1 && ((forward = text[0] == '/') || text[0] == '?')) {
        webkit_web_view_unmark_text_matches(webview);
        webkit_web_view_search_text(webview, &text[1], searchoptions & CaseSensitive, forward, searchoptions & Wrapping);
    }
#endif
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
    FILE *f;
    const char *filename;
    char *str, *s, *p, *markup, *entry, *searchfor, command[32] = "", suggline[512] = "", *url, **suggurls;
    size_t listlen, len, cmdlen;
    int i, spacepos;
    gboolean highlight = FALSE, finished = FALSE;
    GtkBox *row;
    GtkWidget *row_eventbox, *el;
    GtkBox *_table;
    GdkColor color;
    static GtkWidget *table, **widgets, *top_border;
    static char **suggestions, *prefix;
    static int n = 0, current = -1;

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
        listlen = LENGTH(commands);
        widgets = malloc(sizeof(GtkWidget*) * listlen);
        suggestions = malloc(sizeof(char*) * listlen);
        top_border = gtk_event_box_new();
        gtk_widget_set_size_request(GTK_WIDGET(top_border), 0, 1);
        gdk_color_parse(completioncolor[2], &color);
        gtk_widget_modify_bg(top_border, GTK_STATE_NORMAL, &color);
        table = gtk_event_box_new();
        gdk_color_parse(completionbgcolor[0], &color);
        _table = GTK_BOX(gtk_vbox_new(FALSE, 0));
        highlight = len > 1;
        if (strchr(str, ' ') == NULL) {
            for (i = 0; i < listlen; i++) {
                cmdlen = strlen(commands[i].cmd);
                if (!highlight || (len - 1 <= cmdlen && !strncmp(&str[1], commands[i].cmd, len - 1))) {
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
            if (entry != NULL) {
                memset(entry, 0, 512);
                suggurls = malloc(sizeof(char*) * listlen);
                if (suggurls != NULL) {
                    spacepos = strcspn(str, " ");
                    searchfor = (str + spacepos + 1);
                    strncpy(command, (str + 1), spacepos - 1);
                    if (strlen(command) == 3 && strncmp(command, "set", 3) == 0) {
                        /* browser settings */
                        listlen = LENGTH(browsersettings);
                        for (i = 0; i < listlen; i++) {
                            if (strstr(browsersettings[i].name, searchfor) != NULL) {
                                /* match */
                                fill_suggline(suggline, command, browsersettings[i].name);
                                suggurls[n] = (char *)malloc(sizeof(char) * 512 + 1);
                                strncpy(suggurls[n], suggline, 512);
                                suggestions[n] = suggurls[n];
                                row_eventbox = fill_eventbox(suggline);
                                gtk_box_pack_start(_table, GTK_WIDGET(row_eventbox), FALSE, FALSE, 0);
                                widgets[n++] = row_eventbox;
                            }

                        }
                    } else {
                        /* URL completion using the current command */
                        filename = g_strdup_printf(BOOKMARKS_STORAGE_FILENAME);
                        f = fopen(filename, "r");
                        if (f != NULL) {
                            while (finished != TRUE) {
                                if ((char *)NULL == fgets(entry, 512, f)) {
                                    /* check if end of file was reached / error occured */
                                    if (!feof(f)) {
                                        break;
                                    }
                                    /* end of file reached */
                                    finished = TRUE;
                                    continue;
                                }
                                if (strstr(entry, searchfor) != NULL) {
                                    /* found in bookmarks */
                                    if (strchr(entry, ' ') != NULL) {
                                        url = strtok(entry, " ");
                                    } else {
                                        url = strtok(entry, "\n");
                                    }
                                    fill_suggline(suggline, command, url);
                                    suggurls[n] = (char *)malloc(sizeof(char) * 512 + 1);
                                    strncpy(suggurls[n], suggline, 512);
                                    suggestions[n] = suggurls[n];
                                    row_eventbox = fill_eventbox(suggline);
                                    gtk_box_pack_start(_table, GTK_WIDGET(row_eventbox), FALSE, FALSE, 0);
                                    widgets[n++] = row_eventbox;
                                }
                                if (n >= listlen) {
                                    break;
                                }
                            }
                            fclose(f);
                            /* history */
                            if (n < listlen) {
                                filename = g_strdup_printf(HISTORY_STORAGE_FILENAME);
                                f = fopen(filename, "r");
                                if (f != NULL) {
                                    finished = FALSE;
                                    while (finished != TRUE) {
                                        if ((char *)NULL == fgets(entry, 512, f)) {
                                            /* check if end of file was reached / error occured */
                                            if (!feof(f)) {
                                                break;
                                            }
                                            /* end of file reached */
                                            finished = TRUE;
                                            continue;
                                        }
                                        if (strstr(entry, searchfor) != NULL) {
                                            /* found in history */
                                            if (strchr(entry, ' ') != NULL) {
                                                url = strtok(entry, " ");
                                            } else {
                                                url = strtok(entry, "\n");
                                            }
                                            fill_suggline(suggline, command, url);
                                            suggurls[n] = (char *)malloc(sizeof(char) * 512 + 1);
                                            strncpy(suggurls[n], suggline, 512);
                                            suggestions[n] = suggurls[n];
                                            row_eventbox = fill_eventbox(suggline);
                                            gtk_box_pack_start(_table, GTK_WIDGET(row_eventbox), FALSE, FALSE, 0);
                                            widgets[n++] = row_eventbox;
                                        }
                                        if (n >= listlen) {
                                            break;
                                        }
                                    }
                                    fclose(f);
                                }
                            }
                        }
                    }
                    if (suggurls != NULL) {
                        free(suggurls);
                        suggurls = NULL;
                    }
                }
                if (entry != NULL) {
                    free(entry);
                    entry = NULL;
                }
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
        for (i = 0; i < 3; i++)                  /* get to the third slash */
            if (!(p = strchr(++p, '/')))
                return TRUE;                    /* if we cannot find it quit */
    } else {
        len = strlen(source);
        if (!len)                                /* if string is empty quit */
            return TRUE;
        p = source + len;                       /* start at the end */
        if (*(p - 1) == '/')                     /* /\/$/ is not an additional level */
            ++count;
        for (i = 0; i < count; i++)
            while(*(p--) != '/' || *p == '/')   /* count /\/+/ as one slash */
                if (p == source)                 /* if we reach the first char pointer quit */
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
    return TRUE;
}

gboolean
input(const Arg *arg) {
    int pos = 0;
    count = 0;
    const char *url;

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
open(const Arg *arg) {
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
            for (i = 0; i < LENGTH(searchengines); i++)
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
    const char *url;
    Arg a = { .i = Info };

    if (arg->i & SourceURL) {
        url = webkit_web_view_get_uri(webview);
        if (!url)
            return TRUE;
        a.s = g_strdup_printf("Yanked %s", url);
        echo(&a);
        g_free(a.s);
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
        open(&new_target);
        return TRUE;
    }

    if (arg->i & ClipboardPrimary)
        a.s = gtk_clipboard_wait_for_text(clipboards[0]);
    if (!a.s && arg->i & ClipboardGTK)
        a.s = gtk_clipboard_wait_for_text(clipboards[1]);
    if (a.s)
        open(&a);
    return TRUE;
}

gboolean
quit(const Arg *arg) {
    FILE *f;
    const char *filename;
    const char *uri = webkit_web_view_get_uri(webview);
    /* write last URL into status file for recreation with "u" */
    filename = g_strdup_printf(CLOSED_URL_FILENAME);
    f = fopen(filename, "w");
    if (f != NULL) {
        fprintf(f, "%s", uri);
        fclose(f);
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
        open(&a);
        return TRUE;
    }
    return FALSE;
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
                    g_free(a.s);
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
        g_free(a.s);
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
        a.s = "-- PASS THROUGH --";
        echo(&a);
        break;
    case ModeSendKey:
        a.s = "-- PASS TROUGH (next) --";
        echo(&a);
        break;
    case ModeInsert: /* should not be called manually but automatically */
        a.s = "-- INSERT --";
        echo(&a);
        break;
    case ModeHints:
        memset(followTarget, 0, 8);
        strncpy(followTarget, arg->s, 8);
        a.i = Silent;
        a.s = "show_hints()";
        script(&a);
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
quickmark(const Arg *a) {
    int i, b;
    b = atoi(a->s);
    char *fn = g_strdup_printf(QUICKMARK_FILE);
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
       if ( strlen(buf)) return open(&x);
       else  
       {       
           x.i = Error;
           x.s = g_strdup_printf("Quickmark %d not defined", b);
           echo(&x);
           g_free(x.s);
	   return false; 
       }
    }
    else { return false; }
}

gboolean
script(const Arg *arg) {
    gchar *value = NULL, *message = NULL;
    Arg a;

    if (!arg->s)
        return TRUE;
    jsapi_evaluate_script(arg->s, &value, &message);
    if (message) {
        a.i = Error;
        a.s = message;
        echo(&a);
        g_free(message);
    }
    if (arg->i != Silent && value) {
        a.i = arg->i;
        a.s = value;
        echo(&a);
    }
    if (value) {
        if (strncmp(value, "fire;", 5) == 0) {
            count = 0;
            a.s = g_strconcat("fire(", (value + 5), ")", NULL);
            a.i = Silent;
            script(&a);
        } else if (strncmp(value, "open;", 5) == 0) {
            a.i = ModeNormal;
            set(&a);
            if (strncmp(followTarget, "new", 3) == 0)
                a.i = TargetNew;
            else
                a.i = TargetCurrent;
            memset(followTarget, 0, 8);
            a.s = (value + 5);
            open(&a);
        }
    }
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
commandhistoryfetch(const Arg *arg) {
    if (arg->i == DirectionPrev) {
        commandpointer--;
        if (commandpointer < 0)
            commandpointer = maxcommands - 1;
    } else {
        commandpointer++;
        if (commandpointer == COMMANDHISTSIZE || commandpointer == maxcommands)
            commandpointer = 0;
    }

    if (commandpointer < 0)
        return FALSE;

    gtk_entry_set_text(GTK_ENTRY(inputbox), commandhistory[commandpointer ]);
    gtk_editable_set_position(GTK_EDITABLE(inputbox), -1);
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
    if (f != NULL && uri != NULL) {
        fprintf(f, "%s", uri);
        if (title != NULL) {
            fprintf(f, "%s", " ");
            fprintf(f, "%s", title);
        }
        fprintf(f, "%s", "\n");
        fclose(f);
        return TRUE;
    } else {
       return FALSE;
    }
}

gboolean
history() {
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
                        if (i >= HISTORY_MAX_ENTRIES) {
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
    return TRUE;
}

static gboolean
view_source(const Arg * arg) {
    gboolean current_mode = webkit_web_view_get_view_source_mode(webview);
    webkit_web_view_set_view_source_mode(webview, !current_mode);
    webkit_web_view_reload(webview);
    return TRUE;
}

static gboolean
mappings(const Arg *arg) {
    char line[255];

    if ( !arg->s )
        return FALSE;
    strncpy(line, arg->s, 254);
    if (process_map_line(line))
        return TRUE;
    else
        return FALSE;
}

static gboolean
focus_input(const Arg *arg) {
    static Arg a;

    a.s = g_strconcat("focus_input()", NULL);
    a.i = Silent;
    script(&a);
    update_state();
    return TRUE;
}

static gboolean
changemapping(Key * search_key, int maprecord) {
    KeyList *current, *newkey;

    current = keylistroot;

    if (current)
        while (current->next != NULL) {
            if (
                current->Element.mask   == search_key->mask &&
                current->Element.modkey == search_key->modkey &&
                current->Element.key    == search_key->key
               ) {
                current->Element.func = commands[maprecord].func;
                current->Element.arg  =  commands[maprecord].arg;
                return TRUE;
            }
            current = current->next;
        }
    newkey = malloc(sizeof(KeyList));
    if (newkey == NULL) {
        printf("Not enough memory\n");
        exit (-1);
    }
    newkey->Element.mask   = search_key->mask;
    newkey->Element.modkey = search_key->modkey;
    newkey->Element.key    = search_key->key;
    newkey->Element.func   = commands[maprecord].func;
    newkey->Element.arg    = commands[maprecord].arg;
    newkey->next           = NULL;

    if (keylistroot == NULL) keylistroot = newkey;

    if (current != NULL) current->next = newkey;

    return TRUE;
}

static gboolean
process_mapping(char * keystring, int maprecord) {
    Key search_key;

    search_key.mask   = 0;
    search_key.modkey = 0;
    search_key.key    = 0;

    if (strlen(keystring) == 1) {
        search_key.key = keystring[0];
    }

    if (strlen(keystring) == 2) {
        search_key.modkey= keystring[0];
        search_key.key = keystring[1];
    }

    /* process stuff like <S-v> for Shift-v or <C-v> for Ctrl-v
       or stuff like <S-v>a for Shift-v,a or <C-v>a for Ctrl-v,a
    */
    if ((strlen(keystring) == 5 ||  strlen(keystring) == 6)  && keystring[0] == '<'  && keystring[4] == '>') {
        switch (toupper(keystring[1])) {
            case 'S':
                search_key.mask = GDK_SHIFT_MASK;
                if (strlen(keystring) == 5) {
                    keystring[3] = toupper(keystring[3]);
                } else {
                    keystring[3] = tolower(keystring[3]);
                    keystring[5] = toupper(keystring[5]);
                }
            break;
            case 'C':
                search_key.mask = GDK_CONTROL_MASK;
            break;
        }
        if (!search_key.mask)
            return FALSE;
        if (strlen(keystring) == 5) {
            search_key.key = keystring[3];
        } else {
            search_key.modkey= keystring[3];
            search_key.key = keystring[5];
        }
    }

    /* process stuff like <S-v> for Shift-v or <C-v> for Ctrl-v
       or  stuff like a<S-v> for a,Shift-v or a<C-v> for a,Ctrl-v
    */
    if (strlen(keystring) == 6 && keystring[1] == '<' && keystring[5] == '>') {
        switch (toupper(keystring[2])) {
            case 'S':
                search_key.mask = GDK_SHIFT_MASK;
                keystring[4] = toupper(keystring[4]);
            break;
            case 'C':
                search_key.mask = GDK_CONTROL_MASK;
            break;
        }
        if (!search_key.mask)
            return FALSE;
        search_key.modkey= keystring[0];
        search_key.key = keystring[4];
    }
    return (changemapping(&search_key, maprecord));
}

static gboolean
process_map_line(char *line) {
    int listlen, i;
    char *c;
    my_pair.line = line;
    c = search_word (0);

    if (!strlen (my_pair.what))
        return FALSE;
    while (isspace (*c) && *c)
        c++;

    if (*c == ':' || *c == '=')
        c++;
    my_pair.line = c;
    c = search_word (1);
    if (!strlen (my_pair.value))
        return FALSE;
    listlen = LENGTH(commands);
    for (i = 0; i < listlen; i++) {
        if (strlen(commands[i].cmd) == strlen(my_pair.value) && strncmp(commands[i].cmd, my_pair.value, strlen(my_pair.value)) == 0)
            return process_mapping(my_pair.what, i);
    }
    return FALSE;
}
static gboolean
browser_settings(const Arg *arg) {
    char line[255];
    if (!arg->s)
        return FALSE;
    strncpy(line, arg->s, 254);
    if (process_set_line(line))
        return TRUE;
    else
        return FALSE;
}

char *
search_word(int whichword) {
    int k = 0;
    static char word[240];
    char *c = my_pair.line;

    while (isspace(*c) && *c)
        c++;

    switch (whichword) {
        case 0:
            while (*c && !isspace (*c) && *c != '=' && k < 240) {
                word[k++] = *c;
                c++;
            }
            word[k] = '\0';
            strncpy(my_pair.what, word, 20);
        break;
        case 1:
            while (*c && k < 240) {
                word[k++] = *c;
                c++;
            }
            word[k] = '\0';
            strncpy(my_pair.value, word, 240);
        break;
    }

    return c;
}

static gboolean
process_set_line(char *line) {
    char              *c;
    int               listlen, i;
    gboolean          boolval;
    WebKitWebSettings *settings;

    settings = webkit_web_view_get_settings(webview);
    my_pair.line = line;
    c = search_word(0);
    if (!strlen(my_pair.what))
        return FALSE;

    while (isspace(*c) && *c)
        c++;

    if (*c == ':' || *c == '=')
        c++;

    my_pair.line = c;
    c = search_word(1);

    listlen = LENGTH(browsersettings);
    for (i = 0; i < listlen; i++) {
        if (strlen(browsersettings[i].name) == strlen(my_pair.what) && strncmp(browsersettings[i].name, my_pair.what, strlen(my_pair.what)) == 0) {
            /* mandatory argument not provided */
            if (strlen(my_pair.value) == 0)
                return FALSE;
            /* process qmark? */
            if (strlen(my_pair.what) == 5 && strncmp("qmark", my_pair.what, 5) == 0) {
                return (process_save_qmark(my_pair.value, webview));
            }
            /* interpret boolean values */
            if (browsersettings[i].boolval) {
                if (strncmp(my_pair.value, "on", 2) == 0 || strncmp(my_pair.value, "true", 4) == 0 || strncmp(my_pair.value, "ON", 2) == 0 || strncmp(my_pair.value, "TRUE", 4) == 0) {
                    boolval = TRUE;
                } else if (strncmp(my_pair.value, "off", 3) == 0 || strncmp(my_pair.value, "false", 5) == 0 || strncmp(my_pair.value, "OFF", 3) == 0 || strncmp(my_pair.value, "FALSE", 5) == 0) {
                    boolval = FALSE;
                } else {
                    return FALSE;
                }
            } else if (browsersettings[i].colourval) {
                /* interpret as hexadecimal colour */
                if (!parse_colour(my_pair.value)) {
                    return FALSE;
                }
            }
            if (browsersettings[i].var != NULL) {
                /* write value into internal variable */
                /*if (browsersettings[i].intval) {
                    browsersettings[i].var = atoi(my_pair.value);
                } else {*/
                    strncpy(browsersettings[i].var, my_pair.value, strlen(my_pair.value) + 1);
                /*}*/
            }
            if (strlen(browsersettings[i].webkit) > 0) {
                /* activate appropriate webkit setting */
                if (browsersettings[i].boolval) {
                    g_object_set((GObject*)settings, browsersettings[i].webkit, boolval, NULL);
                } else if (browsersettings[i].intval) {
                    g_object_set((GObject*)settings, browsersettings[i].webkit, atoi(my_pair.value), NULL);
                } else {
                    g_object_set((GObject*)settings, browsersettings[i].webkit, my_pair.value, NULL);
                }
                webkit_web_view_set_settings(webview, settings);
            }
            /* toggle proxy usage? */
            if (strlen(my_pair.what) == 5 && strncmp("proxy", my_pair.what, 5) == 0) {
                toggle_proxy(boolval);
            }
            /* reload page? */
            if (browsersettings[i].reload)
                webkit_web_view_reload(webview);
            return TRUE;
        }
    }
    return FALSE;
}

gboolean
parse_colour(char *color) {
    char goodcolor[8];
    int colorlen;

    colorlen = (int)strlen(color);

    goodcolor[0] = '#';
    goodcolor[7] = '\0';

    /* help the user a bit by making string like
       #a10 and strings like ffffff full 6digit
       strings with # in front :)
     */

    if (color[0] == '#') {
        switch (colorlen) {
            case 7:
                strncpy(goodcolor, color, 7);
            break;
            case 4:
                goodcolor[1] = color[1];
                goodcolor[2] = color[1];
                goodcolor[3] = color[2];
                goodcolor[4] = color[2];
                goodcolor[5] = color[3];
                goodcolor[6] = color[3];
            break;
            case 2:
                goodcolor[1] = color[1];
                goodcolor[2] = color[1];
                goodcolor[3] = color[1];
                goodcolor[4] = color[1];
                goodcolor[5] = color[1];
                goodcolor[6] = color[1];
            break;
        }
    } else {
        switch (colorlen) {
            case 6:
                strncpy(&goodcolor[1], color, 6);
            break;
            case 3:
                goodcolor[1] = color[0];
                goodcolor[2] = color[0];
                goodcolor[3] = color[1];
                goodcolor[4] = color[1];
                goodcolor[5] = color[2];
                goodcolor[6] = color[2];
            break;
            case 1:
                goodcolor[1] = color[0];
                goodcolor[2] = color[0];
                goodcolor[3] = color[0];
                goodcolor[4] = color[0];
                goodcolor[5] = color[0];
                goodcolor[6] = color[0];
            break;
        }
    }

    if (strlen (goodcolor) != 7) {
        return FALSE;
    } else {
        strncpy(color, goodcolor, 8);
        return TRUE;
    }
}

gboolean
process_line(char *line) {
    char *c = line;

    while (isspace(*c))
        c++;
    /* Ignore blank lines.  */
    if (c[0] == '\0')
        return TRUE;
    if (strncmp(c, "map", 3) == 0) {
        c += 4;
        return process_map_line(c);
    } else if (strncmp(c, "set", 3) == 0) {
        c += 4;
        return process_set_line(c);
    }
    return FALSE;
}

void
toggle_proxy(gboolean onoff) {
    SoupURI *proxy_uri;
    Arg     a;
    char    *filename, *new;
    int     len;

    if (onoff == FALSE)  {
        g_object_set(session, "proxy-uri", NULL);
        a.i = Info;
        a.s = "Proxy deactivated";
        echo(&a);
    } else  {
        filename = (char *)g_getenv("http_proxy");
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
            a.i = Info;
            a.s = "Proxy activated";
            echo(&a);
        } 
    }
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
    int max = gtk_adjustment_get_upper(adjust_v) - gtk_adjustment_get_page_size(adjust_v);
    int val = (int)(gtk_adjustment_get_value(adjust_v) / max * 100);
    char *markup;
#ifdef ENABLE_WGET_PROGRESS_BAR
    double progress;
    char progressbar[progressbartick + 1];
    WebKitDownloadStatus status;

    g_object_get((GObject*)webview, "progress", &progress, NULL);
#endif

    if (max == 0)
        sprintf(&scroll_state[0], "All");
    else if (val == 0)
        sprintf(&scroll_state[0], "Top");
    else if (val == 100)
        sprintf(&scroll_state[0], "Bot");
    else
        sprintf(&scroll_state[0], "%d%%", val);
#ifdef ENABLE_WGET_PROGRESS_BAR
    if (activeDownload != NULL) {
        status = webkit_download_get_status(activeDownload);
        if (status == WEBKIT_DOWNLOAD_STATUS_STARTED || status == WEBKIT_DOWNLOAD_STATUS_CREATED) {
            progress = (gint)(webkit_download_get_progress(activeDownload) * 100);
            ascii_bar(progressbartick, (int)(progress * progressbartick / 100), (char*)progressbar);
            markup = (char*)g_markup_printf_escaped("<span font=\"%s\">%.0d%c %c%s%c %s</span>",
                statusfont, count, current_modkey, progressborderleft, progressbar, progressborderright, scroll_state);
        } else {
            markup = (char*)g_markup_printf_escaped("<span font=\"%s\">%.0d%c %s</span>", statusfont, count, current_modkey, scroll_state);
        }
    } else if ((webkit_web_view_get_load_status(webview) != WEBKIT_LOAD_FINISHED) &&
		(activeDownload != NULL)) {
        ascii_bar(progressbartick, (int)(progress * progressbartick), (char*)progressbar);
        markup = (char*)g_markup_printf_escaped("<span font=\"%s\">%.0d%c %c%s%c %s</span>",
            statusfont, count, current_modkey, progressborderleft, progressbar, progressborderright, scroll_state);
    } else
#endif
    markup = (char*)g_markup_printf_escaped("<span font=\"%s\">%.0d%c %s</span>", statusfont, count, current_modkey, scroll_state);
    gtk_label_set_markup(GTK_LABEL(status_state), markup);
}

void
setup_modkeys() {
    unsigned int i;
    modkeys = calloc(LENGTH(keys) + 1, sizeof(char));
    char *ptr = modkeys;

    for (i = 0; i < LENGTH(keys); i++)
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
        gtk_window_set_wmclass(GTK_WINDOW(window), "vimprobable2", "Vimprobable2");
    }
    gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);
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
    inspector = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(webview));

    clipboards[0] = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    clipboards[1] = gtk_clipboard_get(GDK_NONE);
    setup_settings();
    gdk_color_parse(statusbgcolor, &bg);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &bg);
    gtk_widget_set_name(GTK_WIDGET(window), "Vimprobable2");
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
#ifdef ENABLE_COOKIE_SUPPORT
    SoupCookieJar *cookiejar;
#endif
    session = webkit_get_default_session();
    g_object_set((GObject*)settings, "default-font-size", DEFAULT_FONT_SIZE, NULL);
    g_object_set((GObject*)settings, "enable-scripts", enablePlugins, NULL);
    g_object_set((GObject*)settings, "enable-plugins", enablePlugins, NULL);
    filename = g_strdup_printf(USER_STYLESHEET);
    filename = g_strdup_printf("file://%s", filename);
    g_object_set((GObject*)settings, "user-stylesheet-uri", filename, NULL);
    g_object_set((GObject*)settings, "user-agent", useragent, NULL);
    g_object_get((GObject*)settings, "zoom-step", &zoomstep, NULL);
    webkit_web_view_set_settings(webview, settings);
#ifdef ENABLE_COOKIE_SUPPORT
    filename = g_strdup_printf(COOKIES_STORAGE_FILENAME);
    cookiejar = soup_cookie_jar_text_new(filename, COOKIES_STORAGE_READONLY);
    g_free(filename);
    soup_session_add_feature(session, (SoupSessionFeature*)cookiejar);
#endif
    /* proxy */
    if (use_proxy == TRUE) {
        filename = (char *)g_getenv("http_proxy");
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
    NULL);
    /* inspector */
    g_signal_connect((GObject*)inspector,
        "inspect-web-view",                             (GCallback)inspector_inspect_web_view_cb,   NULL);
}

int
main(int argc, char *argv[]) {
    static Arg a;
    static char url[256] = "";
    static gboolean ver = false;
    static GOptionEntry opts[] = {
            { "version", 'v', 0, G_OPTION_ARG_NONE, &ver, "print version", NULL },
            { "embed", 'e', 0, G_OPTION_ARG_STRING, &winid, "embedded", NULL },
            { NULL }
    };
    static GError *err;
    args = argv;

    /* command line argument: version */
    if (!gtk_init_with_args(&argc, &argv, "[<uri>]", opts, NULL, &err)) {
        g_printerr("can't init gtk: %s\n", err->message);
        g_error_free(err);
        return EXIT_FAILURE;
    }

    if (ver) {
        printf("%s\n", useragent);
        return EXIT_SUCCESS;
    }

    if (!g_thread_supported())
        g_thread_init(NULL);

    if (winid)
        embed = atoi(winid);

    setup_modkeys();
    make_keyslist();
    setup_gui();

    /* read config file */
    if (!read_rcfile()) {
        a.i = Error;
        a.s = g_strdup_printf("Error in config file");
        echo(&a);
    }

    /* command line argument: URL */
    if (argc > 1) {
        strncpy(url, argv[argc - 1], 255);
    } else {
        strncpy(url, startpage, 255);
    }

    a.i = TargetCurrent;
    a.s = url;
    open(&a);
    gtk_main();

    return EXIT_SUCCESS;
}
