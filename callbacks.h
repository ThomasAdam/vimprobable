/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    (c) 2010 by Hans-Peter Deifel
    (c) 2010 by Thomas Adam
    see LICENSE file
*/

#include "includes.h"

void webview_scroll_cb(GtkAdjustment * adjustment, gpointer user_data);
gboolean webview_navigation_cb(WebKitWebView * webview, WebKitWebFrame * frame,
			       WebKitNetworkRequest * request,
			       WebKitWebPolicyDecision * decision,
			       gpointer user_data);
