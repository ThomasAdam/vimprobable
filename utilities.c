/*
    (c) 2009 by Leon Winter
    (c) 2009-2012 by Hannes Schueller
    (c) 2009-2010 by Matto Fransen
    (c) 2010-2011 by Hans-Peter Deifel
    (c) 2010-2011 by Thomas Adam
    see LICENSE file
*/

#include "includes.h"
#include "vimprobable.h"
#include "main.h"
#include "utilities.h"

extern GList *commandhistory;
extern int commandpointer;
extern Command commands[COMMANDSIZE];
extern KeyList *keylistroot;
extern Key keys[];
extern char *error_msg;
extern gboolean complete_case_sensitive;
extern char *config_base;
static GList *dynamic_searchengines = NULL, *dynamic_uri_handlers = NULL;

void add_modkeys(char key);

void save_command_history(char *line)
{
	char *c = line;

	while (isspace(*c) && *c)
		c++;
	if (!strlen(c))
		return;

    if (COMMANDHISTSIZE <= g_list_length(commandhistory)) {
        /* if list is too long - remove items from beginning */
        commandhistory = g_list_delete_link(commandhistory, g_list_first(commandhistory));
    }
    commandhistory = g_list_append(commandhistory, g_strdup(c));
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
    mark = -1;
    mark = atoi(bm);
    if ( mark < 1 || mark > 9 ) {
	    echo_message(Error, "Invalid quickmark, only 1-9");
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
    g_free((gpointer *)filename);
    if (fp == NULL) return FALSE;
    for( i=0; i < 10; ++i ) 
        fprintf(fp, "%s\n", qmarks[i]);
    fclose(fp);
    echo_message(Error, "Saved as quickmark %d: %s", mark, uri);

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
process_line_arg(const Arg *arg) {
    return process_line(arg->s);
}

gboolean
changemapping(Key *search_key, int maprecord, char *cmd) {
    KeyList *current, *newkey;
    Arg a = { .s = cmd };

    /* sanity check */
    if (maprecord < 0 && cmd == NULL) {
        /* possible states:
         * - maprecord >= 0 && cmd == NULL: mapping to internal symbol
         * - maprecord < 0 && cmd != NULL: mapping to command line
         * - maprecord >= 0 && cmd != NULL: cmd will be ignored, treated as mapping to internal symbol
         * - anything else (gets in here): an error, hence we return FALSE */
        return FALSE;
    }

    current = keylistroot;

    if (current)
        while (current->next != NULL) {
            if (
                current->Element.mask   == search_key->mask &&
                current->Element.modkey == search_key->modkey &&
                current->Element.key    == search_key->key
               ) {
                if (maprecord >= 0) {
                    /* mapping to an internal signal */
                    current->Element.func = commands[maprecord].func;
                    current->Element.arg  = commands[maprecord].arg;
                } else {
                    /* mapping to a command line */
                    current->Element.func = process_line_arg;
                    current->Element.arg  = a;
                }
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
    if (maprecord >= 0) {
        /* mapping to an internal signal */
        newkey->Element.func = commands[maprecord].func;
        newkey->Element.arg  = commands[maprecord].arg;
    } else {
        /* mapping to a command line */
        newkey->Element.func = process_line_arg;
        newkey->Element.arg  = a;
    }
    add_modkeys(newkey->Element.modkey);

    newkey->next           = NULL;

    if (keylistroot == NULL) keylistroot = newkey;

    if (current != NULL) current->next = newkey;

    return TRUE;
}

void add_modkeys(char key)
{
    unsigned int k, len;
    extern char *modkeys;
    len = strlen( modkeys );
    while (k < len ) { 
        if ( modkeys[k] == key ) return;
        k++;
    }
    modkeys = realloc(modkeys, len + 2);
    modkeys[len] = key;
    modkeys[len+1] = '\0';
}

gboolean
mappings(const Arg *arg) {
    char line[255];

    if (!arg->s) {
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

int 
get_modkey(char key) {
    switch (key) {
        case '1':
            return GDK_MOD1_MASK;
        case '2':
            return GDK_MOD2_MASK;
        case '3':
            return GDK_MOD3_MASK;
        case '4':
            return GDK_MOD4_MASK;
        case '5':
            return GDK_MOD5_MASK;
        default:
            return FALSE;
    }
}

gboolean
process_mapping(char *keystring, int maprecord, char *cmd) {
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

    /* process stuff like <S-v> for Shift-v or <C-v> for Ctrl-v (strlen == 5),
       stuff like <S-v>a for Shift-v,a or <C-v>a for Ctrl-v,a (strlen == 6 && keystring[4] == '>')
       stuff like <M1-v> for Mod1-v (strlen == 6 && keystring[5] == '>')
       or stuff like <M1-v>a for Mod1-v,a (strlen = 7)
     */
    if (
        ((strlen(keystring) == 5 ||  strlen(keystring) == 6)  && keystring[0] == '<'  && keystring[4] == '>') || 
        ((strlen(keystring) == 6 ||  strlen(keystring) == 7)  && keystring[0] == '<'  && keystring[5] == '>')
       ) {
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
            case 'M':
                search_key.mask = get_modkey(keystring[2]);
            break;
        }
        if (!search_key.mask)
            return FALSE;
        if (strlen(keystring) == 5) {
            search_key.key = keystring[3];
        } else if (strlen(keystring) == 7) {
            search_key.modkey = keystring[4];
            search_key.key    = keystring[6];
        } else {
            if (search_key.mask == 'S' || search_key.mask == 'C') {
                search_key.modkey = keystring[3];
                search_key.key    = keystring[5];
            } else {
                search_key.key = keystring[4];
            }
        }
    }

    /* process stuff like a<S-v> for a,Shift-v or a<C-v> for a,Ctrl-v (strlen == 6)
       or stuff like a<M1-v> for a,Mod1-v (strlen == 7)
     */
    if (
        (strlen(keystring) == 6 && keystring[1] == '<' && keystring[5] == '>') || 
        (strlen(keystring) == 7 && keystring[1] == '<' && keystring[6] == '>')
       ) {
        switch (toupper(keystring[2])) {
            case 'S':
                search_key.mask = GDK_SHIFT_MASK;
                keystring[4] = toupper(keystring[4]);
            break;
            case 'C':
                search_key.mask = GDK_CONTROL_MASK;
            break;
            case 'M':
                search_key.mask = get_modkey(keystring[3]);
            break;
        }
        if (!search_key.mask)
            return FALSE;
        search_key.modkey= keystring[0];
        if (strlen(keystring) == 6) {
            search_key.key = keystring[4];
        } else {
            search_key.key = keystring[5];
        }
    }
    return (changemapping(&search_key, maprecord, cmd));
}

gboolean
process_map_line(char *line) {
    int listlen, i;
    char *c, *cmd;
    my_pair.line = line;
    c = search_word(0);

    if (!strlen(my_pair.what))
        return FALSE;
    while (isspace(*c) && *c)
        c++;

    if (*c == ':' || *c == '=')
        c++;
    my_pair.line = c;
    c = search_word(1);
    if (!strlen(my_pair.value))
        return FALSE;
    listlen = LENGTH(commands);
    for (i = 0; i < listlen; i++) {
        /* commands is fixed size */
        if (commands[i].cmd == NULL)
            break;
        if (strlen(commands[i].cmd) == strlen(my_pair.value) && strncmp(commands[i].cmd, my_pair.value, strlen(my_pair.value)) == 0) {
            /* map to an internal symbol */
            return process_mapping(my_pair.what, i, NULL);
        }
    }
    /* if this is reached, the mapping is not for one of the internal symbol - test for command line structure */
    if (strlen(my_pair.value) > 1 && strncmp(my_pair.value, ":", 1) == 0) {
        /* The string begins with a colon, like a command line, but it's not _just_ a colon, 
         * i.e. increasing the pointer by one will not go 'out of bounds'.
         * We don't actually check that the command line after the = is valid.
         * This is user responsibility, the worst case is the new mapping simply doing nothing.
         * Since we will pass the command to the same function which also handles the config file lines,
         * we have to strip the colon itself (a colon counts as a commented line there - like in vim).
         * Last, but not least, the second argument being < 0 signifies to the function that this is a 
         * command line mapping, not a mapping to an existing internal symbol. */
        cmd = (char *)malloc(sizeof(char) * strlen(my_pair.value));
        memset(cmd, 0, strlen(my_pair.value));
        strncpy(cmd, (my_pair.value + 1), strlen(my_pair.value) - 1);
        return process_mapping(my_pair.what, -1, cmd);
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
        t = 0;
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
echo_message(const MessageType type, const char *format, ...)
{
    Arg a;
    va_list ap;

    va_start(ap, format);
    a.i = type;
    a.s = g_strdup_vprintf(format, ap);
    echo(&a);
    g_free(a.s);
    va_end(ap);
}

Listelement *
complete_list(const char *searchfor, const int mode, Listelement *elementlist)
{
    FILE *f;
    const char *filename;
    Listelement *candidatelist = NULL, *candidatepointer = NULL;
    char s[255] = "", readelement[MAXTAGSIZE + 1] = "";
    int i, t, n = 0;

    if (mode == 2) {
        /* open in history file */
        filename = g_strdup_printf(HISTORY_STORAGE_FILENAME);
    } else {
        /* open in bookmark file (for tags and bookmarks) */
        filename = g_strdup_printf(BOOKMARKS_STORAGE_FILENAME);
    }
    f = fopen(filename, "r");
    if (f == NULL) {
        g_free((gpointer *)filename);
        return (elementlist);
    }

    while (fgets(s, 254, f)) {
        if (mode == 1) {
            /* just tags (could be more than one per line) */
            i = 0;
            while (s[i] && i < 254) {
                while (s[i] != '[' && s[i])
                    i++;
                if (s[i] != '[')
                    continue;
                i++;
                t = 0;
                while (s[i] != ']' && s[i] && t < MAXTAGSIZE)
                    readelement[t++] = s[i++];
                readelement[t] = '\0';
                candidatelist = add_list(readelement, candidatelist);
                i++;
            }
        } else {
            /* complete string (bookmarks & history) */
            candidatelist = add_list(s, candidatelist);
        }
        candidatepointer = candidatelist;
        while (candidatepointer != NULL) {
            strncpy(s, candidatepointer->element, sizeof(s));
            if (!complete_case_sensitive) {
                g_strdown(s);
            }
            if (!strlen(searchfor) || strstr(s, searchfor) != NULL) {
                /* only use string up to the first space */
                memset(readelement, 0, MAXTAGSIZE + 1);
                if (strchr(candidatepointer->element, ' ') != NULL) {
                    i = strcspn(candidatepointer->element, " ");
                    if (i > MAXTAGSIZE)
                        i = MAXTAGSIZE;
                    strncpy(readelement, candidatepointer->element, i);
                } else {
                    strncpy(readelement, candidatepointer->element, MAXTAGSIZE);
                }
                /* in the case of URLs without title, remove the line break */
                if (readelement[strlen(readelement) - 1] == '\n') {
                    readelement[strlen(readelement) - 1] = '\0';
                }
                elementlist = add_list(readelement, elementlist);
                n = count_list(elementlist);
            }
            if (n >= MAX_LIST_SIZE)
                break;
            candidatepointer = candidatepointer->next;
        }
        free_list(candidatelist);
        candidatelist = NULL;
        if (n >= MAX_LIST_SIZE)
            break;
    }
    g_free((gpointer)filename);
    return (elementlist);
}

Listelement *
add_list(const char *element, Listelement *elementlist)
{
    int n, i = 0;
    Listelement *newelement, *elementpointer, *lastelement;

    if (elementlist == NULL) { /* first element */
        newelement = malloc(sizeof(Listelement));
        if (newelement == NULL) 
            return (elementlist);
        strncpy(newelement->element, element, 254);
        newelement->next = NULL;
        return newelement;
    }
    elementpointer = elementlist;
    n = strlen(element);

    /* check if element is already in list */
    while (elementpointer != NULL) {
        if (strlen(elementpointer->element) == n && 
                strncmp(elementpointer->element, element, n) == 0)
            return (elementlist);
        lastelement = elementpointer;
        elementpointer = elementpointer->next;
        i++;
    }
    /* add to list */
    newelement = malloc(sizeof(Listelement));
    if (newelement == NULL)
        return (elementlist);
    lastelement->next = newelement;
    strncpy(newelement->element, element, 254);
    newelement->next = NULL;
    return elementlist;
}

void
free_list(Listelement *elementlist)
{
    Listelement *elementpointer;

    while (elementlist != NULL) {
        elementpointer = elementlist->next;
        free(elementlist);
        elementlist = elementpointer;
    }
}

int
count_list(Listelement *elementlist)
{
    Listelement *elementpointer = elementlist;
    int n = 0;

    while (elementpointer != NULL) {
        n++;
        elementpointer = elementpointer->next;
    }
    
    return n;
}

/* split the string at the first occurence of whitespace and return the
 * position of the second half.
 * Unlike strtok, the substrings can be empty and the second string is
 * stripped of trailing and leading whitespace.
 * Return -1 if `string' contains no whitespace */
static int split_string_at_whitespace(char *string)
{
    int index = strcspn(string, "\n\t ");
    if (string[index] != '\0') {
        string[index++] = 0;
        g_strstrip(string+index);
        return index;
    }
    return -1;
}

/* return TRUE, if the string contains exactly one unescaped %s and no other
 * printf directives */
static gboolean sanity_check_search_url(const char *string)
{
    int was_percent_char = 0, percent_s_count = 0;

    for (; *string; string++) {
        switch (*string) {
        case '%':
            was_percent_char = !was_percent_char;
            break;
        case 's':
            if (was_percent_char)
                percent_s_count++;
            was_percent_char = 0;
            break;
        default:
            if (was_percent_char)
                return FALSE;
            was_percent_char = 0;
            break;
        }
    }

    return !was_percent_char && percent_s_count == 1;
}

void make_searchengines_list(Searchengine *searchengines, int length)
{
    int i;
    for (i = 0; i < length; i++, searchengines++) {
        dynamic_searchengines = g_list_prepend(dynamic_searchengines, searchengines);
    }
}

/* find a searchengine with a given handle and return its URI or NULL if
 * nothing is found.
 * The returned string is internal and must not be freed or modified. */
char *find_uri_for_searchengine(const char *handle)
{
    GList *l;

    if (dynamic_searchengines != NULL) {
        for (l = dynamic_searchengines; l; l = g_list_next(l)) {
            Searchengine *s = (Searchengine*)l->data;
            if (!strcmp(s->handle, handle)) {
                return s->uri;
            }
        }
    }

    return NULL;
}

enum ConfigFileError
read_rcfile(const char *config)
{
	int t, linum = 0, index;
	char s[255], *buffer;
	FILE *fpin;
	gboolean found_malformed_lines = FALSE;
	Searchengine *new;
	URIHandler *newhandler;

   if (access(config, F_OK) != 0)
        return FILE_NOT_FOUND;

    fpin = fopen(config, "r");
    if (fpin == NULL)
        return READING_FAILED;

	while (fgets(s, 254, fpin)) {
		linum++;
		/*
		 * ignore lines that begin with #, / and such 
		 */
		if (!isalpha(s[0]))
			continue;
		t = strlen(s);
		s[t - 1] = '\0';
		if (strncmp(s, "searchengine", 12) == 0) {
			buffer = (s + 12);
			while (buffer[0] == ' ')
				buffer++;
			/* split line at whitespace */
			index = split_string_at_whitespace(buffer);
			if (index < 0 || buffer[0] == '\0' || buffer[index] == '\0'
					|| !sanity_check_search_url(buffer+index)) {
				fprintf(stderr, "searchengines: syntax error on line %d\n", linum);
				found_malformed_lines = TRUE;
				continue;
			}
			new = malloc(sizeof(Searchengine));
			if (new == NULL) {
				fprintf(stderr, "Memory exhausted while loading search engines.\n");
				exit(EXIT_FAILURE);
			}
			new->handle = g_strdup(buffer);
			new->uri = g_strdup(buffer+index);
			dynamic_searchengines = g_list_prepend(dynamic_searchengines, new);
		} else if (strncmp(s, "handler", 7) == 0) {
			buffer = (s + 7);
			while (buffer[0] == ' ')
				buffer++;
			/* split line at whitespace */
			index = split_string_at_whitespace(buffer);
			if (index < 0 || buffer[0] == '\0' || buffer[index] == '\0'
					|| !sanity_check_search_url(buffer+index)) { /* this sanity check is also valid for handler definitions */
				fprintf(stderr, "URI handlers: syntax error on line %d\n", linum);
				found_malformed_lines = TRUE;
				continue;
			}
			newhandler = malloc(sizeof(URIHandler));
			if (newhandler == NULL) {
				fprintf(stderr, "Memory exhausted while loading uri handlers.\n");
				exit(EXIT_FAILURE);
			}
			newhandler->handle = g_strdup(buffer);
			newhandler->handler = g_strdup(buffer+index);
			dynamic_uri_handlers = g_list_prepend(dynamic_uri_handlers, newhandler);
		} else {
			if (!process_line(s))
				found_malformed_lines = TRUE;
		}
	}
	fclose(fpin);
    return found_malformed_lines ? SYNTAX_ERROR : SUCCESS;
}

void make_uri_handlers_list(URIHandler *uri_handlers, int length)
{
    int i;
    for (i = 0; i < length; i++, uri_handlers++) {
        dynamic_uri_handlers = g_list_prepend(dynamic_uri_handlers, uri_handlers);
    }
}


/* spawn a child process handling a protocol encoded in URI.

On success, pid will contain the pid of the spawned child.
If you pass NULL as child_pid, glib will reap the child. */
gboolean
open_handler_pid(char *uri, GPid *child_pid) {
    char *argv[64];
    char *p = NULL, *arg, arg_temp[MAX_SETTING_SIZE], *temp, temp2[MAX_SETTING_SIZE] = "", *temp3;
    int j;
    GList *l;
    GSpawnFlags flags = G_SPAWN_SEARCH_PATH; 

    if (child_pid) {
        flags |= G_SPAWN_DO_NOT_REAP_CHILD;
    }
    p = strchr(uri, ':');
    if (p) {
    	if (dynamic_uri_handlers != NULL) {
        	for (l = dynamic_uri_handlers; l; l = g_list_next(l)) {
            	URIHandler *s = (URIHandler *)l->data;
            	if (strlen(uri) >= strlen(s->handle) && strncmp(s->handle, uri, strlen(s->handle)) == 0) {
                	if (strlen(s->handler) > 0) {
                    	arg = (uri + strlen(s->handle));
                    	strncpy(temp2, s->handler, MAX_SETTING_SIZE);
                    	temp = strtok(temp2, " ");
                    	j = 0;
                    	while (temp != NULL) {
                        	if (strstr(temp, "%s")) {
                            	temp3 = temp;
                            	memset(arg_temp, 0, MAX_SETTING_SIZE);
                            	while (strncmp(temp3, "%s", 2) != 0) {
                                	strncat(arg_temp, temp3, 1);
                                	temp3++;
                            	}
                            	strcat(arg_temp, arg);
                            	temp3++;
                            	temp3++;
                            	strcat(arg_temp, temp3);
                            	argv[j] = arg_temp;
                        	} else {
                            	argv[j] = temp;
                        	}
                        	temp = strtok(NULL, " ");
                        	j++;
                    	}
                    	argv[j] =  NULL;
                    	g_spawn_async(NULL, argv, NULL, flags,
                    	    NULL, NULL, child_pid, NULL);
                	}
                	return TRUE;
            	}
        	}
    	}
    }
    return FALSE;
}


gboolean
open_handler(char *uri) {
    return open_handler_pid(uri, NULL);
}
