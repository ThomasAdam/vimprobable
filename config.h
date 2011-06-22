/*
    (c) 2009 by Leon Winter
    (c) 2009-2011 by Hannes Schueller
    (c) 2009-2010 by Matto Fransen
    (c) 2010-2011 by Hans-Peter Deifel
    (c) 2010-2011 by Thomas Adam
    (c) 2011 by Albert Kim
    see LICENSE file
*/

/* Vimprobable version number */
#define VERSION "0.9.9.1"
#define INTERNAL_VERSION "Vimprobable2/"VERSION

/* general settings */
char startpage[MAX_SETTING_SIZE]      = "http://www.vimprobable.org/";
char useragent[MAX_SETTING_SIZE]      = "Vimprobable2/" VERSION;
char acceptlanguage[MAX_SETTING_SIZE] = "";
static const gboolean enablePlugins     = TRUE; /* TRUE keeps plugins enabled */
static const gboolean enableJava        = TRUE; /* FALSE disables Java applets */
static const gboolean enablePagecache   = FALSE; /* TRUE turns on the page cache. */

/* appearance */
char statusbgcolor[MAX_SETTING_SIZE]    = "#000000";            /* background color for status bar */
char statuscolor[MAX_SETTING_SIZE]      = "#ffffff";            /* color for status bar */
char sslbgcolor[MAX_SETTING_SIZE]       = "#b0ff00";            /* background color for status bar with SSL url */
char sslcolor[MAX_SETTING_SIZE]         = "#000000";            /* color for status bar with SSL url */

                                        /*  normal,                 warning,                error       */
static const char *urlboxfont[]         = { "monospace normal 8",   "monospace normal 8",   "monospace bold 8"};
static const char *urlboxcolor[]        = { NULL,                   "#ff0000",              "#ffffff" };
static const char *urlboxbgcolor[]      = { NULL,                   NULL,                   "#ff0000" };

                                        /*  normal,                 error               */
static const char *completionfont[]     = { "monospace normal 8",   "monospace bold 8" };
                                                                                        /* topborder color */
static const char *completioncolor[]    = { "#000000",              "#ff00ff",              "#000000" };
                                                                                        /* current row background */
static const char *completionbgcolor[]  = { "#ffffff",              "#ffffff",              "#fff000" };
/* pango markup for prefix highliting:      opening,                closing             */
#define             COMPLETION_TAG_OPEN     "<b>"
#define             COMPLETION_TAG_CLOSE    "</b>"

static const char statusfont[]          = "monospace bold 8";   /* font for status bar */
#define             ENABLE_HISTORY_INDICATOR
#define             ENABLE_INCREMENTAL_SEARCH
#define             ENABLE_GTK_PROGRESS_BAR
#define             ENABLE_WGET_PROGRESS_BAR
static const int progressbartick        = 20;
static const char progressborderleft    = '[';
static const char progressbartickchar   = '=';
static const char progressbarcurrent    = '>';
static const char progressbarspacer     = ' ';
static const char progressborderright   = ']';

/* cookies */
#define             ENABLE_COOKIE_SUPPORT
#define             COOKIES_STORAGE_FILENAME    "%s/.config/vimprobable/cookies", getenv("HOME")
#define             COOKIES_STORAGE_READONLY    FALSE   /* if TRUE new cookies will be lost if you quit */

/* downloads directory */
#define             DOWNLOADS_PATH              "%s", getenv("HOME")

/* font size */
#define             DEFAULT_FONT_SIZE           12

/* user styles */
#define             USER_STYLESHEET             "%s/.config/vimprobable/style.css", getenv("HOME")

/* proxy */
static const gboolean use_proxy         = TRUE; /* TRUE if you're going to use a proxy (whose address
                                                  is specified in http_proxy environment variable), false otherwise */
/* scrolling */
static unsigned int scrollstep          = 40;   /* cursor difference in pixel */
static unsigned int pagingkeep          = 40;   /* pixels kept when paging */
#define             DISABLE_SCROLLBAR

/* searching */
#define             ENABLE_MATCH_HIGHLITING
static const int searchoptions          = CaseInsensitive | Wrapping;
gboolean complete_case_sensitive        = TRUE;

/* search engines */
static Searchengine searchengines[] = {
    { "i",          "http://ixquick.com/do/metasearch.pl?query=%s" },
    { "s",          "https://ssl.scroogle.org/cgi-bin/nbbwssl.cgi?Gw=%s" },
    { "w",          "https://secure.wikimedia.org/wikipedia/en/w/index.php?title=Special%%3ASearch&search=%s&go=Go" },
    { "wd",         "https://secure.wikimedia.org/wikipedia/de/w/index.php?title=Special%%3ASearch&search=%s&go=Go" },
};

static char defaultsearch[MAX_SETTING_SIZE] = "i";

/* command mapping */
Command commands[COMMANDSIZE] = {
    /* command,                                        	function,         argument */
    { "ba",                                            	navigate,         {NavigationBack} },
    { "back",                                          	navigate,         {NavigationBack} },
    { "ec",                                            	script,           {Info} },
    { "echo",                                          	script,           {Info} },
    { "echoe",                                         	script,           {Error} },
    { "echoerr",                                       	script,           {Error} },
    { "fw",                                            	navigate,         {NavigationForward} },
    { "fo",                                            	navigate,         {NavigationForward} },
    { "forward",                                       	navigate,         {NavigationForward} },
    { "javascript",                                    	script,           {Silent} },
    { "o",                                             	open_arg,         {TargetCurrent} },
    { "open",                                          	open_arg,         {TargetCurrent} },
    { "q",                                             	quit,             {0} },
    { "quit",                                          	quit,             {0} },
    { "re",                                            	navigate,         {NavigationReload} },
    { "re!",                                           	navigate,         {NavigationForceReload} },
    { "reload",                                        	navigate,         {NavigationReload} },
    { "reload!",                                       	navigate,         {NavigationForceReload} },
    { "qt",                                             search_tag,       {0} },
    { "st",                                            	navigate,         {NavigationCancel} },
    { "stop",                                          	navigate,         {NavigationCancel} },
    { "t",                                             	open_arg,         {TargetNew} },
    { "tabopen",                                       	open_arg,         {TargetNew} },
    { "print",                                         	print_frame,      {0} },
    { "bma",                                           	bookmark,         {0} },
    { "bookmark",                                      	bookmark,         {0} },
    { "source",                                        	view_source,      {0} },
    { "set",                                           	browser_settings, {0} },
    { "map",                                           	mappings,         {0} },
    { "jumpleft",                                       scroll,           {ScrollJumpTo   | DirectionLeft} },
    { "jumpright",                                      scroll,           {ScrollJumpTo   | DirectionRight} },
    { "jumptop",                                        scroll,           {ScrollJumpTo   | DirectionTop} },
    { "jumpbottom",                                     scroll,           {ScrollJumpTo   | DirectionBottom} },
    { "pageup",                                         scroll,           {ScrollMove     | DirectionTop      | UnitPage} },	
    { "pagedown",                                       scroll,           {ScrollMove     | DirectionBottom   | UnitPage} },
    { "navigationback",   	                            navigate,         {NavigationBack} },
    { "navigationforward",	                            navigate,         {NavigationForward} },
    { "scrollleft",                                     scroll,           {ScrollMove     | DirectionLeft     | UnitLine} },
    { "scrollright",                                    scroll,           {ScrollMove     | DirectionRight    | UnitLine} },
    { "scrollup",                                       scroll,           {ScrollMove     | DirectionTop      | UnitLine} },
    { "scrolldown",                                     scroll,           {ScrollMove     | DirectionBottom   | UnitLine} },
};

/* mouse bindings
   you can use MOUSE_BUTTON_1 to MOUSE_BUTTON_5
*/
static Mouse mouse[] = {
    /* modmask,             modkey,         button,            function,        argument */
    { 0,                    0,              MOUSE_BUTTON_2,    paste,           {TargetCurrent  | ClipboardPrimary  | ClipboardGTK, rememberedURI} },
    { GDK_CONTROL_MASK,     0,              MOUSE_BUTTON_2,    paste,           {TargetNew  | ClipboardPrimary  | ClipboardGTK} },
    { GDK_CONTROL_MASK,     0,              MOUSE_BUTTON_1,    open_remembered, {TargetNew} },
};

/* settings (arguments of :set command) */
static Setting browsersettings[] = {
    /* public name,      internal variable   webkit setting                 integer value?  boolean value?   colour value?   reload page? */
    { "useragent",       useragent,          "user-agent",                  FALSE,          FALSE,           FALSE,          FALSE  },
    { "scripts",         NULL,               "enable-scripts",              FALSE,          TRUE,            FALSE,          FALSE  },
    { "plugins",         NULL,               "enable-plugins",              FALSE,          TRUE,            FALSE,          FALSE  },
    { "pagecache",       NULL,               "enable-page-cache",           FALSE,          TRUE,            FALSE,          FALSE  },
    { "java",            NULL,               "enable-java-applet",          FALSE,          TRUE,            FALSE,          FALSE  },
    { "images",          NULL,               "auto-load-images",            FALSE,          TRUE,            FALSE,          FALSE  },
    { "shrinkimages",    NULL,               "auto-shrink-images",          FALSE,          TRUE,            FALSE,          FALSE  },
    { "cursivefont",     NULL,               "cursive-font-family",         FALSE,          FALSE,           FALSE,          FALSE  },
    { "defaultencoding", NULL,               "default-encoding",            FALSE,          FALSE,           FALSE,          FALSE  },
    { "defaultfont",     NULL,               "default-font-family",         FALSE,          FALSE,           FALSE,          FALSE  },
    { "fontsize",        NULL,               "default-font-size",           TRUE,           FALSE,           FALSE,          FALSE  },
    { "monofontsize",    NULL,               "default-monospace-font-size", TRUE,           FALSE,           FALSE,          FALSE  },
    { "caret",           NULL,               "enable-caret-browsing",       FALSE,          TRUE,            FALSE,          FALSE  },
    { "fantasyfont",     NULL,               "fantasy-font-family",         FALSE,          FALSE,           FALSE,          FALSE  },
    { "minimumfontsize", NULL,               "minimum-font-size",           TRUE,           FALSE,           FALSE,          FALSE  },
    { "monofont",        NULL,               "monospace-font-family",       FALSE,          FALSE,           FALSE,          FALSE  },
    { "backgrounds",     NULL,               "print-backgrounds",           FALSE,          TRUE,            FALSE,          FALSE  },
    { "sansfont",        NULL,               "sans-serif-font-family",      FALSE,          FALSE,           FALSE,          FALSE  },
    { "seriffont",       NULL,               "serif-font-family",           FALSE,          FALSE,           FALSE,          FALSE  },
    { "stylesheet",      NULL,               "user-stylesheet-uri",         FALSE,          FALSE,           FALSE,          FALSE  },
    { "resizetextareas", NULL,               "resizable-text-areas",        FALSE,          TRUE,            FALSE,          FALSE  },
    { "webinspector",    NULL,               "enable-developer-extras",     FALSE,          TRUE,            FALSE,          FALSE  },

    { "homepage",        startpage,          "",                            FALSE,          FALSE,           FALSE,          FALSE  },
    { "statusbgcolor",   statusbgcolor,      "",                            FALSE,          FALSE,           TRUE,           TRUE   },
    { "statuscolor",     statuscolor,        "",                            FALSE,          FALSE,           TRUE,           TRUE   },
    { "sslbgcolor",      sslbgcolor,         "",                            FALSE,          FALSE,           TRUE,           TRUE   },
    { "sslcolor",        sslcolor,           "",                            FALSE,          FALSE,           TRUE,           TRUE   },
    { "acceptlanguage",  acceptlanguage,     "",                            FALSE,          FALSE,           FALSE,          FALSE  },
    { "defaultsearch",   defaultsearch,      "",                            FALSE,          FALSE,           FALSE,          FALSE  },
    { "qmark",           NULL,               "",                            FALSE,          FALSE,           FALSE,          FALSE  },
    { "proxy",           NULL,               "",                            FALSE,          TRUE,            FALSE,          FALSE  },
    { "scrollbars",      NULL,               "",                            FALSE,          TRUE,            FALSE,          FALSE  },
    { "statusbar",       NULL,               "",                            FALSE,          TRUE,            FALSE,          FALSE  },
    { "inputbox",        NULL,               "",                            FALSE,          TRUE,            FALSE,          FALSE  },
    { "completioncase",  NULL,               "",                            FALSE,          TRUE,            FALSE,          FALSE  },
};
