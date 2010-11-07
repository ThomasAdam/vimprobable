/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    (c) 2010 by Hans-Peter Deifel
    (c) 2010 by Thomas Adam
    see LICENSE file
*/

/* functions */
void update_state(void);
gboolean process_line(char *line);
gboolean echo(const Arg *arg);
char * search_word(int whichword);
