/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2009, 2010 by Matto Fransen
    see LICENSE file
*/

#include "includes.h"
#include "vimprobable.h"
#include "keymap.h"

extern KeyList *keylistroot;
extern Key keys[];

void make_keyslist(void)
{
	int i;
	KeyList *ptr, *current;

	ptr = NULL;
	current = NULL;
	for (i = 0; i < LENGTH(keys); i++) {
		current = malloc(sizeof(KeyList));
		if (current == NULL) {
			printf("Not enough memory\n");
			exit(-1);
		}
		current->Element = keys[i];
		current->next = NULL;
		if (keylistroot == NULL)
			keylistroot = current;
		if (ptr != NULL)
			ptr->next = current;
		ptr = current;
	}
}
