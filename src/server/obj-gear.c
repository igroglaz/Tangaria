/*
 * File: obj-gear.c
 * Purpose: Management of inventory, equipment and quiver
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2014 Nick McConnell
 * Copyright (c) 2022 MAngband and PWMAngband Developers
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */


#include "s-angband.h"


/*
 * Gets a slot of the given type, preferentially empty unless full is true
 */
static int slot_by_type(struct player *p, int type, bool full)
{
    struct player_body body = (p? p->body: bodies[0]);
    int i, fallback = body.count;

    /* Look for a correct slot type */
    for (i = 0; i < body.count; i++)
    {
        if (type == body.slots[i].type)
        {
            /* Found a full slot */
            if (full)
            {
                if (body.slots[i].obj != NULL) break;
            }

            /* Found an empty slot */
            else
            {
                if (body.slots[i].obj == NULL) break;
            }

            /* Not right for full/empty, but still the right type */
            if (fallback == body.count) fallback = i;
        }
    }

    /* Index for the best slot we found, or p->body.count if none found  */
    return ((i != body.count)? i :fallback);
}


/*
 * Indicate whether a slot is of a given type.
 *
 * p is the player to test; if NULL, will assume the default body plan.
 * slot is the slot index for the player.
 * type is one of the EQUIP_* constants from list-equip-slots.h.
 *
 * Return true if the slot can hold that type; otherwise false
 */
bool slot_type_is(struct player *p, int slot, int type)
{
    /* Assume default body if no player */
    struct player_body body = (p? p->body: bodies[0]);

    return ((body.slots[slot].type == type)? true: false);
}


bool object_is_carried(struct player *p, const struct object *obj)
{
    return pile_contains(p->gear, obj);
}


/*
 * Check if an object is in the quiver
 */
bool object_is_in_quiver(struct player *p, const struct object *obj)
{
    int i;

    for (i = 0; i < z_info->quiver_size; i++)
    {
        if (obj == p->upkeep->quiver[i]) return true;
    }

    return false;
}


/*
 * Get the total number of objects in the pack or quiver that are like the
 * given object.
 *
 * p is the player whose inventory is used for the calculation.
 * obj is the template for the objects to look for.
 * ignore_inscrip if true, ignore the inscriptions when testing whether
 * an object is similar; otherwise, test the inscriptions as well.
 * first if not NULL, set to the first stack like obj (by ordering in
 * the quiver or pack with quiver taking precedence over pack; if the pack
 * and quiver haven't been computed, it will be the first non-equipped stack
 * in the gear).
 */
static uint16_t object_pack_total(struct player *p, const struct object *obj, bool ignore_inscrip,
    struct object **first)
{
    uint16_t total = 0;
    char first_label = '\0';
    struct object *cursor;

    if (first) *first = NULL;

    for (cursor = p->gear; cursor; cursor = cursor->next)
    {
        bool like;

        if (cursor == obj)
        {
            /*
             * object_similar() excludes cursor == obj so if
             * obj is not equipped, account for it here.
             */
            like = !object_is_equipped(p->body, obj);
        }
        else if (ignore_inscrip)
            like = object_similar(p, obj, cursor, OSTACK_PACK);
        else
            like = object_stackable(p, obj, cursor, OSTACK_PACK);
        if (like)
        {
            total += cursor->number;
            if (first)
            {
                char test_label = gear_to_label(p, cursor);

                if (!*first)
                {
                    *first = cursor;
                    first_label = test_label;
                }
                else
                {
                    if (test_label >= 'a' && test_label <= 'z')
                    {
                        if (first_label == '\0' || (first_label >= 'a' && first_label <= 'z' &&
                            test_label < first_label))
                        {
                            *first = cursor;
                            first_label = test_label;
                        }
                    }
                    else if (test_label >= '0' && test_label <= '9')
                    {
                        if (first_label == '\0' || (first_label >= 'a' && first_label <= 'z') ||
                            (first_label >= '0' && first_label <= '9' && test_label < first_label))
                        {
                            *first = cursor;
                            first_label = test_label;
                        }
                    }
                }
            }
        }
    }

    return total;
}


/*
 * Calculate the number of pack slots used by the current gear.
 *
 * Note that this function does not check that there are adequate slots in the
 * quiver, just the total quantity of missiles.
 */
int pack_slots_used(struct player *p)
{
    struct object *obj;
    int i, pack_slots = 0;
    int quiver_ammo = 0;

    for (obj = p->gear; obj; obj = obj->next)
    {
        bool found = false;

        /* Equipment doesn't count */
        if (!object_is_equipped(p->body, obj))
        {
            /* Check if it is in the quiver */
            if (tval_is_ammo(obj) || of_has(obj->flags, OF_THROWING))
            {
                for (i = 0; i < z_info->quiver_size; i++)
                {
                    if (p->upkeep->quiver[i] == obj)
                    {
                        quiver_ammo += obj->number *
                            (tval_is_ammo(obj)? 1: z_info->thrown_quiver_mult);
                        found = true;
                        break;
                    }
                }
            }

            /* Count regular slots */
            if (!found) pack_slots++;
        }
    }

    /* Full slots */
    pack_slots += quiver_ammo / z_info->quiver_slot_size;

    /* Plus one for any remainder */
    if (quiver_ammo % z_info->quiver_slot_size) pack_slots++;

    return pack_slots;
}


/*
 * Determine which equipment slot (if any) an item likes. The slot might (or
 * might not) be open, but it is a slot which the object could be equipped in.
 *
 * For items where multiple slots could work (e.g. rings), the function
 * will try to a return an open slot if possible.
 */
int16_t wield_slot(struct player *p, const struct object *obj)
{
    /* Slot for equipment */
    switch (obj->tval)
    {
        case TV_MSTAFF: return slot_by_type(p, EQUIP_WEAPON, false);
        case TV_BOW: return slot_by_type(p, EQUIP_BOW, false);
        case TV_AMULET: return slot_by_type(p, EQUIP_AMULET, false);
        case TV_CLOAK: return slot_by_type(p, EQUIP_CLOAK, false);
        case TV_SHIELD: return slot_by_type(p, EQUIP_SHIELD, false);
        case TV_GLOVES: return slot_by_type(p, EQUIP_GLOVES, false);
        case TV_BOOTS: return slot_by_type(p, EQUIP_BOOTS, false);
        case TV_DIGGING:
        case TV_HORN: return slot_by_type(p, EQUIP_TOOL, false);
    }

    if (tval_is_melee_weapon(obj))
        return slot_by_type(p, EQUIP_WEAPON, false);
    if (tval_is_ring(obj))
        return slot_by_type(p, EQUIP_RING, false);
    if (tval_is_light(obj))
        return slot_by_type(p, EQUIP_LIGHT, false);
    if (tval_is_body_armor(obj))
        return slot_by_type(p, EQUIP_BODY_ARMOR, false);
    if (tval_is_head_armor(obj))
        return slot_by_type(p, EQUIP_HAT, false);

    /* No slot available */
    return -1;
}


/*
 * Acid has hit the player, attempt to affect some armor.
 *
 * Note that the "base armor" of an object never changes.
 *
 * If any armor is damaged (or resists), the player takes less damage.
 */
bool minus_ac(struct player *p)
{
    int i, count = 0;
    struct object *obj = NULL;

    /* Avoid crash during monster power calculations */
    if (!p->gear) return false;

    /* Count the armor slots */
    for (i = 0; i < p->body.count; i++)
    {
        /* Ignore non-armor */
        if (slot_type_is(p, i, EQUIP_WEAPON)) continue;
        if (slot_type_is(p, i, EQUIP_BOW)) continue;
        if (slot_type_is(p, i, EQUIP_RING)) continue;
        if (slot_type_is(p, i, EQUIP_AMULET)) continue;
        if (slot_type_is(p, i, EQUIP_LIGHT)) continue;
        if (slot_type_is(p, i, EQUIP_TOOL)) continue;

        /* Add */
        count++;
    }

    /* Pick one at random */
    for (i = p->body.count - 1; i >= 0; i--)
    {
        /* Ignore non-armor */
        if (slot_type_is(p, i, EQUIP_WEAPON)) continue;
        if (slot_type_is(p, i, EQUIP_BOW)) continue;
        if (slot_type_is(p, i, EQUIP_RING)) continue;
        if (slot_type_is(p, i, EQUIP_AMULET)) continue;
        if (slot_type_is(p, i, EQUIP_LIGHT)) continue;
        if (slot_type_is(p, i, EQUIP_TOOL)) continue;

        if (one_in_(count--)) break;
    }

    /* Get the item */
    obj = slot_object(p, i);

    /* If we can still damage the item */
    if (obj && (obj->ac + obj->to_a > 0))
    {
        char o_name[NORMAL_WID];

        object_desc(p, o_name, sizeof(o_name), obj, ODESC_BASE);

        /* Object resists */
        if (obj->el_info[ELEM_ACID].flags & EL_INFO_IGNORE)
            msg(p, "Your %s is unaffected!", o_name);
        else
        {
            msg(p, "Your %s is damaged!", o_name);

            /* Damage the item */
            obj->to_a--;

            p->upkeep->update |= (PU_BONUS);
            set_redraw_equip(p, obj);
        }

        /* There was an effect */
        return true;
    }

    /* No damage or effect */
    return false;
}


/*
 * Remove an object from the gear list, leaving it unattached
 */
void gear_excise_object(struct player *p, struct object *obj)
{
    int i;

    pile_excise(&p->gear, obj);

    /* Change the weight */
    p->upkeep->total_weight -= (obj->number * obj->weight);

    /* Hack -- excise object index */
    obj->oidx = 0;

    /* Make sure it isn't still equipped */
    for (i = 0; i < p->body.count; i++)
    {
        if (slot_object(p, i) == obj)
        {
            p->body.slots[i].obj = NULL;
            p->upkeep->equip_cnt--;
        }
    }

    /* Update the gear */
    calc_inventory(p);

    /* Housekeeping */
    p->upkeep->update |= (PU_BONUS);
    p->upkeep->notice |= (PN_COMBINE);
    set_redraw_equip(p, NULL);
    set_redraw_inven(p, NULL);
}


struct object *gear_last_item(struct player *p)
{
    return pile_last_item(p->gear);
}


void gear_insert_end(struct player *p, struct object *obj)
{
    pile_insert_end(&p->gear, obj);
}


/*
 * Remove an amount of an object from the inventory or quiver, returning
 * a detached object which can be used.
 *
 * Optionally describe what remains.
 */
struct object *gear_object_for_use(struct player *p, struct object *obj, int num, bool message,
    bool *none_left)
{
    struct object *usable;
    struct object *first_remainder = NULL;
    char name[NORMAL_WID];
    char label = gear_to_label(p, obj);

    /* Bounds check */
    num = MIN(num, obj->number);

    /* Split off a usable object if necessary */
    if (obj->number > num)
    {
        usable = object_split(obj, num);

        /* Change the weight */
        p->upkeep->total_weight -= (num * obj->weight);

        if (message)
        {
            uint16_t total;

            /*
             * Don't show aggregate total in pack if equipped or
             * if the description could have a number of charges
             * or recharging notice specific to the stack (not
             * aggregating those quantities so there would be
             * confusion if aggregating the count).
             */
            if (object_is_equipped(p->body, obj) || tval_can_have_charges(obj) ||
                tval_is_rod(obj) || obj->timeout > 0)
            {
                total = obj->number;
            }
            else
            {
                total = object_pack_total(p, obj, false, &first_remainder);
                my_assert(total >= first_remainder->number);
                if (total == first_remainder->number) first_remainder = NULL;
            }

            object_desc(p, name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
                (total << 16));
        }
    }
    else
    {
        if (message)
        {
            uint16_t total;

            /* Use same logic as above for showing an aggregate total. */
            if (object_is_equipped(p->body, obj) || tval_can_have_charges(obj) ||
                tval_is_rod(obj) || obj->timeout > 0)
            {
                total = obj->number;
            }
            else
                total = object_pack_total(p, obj, false, &first_remainder);

            my_assert(total >= num);
            total -= num;
            if (!total || total <= first_remainder->number) first_remainder = NULL;
            object_desc(p, name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
                (total << 16));
        }

        /* We're using the entire stack */
        usable = obj;
        gear_excise_object(p, usable);
        *none_left = true;

        /* Stop tracking item */
        if (tracked_object_is(p->upkeep, obj)) track_object(p->upkeep, NULL);
    }

    /* Housekeeping */
    p->upkeep->update |= (PU_BONUS);
    p->upkeep->notice |= (PN_COMBINE);
    set_redraw_equip(p, NULL);
    set_redraw_inven(p, NULL);

    /* Print a message if desired */
    if (message)
    {
        if (first_remainder)
        {
            label = gear_to_label(p, first_remainder);
            msg(p, "You have %s (1st %c).", name, label);
        }
        else
            msg(p, "You have %s (%c).", name, label);
    }

    return usable;
}


/*
 * Check how many missiles can be put in the quiver with a limit on whether
 * the quiver can expand to take more slots in the pack.
 *
 * p Is the player with the quiver to use.
 * obj is the object to add
 *
 * At entry, *n_add_pack is the maximum number of additional
 * pack slots to give to the quiver. At exit, *n_add_pack will be the number
 * of those slots that were not used to expand the quiver.
 *
 * At exit, *n_to_quiver will be the number that can be
 * added to the quiver. It will be no more than obj->number. The value of
 * *n_to_quiver at entry is not used.
 */
static void quiver_absorb_num(struct player *p, const struct object *obj, int *n_add_pack,
    int *n_to_quiver)
{
    bool ammo = tval_is_ammo(obj);

    /* Must be ammo or good for throwing */
    if (ammo || of_has(obj->flags, OF_THROWING))
    {
        int i, quiver_count = 0, space_free = 0, n_empty = 0;
        int desired_slot = preferred_quiver_slot(p, obj);
        bool displaces = false;

        /* Count the current space this object could go into. */
        for (i = 0; i < z_info->quiver_size; i++)
        {
            struct object *quiver_obj = p->upkeep->quiver[i];

            if (quiver_obj)
            {
                int mult = (tval_is_ammo(quiver_obj)? 1: z_info->thrown_quiver_mult);

                quiver_count += quiver_obj->number * mult;
                if (object_stackable(p, quiver_obj, obj, OSTACK_PACK))
                {
                    my_assert(quiver_obj->number * mult <= z_info->quiver_slot_size);
                    space_free += z_info->quiver_slot_size - quiver_obj->number * mult;
                }
                else if ((desired_slot == i) && (preferred_quiver_slot(p, quiver_obj) != i))
                {
                    /*
                     * The object to be added prefers to go in this slot, but it's occupied by
                     * something that could be displaced to another quiver slot, if one is
                     * available.
                     */
                    displaces = true;
                    my_assert(quiver_obj->number * mult <= z_info->quiver_slot_size);

                    /*
                     * Avoid double counting in the ammo case since the empty slot, if any,
                     * for the displaced stack is treated as fully available.
                     */
                    if (ammo)
                        space_free += z_info->quiver_slot_size - quiver_obj->number * mult;
                    else
                        space_free += z_info->quiver_slot_size;
                }
            }
            else
            {
                ++n_empty;

                /*
                 * Ammo can fit in any empty slot in the quiver.
                 * Non-ammo throwing items are restricted to
                 * their preferred slot.
                 */
                if (ammo || (desired_slot == i))
                    space_free += z_info->quiver_slot_size;
            }
        }

        /*
         * Only possible to add if there is space free in the quiver
         * and either are displacing a pile with an empty quiver slot
         * available for it or are not displacing a pile at all.
         */
        if (space_free && ((displaces && n_empty) || !displaces))
        {
            int mult = (ammo? 1: z_info->thrown_quiver_mult);

            /*
             * When quiver_count % quiver_slot_size is zero, adding
             * anything will require a pack slot.
             */
            int remainder = quiver_count % z_info->quiver_slot_size;
            int limit_from_pack = (remainder? z_info->quiver_slot_size - remainder: 0);

            if (*n_add_pack > 0)
                limit_from_pack += *n_add_pack * z_info->quiver_slot_size;

            /* Return the number or amount that fits. */
            space_free = MIN(space_free, limit_from_pack);
            *n_to_quiver = MIN(obj->number, space_free / mult);
            *n_add_pack -= (*n_to_quiver * mult + z_info->quiver_slot_size - 1 - remainder) /
                z_info->quiver_slot_size;
            return;
        }
    }

    /* Not suitable for the quiver or no space */
    *n_to_quiver = 0;
}


/*
 * Calculate how much of an item can be carried in the inventory or quiver.
 */
int inven_carry_num(struct player *p, struct object *obj)
{
    int n_free_slot = z_info->pack_size - pack_slots_used(p);
    int num_to_quiver, num_left, i;

    /* Treasure can always be picked up. */
    if (tval_is_money(obj) && lookup_kind(obj->tval, obj->sval))
        return obj->number;

    /* Absorb as many as we can in the quiver. */
    quiver_absorb_num(p, obj, &n_free_slot, &num_to_quiver);

    /* The quiver will get everything, or the pack can hold what's left. */
    if ((num_to_quiver == obj->number) || (n_free_slot > 0))
        return obj->number;

    /* See if we can add to a partially full inventory slot. */
    num_left = obj->number - num_to_quiver;
    for (i = 0; i < z_info->pack_size; i++)
    {
        struct object *inven_obj = p->upkeep->inven[i];

        if (inven_obj && object_stackable(p, inven_obj, obj, OSTACK_PACK))
        {
            num_left -= inven_obj->kind->base->max_stack - inven_obj->number;
            if (num_left <= 0) break;
        }
    }

    /* Return the number we can absorb */
    return obj->number - MAX(num_left, 0);
}


/*
 * Check if we're allowed to get rid of an item easily.
 */
bool inven_drop_okay(struct player *p, struct object *obj)
{
    /* Never drop true artifacts above their base depth except the Crown and Grond */
    if (!cfg_artifact_drop_shallow && true_artifact_p(obj) &&
        (p->wpos.depth < obj->artifact->level) && !kf_has(obj->kind->kind_flags, KF_QUEST_ART))
    {
        /* Do not apply this rule to no_recall characters and DMs */
        if ((cfg_diving_mode < 3) && !is_dm_p(p)) return false;
    }

    return true;
}


/*
 * Check if we have space for some of an item in the pack.
 */
bool inven_carry_okay(struct player *p, struct object *obj)
{
    return ((inven_carry_num(p, obj) > 0)? true: false);
}


/*
 * Describe the charges on an item in the inventory.
 */
void inven_item_charges(struct player *p, struct object *obj)
{
    /* Require staff/wand */
    if (tval_can_have_charges(obj) && object_is_known(p, obj))
        msg(p, "You have %d charge%s remaining.", obj->pval, PLURAL(obj->pval));
}


/*
 * Add an item to the players inventory.
 *
 * If the new item can combine with an existing item in the inventory,
 * it will do so, using object_mergeable() and object_absorb(), otherwise,
 * the item will be placed into the first available gear array index.
 *
 * This function can be used to "over-fill" the player's pack, but only
 * once, and such an action must trigger the "overflow" code immediately.
 * Note that when the pack is being "over-filled", the new item must be
 * placed into the "overflow" slot, and the "overflow" must take place
 * before the pack is reordered, but (optionally) after the pack is
 * combined. This may be tricky. See "dungeon.c" for info.
 *
 * Note that this code removes any location information from the object once
 * it is placed into the inventory, but takes no responsibility for removing
 * the object from any other pile it was in.
 */
void inven_carry(struct player *p, struct object *obj, bool absorb, bool message)
{
    bool combining = false;
    struct object *local_obj = obj;

    /* Object is now owned */
    object_own(p, obj);

    /* Check for combining, if appropriate */
    if (absorb)
    {
        struct object *combine_item = p->gear;

        while (combine_item)
        {
            object_stack_t stack_mode = (object_is_in_quiver(p, combine_item)? OSTACK_QUIVER:
                OSTACK_PACK);

            if (!object_is_equipped(p->body, combine_item) &&
                object_mergeable(p, combine_item, obj, stack_mode))
            {
                break;
            }

            combine_item = combine_item->next;
        }

        if (combine_item)
        {
            /* Increase the weight */
            p->upkeep->total_weight += (obj->number * obj->weight);

            /* Combine the items, and their known versions */
            object_absorb(combine_item, obj);

            local_obj = combine_item;
            combining = true;
        }
    }

    /* We didn't manage the find an object to combine with */
    if (!combining)
    {
        /* Paranoia */
        my_assert(pack_slots_used(p) <= z_info->pack_size);

        gear_insert_end(p, obj);
        apply_autoinscription(p, obj);

        /* Remove cave object details */
        obj->held_m_idx = 0;
        loc_init(&obj->grid, 0, 0);
        memset(&obj->wpos, 0, sizeof(struct worldpos));

        /* Update the inventory */
        p->upkeep->total_weight += (obj->number * obj->weight);
        p->upkeep->notice |= (PN_COMBINE);

        /* Hobbits ID mushrooms on pickup, gnomes ID wands and staffs on pickup */
        if (!object_is_known(p, obj))
        {
            if (player_has(p, PF_KNOW_MUSHROOM) && tval_is_mushroom(obj))
            {
                object_know_everything(p, obj);
                msg(p, "Mushrooms for breakfast!");
            }

            else if (player_has(p, PF_KNOW_ZAPPER) && tval_is_zapper(obj))
                object_know_everything(p, obj);

            /*
             * PWMAngband: permanently polymorphed characters and Monks cannot use weapons,
             * so they need to learn "on wield" flags when picking them up
             */
            else if ((player_has(p, PF_PERM_SHAPE) || player_has(p, PF_MARTIAL_ARTS)) &&
                (tval_is_melee_weapon(obj) || tval_is_mstaff(obj) || tval_is_launcher(obj)))
            {
                object_learn_on_carry(p, obj);
            }      
            
            /* PWMAngband: need to learn "on wield" flags when picking up ammo */
            else if (tval_is_ammo(obj))
                object_learn_on_carry(p, obj);

            // NO_BOOTS races can't wear boots for ID, so..
            else if (player_has(p, PF_NO_BOOTS))
                boots_learn_on_carry(p, obj);      
        }
    }

    p->upkeep->update |= (PU_BONUS | PU_INVEN);
    p->upkeep->redraw |= (PR_SPELL | PR_STUDY);
    set_redraw_equip(p, NULL);
    set_redraw_inven(p, NULL);
    update_stuff(p, chunk_get(&p->wpos));

    if (message)
    {
        char o_name[120];
        struct object *first;
        uint16_t total;
        char label;

        /*
         * Show an aggregate total if the description doesn't have
         * a charge/recharging notice that's specific to the stack.
         */
        if (tval_can_have_charges(local_obj) || tval_is_rod(local_obj) || local_obj->timeout > 0)
        {
            total = local_obj->number;
            first = local_obj;
        }
        else
            total = object_pack_total(p, local_obj, false, &first);

        my_assert(first && total >= first->number);
        object_desc(p, o_name, sizeof(o_name), local_obj, ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
            (total << 16));
        label = gear_to_label(p, first);
        if (total > first->number)
            msg(p, "You have %s (1st %c).", o_name, label);
        else
        {
            my_assert(first == local_obj);
            msg(p, "You have %s (%c).", o_name, label);
        }
    }

    /* Sound */

    // On character creation you get items, so 'sound' cause warning message in server log.
    // It's not critical, but lets have clean logs and wait for 1st turn
    if (p->active_turn.turn)
    {
        if (object_is_in_quiver(p, local_obj))
            sound(p, MSG_QUIVER);
        else if (tval_is_weapon(local_obj))
        {
            if (local_obj->tval == TV_SWORD)
                sound(p, MSG_ITEM_BLADE);
            else if (local_obj->tval == TV_BOW || tval_is_mstaff(local_obj))
                sound(p, MSG_ITEM_WOOD);
            else if (((local_obj->tval == TV_HAFTED) && (local_obj->sval == lookup_sval(local_obj->tval, "Whip"))) ||
                     ((local_obj->tval == TV_BOW) && (local_obj->sval == lookup_sval(local_obj->tval, "Sling"))))
                         sound(p, MSG_ITEM_WHIP);
            else
                sound(p, MSG_ITEM_PICKUP);
        }
        else if (tval_is_body_armor(local_obj))
        {
            if ((local_obj)->weight < 121)
                sound(p, MSG_ITEM_LIGHT_ARMOR);
            else if ((local_obj)->weight < 251)
                sound(p, MSG_ITEM_MEDIUM_ARMOR);
            else
                sound(p, MSG_ITEM_HEAVY_ARMOR);
        }
        else if (tval_is_armor(local_obj))
        {
            if (local_obj->tval == TV_CROWN)
                sound(p, MSG_ITEM_PICKUP);
            else if (local_obj->weight < 25 || local_obj->tval == TV_SHIELD || local_obj->tval == TV_CLOAK)
                    sound(p, MSG_ITEM_LIGHT_ARMOR);
            else if ((local_obj)->weight < 51)
                sound(p, MSG_ITEM_MEDIUM_ARMOR);
            else
                sound(p, MSG_ITEM_HEAVY_ARMOR);
        }
        else if (tval_is_ring(local_obj))
            sound(p, MSG_ITEM_RING);
        else if (tval_is_amulet(local_obj))
            sound(p, MSG_ITEM_AMULET);
        else
            sound(p, MSG_ITEM_PICKUP);
    }
}


static void know_everything(struct player *p, struct chunk *c)
{
    struct object *obj;

    /* Know all objects under the player */
    for (obj = square_object(c, &p->grid); obj; obj = obj->next)
    {
        if (object_is_known(p, obj)) continue;
        object_know_everything(p, obj);
    }

    /* Know all carried objects */
    for (obj = p->gear; obj; obj = obj->next)
    {
        if (object_is_known(p, obj)) continue;
        object_know_everything(p, obj);
    }
}


/*
 * Wield or wear a single item from the pack or floor
 */
void inven_wield(struct player *p, struct object *obj, int slot, char *message, int len)
{
    struct object *wielded, *old = p->body.slots[slot].obj;
    const char *fmt;
    char o_name[NORMAL_WID];
    bool dummy = false;
    struct chunk *c = chunk_get(&p->wpos);

    /* Increase equipment counter if empty slot */
    if (old == NULL) p->upkeep->equip_cnt++;

    /* Take a turn */
    use_energy(p);

    /* It's either a gear object or a floor object */
    if (object_is_carried(p, obj))
    {
        /* Split off a new object if necessary */
        if (obj->number > 1)
        {
            wielded = gear_object_for_use(p, obj, 1, false, &dummy);

            /* Change the weight */
            p->upkeep->total_weight += (wielded->number * wielded->weight);

            /* The new item needs new gear and known gear entries */
            wielded->next = obj->next;
            obj->next = wielded;
            wielded->prev = obj;
            if (wielded->next) (wielded->next)->prev = wielded;
        }

        /* Just use the object directly */
        else
            wielded = obj;

        p->upkeep->notice |= (PN_COMBINE);
    }
    else
    {
        // 1) to prevent ground-swap-curse-cheeze:
        struct object *equip_obj;
        equip_obj = slot_object(p, slot);

        // Don't allow replacing cursed items
        // (anticheeze: when you replace cursed item with normal one from the ground)
        // (note: `equip_obj` at 1st place there is to prevent "empty slot crush")
        if (equip_obj && equip_obj->curses)
        {
            msg(p, "Can't swap cursed item with the item from ground. Try to 't'ake if off first.");
            return;
        }

        // 2) regular case:

        /* Get a floor item and carry it */
        wielded = floor_object_for_use(p, c, obj, 1, false, &dummy);
        inven_carry(p, wielded, false, false);
    }

    /* Wear the new stuff */
    wielded->oidx = z_info->pack_size + slot;
    p->body.slots[slot].obj = wielded;

    /* Object is now owned */
    object_own(p, wielded);

    /* Bypass auto-ignore */
    wielded->ignore_protect = 1;

    /* Do any ID-on-wield */
    object_learn_on_wield(p, wielded);

    /* Auto-id */
    if (of_has(wielded->flags, OF_KNOWLEDGE)) know_everything(p, c);

    /* Where is the item now */
    if (tval_is_melee_weapon(wielded) || tval_is_mstaff(wielded))
        fmt = "You are wielding %s (%c).";
    else if (tval_is_launcher(wielded))
        fmt = "You are shooting with %s (%c).";
    else if (tval_is_light(wielded))
        fmt = "Your light source is %s (%c).";
    else if (tval_is_tool(wielded))
        fmt = "You are using %s (%c).";
    else
        fmt = "You are wearing %s (%c).";

    /* Describe the result */
    object_desc(p, o_name, sizeof(o_name), wielded, ODESC_PREFIX | ODESC_FULL);

    /* Message */
    if (message) strnfmt(message, len, fmt, o_name, gear_to_label(p, wielded));
    else
    {
        // message
        msg(p, fmt, o_name, gear_to_label(p, wielded));
        // sound
        if (tval_is_weapon(obj))
        {
            if (obj->tval == TV_SWORD)
                sound(p, MSG_ITEM_BLADE);
            else if (obj->tval == TV_BOW || tval_is_mstaff(obj))
                sound(p, MSG_ITEM_WOOD);
            else if (((obj->tval == TV_HAFTED) && (obj->sval == lookup_sval(obj->tval, "Whip"))) ||
                     ((obj->tval == TV_BOW) && (obj->sval == lookup_sval(obj->tval, "Sling"))))
                         sound(p, MSG_ITEM_WHIP);
            else
                sound(p, MSG_WIELD);
        }
        else if (tval_is_body_armor(obj))
        {
            if ((obj)->weight < 121)
                sound(p, MSG_ITEM_LIGHT_ARMOR);
            else if ((obj)->weight < 251)
                sound(p, MSG_ITEM_MEDIUM_ARMOR);
            else
                sound(p, MSG_ITEM_HEAVY_ARMOR);
        }
        else if (tval_is_armor(obj))
        {
            if (obj->tval == TV_CROWN)
                sound(p, MSG_ITEM_PICKUP);
            else if (obj->weight < 25 || obj->tval == TV_SHIELD || obj->tval == TV_CLOAK)
                    sound(p, MSG_ITEM_LIGHT_ARMOR);
            else if ((obj)->weight < 51)
                sound(p, MSG_ITEM_MEDIUM_ARMOR);
            else
                sound(p, MSG_ITEM_HEAVY_ARMOR);
        }
        else if (tval_is_ring(obj))
            sound(p, MSG_ITEM_RING);
        else if (tval_is_amulet(obj))
            sound(p, MSG_ITEM_AMULET);
        else if (tval_is_light(obj))
        {
            if (obj->sval == lookup_sval(obj->tval, "Wooden Torch"))
                sound(p, MSG_ITEM_LIGHT_TORCH);
            else if (obj->sval == lookup_sval(obj->tval, "Lantern"))
                sound(p, MSG_ITEM_LIGHT_LANTERN);
            else if (obj->sval == lookup_sval(obj->tval, "Lamp"))
                sound(p, MSG_ITEM_LIGHT_LAMP);
            else if (obj->sval == lookup_sval(obj->tval, "Palantir"))
                sound(p, MSG_ITEM_LIGHT_PALANTIR);
            else if (obj->artifact)              // Phial, Star, Stone etc
                sound(p, MSG_ITEM_LIGHT_PHIAL);
        }
        else
            sound(p, MSG_WIELD);
    }

    /* Sticky flag gets a special mention */
    if (of_has(wielded->flags, OF_STICKY))
    {
        /* Warn the player */
        msgt(p, MSG_CURSED, "Oops! It feels deathly cold!");
    }

    /* See if we have to overflow the pack */
    combine_pack(p);
    pack_overflow(p, c, old);

    /* Recalculate bonuses, torch, mana, gear */
    p->upkeep->notice |= (PN_IGNORE);
    p->upkeep->update |= (PU_BONUS | PU_INVEN | PU_UPDATE_VIEW);
    p->upkeep->redraw |= (PR_PLUSSES | PR_BASIC);
    set_redraw_equip(p, NULL);
    set_redraw_inven(p, NULL);
    update_stuff(p, c);
}


/*
 * Take off a non-cursed equipment item
 *
 * Note that taking off an item when "full" may cause that item
 * to fall to the ground.
 *
 * Note also that this function does not try to combine the taken off item
 * with other inventory items - that must be done by the calling function.
 */
void inven_takeoff(struct player *p, struct object *obj)
{
    int slot = equipped_item_slot(p->body, obj);
    const char *act;
    char o_name[NORMAL_WID];

    /* Paranoia */
    if (slot == p->body.count) return;

    /* Check preventive inscription '!t' */
    if (object_prevent_inscription(p, obj, INSCRIPTION_TAKEOFF, false))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Describe the object */
    object_desc(p, o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL);

    /* Describe removal by slot */
    if (slot_type_is(p, slot, EQUIP_WEAPON))
        act = "You were wielding";
    else if (slot_type_is(p, slot, EQUIP_BOW) || slot_type_is(p, slot, EQUIP_LIGHT))
        act = "You were holding";
    else if (slot_type_is(p, slot, EQUIP_TOOL))
        act = "You were using";
    else
        act = "You were wearing";

    /* De-equip the object */
    p->body.slots[slot].obj = NULL;
    p->upkeep->equip_cnt--;

    p->upkeep->update |= (PU_BONUS | PU_INVEN | PU_UPDATE_VIEW);
    p->upkeep->redraw |= (PR_PLUSSES);
    set_redraw_equip(p, NULL);
    set_redraw_inven(p, NULL);
    p->upkeep->notice |= (PN_IGNORE);
    update_stuff(p, chunk_get(&p->wpos));

    /* Message */
    msg(p, "%s %s (%c).", act, o_name, gear_to_label(p, obj));

    /* Sound */
    if (obj->weight < 15)
        sound(p, MSG_ITEM_TAKEOFF_TINY);
    else if (obj->weight < 50)
        sound(p, MSG_ITEM_TAKEOFF_SMALL);
    else if (obj->weight < 150)
        sound(p, MSG_ITEM_TAKEOFF_MEDIUM);
    else
        sound(p, MSG_ITEM_TAKEOFF_LARGE);
}


/*
 * Drop (some of) a non-cursed inventory/equipment item "near" the current
 * location
 *
 * There are two cases here - a single object or entire stack is being dropped,
 * or part of a stack is being split off and dropped
 */
bool inven_drop(struct player *p, struct object *obj, int amt, bool bypass_inscr)
{
    struct object *dropped;
    bool none_left = false;
    bool equipped = false;
    char name[NORMAL_WID];
    char label;
    struct object *first;
    struct object *desc_target;
    uint16_t total;
    struct curse_data *c = obj->curses;
    size_t i; // for curse check

    /* Error check */
    if (amt <= 0) return true;

    /*
     * Check it is still held, in case there were two drop commands queued
     * for this item. This is in theory not ideal, but in practice should
     * be safe.
     */
    if (!object_is_carried(p, obj)) return true;

    /* Get where the object is now */
    label = gear_to_label(p, obj);

    /* Not too many */
    if (amt > obj->number) amt = obj->number;

    /* Check preventive inscription '!d' */
    if (object_prevent_inscription(p, obj, INSCRIPTION_DROP, false) && !bypass_inscr)
    {
        msg(p, "The item's inscription prevents it.");
        return true;
    }

    /* Never drop true artifacts above their base depth except the Crown and Grond */
    if (!inven_drop_okay(p, obj))
    {
        if (!bypass_inscr) msg(p, "You cannot drop this here.");
        return false;
    }

    // Can not drop regular cursed items
    if (obj->curses && !obj->artifact)
    {
        struct location *dungeon = get_dungeon(&p->wpos);

        // If we outside - we can drop cursed item, but only light curses and only during the day 
        if (p->wpos.depth == 0 || (dungeon && df_has(dungeon->flags, DF_OPEN_SKY)))
        {
            for (i = 0; c && (i < (size_t)z_info->curse_max); i++)
            {
                if (c[i].power == 0) continue;

                // can drop only light cursed items at night
                if (c[i].power > 35 && c[i].power < 100 && !is_daytime())
                {
                   msg(p, "You can not drop this item. It seems it's cursed. Try to uncurse it or");
                   msg(p, "wait till the day cames to bring it under the sunlight to weaken the curse.");
                   return false;
                }
                // no drop moderately cursed items even at daytime
                else if (c[i].power > 45 && c[i].power < 100)
                {
                   msg(p, "You can not drop this item. It seems it's heavely cursed. Current curse is");
                   msg(p, "too strong, so even sunlight don't weaken it. Try to uncurse it by other means.");
                   return false;
                }
            }
        }
        // in the dungeon:
        else
        {
            for (i = 0; c && (i < (size_t)z_info->curse_max); i++)
            {
                if (c[i].power == 0) continue;
                if (c[i].power < 100)
                {
                   msg(p, "You can not drop this item. It seems it's cursed. Try to uncurse it or");
                   msg(p, "bring it under sky; sunlight might help to take off weakly cursed items.");
                   return false;
                }
            }
        }
    }

    /* NO_DROP object flag */
    if (obj->soulbound || (of_has(obj->flags, OF_NO_DROP) && !(p->dm_flags & DM_HOUSE_CONTROL)))
    {
        msg(p, "You cannot drop this item.");
        return false;
    }    

    /* Never drop items in wrong house */
    if (!check_store_drop(p) && !(p->dm_flags & DM_HOUSE_CONTROL))
    {
        if (!bypass_inscr) msg(p, "You cannot drop this here.");
        return false;
    }

    /* Take off equipment, don't combine */
    if (object_is_equipped(p->body, obj))
    {
        equipped = true;
        inven_takeoff(p, obj);
    }

    /* Get the object */
    dropped = gear_object_for_use(p, obj, amt, false, &none_left);

    /* Describe the dropped object */
    object_desc(p, name, sizeof(name), dropped, ODESC_PREFIX | ODESC_FULL);

    /* Message */
    msg(p, "You drop %s (%c).", name, label);

    /*
     * Describe what's left
     *
     * Like gear_object_for_use(), don't show an aggregate total
     * if it was equipped or the item has charges/recharging
     * notice that is specific to the stack.
     */
    if (equipped || tval_can_have_charges(obj) || tval_is_rod(obj) || obj->timeout > 0)
    {
        first = NULL;
        if (none_left)
        {
            total = 0;
            desc_target = dropped;
        }
        else
        {
            total = obj->number;
            desc_target = obj;
        }
    }
    else
    {
        total = object_pack_total(p, obj, false, &first);
        desc_target = (total? obj: dropped);
    }
    object_desc(p, name, sizeof(name), desc_target, ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
        (total << 16));
    if (!first)
        msg(p, "You have %s (%c).", name, label);
    else
    {
        label = gear_to_label(p, first);
        if (total > first->number)
            msg(p, "You have %s (1st %c).", name, label);
        else
            msg(p, "You have %s (%c).", name, label);
    }

    /* Drop it (carefully) near the player */
    drop_near(p, chunk_get(&p->wpos), &dropped, 0, &p->grid, false,
        (bypass_inscr? DROP_SILENT: DROP_FORBID), true);

    return true;
}


/*
 * Return whether each stack of objects can be merged into two uneven stacks.
 */
static bool inven_can_stack_partial(struct player *p, const struct object *obj1,
    const struct object *obj2, object_stack_t mode1, object_stack_t mode2)
{
    object_stack_t cmode = mode1 | mode2;

    if (!object_stackable(p, obj1, obj2, cmode)) return false;

    /*
     * Now verify the numbers are suitable for uneven stacks. Want the
     * leading stack, obj1, to have its count maximized.
     */
    if (!(cmode & OSTACK_STORE))
    {
        /* The quiver may have stricter limits. */
        if (mode1 & OSTACK_QUIVER)
        {
            int qlimit = z_info->quiver_slot_size /
                (tval_is_ammo(obj1)? 1: z_info->thrown_quiver_mult);

            /* These's no reason to combine if already at the limit. */
            if (obj1->number == qlimit) return false;

            /*
             * Checked the per-stack limits. If trying to move
             * items to the quiver, also check the overall quiver
             * limits to avoid combining and then splitting in
             * calc_inventory().
             */
            if (mode2 & ~OSTACK_QUIVER)
            {
                int n_free_slot = z_info->pack_size - pack_slots_used(p);
                int num_to_quiver;

                quiver_absorb_num(p, obj2, &n_free_slot, &num_to_quiver);
                if (num_to_quiver <= 0) return false;
            }
        }

        /* These's no reason to combine if already at the limit. */
        else if (obj1->number == obj1->kind->base->max_stack)
            return false;
    }

    return true;
}


/*
 * Combine items in the pack, confirming no blank objects or gold
 */
void combine_pack(struct player *p)
{
    struct object *obj1, *obj2, *prev;
    bool display_message = false;
    bool redraw = false;

    /* Combine the pack (backwards) */
    obj1 = gear_last_item(p);
    while (obj1)
    {
        my_assert(!tval_is_money(obj1));
        prev = obj1->prev;

        /* Scan the items above that item */
        for (obj2 = p->gear; obj2 && (obj2 != obj1); obj2 = obj2->next)
        {
            object_stack_t stack_mode2 = (object_is_in_quiver(p, obj2)? OSTACK_QUIVER: OSTACK_PACK);

            /* Can we drop "obj1" onto "obj2"? */
            if (object_mergeable(p, obj2, obj1, stack_mode2))
            {
                display_message = true;

                redraw = true;
                object_absorb(obj2, obj1);
                break;
            }
            else
            {
                object_stack_t stack_mode1 = (object_is_in_quiver(p, obj1)? OSTACK_QUIVER: OSTACK_PACK);

                if (inven_can_stack_partial(p, obj2, obj1, stack_mode2, stack_mode1))
                {
                    /*
                     * Don't display a message for this case: shuffling items between
                     * stacks isn't interesting to the player.
                     */
                    redraw = true;
                    object_absorb_partial(obj2, obj1, stack_mode2, stack_mode1);
                    break;
                }
            }
        }
        obj1 = prev;
    }

    calc_inventory(p);

    /* Redraw */
    if (redraw)
    {
        p->upkeep->redraw |= (PR_SPELL | PR_STUDY);
        set_redraw_equip(p, NULL);
        set_redraw_inven(p, NULL);
    }

    /* Message */
    if (display_message) msg(p, "You combine some items in your pack.");
}


/*
 * Returns whether the pack is holding the more than the maximum number of
 * items. If this is true, calling pack_overflow() will trigger a pack overflow.
 */
static bool pack_is_overfull(struct player *p)
{
    return ((pack_slots_used(p) > z_info->pack_size)? true: false);
}


/*
 * Overflow an item from the pack, if it is overfull.
 */
void pack_overflow(struct player *p, struct chunk *c, struct object *obj)
{
    int i;
    char o_name[NORMAL_WID];

    if (!pack_is_overfull(p)) return;

    /* Disturbing */
    disturb(p, 0);

    /* Warning */
    msg(p, "Your pack overflows!");

    /* Get the last proper item */
    for (i = 1; i <= z_info->pack_size; i++)
    {
        if (!p->upkeep->inven[i]) break;
    }

    /* Drop the last inventory item unless requested otherwise */
    if (!obj) obj = p->upkeep->inven[i - 1];

    /* Describe */
    object_desc(p, o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL);

    /* Message */
    msg(p, "You drop %s.", o_name);

    /* Excise the object and drop it (carefully) near the player */
    gear_excise_object(p, obj);
    drop_near(p, c, &obj, 0, &p->grid, false, DROP_FADE, true);

    /* Describe */
    msg(p, "You no longer have %s.", o_name);

    /* Notice, update, redraw */
    if (p->upkeep->notice) notice_stuff(p);
    if (p->upkeep->update) update_stuff(p, c);
    if (p->upkeep->redraw) redraw_stuff(p);
}


/*
 * Look at an item's inscription to determine where it wants to be placed in
 * the quiver. If the item is not appropriate for the quiver or is not
 * appropriately inscribed, return -1.
 */
int preferred_quiver_slot(struct player *p, const struct object *obj)
{
    int desired_slot = -1;

    if (obj->note && (tval_is_ammo(obj) || of_has(obj->flags, OF_THROWING)))
    {
        const char *s = strchr(quark_str(obj->note), '@');
        char fire_key, throw_key;

        /*
         * Would be nice to use cmd_lookup_key() for this, but that is
         * part of the ui layer (declared in ui-game.h). Instead,
         * hardwire the keys for the fire and throw commands.
         */
        if (OPT(p, rogue_like_commands)) fire_key = 't';
        else fire_key = 'f';
        throw_key = 'v';

        while (1)
        {
            if (!s) break;
            if (s[1] == fire_key || s[1] == throw_key)
            {
                desired_slot = s[2] - '0';
                break;
            }
            s = strchr(s + 1, '@');
        }
    }

    return desired_slot;
}


/* Can only put on wieldable items */
bool item_tester_hook_wear(struct player *p, const struct object *obj)
{
    /* Check for a usable slot */
    int slot = wield_slot(p, obj);

    if ((slot < 0) || (slot >= p->body.count)) return false;

    /* Permanently polymorphed characters and Monks cannot use weapons */
    if ((player_has(p, PF_PERM_SHAPE) || player_has(p, PF_MARTIAL_ARTS)) &&
        ((slot == slot_by_name(p, "weapon")) || (slot == slot_by_name(p, "shooting"))))
    {
        return false;
    }
    
    /* NO_BOOTS racial: can't wear boots */
    if (player_has(p, PF_NO_BOOTS) && (slot == slot_by_name(p, "feet")))
    {
        return false;
    }

    /* Assume wearable */
    return true;
}
