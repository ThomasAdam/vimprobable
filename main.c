/*
    (c) 2009 by Leon Winter
    (c) 2009-2012 by Hannes Schueller
    (c) 2009-2010 by Matto Fransen
    (c) 2010-2011 by Hans-Peter Deifel
    (c) 2010-2011 by Thomas Adam
    (c) 2011 by Albert Kim
    (c) 2011 by Daniel Carl
    (c) 2012 by Matthew Carter
    see LICENSE file
*/

#include <X11/Xlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include "includes.h"
#include "vimprobable.h"
#include "utilities.h"
#include "callbacks.h"
#include "javascript.h"

/* the CLEAN_MOD_*_MASK defines have all the bits set that will be stripped from the modifier bit field */
#define CLEAN_MOD_NUMLOCK_MASK (GDK_MOD2_MASK)
#define CLEAN_MOD_BUTTON_MASK (GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK|GDK_BUTTON4_MASK|GDK_BUTTON5_MASK)

/* remove unused bits, numlock symbol and buttons from keymask */
#define CLEAN(mask) (mask & (GDK_MODIFIER_MASK) & ~(CLEAN_MOD_NUMLOCK_MASK) & ~(CLEAN_MOD_BUTTON_MASK))

#define IS_ESCAPE(event) (IS_ESCAPE_KEY(CLEAN(event->state), event->keyval))
#define IS_ESCAPE_KEY(s, k) ((s == 0 && k == GDK_Escape) || \
        (s == GDK_CONTROL_MASK && k == GDK_bracketleft))

/* callbacks here */
static void inputbox_activate_cb(GtkEntry *entry, gpointer user_data);
static gboolean inputbox_keypress_cb(GtkEntry *entry, GdkEventKey *event);
static gboolean inputbox_keyrelease_cb(GtkEntry *entry, GdkEventKey *event);
static gboolean inputbox_changed_cb(GtkEditable *entry, gpointer user_data);
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
static void webview_open_js_window_cb(WebKitWebView* temp_view, GParamSpec param_spec);
static gboolean webview_new_window_cb(WebKitWebView *webview, WebKitWebFrame *frame, WebKitNetworkRequest *request,
                        WebKitWebNavigationAction *action, WebKitWebPolicyDecision *decision, gpointer user_data);
static WebKitWebView* webview_open_in_new_window_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data);
static void webview_progress_changed_cb(WebKitWebView *webview, int progress, gpointer user_data);
static void webview_title_changed_cb(WebKitWebView *webview, WebKitWebFrame *frame, char *title, gpointer user_data);
static void window_destroyed_cb(GtkWidget *window, gpointer func_data);
static gboolean blank_cb(void);

/* functions */
static gboolean bookmark(const Arg *arg);
static gboolean browser_settings(const Arg *arg);
static gboolean commandhistoryfetch(const Arg *arg);
static gboolean complete(const Arg *arg);
static gboolean descend(const Arg *arg);
gboolean echo(const Arg *arg);
static gboolean focus_input(const Arg *arg);
static gboolean open_editor(const Arg *arg);
void _resume_from_editor(GPid child_pid, int status, gpointer data);
static gboolean input(const Arg *arg);
static gboolean open_inspector(const Arg * arg);
static gboolean navigate(const Arg *arg);
static gboolean number(const Arg *arg);
static gboolean open_arg(const Arg *arg);
static gboolean open_remembered(const Arg *arg);
static gboolean paste(const Arg *arg);
static gboolean quickmark(const Arg *arg);
static gboolean quit(const Arg *arg);
static gboolean revive(const Arg *arg);
static gboolean print_frame(const Arg *arg);
static gboolean search(const Arg *arg);
static gboolean set(const Arg *arg);
static gboolean script(const Arg *arg);
static gboolean scroll(const Arg *arg);
static gboolean search_tag(const Arg *arg);
static gboolean yank(const Arg *arg);
static gboolean view_source(const Arg * arg);
static gboolean zoom(const Arg *arg);
static gboolean fake_key_event(const Arg *arg);

static void clear_focus(void);
static void update_url(const char *uri);
static void setup_modkeys(void);
static void setup_gui(void);
static void setup_settings(void);
static void setup_signals(void);
static void ascii_bar(int total, int state, char *string);
static gchar *jsapi_ref_to_string(JSContextRef context, JSValueRef ref);
static void jsapi_evaluate_script(const gchar *script, gchar **value, gchar **message);
static void download_progress(WebKitDownload *d, GParamSpec *pspec);
static void set_widget_font_and_color(GtkWidget *widget, const char *font_str,
                const char *bg_color_str, const char *fg_color_str);
static void scripts_run_user_file(void);

static gboolean history(void);
static gboolean process_set_line(char *line);
void save_command_history(char *line);
void toggle_proxy(gboolean onoff);
void toggle_scrollbars(gboolean onoff);
void set_default_winsize(const char * const size);

gboolean process_keypress(GdkEventKey *event);
void fill_suggline(char * suggline, const char * command, const char *fill_with);
GtkWidget * fill_eventbox(const char * completion_line);
static void mop_up(void);

#include "main.h"

/* variables */
static GtkWindow *window;
static GtkWidget *viewport;
static GtkBox *box;
static GtkScrollbar *scroll_h;
static GtkScrollbar *scroll_v;
static GtkAdjustment *adjust_h;
static GtkAdjustment *adjust_v;
static GtkWidget *inputbox;
static GtkWidget *eventbox;
static GtkBox *statusbar;
static GtkWidget *status_url;
static GtkWidget *status_state;
static WebKitWebView *webview;
static SoupSession *session;
static GtkClipboard *clipboards[2];
static GdkKeymap *keymap;

static char **args;
static unsigned int mode = ModeNormal;
static unsigned int count = 0;
static float zoomstep;
char *modkeys;
static char current_modkey;
static char *search_handle;
static gboolean search_direction;
static gboolean echo_active = TRUE;
static WebKitWebInspector *inspector;

static GdkNativeWindow embed = 0;
static char *configfile = NULL;
static char *winid = NULL;

static char rememberedURI[1024] = "";
static char followTarget[8] = "";
char *error_msg = NULL;
char *config_base = NULL;
static gboolean manual_focus = FALSE;

GList *activeDownloads;

#include "config.h"
#include "keymap.h"

GList *commandhistory = NULL;
int commandpointer = 0;

KeyList *keylistroot = NULL;

/* Cookie support. */
#ifdef ENABLE_COOKIE_SUPPORT
static SoupCookieJar *session_cookie_jar = NULL;
static SoupCookieJar *file_cookie_jar = NULL;
static time_t cookie_timeout = 4800;
static char *cookie_store;
static void setup_cookies(void);
static char *get_cookies(SoupURI *soup_uri);
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
    g_free(a.s);
    scripts_run_user_file();

    if (mode == ModeInsert || mode == ModeHints) {
        Arg a = { .i = ModeNormal };
        set(&a);
    }
    manual_focus = FALSE;
}

void
webview_load_finished_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data) {
    WebKitWebSettings *settings = webkit_web_view_get_settings(webview);
    gboolean scripts;

    g_object_get(settings, "enable-scripts", &scripts, NULL);
    if (escape_input_on_load && scripts && !manual_focus && !gtk_widget_is_focus(inputbox)) {
        clear_focus();
    }
    if (HISTORY_MAX_ENTRIES > 0)
        history();
    update_state();
}

void
webview_open_js_window_cb(WebKitWebView* temp_view, GParamSpec param_spec) {
    /* retrieve the URI of the temporary webview */
    Arg a = { .i = TargetNew, .s = (char*)webkit_web_view_get_uri(temp_view) };
    /* clean up */
    webkit_web_view_stop_loading(temp_view);
    gtk_widget_destroy(GTK_WIDGET(temp_view));
    /* open the requested window */
    open_arg(&a);
}

static WebKitWebView *
webview_open_in_new_window_cb(WebKitWebView *webview, WebKitWebFrame *frame, gpointer user_data) {
    if (rememberedURI != NULL && strlen(rememberedURI) > 0) {
        if (strncmp(rememberedURI, "javascript:", 11) != 0) {
            Arg a = { .i = TargetNew, .s = rememberedURI };
            open_arg(&a);
            return NULL;
        }
    }
    /* create a temporary webview to execute the script in */
    WebKitWebView *temp_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    /* wait until the new webview receives its new URI */
    g_object_connect(temp_view, "signal::notify::uri", G_CALLBACK(webview_open_js_window_cb), NULL, NULL);
    return temp_view;
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
    WebKitDownloadStatus status;

    filename = webkit_download_get_suggested_filename(download);
    if (filename == NULL || strlen(filename) == 0) {
        filename = "vimprobable_download";
    }
    path = g_build_filename(g_strdup_printf(DOWNLOADS_PATH), filename, NULL);
    uri = g_strconcat("file://", path, NULL);
    webkit_download_set_destination_uri(download, uri);
    g_free(uri);
    size = (uint32_t)webkit_download_get_total_size(download);
    if (size > 0)
        echo_message(Info, "Download %s started (expected size: %u bytes)...", filename, size);
    else
        echo_message(Info, "Download %s started (unknown size)...", filename);
    activeDownloads = g_list_prepend(activeDownloads, download);
    g_signal_connect(download, "notify::progress", G_CALLBACK(download_progress), NULL);
    g_signal_connect(download, "notify::status", G_CALLBACK(download_progress), NULL);
    status = webkit_download_get_status(download);
    if (status == WEBKIT_DOWNLOAD_STATUS_CREATED)
        webkit_download_start(download);
    update_state();
    return TRUE;
}

gboolean
blank_cb(void) {
    return TRUE;
}

void
download_progress(WebKitDownload *d, GParamSpec *pspec) {
    WebKitDownloadStatus status = webkit_download_get_status(d);

    if (status != WEBKIT_DOWNLOAD_STATUS_STARTED && status != WEBKIT_DOWNLOAD_STATUS_CREATED) {
        if (status != WEBKIT_DOWNLOAD_STATUS_FINISHED) {
            echo_message(Error, "Error while downloading %s", webkit_download_get_suggested_filename(d));
        } else {
            echo_message(Info, "Download %s finished", webkit_download_get_suggested_filename(d));
        }
        activeDownloads = g_list_remove(activeDownloads, d);
    }
    update_state();
}


gboolean
process_keypress(GdkEventKey *event) {
    KeyList *current;
    guint keyval;
    GdkModifierType irrelevant;

    /* Get a mask of modifiers that shouldn't be considered for this event.
     * E.g.: It shouldn't matter whether ';' is shifted or not. */
    gdk_keymap_translate_keyboard_state(keymap, event->hardware_keycode,
            event->state, event->group, &keyval, NULL, NULL, &irrelevant);

    current = keylistroot;

    while (current != NULL) {
        if (current->Element.mask == (CLEAN(event->state) & ~irrelevant)
                && (current->Element.modkey == current_modkey
                    || (!current->Element.modkey && !current_modkey)
                    || current->Element.modkey == GDK_VoidSymbol )    /* wildcard */
                && current->Element.key == keyval
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
    guint keyval;
    GdkModifierType irrelevant;

    /* Get a mask of modifiers that shouldn't be considered for this event.
     * E.g.: It shouldn't matter whether ';' is shifted or not. */
    gdk_keymap_translate_keyboard_state(keymap, event->hardware_keycode,
            event->state, event->group, &keyval, NULL, NULL, &irrelevant);

    switch (mode) {
    case ModeNormal:
        if ((CLEAN(event->state) & ~irrelevant) == 0) {
            if (IS_ESCAPE(event)) {
                echo_message(Info, "");
                g_free(a.s);
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
        if (IS_ESCAPE(event)) {
            a.i = Silent;
            a.s = g_strdup("hints.clearFocus();");
            script(&a);
            g_free(a.s);
            a.i = ModeNormal;
            return set(&a);
        } else if (CLEAN(event->state) & GDK_CONTROL_MASK) {
            /* keybindings of non-printable characters */
            if (process_keypress(event) == TRUE) return TRUE;
        }
    case ModePassThrough:
        if (IS_ESCAPE(event)) {
            echo_message(Info, "");
            set(&a);
            return TRUE;
        }
        break;
    case ModeSendKey:
        echo_message(Info, "");
        set(&a);
        break;
    }
    return FALSE;
}

void
set_widget_font_and_color(GtkWidget *widget, const char *font_str, const char *bg_color_str,
        const char *fg_color_str) {
    GdkColor fg_color;
    GdkColor bg_color;
    PangoFontDescription *font;

    font = pango_font_description_from_string(font_str);
    gtk_widget_modify_font(widget, font);
    pango_font_description_free(font);

    if (fg_color_str)
        gdk_color_parse(fg_color_str, &fg_color);
    if (bg_color_str)
        gdk_color_parse(bg_color_str, &bg_color);

    gtk_widget_modify_text(widget, GTK_STATE_NORMAL, fg_color_str ? &fg_color : NULL);
    gtk_widget_modify_base(widget, GTK_STATE_NORMAL, bg_color_str ? &bg_color : NULL);

    return;
}

void
webview_hoverlink_cb(WebKitWebView *webview, char *title, char *link, gpointer data) {
    const char *uri = webkit_web_view_get_uri(webview);
    char *markup;

    memset(rememberedURI, 0, 1024);
    if (link) {
        markup = g_markup_printf_escaped("<span font=\"%s\">Link: %s</span>", statusfont, link);
        gtk_label_set_markup(GTK_LABEL(status_url), markup);
        strncpy(rememberedURI, link, 1024);
        g_free(markup);
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
    gboolean success = FALSE, forward = FALSE;

    a.i = HideCompletion;
    complete(&a);
    if (length == 0)
        return;
    text = (char*)gtk_entry_get_text(entry);

    /* move focus from inputbox to print potential messages that could not be
     * printed as long as the inputbox is focused */
    gtk_widget_grab_focus(GTK_WIDGET(webview));

    if (length > 1 && text[0] == ':') {
        success = process_line((text + 1));
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
    } else if (text[0] == '.' || text[0] == ',' || text[0] == ';') {
        a.i = Silent;
        a.s = g_strdup_printf("hints.fire();");
        script(&a);
        g_free(a.s);
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

    if (mode == ModeHints) {
        if (event->keyval == GDK_Tab) {
            a.i = Silent;
            a.s = g_strdup_printf("hints.focusNextHint();");
            script(&a);
            g_free(a.s);
            update_state();
            return TRUE;
        }
        if (event->keyval == GDK_ISO_Left_Tab) {
            a.i = Silent;
            a.s = g_strdup_printf("hints.focusPreviousHint();");
            script(&a);
            g_free(a.s);
            update_state();
            return TRUE;
        }
        if (event->keyval == GDK_Return) {
            a.i = Silent;
            a.s = g_strdup_printf("hints.fire();");
            script(&a);
            g_free(a.s);
            update_state();
            return TRUE;
        }
    }
    switch (event->keyval) {
        case GDK_bracketleft:
        case GDK_Escape:
            if (!IS_ESCAPE(event)) break;
            a.i = HideCompletion;
            complete(&a);
            a.i = ModeNormal;
            commandpointer = 0;
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

    if (mode == ModeHints) {
        if ((CLEAN(event->state) & GDK_SHIFT_MASK) &&
                (CLEAN(event->state) & GDK_CONTROL_MASK) &&
                (event->keyval == GDK_BackSpace)) {
            count /= 10;
            a.i = Silent;
            a.s = g_strdup_printf("hints.updateHints(%d);", count);
            script(&a);
            g_free(a.s);
            update_state();
            return TRUE;
        }

        numval = g_unichar_digit_value((gunichar) gdk_keyval_to_unicode(event->keyval));
        if ((numval >= 1 && numval <= 9) || (numval == 0 && count)) {
            /* allow a zero as non-first number */
            count = (count ? count * 10 : 0) + numval;
            a.i = Silent;
            a.s = g_strdup_printf("hints.updateHints(%d);", count);
            script(&a);
            g_free(a.s);
            update_state();
            return TRUE;
        }
    }

    return FALSE;
}

gboolean
notify_event_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    int i;
    WebKitHitTestResult *result;
    WebKitHitTestResultContext context;
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
        result = webkit_web_view_get_hit_test_result(WEBKIT_WEB_VIEW(widget), (GdkEventButton*)event);
        g_object_get(result, "context", &context, NULL);
        if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE) {
            Arg a = { .i = ModeInsert };
            set(&a);
            manual_focus = TRUE;
        }
    } else if (mode == ModeInsert && event->type == GDK_BUTTON_RELEASE) {
        result = webkit_web_view_get_hit_test_result(WEBKIT_WEB_VIEW(widget), (GdkEventButton*)event);
        g_object_get(result, "context", &context, NULL);
        if (!(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE)) {
            Arg a = { .i = ModeNormal };
            set(&a);
        }
    } else {
        gchar *value = NULL, *message = NULL;
        jsapi_evaluate_script("window.getSelection().focusNode", &value, &message);
        if (value && !strcmp(value, "[object HTMLFormElement]")) {
            Arg a = { .i = ModeInsert, .s = NULL };
            set(&a);
            manual_focus = TRUE;
        }
        g_free(value);
        g_free(message);
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
            (text[0] == '.' || text[0] == ',' || text[0] == ';')) {
        a.i = Silent;
        switch (text[0]) {
            case '.':
                a.s = g_strconcat("hints.createHints('", text + 1, "', 'f');", NULL);
                break;

            case ',':
                a.s = g_strconcat("hints.createHints('", text + 1, "', 'F');", NULL);
                break;

            case ';':
                a.s = NULL;
                switch (text[1]) {
                    case 's':
                        a.s = g_strconcat("hints.createHints('", text + 2, "', 's');", NULL);
                        break;
                    case 'y':
                        a.s = g_strconcat("hints.createHints('", text + 2, "', 'y');", NULL);
                        break;
                    case 'o':
                        a.s = g_strconcat("hints.createHints('", text + 2, "', 'f');", NULL);
                        break;
                    case 't': case 'w':
                        a.s = g_strconcat("hints.createHints('", text + 2, "', 'F');", NULL);
                        break;
                    case 'O': case 'T': case 'W':
                        a.s = g_strconcat("hints.createHints('", text + 2, "', 'O');", NULL);
                        break;
                    case 'i':
                        a.s = g_strconcat("hints.createHints('", text + 2, "', 'i');", NULL);
                        break;
                    case 'I':
                        a.s = g_strconcat("hints.createHints('", text + 2, "', 'I');", NULL);
                        break;
                }
                break;
        }
        count = 0;
        if (a.s) {
            script(&a);
            g_free(a.s);
        }

        return TRUE;
    } else if (length == 0 && followTarget[0]) {
        mode = ModeNormal;
        a.i = Silent;
        a.s = g_strdup("hints.clearHints();");
        script(&a);
        g_free(a.s);
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
    char      *markup, *markup_tmp;

    row = GTK_BOX(gtk_hbox_new(FALSE, 0));
    row_eventbox = gtk_event_box_new();
    gdk_color_parse(completionbgcolor[0], &color);
    gtk_widget_modify_bg(row_eventbox, GTK_STATE_NORMAL, &color);
    el = gtk_label_new(NULL);
    markup_tmp = g_markup_escape_text(completion_line, strlen(completion_line));
    markup = g_strconcat("<span font=\"", completionfont[0], "\" color=\"", completioncolor[0], "\">",
        markup_tmp, "</span>", NULL);
    gtk_label_set_markup(GTK_LABEL(el), markup);
    g_free(markup_tmp);
    g_free(markup);
    gtk_misc_set_alignment(GTK_MISC(el), 0, 0);
    gtk_box_pack_start(row, el, TRUE, TRUE, 2);
    gtk_container_add(GTK_CONTAINER(row_eventbox), GTK_WIDGET(row));
    return row_eventbox;
}

gboolean
complete(const Arg *arg) {
    char *str, *p, *s, *markup, *entry, *searchfor, command[32] = "", suggline[512] = "", **suggurls;
    size_t listlen, len, cmdlen;
    int i, spacepos;
    Listelement *elementlist = NULL, *elementpointer;
    gboolean highlight = FALSE;
    GtkBox *row;
    GtkWidget *row_eventbox, *el;
    GtkBox *_table;
    GdkColor color;
    static GtkWidget *table, *top_border;
    static char *prefix;
    static char **suggestions;
    static GtkWidget **widgets;
    static int n = 0, m, current = -1;

    str = (char*)gtk_entry_get_text(GTK_ENTRY(inputbox));
    len = strlen(str);

    /* Get the length of the list of commands for completion.  We need this to
     * malloc/realloc correctly.
     */
    listlen = LENGTH(commands);

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
        prefix = g_strdup(str);
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
            /* command completion */
            listlen = LENGTH(commands);
			for (i = 0; i < listlen; i++) {
                if (commands[i].cmd == NULL)
                    break;
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
            suggurls = malloc(sizeof(char*) * listlen);
            if (suggurls == NULL) {
                return FALSE;
            }
            spacepos = strcspn(str, " ");
            searchfor = (str + spacepos + 1);
            strncpy(command, (str + 1), spacepos - 1);
            if (strlen(command) == 3 && strncmp(command, "set", 3) == 0) {
                /* browser settings */
                listlen = LENGTH(browsersettings);
                for (i = 0; i < listlen; i++) {
                    if (n < MAX_LIST_SIZE && strstr(browsersettings[i].name, searchfor) != NULL) {
                        /* match */
                        fill_suggline(suggline, command, browsersettings[i].name);
                        /* FIXME(HP): This memory is never freed */
                        suggurls[n] = (char *)malloc(sizeof(char) * 512 + 1);
                        strncpy(suggurls[n], suggline, 512);
                        suggestions[n] = suggurls[n];
                        row_eventbox = fill_eventbox(suggline);
                        gtk_box_pack_start(_table, GTK_WIDGET(row_eventbox), FALSE, FALSE, 0);
                        widgets[n++] = row_eventbox;
                    }

                }
            } else if (strlen(command) == 2 && strncmp(command, "qt", 2) == 0) {
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
                /* FIXME(HP): This memory is never freed */
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
        /* TA:  FIXME - this needs rethinking entirely. */
        {
            GtkWidget **widgets_temp = realloc(widgets, sizeof(*widgets) * n);
            if (widgets_temp == NULL && widgets == NULL) {
                fprintf(stderr, "Couldn't realloc() widgets\n");
                exit(1);
            }
            widgets = widgets_temp;
            char **suggestions_temp = realloc(suggestions, sizeof(*suggestions) * n);
            if (suggestions_temp == NULL && suggestions == NULL) {
                fprintf(stderr, "Couldn't realloc() suggestions\n");
                exit(1);
            }
            suggestions = suggestions_temp;
        }
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
    new = malloc(len + 1);
    memcpy(new, source, len);
    new[len] = '\0';
    webkit_web_view_load_uri(webview, new);
    free(new);
    return TRUE;
}

gboolean
echo(const Arg *arg) {
    int index = !arg->s ? 0 : arg->i & (~NoAutoHide);

    if (index < Info || index > Error)
        return TRUE;

    if (!gtk_widget_is_focus(GTK_WIDGET(inputbox))) {
        set_widget_font_and_color(inputbox, urlboxfont[index], urlboxbgcolor[index], urlboxcolor[index]);
        gtk_entry_set_text(GTK_ENTRY(inputbox), !arg->s ? "" : arg->s);
    }

    return TRUE;
}

static gboolean
open_inspector(const Arg * arg) {
    gboolean inspect_enabled;
    WebKitWebSettings *settings;

    settings = webkit_web_view_get_settings(webview);
    g_object_get(G_OBJECT(settings), "enable-developer-extras", &inspect_enabled, NULL);
    if (inspect_enabled) {
        webkit_web_inspector_show(inspector);
        return TRUE;
    } else {
        echo_message(Error, "Webinspector is not enabled");
        return FALSE;
    }
}

gboolean
input(const Arg *arg) {
    int pos = 0;
    count = 0;
    const char *url;
    int index = Info;
    Arg a;

    /* if inputbox hidden, show it again */
    if (!gtk_widget_get_visible(inputbox))
        gtk_widget_set_visible(inputbox, TRUE);

    update_state();

    /* Set the colour and font back to the default, so that we don't still
     * maintain a red colour from a warning from an end of search indicator,
     * etc.
     */
    set_widget_font_and_color(inputbox, urlboxfont[index], urlboxbgcolor[index], urlboxcolor[index]);

    /* to avoid things like :open URL :open URL2  or :open :open URL */
    gtk_entry_set_text(GTK_ENTRY(inputbox), "");
    gtk_editable_insert_text(GTK_EDITABLE(inputbox), arg->s, -1, &pos);
    if (arg->i & InsertCurrentURL && (url = webkit_web_view_get_uri(webview)))
        gtk_editable_insert_text(GTK_EDITABLE(inputbox), url, -1, &pos);

    gtk_widget_grab_focus(inputbox);
    gtk_editable_set_position(GTK_EDITABLE(inputbox), -1);

    if (arg->s[0] == '.' || arg->s[0] == ',' || arg->s[0] == ';') {
        mode = ModeHints;
        memset(followTarget, 0, 8);
        strncpy(followTarget, "current", 8);
        a.i = Silent;
        switch (arg->s[0]) {
            case '.':
                a.s = g_strdup("hints.createHints('', 'f');");
                break;

            case ',':
                a.s = g_strdup("hints.createHints('', 'F');");
                break;

            case ';':
                a.s = NULL;
                if (arg->s[1]) {
                    switch (arg->s[1]) {
                        case 's':
                            a.s = g_strdup("hints.createHints('', 's');");
                            break;
                        case 'y':
                            a.s = g_strdup("hints.createHints('', 'y');");
                            break;
                        case 'o':
                            a.s = g_strdup("hints.createHints('', 'f');");
                            break;
                        case 't': case 'w':
                            a.s = g_strdup("hints.createHints('', 'F');");
                            break;
                        case 'O': case 'T': case 'W':
                            a.s = g_strdup("hints.createHints('', 'O');");
                            break;
                        case 'i':
                            a.s = g_strdup("hints.createHints('', 'i');");
                            break;
                        case 'I':
                            a.s = g_strdup("hints.createHints('', 'I');");
                            break;
                    }
                }
                break;
        }
        count = 0;
        if (a.s) {
            script(&a);
            g_free(a.s);
        }
    }

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
    uri = g_strdup(source); /* copy string */
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
    char *argv[64];
    char *s = arg->s, *p = NULL, *new;
    Arg a = { .i = NavigationReload };
    int len;
    char *search_uri, *search_term;

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
        while(*s == ' ') /* strip leading whitespace */
            ++s;
        p = (s + strlen(s) - 1);
        while(*p == ' ') /* strip trailing whitespace */
            --p;
        *(p + 1) = '\0';
        len = strlen(s);
        new = NULL;
        /* check for external handlers */
        if (open_handler(s))
            return TRUE;        
        /* check for search engines */
        p = strchr(s, ' ');
        if (p) {                                                         /* check for search engines */
            *p = '\0';
            search_uri = find_uri_for_searchengine(s);
            if (search_uri != NULL) {
                search_term = soup_uri_encode(p+1, "&");
                new = g_strdup_printf(search_uri, search_term);
                g_free(search_term);
            }
            *p = ' ';
        }
        if (!new) {
            if (len > 3 && strstr(s, "://")) {                      /* valid url? */
                p = new = g_malloc(len + 1);
                while(*s != '\0') {                                     /* strip whitespaces */
                    if (*s != ' ')
                        *(p++) = *s;
                    ++s;
                }
                *p = '\0';
            } else if (strcspn(s, "/") == 0 || strcspn(s, "./") == 0) {  /* prepend "file://" */
                new = g_malloc(sizeof("file://") + len);
                strcpy(new, "file://");
                memcpy(&new[sizeof("file://") - 1], s, len + 1);
            } else if (p || !strchr(s, '.')) {                      /* whitespaces or no dot? */
                search_uri = find_uri_for_searchengine(defaultsearch);
                if (search_uri != NULL) {
                    search_term = soup_uri_encode(s, "&");
                    new = g_strdup_printf(search_uri, search_term);
                    g_free(search_term);
                }
            } else {                                                    /* prepend "http://" */
                new = g_malloc(sizeof("http://") + len);
                strcpy(new, "http://");
                memcpy(&new[sizeof("http://") - 1], s, len + 1);
            }
        }
        webkit_web_view_load_uri(webview, new);
        g_free(new);
    } else
        g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
    return TRUE;
}

gboolean
open_remembered(const Arg *arg)
{
    Arg a = {arg->i, rememberedURI};

    if (strcmp(rememberedURI, "")) {
        open_arg(&a);
    }
    return TRUE;
}

gboolean
yank(const Arg *arg) {
    const char *url, *content;

    if (arg->i & SourceSelection) {
        webkit_web_view_copy_clipboard(webview);
        if (arg->i & ClipboardPrimary)
            content = gtk_clipboard_wait_for_text(clipboards[0]);
        if (!content && arg->i & ClipboardGTK)
            content = gtk_clipboard_wait_for_text(clipboards[1]);
        if (content) {
            echo_message(Info, "Yanked %s", content);
            g_free((gpointer *)content);
        }
    } else {
        if (arg->i & SourceURL) {
            url = webkit_web_view_get_uri(webview);
        } else {
            url = arg->s;
        }
        if (!url)
            return TRUE;

        echo_message(Info, "Yanked %s", url);
        if (arg->i & ClipboardPrimary)
            gtk_clipboard_set_text(clipboards[0], url, -1);
        if (arg->i & ClipboardGTK)
            gtk_clipboard_set_text(clipboards[1], url, -1);
    }
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
    if (a.s) {
        open_arg(&a);
        g_free(a.s);
    }
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
        g_free((gpointer *)filename);
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
    g_free((gpointer *)filename);
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

    if (arg->s) {
        free(search_handle);
        search_handle = g_strdup(arg->s);
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
                    echo_message(Warning, "search hit %s, continuing at %s",
                            direction ? "BOTTOM" : "TOP",
                            direction ? "TOP" : "BOTTOM");
                } else
                    break;
            } else
                break;
        }
    } while(--count);
    if (!success) {
        echo_message(Error, "Pattern not found: %s", search_handle);
    }
    return TRUE;
}

gboolean
set(const Arg *arg) {
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
        echo_message(Info | NoAutoHide, "-- PASS THROUGH --");
        break;
    case ModeSendKey:
        echo_message(Info | NoAutoHide, "-- PASS TROUGH (next) --");
        break;
    case ModeInsert: /* should not be called manually but automatically */
        echo_message(Info | NoAutoHide, "-- INSERT --");
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
    g_free(fn);
    fn = NULL;
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
        if (strlen(buf)) {
            Arg x = { .s = buf };
            return open_arg(&x);
        } else {
            echo_message(Error, "Quickmark %d not defined", b);
            return false;
        }
    } else { return false; }
}

gboolean
script(const Arg *arg) {
    gchar *value = NULL, *message = NULL;
    char text[1024] = "";
    Arg a;
    WebKitNetworkRequest *request;
    WebKitDownload *download;

    if (!arg->s) {
        set_error("Missing argument.");
        return FALSE;
    }
    jsapi_evaluate_script(arg->s, &value, &message);
    if (message) {
        set_error(message);
        g_free(message);
        return FALSE;
    }
    g_free(message);
    if (arg->i != Silent && value) {
        echo_message(arg->i, value);
    }
    /* switch mode according to scripts return value */
    if (value) {
        if (strncmp(value, "done;", 5) == 0) {
            a.i = ModeNormal;
            set(&a);
        } else if (strncmp(value, "insert;", 7) == 0) {
            a.i = ModeInsert;
            set(&a);
            manual_focus = TRUE;
        } else if (strncmp(value, "save;", 5) == 0) {
            /* forced download */
            a.i = ModeNormal;
            set(&a);
            request = webkit_network_request_new((value + 5));
            download = webkit_download_new(request);
            webview_download_cb(webview, download, (gpointer *)NULL);
        } else if (strncmp(value, "yank;", 5) == 0) {
            /* yank link URL to clipboard */
            a.i = ModeNormal;
            set(&a);
            a.i = ClipboardPrimary | ClipboardGTK;
            a.s = (value + 5);
            yank(&a);
        } else if (strncmp(value, "colon;", 6) == 0) {
            /* use link URL for colon command */
            strncpy(text, (char *)gtk_entry_get_text(GTK_ENTRY(inputbox)), 1023);
            a.i = ModeNormal;
            set(&a);
            switch (text[1]) {
                case 'O':
                    a.s = g_strconcat(":open ", (value + 6), NULL);
                    break;
                case 'T': case 'W':
                    a.s = g_strconcat(":tabopen ", (value + 6), NULL);
                    break;
            }
            if (a.s) {
                input(&a);
                g_free(a.s);
            }
        } else if (strncmp(value, "error;", 6) == 0) {
            a.i = Error;
            set(&a);
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
fake_key_event(const Arg *a) {
    if(!embed) {
        return FALSE;
    }
    Display *xdpy;
    if ( (xdpy = XOpenDisplay(NULL)) == NULL ) {
        echo_message(Error, "Couldn't find the XDisplay.");
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
        echo_message(Error, "Zero pointer as argument! Check your config.h");
        return FALSE;
    }

    KeySym keysym;
    if( (keysym = XStringToKeysym(a->s)) == NoSymbol ) {
        echo_message(Error, "Couldn't translate %s to keysym", a->s );
        return FALSE;
    }
    
    if( (xk.keycode = XKeysymToKeycode(xdpy, keysym)) == NoSymbol ) {
        echo_message(Error, "Couldn't translate keysym to keycode");
        return FALSE;
    }
   
    xk.type = KeyPress;
    if( !XSendEvent(xdpy, embed, True, KeyPressMask, (XEvent *)&xk) ) {
        echo_message(Error, "XSendEvent failed");
        return FALSE;
    }
    XFlush(xdpy);

    return TRUE;
}

gboolean
commandhistoryfetch(const Arg *arg) {
    const int length = g_list_length(commandhistory);

    if (length > 0) {
        if (arg->i == DirectionPrev) {
            commandpointer = (length + commandpointer - 1) % length;
        } else {
            commandpointer = (length + commandpointer + 1) % length;
        }

        const char* command = (char *)g_list_nth_data(commandhistory, commandpointer);
        gtk_entry_set_text(GTK_ENTRY(inputbox), g_strconcat(":", command, NULL));
        gtk_editable_set_position(GTK_EDITABLE(inputbox), -1);
        return TRUE;
    }

    return FALSE;
}

gboolean
bookmark(const Arg *arg) {
    FILE *f;
    const char *filename;
    const char *uri = webkit_web_view_get_uri(webview);
    const char *title = webkit_web_view_get_title(webview);
    filename = g_strdup_printf(BOOKMARKS_STORAGE_FILENAME);
    f = fopen(filename, "a");
    g_free((gpointer *)filename);
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
        echo_message(Info, "Bookmark saved");
        return TRUE;
    } else {
    	set_error("Bookmarks file not found.");
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
                        if ((i + 1) >= HISTORY_MAX_ENTRIES) {
                            break;
                        }
                    }
                    fclose(f);
                }
                f = fopen(filename, "w");
                g_free((gpointer *)filename);
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

/* open an external editor defined by the protocol handler for
vimprobableedit on a text box or similar */
static gboolean
open_editor(const Arg *arg) {
    char *text = NULL;
    gboolean success;
    GPid child_pid;
    gchar *value = NULL, *message = NULL, *tag = NULL, *edit_url = NULL;
    gchar *temp_file_name = g_strdup_printf("%s/vimprobableeditXXXXXX",
      temp_dir);
    int temp_file_handle = -1;

    /* check if active element is suitable for text editing */
    jsapi_evaluate_script("document.activeElement.tagName", &value, &message);
    if (value == NULL)
        return FALSE;
    tag = g_strdup(value);
    if (strcmp(tag, "INPUT") == 0) {
        /* extra check: type == text */
        jsapi_evaluate_script("document.activeElement.type", &value, &message);
        if (strcmp(value, "text") != 0) {
            g_free(value);
            g_free(message);
            return FALSE;
        }
    } else if (strcmp(tag, "TEXTAREA") != 0) {
        g_free(value);
        g_free(message);
        return FALSE;
    }
    jsapi_evaluate_script("document.activeElement.value", &value, &message);
    text = g_strdup(value);
    if (text == NULL) {
        g_free(value);
        g_free(message);
        return FALSE;
    }

    /* write text into temporary file */
    temp_file_handle = mkstemp(temp_file_name);
    if (temp_file_handle == -1) {
        message = g_strdup_printf("Could not create temporary file: %s",
            strerror(errno));
        echo_message(Error, message);
        g_free(value);
        g_free(message);
        g_free(text);
        return FALSE;
    }
    if (write(temp_file_handle, text, strlen(text)) != strlen(text)) {
        message = g_strdup_printf("Short write to temporary file: %s",
            strerror(errno));
        echo_message(Error, message);
        g_free(value);
        g_free(message);
        g_free(text);
        return FALSE;
	}
    close(temp_file_handle);
    g_free(text);

    /* spawn editor */
    edit_url = g_strdup_printf("vimprobableedit:%s", temp_file_name);
    success = open_handler_pid(edit_url, &child_pid);
    g_free(edit_url);
    if (!success) {
        echo_message(Error, "External editor open failed (no handler for"
            " vimprobableedit protocol?)");
        unlink(temp_file_name);
        g_free(value);
        g_free(message);
        return FALSE;
    }
   
    /* mark the active text box as "under processing" */
    jsapi_evaluate_script(
        "document.activeElement.disabled = true;"
        "document.activeElement.originalBackground = "
        "   document.activeElement.style.background;"
        "document.activeElement.style.background = '#aaaaaa';"
        ,&value, &message);

    g_child_watch_add(child_pid, _resume_from_editor, temp_file_name);

    /* temp_file_name is freed in _resume_from_editor */
    g_free(value);
    g_free(message);
    g_free(tag);
    return TRUE;
}


/* pick up from where open_editor left the work to the glib event loop.

This is called when the external editor exits. 

The data argument points to allocated memory containing the temporary file
name. */
void 
_resume_from_editor(GPid child_pid, int child_status, gpointer data) {
    FILE *fp;
    GString *set_value_js = g_string_new(
        "document.activeElement.value = \"");
    g_spawn_close_pid(child_pid);
    gchar *value = NULL, *message = NULL;
    gchar *temp_file_name = data;
    gchar buffer[BUF_SIZE] = ""; 
    gchar *buf_ptr = buffer;
    int char_read;

    jsapi_evaluate_script(
        "document.activeElement.disabled = true;"
        "document.activeElement.style.background = '#aaaaaa';"
        ,&value, &message);

    if (child_status) {
        echo_message(Error, "External editor returned with non-zero status,"
            " discarding edits.");
        goto error_exit;
    }

    /* re-read the new contents of the file and put it into the HTML element */
    if (!access(temp_file_name, R_OK) == 0) {
        message = g_strdup_printf("Could not access temporary file: %s",
            strerror(errno));
        goto error_exit;
    }
    fp = fopen(temp_file_name, "r");
    if (fp == NULL) {
        /* this would be too weird to even emit an error message */
        goto error_exit;
    }
    jsapi_evaluate_script("document.activeElement.value = '';", 
        &value, &message);

    while (EOF != (char_read = fgetc(fp))) {
        if (char_read == '\n') {
            *buf_ptr++ = '\\';
            *buf_ptr++ = 'n';
        } else if (char_read == '"') {
            *buf_ptr++ = '\\';
            *buf_ptr++ = '"';
        } else {
            *buf_ptr++ = char_read;
        }
        /* ship out as the buffer when space gets tight.  This has
        fuzz to save on thinking, plus we have enough space for the
        trailing "; in any case. */
        if (buf_ptr-buffer>=BUF_SIZE-10) {
            *buf_ptr = 0;
            g_string_append(set_value_js, buffer);
            buf_ptr = buffer;
        }
    }
    *buf_ptr++ = '"';
    *buf_ptr++ = ';';
    *buf_ptr = 0;
    g_string_append(set_value_js, buffer);
    fclose(fp);

    jsapi_evaluate_script(set_value_js->str, &value, &message);

    /* Fall through, error and normal exit are identical */
error_exit:
    jsapi_evaluate_script(
        "document.activeElement.disabled = false;"
        "document.activeElement.style.background ="
        "   document.activeElement.originalBackground;"
        ,&value, &message);

    g_string_free(set_value_js, TRUE);
    unlink(temp_file_name);
    g_free(temp_file_name);
    g_free(value);
    g_free(message);
}

static gboolean
focus_input(const Arg *arg) {
    static Arg a;

    a.s = g_strdup("hints.focusInput();");
    a.i = Silent;
    script(&a);
    g_free(a.s);
    update_state();
    manual_focus = TRUE;
    return TRUE;
}

static void
clear_focus(void) {
    static Arg a;

    a.s = g_strdup("hints.clearFocus();");
    a.i = Silent;
    script(&a);
    g_free(a.s);
    a.i = ModeNormal;
    a.s = NULL;
    set(&a);
}

static gboolean
browser_settings(const Arg *arg) {
    char line[255];
    if (!arg->s) {
    	set_error("Missing argument.");
        return FALSE;
    }
    strncpy(line, arg->s, 254);
    if (process_set_line(line))
        return TRUE;
    else {
        set_error("Invalid setting.");
        return FALSE;
    }
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
                strncpy(browsersettings[i].var, my_pair.value, MAX_SETTING_SIZE);
                if (strlen(my_pair.value) > MAX_SETTING_SIZE - 1) {
                    /* in this case, \0 will not have been copied */
                    browsersettings[i].var[MAX_SETTING_SIZE - 1] = '\0';
                    /* in case this string is also used for a webkit setting, make sure it's consistent */
                    my_pair.value[MAX_SETTING_SIZE - 1] = '\0';
                    echo_message(Info, "String too long; automatically truncated!");
                }
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

            if (strlen(my_pair.what) == 14) {
                if (strncmp("acceptlanguage", my_pair.what, 14) == 0) {
                    g_object_set(G_OBJECT(session), "accept-language", acceptlanguage, NULL);
                } else if (strncmp("completioncase", my_pair.what, 14) == 0) {
                    complete_case_sensitive = boolval;
                }
            } else if (strlen(my_pair.what) == 5 && strncmp("proxy", my_pair.what, 5) == 0) {
                toggle_proxy(boolval);
            } else if (strlen(my_pair.what) == 10 && strncmp("scrollbars", my_pair.what, 10) == 0) {
                toggle_scrollbars(boolval);
            } else if (strlen(my_pair.what) == 9 && strncmp("statusbar", my_pair.what, 9) == 0) {
                gtk_widget_set_visible(GTK_WIDGET(statusbar), boolval);
            } else if (strlen(my_pair.what) == 8 && strncmp("inputbox", my_pair.what, 8) == 0) {
                gtk_widget_set_visible(inputbox, boolval);
            } else if (strlen(my_pair.what) == 11 && strncmp("escapeinput", my_pair.what, 11) == 0) {
                escape_input_on_load = boolval;
            }

            /* SSL certificate checking */
            if (strlen(my_pair.what) == 9 && strncmp("strictssl", my_pair.what, 9) == 0) {
                if (boolval) {
                    strict_ssl = TRUE;
                    g_object_set(G_OBJECT(session), "ssl-strict", TRUE, NULL);
                } else {
                    strict_ssl = FALSE;
                    g_object_set(G_OBJECT(session), "ssl-strict", FALSE, NULL);
                }
            }
            if (strlen(my_pair.what) == 8 && strncmp("cabundle", my_pair.what, 8) == 0) {
                g_object_set(G_OBJECT(session), SOUP_SESSION_SSL_CA_FILE, ca_bundle, NULL);
            }
            if (strlen(my_pair.what) == 10 && strncmp("windowsize", my_pair.what, 10) == 0) {
                set_default_winsize(my_pair.value);
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
process_line(char *line) {
    char *c = line, *command_hist;
    int i;
    size_t len, length = strlen(line);
    gboolean found = FALSE, success = FALSE;
    Arg a;

    while (isspace(*c))
        c++;
    /* Ignore blank lines.  */
    if (c[0] == '\0')
        return TRUE;

    command_hist = g_strdup(c);
    for (i = 0; i < LENGTH(commands); i++) {
        if (commands[i].cmd == NULL)
            break;
        len = strlen(commands[i].cmd);
        if (length >= len && !strncmp(c, commands[i].cmd, len) && (c[len] == ' ' || !c[len])) {
            found = TRUE;
            a.i = commands[i].arg.i;
            a.s = g_strdup(length > len + 1 ? &c[len + 1] : commands[i].arg.s);
            success = commands[i].func(&a);
            g_free(a.s);
            break;
        }
    }

    save_command_history(command_hist);
    g_free(command_hist);

    if (!found) {
        echo_message(Error, "Not a browser command: %s", c);
    } else if (!success) {
        if (error_msg != NULL) {
            echo_message(Error, error_msg);
            g_free(error_msg);
            error_msg = NULL;
        } else {
            echo_message(Error, "Unknown error. Please file a bug report!");
        }
    }
    return success;
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
    g_free((gpointer *)filename);
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

void
toggle_proxy(gboolean onoff) {
    SoupURI *proxy_uri;
    char    *filename, *new;

    if (onoff == FALSE)  {
        g_object_set(session, "proxy-uri", NULL, NULL);
    } else  {
        filename = (char *)g_getenv("http_proxy");

        /* Fallthrough to checking HTTP_PROXY as well, since this can also be
         * defined.
         */
        if (filename == NULL)
            filename = (char *)g_getenv("HTTP_PROXY");

        if (filename != NULL && 0 < strlen(filename)) {
            new = g_strrstr(filename, "http://") ? g_strdup(filename) : g_strdup_printf("http://%s", filename);
            proxy_uri = soup_uri_new(new);

            g_object_set(session, "proxy-uri", proxy_uri, NULL);

            soup_uri_free(proxy_uri);
            g_free(new);
        }
    }
}

void
toggle_scrollbars(gboolean onoff) {
	if (onoff == TRUE) {
		adjust_h = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(viewport));
		adjust_v = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(viewport));
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(viewport), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	} else {
		adjust_v = gtk_range_get_adjustment(GTK_RANGE(scroll_v));
		adjust_h = gtk_range_get_adjustment(GTK_RANGE(scroll_h));
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(viewport), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	}
	gtk_widget_set_scroll_adjustments (GTK_WIDGET(webview), adjust_h, adjust_v);

	return;
}

void set_default_winsize(const char * const size) {
    char *p;
    int x = 640, y = 480;

    x = strtol(size, &p, 10);
    if (errno == ERANGE || x <= 0) {
	    x = 640;
	    goto out;
    }

    if (p == size || strlen(size) == p - size)
	    goto out;

    y = strtol(p + 1, NULL, 10);
    if (errno == ERANGE || y <= 0)
	    y = 480;

out:
    gtk_window_resize(GTK_WINDOW(window), x, y);
}

void
update_url(const char *uri) {
    gboolean ssl = g_str_has_prefix(uri, "https://");
    GdkColor color;
    WebKitWebFrame *frame;
    WebKitWebDataSource *src;
    WebKitNetworkRequest *request;
    SoupMessage *msg;
    gboolean ssl_ok;
    char *sslactivecolor;
    gchar *markup;
#ifdef ENABLE_HISTORY_INDICATOR
    char before[] = " [";
    char after[] = "]";
    gboolean back = webkit_web_view_can_go_back(webview);
    gboolean fwd = webkit_web_view_can_go_forward(webview);

    if (!back && !fwd)
        before[0] = after[0] = '\0';
#endif
    markup = g_markup_printf_escaped(
#ifdef ENABLE_HISTORY_INDICATOR
        "<span font=\"%s\">%s%s%s%s%s</span>", statusfont, uri,
        before, back ? "+" : "", fwd ? "-" : "", after
#else
        "<span font=\"%s\">%s</span>", statusfont, uri
#endif
    );
    gtk_label_set_markup(GTK_LABEL(status_url), markup);
    g_free(markup);
    if (ssl) {
        frame = webkit_web_view_get_main_frame(webview);
        src = webkit_web_frame_get_data_source(frame);
        request = webkit_web_data_source_get_request(src);
        msg = webkit_network_request_get_message(request);
        ssl_ok = soup_message_get_flags(msg) & SOUP_MESSAGE_CERTIFICATE_TRUSTED;
        if (ssl_ok)
            sslactivecolor = sslbgcolor;
        else
            sslactivecolor = sslinvalidbgcolor;
    }
    gdk_color_parse(ssl ? sslactivecolor : statusbgcolor, &color);
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
    g_free(markup);

    g_string_free(status, TRUE);
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
    scroll_h = GTK_SCROLLBAR(gtk_hscrollbar_new(NULL));
    scroll_v = GTK_SCROLLBAR(gtk_vscrollbar_new(NULL));
    adjust_h = gtk_range_get_adjustment(GTK_RANGE(scroll_h));
    adjust_v = gtk_range_get_adjustment(GTK_RANGE(scroll_v));
    if (embed) {
        window = GTK_WINDOW(gtk_plug_new(embed));
    } else {
        window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
        gtk_window_set_wmclass(GTK_WINDOW(window), "vimprobable2", "Vimprobable2");
    }
    gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);
    box = GTK_BOX(gtk_vbox_new(FALSE, 0));
    inputbox = gtk_entry_new();
    webview = (WebKitWebView*)webkit_web_view_new();
    statusbar = GTK_BOX(gtk_hbox_new(FALSE, 0));
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

    keymap = gdk_keymap_get_default();

#ifdef DISABLE_SCROLLBAR
    viewport = gtk_scrolled_window_new(NULL, NULL);
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
    set_widget_font_and_color(inputbox, urlboxfont[0], urlboxbgcolor[0], urlboxcolor[0]);
    g_object_set(gtk_widget_get_settings(inputbox), "gtk-entry-select-on-focus", FALSE, NULL);
}

void
setup_settings() {
    WebKitWebSettings *settings = (WebKitWebSettings*)webkit_web_settings_new();
    char *filename, *file_url;

    session = webkit_get_default_session();
    g_object_set(G_OBJECT(session), "ssl-ca-file", ca_bundle, NULL);
    g_object_set(G_OBJECT(session), "ssl-strict", strict_ssl, NULL);
    g_object_set(G_OBJECT(settings), "default-font-size", DEFAULT_FONT_SIZE, NULL);
    g_object_set(G_OBJECT(settings), "enable-scripts", enablePlugins, NULL);
    g_object_set(G_OBJECT(settings), "enable-plugins", enablePlugins, NULL);
    g_object_set(G_OBJECT(settings), "enable-java-applet", enableJava, NULL);
    g_object_set(G_OBJECT(settings), "enable-page-cache", enablePagecache, NULL);
    filename = g_strdup_printf(USER_STYLESHEET);
    file_url = g_strdup_printf("file://%s", filename);
    g_object_set(G_OBJECT(settings), "user-stylesheet-uri", file_url, NULL);
    g_free(file_url);
    g_free(filename);
    g_object_set(G_OBJECT(settings), "user-agent", useragent, NULL);
    g_object_get(G_OBJECT(settings), "zoom-step", &zoomstep, NULL);
    webkit_web_view_set_settings(webview, settings);

    /* proxy */
    toggle_proxy(use_proxy);
}

void
setup_signals() {
    WebKitWebFrame *frame = webkit_web_view_get_main_frame(webview);
#ifdef ENABLE_COOKIE_SUPPORT
    /* Headers. */
    g_signal_connect_after(G_OBJECT(session), "request-started", G_CALLBACK(new_generic_request), NULL);
#endif
    /* Accept-language header */
    g_object_set(G_OBJECT(session), "accept-language", acceptlanguage, NULL);
    /* window */
    g_object_connect(G_OBJECT(window),
        "signal::destroy",                              G_CALLBACK(window_destroyed_cb),            NULL,
    NULL);
    /* frame */
    g_signal_connect(G_OBJECT(frame),
        "scrollbars-policy-changed",                    G_CALLBACK(blank_cb),                       NULL);
    /* webview */
    g_object_connect(G_OBJECT(webview),
        "signal::title-changed",                        G_CALLBACK(webview_title_changed_cb),        NULL,
        "signal::load-progress-changed",                G_CALLBACK(webview_progress_changed_cb),     NULL,
        "signal::load-committed",                       G_CALLBACK(webview_load_committed_cb),       NULL,
        "signal::load-finished",                        G_CALLBACK(webview_load_finished_cb),        NULL,
        "signal::navigation-policy-decision-requested", G_CALLBACK(webview_navigation_cb),           NULL,
        "signal::new-window-policy-decision-requested", G_CALLBACK(webview_new_window_cb),           NULL,
        "signal::mime-type-policy-decision-requested",  G_CALLBACK(webview_mimetype_cb),             NULL,
        "signal::download-requested",                   G_CALLBACK(webview_download_cb),             NULL,
        "signal::key-press-event",                      G_CALLBACK(webview_keypress_cb),             NULL,
        "signal::hovering-over-link",                   G_CALLBACK(webview_hoverlink_cb),            NULL,
        "signal::console-message",                      G_CALLBACK(webview_console_cb),              NULL,
        "signal::create-web-view",                      G_CALLBACK(webview_open_in_new_window_cb),   NULL,
        "signal::event",                                G_CALLBACK(notify_event_cb),                 NULL,
    NULL);
    /* webview adjustment */
    g_object_connect(G_OBJECT(adjust_v),
        "signal::value-changed",                        G_CALLBACK(webview_scroll_cb),               NULL,
    NULL);
    /* inputbox */
    g_object_connect(G_OBJECT(inputbox),
        "signal::activate",                             G_CALLBACK(inputbox_activate_cb),            NULL,
        "signal::key-press-event",                      G_CALLBACK(inputbox_keypress_cb),            NULL,
        "signal::key-release-event",                    G_CALLBACK(inputbox_keyrelease_cb),          NULL,
#ifdef ENABLE_INCREMENTAL_SEARCH
        "signal::changed",                              G_CALLBACK(inputbox_changed_cb),             NULL,
#endif
    NULL);
    /* inspector */
    g_signal_connect(G_OBJECT(inspector),
        "inspect-web-view",                             G_CALLBACK(inspector_inspect_web_view_cb),   NULL);
}

#ifdef ENABLE_USER_SCRIPTFILE
static void
scripts_run_user_file() {
    gchar *js = NULL, *user_scriptfile = NULL;
    GError *error = NULL;

    user_scriptfile = g_strdup_printf(USER_SCRIPTFILE);

    /* run the users script file */
    if (g_file_test(user_scriptfile, G_FILE_TEST_IS_REGULAR)
            && g_file_get_contents(user_scriptfile, &js, NULL, &error)) {

        gchar *value = NULL, *message = NULL;

        jsapi_evaluate_script(js, &value, &message);
        if (message) {
            fprintf(stderr, "%s", message);
            g_free(message);
        }
    } else {
        fprintf(stderr, "Cannot open %s: %s\n", user_scriptfile, error ? error->message : "file not found");
    }

    g_free(user_scriptfile);
}
#endif

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

	g_signal_connect(G_OBJECT(file_cookie_jar), "changed",
			G_CALLBACK(update_cookie_jar), NULL);

	return;
}

/* TA:  XXX - we should be using this callback for any header-requests we
 *      receive (hence the name "new_generic_request" -- but for now, its use
 *      is limited to handling cookies.
 */
void
new_generic_request(SoupSession *session, SoupMessage *soup_msg, gpointer unused) 
{
    SoupMessageHeaders *soup_msg_h;
    SoupURI *uri;
    char *cookie_str;

    soup_msg_h = soup_msg->request_headers;
    soup_message_headers_remove(soup_msg_h, "Cookie");
    uri = soup_message_get_uri(soup_msg);
    if ((cookie_str = get_cookies(uri))) {
        soup_message_headers_append(soup_msg_h, "Cookie", cookie_str);
        g_free(cookie_str);
    }

    g_signal_connect_after(G_OBJECT(soup_msg), "got-headers", G_CALLBACK(handle_cookie_request), NULL);

    return;
}

char *
get_cookies(SoupURI *soup_uri) {
	char *cookie_str;

	cookie_str = soup_cookie_jar_get_cookies(file_cookie_jar, soup_uri, TRUE);

	return cookie_str;
}

void
handle_cookie_request(SoupMessage *soup_msg, gpointer unused)
{
	GSList *resp_cookie = NULL, *cookie_list;
	SoupCookie *cookie;

	cookie_list = soup_cookies_from_response(soup_msg);
	for(resp_cookie = cookie_list; resp_cookie; resp_cookie = g_slist_next(resp_cookie))
	{
		SoupDate *soup_date;
		cookie = soup_cookie_copy((SoupCookie *)resp_cookie->data);

		if (cookie_timeout && cookie->expires == NULL) {
			soup_date = soup_date_new_from_time_t(time(NULL) + cookie_timeout * 10);
			soup_cookie_set_expires(cookie, soup_date);
			soup_date_free(soup_date);
		}
		soup_cookie_jar_add_cookie(file_cookie_jar, cookie);
	}

	soup_cookies_free(cookie_list);

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
	GSList *cookie_list;
	file_cookie_jar = soup_cookie_jar_text_new(cookie_store, COOKIES_STORAGE_READONLY);

	/* Put them back in the session store. */
	GSList *cookies_from_file = soup_cookie_jar_all_cookies(file_cookie_jar);
	cookie_list = cookies_from_file;

	for (; cookies_from_file;
	       cookies_from_file = cookies_from_file->next)
	{
		soup_cookie_jar_add_cookie(session_cookie_jar, cookies_from_file->data);
	}

	soup_cookies_free(cookies_from_file);
	g_slist_free(cookie_list);

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

int
main(int argc, char *argv[]) {
    static Arg a;
    static char url[256] = "";
    static gboolean ver = false;
    static gboolean configfile_exists = FALSE;
    static const char *cfile = NULL;
    static GOptionEntry opts[] = {
            { "version", 'v', 0, G_OPTION_ARG_NONE, &ver, "print version", NULL },
            { "embed", 'e', 0, G_OPTION_ARG_STRING, &winid, "embedded", NULL },
            { "configfile", 'c', 0, G_OPTION_ARG_STRING, &cfile, "config file", NULL },
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
        printf("%s\n", INTERNAL_VERSION);
        return EXIT_SUCCESS;
    }

    if (getenv("TMPDIR")) {
      strncpy(temp_dir, getenv("TMPDIR"), MAX_SETTING_SIZE);
      temp_dir[MAX_SETTING_SIZE-1] = 0;
    }

    if( getenv("XDG_CONFIG_HOME") )
        config_base = g_strdup_printf("%s", getenv("XDG_CONFIG_HOME"));
    else
        config_base = g_strdup_printf("%s/.config/", getenv("HOME"));

    if (cfile)
        configfile = g_strdup(cfile);
    else
        configfile = g_strdup_printf(RCFILE);

    if (!g_thread_supported())
        g_thread_init(NULL);

    if (winid) {
        if (strncmp(winid, "0x", 2) == 0) {
            embed = strtol(winid, NULL, 16);
        } else {
            embed = atoi(winid);
        }
    }

    setup_modkeys();
    make_keyslist();
    setup_gui();
#ifdef ENABLE_COOKIE_SUPPORT
    setup_cookies();
#endif

    make_searchengines_list(searchengines, LENGTH(searchengines));
    make_uri_handlers_list(uri_handlers, LENGTH(uri_handlers));

    /* Check if the specified file exists. */
    /* And only warn the user, if they explicitly asked for a config on the
     * command line.
     */
    if (!(access(configfile, F_OK) == 0) && cfile) {
        echo_message(Info, "Config file '%s' doesn't exist", cfile);
    } else if ((access(configfile, F_OK) == 0))
	    configfile_exists = true;

    /* read config file */
    /* But only report errors if we failed, and the file existed. */
    if ((SUCCESS != read_rcfile(configfile)) && configfile_exists) {
        echo_message(Error, "Error in config file '%s'", configfile);
        g_free(configfile);
    }

    /* command line argument: URL */
    if (argc > 1) {
        strncpy(url, argv[argc - 1], 255);
    } else {
        strncpy(url, startpage, 255);
    }

    a.i = TargetCurrent;
    a.s = url;
    open_arg(&a);
    gtk_main();

    mop_up();

    return EXIT_SUCCESS;
}
