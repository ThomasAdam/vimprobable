/*
    (c) 2009 by Leon Winter, see LICENSE file
*/

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit/webkit.h>
#include <libsoup/soup.h>

enum { ModeNormal, ModeInsert, ModeSearch, ModeWebsearch, ModeHints };  /* modes */
enum { TargetCurrent, TargetNew };                                      /* target */

/* structs here */

/* callbacks here */
static void window_destroyed_cb(GtkWidget* window, gpointer func_data);
static void webview_title_changed(WebKitWebView* webview, WebKitWebFrame* frame, char* title, gpointer user_data);

/* functions */
static void setup_gui();
static void setup_signals(GObject* window, GObject* webview);

/* variables */

static GtkAdjustment* adjust_h;
static GtkAdjustment* adjust_v;
static GtkWidget* input;
static WebKitWebView* webview;

#include "config.h"

/* callbacks */
void
window_destroyed_cb(GtkWidget* window, gpointer func_data) {
    gtk_main_quit();
}

void
webview_title_changed(WebKitWebView* webview, WebKitWebFrame* frame, char* title, gpointer user_data) {

}

/* funcs here */

void
setup_gui() {
    GtkScrollbar* scroll_h = (GtkScrollbar*)gtk_hscrollbar_new(NULL);
    GtkScrollbar* scroll_v = (GtkScrollbar*)gtk_vscrollbar_new(NULL);
    adjust_h = gtk_range_get_adjustment((GtkRange*)scroll_h);
    adjust_v = gtk_range_get_adjustment((GtkRange*)scroll_v);
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkBox* box = gtk_vbox_new(FALSE, 0);
    input = gtk_entry_new();
    GtkWidget* viewport = gtk_scrolled_window_new(adjust_h, adjust_v);
    webview = webkit_web_view_new();
    WebKitWebSettings* settings = webkit_web_settings_new();

    // let config.h set up WebKitWebSettings
    gtk_widget_set_name(window, "WebKitBrowser");
#ifdef DISABLE_SCROLLBAR
    gtk_scrolled_window_set_policy((GtkScrolledWindow*)viewport, GTK_POLICY_NEVER, GTK_POLICY_NEVER);
#endif
    setup_signals(window, webview);
    gtk_container_add((GtkContainer*)viewport, (GtkWidget*)webview);
    gtk_box_pack_start(box, input, FALSE, FALSE, 0);
    gtk_box_pack_start(box, viewport, TRUE, TRUE, 0);
    gtk_container_add((GtkContainer*)window, box);
    gtk_widget_grab_focus((GtkWidget*)webview);
    gtk_widget_show_all(window);
    webkit_web_view_load_uri(webview, startpage);
    gtk_main();
}

void
setup_signals(GObject* window, GObject* webview) {
    g_signal_connect((GObject*)window,  "destroy", (GCallback)window_destroyed_cb, NULL);

    g_signal_connect((GObject*)webview, "title-changed", (GCallback)webview_title_changed, NULL);
    //g_signal_connect((GObject*)webview, "", (GCallback)webview_title_changed, NULL);
}

int
main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);
    if(!g_thread_supported())
        g_thread_init(NULL);
    setup_gui();

    return EXIT_SUCCESS;
}
