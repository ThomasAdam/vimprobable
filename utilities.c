/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    see LICENSE file
*/

#include "includes.h"
#include "vimprobable.h"
#include "main.h"
#include "utilities.h"

extern char commandhistory[COMMANDHISTSIZE][255];
extern int lastcommand, maxcommands, commandpointer;
extern KeyList *keylistroot;
extern Key keys[];

gboolean read_rcfile(void)
{
	int t;
	char s[255];
	const char *rcfile;
	FILE *fpin;
	gboolean returnval = TRUE;

	rcfile = g_strdup_printf(RCFILE);
	if ((fpin = fopen(rcfile, "r")) == NULL)
		return TRUE;
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
	    g_free(a.s);
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
    g_free(a.s);

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
