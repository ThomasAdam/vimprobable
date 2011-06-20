/*
    (c) 2009 by Leon Winter
    (c) 2009-2011 by Hannes Schueller
    (c) 2009-2010 by Matto Fransen
    (c) 2010-2011 by Hans-Peter Deifel
    (c) 2010-2011 by Thomas Adam
    see LICENSE file
*/

/* config file */
#define             RCFILE                      "%s/.config/vimprobable/vimprobablerc", getenv("HOME")

/* max entries in command history */
#define COMMANDHISTSIZE 50

gboolean read_rcfile(const char *config);
void save_command_history(char *line);
gboolean process_save_qmark(const char *bm, WebKitWebView *webview);
void make_keyslist(void);
gboolean parse_colour(char *color);
gboolean mappings(const Arg *arg);
gboolean process_mapping(char *keystring, int maprecord, char *cmd);
gboolean process_map_line(char *line);
gboolean changemapping(Key *search_key, int maprecord, char *cmd);
gboolean process_line_arg(const Arg *arg);
gboolean build_taglist(const Arg *arg, FILE *f);
void set_error(const char *error);
void give_feedback(const char *feedback);
Listelement * complete_list(const char *searchfor, const int mode, Listelement *elementlist);
Listelement * add_list(const char *element, Listelement *elementlist);
int count_list(Listelement *elementlist);
void free_list(Listelement *elementlist);

enum ConfigFileError read_searchengines(const char *filename);
char *find_uri_for_searchengine(const char *handle);
void make_searchengines_list(Searchengine *searchengines, int length);
