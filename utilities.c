/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    (c) 2010 by Hans-Peter Deifel
    (c) 2010 by Thomas Adams
    see LICENSE file
*/

#include "includes.h"
#include "vimprobable.h"
#include "main.h"
#include "utilities.h"

extern char commandhistory[COMMANDHISTSIZE][255];
extern Command *commands;
extern int lastcommand, maxcommands, commandpointer;
extern KeyList *keylistroot;
extern Key keys[];
extern char *error_msg;

gboolean read_rcfile(const char *config)
{
	int t;
	char s[255];
	FILE *fpin;
	gboolean returnval = TRUE;

	if ((fpin = fopen(config, "r")) == NULL)
		return FALSE;
	while (fgets(s, 254, fpin)) {
		/*
		 * ignore lines that begin with #, / and such 
		 */
		if (!isalpha(s[0]))
			continue;
		t = strlen(s);
		s[t - 1] = '\0';
		if (!process_line(s))
			returnval = FALSE;
	}
	fclose(fpin);
	return returnval;
}

void save_command_history(char *line)
{
	char *c;

	c = line;
	while (isspace(*c) && *c)
		c++;
	if (!strlen(c))
		return;
	strncpy(commandhistory[lastcommand], c, 254);
	lastcommand++;
	if (maxcommands < COMMANDHISTSIZE - 1)
		maxcommands++;
	if (lastcommand == COMMANDHISTSIZE)
		lastcommand = 0;
	commandpointer = lastcommand;
}

gboolean
process_save_qmark(const char *bm, WebKitWebView *webview)
{
    FILE *fp;
    const char *filename;
    const char *uri = webkit_web_view_get_uri(webview);
    char qmarks[10][101];
    char buf[100];
    int  i, mark, l=0;
    Arg a;
    mark = -1;
    mark = atoi(bm);
    if ( mark < 1 || mark > 9 ) 
    {
	    a.i = Error;
	    a.s = g_strdup_printf("Invalid quickmark, only 1-9");
	    echo(&a);
	    return TRUE;
    }	    
    if ( uri == NULL ) return FALSE;
    for( i=0; i < 9; ++i ) strcpy( qmarks[i], "");

    filename = g_strdup_printf(QUICKMARK_FILE);

    /* get current quickmarks */
    
    fp = fopen(filename, "r");
    if (fp != NULL){
       for( i=0; i < 10; ++i ) {
           if (feof(fp)) {
               break;
           }
           fgets(buf, 100, fp);
	   l = 0;
	   while (buf[l] && l < 100 && buf[l] != '\n') {
		   qmarks[i][l]=buf[l]; 
		   l++;
	  }	   
          qmarks[i][l]='\0';
       }
       fclose(fp);
    }

    /* save quickmarks */
    strcpy( qmarks[mark-1], uri );
    fp = fopen(filename, "w");
    if (fp == NULL) return FALSE;
    for( i=0; i < 10; ++i ) 
        fprintf(fp, "%s\n", qmarks[i]);
    fclose(fp);
    a.i = Error;
    a.s = g_strdup_printf("Saved as quickmark %d: %s", mark, uri);
    echo(&a);

    return TRUE;
}

void
make_keyslist(void) 
{
    int i;
    KeyList *ptr, *current;

    ptr     = NULL;
    current = NULL;
    i       = 0;
    while ( keys[i].key != 0 )
    {
        current = malloc(sizeof(KeyList));
        if (current == NULL) {
            printf("Not enough memory\n");
            exit(-1);
        }
        current->Element = keys[i];
        current->next = NULL;
        if (keylistroot == NULL) keylistroot = current;
        if (ptr != NULL) ptr->next = current;
        ptr = current;
        i++;
    }
}

gboolean
parse_colour(char *color) {
    char goodcolor[8];
    int colorlen;

    colorlen = (int)strlen(color);

    goodcolor[0] = '#';
    goodcolor[7] = '\0';

    /* help the user a bit by making string like
       #a10 and strings like ffffff full 6digit
       strings with # in front :)
     */

    if (color[0] == '#') {
        switch (colorlen) {
            case 7:
                strncpy(goodcolor, color, 7);
            break;
            case 4:
                goodcolor[1] = color[1];
                goodcolor[2] = color[1];
                goodcolor[3] = color[2];
                goodcolor[4] = color[2];
                goodcolor[5] = color[3];
                goodcolor[6] = color[3];
            break;
            case 2:
                goodcolor[1] = color[1];
                goodcolor[2] = color[1];
                goodcolor[3] = color[1];
                goodcolor[4] = color[1];
                goodcolor[5] = color[1];
                goodcolor[6] = color[1];
            break;
        }
    } else {
        switch (colorlen) {
            case 6:
                strncpy(&goodcolor[1], color, 6);
            break;
            case 3:
                goodcolor[1] = color[0];
                goodcolor[2] = color[0];
                goodcolor[3] = color[1];
                goodcolor[4] = color[1];
                goodcolor[5] = color[2];
                goodcolor[6] = color[2];
            break;
            case 1:
                goodcolor[1] = color[0];
                goodcolor[2] = color[0];
                goodcolor[3] = color[0];
                goodcolor[4] = color[0];
                goodcolor[5] = color[0];
                goodcolor[6] = color[0];
            break;
        }
    }

    if (strlen (goodcolor) != 7) {
        return FALSE;
    } else {
        strncpy(color, goodcolor, 8);
        return TRUE;
    }
}

gboolean
changemapping(Key * search_key, int maprecord) {
    KeyList *current, *newkey;

    current = keylistroot;

    if (current)
        while (current->next != NULL) {
            if (
                current->Element.mask   == search_key->mask &&
                current->Element.modkey == search_key->modkey &&
                current->Element.key    == search_key->key
               ) {
                current->Element.func = commands[maprecord].func;
                current->Element.arg  = commands[maprecord].arg;
                return TRUE;
            }
            current = current->next;
        }
    newkey = malloc(sizeof(KeyList));
    if (newkey == NULL) {
        printf("Not enough memory\n");
        exit (-1);
    }
    newkey->Element.mask   = search_key->mask;
    newkey->Element.modkey = search_key->modkey;
    newkey->Element.key    = search_key->key;
    newkey->Element.func   = commands[maprecord].func;
    newkey->Element.arg    = commands[maprecord].arg;
    newkey->next           = NULL;

    if (keylistroot == NULL) keylistroot = newkey;

    if (current != NULL) current->next = newkey;

    return TRUE;
}

gboolean
mappings(const Arg *arg) {
    char line[255];

    if ( !arg->s ) {
        set_error("Missing argument.");
        return FALSE;
    }
    strncpy(line, arg->s, 254);
    if (process_map_line(line))
        return TRUE;
    else {
        set_error("Invalid mapping.");
        return FALSE;
    }
}

gboolean
process_mapping(char * keystring, int maprecord) {
    Key search_key;

    search_key.mask   = 0;
    search_key.modkey = 0;
    search_key.key    = 0;

    if (strlen(keystring) == 1) {
        search_key.key = keystring[0];
    }

    if (strlen(keystring) == 2) {
        search_key.modkey= keystring[0];
        search_key.key = keystring[1];
    }

    /* process stuff like <S-v> for Shift-v or <C-v> for Ctrl-v
       or stuff like <S-v>a for Shift-v,a or <C-v>a for Ctrl-v,a
    */
    if ((strlen(keystring) == 5 ||  strlen(keystring) == 6)  && keystring[0] == '<'  && keystring[4] == '>') {
        switch (toupper(keystring[1])) {
            case 'S':
                search_key.mask = GDK_SHIFT_MASK;
                if (strlen(keystring) == 5) {
                    keystring[3] = toupper(keystring[3]);
                } else {
                    keystring[3] = tolower(keystring[3]);
                    keystring[5] = toupper(keystring[5]);
                }
            break;
            case 'C':
                search_key.mask = GDK_CONTROL_MASK;
            break;
        }
        if (!search_key.mask)
            return FALSE;
        if (strlen(keystring) == 5) {
            search_key.key = keystring[3];
        } else {
            search_key.modkey= keystring[3];
            search_key.key = keystring[5];
        }
    }

    /* process stuff like <S-v> for Shift-v or <C-v> for Ctrl-v
       or  stuff like a<S-v> for a,Shift-v or a<C-v> for a,Ctrl-v
    */
    if (strlen(keystring) == 6 && keystring[1] == '<' && keystring[5] == '>') {
        switch (toupper(keystring[2])) {
            case 'S':
                search_key.mask = GDK_SHIFT_MASK;
                keystring[4] = toupper(keystring[4]);
            break;
            case 'C':
                search_key.mask = GDK_CONTROL_MASK;
            break;
        }
        if (!search_key.mask)
            return FALSE;
        search_key.modkey= keystring[0];
        search_key.key = keystring[4];
    }
    return (changemapping(&search_key, maprecord));
}

gboolean
process_map_line(char *line) {
    int listlen, i;
    char *c;
    my_pair.line = line;
    c = search_word (0);

    if (!strlen (my_pair.what))
        return FALSE;
    while (isspace (*c) && *c)
        c++;

    if (*c == ':' || *c == '=')
        c++;
    my_pair.line = c;
    c = search_word (1);
    if (!strlen (my_pair.value))
        return FALSE;
    listlen = LENGTH(commands);
    for (i = 0; i < listlen; i++) {
        if (strlen(commands[i].cmd) == strlen(my_pair.value) && strncmp(commands[i].cmd, my_pair.value, strlen(my_pair.value)) == 0)
            return process_mapping(my_pair.what, i);
    }
    return FALSE;
}

gboolean
build_taglist(const Arg *arg, FILE *f) {
    int k = 0, in_tag = 0;
    int t = 0, marker = 0;
    char foundtab[MAXTAGSIZE+1];
    while (arg->s[k]) {
        if (!isspace(arg->s[k]) && !in_tag) {
            in_tag = 1;
            marker = k;
        }
        if (isspace(arg->s[k]) && in_tag) {
            /* found a tag */
            t = 0;
            while (marker < k && t < MAXTAGSIZE) foundtab[t++] = arg->s[marker++];
            foundtab[t] = '\0';
            fprintf(f, " [%s]", foundtab);
            in_tag = 0;
        }
        k++;
    }
    if (in_tag) {
        while (marker < strlen(arg->s) && t < MAXTAGSIZE) foundtab[t++] = arg->s[marker++];
        foundtab[t] = '\0';
        fprintf(f, " [%s]", foundtab );
    }
    return TRUE;
}

void
set_error(const char *error) {
    /* it should never happen that set_error is called more than once, 
     * but to avoid any potential memory leaks, we ignore any subsequent 
     * error if the current one has not been shown */
    if (error_msg == NULL) {
        error_msg = g_strdup_printf("%s", error);
    }
}

void 
give_feedback(const char *feedback) 
{ 
    Arg a = { .i = Info };

    a.s = g_strdup_printf(feedback);
    echo(&a);
}
