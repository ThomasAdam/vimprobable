/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    (c) 2010 by Hans-Peter Deifel
    (c) 2010 by Thomas Adam
    see LICENSE file
*/

#include "includes.h"
#include "callbacks.h"
#include "vimprobable.h"
#include "utilities.h"
#include "main.h"

void webview_scroll_cb(GtkAdjustment * adjustment, gpointer user_data)
{
	update_state();
}

gboolean
webview_navigation_cb(WebKitWebView * webview, WebKitWebFrame * frame,
		      WebKitNetworkRequest * request,
		      WebKitWebPolicyDecision * decision, gpointer user_data)
{
	char *uri = (char *)webkit_network_request_get_uri(request);
	/* check for external handlers */
	if (open_handler(uri))
		return TRUE;
	else
		return FALSE;
}
