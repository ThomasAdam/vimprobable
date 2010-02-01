/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    see LICENSE file
*/

#include "includes.h"
#include "main.h"
#include "utilities.h"

extern char commandhistory[COMMANDHISTSIZE][255];
extern int lastcommand, maxcommands, commandpointer;

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
