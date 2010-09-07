/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    see LICENSE file
*/

/* config file */
#define             RCFILE                      "%s/.config/vimprobable/vimprobablerc", getenv("HOME")

/* max entries in command history */
#define COMMANDHISTSIZE 50

gboolean read_rcfile(void);
void save_command_history(char *line);
gboolean process_save_qmark(const char *bm, WebKitWebView *webview);
void make_keyslist(void);
gboolean parse_colour(char *color);
gboolean mappings(const Arg *arg);
gboolean process_mapping(char * keystring, int maprecord);
gboolean process_map_line(char *line);
gboolean changemapping(Key * search_key, int maprecord);
gboolean mappings(const Arg *arg);
gboolean build_taglist(const Arg *arg, FILE *f);
