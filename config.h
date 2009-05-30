/*
    (c) 2009 by Leon Winter, see LICENSE file
*/

/* general settings */
static char startpage[]                 = "https://projects.ring0.de/webkitbrowser/";

/* appearance */
static const char statusbgcolor[]       = "#000000";            /* background color for status bar */
static const char statuscolor[]         = "#ffffff";            /* color for status bar */
static const char sslbgcolor[]          = "#b0ff00";            /* background color for status bar with SSL url */
static const char sslcolor[]            = "#000000";            /* color for status bar with SSL url */

                                        /*  normal,                 warning,                error       */
static const char *urlboxfont[]         = { "monospace normal 8",   "monospace normal 8",   "monospace bold 8"};
static const char *urlboxcolor[]        = { NULL,                   "#ff0000",              "#ffffff" };
static const char *urlboxbgcolor[]      = { NULL,                   NULL,                   "#ff0000" };

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
#define             COOKIES_STORAGE_FILENAME    "%s/cookies.txt", getenv("HOME")
#define             COOKIES_STORAGE_READONLY    FALSE   /* if TRUE new cookies will be lost if you quit */

/* scrolling */
static unsigned int scrollstep          = 40;   /* cursor difference in pixel */
static unsigned int pagingkeep          = 40;   /* pixels kept when paging */
#define             DISABLE_SCROLLBAR

/* searching */
#define             ENABLE_MATCH_HIGHLITING
static const int searchoptions          = DirectionForward | CaseInsensitive | Wrapping;

/* search engines */
static Searchengine searchengines[] = {
    { "s",          "https://ssl.scroogle.org/cgi-bin/nbbwssl.cgi?Gw=%s" },
    { "m",          "http://www.google.de/maps?q=%s" },
    { "g",          "http://www.google.de/search?q=%s" },
    { "w",          "https://secure.wikimedia.org/wikipedia/de/w/index.php?title=Special%%3ASearch&search=%s&go=Go" },
};
static Searchengine *defsearch = &searchengines[0];

/* webkit settings */
#define WEBKITSETTINGS \
    "default-font-size",                12, \
    "user-stylesheet-uri",              NULL, \
    "enable-plugins",                   TRUE, \
    "enable-scripts",                   TRUE

/* key bindings for normal mode
    Note: GDK_VoidSymbol is a wildcard so it matches on every modkey
*/
static Key keys[] = {
    /* modmask,             modkey,         key,            function,   argument */
    { 0,                    0,              GDK_0,          scroll,     {ScrollJumpTo   | DirectionLeft} },
    { GDK_SHIFT_MASK,       0,              GDK_dollar,     scroll,     {ScrollJumpTo   | DirectionRight} },
    { 0,                    GDK_g,          GDK_g,          scroll,     {ScrollJumpTo   | DirectionTop} },
    { GDK_SHIFT_MASK,       0,              GDK_G,          scroll,     {ScrollJumpTo   | DirectionBottom} },
    { 0,                    0,              GDK_h,          scroll,     {ScrollMove     | DirectionLeft     | UnitLine} },
    { 0,                    0,              GDK_j,          scroll,     {ScrollMove     | DirectionBottom   | UnitLine} },
    { 0,                    0,              GDK_k,          scroll,     {ScrollMove     | DirectionTop      | UnitLine} },
    { 0,                    0,              GDK_l,          scroll,     {ScrollMove     | DirectionRight    | UnitLine} },
    { 0,                    0,              GDK_space,      scroll,     {ScrollMove     | DirectionBottom   | UnitPage} },
    { GDK_SHIFT_MASK,       0,              GDK_space,      scroll,     {ScrollMove     | DirectionTop      | UnitPage} },
    { GDK_CONTROL_MASK,     0,              GDK_b,          scroll,     {ScrollMove     | DirectionTop      | UnitPage} },
    { GDK_CONTROL_MASK,     0,              GDK_f,          scroll,     {ScrollMove     | DirectionBottom   | UnitPage} },
    { GDK_CONTROL_MASK,     0,              GDK_d,          scroll,     {ScrollMove     | DirectionBottom   | UnitBuffer} },
    { GDK_CONTROL_MASK,     0,              GDK_u,          scroll,     {ScrollMove     | DirectionTop      | UnitBuffer} },
    { GDK_CONTROL_MASK,     0,              GDK_e,          scroll,     {ScrollMove     | DirectionBottom   | UnitLine} },
    { GDK_CONTROL_MASK,     0,              GDK_y,          scroll,     {ScrollMove     | DirectionTop      | UnitLine} },

    { GDK_CONTROL_MASK,     0,              GDK_i,          navigate,   {NavigationBack} },
    { GDK_CONTROL_MASK,     0,              GDK_o,          navigate,   {NavigationForward} },
    { GDK_SHIFT_MASK,       0,              GDK_H,          navigate,   {NavigationBack} },
    { GDK_SHIFT_MASK,       0,              GDK_L,          navigate,   {NavigationForward} },
    { 0,                    0,              GDK_r,          navigate,   {NavigationReload} },
    { GDK_SHIFT_MASK,       0,              GDK_R,          navigate,   {NavigationForceReload} },
    { GDK_CONTROL_MASK,     0,              GDK_c,          navigate,   {NavigationCancel} },

    { 0,                    0,              GDK_plus,       zoom,       {ZoomIn         | ZoomText} },
    { 0,                    0,              GDK_minus,      zoom,       {ZoomOut        | ZoomText} },
    { 0,                    GDK_z,          GDK_i,          zoom,       {ZoomIn         | ZoomText} },
    { 0,                    GDK_z,          GDK_o,          zoom,       {ZoomOut        | ZoomText} },
    { 0,                    GDK_z,          GDK_z,          zoom,       {ZoomReset      | ZoomText} },
    { GDK_SHIFT_MASK,       GDK_z,          GDK_I,          zoom,       {ZoomIn         | ZoomFullContent} },
    { GDK_SHIFT_MASK,       GDK_z,          GDK_O,          zoom,       {ZoomOut        | ZoomFullContent} },
    { GDK_SHIFT_MASK,       GDK_z,          GDK_Z,          zoom,       {ZoomReset      | ZoomFullContent} },

    { 0,                    0,              GDK_y,          yank,       {SourceURL      | ClipboardPrimary  | ClipboardGTK} },
    { GDK_SHIFT_MASK,       0,              GDK_Y,          yank,       {SourceSelection} },

    { 0,                    GDK_g,          GDK_u,          descend,    {NthSubdir} },
    { GDK_SHIFT_MASK,       GDK_g,          GDK_U,          descend,    {Rootdir} },

    { 0,                    GDK_g,          GDK_h,          open,       {TargetCurrent, startpage} },
    { GDK_SHIFT_MASK,       GDK_g,          GDK_H,          open,       {TargetNew,     startpage} },

    { 0,                    0,              GDK_p,          paste,      {TargetCurrent  | ClipboardPrimary  | ClipboardGTK} },
    { GDK_SHIFT_MASK,       0,              GDK_P,          paste,      {TargetNew      | ClipboardPrimary  | ClipboardGTK} },

    { GDK_CONTROL_MASK,     0,              GDK_a,          number,     {Increment} },
    { GDK_CONTROL_MASK,     0,              GDK_x,          number,     {Decrement} },

    { 0,                    0,              GDK_n,          search,     {DirectionForward   | CaseInsensitive   | Wrapping} },
    { GDK_SHIFT_MASK,       0,              GDK_N,          search,     {DirectionBackwards | CaseInsensitive   | Wrapping} },

    { GDK_SHIFT_MASK,       0,              GDK_colon,      input,      {.s = ":" } },
    { 0,                    0,              GDK_o,          input,      {.s = ":open "} },
    { GDK_SHIFT_MASK,       0,              GDK_O,          input,      {.s = ":open ", .i = InsertCurrentURL} },
    { 0,                    0,              GDK_t,          input,      {.s = ":tabopen "} },
    { GDK_SHIFT_MASK,       0,              GDK_T,          input,      {.s = ":tabopen ", .i = InsertCurrentURL} },
    { GDK_SHIFT_MASK,       0,              GDK_slash,      input,      {.s = "/"} },

    { 0,                    GDK_VoidSymbol, GDK_Escape,     set,        {ModeNormal} },
    { GDK_CONTROL_MASK,     0,              GDK_z,          set,        {ModePassThrough} },
    { GDK_CONTROL_MASK,     0,              GDK_v,          set,        {ModeSendKey} },

    { 0,                    0,              GDK_d,          quit,       {0} },
};

/* command mapping */
static Command commands[] = {
    /* command,                                             function,   argument */
    { "ba",                                                 navigate,   {NavigationBack} },
    { "back",                                               navigate,   {NavigationBack} },
    { "ec",                                                 echo,       {Info} },
    { "echo",                                               echo,       {Info} },
    { "echoe",                                              echo,       {Error} },
    { "echoerr",                                            echo,       {Error} },
    { "fw",                                                 navigate,   {NavigationForward} },
    { "fo",                                                 navigate,   {NavigationForward} },
    { "forward",                                            navigate,   {NavigationForward} },
    { "o",                                                  open,       {TargetCurrent} },
    { "open",                                               open,       {TargetCurrent} },
    { "q",                                                  quit,       {0} },
    { "quit",                                               quit,       {0} },
    { "re",                                                 navigate,   {NavigationReload} },
    { "re!",                                                navigate,   {NavigationForceReload} },
    { "reload",                                             navigate,   {NavigationReload} },
    { "reload!",                                            navigate,   {NavigationForceReload} },
    { "st",                                                 navigate,   {NavigationCancel} },
    { "stop",                                               navigate,   {NavigationCancel} },
    { "t",                                                  open,       {TargetNew} },
    { "tabopen",                                            open,       {TargetNew} },
};
