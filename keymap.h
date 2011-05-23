/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    (c) 2010 by Hans-Peter Deifel
    (c) 2010 by Thomas Adam
    see LICENSE file
*/
#ifndef _KEYMAP

#define _KEYMAP

/* key bindings for normal mode
    Note: GDK_VoidSymbol is a wildcard so it matches on every modkey
*/
Key keys[] = {
    /* modmask,             modkey,         key,            function,   argument */
    { 0,                    GDK_q,          GDK_1,          quickmark,  { .s = "1" } },
    { 0,                    GDK_q,          GDK_2,          quickmark,  { .s = "2"  } },
    { 0,                    GDK_q,          GDK_3,          quickmark,  { .s = "3" } },
    { 0,                    GDK_q,          GDK_4,          quickmark,  { .s = "4" } },
    { 0,                    GDK_q,          GDK_5,          quickmark,  { .s = "5" } },
    { 0,                    GDK_q,          GDK_6,          quickmark,  { .s = "6" } },
    { 0,                    GDK_q,          GDK_7,          quickmark,  { .s = "7" } },
    { 0,                    GDK_q,          GDK_8,          quickmark,  { .s = "8" } },
    { 0,                    GDK_q,          GDK_9,          quickmark,  { .s = "9" } },
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

    { 0,                    GDK_g,          GDK_t,          fake_key_event,   { .s = "l", .i = ShiftMask + ControlMask   } },
    { GDK_SHIFT_MASK,       GDK_g,          GDK_T,          fake_key_event,   { .s = "h", .i = ShiftMask + ControlMask  } },
    { 0,                    GDK_g,          GDK_1,          fake_key_event,   { .s = "1", .i = ControlMask   } },
    { 0,                    GDK_g,          GDK_2,          fake_key_event,   { .s = "2", .i = ControlMask   } },
    { 0,                    GDK_g,          GDK_3,          fake_key_event,   { .s = "3", .i = ControlMask   } },
    { 0,                    GDK_g,          GDK_4,          fake_key_event,   { .s = "4", .i = ControlMask   } },
    { 0,                    GDK_g,          GDK_5,          fake_key_event,   { .s = "5", .i = ControlMask   } },
    { 0,                    GDK_g,          GDK_6,          fake_key_event,   { .s = "6", .i = ControlMask   } },
    { 0,                    GDK_g,          GDK_7,          fake_key_event,   { .s = "7", .i = ControlMask   } },
    { 0,                    GDK_g,          GDK_8,          fake_key_event,   { .s = "8", .i = ControlMask   } },
    { 0,                    GDK_g,          GDK_9,          fake_key_event,   { .s = "9", .i = ControlMask   } },
    { 0,                    GDK_g,          GDK_0,          fake_key_event,   { .s = "0", .i = ControlMask   } },

    { GDK_CONTROL_MASK,     0,              GDK_i,          navigate,   {NavigationBack} },
    { GDK_CONTROL_MASK,     0,              GDK_o,          navigate,   {NavigationForward} },
    { GDK_SHIFT_MASK,       0,              GDK_H,          navigate,   {NavigationBack} },
    { GDK_SHIFT_MASK,       0,              GDK_L,          navigate,   {NavigationForward} },
    { 0,                    0,              GDK_r,          navigate,   {NavigationReload} },
    { GDK_SHIFT_MASK,       0,              GDK_R,          navigate,   {NavigationForceReload} },
    { GDK_CONTROL_MASK,     0,              GDK_c,          navigate,   {NavigationCancel} },

    { 0,                    0,              GDK_plus,       zoom,       {ZoomIn         | ZoomText} },
    { 0,                    0,              GDK_minus,      zoom,       {ZoomOut        | ZoomText} },
    { 0,                    0,              GDK_KP_Add,     zoom,       {ZoomIn         | ZoomText} },
    { 0,                    0,              GDK_KP_Subtract,zoom,       {ZoomOut        | ZoomText} },
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

    { 0,                    GDK_g,          GDK_h,          open_arg,   {TargetCurrent, startpage} },
    { GDK_SHIFT_MASK,       GDK_g,          GDK_H,          open_arg,   {TargetNew,     startpage} },

    { 0,                    0,              GDK_p,          paste,      {TargetCurrent  | ClipboardPrimary  | ClipboardGTK} },
    { GDK_SHIFT_MASK,       0,              GDK_P,          paste,      {TargetNew      | ClipboardPrimary  | ClipboardGTK} },

    { GDK_CONTROL_MASK,     0,              GDK_a,          number,     {Increment} },
    { GDK_CONTROL_MASK,     0,              GDK_x,          number,     {Decrement} },

    { 0,                    0,              GDK_n,          search,     {DirectionNext      | CaseInsensitive   | Wrapping} },
    { GDK_SHIFT_MASK,       0,              GDK_N,          search,     {DirectionPrev      | CaseInsensitive   | Wrapping} },

    { GDK_SHIFT_MASK,       0,              GDK_colon,      input,      {.s = ":" } },
    { 0,                    0,              GDK_o,          input,      {.s = ":open "} },
    { GDK_SHIFT_MASK,       0,              GDK_O,          input,      {.s = ":open ", .i = InsertCurrentURL} },
    { 0,                    0,              GDK_t,          input,      {.s = ":tabopen "} },
    { GDK_SHIFT_MASK,       0,              GDK_T,          input,      {.s = ":tabopen ", .i = InsertCurrentURL} },
    { 0,                    0,              GDK_slash,      input,      {.s = "/"} },
    { GDK_SHIFT_MASK,       0,              GDK_slash,      input,      {.s = "/"} },
    { 0,                    0,              GDK_KP_Divide,  input,      {.s = "/"} },
    { GDK_SHIFT_MASK,       0,              GDK_question,   input,      {.s = "?"} },

    { 0,                    0,              GDK_quoteleft,  input,      {.s = "`"} },

    { 0,                    GDK_VoidSymbol, GDK_Escape,     set,        {ModeNormal} },
    { GDK_CONTROL_MASK,     GDK_VoidSymbol, GDK_bracketleft,set,        {ModeNormal} },
    { GDK_CONTROL_MASK,     0,              GDK_z,          set,        {ModePassThrough} },
    { GDK_CONTROL_MASK,     0,              GDK_v,          set,        {ModeSendKey} },
    { 0,                    0,              GDK_f,          set,        { .i = ModeHints, .s = "current" } },
    { GDK_SHIFT_MASK,       0,              GDK_F,          set,        { .i = ModeHints, .s = "new" } },

    { 0,                    GDK_g,          GDK_i,          focus_input,{} },
    { 0,                    0,              GDK_u,          revive,     {} },

    { 0,                    0,              GDK_d,          quit,       {0} },
	/* leave this last line as last */
    { 0,                    0,              0,              0,          {0} },
};

#endif
