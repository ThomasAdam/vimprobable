/*
    (C) 2009 by Leon Winter
*/
/*
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <gdk/gdkkeysyms.h>

#define APPNAME   "WebKit Browser"
#define STARTPAGE "https://blog.fefe.de/?css=fefe.css"

#define MODE_NORMAL  0
#define MODE_INSERT  1
#define MODE_SEARCH  2

static GtkWidget* vbox;
static GtkWidget* main_window;
static GtkWidget* uri_entry;
static WebKitWebView* web_view;
static gchar* main_title;
static gint load_progress;
static int mode;  // vimperator mode
static int count; // cmdcounter for multiple command execution (vimperator style)
static gchar modkey; // vimperator modkey e.g. "g" für gg or gf

static void
activate_uri_entry_cb (GtkWidget* entry, gpointer data)
{
    guint c;
    const gchar* uri = gtk_entry_get_text (GTK_ENTRY (entry));
    g_assert (uri);
    if(mode == MODE_NORMAL) {
        webkit_web_view_load_uri (web_view, uri);
    } else if(mode == MODE_SEARCH) {
        /* unmark the old hits if any */
        webkit_web_view_unmark_text_matches(web_view);
        /* the new search */
        c = webkit_web_view_mark_text_matches(web_view, uri, (gboolean)FALSE, 0);
        webkit_web_view_set_highlight_text_matches(web_view, (gboolean)TRUE);
        webkit_web_view_search_text(web_view, uri, (gboolean)FALSE, (gboolean)TRUE, (gboolean)TRUE);
        printf("%d\n", c);
        /* focus webview */
        gtk_widget_grab_focus (GTK_WIDGET (web_view));
    }
}

static void
find_next (gboolean direction)
{
    const gchar* txt = gtk_entry_get_text (GTK_ENTRY (uri_entry));
    webkit_web_view_search_text(web_view, txt, (gboolean)FALSE, (gboolean)direction, (gboolean)TRUE);
}

static void
update_title (GtkWindow* window)
{
    GString* string = g_string_new (main_title);
    g_string_append (string, " - " APPNAME);
    if (load_progress < 100)
        g_string_append_printf (string, " (%d%%)", load_progress);
    gchar* title = g_string_free (string, FALSE);
    gtk_window_set_title (window, title);
    g_free (title);
}

static void
link_hover_cb (WebKitWebView* page, const gchar* title, const gchar* link, gpointer data)
{
    if (link)
        gtk_window_set_title ((GtkWindow*)main_window, link); 
    else
        update_title((GtkWindow*)main_window);
}

static void
title_change_cb (WebKitWebView* web_view, WebKitWebFrame* web_frame, const gchar* title, gpointer data)
{
    if (main_title)
        g_free (main_title);
    main_title = g_strdup (title);
    update_title (GTK_WINDOW (main_window));
}

static void
progress_change_cb (WebKitWebView* page, gint progress, gpointer data)
{
    load_progress = progress;
    gtk_entry_set_progress_fraction((GtkEntry*)uri_entry, load_progress == 100 ? 0 : (double)load_progress / 100);
    update_title (GTK_WINDOW (main_window));
}

static void
load_commit_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data)
{
    const gchar* uri = webkit_web_frame_get_uri(frame);
    if (uri)
        gtk_entry_set_text (GTK_ENTRY (uri_entry), uri);
}

static void
destroy_cb (GtkWidget* widget, gpointer data)
{
    gtk_main_quit ();
}

static void
icon_loaded_cb (WebKitWebView* page, gpointer data)
{
    // gtk_entry_set_icon_from_gicon((GtkEntry*)uri_entry, GTK_ENTRY_ICON_PRIMARY, data);
}

static void
jump_uri_and_set (const char* str)
{
    gtk_widget_grab_focus((GtkWidget*) uri_entry);
    gtk_entry_set_text (GTK_ENTRY (uri_entry), str);
}

static gboolean
key_press_uri_entry_cb (WebKitWebView* page, GdkEventKey* event)
{
    if(event->type == GDK_KEY_PRESS && event->keyval == GDK_Escape) {
        /* clean search results if any */
        webkit_web_view_unmark_text_matches(web_view);
        /* revert to url bar */
        gtk_entry_set_text (GTK_ENTRY (uri_entry), webkit_web_view_get_uri(web_view));
        /* remove icon */
        gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(uri_entry), GTK_ENTRY_ICON_PRIMARY, NULL);
        /* focus webview */
        gtk_widget_grab_focus((GtkWidget*)web_view);
        mode = MODE_NORMAL;
        return (gboolean)TRUE;
    }
    return (gboolean)FALSE;
}

static gboolean
key_press_cb (WebKitWebView* page, GdkEventKey* event)
{
/*
    Vimperator modes

    In normal mode we will listen for commands (and numbers for multiple execution)
    Maybe cmdline mode is invoked via ":" or a shortcut cmd
    If anyone clicks at an object we will enter insert mode

*/
    //if(event->type != GDK_KEY_PRESS)
    //    return (gboolean)FALSE;
// TODO check for no control mask!
// TODO add INPUT mode
    if(mode == MODE_SEARCH && key_press_uri_entry_cb(page, event))
        return (gboolean)TRUE;
    if(mode != MODE_INSERT) {
        if(event->keyval >= GDK_0 && event->keyval <= GDK_9) {
            count = (count ? count * 10 : 0) + (event->keyval - GDK_0);
            modkey = '\0';
        } else if(event->state == GDK_CONTROL_MASK) {
            switch(event->keyval) {
                case GDK_b:
                    do webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_PAGES, -1);
                    while(--count > 0);
                    break;
                case GDK_c:
                    webkit_web_view_stop_loading(web_view);
                    break;
                case GDK_i: /* back */
                    webkit_web_view_go_back_or_forward(web_view, (gint)(-1 * (count ? count : 1)));
                    break;
                case GDK_o: /* fwd */
                    webkit_web_view_go_back_or_forward(web_view, (gint)(count ? count : 1));
                    break;
                default:
                    return (gboolean)FALSE;
            }
            count = 0;
            modkey = '\0';
        } else if(event->state == 0) {
            switch(event->keyval) {
                case GDK_g:
                    if(modkey == GDK_g)
                        webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_BUFFER_ENDS, -1);
                    else {
                        modkey = GDK_g;
                        return (gboolean)TRUE;
                    }
                    break;
                case GDK_G:
                    webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_BUFFER_ENDS, 1);
                    break;
                case GDK_h:
                    if(modkey == GDK_g)
                        webkit_web_view_load_uri (web_view, STARTPAGE);
                    else
                        do webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_VISUAL_POSITIONS, -1);
                        while(--count > 0);
                    break;
                case GDK_j:
                    do webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_DISPLAY_LINES, -1);
                    while(--count > 0);
                    break;
                case GDK_k:
                    do webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_DISPLAY_LINES, 1);
                    while(--count > 0);
                    break;
                case GDK_l:
                    do webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_VISUAL_POSITIONS, 1);
                    while(--count > 0);
                    break;
                case GDK_space:
                    do webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_PAGES, 1);
                    while(--count > 0);
                    break;
                case GDK_r:
                    webkit_web_view_reload (web_view);
                    break;
                case GDK_R:
                    webkit_web_view_reload_bypass_cache (web_view);
                    break;
                case GDK_slash: /* set up search mode */
                    gtk_entry_set_icon_from_stock(GTK_ENTRY(uri_entry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_FIND);
                    jump_uri_and_set("");
                    mode = MODE_SEARCH;
                    break;
                /*case GDK_colon:
                    jump_uri_and_set(":");
                    break;*/
                case GDK_o: /* insert url */
                    jump_uri_and_set("");
                    break;
                case GDK_d:
                    gtk_main_quit();
                    break;
                case GDK_n: /* next match(es) */
                    if(mode == MODE_SEARCH)
                        do find_next((gboolean)TRUE);
                        while(--count > 0);
                    else
                        return (gboolean)FALSE;
                    break;
                case GDK_N: /* prev match(es) */
                    if(mode == MODE_SEARCH)
                        do find_next((gboolean)FALSE);
                        while(--count > 0);
                    else
                        return (gboolean)FALSE;
                    break;
                default:
                    return (gboolean)FALSE;
            }
            count = 0;
            modkey = '\0';
        }
    }

    return (gboolean)TRUE;

//        switch(event->hardware_keycode) {
//            case 233: /* XF86XK_Forward */
//                webkit_web_view_go_forward (web_view);
//                return (gboolean)TRUE;
//            case 234: /* XF86XK_Back */
//                webkit_web_view_go_back (web_view);
//                return (gboolean)TRUE;
//        }
/*        if(event->state == GDK_CONTROL_MASK)
            switch(event->keyval) {
                case GDK_l:
                    gtk_widget_grab_focus((GtkWidget*) uri_entry);
                    return (gboolean)TRUE;
                case GDK_plus:
                    webkit_web_view_zoom_in(web_view);
                    return (gboolean)TRUE;
                case GDK_minus:
                    webkit_web_view_zoom_out(web_view);
                    return (gboolean)TRUE;
                case GDK_0:
                    webkit_web_view_set_zoom_level(web_view, 1);
                    return (gboolean)TRUE;
            }
*/
}

static GtkWidget*
create_browser ()
{
    GtkWidget* scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_NEVER);

    web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
    gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (web_view));

    g_signal_connect (G_OBJECT (web_view), "title-changed", G_CALLBACK (title_change_cb), web_view);
    g_signal_connect (G_OBJECT (web_view), "load-progress-changed", G_CALLBACK (progress_change_cb), web_view);
    g_signal_connect (G_OBJECT (web_view), "load-committed", G_CALLBACK (load_commit_cb), web_view);
    g_signal_connect (G_OBJECT (web_view), "hovering-over-link", G_CALLBACK (link_hover_cb), web_view);

    g_signal_connect (G_OBJECT (web_view), "key-press-event", G_CALLBACK(key_press_cb), web_view);

    return scrolled_window;
}

static GtkWidget*
create_toolbar ()
{
    GtkWidget* toolbar = gtk_toolbar_new ();

    gtk_toolbar_set_orientation (GTK_TOOLBAR (toolbar), GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    GtkToolItem* item;

    /* The URL entry */
    item = gtk_tool_item_new ();
    gtk_tool_item_set_expand (item, TRUE);
    uri_entry = gtk_entry_new ();
    /* use new fraction prop of GtkEntry (requires gtk >= 2.16) */
    gtk_container_add (GTK_CONTAINER (item), uri_entry);
    g_signal_connect (G_OBJECT (uri_entry), "activate", G_CALLBACK (activate_uri_entry_cb), NULL);
    g_signal_connect (G_OBJECT (uri_entry), "key-press-event", G_CALLBACK (key_press_uri_entry_cb), NULL);    

    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
    // gtk_entry_set_icon_from_stock( GTK_ENTRY( uri_entry ), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_ABOUT );

    return toolbar;
}

static GtkWidget*
create_window ()
{
    GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
    gtk_widget_set_name (window, "GtkLauncher");
    g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_cb), NULL);

    return window;
}
/*
static WebKitWebView*
embed_inspector (WebKitWebInspector *web_inspector, WebKitWebView *target, gpointer user_data)
{
    WebKitWebView* webview;

    GtkWidget* scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_NEVER);

    webview = WEBKIT_WEB_VIEW (webkit_web_view_new ());
    gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (web_view));

    gtk_box_pack_start ( GTK_BOX (vbox), (GtkWidget*)webview, TRUE, TRUE, 0);
    return webview;
}
*/
int
main (int argc, char* argv[])
{
    //WebKitWebInspector *inspector;
    gtk_init (&argc, &argv);
    if (!g_thread_supported ())
        g_thread_init (NULL);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), create_toolbar (), FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), create_browser (), TRUE, TRUE, 0);
    //inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW(web_view));

    main_window = create_window ();
    gtk_container_add (GTK_CONTAINER (main_window), vbox);

    mode = MODE_NORMAL;

    gchar* uri = (gchar*) (argc > 1 ? argv[1] : STARTPAGE);
    webkit_web_view_load_uri (web_view, uri);

    //g_signal_connect (G_OBJECT (inspector), "inspect-web-view", G_CALLBACK(embed_inspector), NULL);

    gtk_widget_grab_focus (GTK_WIDGET (web_view));
    gtk_widget_show_all (main_window);
    gtk_main ();

    return 0;
}
