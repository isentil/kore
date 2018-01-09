/* ************************************************************************
*   File: ban.c                                         Part of CircleMUD *
*  Usage: banning/unbanning/checking sites and player names               *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"

struct ban_list_element *ban_list = NULL;


char *ban_types[] = {
  "no",
  "new",
  "select",
  "all",
  "ERROR"
};


void load_banned(void)
{
  FILE *fl;
  int i, date;
  char site_name[BANNED_SITE_LENGTH + 1], ban_type[100];
  char name[MAX_NAME_LENGTH + 1];
  struct ban_list_element *next_node;

  ban_list = 0;

  if (!(fl = fopen(BAN_FILE, "r"))) {
    perror("Unable to open banfile");
    return;
  }
  while (fscanf(fl, " %s %s %d %s ", ban_type, site_name, &date, name) == 4) {
    CREATE(next_node, struct ban_list_element, 1);
    strncpy(next_node->site, site_name, BANNED_SITE_LENGTH);
    next_node->site[BANNED_SITE_LENGTH] = '\0';
    strncpy(next_node->name, name, MAX_NAME_LENGTH);
    next_node->name[MAX_NAME_LENGTH] = '\0';
    next_node->date = date;

    for (i = BAN_NOT; i <= BAN_ALL; i++)
      if (!strcmp(ban_type, ban_types[i]))
	next_node->type = i;

    next_node->next = ban_list;
    ban_list = next_node;
  }

  fclose(fl);
}


int isbanned(char *hostname)
{
  int i;
  struct ban_list_element *banned_node;
  char *nextchar;

  if (!hostname || !*hostname)
    return (0);

  i = 0;
  for (nextchar = hostname; *nextchar; nextchar++)
    *nextchar = LOWER(*nextchar);

  for (banned_node = ban_list; banned_node; banned_node = banned_node->next)
    if (strstr(hostname, banned_node->site))	/* if hostname is a substring */
      i = MAX(i, banned_node->type);

  return i;
}


void _write_one_node(FILE * fp, struct ban_list_element * node)
{
  if (node) {
    _write_one_node(fp, node->next);
    fprintf(fp, "%s %s %ld %s\n", ban_types[node->type],
	    node->site, (long) node->date, node->name);
  }
}



void write_ban_list(void)
{
  FILE *fl;

  if (!(fl = fopen(BAN_FILE, "w"))) {
    perror("write_ban_list");
    return;
  }
  _write_one_node(fl, ban_list);/* recursively write from end to start */
  fclose(fl);
  return;
}


ACMD(do_ban)
{
  char flag[MAX_INPUT_LENGTH], site[MAX_INPUT_LENGTH],
	format[MAX_INPUT_LENGTH], *nextchar, *timestr;
  int i;
  struct ban_list_element *ban_node;

  *buf = '\0';

  if (!*argument) {
    if (!ban_list) {
      send_to_char("No sites are banned.\r\n", ch);
      return;
    }
    strcpy(format, "%-25.25s  %-8.8s  %-10.10s  %-16.16s\r\n");
    sprintf(buf, format,
	    "Banned Site Name",
	    "Ban Type",
	    "Banned On",
	    "Banned By");
    send_to_char(buf, ch);
    sprintf(buf, format,
	    "---------------------------------",
	    "---------------------------------",
	    "---------------------------------",
	    "---------------------------------");
    send_to_char(buf, ch);

    for (ban_node = ban_list; ban_node; ban_node = ban_node->next) {
      if (ban_node->date) {
	timestr = asctime(localtime(&(ban_node->date)));
	*(timestr + 10) = 0;
	strcpy(site, timestr);
      } else
	strcpy(site, "Unknown");
      sprintf(buf, format, ban_node->site, ban_types[ban_node->type], site,
	      ban_node->name);
      send_to_char(buf, ch);
    }
    return;
  }
  two_arguments(argument, flag, site);
  if (!*site || !*flag) {
    send_to_char("Usage: ban {all | select | new} site_name\r\n", ch);
    return;
  }
  if (!(!str_cmp(flag, "select") || !str_cmp(flag, "all") || !str_cmp(flag, "new"))) {
    send_to_char("Flag must be ALL, SELECT, or NEW.\r\n", ch);
    return;
  }
  for (ban_node = ban_list; ban_node; ban_node = ban_node->next) {
    if (!str_cmp(ban_node->site, site)) {
      send_to_char("That site has already been banned -- unban it to change the ban type.\r\n", ch);
      return;
    }
  }

  CREATE(ban_node, struct ban_list_element, 1);
  strncpy(ban_node->site, site, BANNED_SITE_LENGTH);
  for (nextchar = ban_node->site; *nextchar; nextchar++)
    *nextchar = LOWER(*nextchar);
  ban_node->site[BANNED_SITE_LENGTH] = '\0';
  strncpy(ban_node->name, GET_NAME(ch), MAX_NAME_LENGTH);
  ban_node->name[MAX_NAME_LENGTH] = '\0';
  ban_node->date = time(0);

  for (i = BAN_NEW; i <= BAN_ALL; i++)
    if (!str_cmp(flag, ban_types[i]))
      ban_node->type = i;

  ban_node->next = ban_list;
  ban_list = ban_node;

  sprintf(buf, "%s has banned %s for %s players.", GET_NAME(ch), site,
	  ban_types[ban_node->type]);
  mudlog(buf, NRM, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);
  send_to_char("Site banned.\r\n", ch);
  write_ban_list();
}


ACMD(do_unban)
{
  char site[80];
  struct ban_list_element *ban_node, *temp;
  int found = 0;

  one_argument(argument, site);
  if (!*site) {
    send_to_char("A site to unban might help.\r\n", ch);
    return;
  }
  ban_node = ban_list;
  while (ban_node && !found) {
    if (!str_cmp(ban_node->site, site))
      found = 1;
    else
      ban_node = ban_node->next;
  }

  if (!found) {
    send_to_char("That site is not currently banned.\r\n", ch);
    return;
  }
  REMOVE_FROM_LIST(ban_node, ban_list, next);
  send_to_char("Site unbanned.\r\n", ch);
  sprintf(buf, "%s removed the %s-player ban on %s.",
	  GET_NAME(ch), ban_types[ban_node->type], ban_node->site);
  mudlog(buf, NRM, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);

  free(ban_node);
  write_ban_list();
}


/**************************************************************************
 *  Code to check for invalid names (i.e., profanity, etc.)		  *
 *  Written by Sharon P. Goza						  *
 **************************************************************************/

typedef char namestring[MAX_NAME_LENGTH];

namestring *invalid_list = NULL;
int num_invalid = 0;

int Valid_Name(char *newname)
{
  int i;

  char tempname[MAX_NAME_LENGTH];

  /* return valid if list doesn't exist */
  if (!invalid_list || num_invalid < 1)
    return 1;

  /* change to lowercase */
  strcpy(tempname, newname);
  for (i = 0; tempname[i]; i++)
    tempname[i] = LOWER(tempname[i]);

  /* Does the desired name contain a string in the invalid list? */
  for (i = 0; i < num_invalid; i++)
    if (strstr(tempname, invalid_list[i]))
      return 0;

  return 1;
}


void Read_Invalid_List(void)
{
  FILE *fp;
  int i = 0;
  char string[80];

  if (!(fp = fopen(XNAME_FILE, "r"))) {
    perror("Unable to open invalid name file");
    return;
  }
  /* count how many records */
  while (fgets(string, 80, fp) != NULL && strlen(string) > 1)
    num_invalid++;

  rewind(fp);

  CREATE(invalid_list, namestring, num_invalid);

  for (i = 0; i < num_invalid; i++) {
    fgets(invalid_list[i], 80, fp);	/* read word */
    invalid_list[i][strlen(invalid_list[i]) - 1] = '\0'; /* cleave off \n */
  }

  fclose(fp);
}
