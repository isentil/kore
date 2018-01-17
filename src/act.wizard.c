/* ************************************************************************
*   File: act.wizard.c                                  Part of CircleMUD *
*  Usage: Player-level god commands and other goodies                     *
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
#include "spells.h"
#include "house.h"
#include "screen.h"
#include "auction.h"
#include "olc.h"
#include "buffer.h"

/*   external vars  */
extern FILE *player_fl;
extern struct room_data *world;
extern struct char_data *character_list;
extern struct obj_data *object_list;
extern struct descriptor_data *descriptor_list;
extern struct title_type titles[NUM_CLASSES][LVL_IMPL + 1];
extern int experience_table[LVL_IMPL + 1];
extern struct char_data *mob_proto;
extern struct index_data *mob_index;
extern struct obj_data *obj_proto;
extern struct index_data *obj_index;
extern struct int_app_type int_app[36];
extern struct wis_app_type wis_app[36];
extern struct zone_data *zone_table;
extern int top_of_zone_table;
extern int circle_restrict;
extern int top_of_world;
extern int top_of_mobt;
extern int top_of_objt;
extern int top_of_p_table;
extern byte saving_throws[NUM_CLASSES][NUM_SAVING_THROW_TYPES][LVL_IMPL+1];
extern int mob_defaults[LVL_IMPL + 1][8];

/* for objects */
extern char *item_types[];
extern char *wear_bits[];
extern char *extra_bits[];
extern char *drinks[];

/* for rooms */
extern char *dirs[];
extern char *room_bits[];
extern char *exit_bits[];
extern char *sector_types[];

/* for chars */
extern char *spells[];
extern char *equipment_types[];
extern char *affected_bits[];
extern char *affected2_bits[];
extern char *apply_types[];
extern char *pc_race_types[];
extern char *pc_class_types[];
/* HACKED to add clans */
extern char *clan_names[];
/* end of hack */
extern char *action_bits[];
extern char *player_bits[];
extern char *preference_bits[];
extern char *preference2_bits[];
extern char *position_types[];
extern char *connected_types[];
extern char *color_codes[];
extern char *mobformats[];
extern struct player_index_element *player_table;

int real_zone_by_thing(int number);
char *one_word(char *argument, char *first_arg);

ACMD(do_echo)
{
  skip_spaces(&argument);

  if (!*argument)
    send_to_char("Yes.. but what?\r\n", ch);
  else {
    if (subcmd == SCMD_EMOTE)
      sprintf(buf, "$n %s", argument);
    else
      strcpy(buf, argument);
    MOBTrigger = FALSE;
    act(buf, FALSE, ch, 0, 0, TO_ROOM);
    if (PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else {
      MOBTrigger = FALSE;
      act(buf, FALSE, ch, 0, 0, TO_CHAR);
    }
  }
}



ACMD(do_banner)
{
  skip_spaces(&argument);

  if (!*argument)
    send_to_char("Yes.. but banner what?\r\n", ch);
  else {
    sprintf(buf, "\x1B#3%s\r\n\x1B#4%s", argument, argument);
    MOBTrigger = FALSE;
    act(buf, FALSE, ch, 0, 0, TO_ROOM);
    if (PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else {
      MOBTrigger = FALSE;
      act(buf, FALSE, ch, 0, 0, TO_CHAR);
    }
  }
}



ACMD(do_send)
{
  struct char_data *vict;

  half_chop(argument, arg, buf);

  if (!*arg) {
    send_to_char("Send what to who?\r\n", ch);
    return;
  }
  if (!(vict = get_char_vis(ch, arg))) {
    send_to_char(NOPERSON, ch);
    return;
  }
  send_to_char(buf, vict);
  send_to_char("\r\n", vict);
  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char("Sent.\r\n", ch);
  else {
    sprintf(buf2, "You send '%s' to %s.\r\n", buf, GET_NAME(vict));
    send_to_char(buf2, ch);
  }
}



/* take a string, and return an rnum.. used for goto, at, etc.  -je 4/6/93 */
sh_int find_target_room(struct char_data * ch, char *rawroomstr)
{
  int tmp;
  sh_int location;
  struct char_data *target_mob;
  struct obj_data *target_obj;
  char roomstr[MAX_INPUT_LENGTH];

  one_argument(rawroomstr, roomstr);

  if (!*roomstr) {
    send_to_char("You must supply a room number or name.\r\n", ch);
    return NOWHERE;
  }
  if (isdigit(*roomstr) && !strchr(roomstr, '.')) {
    tmp = atoi(roomstr);
    if ((location = real_room(tmp)) < 0) {
      send_to_char("No room exists with that number.\r\n", ch);
      return NOWHERE;
    }
  } else if ((target_mob = get_char_vis(ch, roomstr)))
    location = target_mob->in_room;
  else if ((target_obj = get_obj_vis(ch, roomstr))) {
    if (target_obj->in_room != NOWHERE)
      location = target_obj->in_room;
    else {
      send_to_char("That object is not available.\r\n", ch);
      return NOWHERE;
    }
  } else {
    send_to_char("No such creature or object around.\r\n", ch);
    return NOWHERE;
  }

  /* a location has been found -- if you're < GRGOD, check restrictions. */
  if ((GET_LEVEL(ch) < LVL_GRGOD) && (!IS_NPC(ch))) {
    if (ROOM_FLAGGED(location, ROOM_GODROOM) || ZONE_FLAGGED(location,
ZONE_GODZONE)) {
      send_to_char("You are not godly enough to use that room!\r\n", ch);
      return NOWHERE;
    }
    if (ROOM_FLAGGED(location, ROOM_PRIVATE) &&
        world[location].people && world[location].people->next_in_room) {
      send_to_char("There's a private conversation going on in that room.\r\n",
ch);
      return NOWHERE;
    }
    if (ROOM_FLAGGED(location, ROOM_HOUSE) &&
        !House_can_enter(ch, world[location].number)) {
      send_to_char("That's private property -- no trespassing!\r\n", ch);
      return NOWHERE;
    }
  }
  return location;
}



ACMD(do_at)
{
  char command[MAX_INPUT_LENGTH];
  int location, original_loc;

  half_chop(argument, buf, command);
  if (!*buf) {
    send_to_char("You must supply a room number or a name.\r\n", ch);
    return;
  }

  if (!*command) {
    send_to_char("What do you want to do there?\r\n", ch);
    return;
  }

  if ((location = find_target_room(ch, buf)) < 0)
    return;

  /* a location has been found. */
  original_loc = ch->in_room;
  char_from_room(ch);
  char_to_room(ch, location);
  command_interpreter(ch, command);

  /* check if the char is still there */
  if (ch->in_room == location) {
    char_from_room(ch);
    char_to_room(ch, original_loc);
  }
}



ACMD(do_goto)
{
  sh_int location;

  if ((location = find_target_room(ch, argument)) < 0)
    return;

  if (POOFOUT(ch))
    sprintf(buf, "$n %s", POOFOUT(ch));
  else
    strcpy(buf, "$n steps into a portal and vanishes.");

  act(buf, TRUE, ch, 0, 0, TO_ROOM);
  if (HAS_PET(ch)) {
    if (GET_PET(ch)->in_room == ch->in_room) {
      char_from_room(GET_PET(ch));
      char_to_room(GET_PET(ch), location);
    }
  }
  char_from_room(ch);
  char_to_room(ch, location);

  if (POOFIN(ch))
    sprintf(buf, "$n %s", POOFIN(ch));
  else
    strcpy(buf, "$n emerges from a portal before you.");

  act(buf, TRUE, ch, 0, 0, TO_ROOM);
  look_at_room(ch, 0);
}



ACMD(do_trans)
{
  struct descriptor_data *i;
  struct char_data *victim;

  one_argument(argument, buf);
  if (!*buf)
    send_to_char("Whom do you wish to transfer?\r\n", ch);
  else if (str_cmp("all", buf)) {
    if (!(victim = get_char_vis(ch, buf)))
      send_to_char(NOPERSON, ch);
    else if (victim == ch)
      send_to_char("That doesn't make much sense, does it?\r\n", ch);
    else {
      if ((GET_LEVEL(ch) < GET_LEVEL(victim)) && !IS_NPC(victim)) {
        send_to_char("Go transfer someone your own size.\r\n", ch);
        return;
      }
      act("$n is consumed by a swirling black vortex.",
          FALSE, victim, 0, 0, TO_ROOM);
      char_from_room(victim);
      char_to_room(victim, ch->in_room);
      act("$n emerges from a swirling black vortex.",
          FALSE, victim, 0, 0, TO_ROOM);
      act("$n has transferred you!", FALSE, ch, 0, victim, TO_VICT);
      look_at_room(victim, 0);
    }
  } else {                      /* Trans All */
    if (GET_LEVEL(ch) < LVL_GRGOD) {
      send_to_char("I think not.\r\n", ch);
      return;
    }

    for (i = descriptor_list; i; i = i->next)
      if (!i->connected && i->character && i->character != ch) {
        victim = i->character;
        if (GET_LEVEL(victim) >= GET_LEVEL(ch))
          continue;
        act("$n disappears in a flash of light.", FALSE, victim, 0, 0, TO_ROOM);

        char_from_room(victim);
        char_to_room(victim, ch->in_room);
        act("$n arrives from a puff of smoke.", FALSE, victim, 0, 0, TO_ROOM);
        act("$n has transferred you!", FALSE, ch, 0, victim, TO_VICT);
        look_at_room(victim, 0);
      }
    send_to_char(OK, ch);
  }
}



ACMD(do_teleport)
{
  struct char_data *victim;
  sh_int target;

  two_arguments(argument, buf, buf2);

  if (!*buf)
    send_to_char("Whom do you wish to teleport?\r\n", ch);
  else if (!(victim = get_char_vis(ch, buf)))
    send_to_char(NOPERSON, ch);
  else if (victim == ch)
    send_to_char("Use 'goto' to teleport yourself.\r\n", ch);
  else if (GET_LEVEL(victim) >= GET_LEVEL(ch))
    send_to_char("Maybe you shouldn't do that.\r\n", ch);
  else if (!*buf2)
    send_to_char("Where do you wish to send this person?\r\n", ch);
  else if ((target = find_target_room(ch, buf2)) >= 0) {
    send_to_char(OK, ch);
    act("$n disappears in a puff of smoke.", FALSE, victim, 0, 0, TO_ROOM);
    char_from_room(victim);
    char_to_room(victim, target);
    act("$n arrives from a puff of smoke.", FALSE, victim, 0, 0, TO_ROOM);
    act("$n has teleported you!", FALSE, ch, 0, (char *) victim, TO_VICT);
    look_at_room(victim, 0);
  }
}



ACMD(do_vnum)
{
  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2 || (!is_abbrev(buf, "mob") && !is_abbrev(buf, "obj") &&
      !is_abbrev(buf, "room"))) {
    send_to_char("Usage: vnum { obj | mob } <name>\r\n", ch);
    return;
  }
  if (is_abbrev(buf, "mob"))
    if (!vnum_mobile(buf2, ch))
      send_to_char("No mobiles by that name.\r\n", ch);

  if (is_abbrev(buf, "obj"))
    if (!vnum_object(buf2, ch))
      send_to_char("No objects by that name.\r\n", ch);

  if (is_abbrev(buf, "room"))
    if (!vnum_room(buf2, ch))
      send_to_char("No rooms by that name.\r\n", ch);
}



void do_stat_room(struct char_data * ch)
{
  struct extra_descr_data *desc;
  struct room_data *rm = &world[ch->in_room];
  int i, found = 0;
  struct obj_data *j = 0;
  struct char_data *k = 0;

  sprintf(buf, "Room name: %s%s%s\r\n", CCROOMNAME(ch), rm->name,
          CCNRM(ch));
  send_to_char(buf, ch);

  sprinttype(rm->sector_type, sector_types, buf2);
  sprintf(buf, "Zone: [%3d], VNum: [%s%5d%s], RNum: [%5d], Type: %s\r\n",
          rm->zone, CCINFO(ch), rm->number, CCNRM(ch), ch->in_room, buf2);
  send_to_char(buf, ch);

  sprintbit((long) rm->room_flags, room_bits, buf2);
  sprintf(buf, "SpecProc: %s, Flags: %s\r\n",
          (rm->func == NULL) ? "None" : "Exists", buf2);
  send_to_char(buf, ch);

  send_to_char("Description:\r\n", ch);
  send_to_char(CCROOMDESC(ch), ch);
  if (rm->description)
    send_to_char(rm->description, ch);
  else
    send_to_char("  None.\r\n", ch);
  send_to_char(CCNRM(ch), ch);

  if (rm->ex_description) {
    sprintf(buf, "Extra descs:%s", CCROOMDESC(ch));
    for (desc = rm->ex_description; desc; desc = desc->next) {
      strcat(buf, " ");
      strcat(buf, desc->keyword);
    }
    strcat(buf, CCNRM(ch));
    send_to_char(strcat(buf, "\r\n"), ch);
  }
  sprintf(buf, "Chars present:%s", CCPLAYERS(ch));
  for (found = 0, k = rm->people; k; k = k->next_in_room) {
    if (!CAN_SEE(ch, k))
      continue;
    sprintf(buf2, "%s %s(%s)", found++ ? "," : "", GET_NAME(k),
            (!IS_NPC(k) ? "PC" : (!IS_MOB(k) ? "NPC" : "MOB")));
    strcat(buf, buf2);
    if (strlen(buf) >= 62) {
      if (k->next_in_room)
        send_to_char(strcat(buf, ",\r\n"), ch);
      else
        send_to_char(strcat(buf, "\r\n"), ch);
      *buf = found = 0;
    }
  }

  if (*buf)
    send_to_char(strcat(buf, "\r\n"), ch);
  send_to_char(CCNRM(ch), ch);

  if (rm->contents) {
    sprintf(buf, "Contents:%s", CCOBJECTS(ch));
    for (found = 0, j = rm->contents; j; j = j->next_content) {
      if (!CAN_SEE_OBJ(ch, j))
        continue;
      sprintf(buf2, "%s %s", found++ ? "," : "", j->short_description);
      strcat(buf, buf2);
      if (strlen(buf) >= 62) {
        if (j->next_content)
          send_to_char(strcat(buf, ",\r\n"), ch);
        else
          send_to_char(strcat(buf, "\r\n"), ch);
        *buf = found = 0;
      }
    }

    if (*buf)
      send_to_char(strcat(buf, "\r\n"), ch);
    send_to_char(CCNRM(ch), ch);
  }
  for (i = 0; i < NUM_OF_DIRS; i++) {
    if (rm->dir_option[i]) {
      if (rm->dir_option[i]->to_room == NOWHERE)
        sprintf(buf1, " %sNONE%s", CCEXITS(ch), CCNRM(ch));
      else
        sprintf(buf1, "%s%5d%s", CCEXITS(ch),
                world[rm->dir_option[i]->to_room].number, CCNRM(ch));
      sprintbit(rm->dir_option[i]->exit_info, exit_bits, buf2);
      sprintf(buf,
          "Exit %s%-5s%s:  To: [%s], Key: [%5d], Keywrd: %s, Type: %s\r\n ",
              CCEXITS(ch), dirs[i], CCNRM(ch), buf1, rm->dir_option[i]->key,
           rm->dir_option[i]->keyword ? rm->dir_option[i]->keyword : "None",
              buf2);
      send_to_char(buf, ch);
      if (rm->dir_option[i]->general_description)
        strcpy(buf, rm->dir_option[i]->general_description);
      else
        strcpy(buf, "  No exit description.\r\n");
      send_to_char(buf, ch);
    }
  }
}



void do_stat_object(struct char_data * ch, struct obj_data * j)
{
  int i, virtual, found;
  struct obj_data *j2;
  struct extra_descr_data *desc;

  virtual = GET_OBJ_VNUM(j);
  sprintf(buf, "Name: '%s%s%s', Aliases: %s\r\n", CCINFO(ch),
          ((j->short_description) ? j->short_description : "<None>"),
          CCNRM(ch), j->name);
  send_to_char(buf, ch);
  sprinttype(GET_OBJ_TYPE(j), item_types, buf1);
  if (GET_OBJ_RNUM(j) >= 0)
    strcpy(buf2, (obj_index[GET_OBJ_RNUM(j)].func ? "Exists" : "None"));
  else
    strcpy(buf2, "None");
  sprintf(buf, "VNum: [%s%5d%s], RNum: [%5d], Type: %s, SpecProc: %s\r\n",
   CCINFO(ch), virtual, CCNRM(ch), GET_OBJ_RNUM(j), buf1, buf2);
  send_to_char(buf, ch);
  sprintf(buf, "L-Des: %s\r\n", ((j->description) ? j->description : "None"));
  send_to_char(buf, ch);

  if (j->ex_description) {
    sprintf(buf, "Extra descs:%s", CCINFO(ch));
    for (desc = j->ex_description; desc; desc = desc->next) {
      strcat(buf, " ");
      strcat(buf, desc->keyword);
    }
    strcat(buf, CCNRM(ch));
    send_to_char(strcat(buf, "\r\n"), ch);
  }
  send_to_char("Can be worn on: ", ch);
  sprintbit(j->obj_flags.wear_flags, wear_bits, buf);
  strcat(buf, "\r\n");
  send_to_char(buf, ch);

  send_to_char("Set char bits : ", ch);

    sprintbit(j->obj_flags.bitvector, affected_bits, buf);
    strcat(buf, "\r\n");
    send_to_char(buf, ch);

    sprintbit(j->obj_flags.bitvector2, affected2_bits, buf);
    strcat(buf, "\r\n");
    send_to_char(buf, ch);

  send_to_char("Extra flags   : ", ch);
  sprintbit(GET_OBJ_EXTRA(j), extra_bits, buf);
  strcat(buf, "\r\n");
  send_to_char(buf, ch);

  sprintf(buf, "Weight: %d, Value: %d, Cost/day: %d, Timer: %d\r\n",
     GET_OBJ_WEIGHT(j), GET_OBJ_COST(j), GET_OBJ_RENT(j), GET_OBJ_TIMER(j));
  send_to_char(buf, ch);

  strcpy(buf, "In room: ");
  if (j->in_room == NOWHERE)
    strcat(buf, "Nowhere");
  else {
    sprintf(buf2, "%d", world[j->in_room].number);
    strcat(buf, buf2);
  }
  strcat(buf, ", In object: ");
  strcat(buf, j->in_obj ? j->in_obj->short_description : "None");
  strcat(buf, ", Carried by: ");
  strcat(buf, j->carried_by ? GET_NAME(j->carried_by) : "Nobody");
  strcat(buf, ", Worn by: ");
  strcat(buf, j->worn_by ? GET_NAME(j->worn_by) : "Nobody");
  strcat(buf, "\r\n");
  send_to_char(buf, ch);

  switch (GET_OBJ_TYPE(j)) {
  case ITEM_LIGHT:
    sprintf(buf, "Color: [%d], Type: [%d], Hours: [%d]",
            GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2));
    break;
  case ITEM_SCROLL:
  case ITEM_POTION:
  case ITEM_PILL:
    sprintf(buf, "Spells, level %d: %s, %s, %s",
            GET_OBJ_VAL(j, 0),
            GET_OBJ_VAL(j, 1) > 0 && GET_OBJ_VAL(j, 1) < NUM_SPELLS ? spells[GET_OBJ_VAL(j, 1)] : "null",
            GET_OBJ_VAL(j, 2) > 0 && GET_OBJ_VAL(j, 2) < NUM_SPELLS ? spells[GET_OBJ_VAL(j, 2)] : "null",
            GET_OBJ_VAL(j, 3) > 0 && GET_OBJ_VAL(j, 3) < NUM_SPELLS ? spells[GET_OBJ_VAL(j, 3)] : "null");
    break;
  case ITEM_WAND:
  case ITEM_STAFF:
    sprintf(buf, "Spell: %d, Mana: %d", GET_OBJ_VAL(j, 0),
            GET_OBJ_VAL(j, 1));
    break;
  case ITEM_FIREWEAPON:
  case ITEM_WEAPON:
    sprintf(buf, "Tohit: %d, Todam: %dd%d, Type: %d", GET_OBJ_VAL(j, 0),
            GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2), GET_OBJ_VAL(j, 3));
    break;
  case ITEM_MISSILE:
    sprintf(buf, "Tohit: %d, Todam: %d, Type: %d", GET_OBJ_VAL(j, 0),
            GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 3));
    break;
  case ITEM_ARMOR:
    sprintf(buf, "AC-apply: [%d]", GET_OBJ_VAL(j, 0));
    break;
  case ITEM_TRAP:
    sprintf(buf, "Spell: %d, - Hitpoints: %d",
            GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1));
    break;
  case ITEM_CONTAINER:
    sprintf(buf, "Max-contains: %d, Locktype: %d, Corpse: %s",
            GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1),
            GET_OBJ_VAL(j, 3) ? "Yes" : "No");
    break;
  case ITEM_DRINKCON:
  case ITEM_FOUNTAIN:
    sprinttype(GET_OBJ_VAL(j, 2), drinks, buf2);
    sprintf(buf, "Max-contains: %d, Contains: %d, Poisoned: %s, Liquid: %s",
            GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1),
            GET_OBJ_VAL(j, 3) ? "Yes" : "No", buf2);
    break;
  case ITEM_NOTE:
    sprintf(buf, "Tongue: %d", GET_OBJ_VAL(j, 0));
    break;
  case ITEM_KEY:
    sprintf(buf, "Keytype: %d", GET_OBJ_VAL(j, 0));
    break;
  case ITEM_FOOD:
    sprintf(buf, "Makes full: %d, Poisoned: %d",
            GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 3));
    break;
  default:
    sprintf(buf, "Values 0-3: [%d] [%d] [%d] [%d]",
            GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1),
            GET_OBJ_VAL(j, 2), GET_OBJ_VAL(j, 3));
    break;
  }
  send_to_char(buf, ch);
  send_to_char("\r\n", ch);

  /*
   * I deleted the "equipment status" code from here because it seemed
   * more or less useless and just takes up valuable screen space.
   */
 
  if (j->contains) {
    sprintf(buf, "Contents:%s", CCOBJECTS(ch));
    for (found = 0, j2 = j->contains; j2; j2 = j2->next_content) {
      sprintf(buf2, "%s %s", found++ ? "," : "", j2->short_description);
      strcat(buf, buf2);
      if (strlen(buf) >= 62) {
        if (j2->next_content)
          send_to_char(strcat(buf, ",\r\n"), ch);
        else
          send_to_char(strcat(buf, "\r\n"), ch);
        *buf = found = 0;
      }
    }

    if (*buf)
      send_to_char(strcat(buf, "\r\n"), ch);
    send_to_char(CCNRM(ch), ch);
  }
  found = 0;
  send_to_char("Affections:", ch);
  for (i = 0; i < MAX_OBJ_AFFECT; i++)
    if (j->affected[i].modifier) {
      sprinttype(j->affected[i].location, apply_types, buf2);
      sprintf(buf, "%s %+d to %s", found++ ? "," : "",
              j->affected[i].modifier, buf2);
      send_to_char(buf, ch);
    }
  if (!found)
    send_to_char(" None", ch);
  
  send_to_char("\r\n", ch);

  send_to_char("Objprogs: ", ch);
  if (j->progs) {
    send_to_char("yes\r\n", ch);
  } else {
    send_to_char("no\r\n", ch);
  }
}



void do_stat_character(struct char_data * ch, struct char_data * k)
{
  int i, i2, found = 0;
  struct obj_data *j;
  struct follow_type *fol;
  struct affected_type *aff;
  extern struct attack_hit_type attack_hit_text[];
  extern struct char_data *mob_proto;

  extern char * mprog_type_to_name(int type);
  extern void stat_shop(struct char_data *ch, struct char_data *k);
  extern void stat_pet(struct char_data *ch, struct char_data *k);

  switch (GET_SEX(k)) {
  case SEX_NEUTRAL:    strcpy(buf, "NEUTRAL-SEX");   break;
  case SEX_MALE:       strcpy(buf, "MALE");          break;
  case SEX_FEMALE:     strcpy(buf, "FEMALE");        break;
  default:             strcpy(buf, "ILLEGAL-SEX!!"); break;
  }
  if ((k->in_room >= top_of_world) || (k->in_room < 0))
    sprintf(buf2, " %s '%s'  IDNum: [%5ld], In room [unknown]\r\n",
	  (!IS_NPC(k) ? "PC" : (!IS_MOB(k) ? "NPC" : "MOB")),
	  GET_NAME(k), GET_IDNUM(k));
  else sprintf(buf2, " %s '%s'  IDNum: [%5ld], In room [%5d]\r\n",
	  (!IS_NPC(k) ? "PC" : (!IS_MOB(k) ? "NPC" : "MOB")),
	  GET_NAME(k), GET_IDNUM(k), world[k->in_room].number);
  send_to_char(strcat(buf, buf2), ch);
  if (IS_MOB(k)) {
    sprintf(buf, "Alias: %s, VNum: [%5d], RNum: [%5d], Format: [%s]\r\n",
	    k->player.name, GET_MOB_VNUM(k), GET_MOB_RNUM(k),
            mobformats[GET_MOBFORMAT(k)]);
    send_to_char(buf, ch);
  }
  sprintf(buf, "Title: %s\r\n", (k->player.title ? k->player.title : "<None>"));
  send_to_char(buf, ch);

  sprintf(buf, "L-Des: %s", (k->player.long_descr ? k->player.long_descr : "<None>\r\n"));
  send_to_char(buf, ch);

  if (IS_NPC(k)) {
    if (GET_RACE(k) == RACE_UNDEFINED) {
      strcpy(buf, "Monster Race: [");
      strcpy(buf2, "Undefined");
    } else {
      strcpy(buf, "Monster Race: [");
      sprinttype(GET_RACE(k), pc_race_types, buf2);
    }
  } else {
    strcpy(buf, "Race: [");
    sprinttype(GET_RACE(k), pc_race_types, buf2);
  }
  strcat(buf, buf2);
  send_to_char(buf, ch);

  if (IS_NPC(k)) {
    if (GET_CLASS(k) == CLASS_UNDEFINED) {
      strcpy(buf, "], Monster Class: [");
      strcpy(buf2, "Undefined");
    } else {
      strcpy(buf, "], Monster Class: [");
      sprinttype(GET_CLASS(k), pc_class_types, buf2);
    }
  } else {
    strcpy(buf, "], Class: [");
    sprinttype(k->player.class, pc_class_types, buf2);
  }
  strcat(buf, buf2);

  sprintf(buf2, "], Lev: [%s%2d%s]\r\nXP: [%s%7d%s], Align: [%4d]\r\n",
	  CCINFO(ch), GET_LEVEL(k), CCNRM(ch),
	  CCINFO(ch), GET_EXP(k), CCNRM(ch),
	  GET_ALIGNMENT(k));
  strcat(buf, buf2);
  send_to_char(buf, ch);

/* HACKED to add clan info to players stat */
  if (!IS_NPC(k)) {
    sprintf(buf, "Clan Number: [%d], Clan Name: [", (int) GET_CLAN(k));
    sprinttype(GET_CLAN(k), clan_names, buf2);
    strcat(buf, buf2);
    send_to_char(buf, ch);
    sprintf(buf, "], Clan Level: [%d]\r\n", GET_CLAN_LEVEL(k));
    send_to_char(buf, ch);
  }
/* end of hack */

  if (!IS_NPC(k)) {
    strcpy(buf1, (char *) asctime(localtime(&(k->player.time.birth))));
    strcpy(buf2, (char *) asctime(localtime(&(k->player.time.logon))));
    buf1[10] = buf2[10] = '\0';

    sprintf(buf, "Created: [%s], Last Logon: [%s], Played [%dh %dm], Age [%d]\r\n",
	    buf1, buf2, k->player.time.played / 3600,
	    ((k->player.time.played / 3600) % 60), age(k).year);
    send_to_char(buf, ch);

    sprintf(buf, "Hometown: [%d], Speaks: [%d/%d/%d], ",
	 k->player.hometown, GET_TALK(k, 0), GET_TALK(k, 1), GET_TALK(k, 2));
    send_to_char(buf, ch);
/* HACKED to remove practices */
    sprintf(buf, "("
        /*       "STL[%d]/" */	/* practices, removed */
                 "Int Apply[%d]/"
                 "Wis Apply[%d]"
                 ")",          
        /*       GET_PRACTICES(k), */
                 int_app[GET_INT(k)].learn,
                 wis_app[GET_WIS(k)].bonus);
    send_to_char(buf, ch);
/* end of hack */
/* HACKED to add oasis-olc */
    /* Display OLC zone for immorts */
    /* the olc zone is saved one larger than it is */
    if (GET_LEVEL(k) >= LVL_IMMORT)
      sprintf(buf, ", OLC: [%d]", GET_OLC_ZONE(k) - 1);
    strcat(buf, "\r\n");
    send_to_char(buf, ch);
/* end of hack */
  }
  sprintf(buf, "Str: [%s%d%s]  Int: [%s%d%s]  Wis: [%s%d%s]  "
	  "Dex: [%s%d%s]  Con: [%s%d%s]  Cha: [%s%d%s]\r\n",
/*
	  CCINFO(ch), GET_STR(k), GET_ADD(k), CCNRM(ch),
*/
	  CCINFO(ch), GET_STR(k), CCNRM(ch),
	  CCINFO(ch), GET_INT(k), CCNRM(ch),
	  CCINFO(ch), GET_WIS(k), CCNRM(ch),
	  CCINFO(ch), GET_DEX(k), CCNRM(ch),
	  CCINFO(ch), GET_CON(k), CCNRM(ch),
	  CCINFO(ch), GET_CHA(k), CCNRM(ch));
  send_to_char(buf, ch);

  sprintf(buf, "Hit p.:[%s%d/%d+%d%s]  Mana p.:[%s%d/%d+%d%s]  Move p.:[%s%d/%d+%d%s]\r\n",
	  CCINFO(ch), GET_HIT(k), GET_MAX_HIT(k), hit_gain(k), CCNRM(ch),
	  CCINFO(ch), GET_MANA(k), GET_MAX_MANA(k), mana_gain(k), CCNRM(ch),
	  CCINFO(ch), GET_MOVE(k), GET_MAX_MOVE(k), move_gain(k), CCNRM(ch));
  send_to_char(buf, ch);

  sprintf(buf, "Coins: [%9d], Bank: [%9d] (Total: %d)\r\n",
	  GET_GOLD(k), GET_BANK_GOLD(k), GET_GOLD(k) + GET_BANK_GOLD(k));
  send_to_char(buf, ch);

  if(!IS_NPC(k)) {
    sprintf(buf, "Quest points: [%6ld]  Quest obj: [%6d]  Quest mob: [%6d]\n\r",
	(k)->player_specials->saved.questpoints,
	(k)->player_specials->saved.questobj,
	(k)->player_specials->saved.questmob);
    send_to_char(buf, ch);
  }

  sprintf(buf, "AC: [%d/10], Hitroll: [%2d], Damroll: [%2d]\r\n",
	  GET_AC(k), k->points.hitroll, k->points.damroll);
  send_to_char(buf, ch);

  if (IS_NPC(k)) {
  sprintf(buf, "Saving throws: [%d/%d/%d/%d/%d]\r\n",
	    saving_throws[(int) CLASS_WARRIOR][1][(int) GET_LEVEL(k)] + GET_SAVE(k, 0),
	    saving_throws[(int) CLASS_WARRIOR][1][(int) GET_LEVEL(k)] + GET_SAVE(k, 1),
	    saving_throws[(int) CLASS_WARRIOR][2][(int) GET_LEVEL(k)] + GET_SAVE(k, 2),
	    saving_throws[(int) CLASS_WARRIOR][3][(int) GET_LEVEL(k)] + GET_SAVE(k, 3),
	    saving_throws[(int) CLASS_WARRIOR][4][(int) GET_LEVEL(k)] + GET_SAVE(k, 4));
  send_to_char(buf, ch);
  } else {
  sprintf(buf, "Saving throws: [%d/%d/%d/%d/%d]\r\n",
	  25 + GET_SAVE(k, 0),
	  25 + GET_SAVE(k, 1),
	  25 + GET_SAVE(k, 2),
	  25 + GET_SAVE(k, 3),
	  25 + GET_SAVE(k, 4));
  send_to_char(buf, ch);
  }


  sprinttype(GET_POS(k), position_types, buf2);
  sprintf(buf, "Pos: %s, Fighting: %s", buf2,
	  (FIGHTING(k) ? GET_NAME(FIGHTING(k)) : "Nobody"));

  if (IS_NPC(k)) {
    strcat(buf, ", Attack type: ");
    strcat(buf, attack_hit_text[k->mob_specials.attack_type].singular);
  }
  if (k->desc) {
    sprinttype(k->desc->connected, connected_types, buf2);
    strcat(buf, ", Connected: ");
    strcat(buf, buf2);
  }
  send_to_char(strcat(buf, "\r\n"), ch);

  strcpy(buf, "Default position: ");
  sprinttype((k->mob_specials.default_pos), position_types, buf2);
  strcat(buf, buf2);

  sprintf(buf2, ", Idle Timer (in tics) [%d]\r\n", k->char_specials.timer);
  strcat(buf, buf2);
  send_to_char(buf, ch);

  if (IS_NPC(k)) {
    sprintbit(MOB_FLAGS(k), action_bits, buf2);
    sprintf(buf, "NPC flags: %s%s%s\r\n", CCINFO(ch), buf2, CCNRM(ch));
    send_to_char(buf, ch);
  } else {
    sprintbit(PLR_FLAGS(k), player_bits, buf2);
    sprintf(buf, "PLR: %s%s%s\r\n", CCINFO(ch), buf2, CCNRM(ch));
    send_to_char(buf, ch);
    sprintbit(PRF_FLAGS(k), preference_bits, buf2);
    sprintf(buf, "PRF: %s%s%s\r\n", CCINFO(ch), buf2, CCNRM(ch));
    send_to_char(buf, ch);
    sprintbit(PRF2_FLAGS(k), preference2_bits, buf2);
    sprintf(buf, "PRF2: %s%s%s\r\n", CCINFO(ch), buf2, CCNRM(ch));
    send_to_char(buf, ch);
  }

  if (IS_MOB(k)) {
    sprintf(buf, "Mob Spec-Proc: %s, NPC Bare Hand Dam: %dd%d\r\n",
	    (mob_index[GET_MOB_RNUM(k)].func ? "Exists" : "None"),
	    k->mob_specials.damnodice, k->mob_specials.damsizedice);
    send_to_char(buf, ch);
  }
  sprintf(buf, "Carried: weight: %d, items: %d; ",
	  IS_CARRYING_W(k), IS_CARRYING_N(k));

  for (i = 0, j = k->carrying; j; j = j->next_content, i++);
  sprintf(buf, "%sItems in: inventory: %d, ", buf, i);

  for (i = 0, i2 = 0; i < GET_NUM_WEARS(k); i++)
    if (k->equipment[i])
      i2++;
  sprintf(buf2, "eq: %d\r\n", i2);
  strcat(buf, buf2);
  send_to_char(buf, ch);

  sprintf(buf, "Hunger: %d, Thirst: %d, Drunk: %d\r\n", 
	  GET_COND(k, FULL), GET_COND(k, THIRST), GET_COND(k, DRUNK));
  send_to_char(buf, ch);

  sprintf(buf, "Master is: %s, Followers are:",
	  ((k->master) ? GET_NAME(k->master) : "<none>"));

  for (fol = k->followers; fol; fol = fol->next) {
    sprintf(buf2, "%s %s", found++ ? "," : "", PERS(fol->follower, ch));
    strcat(buf, buf2);
    if (strlen(buf) >= 62) {
      if (fol->next)
	send_to_char(strcat(buf, ",\r\n"), ch);
      else
	send_to_char(strcat(buf, "\r\n"), ch);
      *buf = found = 0;
    }
  }

  if (*buf)
    send_to_char(strcat(buf, "\r\n"), ch);

  /* Show the bitvectors */
  sprintbit(AFF_FLAGS(k), affected_bits, buf2);
  sprintf(buf, "AFF: %s%s%s\r\n", CCALERT(ch), buf2, CCNRM(ch));
  send_to_char(buf, ch);

  sprintbit(AFF2_FLAGS(k), affected2_bits, buf2);
  sprintf(buf, "AFF2: %s%s%s\r\n", CCALERT(ch), buf2, CCNRM(ch));
  send_to_char(buf, ch);

  /* Routine to show what spells a char is affected by */
  if (k->affected) {
    for (aff = k->affected; aff; aff = aff->next) {
      *buf2 = '\0';
      sprintf(buf, "SPL: (%3dhr) %s%-21s%s ", aff->duration + 1,
	      CCINFO(ch), spells[aff->type], CCNRM(ch));
      if (aff->modifier) {
	sprintf(buf2, "%+d to %s", aff->modifier, apply_types[(int) aff->location]);
	strcat(buf, buf2);
      }
      if (aff->bitvector) {
	if (*buf2) strcat(buf, ", sets ");
	else strcat(buf, "sets ");
	sprintbit(aff->bitvector, affected_bits, buf2);
	strcat(buf, buf2);
      }
      if (aff->bitvector2) {
        if (*buf2) strcat(buf, ", sets ");
        else strcat(buf, "sets ");
        sprintbit(aff->bitvector2, affected2_bits, buf2);
        strcat(buf, buf2);
      }
      send_to_char(strcat(buf, "\r\n"), ch);
    }
  }

/* HACKED to show if the mob has a mobprog (use mpstat to see it) */
  if (IS_MOB(k)) {
    if (!(mob_index[k->nr].progtypes)) {
      send_to_char("Mobprogs: None.\n\r", ch);
    } else {
      send_to_char("Mobprogs: Exists!\n\r", ch);
    }
  } 
/* end of hack */
/* HACKED to show kills / deahts */
  if (IS_MOB(k)) {
    sprintf(buf, "Kills: %d   VKills: %d   VDeaths: %d\r\n",
      k->mob_specials.kills, mob_proto[k->nr].mob_specials.kills,
      mob_proto[k->nr].mob_specials.deaths
    );
    send_to_char(buf, ch);
  }
/* END of hcak */
  stat_shop(ch, k);
  stat_pet(ch, k);
}


ACMD(do_stat)
{
  struct char_data *victim = 0;
  struct obj_data *object = 0;
  struct char_file_u tmp_store;
  int tmp;

  half_chop(argument, buf1, buf2);

  if (!*buf1) {
    send_to_char("Stats on who or what?\r\n", ch);
    return;
  } else if (is_abbrev(buf1, "room")) {
    do_stat_room(ch);
  } else if (is_abbrev(buf1, "mob")) {
    if (!*buf2)
      send_to_char("Stats on which mobile?\r\n", ch);
    else {
      if ((victim = get_char_vis(ch, buf2)))
	do_stat_character(ch, victim);
      else
	send_to_char("No such mobile around.\r\n", ch);
    }
  } else if (is_abbrev(buf1, "player")) {
    if (!*buf2) {
      send_to_char("Stats on which player?\r\n", ch);
    } else {
      if ((victim = get_player_vis(ch, buf2)))
	do_stat_character(ch, victim);
      else
	send_to_char("No such player around.\r\n", ch);
    }
  } else if (is_abbrev(buf1, "file")) {
    if (!*buf2) {
      send_to_char("Stats on which player?\r\n", ch);
    } else {
      CREATE(victim, struct char_data, 1);
      clear_char(victim);
      if (load_char(buf2, &tmp_store) > -1) {
	store_to_char(&tmp_store, victim);
	if (GET_LEVEL(victim) > GET_LEVEL(ch))
	  send_to_char("Sorry, you can't do that.\r\n", ch);
	else
	  do_stat_character(ch, victim);
	free_char(victim);
      } else {
	send_to_char("There is no such player.\r\n", ch);
	free(victim);
      }
    }
  } else if (is_abbrev(buf1, "object")) {
    if (!*buf2)
      send_to_char("Stats on which object?\r\n", ch);
    else {
      if ((object = get_obj_vis(ch, buf2)))
	do_stat_object(ch, object);
      else
	send_to_char("No such object around.\r\n", ch);
    }
  } else {
    if ((object = get_object_in_equip_vis(ch, buf1, ch->equipment, &tmp)))
      do_stat_object(ch, object);
    else if ((object = get_obj_in_list_vis(ch, buf1, ch->carrying)))
      do_stat_object(ch, object);
    else if ((victim = get_char_room_vis(ch, buf1)))
      do_stat_character(ch, victim);
    else if ((object = get_obj_in_list_vis(ch, buf1, world[ch->in_room].contents)))
      do_stat_object(ch, object);
    else if ((victim = get_char_vis(ch, buf1)))
      do_stat_character(ch, victim);
    else if ((object = get_obj_vis(ch, buf1)))
      do_stat_object(ch, object);
    else
      send_to_char("Nothing around by that name.\r\n", ch);
  }
}


ACMD(do_shutdown)
{
  extern int circle_shutdown, circle_reboot;

  if (subcmd != SCMD_SHUTDOWN) {
    send_to_char("If you want to shut something down, say so!\r\n", ch);
    return;
  }
  one_argument(argument, arg);

  if (!*arg) {
    sprintf(buf, "(GC) Shutdown by %s.", GET_NAME(ch));
    log(buf);
    send_to_all("Shutting down.\r\n");
    circle_shutdown = 1;
  } else if (!str_cmp(arg, "reboot")) {
    sprintf(buf, "(GC) Reboot by %s.", GET_NAME(ch));
    log(buf);
    send_to_all("Rebooting.. come back in a minute or two.\r\n");
    touch("../.fastboot");
    circle_shutdown = circle_reboot = 1;
#ifndef DRAGONSLAVE
  } else if (!str_cmp(arg, "die") || !str_cmp(arg, "dragonslave")) {
    sprintf(buf, "(GC) Shutdown by %s.", GET_NAME(ch));
    log(buf);
    if (!str_cmp(arg, "dragonslave")) {
      send_to_all(
"You are rocked by a massive explosion!\r\n"
"Fire and sulphur fill the air as a wave of heat washes over you...\r\n"
"As your legs crumble beneath you and you sink to the ground, a feeling of\r\n"
"dread fills your entire being, as if the most important thing in your life\r\n"
"has been torn away from you...\r\n\r\n\r\n"
      );
    } else {
      send_to_all("Shutting down for maintenance.\r\n");
    }
    touch("../.killscript");
    circle_shutdown = 1;
#else
  } else if (!str_cmp(arg, "die")) {
    sprintf(buf, "(GC) Shutdown by %s.", GET_NAME(ch));
    log(buf);
    send_to_all("Shutting down for maintenance.\r\n");
    touch("../.killscript");
    circle_shutdown = 1;
#endif
  } else if (!str_cmp(arg, "pause")) {
    sprintf(buf, "(GC) Shutdown by %s.", GET_NAME(ch));
    log(buf);
    send_to_all("Shutting down for maintenance.\r\n");
    touch("../pause");
    circle_shutdown = 1;
  } else
    send_to_char("Unknown shutdown option.\r\n", ch);
}


void stop_snooping(struct char_data * ch)
{
  if (!ch->desc->snooping)
    send_to_char("You aren't snooping anyone.\r\n", ch);
  else {
    send_to_char("You stop snooping.\r\n", ch);
    ch->desc->snooping->snoop_by = NULL;
    ch->desc->snooping = NULL;
  }
}


ACMD(do_snoop)
{
  struct char_data *victim, *tch;

  if (!ch->desc)
    return;

  one_argument(argument, arg);

  if (!*arg)
    stop_snooping(ch);
  else if (!(victim = get_char_vis(ch, arg)))
    send_to_char("No such person around.\r\n", ch);
  else if (!victim->desc)
    send_to_char("There's no link.. nothing to snoop.\r\n", ch);
  else if (victim == ch)
    stop_snooping(ch);
  else if (victim->desc->snoop_by)
    send_to_char("Busy already. \r\n", ch);
  else if (victim->desc->snooping == ch->desc)
    send_to_char("Don't be stupid.\r\n", ch);
  else {
    if (victim->desc->original)
      tch = victim->desc->original;
    else
      tch = victim;

    if (GET_LEVEL(tch) >= GET_LEVEL(ch)) {
      send_to_char("You can't.\r\n", ch);
      return;
    }
    send_to_char(OK, ch);

    if (ch->desc->snooping)
      ch->desc->snooping->snoop_by = NULL;

    ch->desc->snooping = victim->desc;
    victim->desc->snoop_by = ch->desc;
  }
}



ACMD(do_jar)
{
  struct char_data *victim;

  one_argument(argument, arg);

  if (ch->desc->original)
    send_to_char("You're already switched.\r\n", ch);
  else if (!*arg)
    send_to_char("Switch with who?\r\n", ch);
  else if (!(victim = get_char_vis(ch, arg)))
    send_to_char("No such character.\r\n", ch);
  else if (ch == victim)
    send_to_char("Hee hee... we are jolly funny today, eh?\r\n", ch);
  else if (victim->desc)
    send_to_char("You can't do that, the body is already in use!\r\n", ch);
  else if ((GET_LEVEL(ch) < LVL_IMPL) && !IS_NPC(victim))
    send_to_char("You aren't holy enough to use a mortal's body.\r\n", ch);
  else {
    send_to_char(OK, ch);

/* PETS */
    if (HAS_PET(ch)) {
      GET_OWNER_DESC(GET_PET(ch)) = NULL;
      if (ch->desc) GET_PET(ch->desc) = NULL;
    }
/* END of PETS */

    ch->desc->character = victim;
    ch->desc->original = ch;

    victim->desc = ch->desc;
    ch->desc = NULL;
  }
}


ACMD(do_return)
{
  if (ch->desc && ch->desc->original) {
    send_to_char("You return to your original body.\r\n", ch);

    /* JE 2/22/95 */
    /* if someone switched into your original body, disconnect them */
    if (ch->desc->original->desc)
      close_socket(ch->desc->original->desc);

    ch->desc->character = ch->desc->original;
    ch->desc->original = NULL;
    
/* PETS */
    if (HAS_PET(ch->desc->character)) {
      GET_PET(ch->desc) = GET_PET(ch->desc->character);
      GET_OWNER_DESC(GET_PET(ch->desc)) = ch->desc;
    }
/* END of PETS */

    ch->desc->character->desc = ch->desc;
    ch->desc = NULL;
  }
}



ACMD(do_load)
{
  struct char_data *mob;
  struct obj_data *obj;
  int number, r_num;


  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2 || !isdigit(*buf2)) {
    send_to_char("Usage: load { obj | mob } <number>\r\n", ch);
    return;
  }
  if ((number = atoi(buf2)) < 0) {
    send_to_char("A NEGATIVE number??\r\n", ch);
    return;
  }
/* HACKED to let people load their own zones eq and mobs */
  if (GET_LEVEL(ch) < LVL_GOD) {
    if ((number / 100) != (GET_OLC_ZONE(ch) - 1)) {
      send_to_char("You can only load things from your own OLC zone.\r\n", ch);
      return;
    }
  }
/* end of hack */
  if (is_abbrev(buf, "mob")) {
    if ((r_num = real_mobile(number)) < 0) {
      send_to_char("There is no monster with that number.\r\n", ch);
      return;
    }
    mob = read_mobile(r_num, REAL);
    char_to_room(mob, ch->in_room);

    act("$n makes a quaint, magical gesture with one hand.", TRUE, ch,
	0, 0, TO_ROOM);
    act("$n has created $N!", FALSE, ch, 0, mob, TO_ROOM);
    act("You create $N.", FALSE, ch, 0, mob, TO_CHAR);
  } else if (is_abbrev(buf, "obj")) {
    if ((r_num = real_object(number)) < 0) {
      send_to_char("There is no object with that number.\r\n", ch);
      return;
    }
    obj = read_object(r_num, REAL);
    obj_to_char(obj, ch);
    act("$n makes a strange magical gesture.", TRUE, ch, 0, 0, TO_ROOM);
    act("$n has created $p!", FALSE, ch, obj, 0, TO_ROOM);
    act("You create $p.", FALSE, ch, obj, 0, TO_CHAR);
    sprintf(buf3, "(GC) %s has loaded [%5d] %s.", GET_NAME(ch),
        number, obj->short_description);
    mudlog(buf3, CMP, MAX(LVL_GRGOD, GET_INVIS_LEV(ch)), TRUE);
  } else
    send_to_char("That'll have to be either 'obj' or 'mob'.\r\n", ch);
}



ACMD(do_vstat)
{
  struct char_data *mob;
  struct obj_data *obj;
  int number, r_num;

  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2 || !isdigit(*buf2)) {
    send_to_char("Usage: vstat { obj | mob } <number>\r\n", ch);
    return;
  }
  if ((number = atoi(buf2)) < 0) {
    send_to_char("A NEGATIVE number??\r\n", ch);
    return;
  }
  if (is_abbrev(buf, "mob")) {
    if ((r_num = real_mobile(number)) < 0) {
      send_to_char("There is no monster with that number.\r\n", ch);
      return;
    }
    mob = read_mobile(r_num, REAL);
    char_to_room(mob, 0);
    do_stat_character(ch, mob);
    extract_char(mob);
  } else if (is_abbrev(buf, "obj")) {
    if ((r_num = real_object(number)) < 0) {
      send_to_char("There is no object with that number.\r\n", ch);
      return;
    }
    obj = read_object(r_num, REAL);
    do_stat_object(ch, obj);
    extract_obj(obj);
  } else
    send_to_char("That'll have to be either 'obj' or 'mob'.\r\n", ch);
}



/* purge every mob and obj in the room with ch */
void perform_purge_room(struct char_data * ch)
{
  struct char_data *vict, *next_v;
  struct obj_data *obj, *next_o;


  act("$n gestures... You are surrounded by scorching flames!",
      FALSE, ch, 0, 0, TO_ROOM);
  send_to_room("The world seems a little cleaner.\r\n", ch->in_room);

  for (vict = world[ch->in_room].people; vict; vict = next_v) {
    next_v = vict->next_in_room;
    if (IS_NPC(vict))
      extract_char(vict);
  }

  for (obj = world[ch->in_room].contents; obj; obj = next_o) {
    next_o = obj->next_content;
    extract_obj(obj);
  }
}



void perform_purge_player(struct char_data * ch, struct char_data * vict)
{
  sprintf(buf, "(GC) %s has purged %s.", GET_NAME(ch), GET_NAME(vict));
  mudlog(buf, BRF, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);

  if (vict->desc) {
    close_socket(vict->desc);
    vict->desc = NULL;
  }

  extract_char(vict);
}



/* clean a room of all mobiles and objects */
ACMD(do_purge)
{
  struct char_data *vict;
  struct obj_data *obj;


  one_argument(argument, buf);

  if (!*buf) {
    /* purge everything in the room */
    if (GET_LEVEL(ch) < LVL_GOD) {
      if (zone_table[world[ch->in_room].zone].number != GET_OLC_ZONE(ch) - 1) {
        send_to_char("You can only purge in your own zone.\r\n", ch);
        return;
      }
    }
    perform_purge_room(ch);
  } else {
    /* possibly purge a character (mob or player) or an object */
    if ((vict = get_char_room_vis(ch, buf))) {
      if (!IS_NPC(vict)) {
        if (GET_LEVEL(ch) < LVL_GOD) {
          send_to_char("You are not high enough level to purge players.\r\n",
              ch);
          return;
        }
        if (GET_LEVEL(ch) <= GET_LEVEL(vict)) {
          send_to_char("You can only purge players"
              " that are lower level than you.\r\n", ch);
          return;
        }
        perform_purge_player(ch, vict);
      } else {
        /* purge a mob just standing there */
        if (GET_LEVEL(ch) < LVL_GOD) {
          if (zone_table[world[ch->in_room].zone].number !=
              GET_OLC_ZONE(ch) -1) {
            send_to_char("You can only purge in your own zone.\r\n", ch);
            return;
          }
        }
        extract_char(vict);
      }
    } else if ((obj = get_obj_in_list_vis(ch, buf, world[ch->in_room].contents))) {
      /* just an object, purge it */
      if (GET_LEVEL(ch) < LVL_GOD) {
        if (zone_table[world[ch->in_room].zone].number != GET_OLC_ZONE(ch) -1) {
          send_to_char("You can only purge in your own zone.\r\n", ch);
          return;
        }
      }
      act("You purge $p.", FALSE, ch, obj, 0, TO_CHAR);
      act("$n purges $p.", TRUE, ch, obj, 0, TO_ROOM);
      extract_obj(obj);
    } else {
      send_to_char("Nothing here by that name.\r\n", ch);
      return;
    }

  } /* else *buf - there was something in the room */

  send_to_char(OK, ch);
}



ACMD(do_sacrifice)
{
  struct obj_data *obj;


  one_argument(argument, buf);

  if (!*buf) {
    send_to_char("What do you want to sacrifice?\r\n", ch);
    return;
  }

  if ((obj = get_obj_in_list_vis(ch, buf, world[ch->in_room].contents))) {
    /* only allow players to sacrifice corpses */
    if ((GET_OBJ_TYPE(obj) == ITEM_CONTAINER) && GET_OBJ_VAL(obj, 3)) {
      /* its a corpse, but is it a player corpse? */
      if (GET_OBJ_VAL(obj, 2) == -2) {
        send_to_char("You can't sacrifice that, that's a player corpse!\r\n",
            ch);
        return;
      }
    } else {
      send_to_char("You can't sacrifice that, that's not a corpse!\r\n", ch);
      return;
    }
    act("You sacrifice $p.\r\n"
        "It disappears in a gout of green flame!", FALSE, ch, obj, 0, TO_CHAR);
    act("$n sacrifices $p.\r\n"
        "It disappears in a gout of green flame!", TRUE, ch, obj, 0, TO_ROOM);
    extract_obj(obj);
  } else {
    send_to_char("Nothing here by that name.\r\n", ch);
    return;
  }
}



ACMD(do_bury)
{
  send_to_char("Sorry, bury has been temporarily disabled; try sacrifice.\r\n", ch);
}



ACMD(do_advance)
{
  struct char_data *victim;
  char *name = arg, *level = buf2;
  int newlevel;
  int oldlevel;

  void do_start(struct char_data *ch);
  void gain_exp(struct char_data * ch, int gain);


  two_arguments(argument, name, level);

  if (*name) {
    if (!(victim = get_char_vis(ch, name))) {
      send_to_char("That player is not here.\r\n", ch);
      return;
    }
  } else {
    send_to_char("Advance who?\r\n", ch);
    return;
  }

  if (GET_LEVEL(ch) <= GET_LEVEL(victim)) {
    send_to_char("Maybe that's not such a great idea.\r\n", ch);
    return;
  }

  if (IS_NPC(victim)) {
    send_to_char("NO!  Not on NPC's.\r\n", ch);
    return;
  }

  if (!*level || (newlevel = atoi(level)) <= 0) {
    send_to_char("That's not a level!\r\n", ch);
    return;
  }
  if (newlevel > LVL_IMPL) {
    sprintf(buf, "%d is the highest possible level.\r\n", LVL_IMPL);
    send_to_char(buf, ch);
    return;
  }
  if (newlevel > GET_LEVEL(ch)) {
    send_to_char("Yeah, right.\r\n", ch);
    return;
  }
  if (newlevel == GET_LEVEL(victim)) {
    send_to_char("They are already that level.\r\n", ch);
    return;
  }
  oldlevel = GET_LEVEL(victim);
  if (newlevel < GET_LEVEL(victim)) {
    do_start(victim);
    GET_LEVEL(victim) = newlevel;
  } else {
    act("$n makes some strange gestures.\r\n"
	"A strange feeling comes upon you,\r\n"
	"Like a giant hand, light comes down\r\n"
	"from above, grabbing your body, that\r\n"
	"begins to pulse with colored lights\r\n"
	"from inside.\r\n\r\n"
	"Your head seems to be filled with demons\r\n"
	"from another plane as your body dissolves\r\n"
	"to the elements of time and space itself.\r\n"
	"Suddenly a silent explosion of light\r\n"
	"snaps you back to reality.\r\n\r\n"
	"You feel slightly different.", FALSE, ch, 0, victim, TO_VICT);
  }

  send_to_char(OK, ch);

  if (newlevel < oldlevel) {
    snprintf(buf, sizeof(buf), "(GC) %s demoted %s from level %d to %d.", GET_NAME(ch), GET_NAME(victim), oldlevel, newlevel);
    log(buf);
  } else {
    snprintf(buf, sizeof(buf), "(GC) %s has advanced %s to level %d (from %d)", GET_NAME(ch), GET_NAME(victim), newlevel, oldlevel);
    log(buf);
  }
  if (oldlevel >= LVL_IMMORT && newlevel < LVL_IMMORT) {
    REMOVE_BIT(PRF_FLAGS(victim), PRF_LOG1 | PRF_LOG2);
    REMOVE_BIT(PRF_FLAGS(victim), PRF_NOHASSLE | PRF_HOLYLIGHT);
    check_autowiz(victim);
  }
  gain_exp_regardless(victim,
	 (titles[(int) GET_CLASS(victim)][newlevel].exp) - GET_EXP(victim));
  save_char(victim, NOWHERE);
}



ACMD(do_restore)
{
  struct descriptor_data *i, *next_desc;
  struct char_data *vict, *next_restore;
  int j;

  one_argument(argument, buf);
  if (!*buf)
    send_to_char("Whom do you wish to restore?\r\n", ch);
  else if ((GET_LEVEL(ch) < LVL_GRGOD) || (str_cmp("all", buf) && str_cmp("room", buf))) {

    if (!(vict = get_char_vis(ch, buf))) {
	send_to_char(NOPERSON, ch);
        return;
    }

    GET_HIT(vict) = GET_MAX_HIT(vict);
    GET_MANA(vict) = GET_MAX_MANA(vict);
    GET_MOVE(vict) = GET_MAX_MOVE(vict);

    if ((GET_LEVEL(ch) >= LVL_GRGOD) && (GET_LEVEL(vict) >= LVL_IMMORT)) {
      for (j = 1; j <= MAX_SKILLS; j++)
	SET_SKILL(vict, j, 100);

      if (GET_LEVEL(vict) >= LVL_GRGOD) {
	vict->real_abils.str_add = 100;
	vict->real_abils.intel = 25;
	vict->real_abils.wis = 25;
	vict->real_abils.dex = 25;
	vict->real_abils.str = 25;
	vict->real_abils.con = 25;
	vict->real_abils.cha = 25;
      }
      vict->aff_abils = vict->real_abils;
    }
    update_pos(vict);
    send_to_char(OK, ch);
    act("You have been fully healed by $N!", FALSE, vict, 0, ch, TO_CHAR);
  } else if (!str_cmp("room", buf)) {
    send_to_char(OK, ch);

    for (vict = world[ch->in_room].people; vict; vict = next_restore) {
      next_restore = vict->next_in_room;
    GET_HIT(vict) = GET_MAX_HIT(vict);
    GET_MANA(vict) = GET_MAX_MANA(vict);
    GET_MOVE(vict) = GET_MAX_MOVE(vict);
    act("You have been fully healed by $N!", FALSE, vict, 0, ch, TO_CHAR);
    }
  } else { /* restore all */
    send_to_char(OK, ch);

    for (i = descriptor_list; i; i = next_desc) {
	next_desc = i->next;

	if (i->connected || !(vict = i->character))
	    continue;

    GET_HIT(vict) = GET_MAX_HIT(vict);
    GET_MANA(vict) = GET_MAX_MANA(vict);
    GET_MOVE(vict) = GET_MAX_MOVE(vict);
    act("You have been fully healed by $N!", FALSE, vict, 0, ch, TO_CHAR);
    }
  }
}


ACMD(do_invis)
{
  int level;

  one_argument(argument, arg);
  if (!*arg) {
    if (GET_INVIS_LEV(ch) > 0) {
      GET_INVIS_LEV(ch) = 0;
      act("$n fades into existence.", FALSE, ch, 0, 0, TO_ROOM );
      sprintf(buf, "You fade into existence.\r\n");
    } else {
      sprintf(buf, "Your invisibility level is %d.\r\n", GET_LEVEL(ch));
      act("$n slowly fades out of existence.", FALSE, ch, 0, 0, TO_ROOM );
      GET_INVIS_LEV(ch) = GET_LEVEL(ch);
    }
  } else {
    level = atoi(arg);
    if (level > GET_LEVEL(ch)) {
      send_to_char("You can't go invisible above your own level.\r\n", ch);
      return;
    } else if (level < 1) {
      GET_INVIS_LEV(ch) = 0;
      sprintf(buf, "You are now fully visible.\r\n");
    } else {
      GET_INVIS_LEV(ch) = level;
      sprintf(buf, "Your invisibility level is now %d.\r\n", level);
    }
  }
  send_to_char(buf, ch);
}


ACMD(do_gecho)
{
  struct descriptor_data *pt;

  skip_spaces(&argument);

  if (!*argument)
    send_to_char("That must be a mistake...\r\n", ch);
  else {
    sprintf(buf, "%s\r\n", argument);
    for (pt = descriptor_list; pt; pt = pt->next)
      if (!pt->connected && pt->character && pt->character != ch)
	send_to_char(buf, pt->character);
    if (PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else
      send_to_char(buf, ch);
  }
}


ACMD(do_system)
{
  struct descriptor_data *pt;

  skip_spaces(&argument);

  if (!*argument)
    send_to_char("Send what message to the system?\r\n", ch);
  else {
    sprintf(buf, "^Y[System: %s]^n\r\n", argument);
    for (pt = descriptor_list; pt; pt = pt->next)
      if (!pt->connected && pt->character && pt->character != ch)
	send_to_char(buf, pt->character);
    if (PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else
      send_to_char(buf, ch);
  }
}


ACMD(do_gbanner)
{
  struct descriptor_data *pt;

  skip_spaces(&argument);

  if (!*argument)
    send_to_char("That must be a mistake...\r\n", ch);
  else {
    sprintf(buf, "\x1B#3%s\r\n\x1B#4%s\r\n", argument, argument);
    for (pt = descriptor_list; pt; pt = pt->next)
      if (!pt->connected && pt->character && pt->character != ch)
        send_to_char(buf, pt->character);
    if (PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else
      send_to_char(buf, ch);
  }
}



ACMD(do_poofset)
{
  char **msg;

  if (IS_NPC(ch)) {
    send_to_char("Mobs can't do that!\r\n", ch);
    return;
  }
  
  if (PLR_FLAGGED(ch, PLR_NOTITLE)) {
    send_to_char("Hmm...apparantly you've abused your priveleges. Sorry.\r\n", ch);
    return;
  }
  
  switch (subcmd) {
  case SCMD_POOFIN:    msg = &(POOFIN(ch));    break;
  case SCMD_POOFOUT:   msg = &(POOFOUT(ch));   break;
  default:    return;    break;
  }

  skip_spaces(&argument);

  if (*msg)
    free(*msg);

  if (!*argument)
    *msg = NULL;
  else
    *msg = strdup(argument);

  send_to_char(OK, ch);
}



ACMD(do_dc)
{
  struct descriptor_data *d;
  int num_to_dc;

  one_argument(argument, arg);
  if (!(num_to_dc = atoi(arg))) {
    send_to_char("Usage: DC <connection number> (type USERS for a list)\r\n", ch);
    return;
  }
  for (d = descriptor_list; d && d->desc_num != num_to_dc; d = d->next);

  if (!d) {
    send_to_char("No such connection.\r\n", ch);
    return;
  }
  if (d->character && GET_LEVEL(d->character) >= GET_LEVEL(ch)) {
    send_to_char("Umm.. maybe that's not such a good idea...\r\n", ch);
    return;
  }
  close_socket(d);
  sprintf(buf, "Connection #%d closed.\r\n", num_to_dc);
  send_to_char(buf, ch);
  sprintf(buf, "(GC) Connection closed by %s.", GET_NAME(ch));
  log(buf);
}



ACMD(do_wizlock)
{
  int value;
  char *when;

  one_argument(argument, arg);
  if (*arg) {
    value = atoi(arg);
    if (value < 0 || value > GET_LEVEL(ch)) {
      send_to_char("Invalid wizlock value.\r\n", ch);
      return;
    }
    circle_restrict = value;
    when = "now";
  } else
    when = "currently";

  switch (circle_restrict) {
  case 0:
    sprintf(buf, "The game is %s completely open.\r\n", when);
    break;
  case 1:
    sprintf(buf, "The game is %s closed to new players.\r\n", when);
    break;
  default:
    sprintf(buf, "Only level %d and above may enter the game %s.\r\n",
	    circle_restrict, when);
    break;
  }
  send_to_char(buf, ch);
}


ACMD(do_date)
{
  char *tmstr;
  time_t mytime;
  int d, h, m;
  extern time_t boot_time;

  if (subcmd == SCMD_DATE)
    mytime = time(0);
  else
    mytime = boot_time;

  tmstr = (char *) asctime(localtime(&mytime));
  *(tmstr + strlen(tmstr) - 1) = '\0';

  if (subcmd == SCMD_DATE)
    sprintf(buf, "Current machine time: %s\r\n", tmstr);
  else {
    mytime = time(0) - boot_time;
    d = mytime / 86400;
    h = (mytime / 3600) % 24;
    m = (mytime / 60) % 60;

    sprintf(buf, "Up since %s: %d day%s, %d:%02d\r\n", tmstr, d,
	    ((d == 1) ? "" : "s"), h, m);
  }

  send_to_char(buf, ch);
}



/* hacked to let regular players enjoy */
/*
 * to Greb: what i did in here was set it up so that it really reads in
 * the vict all the way by doing store_to_char() and storing it in vict
 * This way we can use the normal macros like GET_LEVEL(vict) and stuff
 * and I think that'll make it a lot easier for you to modify
 * Unfortunately there was no easy way to do last_logon and host info
 * except with victdata.last_logon... etc.
 */
ACMD(do_last)
{
  struct char_file_u victdata;
  struct char_data *vict;
  extern char *race_abbrevs[];
  extern char *class_abbrevs[];
  extern char *immort_abbrevs[];
  int need_to_free_memory = 0;


  one_argument(argument, arg);
  if (!*arg) {
    send_to_char("For whom do you wish to search?\r\n", ch);
    return;
  }

  if (load_char(arg, &victdata) < 0) {
    send_to_char("There is no such player.\r\n", ch);
    return;
  } else {
    vict = get_player_vis(ch, arg);
    if (!vict) {
      CREATE(vict, struct char_data, 1);
      clear_char(vict);
      store_to_char(&victdata, vict);
      need_to_free_memory = 1;
    }
  }

  if (PRF2_FLAGGED(vict, PRF2_ANONYMOUS) && GET_LEVEL(ch) < LVL_IMMORT) {
    send_to_char("They are anonymous.\r\n", ch);
    if (need_to_free_memory)
      free_char(vict);
    return;
  }

  if ((GET_LEVEL(vict) >= LVL_IMMORT) && (GET_LEVEL(ch) < GET_LEVEL(vict))) {
    send_to_char("You are not sufficiently godly for that!\r\n", ch);
    if (need_to_free_memory)
      free_char(vict);
    return;
  }

  if (GET_LEVEL(ch) < LVL_IMMORT) {
    if (GET_LEVEL(vict) >= LVL_IMMORT) {
      sprintf(buf, "[%2d %s %s] %-12s : %-20s\r\n",
              GET_LEVEL(vict),
              race_abbrevs[GET_RACE(vict)],
              immort_abbrevs[GET_LEVEL(vict) - LVL_IMMORT],
              GET_NAME(vict),
              ctime(&victdata.last_logon));
    } else {
      sprintf(buf, "[%2d %s %s] %-12s : %-20s\r\n",
              GET_LEVEL(vict),
              race_abbrevs[GET_RACE(vict)],
              class_abbrevs[(int) GET_CLASS(vict)],
              GET_NAME(vict),
              ctime(&victdata.last_logon));
    }
  } else {
    if (GET_LEVEL(vict) >= LVL_IMMORT) {
      sprintf(buf, "[%5ld] [%2d %s %s] %-12s : %-18s : %-20s\r\n",
              GET_IDNUM(vict), 
              GET_LEVEL(vict),
              race_abbrevs[GET_RACE(vict)],
              immort_abbrevs[GET_LEVEL(vict) - LVL_IMMORT],
              GET_NAME(vict), 
              victdata.host,
              ctime(&victdata.last_logon));
    } else {
      sprintf(buf, "[%5ld] [%2d %s %s] %-12s : %-18s : %-20s\r\n",
              GET_IDNUM(vict),
              GET_LEVEL(vict),
              race_abbrevs[GET_RACE(vict)],
              class_abbrevs[(int) GET_CLASS(vict)],
              GET_NAME(vict),
              victdata.host,
              ctime(&victdata.last_logon));
    }
  }
  send_to_char(buf, ch);
  if (need_to_free_memory)
    free_char(vict);
}



#if 0
ACMD(do_finger)
{
  struct char_file_u victdata;
  struct char_data *vict;
  int need_to_free_memory = 0;
  int r_num;
  struct char_data *mob;

  one_argument(argument, arg);
  if (!*arg) {
    send_to_char("Whom do you wish to finger?\r\n", ch);
    return;
  }

  if (load_char(arg, &victdata) < 0) {
    send_to_char("There is no such player.\r\n", ch);
    return;
  } else {
    vict = get_player_vis(ch, arg);
    if (!vict) {
      CREATE(vict, struct char_data, 1);
      clear_char(vict);
      store_to_char(&victdata, vict);
      need_to_free_memory = 1;
    }
  }

  if (PRF2_FLAGGED(vict, PRF2_ANONYMOUS) && GET_LEVEL(ch) < LVL_GOD) {
    send_to_char("They are anonymous.\r\n", ch);
    if (need_to_free_memory)
      free_char(vict);
    return;
  }
  if (PRF2_FLAGGED(ch, PRF2_ANONYMOUS) && GET_LEVEL(ch) < LVL_GOD) {
    send_to_char("You cant finger while anonymous.\r\n", ch);
    if (need_to_free_memory)
      free_char(vict);
    return;
  }
  if ((GET_LEVEL(ch) < LVL_IMMORT) && (GET_LEVEL(vict) >= LVL_IMMORT)) {
    send_to_char("You are not sufficiently godly for that!\r\n", ch);
    if (need_to_free_memory)
      free_char(vict);
    return;
  }


  /* Archfoe and best kill */
  if (GET_TEMPBESTKILL(vict) == 0) {
    GET_TEMPBESTKILLRANK(vict) = GET_BESTKILLRANK(vict);
    GET_TEMPBESTKILL(vict) = GET_BESTKILL(vict);
  } 
  if (GET_BESTKILL(ch) == -1) {
    GET_BESTKILL(ch) = 0;
    GET_BESTKILLRANK(ch) = 0;
    GET_TEMPBESTKILL(ch) = 0;
    GET_TEMPBESTKILLRANK(ch) = 0;
  }
  if (GET_ARCHFOE(vict) < 0) {
    sprintf(buf2, "%s", player_table[-1 * GET_ARCHFOE(vict)].name);
  } else {
    if ((r_num = real_mobile(GET_ARCHFOE(vict))) < 0) {
      sprintf(buf2, "None");
    } else {
      mob = read_mobile(r_num, REAL);
      char_to_room(mob, 0);
      sprintf(buf2, "%s", GET_NAME(mob));
      extract_char(mob);
    }
  }
  if (GET_BESTKILL(vict) < 0) {
    sprintf(buf3, "%s", player_table[-1 * GET_BESTKILL(vict)].name);
  } else { 
    if ((r_num = real_mobile(GET_BESTKILL(vict))) < 0) {
      sprintf(buf3, "None");
    } else {
      mob = read_mobile(r_num, REAL);
      char_to_room(mob, 0);
      sprintf(buf3, "%s", GET_NAME(mob));
      extract_char(mob);
    }
  }

  sprintf(buf, "%s %s\r\n"
               "Level: %-2d               Class: %s\r\n"
               "Clan: %-17s Race: %s\r\n"
               "Archfoe: %s (rank %ld)\r\n"
               "Bestkill: %s (rank %ld)\r\n"
               "Last Login: %s",
      GET_NAME(vict), GET_TITLE(vict),
      GET_LEVEL(vict), pc_class_types[(int) GET_CLASS(vict)],
      clan_names[(int) GET_CLAN(vict)],
      pc_race_types[GET_RACE(vict)],
      buf2, GET_ARCHFOERANK(vict),
      buf3, GET_BESTKILLRANK(vict),
      ctime(&victdata.last_logon));
  send_to_char(buf, ch);
  if (need_to_free_memory)
    free_char(vict);
  return;
}
#endif


ACMD(do_compel)
{
  struct descriptor_data *i, *next_desc;
  struct char_data *vict, *next_compel;
  char to_compel[MAX_INPUT_LENGTH + 2];

  half_chop(argument, arg, to_compel);

  if (!*arg || !*to_compel)
    send_to_char("Whom do you wish to compel?\r\n", ch);
  else if ((GET_LEVEL(ch) < LVL_GRGOD) || (str_cmp("all", arg) && str_cmp("room", arg))) {
    if (!(vict = get_char_vis(ch, arg)))
      send_to_char(NOPERSON, ch);
    else if (GET_LEVEL(ch) <= GET_LEVEL(vict))
      send_to_char("No, no, no!\r\n", ch);
    else {
      send_to_char(OK, ch);
      sprintf(buf, "(GC) %s compelled %s to %s", GET_NAME(ch), GET_NAME(vict), to_compel);
      mudlog(buf, NRM, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);
      command_interpreter(vict, to_compel);
    }
  } else if (!str_cmp("room", arg)) {
    send_to_char(OK, ch);
    sprintf(buf, "(GC) %s compelled room %d to %s", GET_NAME(ch), world[ch->in_room].number, to_compel);
    mudlog(buf, NRM, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);

    for (vict = world[ch->in_room].people; vict; vict = next_compel) {
      next_compel = vict->next_in_room;
      if (GET_LEVEL(vict) >= GET_LEVEL(ch))
	continue;
      command_interpreter(vict, to_compel);
    }
  } else { /* compel all */
    send_to_char(OK, ch);
    sprintf(buf, "(GC) %s compelled all to %s", GET_NAME(ch), to_compel);
    mudlog(buf, NRM, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);

    for (i = descriptor_list; i; i = next_desc) {
      next_desc = i->next;

      if (i->connected || !(vict = i->character) || GET_LEVEL(vict) >= GET_LEVEL(ch))
	continue;
      command_interpreter(vict, to_compel);
    }
  }
}

ACMD(do_force)
{
  struct descriptor_data *i, *next_desc;
  struct char_data *vict, *next_force;
  char to_force[MAX_INPUT_LENGTH + 2];
  ACMD(do_save);

  half_chop(argument, arg, to_force);

  sprintf(buf1, "$n has forced you to '%s'.", to_force);

  if (!*arg || !*to_force)
    send_to_char("Whom do you wish to force do what?\r\n", ch);
  else if ((GET_LEVEL(ch) < LVL_GRGOD) || (str_cmp("all", arg) && str_cmp("room", arg))) {
    if (!(vict = get_char_vis(ch, arg)))
      send_to_char(NOPERSON, ch);
    else if ((GET_LEVEL(ch) <= GET_LEVEL(vict)) && GET_LEVEL(ch) != LVL_IMPL)
      send_to_char("No, no, no!\r\n", ch);
    else {
      send_to_char(OK, ch);
      act(buf1, TRUE, ch, NULL, vict, TO_VICT);
      sprintf(buf, "(GC) %s forced %s to %s", GET_NAME(ch), GET_NAME(vict), to_force);
      mudlog(buf, NRM, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);
      command_interpreter(vict, to_force);
    }
  } else if (!str_cmp("room", arg)) {
    send_to_char(OK, ch);
    sprintf(buf, "(GC) %s forced room %d to %s", GET_NAME(ch), world[ch->in_room].number, to_force);
    mudlog(buf, NRM, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);

    for (vict = world[ch->in_room].people; vict; vict = next_force) {
      next_force = vict->next_in_room;
      if (GET_LEVEL(vict) >= GET_LEVEL(ch))
	continue;
      act(buf1, TRUE, ch, NULL, vict, TO_VICT);
      command_interpreter(vict, to_force);
    }
  } else { /* force all */
    send_to_char(OK, ch);
    sprintf(buf, "(GC) %s forced all to %s", GET_NAME(ch), to_force);
    mudlog(buf, NRM, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);

#ifdef DRAGONSLAVE
    if (!str_cmp(buf1, "save")) {
      for (i = descriptor_list; i; i = next_desc) {
        next_desc = i->next;

        if (i->connected || !(vict = i->character) || GET_LEVEL(vict) >= GET_LEVEL(ch))
   	  continue;
   	
   	do_save(vict, "", 0, SCMD_QUIET_SAVE);
   	act("Saving $N", TRUE, ch, NULL, vict, TO_CHAR);
      }
      return;
    }
#endif

    for (i = descriptor_list; i; i = next_desc) {
      next_desc = i->next;

      if (i->connected || !(vict = i->character) || GET_LEVEL(vict) >= GET_LEVEL(ch))
	continue;
      act(buf1, TRUE, ch, NULL, vict, TO_VICT);
      command_interpreter(vict, to_force);
    }
  }
}



ACMD(do_wiznet)
{
  struct descriptor_data *d;
  char emote = FALSE;
  char any = FALSE;
  int level = LVL_IMMORT;

  skip_spaces(&argument);
  delete_doubledollar(argument);

  if (!*argument) {
    send_to_char("Usage: wiznet <text> | #<level> <text> | *<emotetext> |\r\n "
		 "       wiznet @<level> *<emotetext> | wiz @\r\n", ch);
    return;
  }
  switch (*argument) {
  case '*':
    emote = TRUE;
  case '#':
    one_argument(argument + 1, buf1);
    if (is_number(buf1)) {
      half_chop(argument+1, buf1, argument);
      level = MAX(atoi(buf1), LVL_IMMORT);
      if (level > GET_LEVEL(ch)) {
	send_to_char("You can't wizline above your own level.\r\n", ch);
	return;
      }
    } else if (emote)
      argument++;
    break;
  case '@':
    for (d = descriptor_list; d; d = d->next) {
      if (!d->connected && GET_LEVEL(d->character) >= LVL_IMMORT &&
	  !PRF_FLAGGED(d->character, PRF_NOWIZ) &&
	  (CAN_SEE(ch, d->character) || GET_LEVEL(ch) == LVL_IMPL)) {
	if (!any) {
	  sprintf(buf1, "Gods online:\r\n");
	  any = TRUE;
	}
	sprintf(buf1, "%s  %s", buf1, GET_NAME(d->character));
	if (PLR_FLAGGED(d->character, PLR_WRITING))
	  sprintf(buf1, "%s (Writing)\r\n", buf1);
	else if (PLR_FLAGGED(d->character, PLR_MAILING))
	  sprintf(buf1, "%s (Writing mail)\r\n", buf1);
	else
	  sprintf(buf1, "%s\r\n", buf1);

      }
    }
    any = FALSE;
    for (d = descriptor_list; d; d = d->next) {
      if (!d->connected && GET_LEVEL(d->character) >= LVL_IMMORT &&
	  PRF_FLAGGED(d->character, PRF_NOWIZ) &&
	  CAN_SEE(ch, d->character)) {
	if (!any) {
	  sprintf(buf1, "%sGods offline:\r\n", buf1);
	  any = TRUE;
	}
	sprintf(buf1, "%s  %s\r\n", buf1, GET_NAME(d->character));
      }
    }
    send_to_char(buf1, ch);
    return;
    break;
  case '\\':
    ++argument;
    break;
  default:
    break;
  }
  if (PRF_FLAGGED(ch, PRF_NOWIZ)) {
    send_to_char("You are offline!\r\n", ch);
    return;
  }
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char("Don't bother the gods like that!\r\n", ch);
    return;
  }
  if (level > LVL_IMMORT) {
    sprintf(buf1, "%s: <%d> %s%s\r\n", GET_NAME(ch), level,
	    emote ? "<--- " : "", argument);
    sprintf(buf2, "Someone: <%d> %s%s\r\n", level, emote ? "<--- " : "",
	    argument);
  } else {
    sprintf(buf1, "%s: %s%s\r\n", GET_NAME(ch), emote ? "<--- " : "",
	    argument);
    sprintf(buf2, "Someone: %s%s\r\n", emote ? "<--- " : "", argument);
  }

  for (d = descriptor_list; d; d = d->next) {
    if ((!d->connected) && (GET_LEVEL(d->character) >= level) &&
	(!PRF_FLAGGED(d->character, PRF_NOWIZ)) &&
	(!PLR_FLAGGED(d->character, PLR_WRITING | PLR_MAILING))
	&& (d != ch->desc || !(PRF_FLAGGED(d->character, PRF_NOREPEAT)))) {
      send_to_char(CCWIZNET(d->character), d->character);
      if (CAN_SEE(d->character, ch))
	send_to_char(buf1, d->character);
      else
	send_to_char(buf2, d->character);
      send_to_char(CCNRM(d->character), d->character);
    }
  }

  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
}



ACMD(do_zreset)
{
  void reset_zone(int zone);

  int i, j;

  one_argument(argument, arg);
  if (!*arg) {
    send_to_char("You must specify a zone.\r\n", ch);
    return;
  }

  if (*arg == '*') {
    if (GET_LEVEL(ch) < LVL_IMPL) {
      send_to_char("You can only zreset your own zone.\r\n", ch);
      return;
    }
    for (i = 0; i <= top_of_zone_table; i++)
      reset_zone(i);
    send_to_char("Reset world.\r\n", ch);
    return;
  } else if (*arg == '.') {
    if (GET_LEVEL(ch) == LVL_IMPL)
      i = world[ch->in_room].zone;
    else {
      if ((world[ch->in_room].number / 100) == (GET_OLC_ZONE(ch) - 1))
        i = world[ch->in_room].zone;
      else {
        send_to_char("You can only zreset your own zone"
            " while you are in it.\r\n", ch);
        return;
      }
    }
  } else {
    j = atoi(arg);
    if ((GET_LEVEL(ch) < LVL_IMPL) && ((GET_OLC_ZONE(ch) - 1) != j)) {
      send_to_char("You can only zreset your own zone.\r\n", ch);
      return;
    }
    for (i = 0; i <= top_of_zone_table; i++)
      if (zone_table[i].number == j)
	break;
  }
  if (i >= 0 && i <= top_of_zone_table) {
    reset_zone(i);
    sprintf(buf, "Reset zone %d (%s).\r\n", zone_table[i].number,
	    zone_table[i].name);
    send_to_char(buf, ch);
    sprintf(buf, "(GC) %s reset zone %d (%s)", GET_NAME(ch),
            zone_table[i].number, zone_table[i].name);
    mudlog(buf, NRM, MAX(LVL_GRGOD, GET_INVIS_LEV(ch)), TRUE);
  } else
    send_to_char("Invalid zone number.\r\n", ch);
}



/*
 *  General fn for wizcommands of the sort: cmd <player>
 */
ACMD(do_wizutil)
{
  struct char_data *vict;
  long result;
  void roll_real_abils(struct char_data *ch);

  one_argument(argument, arg);

  if (!*arg)
    send_to_char("Yes, but for whom?!?\r\n", ch);
  else if (!(vict = get_char_vis(ch, arg)))
    send_to_char("There is no such player.\r\n", ch);
  else if (IS_NPC(vict))
    send_to_char("You can't do that to a mob!\r\n", ch);
  else if (GET_LEVEL(vict) > GET_LEVEL(ch))
    send_to_char("Hmmm...you'd better not.\r\n", ch);
  else {
    switch (subcmd) {
    case SCMD_REROLL:
      send_to_char("Rerolled...\r\n", ch);
      roll_real_abils(vict);
      sprintf(buf, "(GC) %s has rerolled %s.", GET_NAME(ch), GET_NAME(vict));
      log(buf);
      sprintf(buf, "New stats: Str %d, Int %d, Wis %d, Dex %d, Con %d, Cha %d\r\n",
	      GET_STR(vict), GET_INT(vict), GET_WIS(vict),
/*
	      GET_STR(vict), GET_ADD(vict), GET_INT(vict), GET_WIS(vict),
*/
	      GET_DEX(vict), GET_CON(vict), GET_CHA(vict));
      send_to_char(buf, ch);
      break;
    case SCMD_PARDON:
      if (!PLR_FLAGGED(vict, PLR_THIEF | PLR_KILLER)) {
	send_to_char("Your victim is not flagged.\r\n", ch);
	return;
      }
      REMOVE_BIT(PLR_FLAGS(vict), PLR_THIEF | PLR_KILLER);
      send_to_char("Pardoned.\r\n", ch);
      send_to_char("You have been pardoned by the Gods!\r\n", vict);
      sprintf(buf, "(GC) %s pardoned by %s", GET_NAME(vict), GET_NAME(ch));
      mudlog(buf, BRF, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);
      break;
    case SCMD_NOTITLE:
      result = PLR_TOG_CHK(vict, PLR_NOTITLE);
      sprintf(buf, "(GC) Notitle %s for %s by %s.", ONOFF(result),
	      GET_NAME(vict), GET_NAME(ch));
      mudlog(buf, NRM, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);
      strcat(buf, "\r\n");
      send_to_char(buf, ch);
      break;
    case SCMD_SQUELCH:
      result = PLR_TOG_CHK(vict, PLR_NOSHOUT);
      sprintf(buf, "(GC) Squelch %s for %s by %s.", ONOFF(result),
	      GET_NAME(vict), GET_NAME(ch));
      mudlog(buf, BRF, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);
      strcat(buf, "\r\n");
      send_to_char(buf, ch);
      break;
    case SCMD_FREEZE:
      if (ch == vict) {
	send_to_char("Oh, yeah, THAT'S real smart...\r\n", ch);
	return;
      }
      if (PLR_FLAGGED(vict, PLR_FROZEN)) {
	send_to_char("Your victim is already pretty cold.\r\n", ch);
	return;
      }
      SET_BIT(PLR_FLAGS(vict), PLR_FROZEN);
      GET_FREEZE_LEV(vict) = GET_LEVEL(ch);
      send_to_char("A bitter wind suddenly rises and drains every erg of heat from your body!\r\nYou feel frozen!\r\n", vict);
      send_to_char("Frozen.\r\n", ch);
      act("A sudden cold wind conjured from nowhere freezes $n!", FALSE, vict, 0, 0, TO_ROOM);
      sprintf(buf, "(GC) %s frozen by %s.", GET_NAME(vict), GET_NAME(ch));
      mudlog(buf, BRF, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);
      break;
    case SCMD_THAW:
      if (!PLR_FLAGGED(vict, PLR_FROZEN)) {
	send_to_char("Sorry, your victim is not morbidly encased in ice at the moment.\r\n", ch);
	return;
      }
      if (GET_FREEZE_LEV(vict) > GET_LEVEL(ch)) {
	sprintf(buf, "Sorry, a level %d God froze %s... you can't unfreeze %s.\r\n",
	   GET_FREEZE_LEV(vict), GET_NAME(vict), HMHR(vict));
	send_to_char(buf, ch);
	return;
      }
      sprintf(buf, "(GC) %s un-frozen by %s.", GET_NAME(vict), GET_NAME(ch));
      mudlog(buf, BRF, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE);
      REMOVE_BIT(PLR_FLAGS(vict), PLR_FROZEN);
      send_to_char("A fireball suddenly explodes in front of you, melting the ice!\r\nYou feel thawed.\r\n", vict);
      send_to_char("Thawed.\r\n", ch);
      act("A sudden fireball conjured from nowhere thaws $n!", FALSE, vict, 0, 0, TO_ROOM);
      break;
    case SCMD_UNAFFECT:
      if (vict->affected) {
	while (vict->affected)
	  affect_remove(vict, vict->affected);
	send_to_char("There is a brief flash of light!\r\n"
		     "You feel slightly different.\r\n", vict);
	send_to_char("All spells removed.\r\n", ch);
      } else {
	send_to_char("Your victim does not have any affections!\r\n", ch);
/*	return;*/
      }
      AFF_FLAGS(vict) = 0;
      AFF2_FLAGS(vict) = 0;
      affect_total(vict);
      break;
    default:
      log("SYSERR: Unknown subcmd passed to do_wizutil (act.wizard.c)");
      break;
    }
    save_char(vict, NOWHERE);
  }
}



ACMD(do_reroll)
{
  void roll_real_abils(struct char_data *ch);


/*
  if (GET_LEVEL(ch) > 1) {
    send_to_char("Sorry, you can only reroll "
                 "when you are just starting out.\r\n", ch);
    return;
  }
*/

  send_to_char("Rerolling...\r\n", ch);
  roll_real_abils(ch);
  sprintf(buf, "New stats: Str %d, Int %d, Wis %d, "
                          "Dex %d, Con %d, Cha %d\r\n",
/*
    GET_STR(ch), GET_ADD(ch), GET_INT(ch), GET_WIS(ch),
*/
    GET_STR(ch), GET_INT(ch), GET_WIS(ch),
    GET_DEX(ch), GET_CON(ch), GET_CHA(ch));
  send_to_char(buf, ch);
  save_char(ch, NOWHERE);
  WAIT_STATE(ch, PULSE_VIOLENCE);
}



/* single zone printing fn used by "show zone" so it's not repeated in the
   code 3 times ... -je, 4/6/93 */

void print_zone_to_buf(char *bufptr, int zone)
{
  sprintf(bufptr, "%s%3d %-30.30s Age: %3d; Reset: %3d (%1d); Top: %5d\r\n",
	  bufptr, zone_table[zone].number, zone_table[zone].name,
	  zone_table[zone].age, zone_table[zone].lifespan,
	  zone_table[zone].reset_mode, zone_table[zone].top);
}


ACMD(do_show)
{
  struct char_file_u vbuf;
  int i, j, k, l, con;
  char self = 0;
  char inactive = 0;
  struct char_data *vict;
  struct obj_data *obj;
  struct mobprog_var_data *curvar;
  char field[40], value[40], birth[80];

  extern char *race_abbrevs[];
  extern char *class_abbrevs[];
/* HACKED to show the immort abbreviation */
  extern char *immort_abbrevs[];
/* end of hack */
  extern char *genders[];
  extern int buf_switches, buf_largecount, buf_overflows;
  extern struct mobprog_var_data *mobprog_vars;
  
  void show_shops(struct char_data * ch, char *value);
  void list_restricted_rooms( struct char_data *ch );
  void list_storage_rooms( struct char_data *ch );
  void show_pawnshop(struct char_data *ch);
  void show_timer_queue(struct char_data *ch);

  struct show_struct {
    char *cmd;
    char level;
  } fields[] = {
    { "nothing",	0  },				/* 0 */
    { "zones",		LVL_IMMORT },			/* 1 */
    { "player",		LVL_GOD },
    { "rent",		LVL_GOD },
    { "stats",		LVL_IMMORT },
    { "errors",		LVL_IMPL },			/* 5 */
    { "death",		LVL_GOD },
    { "godrooms",	LVL_GOD },
    { "shops",		LVL_IMMORT },
    { "restricted",	LVL_DEITY },
    { "storage",	LVL_DEITY },			/* 10 */
    { "pawnshop",       LVL_DEITY },
    { "events",		LVL_IMPL },
    { "mobvars",	LVL_IMPL },
    { "\n", 0 }
  };

  skip_spaces(&argument);

  if (!*argument) {
    strcpy(buf, "Show options:\r\n");
    for (j = 0, i = 1; fields[i].level; i++)
      if (fields[i].level <= GET_LEVEL(ch))
	sprintf(buf, "%s%-15s%s", buf, fields[i].cmd, (!(++j % 5) ? "\r\n" : ""));
    strcat(buf, "\r\n");
    send_to_char(buf, ch);
    return;
  }

  strcpy(arg, two_arguments(argument, field, value));

  for (l = 0; *(fields[l].cmd) != '\n'; l++)
    if (!strncmp(field, fields[l].cmd, strlen(field)))
      break;

  if (GET_LEVEL(ch) < fields[l].level) {
    send_to_char("You are not godly enough for that!\r\n", ch);
    return;
  }
  if (!strcmp(value, "."))
    self = 1;
  if (!strcmp(value, "inactive"))
    inactive = 1;
  buf[0] = '\0';
  switch (l) {
  case 1:			/* zone */
    if (self)
      print_zone_to_buf(buf, world[ch->in_room].zone);
    else if (inactive) {
	for (i = 0; i <= top_of_zone_table; i++) {
	    if (!IS_SET(zone_table[i].zone_flags, ZONE_ACTIVE))
		print_zone_to_buf(buf, i);
	}
    }

    else if (*value && is_number(value)) {
      for (j = atoi(value), i = 0; zone_table[i].number != j && i <= top_of_zone_table; i++);
      if (i <= top_of_zone_table)
	print_zone_to_buf(buf, i);
      else {
	send_to_char("That is not a valid zone.\r\n", ch);
	return;
      }
    } else
      for (i = 0; i <= top_of_zone_table; i++)
	print_zone_to_buf(buf, i);
    page_string(ch->desc, buf, 0);
    break;
  case 2:			/* player */
    if (!*value) {
      send_to_char("A name would help.\r\n", ch);
      return;
    }

    if (load_char(value, &vbuf) < 0) {
      send_to_char("There is no such player.\r\n", ch);
      return;
    }

    if (vbuf.level >= LVL_IMMORT) {
      sprintf(buf, "Player: %-12s (%s) [%2d %s %s]\r\n", vbuf.name,
        genders[(int) vbuf.sex], vbuf.level, 
        race_abbrevs[(int) vbuf.player_specials_saved.race], 
        immort_abbrevs[(int) vbuf.level - LVL_IMMORT]);
    } else {
      sprintf(buf, "Player: %-12s (%s) [%2d %s %s]\r\n", vbuf.name,
        genders[(int) vbuf.sex], vbuf.level,
        race_abbrevs[(int) vbuf.player_specials_saved.race],
        class_abbrevs[(int) vbuf.class]);
    }

    sprintf(buf,
	 "%sAu: %-8d  Bal: %-8d  Exp: %-8d  Align: %-5d  Lessons: %-3d\r\n",
	    buf, vbuf.points.gold, vbuf.points.bank_gold, vbuf.points.exp,
	    vbuf.char_specials_saved.alignment,
	    vbuf.player_specials_saved.spells_to_learn);
    strcpy(birth, ctime(&vbuf.birth));
    sprintf(buf,
	    "%sStarted: %-20.16s  Last: %-20.16s  Played: %3dh %2dm\r\n",
	    buf, birth, ctime(&vbuf.last_logon), (int) (vbuf.played / 3600),
	    (int) (vbuf.played / 60 % 60));
    send_to_char(buf, ch);
    break;
  case 3:
    Crash_listrent(ch, value);
    break;
  case 4:
    i = 0;
    j = 0;
    k = 0;
    con = 0;
    for (vict = character_list; vict; vict = vict->next) {
      if (IS_NPC(vict))
	j++;
      else if (CAN_SEE(ch, vict)) {
	i++;
	if (vict->desc)
	  con++;
      }
    }
    for (obj = object_list; obj; obj = obj->next)
      k++;
    sprintf(buf, "Current stats:\r\n");
    sprintf(buf, "%s  %5d players in game  %5d connected\r\n", buf, i, con);
    sprintf(buf, "%s  %5d registered\r\n", buf, top_of_p_table + 1);
    sprintf(buf, "%s  %5d mobiles          %5d prototypes\r\n",
	    buf, j, top_of_mobt + 1);
    sprintf(buf, "%s  %5d objects          %5d prototypes\r\n",
	    buf, k, top_of_objt + 1);
    sprintf(buf, "%s  %5d rooms            %5d zones\r\n",
	    buf, top_of_world + 1, top_of_zone_table + 1);
    sprintf(buf, "%s  %5d large bufs\r\n", buf, buf_largecount);
    sprintf(buf, "%s  %5d buf switches     %5d overflows\r\n", buf,
	    buf_switches, buf_overflows);
    send_to_char(buf, ch);
    break;
  case 5:
    strcpy(buf, "Errant Rooms\r\n------------\r\n");
    for (i = 0, k = 0; i <= top_of_world; i++)
      for (j = 0; j < NUM_OF_DIRS; j++)
	if (world[i].dir_option[j] && world[i].dir_option[j]->to_room == 0)
	  sprintf(buf, "%s%2d: [%5d] %s\r\n", buf, ++k, world[i].number,
		  world[i].name);
    send_to_char(buf, ch);
    break;
  case 6:
    strcpy(buf, "Death Traps\r\n-----------\r\n");
    for (i = 0, j = 0; i <= top_of_world; i++)
      if (IS_SET(ROOM_FLAGS(i), ROOM_DEATH))
	sprintf(buf, "%s%2d: [%5d] %s\r\n", buf, ++j,
		world[i].number, world[i].name);
    send_to_char(buf, ch);
    break;
  case 7:
#define GOD_ROOMS_ZONE 2
    strcpy(buf, "Godrooms\r\n--------------------------\r\n");
    for (i = 0, j = 0; i < top_of_world; i++)
      if (world[i].zone == GOD_ROOMS_ZONE)
	sprintf(buf, "%s%2d: [%5d] %s\r\n", buf, j++, world[i].number,
		world[i].name);
    send_to_char(buf, ch);
    break;
  case 8:
    show_shops(ch, value);
    break;
  case 9:
    list_restricted_rooms(ch);
    break;
  case 10:
    list_storage_rooms(ch);
    break;
  case 11:
    show_pawnshop(ch);
    break;
  case 12:
    show_timer_queue(ch);
    break;
  case 13:
    for (curvar = mobprog_vars; curvar; curvar = curvar->next) {
      sprintf(buf, "%20s : %d\r\n", curvar->name, curvar->val);
      send_to_char(buf, ch);
    }
    break;
  default:
    send_to_char("Sorry, I don't understand that.\r\n", ch);
    break;
  }
}



#define PC   1
#define NPC  2
#define BOTH 3

#define MISC	0
#define BINARY	1
#define NUMBER	2

#define SET_OR_REMOVE(flagset, flags) { \
	if (on) SET_BIT(flagset, flags); \
	else if (off) REMOVE_BIT(flagset, flags); }

#define RANGE(low, high) (value = MAX((low), MIN((high), (value))))

ACMD(do_set)
{
  int i, l;
  struct char_data *vict;
  struct char_data *cbuf;
  struct char_file_u tmp_store;
  char field[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
  int on = 0, off = 0, value = 0;
  char is_file = 0, is_mob = 0, is_player = 0;
  int player_i;
  int parse_race(char *arg);
  int parse_clan(char *arg);
  int parse_class(char *arg);

  struct set_struct {
    char *cmd;
    char level;
    char pcnpc;
    char type;
  }          fields[] = {
   { "brief",		LVL_GOD, 	PC, 	BINARY },  /* 0 */
   { "invstart", 	LVL_GOD, 	PC, 	BINARY },  /* 1 */
   { "title",		LVL_GOD, 	PC, 	MISC },
   { "nosummon", 	LVL_GRGOD, 	PC, 	BINARY },
   { "maxhit",		LVL_GOD, 	BOTH, 	NUMBER },
   { "maxmana", 	LVL_GRGOD, 	BOTH, 	NUMBER },  /* 5 */
   { "maxmove", 	LVL_GRGOD, 	BOTH, 	NUMBER },
   { "hit", 		LVL_GOD, 	BOTH, 	NUMBER },
   { "mana",		LVL_GRGOD, 	BOTH, 	NUMBER },
   { "move",		LVL_GRGOD, 	BOTH, 	NUMBER },
   { "align",		LVL_GOD, 	BOTH, 	NUMBER },  /* 10 */
   { "str",		LVL_GOD, 	BOTH, 	NUMBER },
   { "stradd",		LVL_GOD, 	BOTH, 	NUMBER },
   { "int", 		LVL_GRGOD, 	BOTH, 	NUMBER },
   { "wis", 		LVL_GRGOD, 	BOTH, 	NUMBER },
   { "dex", 		LVL_GRGOD, 	BOTH, 	NUMBER },  /* 15 */
   { "con", 		LVL_GRGOD, 	BOTH, 	NUMBER },
   { "sex", 		LVL_GOD, 	BOTH, 	MISC },
   { "ac", 		LVL_GRGOD, 	BOTH, 	NUMBER },
   { "gold",		LVL_GOD, 	BOTH, 	NUMBER },
   { "bank",		LVL_GOD, 	PC, 	NUMBER },  /* 20 */
   { "exp", 		LVL_GOD, 	BOTH, 	NUMBER },
   { "hitroll", 	LVL_GOD, 	BOTH, 	NUMBER },
   { "damroll", 	LVL_GOD, 	BOTH, 	NUMBER },
   { "invis",		LVL_IMPL, 	PC, 	NUMBER },
   { "nohassle", 	LVL_GRGOD, 	PC, 	BINARY },  /* 25 */
   { "frozen",		LVL_FREEZE, 	PC, 	BINARY },
   { "practices", 	LVL_GRGOD, 	PC, 	NUMBER },
   { "lessons", 	LVL_GRGOD, 	PC, 	NUMBER },
   { "drunk",		LVL_GRGOD, 	BOTH, 	MISC },
   { "hunger",		LVL_GRGOD, 	BOTH, 	MISC },    /* 30 */
   { "thirst",		LVL_GRGOD, 	BOTH, 	MISC },
   { "killer",		LVL_GOD, 	PC, 	BINARY },
   { "thief",		LVL_GOD, 	PC, 	BINARY },
   { "level",		LVL_GOD, 	BOTH, 	NUMBER },
   { "room",		LVL_IMPL, 	BOTH, 	NUMBER },  /* 35 */
   { "roomflag", 	LVL_GRGOD, 	PC, 	BINARY },
   { "siteok",		LVL_GOD, 	PC, 	BINARY },
   { "deleted", 	LVL_IMPL, 	PC, 	BINARY },
   { "class",		LVL_GOD, 	BOTH, 	MISC },
   { "nowizlist", 	LVL_GOD, 	PC, 	BINARY },  /* 40 */
   { "squelch",		LVL_GRGOD, 	PC, 	BINARY },
   { "loadroom", 	LVL_GRGOD, 	PC, 	MISC },
   { "color",		LVL_GOD, 	PC, 	BINARY },
   { "idnum",		LVL_IMPL, 	PC, 	NUMBER },
   { "passwd",		LVL_IMPL, 	PC, 	MISC },    /* 45 */
   { "nodelete", 	LVL_GOD, 	PC, 	BINARY },
   { "cha",		LVL_GRGOD, 	BOTH, 	NUMBER },
   { "hometown",	LVL_IMPL,	PC,	NUMBER },
   { "race",		LVL_GOD,	PC,	MISC },
   { "clan",		LVL_GRGOD,	PC,	MISC },    /* 50 */
   { "away",            LVL_IMPL,       PC,     BINARY },
   { "clanlevel",	LVL_GRGOD,	PC,	NUMBER },
   { "olc",		LVL_IMMORT,	PC,	NUMBER },
   { "holylight",       LVL_GRGOD,      PC,     BINARY },
   { "master",		LVL_GRGOD,	PC,	BINARY },  /* 55 */
   { "nopawn",		LVL_IMPL,	PC,	BINARY },
   { "questpoints",	LVL_IMPL,	PC,	NUMBER },
   { "countdown",	LVL_IMPL,	PC,	NUMBER },
   { "nextquest",	LVL_IMPL,	PC,	NUMBER },
   { "showdam",         LVL_IMPL,       PC,     BINARY },  /* 60 */
   { "\n", 0, BOTH, MISC }
  };

  half_chop(argument, name, buf);
  if (!strcmp(name, "file")) {
    is_file = 1;
    half_chop(buf, name, buf);
  } else if (!str_cmp(name, "player")) {
    is_player = 1;
    half_chop(buf, name, buf);
  } else if (!str_cmp(name, "mob")) {
    is_mob = 1;
    half_chop(buf, name, buf);
  }
  half_chop(buf, field, buf);
  strcpy(val_arg, buf);

  if (!*name || !*field) {
    send_to_char("Usage: set <victim> <field> <value>\r\n", ch);
    return;
  }
  if (!is_file) {
    if (is_player) {
      if (!(vict = get_player_vis(ch, name))) {
	send_to_char("There is no such player.\r\n", ch);
	return;
      }
    } else {
      if (!(vict = get_char_vis(ch, name))) {
	send_to_char("There is no such creature.\r\n", ch);
	return;
      }
    }
  } else if (is_file) {
    CREATE(cbuf, struct char_data, 1);
    clear_char(cbuf);
    if ((player_i = load_char(name, &tmp_store)) > -1) {
      store_to_char(&tmp_store, cbuf);
      vict = cbuf;
    } else {
      free(cbuf);
      send_to_char("There is no such player.\r\n", ch);
      return;
    }
  }
  if (GET_LEVEL(ch) != LVL_IMPL) {
    if (!IS_NPC(vict) && GET_LEVEL(ch) <= GET_LEVEL(vict) && vict != ch) {
      send_to_char("Maybe that's not such a great idea...\r\n", ch);
      return;
    }
  }
  for (l = 0; *(fields[l].cmd) != '\n'; l++)
    if (!strncmp(field, fields[l].cmd, strlen(field)))
      break;

  if (GET_LEVEL(ch) < fields[l].level) {
    send_to_char("You are not godly enough for that!\r\n", ch);
    return;
  }
  if (IS_NPC(vict) && !(fields[l].pcnpc && NPC)) {
    send_to_char("You can't do that to a beast!\r\n", ch);
    return;
  } else if (!IS_NPC(vict) && !(fields[l].pcnpc && PC)) {
    send_to_char("That can only be done to a beast!\r\n", ch);
    return;
  }
  if (fields[l].type == BINARY) {
    if (!strcmp(val_arg, "on") || !strcmp(val_arg, "yes"))
      on = 1;
    else if (!strcmp(val_arg, "off") || !strcmp(val_arg, "no"))
      off = 1;
    if (!(on || off)) {
      send_to_char("Value must be on or off.\r\n", ch);
      return;
    }
  } else if (fields[l].type == NUMBER) {
    value = atoi(val_arg);
  }
  strcpy(buf, "Okay.");
  switch (l) {
  case 0:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_BRIEF);
    break;
  case 1:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_INVSTART);
    break;
  case 2:
    set_title(vict, val_arg);
    sprintf(buf, "%s's title is now: %s", GET_NAME(vict), GET_TITLE(vict));
    break;
  case 3:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_SUMMONABLE);
    on = !on;			/* so output will be correct */
    break;
  case 4:
    vict->points.max_hit = RANGE(1, 30000);
    affect_total(vict);
    break;
  case 5:
    vict->points.max_mana = RANGE(1, 30000);
    affect_total(vict);
    break;
  case 6:
    vict->points.max_move = RANGE(1, 30000);
    affect_total(vict);
    break;
  case 7:
    vict->points.hit = RANGE(-9, vict->points.max_hit);
    affect_total(vict);
    break;
  case 8:
    vict->points.mana = RANGE(0, vict->points.max_mana);
    affect_total(vict);
    break;
  case 9:
    vict->points.move = RANGE(0, vict->points.max_move);
    affect_total(vict);
    break;
  case 10:
    GET_ALIGNMENT(vict) = RANGE(-1000, 1000);
    affect_total(vict);
    break;
  case 11:
    RANGE(3, 25);
    vict->real_abils.str = value;
    vict->real_abils.str_add = 0;
    affect_total(vict);
    break;
  case 12:
    vict->real_abils.str_add = RANGE(0, 100);
    affect_total(vict);
    break;
  case 13:
    RANGE(3, 25);
    vict->real_abils.intel = value;
    affect_total(vict);
    break;
  case 14:
    RANGE(3, 25);
    vict->real_abils.wis = value;
    affect_total(vict);
    break;
  case 15:
    RANGE(3, 25);
    vict->real_abils.dex = value;
    affect_total(vict);
    break;
  case 16:
    RANGE(3, 25);
    vict->real_abils.con = value;
    affect_total(vict);
    break;
  case 17:
    if (!str_cmp(val_arg, "male"))
      vict->player.sex = SEX_MALE;
    else if (!str_cmp(val_arg, "female"))
      vict->player.sex = SEX_FEMALE;
    else if (!str_cmp(val_arg, "neutral"))
      vict->player.sex = SEX_NEUTRAL;
    else {
      send_to_char("Must be 'male', 'female', or 'neutral'.\r\n", ch);
      return;
    }
    break;
  case 18:
    vict->points.armor = RANGE(-200, 200);
    affect_total(vict);
    break;
  case 19:
    GET_GOLD(vict) = RANGE(0, 100000000);
    break;
  case 20:
    GET_BANK_GOLD(vict) = RANGE(0, 100000000);
    break;
  case 21:
    vict->points.exp = RANGE(0, 2100000000);
    break;
  case 22:
    vict->points.hitroll = RANGE(-100, 100);
    affect_total(vict);
    break;
  case 23:
    vict->points.damroll = RANGE(-100, 100);
    affect_total(vict);
    break;
  case 24:
    if (GET_LEVEL(ch) < LVL_IMPL && ch != vict) {
      send_to_char("You aren't godly enough for that!\r\n", ch);
      return;
    }
    GET_INVIS_LEV(vict) = RANGE(0, GET_LEVEL(vict));
    break;
  case 25:
    if (GET_LEVEL(ch) < LVL_IMPL && ch != vict) {
      send_to_char("You aren't godly enough for that!\r\n", ch);
      return;
    }
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_NOHASSLE);
    break;
  case 26:
    if (ch == vict) {
      send_to_char("Better not -- could be a long winter!\r\n", ch);
      return;
    }
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_FROZEN);
    break;
  case 27:
  case 28:
  /*  GET_PRACTICES(vict) = RANGE(0, 100); */
    break;
  case 29:
  case 30:
  case 31:
    if (!str_cmp(val_arg, "off")) {
      GET_COND(vict, (l - 29)) = (char) -1;
      sprintf(buf, "%s's %s now off.", GET_NAME(vict), fields[l].cmd);
    } else if (is_number(val_arg)) {
      value = atoi(val_arg);
      RANGE(0, 24);
      GET_COND(vict, (l - 29)) = (char) value;
      sprintf(buf, "%s's %s set to %d.", GET_NAME(vict), fields[l].cmd,
	      value);
    } else {
      send_to_char("Must be 'off' or a value from 0 to 24.\r\n", ch);
      return;
    }
    break;
  case 32:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_KILLER);
    break;
  case 33:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_THIEF);
    break;
  case 34:
    RANGE(0, LVL_IMPL);
    vict->player.level = (byte) value;
    break;
  case 35:
    if ((i = real_room(value)) < 0) {
      send_to_char("No room exists with that number.\r\n", ch);
      return;
    }
    char_from_room(vict);
    char_to_room(vict, i);
    break;
  case 36:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_ROOMFLAGS);
    break;
  case 37:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_SITEOK);
    break;
  case 38:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_DELETED);
    Crash_delete_file(GET_NAME(vict));
    break;
  case 39:
    if ((i = parse_class(val_arg)) == CLASS_UNDEFINED)
      send_to_char("That is not a class.\r\n", ch);
    else {
      GET_CLASS(vict) = i;
      send_to_char(OK, ch);
    }
    break;
  case 40:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NOWIZLIST);
    break;
  case 41:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NOSHOUT);
    break;
  case 42:
    if (!str_cmp(val_arg, "on"))
      SET_BIT(PLR_FLAGS(vict), PLR_LOADROOM);
    else if (!str_cmp(val_arg, "off"))
      REMOVE_BIT(PLR_FLAGS(vict), PLR_LOADROOM);
    else {
      if (real_room(i = atoi(val_arg)) > -1) {
	GET_LOADROOM(vict) = i;
	sprintf(buf, "%s will enter at %d.", GET_NAME(vict),
		GET_LOADROOM(vict));
      } else
	sprintf(buf, "That room does not exist!");
    }
    break;
  case 43:
      SET_OR_REMOVE(PRF_FLAGS(vict), PRF_COLOR);
    break;
  case 44:
    if (GET_IDNUM(ch) != 1 || !IS_NPC(vict))
      return;
    GET_IDNUM(vict) = value;
    break;
  case 45:
    if (!is_file)
      return;
/*
    if (GET_IDNUM(ch) > 1) {
      send_to_char("Please don't use this command, yet.\r\n", ch);
      return;
    }
*/
    if (GET_LEVEL(vict) == LVL_IMPL) {
      send_to_char("You cannot change that.\r\n", ch);
      return;
    }
    strncpy(tmp_store.pwd, CRYPT(val_arg, tmp_store.name), MAX_PWD_LENGTH);
    tmp_store.pwd[MAX_PWD_LENGTH] = '\0';
    sprintf(buf, "Password changed to '%s'.", val_arg);
    break;
  case 46:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NODELETE);
    break;
  case 47:
    RANGE(3, 25);
    vict->real_abils.cha = value;
    affect_total(vict);
    break;
  case 48:
    GET_HOME(vict) = value;
    send_to_char(OK, ch);
    break;
  case 49:
    if ((i = parse_race(val_arg)) == RACE_UNDEFINED) 
      send_to_char("That is not a race.\r\n", ch);
    else {
      GET_PC_RACE(vict) = i;
      send_to_char(OK, ch);
    }
    break;
  case 50:
    if ((i = parse_clan(val_arg)) == CLAN_UNDEFINED) 
      send_to_char("That is not a clan.\r\n", ch);
    else {
      GET_CLAN(vict) = i;
      send_to_char(OK, ch);
    }
    break; 
  case 51:
    SET_OR_REMOVE(PRF2_FLAGS(vict), PRF2_AWAY);
    break;
  case 52:
    if (value > CLAN_LVL_CHAMPION) {
      send_to_char("You can't do that.\r\n", ch);
      return;
    }
    RANGE(0, CLAN_LVL_CHAMPION);
    GET_CLAN_LEVEL(vict) = (byte) value;
    send_to_char(OK, ch);
    break;
  case 53:
    /* the olc zone is saved one larger than it is */
    /* that way 0 (the default) becomes -1 (not able to edit anything) */
    /* here we save it back large */
    GET_OLC_ZONE(vict) = value + 1;
    break;
  case 54:
    if (GET_LEVEL(ch) < LVL_IMPL && ch != vict) {
      send_to_char("You aren't godly enough for that!\r\n", ch);
      return;
    }
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_HOLYLIGHT);
    break;
  case 55:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_MASTER);
    break;
  case 56:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NOPAWN);
    break;
  case 57:
    (vict)->player_specials->saved.questpoints = RANGE(1, 30000);
    break;
  case 58:
    (vict)->player_specials->saved.countdown = RANGE(1, 30000);
    break;
  case 59:
    (vict)->player_specials->saved.nextquest = RANGE(1, 30000);
    break;
  case 60:
    SET_OR_REMOVE(PRF2_FLAGS(vict), PRF2_SHOW_DAMAGE);
    break;
  default:
    sprintf(buf, "Can't set that!");
    break;
  }

  *buf2 = '\0';
  if (fields[l].type == BINARY) {
    sprintf(buf, "%s %s for %s.\r\n", fields[l].cmd, ONOFF(on),
	    GET_NAME(vict));
    CAP(buf);
    sprintf(buf2, "(GC) %s has set %s's %s %s", GET_NAME(ch),
            GET_NAME(vict), fields[l].cmd, ONOFF(on));
    if (is_file)
      strcat(buf2, " (in file).");
    else
      strcat(buf2, ".");
    mudlog(buf2, CMP, MAX(LVL_GRGOD, GET_INVIS_LEV(ch)), TRUE);
  } else if (fields[l].type == NUMBER) {
    sprintf(buf, "%s's %s set to %d.\r\n", GET_NAME(vict),
	    fields[l].cmd, value);
    sprintf(buf2, "(GC) %s has set %s's %s to %d", GET_NAME(ch),
            GET_NAME(vict), fields[l].cmd, value);
    if (is_file)
      strcat(buf2, " (in file).");
    else
      strcat(buf2, ".");
    mudlog(buf2, CMP, MAX(LVL_GRGOD, GET_INVIS_LEV(ch)), TRUE);
  } else {
    sprintf(buf, "%s's %s set to %s.\r\n", GET_NAME(vict),
	    fields[l].cmd, val_arg);
    sprintf(buf2, "(GC) %s has set %s's %s to %s", GET_NAME(ch),
            GET_NAME(vict), fields[l].cmd, val_arg);
    if (is_file)
      strcat(buf2, " (in file).");
    else
      strcat(buf2, ".");
    mudlog(buf2, CMP, MAX(LVL_GRGOD, GET_INVIS_LEV(ch)), TRUE);
  }
/*
    strcat(buf, "\r\n");
*/
  send_to_char(CAP(buf), ch);
/* end of hack */

  if (!is_file && !IS_NPC(vict))
    save_char(vict, NOWHERE);

  if (is_file) {
    char_to_store(vict, &tmp_store);
    fseek(player_fl, (player_i) * sizeof(struct char_file_u), SEEK_SET);
    fwrite(&tmp_store, sizeof(struct char_file_u), 1, player_fl);
    free_char(cbuf);
    send_to_char("Saved in file.\r\n", ch);
  }
}



static char *logtypes[] = {
"off", "brief", "normal", "complete", "\n"};

ACMD(do_syslog)
{
  int tp;

  one_argument(argument, arg);

  if (!*arg) {
    tp = ((PRF_FLAGGED(ch, PRF_LOG1) ? 1 : 0) +
	  (PRF_FLAGGED(ch, PRF_LOG2) ? 2 : 0));
    sprintf(buf, "Your syslog is currently %s.\r\n", logtypes[tp]);
    send_to_char(buf, ch);
    return;
  }
  if (((tp = search_block(arg, logtypes, FALSE)) == -1)) {
    send_to_char("Usage: syslog { Off | Brief | Normal | Complete }\r\n", ch);
    return;
  }
  REMOVE_BIT(PRF_FLAGS(ch), PRF_LOG1 | PRF_LOG2);
  SET_BIT(PRF_FLAGS(ch), (PRF_LOG1 * (tp & 1)) | (PRF_LOG2 * (tp & 2) >> 1));

  sprintf(buf, "Your syslog is now %s.\r\n", logtypes[tp]);
  send_to_char(buf, ch);
}



static char *quest_status[] = {
  "off", "on", "quest", "survival", "pkquest", "deathquest", "\n" };

ACMD(do_enroll)
{
  struct char_data *vict;
  int i;
  int old_tp;
  int tp;

  half_chop(argument, arg, buf);

  if (!*arg) {
    send_to_char("Enroll who in the quest?\r\n", ch);
    return;
  }
  if (!(vict = get_char_vis(ch, arg))) {
    send_to_char(NOPERSON, ch);
    return;
  }

  old_tp = GET_QUEST(vict);

  if ((tp = search_block(buf, quest_status, FALSE)) == -1) {
    sprintf(buf1, "Enroll status options:");
    for (i = 0; *(quest_status[i]) != '\n'; i++) {
      if ((i % 6) == 0)
        strcat(buf1, "\r\n");
      sprintf(buf2, "%-12s", quest_status[i]);
      strcat(buf1, buf2);
    }
    strcat(buf1, "\r\n");
    send_to_char(buf1, ch);
    return;
  } else {
    GET_QUEST(vict) = tp;
    sprintf(buf1, "%s had quest-status '%s' and is now enrolled '%s'.\r\n",
        GET_NAME(vict), quest_status[old_tp], quest_status[tp]);
    send_to_char(buf1, ch);
    sprintf(buf1, "Your quest-status was '%s' "
                  "and you are now enrolled '%s'.\r\n", 
        quest_status[old_tp], quest_status[tp]);
    send_to_char(buf1, vict);
    return;
  }
}



/* HACKED to fix an age bug where the time server got mixed up.
  the idea is to reset a players age to 0 while they are on line
  for now this code is grabbed from db.c and I believe that only 
  a change in time.birth is necessary. */
/*
ACMD(do_baptise)
{
  struct char_data *vict;

  half_chop(argument, arg, buf);

  if (!*arg) {
    send_to_char("Baptise who?\r\n", ch);
    return;
  }
  if (!(vict = get_char_vis(ch, arg))) {
    send_to_char(NOPERSON, ch);
    return;
  }

  act("$n dunks $N and they are reborn!! Halleluiah!!\r\n", 
    FALSE, ch, 0, vict, TO_ROOM);

  vict->player.time.birth = time(0);
}
*/
/* end of hack */



/* this function is supposed to load a players equipment from the
  plrobjs-backup directory.  Hopefully this will be useful in reimbursing
  players */
ACMD(do_reimburse)
{
  struct char_data *vict;

  half_chop(argument, arg, buf);

  if (!*arg) {
    send_to_char("Reimburse who?\r\n", ch);
    return;
  }
  if ((vict = get_char_vis(ch, arg))) {
    send_to_char("Sorry, but they have to be logged off first.\r\n", ch);
    return;
  }

  sprintf(buf, "mv ../lib/plrobjs-backup/%s.objs ../lib/plrobjs", arg);
  system(buf);
  sprintf(buf, "(GC) %s has reimbursed %s", GET_NAME(ch), arg);
  log(buf);
  send_to_char("Reimbursed.\r\n", ch);
}



ACMD(do_vwear)
{
  int i;
  int found = 0;


  one_argument(argument, buf);

  if (!*buf) {
    send_to_char("Usage: vwear <wear position>\r\n"
          "Possible positions are:", ch);
    for (i = 0; i < NUM_ITEM_WEARS; i++) {
      if ((i % 7) == 0)
        send_to_char("\r\n", ch);
      sprintf(buf, "%-9s", wear_bits[i]);
      send_to_char(buf, ch);
    }
    send_to_char("\r\n", ch);
    return;
  }

  for (i = 0; i < NUM_ITEM_WEARS; i++) {
    if (is_abbrev(buf, wear_bits[i])) {
      found = 1;
      vwear_object((1 << i), ch);
      break; 
    }
  }

  if (!found) {
    send_to_char("Usage: vwear <wear position>\r\n"
          "Possible positions are:", ch);
    for (i = 0; i < NUM_ITEM_WEARS; i++) {
      if ((i % 7) == 0)
        send_to_char("\r\n", ch);
      sprintf(buf, "%-9s", wear_bits[i]);
      send_to_char(buf, ch);
    }
    send_to_char("\r\n", ch);
  }
}



ACMD(do_vtype)
{
  int i;
  int found = 0;


  one_argument(argument, buf);

  if (!*buf) {
    send_to_char("Usage: vtype <obj type>\r\n"
          "Possible types are:", ch);
    for (i = 0; i < NUM_ITEM_TYPES; i++) {
      if ((i % 5) == 0)
        send_to_char("\r\n", ch);
      sprintf(buf, "%-14s", item_types[i]);
      send_to_char(buf, ch);
    }
    send_to_char("\r\n", ch);
    return;
  }

  for (i = 0; i < NUM_ITEM_TYPES; i++) {
    if (is_abbrev(buf, item_types[i])) {
      found = 1;
      vtype_object(i, ch);
      break;
    }
  }

  if (!found) {
    send_to_char("Usage: vtype <obj type>\r\n"
          "Possible positions are:", ch);
    for (i = 0; i < NUM_ITEM_TYPES; i++) {
      if ((i % 5) == 0)
        send_to_char("\r\n", ch);
      sprintf(buf, "%-14s", item_types[i]);
      send_to_char(buf, ch);
    }
    send_to_char("\r\n", ch);
  }
}



ACMD(do_peace)
{
  struct char_data *vict, *next_v;


  if (subcmd != SCMD_QUIET_PEACE) {
    act("$n booms out, 'PEACE!'", FALSE, ch, 0, 0,
        TO_ROOM);
    send_to_room("Everything is quite peaceful now.\r\n", ch->in_room);
  }

  for (vict = world[ch->in_room].people; vict; vict = next_v) {
    next_v = vict->next_in_room;
    if (IS_NPC(vict) && (FIGHTING(vict))) {
      if (FIGHTING(FIGHTING(vict)) == vict)
        stop_fighting(FIGHTING(vict));
      stop_fighting(vict);
    }
  }
}



/* crash bug somewhere in here */
/*ACMD(do_players)
{
  int i, count = 0;
  *buf = 0;

  for (i = 0; i <= top_of_p_table; i++) {
    sprintf(buf, "%s  %-20.20s", buf, (player_table + i)->name);
    count++;
    if (count == 3) {
      count = 0;
      strcat(buf, "\r\n");
    }
  }
  page_string(ch->desc, buf, 1);
}
*/


ACMD(do_players2)
{
  int i, count = 0;


  *buf = '\0';

  for (i = 0; i <= top_of_p_table; i++) {
    sprintf(buf, "%s  %-20.20s %d", buf, (player_table + i)->name, i);
    count++;
    if (count == 3) {
      count = 0;
      strcat(buf, "\r\n");
    }
    send_to_char(buf, ch);
    *buf = '\0';
  }
}

/*

void do_otransfer( struct char_data *ch, char *argument )
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    struct obj_data *object;
    struct char_data *vict;
    struct char_data *victim;
    struct room_data *chroom;
    struct room_data *objroom;

    argument = one_argument( argument, arg1 );
    argument = one_argument( argument, arg2 );
 
    if ( arg1[0] == '\0' )
    {
        send_to_char( "Otransfer which object?\n\r", ch );
        return;
    }

    if ( arg2[0] == '\0' ) victim = ch;
    else if (!(vict = get_char_vis(ch, arg2)))
    {
	send_to_char( "They aren't here.\n\r", ch );
	return;
    }

    if (!(object = get_obj_vis(ch, arg1)))
    {
	send_to_char( "Nothing like that in hell, earth, or heaven.\n\r", ch );
	return;
    }

    if (object->carried_by != NULL)
    {
	act("$p vanishes from your hands in an explosion of energy.",object->carried_by,object,NULL,TO_CHAR);
*/
/*    act("$n booms out, 'PEACE!'", FALSE, ch, 0, 0, TO_ROOM);
    act("You get $p.", FALSE, ch, obj, 0, TO_CHAR);
    act("$n gets $p.", TRUE, ch, obj, 0, TO_ROOM); */
/*

	act("$p vanishes from $n's hands in an explosion of energy.", FALSE, victim, object,0,TO_ROOM);
	obj_from_char(object);
    }
    else if (object->in_obj     != NULL) obj_from_obj(object);
    else if (object->in_room != NULL)
    {
    	chroom = ch->in_room;
    	objroom = obj->in_room;
    	char_from_room(ch);
    	char_to_room(ch,objroom);
    	act("$p vanishes from the ground in an explosion of energy.",ch,obj,NULL,TO_ROOM);
	if (chroom == objroom) act("$p vanishes from the ground in an explosion of energy.",ch,obj,NULL,TO_CHAR);
    	char_from_room(ch);
    	char_to_room(ch,chroom);
	obj_from_room(obj);
    }
    else
    {
	send_to_char( "You were unable to get it.\n\r", ch );
	return;
    }
    obj_to_char(obj,victim);
    act("$p appears in your hands in an explosion of energy.",victim,obj,NULL,TO_CHAR);
    act("$p appears in $n's hands in an explosion of energy.",victim,obj,NULL,TO_ROOM);
    return;
}
*/

ACMD(do_pretitle) {
  struct char_data *vict;
  char vict_name[MAX_INPUT_LENGTH], new_title[MAX_TITLE_LENGTH];
  int title_len;
  
  half_chop(argument, vict_name, buf);
  if (!*vict_name || !*buf) {
    send_to_char("Usage: pretitle <player> <text>\r\n", ch);
    return;
  }
  vict = get_player_vis(ch, vict_name);
  if (!vict) {
    send_to_char("Your target isn't around!\r\n", ch);
    return;
  }
  
  strncpy(new_title, buf, MAX_TITLE_LENGTH);
  title_len = strlen(new_title);
  if (title_len >= MAX_TITLE_LENGTH-2) {
    send_to_char("That's WAY too long!\r\n", ch);
    return;
  }
  
  new_title[title_len++] = PRETITLE_SEP_CHAR;
  strncpy(new_title + title_len, GET_TITLE(vict), MAX_TITLE_LENGTH - title_len);
  new_title[MAX_TITLE_LENGTH-1] = '\000';
  sprintf(buf, "New title is %s.\r\n", new_title);
  send_to_char(buf, ch);
  
  set_title(vict, new_title);
  
  return;
}

ACMD(do_loadmoney) {
  int amount;
  struct obj_data *obj;
  
  one_argument(argument, buf);
  
  if (!buf) {
    send_to_char("Usage: loadmoney <amount>\r\n", ch);
    return;
  }
  
  amount = atoi(buf);
  
  if (amount == 0) {
    send_to_char("Now what would THAT accomplish?\r\n", ch);
    return;
  }
  
  if (amount < 0) {
    send_to_char("I know the economy is bad, but...NEGATIVE money?\r\n", ch);
    return;
  }
  
  obj = create_money(amount);
  obj_to_room(obj, ch->in_room);
  act("$n makes a strange magical gesture.", TRUE, ch, 0, 0, TO_ROOM);
  act("$n has created $p!", FALSE, ch, obj, 0, TO_ROOM);
  act("You create $p.", FALSE, ch, obj, 0, TO_CHAR);
  
  sprintf(buf3, "(GC) %s has loaded %d coins.", GET_NAME(ch), amount);
  mudlog(buf3, CMP, MAX(LVL_GRGOD, GET_INVIS_LEV(ch)), TRUE);
  
  return;
}

#define ZCMD zone_table[zone].cmd[cmd_no]

ACMD(do_vload) {
  int zone, cmd_no, mode, rnum;
  int last_room_rnum = 0;
  const int objects = 1;
  const int mobs = 2;
  
  if (IS_NPC(ch)) {
    send_to_char ("Go away, silly monster!\r\n", ch);
    return;
  }

  two_arguments(argument, buf, buf2);
  
  if (!*buf2 || (strcmp(buf, "mob") && strcmp(buf, "obj"))) {
    send_to_char("Usage: vload mob|obj vnum\r\n", ch);
    return;
  }
  
  if (!strcmp(buf, "mob")) mode = mobs; else mode = objects;
  if (mode == objects) rnum = real_object(atoi(buf2));
    else rnum = real_mobile(atoi(buf2));
  *buf = '\0';
  
  for (zone = 0; zone <= top_of_zone_table; zone++) {
    for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++) {
      switch (ZCMD.command) {
      case 'M':
        last_room_rnum = ZCMD.arg3;
        if (mode == mobs && ZCMD.arg1 == rnum) {
          sprintf(buf, "%s[%d] - %s\r\n", buf, world[last_room_rnum].number,
            world[last_room_rnum].name);
        }
        break;
        
      case 'O':
        last_room_rnum = ZCMD.arg3;
      case 'G':
      case 'E':
      case 'P':
        if (mode == objects) {
          if (ZCMD.arg1 == rnum) {
            sprintf(buf, "%s[%d] - %s\r\n", buf, world[last_room_rnum].number,
              world[last_room_rnum].name);
          }
        }
        break;
        
      case 'D':
      case 'R':
        break;
      }
    }
  }

  page_string(ch->desc, buf, 1);
}

ACMD(do_rename) {
  extern struct obj_data *obj_proto;
  struct obj_data *obj, *realobj;
  char **objfield, *original;

  if (!*argument) {
    send_to_char("Usage: rename <object> <field> <text>\r\n", ch);
    send_to_char("Where <field> is one of: name aliases inroom\r\n", ch);
    return;
  }
  
/*** First, find the object to rename ***/

  argument = one_argument(argument, buf);
  obj = get_obj_in_list_vis(ch, buf, ch->carrying);
  if (!obj) obj = get_obj_in_list_vis(ch, buf, world[ch->in_room].contents);

  if (!obj) {
    sprintf(buf2, "I don't see anything resembling a %s around here, do you?\r\n", buf);
    send_to_char(buf2, ch);
    return;
  }

  if (obj->item_number < 0) {
    sprintf(buf2, "Sorry, but you can't rename %s.\r\n", obj->short_description);
    send_to_char(buf2, ch);
    return;
  }
  
  realobj = &obj_proto[obj->item_number];
  
/*** Now, find the field to change ***/

  argument = one_argument(argument, buf);
  
  if (!*buf) {
    send_to_char("Which field are you trying to change?\r\n", ch);
    return;
  }
  
  if (!strcmp(buf, "name")) {
    objfield = &(obj->short_description);
    original = realobj->short_description;
  } else if (!strcmp(buf, "aliases")) {
    objfield = &(obj->name);
    original = realobj->name;
  } else if (!strcmp(buf, "inroom")) {
    objfield = &(obj->description);
    original = realobj->description;
  } else {
    sprintf(buf2, "Unknown field: %s\r\n", buf);
    send_to_char(buf2, ch);
    return;
  }
  
/*** And get the text that we're changing it to ***/

  skip_spaces(&argument);
  
  if (!*argument) {
    send_to_char("Change the text to what?\r\n", ch);
    return;
  }
  
/* At this point, obj is the object to edit, *objfield is the current
 * text, original is the prototype's text, and argument is the text
 * we want.
 */
 
  if (*objfield != original) free(*objfield);
  *objfield = strdup(argument);
  
  send_to_char("Done.\r\n", ch);
}

ACMD(do_arena) {
  extern int arena_base;
  int new_base;

  one_argument(argument, buf);
  
  if (!*buf) {
    send_to_char("What room does the arena start at?\r\n", ch);
    return;
  }
  
  new_base = atoi(buf);
  if (new_base < 1) {
    send_to_char("Syntax : arena <base room>\r\n", ch);
    return;
  }
  
  arena_base = new_base;
  sprintf(buf, "The arena now runs between rooms %d and %d.\r\n", arena_base,
    arena_base+99);
  send_to_char(buf, ch);
  
  return;
}

ACMD(do_questify) {
  struct obj_data *obj;
  one_argument(argument, buf);
  
  if (!*buf) {
    send_to_char("Questify WHAT?\r\n", ch);
    return;
  }
  
  obj = get_obj_in_list_vis(ch, buf, ch->carrying);
  if (!obj) {
    send_to_char("You don't seem to have that.\r\n", ch);
    return;
  }
  
  SET_BIT(obj->obj_flags.extra_flags, ITEM_QUEST);

  send_to_char("Ok.\r\n", ch);
  return;
}

#define  W_EXIT(room, num) (world[(room)].dir_option[(num)])

ACMD(do_dig) {
  int dir, cur, vnum, rnum;
  int i, j, room_num, found = 0, zone, cmd_no;
  struct room_data *new_world, new_room;
  struct char_data *temp_ch;
  struct obj_data *temp_obj;
  extern const char *dir_abbrevs[];
  extern sh_int r_mortal_start_room;
  extern sh_int r_immort_start_room;
  extern sh_int r_frozen_start_room;
  extern sh_int r_race_start_room[NUM_RACES];
  extern sh_int r_lowbie_start_room;
  extern sh_int r_clan_start_room[NUM_CLANS];
  extern const int rev_dir[];
  
  void renum_storage_rooms();
  void renum_restricted_rooms();
 
  if (IS_NPC(ch)) { /* Yeah, whatever */
    send_to_char("Huh?!?\r\n", ch);
    return;
  }
  
  argument = one_argument(argument, buf);

  if (!*buf) {
    send_to_char("Dig in which direction?\r\n", ch);
    return;
  }
  
  for (dir = 0; dir < SOMEWHERE; dir++) {
    if (!strcmp(dir_abbrevs[dir], buf)) break;
  }

  if (dir == SOMEWHERE) {
    send_to_char("Dig WHERE?!?\r\n", ch);
    return;
  }
  
  cur = world[ch->in_room].number;
  zone = cur / 100;
  
  if (GET_LEVEL(ch) < LVL_DEITY) {
    if (zone != GET_OLC_ZONE(ch) - 1) {
      send_to_char("You do not have permission to edit this zone!\r\n", ch);
      return;
    }
  }
  
  /* Now, find a vnum in the zone that's not being used */
  for (vnum = zone * 100; vnum < (zone + 1) * 100; vnum++) {
    if (real_room(vnum) == -1) break;
  }
  if (vnum == (zone + 1) * 100) {
    send_to_char("This zone's full!\r\n", ch);
    return;
  }
  
  /* Final check: Is there already an exit in the chosen direction? */
  if (EXIT(ch, dir)) {
    send_to_char("There's already an exit that way!\r\n", ch);
    return;
  }

  /* Ok, all's clear. Make the room, setup the exits, and move the builder */
  
  /* First, make the room */
  new_room.number = vnum;
  new_room.zone = zone;
  new_room.sector_type = 0;
  new_room.name = strdup("An unfinished room.");
  new_room.description = strdup("It looks unfinished.\n");
  new_room.ex_description = NULL;
  for (i = 0; i < NUM_OF_DIRS; i++) new_room.dir_option[i] = NULL;
  new_room.room_flags = 0;
  new_room.light = 0;
  new_room.func = NULL;
  new_room.contents = NULL;
  new_room.people = NULL;
  /* COPIED from redit.c */
  CREATE(new_world, struct room_data, top_of_world + 2);

  olc_add_to_save_list(zone, OLC_SAVE_ROOM);

  /* count thru world tables */
  for (i = 0; i <= top_of_world; i++) { 
    if (!found) {
      /*. Is this the place? .*/
      if (world[i].number > vnum) {
        found = 1;

        new_world[i] = new_room;
        room_num  = i;
	
        /* copy from world to new_world + 1 */
        new_world[i + 1] = world[i];
        /* people in this room must have their numbers moved */
        for (temp_ch = world[i].people; temp_ch; temp_ch = temp_ch->next_in_room)
         if (temp_ch->in_room != -1)
           temp_ch->in_room = i + 1;

        /* move objects */
        for (temp_obj = world[i].contents; temp_obj; temp_obj = temp_obj->next_content)
          if (temp_obj->in_room != -1)
            temp_obj->in_room = i + 1;
	  
      } else { 
        /*.   Not yet placed, copy straight over .*/
        new_world[i] = world[i];
      }
    } else { 
      /*. Already been found  .*/
 
      /* people in this room must have their in_rooms moved */
      for (temp_ch = world[i].people; temp_ch; temp_ch = temp_ch->next_in_room)
        if (temp_ch->in_room != -1)
          temp_ch->in_room = i + 1;

      /* move objects */
      for (temp_obj = world[i].contents; temp_obj; temp_obj = temp_obj->next_content)
        if (temp_obj->in_room != -1)
          temp_obj->in_room = i + 1;

      new_world[i + 1] = world[i];
    }
  }
  if (!found) {
    /*. Still not found, insert at top of table .*/
    new_world[i] = new_room;
    room_num  = i;
  }

  /* copy world table over */
  free(world);
  world = new_world;
  top_of_world++;

  /*. Update zone table .*/
  for (zone = 0; zone <= top_of_zone_table; zone++)
    for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++)
      switch (ZCMD.command) {
        case 'M':
        case 'O':
          if (ZCMD.arg3 >= room_num)
            ZCMD.arg3++;
            break;
        case 'D':
        case 'R':
          if (ZCMD.arg1 >= room_num)
            ZCMD.arg1++;
        case 'G':
        case 'P':
        case 'E':
        case '*':
          break;
        default:
          mudlog("SYSERR: OLC: dig: Unknown comand", BRF, LVL_BUILDER, TRUE);
      }

  /* update load rooms, to fix creeping load room problem */
  if (room_num <= r_mortal_start_room)
    r_mortal_start_room++;
  if (room_num <= r_immort_start_room)
    r_immort_start_room++;
  if (room_num <= r_frozen_start_room)
    r_frozen_start_room++;
  for (i = 0; i < NUM_RACES; i++)
    if (room_num <= r_race_start_room[i])
      r_race_start_room[i]++;
  if (room_num <= r_lowbie_start_room)
    r_lowbie_start_room++;
  for (i = 0; i < NUM_CLANS; i++)
    if (room_num <= r_clan_start_room[i])
      r_clan_start_room[i]++;
  /*. Update world exits .*/
  for (i = 0; i < top_of_world + 1; i++)
    for (j = 0; j < NUM_OF_DIRS; j++)
      if (W_EXIT(i, j))
        if (W_EXIT(i, j)->to_room >= room_num)
          W_EXIT(i, j)->to_room++;

  /* move storage and restricted rooms */
  renum_storage_rooms();
  renum_restricted_rooms();
  
  /* Second, make the exits */
  rnum = real_room(vnum);
  CREATE(W_EXIT(ch->in_room, dir), struct room_direction_data, 1);
  W_EXIT(ch->in_room, dir)->to_room = rnum;
  CREATE(W_EXIT(rnum, rev_dir[dir]), struct room_direction_data, 1);
  W_EXIT(rnum, rev_dir[dir])->to_room = ch->in_room;
  
  /* Finally, move the builder! */
  sprintf(buf, "You dig %s.\r\n", dirs[dir]);
  send_to_char(buf, ch);
  sprintf(buf, "$n digs %s.", dirs[dir]);
  act(buf, TRUE, ch, 0, 0, TO_ROOM);
  char_from_room(ch);
  char_to_room(ch, rnum);
  look_at_room(ch, 0);
}

ACMD(do_balefire) {
}

ACMD(do_smite)
{
  struct char_data *victim;

  one_argument(argument, buf);
  if (!*argument) {
    send_to_char("Whom do you wish to smite?\r\n", ch);
    return;
  }

  if (GET_LEVEL(ch) < LVL_GRGOD) {
      send_to_char("I think not.\r\n", ch);
      return;
  }

  if (!(victim = get_char_vis(ch, buf)))
      send_to_char(NOPERSON, ch);
  else if (victim == ch)
      send_to_char("That doesn't make much sense, does it?\r\n", ch);
  else {
      if ((GET_LEVEL(ch) < GET_LEVEL(victim)) && !IS_NPC(victim)) {
        send_to_char("I don't think that would be wise.\r\n", ch);
        return;
      }
      act("^C$n is struck by a brilliant bolt of energy!^n",
          FALSE, victim, 0, 0, TO_ROOM);
      act("^CYour body is wracked in pain as a brilliant bolt of energy strikes you!^n", FALSE, ch, 0, victim, TO_VICT);
      GET_HIT(victim) = GET_HIT(victim) / 2;
      if (GET_HIT(victim) == 0)
	GET_HIT(victim) = 1;
      GET_MANA(victim) = GET_MANA(victim) / 2;
      if (GET_MANA(victim) == 0)
	GET_MANA(victim) = 1;
      GET_MOVE(victim) = GET_MOVE(victim) / 2;
      if (GET_MOVE(victim) == 0)
	GET_MOVE(victim) = 1;
      act("^CYou send a brilliant bolt of energy to strike $N.", FALSE, ch, 0, victim, TO_CHAR);
  }
}

ACMD(do_check_gold)
{
  struct char_data *mob, *next_mob;

  for (mob = character_list; mob; mob = next_mob) {
    next_mob = mob->next;
      if (IS_NPC(mob)
	&& (ZONE_FLAGGED(mob->in_room, ZONE_ACTIVE))
	&& !MOB_FLAGGED(mob, MOB_SHOPKEEPER)
	&& !MOB_FLAGGED(mob, MOB_DSHOPKEEPER) ) {

  if (GET_GOLD(mob) > GET_LEVEL(mob) * 200) {
	  sprintf(buf, "%s [%d] has %d.\n\r", GET_NAME(mob), GET_MOB_VNUM(mob), GET_GOLD(mob));
	  send_to_char(buf, ch);
      }
    }
  }
}

ACMD(do_check_values) {
  struct obj_data *obj;
  int minval;
  extern struct obj_data *obj_proto;
  
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char("Specify a minimum price!\r\n", ch);
    return;
  }
  minval = atoi(argument);
  

  for (obj = obj_proto; obj; obj = obj->next) {
    if (GET_OBJ_COST(obj) >= minval && !IS_OBJ_STAT(obj, ITEM_PRICEOK)) {
      sprintf(buf, "[%-5d] %-7d: %s\r\n", GET_OBJ_COST(obj), GET_OBJ_VNUM(obj),
              obj->short_description);
    }
  }
  send_to_char(buf, ch);
}


ACMD(do_findaff2) {
  int num;
  struct obj_data *k;
  int found;
  extern void print_object_location(int num, struct obj_data * obj,
      struct char_data * ch, int recur);

  for (num = 0, k = object_list; k; k = k->next) {
    if (CAN_SEE_OBJ(ch, k) && (k->obj_flags.bitvector2 > 0)) {
      found = 1;
      print_object_location(++num, k, ch, TRUE);
    }
  }
  if (!found)
    send_to_char("Couldn't find any such thing.\r\n", ch);
}

#define MSEARCH_USAGE \
  "Usage: msearch [-k keywords] [-l minlevel[-maxlevel]] [-m mobbits]\r\n" \
  "               [-p permbits] [-z zone]\r\n"

#define MSEARCH_NONE            (-1)
#define MSEARCH_KEYWORDS        (0)
#define MSEARCH_LEVEL           (1)
#define MSEARCH_MOBBITS         (2)
#define MSEARCH_PERMBITS        (3)
#define MSEARCH_ZONE            (4)

/* overflow */
#define MSEARCH_OVERFLOW       (512)

ACMD(do_msearch) {
  skip_spaces(&argument);
  if (*argument == '\0') {
    send_to_char(MSEARCH_USAGE, ch);
  } else {
    size_t count = 0;
    unsigned long crMobBits = 0;
    char crKeywords[MAX_INPUT_LENGTH] = {'\0'};
    int crLevelMin = 1;
    int crLevelMax = LVL_IMMORT - 1;
    unsigned long crPermBits = 0;
    int crZone = NOWHERE;
    ssize_t lookup = 0;
    int opt = MSEARCH_NONE;

    while (argument && *argument != '\0') {
      char word[MAX_INPUT_LENGTH];
      argument = any_one_arg(argument, word);
      if (*word == '-') {
        switch (word[1]) {
        case 'k':
        case 'K':
          opt = MSEARCH_KEYWORDS;
          break;
        case 'l':
        case 'L':
          opt = MSEARCH_LEVEL;
          break;
        case 'm':
        case 'M':
          opt = MSEARCH_MOBBITS;
          break;
        case 'p':
        case 'P':
          opt = MSEARCH_PERMBITS;
          break;
        case 'z':
        case 'Z':
          opt = MSEARCH_ZONE;
          break;
        default:
          send_to_char(MSEARCH_USAGE, ch);
          argument = NULL;
        }
      } else if (*word != '\0') {
        switch (opt) {
        case MSEARCH_NONE:
          send_to_char(MSEARCH_USAGE, ch);
          argument = NULL;
          break;
        case MSEARCH_KEYWORDS:
          if (*crKeywords != '\0') {
            const size_t len = strlen(crKeywords);
            snprintf(crKeywords + len, sizeof(crKeywords) - len, " ");
          }
          do {
            const size_t len = strlen(crKeywords);
            snprintf(crKeywords + len, sizeof(crKeywords) - len, "%s", word);
          } while (0);
          break;
        case MSEARCH_LEVEL:
          lookup = sscanf(word, "%d-%d", &crLevelMin, &crLevelMax);
          if (lookup == 1) {
            crLevelMax = crLevelMin;
          } else if (lookup == 2) {
            if (crLevelMax < crLevelMin) {
              lookup = crLevelMax;
              crLevelMax = crLevelMin;
              crLevelMin = lookup;
            }
          } else {
            argument = NULL;
          }
          opt = MSEARCH_NONE;
          break;
        case MSEARCH_MOBBITS:
          if ((lookup = search_block(word, action_bits, FALSE)) < 0) {
            send_to_char(MSEARCH_USAGE, ch);
            argument = NULL;
          } else {
            crMobBits |= (1 << lookup);
          }
          break;
        case MSEARCH_PERMBITS:
          if ((lookup = search_block(word, affected_bits, FALSE)) < 0) {
            send_to_char(MSEARCH_USAGE, ch);
            argument = NULL;
          } else {
            crPermBits |= (1 << lookup);
          }
          break;
        case MSEARCH_ZONE:
          lookup = atoi(word);
          if ((crZone = real_zone(lookup)) == NOWHERE) {
            send_to_char("There is no such zone.\r\n", ch);
            argument = NULL;
          }
          opt = MSEARCH_NONE;
          break;
        }
      }
    }
    if (argument && *argument == '\0') {
      /* output buffer */
      char buf[MAX_STRING_LENGTH * 2] = {'\0'};
      register size_t buflen = 0;

      /* Loop over the object prototypes */
      register int rMobile = 0;
      for (rMobile = 0; rMobile <= top_of_mobt; ++rMobile) {
        const size_t buflenSaved = buflen;

        /* Determine which zone contains mobile */
        const int zone = real_zone_by_thing(GET_MOB_VNUM(&mob_proto[rMobile]));

        if (*crKeywords != '\0') {
          bool found = FALSE;
          register char *curtok = crKeywords;
          while (!found && curtok && *curtok != '\0') {
            char keyword[MAX_INPUT_LENGTH] = {'\0'};
            curtok = one_word(curtok, keyword);
            if (*keyword) {
              if (isname(keyword, mob_proto[rMobile].player.name))
                found = TRUE;
              if (isname(keyword, mob_proto[rMobile].player.description))
                found = TRUE;
              if (isname(keyword, mob_proto[rMobile].player.short_descr))
                found = TRUE;
              if (isname(keyword, mob_proto[rMobile].player.long_descr))
                found = TRUE;
            }
          }
          if (!found)
            continue;
        }
        if (GET_LEVEL(&mob_proto[rMobile]) < crLevelMin)
          continue;
        if (GET_LEVEL(&mob_proto[rMobile]) > crLevelMax)
          continue;
        if (crMobBits && MOB_FLAGGED(&mob_proto[rMobile], crMobBits) == 0)
          continue;
        if (crPermBits && AFF_FLAGGED(&mob_proto[rMobile], crPermBits) == 0)
          continue;
        if (crZone != NOWHERE && zone != crZone)
          continue;

        if (buflen == 0) {
          BPrintf(buf, sizeof(buf) - MSEARCH_OVERFLOW, buflen,
              "%s[X]-------------------------------------------------------------------------[X]%s\r\n"
              "%s|X|   %sVnum %sLvl  %sName                                                        %s|X|%s\r\n"
              "%s|X| ------ --- ------------------------------------------------------------ |X|%s\r\n",
              CCYEL(ch), CCNRM(ch),
              CCYEL(ch), CCWHT(ch), CCCYN(ch), CCWHT(ch), CCYEL(ch), CCNRM(ch),
              CCYEL(ch), CCNRM(ch));
        }

        /* Format the item data to the buffer */
        BPrintf(buf, sizeof(buf) - MSEARCH_OVERFLOW, buflen,
            "%s|X| %s%6d %s%3d  %s%-59.59s %s|X|%s\r\n",
            CCYEL(ch),
            CCWHT(ch), GET_MOB_VNUM(&mob_proto[rMobile]),
            CCCYN(ch), GET_LEVEL(&mob_proto[rMobile]),
            CCWHT(ch), mob_proto[rMobile].player.short_descr,
            CCYEL(ch), CCNRM(ch));

        if (buflen == buflenSaved) {
          /* Overflow message instead */
          BPrintf(buf, sizeof(buf), buflen,
              "%s|X| %s%-71.71s %s|X|%s\r\n",
              CCYEL(ch),
              CCRED(ch), "*OVERFLOW*",
              CCYEL(ch), CCNRM(ch));

          break;
        }

        ++count;
      }

      if (buflen == 0) {
        send_to_char("No mobiles were found.\r\n", ch);
      } else {
        /* format the number of results */
        char tbuf[MAX_INPUT_LENGTH] = {'\0'};
        snprintf(tbuf, sizeof(tbuf), "%zu mobile%s shown.", count, count != 1 ? "s" : "");

        /* search results footer */
        BPrintf(buf, sizeof(buf), buflen,
            "%s|X| ----------------------------------------------------------------------- |X|%s\r\n"
            "%s|X| %s%-71.71s %s|X|%s\r\n"
            "%s[X]-------------------------------------------------------------------------[X]%s\r\n",
            CCYEL(ch), CCNRM(ch),
            CCYEL(ch), CCWHT(ch), CAP(tbuf), CCYEL(ch), CCNRM(ch),
            CCYEL(ch), CCNRM(ch));

        /* Page the string to the player */
        page_string(ch->desc, buf, TRUE);
      }
    }
  }
}


#define OSEARCH_USAGE \
  "Usage: osearch [-a apply] [-k keywords] [-t type] [-p permbits]\r\n" \
  "               [-w wearbits] [-x extrabits] [-z zone]\r\n"

#define OSEARCH_NONE            (-1)
#define OSEARCH_APPLY           (0)
#define OSEARCH_KEYWORDS        (1)
#define OSEARCH_TYPE            (2)
#define OSEARCH_PERMBITS        (3)
#define OSEARCH_WEARBITS        (4)
#define OSEARCH_EXTRABITS       (5)
#define OSEARCH_ZONE            (6)

/* overflow */
#define OSEARCH_OVERFLOW       (512)

ACMD(do_osearch) {
  skip_spaces(&argument);
  if (*argument == '\0') {
    send_to_char(OSEARCH_USAGE, ch);
  } else {
    size_t count = 0;
    int crApply = APPLY_NONE;
    unsigned long crExtraBits = 0;
    char crKeywords[MAX_INPUT_LENGTH] = {'\0'};
    int crLevelMax = LVL_IMMORT - 1;
    int crLevelMin = 1;
    unsigned long crPermBits = 0;
    int crItemType = -1;
    unsigned long crWearBits = 0;
    int crZone = NOWHERE;
    ssize_t lookup = 0;
    int opt = OSEARCH_NONE;

    while (argument && *argument != '\0') {
      char word[MAX_INPUT_LENGTH];
      argument = any_one_arg(argument, word);
      if (*word == '-') {
        switch (word[1]) {
        case 'a':
        case 'A':
          opt = OSEARCH_APPLY;
          break;
        case 'k':
        case 'K':
          opt = OSEARCH_KEYWORDS;
          break;
        case 'p':
        case 'P':
          opt = OSEARCH_PERMBITS;
          break;
        case 't':
        case 'T':
          opt = OSEARCH_TYPE;
          break;
        case 'w':
        case 'W':
          opt = OSEARCH_WEARBITS;
          break;
        case 'x':
        case 'X':
          opt = OSEARCH_EXTRABITS;
          break;
        case 'z':
        case 'Z':
          opt = OSEARCH_ZONE;
          break;
        default:
          send_to_char(OSEARCH_USAGE, ch);
          argument = NULL;
        }
      } else if (*word != '\0') {
        switch (opt) {
        case OSEARCH_NONE:
          send_to_char(OSEARCH_USAGE, ch);
          argument = NULL;
          break;
        case OSEARCH_APPLY:
          if ((lookup = search_block(word, apply_types, FALSE)) < 0) {
            send_to_char("No such apply.\r\n", ch);
            argument = NULL;
          } else {
            crApply = lookup;
          }
          opt = OSEARCH_NONE;
          break;
        case OSEARCH_EXTRABITS:
          if ((lookup = search_block(word, extra_bits, FALSE)) < 0) {
            send_to_char(OSEARCH_USAGE, ch);
            argument = NULL;
          } else {
            crExtraBits |= (1 << lookup);
          }
          break;
        case OSEARCH_KEYWORDS:
          if (*crKeywords != '\0') {
            const size_t len = strlen(crKeywords);
            snprintf(crKeywords + len, sizeof(crKeywords) - len, " ");
          }
          do {
            const size_t len = strlen(crKeywords);
            snprintf(crKeywords + len, sizeof(crKeywords) - len, "%s", word);
          } while (0);
          break;
        case OSEARCH_PERMBITS:
          if ((lookup = search_block(word, affected_bits, FALSE)) < 0) {
            send_to_char(OSEARCH_USAGE, ch);
            argument = NULL;
          } else {
            crPermBits |= (1 << lookup);
          }
          break;
        case OSEARCH_TYPE:
          if ((lookup = search_block(word, item_types, FALSE)) < 0) {
            send_to_char("There is no such item type.\r\n", ch);
            argument = NULL;
          } else {
            crItemType = lookup;
            opt = OSEARCH_NONE;
          }
          break;
        case OSEARCH_WEARBITS:
          if ((lookup = search_block(word, wear_bits, FALSE)) < 0) {
            send_to_char(OSEARCH_USAGE, ch);
            argument = NULL;
          } else {
            crWearBits |= (1 << lookup);
          }
          break;
        case OSEARCH_ZONE:
          lookup = atoi(word);
          if ((crZone = real_zone(lookup)) == NOWHERE) {
            send_to_char("There is no such zone.\r\n", ch);
            argument = NULL;
          }
	  opt = OSEARCH_NONE;
          break;
        }
      }
    }
    if (argument && *argument == '\0') {
      /* output buffer */
      char buf[MAX_STRING_LENGTH * 2] = {'\0'};
      register size_t buflen = 0;

      /* Loop over the object prototypes */
      register int rObject = 0;
      for (rObject = 0; rObject <= top_of_objt; ++rObject) {
        const size_t buflenSaved = buflen;
        char bits[MAX_INPUT_LENGTH];

        /* Determine which zone contains object */
        const int zone = real_zone_by_thing(GET_OBJ_VNUM(&obj_proto[rObject]));

        if (crApply != APPLY_NONE) {
          register size_t j = (size_t) 0;
          for (j = 0; j < MAX_OBJ_AFFECT; ++j) {
            if (obj_proto[rObject].affected[j].location == crApply)
              break;
          }
          if (j == MAX_OBJ_AFFECT)
            continue;
        }
        if (crExtraBits && OBJ_FLAGGED(&obj_proto[rObject], crExtraBits) == 0)
          continue;
        if (*crKeywords != '\0') {
          bool found = FALSE;
          register char *curtok = crKeywords;
          while (!found && curtok && *curtok != '\0') {
            char keyword[MAX_INPUT_LENGTH] = {'\0'};
            curtok = one_word(curtok, keyword);
            if (*keyword) {
              if (isname(keyword, obj_proto[rObject].name))
                found = TRUE;
              if (isname(keyword, obj_proto[rObject].action_description))
                found = TRUE;
              if (isname(keyword, obj_proto[rObject].description))
                found = TRUE;
              if (isname(keyword, obj_proto[rObject].short_description))
                found = TRUE;
            }
          }
          if (!found)
            continue;
        }
        if (crPermBits && OBJAFF_FLAGGED(&obj_proto[rObject], crPermBits) == 0)
          continue;
        if (crItemType != -1 && GET_OBJ_TYPE(&obj_proto[rObject]) != crItemType)
          continue;
        if (crWearBits && OBJWEAR_FLAGGED(&obj_proto[rObject], crWearBits) == 0)
          continue;
        if (crZone != NOWHERE && zone != crZone)
          continue;
        if (buflen == 0) {
          BPrintf(buf, sizeof(buf) - OSEARCH_OVERFLOW, buflen,
              "%s[X]-------------------------------------------------------------------------[X]%s\r\n"
              "%s|X|   %sVnum %sType          %sName                                               %s|X|%s\r\n"
              "%s|X| ------ ------------- -------------------------------------------------- |X|%s\r\n",
              CCYEL(ch), CCNRM(ch),
              CCYEL(ch), CCWHT(ch), CCMAG(ch), CCWHT(ch), CCYEL(ch), CCNRM(ch),
              CCYEL(ch), CCNRM(ch));
        }
        /* Format the type of item */
        sprinttype(GET_OBJ_TYPE(&obj_proto[rObject]), item_types, bits);

	/* Object name */
	char objname[MAX_INPUT_LENGTH] = {'\0'};
	strlcpy(objname, IF_STR(obj_proto[rObject].short_description), sizeof(objname));
	strip_color(objname);
	register size_t objnamelen = strlen(objname);

	/* Basic object information */
	switch (GET_OBJ_TYPE(&obj_proto[rObject])) {
	case ITEM_ARMOR:
	  BPrintf(objname, sizeof(objname), objnamelen, " (%dac)",
		GET_OBJ_VAL(&obj_proto[rObject], 0));
	  break;
	case ITEM_FIREWEAPON:
	case ITEM_WEAPON:
	  BPrintf(objname, sizeof(objname), objnamelen, " (%dd%d)",
		GET_OBJ_VAL(&obj_proto[rObject], 1),
		GET_OBJ_VAL(&obj_proto[rObject], 2));
	  break;
	}

	/* Object effects */
	register size_t k = 0;
	for (k = 0; k < MAX_OBJ_AFFECT; ++k) {
	  if (obj_proto[rObject].affected[k].location == APPLY_NONE)
	    continue;
	  if (obj_proto[rObject].affected[k].modifier == 0)
	    continue;
	  char applyname[MAX_INPUT_LENGTH] = {'\0'};
	  sprinttype(obj_proto[rObject].affected[k].location, apply_types, applyname);
	  BPrintf(objname, sizeof(objname), objnamelen, " %+d %s",
		obj_proto[rObject].affected[k].modifier,
		applyname);
	}

        /* Format the item data to the buffer */
        BPrintf(buf, sizeof(buf) - OSEARCH_OVERFLOW, buflen,
            "%s|X| %s%6d %s%-13.13s %s%-50.50s %s|X|%s\r\n",
            CCYEL(ch),
            CCWHT(ch), GET_OBJ_VNUM(&obj_proto[rObject]),
            CCMAG(ch), bits,
            CCWHT(ch), *objname != '\0' ? objname : "<Undefined>",
            CCYEL(ch), CCNRM(ch));

        if (buflen == buflenSaved) {
          /* Overflow message instead */
          BPrintf(buf, sizeof(buf), buflen,
              "%s|X| %s%-71.71s %s|X|%s\r\n",
              CCYEL(ch),
              CCRED(ch), "*OVERFLOW*",
              CCYEL(ch), CCNRM(ch));

          break;
        }
        ++count;
      }

      if (buflen == 0) {
        send_to_char("No objects were found.\r\n", ch);
      } else {
        /* format the number of results */
        char tbuf[MAX_INPUT_LENGTH] = {'\0'};
        snprintf(tbuf, sizeof(tbuf), "%zu object%s shown.", count, count != 1 ? "s" : "");

        /* search results footer */
        BPrintf(buf, sizeof(buf), buflen,
		"%s|X| ----------------------------------------------------------------------- |X|%s\r\n"
		"%s|X| %s%-71.71s %s|X|%s\r\n"
		"%s[X]-------------------------------------------------------------------------[X]%s\r\n",
		CCYEL(ch), CCNRM(ch),
		CCYEL(ch), CCWHT(ch), CAP(tbuf), CCYEL(ch), CCNRM(ch),
		CCYEL(ch), CCNRM(ch));

        /* Page the string to the player */
        page_string(ch->desc, buf, TRUE);
      }
    }
  }
}


#define RSEARCH_USAGE \
  "Usage: rsearch [-k keywords] [-l linkage] [-r roombits]\r\n" \
  "               [-s sector] [-z zone]\r\n"

#define RSEARCH_NONE            (-1)
#define RSEARCH_KEYWORDS        (0)
#define RSEARCH_LINKAGE         (1)
#define RSEARCH_ROOMBITS        (2)
#define RSEARCH_SECTOR          (3)
#define RSEARCH_ZONE            (4)

/* overflow */
#define RSEARCH_OVERFLOW       (512)

ACMD(do_rsearch) {
  skip_spaces(&argument);
  if (*argument == '\0') {
    send_to_char(RSEARCH_USAGE, ch);
  } else {
    size_t count = 0;
    int crLinkage = NOWHERE;
    unsigned long crRoomBits = 0;
    int crSector = -1;
    char crKeywords[MAX_INPUT_LENGTH] = {'\0'};
    int crZone = NOWHERE;
    ssize_t lookup = 0;
    int opt = RSEARCH_NONE;

    while (argument && *argument != '\0') {
      char word[MAX_INPUT_LENGTH] = {'\0'};
      argument = any_one_arg(argument, word);
      if (*word == '-') {
        switch (*(word + 1)) {
        case 'k':
        case 'K':
          opt = RSEARCH_KEYWORDS;
          break;
        case 'l':
        case 'L':
          opt = RSEARCH_LINKAGE;
          break;
        case 'r':
        case 'R':
          opt = RSEARCH_ROOMBITS;
          break;
        case 's':
        case 'S':
          opt = RSEARCH_SECTOR;
          break;
        case 'z':
        case 'Z':
          opt = RSEARCH_ZONE;
          break;
        default:
          send_to_char(RSEARCH_USAGE, ch);
          argument = NULL;
        }
      } else if (*word != '\0') {
        switch (opt) {
        case RSEARCH_NONE:
          send_to_char(RSEARCH_USAGE, ch);
          argument = NULL;
          break;
        case RSEARCH_KEYWORDS:
          if (*crKeywords != '\0') {
            const size_t len = strlen(crKeywords);
            snprintf(crKeywords + len, sizeof(crKeywords) - len, " ");
          }
          do {
            const size_t len = strlen(crKeywords);
            snprintf(crKeywords + len, sizeof(crKeywords) - len, "%s", word);
          } while (0);
          break;
        case RSEARCH_LINKAGE:
          lookup = atoi(word);
          if (real_room(lookup) == NOWHERE) {
            send_to_char("There is no such room.\r\n", ch);
            argument = NULL;
          }
          crLinkage = lookup;
          opt = RSEARCH_NONE;
          break;
        case RSEARCH_ROOMBITS:
          if ((lookup = search_block(word, room_bits, FALSE)) < 0) {
            send_to_char(RSEARCH_USAGE, ch);
            argument = NULL;
          } else {
            crRoomBits |= (1 << lookup);
          }
          break;
        case RSEARCH_SECTOR:
          if ((lookup = search_block(word, sector_types, FALSE)) < 0) {
            send_to_char("No such sector type.\r\n", ch);
            argument = NULL;
          } else {
            crSector = lookup;
          }
          opt = RSEARCH_NONE;
          break;
        case RSEARCH_ZONE:
          lookup = atoi(word);
          if ((crZone = real_zone(lookup)) == NOWHERE) {
            send_to_char("There is no such zone.\r\n", ch);
            argument = NULL;
          }
          opt = RSEARCH_NONE;
          break;
        }
      }
    }
    if (argument && *argument == '\0') {
      /* output buffer */
      char buf[MAX_STRING_LENGTH * 2] = {'\0'};
      register size_t buflen = 0;

      /* Loop over the rooms */
      register int rRoom = 0;
      for (rRoom = 0; rRoom <= top_of_world; ++rRoom) {
        const size_t buflenSaved = buflen;
        char bits[MAX_INPUT_LENGTH];
        if (crLinkage != NOWHERE) {
          register size_t door = (size_t) 0;
          for (door = 0; door < NUM_OF_DIRS; ++door) {
            if (REXIT(rRoom, door) &&
		REXIT(rRoom, door)->to_room != NOWHERE &&
                world[REXIT(rRoom, door)->to_room].number == crLinkage) {
              break;
            }
          }
          if (door == NUM_OF_DIRS)
            continue;
        }
        if (*crKeywords != '\0') {
          bool found = FALSE;
          register char *curtok = crKeywords;
          while (!found && curtok && *curtok != '\0') {
            char keyword[MAX_INPUT_LENGTH];
            curtok = one_word(curtok, keyword);
            if (*keyword) {
              if (isname(keyword, world[rRoom].name))
                found = TRUE;
              if (isname(keyword, world[rRoom].description))
                found = TRUE;
            }
          }
          if (found == FALSE)
            continue;
        }
        if (crRoomBits && ROOM_FLAGGED(rRoom, crRoomBits) == 0)
          continue;
        if (crSector != -1 && world[rRoom].sector_type != crSector)
          continue;
        if (crZone != NOWHERE && world[rRoom].zone != crZone)
          continue;
        if (buflen == 0) {
          BPrintf(buf, sizeof(buf) - RSEARCH_OVERFLOW, buflen,
              "%s[X]-------------------------------------------------------------------------[X]%s\r\n"
              "%s|X|   %sVnum %sSector      %sName                                                 %s|X|%s\r\n"
              "%s|X| ------ ----------- ---------------------------------------------------- |X|%s\r\n",
              CCYEL(ch), CCNRM(ch),
              CCYEL(ch), CCWHT(ch), CCCYN(ch), CCWHT(ch), CCYEL(ch), CCNRM(ch),
              CCYEL(ch), CCNRM(ch));
        }
        /* Format the room sector */
        sprinttype(world[rRoom].sector_type, sector_types, bits);

        /* Format the room data to the buffer */
        BPrintf(buf, sizeof(buf) - RSEARCH_OVERFLOW, buflen,
            "%s|X| %s%6d %s%-11.11s %s%-52.52s %s|X|%s\r\n",
            CCYEL(ch),
            CCWHT(ch), world[rRoom].number,
            CCCYN(ch), bits,
            CCWHT(ch), world[rRoom].name,
            CCYEL(ch), CCNRM(ch));

       if (buflen == buflenSaved) {
         /* Overflow message instead */
         BPrintf(buf, sizeof(buf), buflen,
		"%s|X| %s%-71.71s %s|X|%s\r\n",
		CCYEL(ch),
		CCRED(ch), "*OVERFLOW*",
		CCYEL(ch), CCNRM(ch));

	  break;
	}
	count++;
      }

      if (buflen == 0) {
	send_to_char("No rooms were found.\r\n", ch);
      } else {
	/* format the number of results */
	char tbuf[MAX_INPUT_LENGTH] = {'\0'};
	snprintf(tbuf, sizeof(tbuf), "%zu room%s shown.", count, count != 1 ? "s" : "");

	/* search results footer */
	BPrintf(buf, sizeof(buf), buflen,
		"%s|X| ----------------------------------------------------------------------- |X|%s\r\n"
		"%s|X| %s%-71.71s %s|X|%s\r\n"
		"%s[X]-------------------------------------------------------------------------[X]%s\r\n",
		CCYEL(ch), CCNRM(ch),
		CCYEL(ch), CCWHT(ch), CAP(tbuf), CCYEL(ch), CCNRM(ch),
		CCYEL(ch), CCNRM(ch));

	/* Page the string to the player */
	page_string(ch->desc, buf, TRUE);
      }
    }
  }
}
