/*
 * File: cmd-obj.c
 * Purpose: Handle objects in various ways
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007-9 Andi Sidwell, Chris Carr, Ed Graham, Erik Osheim
 * Copyright (c) 2025 MAngband and PWMAngband Developers
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
 * Utility bits and bobs
 */


/*
 * Check to see if the player can use a rod/wand/staff/activatable object.
 *
 * Return a positive value if the given object can be used; return zero if
 * the object cannot be used but might succeed on repetition (i.e. device's
 * failure check did not pass but the failure rate is less than 100%); return
 * a negative value if the object cannot be used and repetition won't help
 * (no charges, requires recharge, or failure rate is 100% or more).
 */
static int check_devices(struct player *p, struct object *obj)
{
    int fail;
    const char *action;
    bool activated = false;
    bool is_unbeliever = streq(p->clazz->name, "Unbeliever");
    bool is_homi = streq(p->race->name, "Homunculus");

    /* Horns are not magical and therefore never fail */
    if (tval_is_horn(obj)) return 1;

    /* Get the right string */
    if (tval_is_rod(obj))
        action = "zap the rod";
    else if (tval_is_wand(obj))
        action = "use the wand";
    else if (tval_is_staff(obj))
        action = "use the staff";
    else
    {
        action = "activate it";
        activated = true;
    }

    /* Figure out how hard the item is to use */
    fail = get_use_device_chance(p, obj);

    ///////////////////////////////////////////////////////////
    // Hack to reduce MD chances for some classes:
    if (is_unbeliever || is_homi)
    {
        fail = 999;
        // allow ID certain rings (of Flames, of Acid etc), amu of the moon
        if (!obj->known->effect && (tval_is_ring(obj) || tval_is_amulet(obj)))
            fail = 500;
    }
    else if (fail < 700 - (p->lev * 10) &&
             (streq(p->clazz->name, "Warrior") || streq(p->clazz->name, "Monk") ||
              streq(p->clazz->name, "Shapechanger")))
    {
        fail = 700 - (p->lev * 10); // 70% -> 20%
    }
    else if (fail < 510 - (p->lev * 10) &&
             (streq(p->clazz->name, "Rogue") || streq(p->clazz->name, "Paladin") ||
             streq(p->clazz->name, "Blackguard") || streq(p->clazz->name, "Archer") ||
             streq(p->clazz->name, "Heretic") || streq(p->clazz->name, "Cutthroat")))
    {
        fail = 510 - (p->lev * 10); // 50% -> 1%
    }
    ///////////////////////////////////////////////////////////

    /* Roll for usage */
    // CHANCE(x, 1000) always rolls a die 0-999.
    // fail < 1000: return 0 (can retry)
    // fail >= 1000: return -1 (no point retrying) - but it won't happen in current code (?) as fail can't be >750
    // (except out Unbeliever or Homi cases)
    if (CHANCE(fail, 1000)) // <-- if it's true - we failed to use item
    {
        // non-HC: tele staves ez to activate for everyone (even Unbeliever)
        if (obj->tval == TV_STAFF && !OPT(p, birth_hardcore) &&
            obj->kind == lookup_kind_by_name(TV_STAFF, "Teleportation"))
        {
            bool final_failure = is_unbeliever ? one_in_(5)   // 20% fail for unbelievers
                                               : one_in_(20);  // 5% fail for everyone else

            if (final_failure) {
                msg(p, "You failed to %s properly.", action);
                return -1;
            }

            // for save-chance of using tp staff - consume another charge (except last one)
            if (obj->pval > 1) {
                obj->pval--;
            }
        }
        // regular case
        else
        {
            msg(p, "You failed to %s properly.", action);
            return ((fail < 1000)? 0 : -1);
        }
    }

    /* Notice activations */
    if (activated) object_notice_effect(p, obj);

    return 1;
}


/*
 * Print an artifact activation message.
 */
static void activation_message(struct player *p, struct object *obj)
{
    const char *message;

    /* See if we have a message */
    if (!obj->activation) return;
    if (!obj->activation->message) return;
    if (true_artifact_p(obj) && obj->artifact->alt_msg)
        message = obj->artifact->alt_msg;
    else
        message = obj->activation->message;

    print_custom_message(p, obj, message, MSG_GENERIC);
}


/*
 * Inscriptions
 */


/*
 * Remove inscription
 */
void do_cmd_uninscribe(struct player *p, int item)
{
    struct object *obj = object_from_index(p, item, true, true);

    /* Paranoia: requires an item */
    if (!obj) return;

    /* Restrict ghosts */
    /* One exception: players in undead form can uninscribe items (from pack only) */
    if (p->ghost && !(p->dm_flags & DM_GHOST_HANDS) &&
        !(player_undead(p) && object_is_carried(p, obj)))
    {
        msg(p, "You cannot uninscribe items!");
        return;
    }

    /* Check preventive inscription '!g' */
    if (!object_is_carried(p, obj) &&
        object_prevent_inscription(p, obj, INSCRIPTION_PICKUP, false))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Nothing to remove */
    if (!obj->note)
    {
        msg(p, "That item had no inscription to remove.");
        return;
    }

    /* Check preventive inscription '!}' */
    if (protected_p(p, obj, INSCRIPTION_UNINSCRIBE, false))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    obj->note = 0;
    msg(p, "Inscription removed.");

    /* PWMAngband: remove autoinscription if aware */
    if (p->kind_aware[obj->kind->kidx])
    {
        remove_autoinscription(p, obj->kind->kidx);
        Send_autoinscription(p, obj->kind);
    }

    /* Update global "preventive inscriptions" */
    update_prevent_inscriptions(p);

    /* Notice, update and redraw */
    p->upkeep->notice |= (PN_COMBINE | PN_IGNORE);
    p->upkeep->update |= (PU_INVEN);
    set_redraw_equip(p, obj);
    set_redraw_inven(p, obj);
    if (!object_is_carried(p, obj))
        redraw_floor(&p->wpos, &obj->grid, NULL);
}


/*
 * Add inscription
 */
void do_cmd_inscribe(struct player *p, int item, const char *inscription)
{
    struct object *obj = object_from_index(p, item, true, true);
    char o_name[NORMAL_WID];
    int32_t price;

    /* Paranoia: requires an item */
    if (!obj) return;

    /* Empty inscription: uninscribe the item instead */
    if (STRZERO(inscription))
    {
        do_cmd_uninscribe(p, item);
        return;
    }

    /* Restrict ghosts */
    /* One exception: players in undead form can inscribe items (from pack only) */
    if (p->ghost && !(p->dm_flags & DM_GHOST_HANDS) &&
        !(player_undead(p) && object_is_carried(p, obj)))
    {
        msg(p, "You cannot inscribe items!");
        return;
    }

    /* Check preventive inscription '!g' */
    if (!object_is_carried(p, obj) &&
        object_prevent_inscription(p, obj, INSCRIPTION_PICKUP, false))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Check preventive inscription '!{' */
    if (protected_p(p, obj, INSCRIPTION_INSCRIBE, true))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Check ownership */
    if (strstr(inscription, "!g") && (p->id != obj->owner))
    {
        msg(p, "You must own this item first.");
        return;
    }

    /* Don't allow certain inscriptions when selling */
    price = get_askprice(inscription);
    if (price >= 0)
    {
        /* Can't sell unindentified items */
        if (!object_is_known(p, obj))
        {
            msg(p, "You must identify this item first.");
            return;
        }

        /* Can't sell overpriced items */
        if (price > PY_MAX_GOLD)
        {
            msg(p, "Your price is too high!");
            return;
        }
    }

    /* Form prompt */
    object_desc(p, o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL);
    msg(p, "Inscribing %s.", o_name);
    message_flush(p);

    /* Save the inscription */
    obj->note = quark_add(inscription);

    /* PWMAngband: add autoinscription if aware and inscription has the right format (@xn) */
    if (p->kind_aware[obj->kind->kidx] && (strlen(inscription) == 3) && (inscription[0] == '@') &&
        isalpha(inscription[1]) && isdigit(inscription[2]))
    {
        add_autoinscription(p, obj->kind->kidx, inscription);
        Send_autoinscription(p, obj->kind);
    }

    /* Update global "preventive inscriptions" */
    update_prevent_inscriptions(p);

    /* Notice, update and redraw */
    p->upkeep->notice |= (PN_COMBINE | PN_IGNORE);
    p->upkeep->update |= (PU_INVEN);
    set_redraw_equip(p, obj);
    set_redraw_inven(p, obj);
    if (!object_is_carried(p, obj))
        redraw_floor(&p->wpos, &obj->grid, NULL);
}


/*
 * Examination
 */


void do_cmd_observe(struct player *p, int item)
{
    struct object *obj = object_from_index(p, item, true, true);
    char o_name[NORMAL_WID];

    /* Paranoia: requires an item */
    if (!obj) return;

    /* Track object for object recall */
    track_object(p->upkeep, obj);

    /* Get name */
    object_desc(p, o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL);

    /* Inform */
    msg(p, "Examining %s...", o_name);

    /* Capitalize object name for header */
    my_strcap(o_name);

    /* Display object recall modally and wait for a keypress */
    display_object_recall_interactive(p, obj, o_name);
}


/*
 * Taking off/putting on
 */


/*
 * Take off an item
 */
void do_cmd_takeoff(struct player *p, int item)
{
    struct object *obj = object_from_index(p, item, true, true);

    /* Paranoia: requires an item */
    if (!obj) return;

    /* Paranoia */
    if (!object_is_carried(p, obj)) return;

    /* Restrict ghosts */
    if (p->ghost && !(p->dm_flags & DM_GHOST_BODY))
    {
        msg(p, "You need a tangible body to remove items!");
        return;
    }

    /* Verify potential overflow */
    if (!inven_carry_okay(p, obj))
    {
        msg(p, "Your pack is full and would overflow!");
        return;
    }

    /* Check preventive inscription '!t' */
    if (object_prevent_inscription(p, obj, INSCRIPTION_TAKEOFF, false))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Item is stuck */
    if (!obj_can_takeoff(obj))
    {
        /* Oops */
        msg(p, "Hmmm, it seems to be stuck.");

        /* Nope */
        return;
    }

    // can't take off cursed items right on..
    if (obj->curses && (turn.turn % 8 != 0)) // // 7 out of 8 turns = 88% to fail
    {
        msg(p, "You fail to take off cursed item this time. Try ones more..");
        return;
    }

    /* Take half a turn */
    use_energy_aux(p, 50);

    /* Take off the item */
    inven_takeoff(p, obj);
    combine_pack(p);
}


/*
 * Hack -- prevent anyone but total winners (and the Dungeon Master) from wielding the Massive Iron
 * Crown of Morgoth or the Mighty Hammer 'Grond'.
 */
static bool deny_winner_artifacts(struct player *p, struct object *obj)
{
    if (!p->total_winner && !is_dm_p(p))
    {
        /*
         * Attempting to wear the crown if you are not a winner is a very,
         * very bad thing to do.
         */
        if (true_artifact_p(obj) && strstr(obj->artifact->name, "of Morgoth"))
        {
            msg(p, "You are blasted by the Crown's power!");

            /* This should pierce invulnerability */
            take_hit(p, 10000, "the Massive Iron Crown of Morgoth",
                "was blasted by the Massive Iron Crown of Morgoth");
            return true;
        }

        /* Attempting to wield Grond isn't so bad. */
        if (true_artifact_p(obj) && strstr(obj->artifact->name, "Grond"))
        {
            msg(p, "You are far too weak to wield the mighty Grond.");
            return true;
        }
    }

    return false;
}


/*
 * Wield or wear an item
 */
void do_cmd_wield(struct player *p, int item, int slot)
{
    struct object *equip_obj;
    char o_name[NORMAL_WID];
    const char *act;
    struct object *obj = object_from_index(p, item, true, true);
    char message[NORMAL_WID];

    /* Paranoia: requires an item */
    if (!obj) return;

    /* Restrict ghosts */
    if (p->ghost && !(p->dm_flags & DM_GHOST_BODY))
    {
        msg(p, "You need a tangible body to wield items!");
        return;
    }

    /* Some checks */
    if (!object_is_carried(p, obj))
    {
        /* Winners cannot pickup artifacts except the Crown and Grond */
        if (true_artifact_p(obj) && restrict_winner(p, obj))
        {
            msg(p, "You cannot wield that item anymore.");
            return;
        }

        /* Restricted by choice */
        if (obj->artifact && (cfg_no_artifacts || OPT(p, birth_no_artifacts)))
        {
            msg(p, "You cannot wield that item.");
            return;
        }

        /* Note that the pack is too heavy */
        if (p->upkeep->total_weight + obj->weight > weight_limit(&p->state) * 6)
        {
            msg(p, "You are already too burdened to wield that item.");
            return;
        }

        /* Restricted by choice */
        if (!is_owner(p, obj))
        {
            msg(p, "This item belongs to someone else!");
            return;
        }

        /* Must meet level requirement */
        if (!has_level_req(p, obj))
        {
            msg(p, "You don't have the required level!");
            return;
        }

        /* Check preventive inscription '!g' */
        if (object_prevent_inscription(p, obj, INSCRIPTION_PICKUP, false))
        {
            msg(p, "The item's inscription prevents it.");
            return;
        }
    }

    /* Check preventive inscription '!w' */
    if (object_prevent_inscription(p, obj, INSCRIPTION_WIELD, true))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Paranoia: requires proper item */
    if (!item_tester_hook_wear(p, obj)) return;

    /* Paranoia */
    if (slot == -1) return;

    /* Get the slot the object wants to go in, and the item currently there */
    equip_obj = slot_object(p, slot);

    /* If the slot is open, wield and be done */
    if (!equip_obj)
    {
        if (deny_winner_artifacts(p, obj)) return;
        inven_wield(p, obj, slot, NULL, 0);
        return;
    }

    /* Prevent wielding into a stuck slot */
    if (!obj_can_takeoff(equip_obj))
    {
        object_desc(p, o_name, sizeof(o_name), equip_obj, ODESC_BASE);
        msg(p, "The %s you are %s appears to be stuck.", o_name, equip_describe(p, slot));
        return;
    }

    // when we swap items, we need also to check for curse
    if (equip_obj->curses && (turn.turn % 8 != 0)) // 7 out of 8 turns = 88% to fail
    {
        msg(p, "You fail to take off cursed item this time. Try once more..");
        return;
    }

    /* Check preventive inscription '!t' */
    if (object_prevent_inscription(p, equip_obj, INSCRIPTION_TAKEOFF, false))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Never drop true artifacts above their base depth except the Crown and Grond */
    if (!inven_carry_okay(p, equip_obj) && !inven_drop_okay(p, equip_obj))
    {
        object_desc(p, o_name, sizeof(o_name), equip_obj, ODESC_BASE);
        msg(p, "Your pack is full and you can't drop the %s here.", o_name);
        return;
    }

    if (deny_winner_artifacts(p, obj)) return;

    /* Describe the object */
    object_desc(p, o_name, sizeof(o_name), equip_obj, ODESC_PREFIX | ODESC_FULL);

    /* Describe removal by slot */
    if (slot_type_is(p, slot, EQUIP_WEAPON))
        act = "You were wielding";
    else if (slot_type_is(p, slot, EQUIP_BOW) || slot_type_is(p, slot, EQUIP_LIGHT))
        act = "You were holding";
    else if (slot_type_is(p, slot, EQUIP_TOOL))
        act = "You were using";
    else
        act = "You were wearing";

    inven_wield(p, obj, slot, message, sizeof(message));

    /* Message */
    msg(p, "%s %s (%c).", act, o_name, gear_to_label(p, equip_obj));

    /* Sound */
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
        else if (obj->artifact)                // Phial, Star, Stone etc
            sound(p, MSG_ITEM_LIGHT_PHIAL);
    }
    else
        sound(p, MSG_WIELD);

    /* Message */
    msg_print(p, message, MSG_WIELD);
}


/*
 * Drop an item
 */
void do_cmd_drop(struct player *p, int item, int quantity)
{
    struct object *obj = object_from_index(p, item, true, true);

    /* Paranoia: requires an item */
    if (!obj) return;

    /* Paranoia */
    if (!object_is_carried(p, obj)) return;

    /* Restrict ghosts */
    if (p->ghost && !(p->dm_flags & DM_GHOST_BODY))
    {
        msg(p, "You need a tangible body to drop items!");
        return;
    }

    /* Handle the newbies_cannot_drop option */
    if (newbies_cannot_drop(p))
    {
        msg(p, "You are not experienced enough to drop items.");
        return;
    }

    /* Check preventive inscription '^d' */
    if (check_prevent_inscription(p, INSCRIPTION_DROP))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Check preventive inscription '!d' */
    if (object_prevent_inscription(p, obj, INSCRIPTION_DROP, false))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Cannot remove stuck items */
    if (object_is_equipped(p->body, obj) && !obj_can_takeoff(obj))
    {
        // you can drop such item on the ground (on surface) on 10-20-30-40-49 lvl
        // (except One Ring or Crown which are 100 lvl)
        
        // TODO: add possibility to take off such items in a very special cases...
        // drop Ring to lava? or pay gold to priest?
        // or able to do it during full moon only? :D some nethackish flavour
        
        // also it's now safe to make more items STICKY (TODO).
        
        if (obj->level_req < 100 && p->wpos.depth == 0 &&
           (p->lev == 10 || p->lev == 20 || p->lev == 30 || p->lev == 40 || p->lev == 49))
           ;
        else {
            msg(p, "Hmmm, it seems to be stuck.");
            return;
        }
    }

    /* Take half a turn */
    use_energy_aux(p, 50);

    /* Hack -- farmers plant seeds */
    if (tval_is_crop(obj) && square_iscropbase(chunk_get(&p->wpos), &p->grid))
    {
        do_cmd_plant_seed(p, obj);
        return;
    }

    /* Drop (some of) the item */
    inven_drop(p, obj, quantity, false);
}


/*
 * Destroy an item
 */
void do_cmd_destroy_aux(struct player *p, struct object *obj, bool des)
{
    char o_name[NORMAL_WID];

    /* Paranoia: requires an item */
    if (!obj) return;

    /* Restrict ghosts */
    if (des && p->ghost && !(p->dm_flags & DM_GHOST_BODY))
    {
        msg(p, "You need a tangible body to destroy items!");
        return;
    }

    /* Check preventive inscription '^k' */
    if (check_prevent_inscription(p, INSCRIPTION_DESTROY))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Some checks */
    if (!object_is_carried(p, obj))
    {
        /* Restricted by choice */
        if (!is_owner(p, obj))
        {
            msg(p, "This item belongs to someone else!");
            return;
        }

        /* Must meet level requirement */
        if (!has_level_req(p, obj))
        {
            msg(p, "You don't have the required level!");
            return;
        }

        /* Check preventive inscription '!g' */
        if (object_prevent_inscription(p, obj, INSCRIPTION_PICKUP, false))
        {
            msg(p, "The item's inscription prevents it.");
            return;
        }
    }

    /* Check preventive inscription '!k' */
    if (object_prevent_inscription(p, obj, INSCRIPTION_DESTROY, false))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Can't ignore or destroy stuck items we're wielding. */
    if (object_is_equipped(p->body, obj) && !obj_can_takeoff(obj))
    {
        /* Message */
        if (des) msg(p, "You cannot destroy the stuck item.");
        else msg(p, "You cannot ignore stuck equipment.");

        /* Done */
        return;
    }

    /* Describe */
    object_desc(p, o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL);

    /* Artifacts cannot be destroyed */
    // exception: soundbound randarts made by crafter class
    if (des && obj->artifact && !obj->soulbound)
    {
        /* Message */
        msg(p, "You cannot destroy %s.", o_name);

        /* Done */
        return;
    }

    // Can't destroy cursed items.
    // exception: you can destroy soundbound cursed items made by crafter class
    if (des && obj->curses && !obj->soulbound)
    {
        msg(p, "You cannot destroy %s as it's cursed.", o_name);
        msg(p, "Try to uncurse it first and then simply throw it away.");
        return;
    }

    /* Destroying or ignoring from equipment? Update object flags! */
    if (object_is_equipped(p->body, obj)) set_redraw_equip(p, NULL);

    /* Destroy */
    if (des)
    {
        /* Message */
        msgt(p, MSG_DESTROY, "You destroy %s.", o_name);

        // soulbound randarts made by crafter class should be destroyed in special way
        if (obj->artifact && !obj->soulbound)
        {
            // now properly delete artifacts
            preserve_artifact_aux(obj);
            history_lose_artifact(p, obj);
        }

        /* Eliminate the item */
        use_object(p, obj, obj->number, true);
    }

    /* Ignore */
    else
    {
        /* Message */
        if (obj->known->notice & OBJ_NOTICE_IGNORE)
            msgt(p, MSG_DESTROY, "Showing %s again.", o_name);
        else
            msgt(p, MSG_DESTROY, "Ignoring %s.", o_name);

        /* Set ignore flag as appropriate */
        p->upkeep->notice |= PN_IGNORE;

        /* Toggle ignore */
        if (obj->known->notice & OBJ_NOTICE_IGNORE)
            obj->known->notice &= ~OBJ_NOTICE_IGNORE;
        else
            obj->known->notice |= OBJ_NOTICE_IGNORE;

        set_redraw_inven(p, obj);
        if (object_is_carried(p, obj))
            set_redraw_inven(p, obj);
        else
            redraw_floor(&p->wpos, &obj->grid, NULL);
    }
}


/*
 * Destroy an item (by index)
 */
void do_cmd_destroy(struct player *p, int item, bool des)
{
    struct object *obj = object_from_index(p, item, true, true);

    do_cmd_destroy_aux(p, obj, des);
}


/*
 * Casting and browsing
 */


/*
 * Determine if a spell is "okay" for the player to cast or study
 * The spell must be legible, not forgotten, and also, to cast,
 * it must be known, and to study, it must not be known.
 */
static bool spell_okay(struct player *p, int spell_index, bool known)
{
    const struct class_spell *spell = spell_by_index(&p->clazz->magic, spell_index);

    /* Spell is illegible - never ok */
    if (spell->slevel > p->lev) return false;

    /* Spell is forgotten - never ok */
    if (p->spell_flags[spell_index] & PY_SPELL_FORGOTTEN) return false;

    /* Spell is learned - cast ok, no study */
    if (p->spell_flags[spell_index] & PY_SPELL_LEARNED) return (known);

    /* Spell has never been learned - study ok, no cast */
    return (!known);
}


/*
 * Allow user to choose a spell/prayer from the given book.
 *
 * Returns -1 if the user hits escape.
 * Returns -2 if there are no legal choices.
 * Returns a valid spell otherwise.
 *
 * The "prompt" should be "cast", "recite", "study", or "use"
 * The "known" should be true for cast/pray, false for study
 */
static int get_spell(struct player *p, struct object *obj, int spell_index, const char *prompt,
    bool known)
{
    const struct class_book *book = player_object_to_book(p, obj);
    int sidx;

    /* Set the spell number */
    if (spell_index < p->clazz->magic.total_spells)
        sidx = book->spells[spell_index].sidx;
    else
    {
        const struct class_spell *spell = &book->spells[spell_index - p->clazz->magic.total_spells];

        sidx = spell->sidx;

        /* Projected spells */
        if (!spell->sproj)
        {
            msg(p, "You cannot project that spell.");
            return -1;
        }
    }

    /* Verify the spell */
    if (!spell_okay(p, sidx, known))
    {
        if (prompt) msg(p, prompt);
        return -1;
    }

    return sidx;
}


/* Study a book to gain a new spell */
void do_cmd_study(struct player *p, int book_index, int spell_index)
{
    const struct class_book *book;
    int sidx = -1;
    const struct class_spell *spell;
    int i, k = 0;
    struct object *obj = object_from_index(p, book_index, true, true);

    /* Paranoia: requires an item */
    if (!obj) return;

    /* Restrict ghosts */
    /* One exception: players in undead form can read books (from pack only) */
    if (p->ghost && !(p->dm_flags & DM_GHOST_HANDS) &&
        !(player_undead(p) && object_is_carried(p, obj)))
    {
        msg(p, "You cannot read books!");
        return;
    }

    if (player_cannot_cast(p, true)) return;

    /* Check preventive inscription '^G' */
    if (check_prevent_inscription(p, INSCRIPTION_STUDY))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Restricted by choice */
    if (!object_is_carried(p, obj) && !is_owner(p, obj))
    {
        msg(p, "This item belongs to someone else!");
        return;
    }

    /* Must meet level requirement */
    if (!object_is_carried(p, obj) && !has_level_req(p, obj))
    {
        msg(p, "You don't have the required level!");
        return;
    }

    /* Get the book */
    book = player_object_to_book(p, obj);

    /* Requires a spellbook */
    if (!book) return;

    /* Elementalists can increase the power of their spells */
    if (streq(book->realm->name, "elemental"))
    {
        /* Check if spell is learned */
        sidx = get_spell(p, obj, spell_index, NULL, true);
        if (sidx != -1)
        {
            int max_power = ((obj->sval == lookup_sval(obj->tval, "[Elemental]"))? 10: 5);

            spell = &book->spells[spell_index];

            /* Check max spellpower */
            if (p->spell_power[sidx] == max_power)
            {
                msg(p, "You already know everything about this spell.");
                return;
            }

            /* Check level */
            if (spell->slevel + p->spell_power[sidx] > p->lev)
            {
                msg(p, "You are too low level to improve this spell.");
                return;
            }

            /* Check allocated points */
            max_power = 0;
            for (i = 0; i < p->clazz->magic.total_spells; i++)
                max_power += p->spell_power[i];
            if (max_power >= p->lev * 2)
            {
                msg(p, "You are too low level to improve this spell.");
                return;
            }

            /* Take a turn */
            use_energy(p);

            /* Improve spellpower */
            p->spell_power[sidx]++;

            /* Mention the result */
            msgt(p, MSG_STUDY, "You improve your knowledge of the %s spell.", spell->name);

            /* Redraw */
            p->upkeep->redraw |= PR_SPELL;

            return;
        }
    }

    if (!p->upkeep->new_spells)
    {
        msg(p, "You cannot learn any new %ss!", book->realm->spell_noun);
        return;
    }

    /* Spellcaster -- learn a selected spell */
    if (player_has(p, PF_CHOOSE_SPELLS))
    {
        const char *prompt = format("You cannot learn that %s!", book->realm->spell_noun);

        /* Ask for a spell */
        sidx = get_spell(p, obj, spell_index, prompt, false);

        /* Allow cancel */
        if (sidx == -1) return;

        spell = spell_by_index(&p->clazz->magic, sidx);
    }

    /* Cleric -- learn a random prayer */
    else
    {
        sidx = -1;

        /* Extract prayers */
        for (i = 0; i < book->num_spells; i++)
        {
            /* Skip non "okay" prayers */
            if (!spell_okay(p, book->spells[i].sidx, false)) continue;

            /* Apply the randomizer */
            if ((++k > 1) && randint0(k)) continue;

            /* Track it */
            sidx = book->spells[i].sidx;

            spell = &book->spells[i];
        }
    }

    /* Nothing to study */
    if (sidx < 0)
    {
        /* Message */
        msg(p, "You cannot learn any %ss in that book.", book->realm->spell_noun);

        /* Abort */
        return;
    }

    /* Take a turn */
    use_energy(p);

    /* Learn the spell */
    p->spell_flags[sidx] |= PY_SPELL_LEARNED;
    p->spell_power[sidx]++;

    /* Find the next open entry in "spell_order[]" */
    for (i = 0; i < p->clazz->magic.total_spells; i++)
    {
        /* Stop at the first empty space */
        if (p->spell_order[i] == 99) break;
    }

    /* Add the spell to the known list */
    p->spell_order[i++] = sidx;

    /* Mention the result */
    msgt(p, MSG_STUDY, "You have learned the %s of %s.", spell->realm->spell_noun, spell->name);

    /* One less spell available */
    p->upkeep->new_spells--;

    /* Message if needed */
    if (p->upkeep->new_spells)
    {
        /* Message */
        msg(p, "You can learn %d more %s%s.", p->upkeep->new_spells, book->realm->spell_noun,
            PLURAL(p->upkeep->new_spells));
    }

    /* Redraw */
    p->upkeep->redraw |= (PR_STUDY | PR_SPELL);
}


/* Cast the specified spell */
static bool spell_cast(struct player *p, int spell_index, int dir, quark_t note, bool projected)
{
    int chance;

    /* Spell failure chance */
    chance = spell_chance(p, spell_index);

    /* Fail or succeed */
    // some classes use "skills" which do not fail
    if (strcmp(p->clazz->name, "Knight") && strcmp(p->clazz->name, "Fighter") &&
        strcmp(p->clazz->name, "Scavenger") && magik(chance))
            msgt(p, MSG_SPELL_FAIL, "You failed to concentrate hard enough!");
    else
    {
        struct source who_body;
        struct source *who = &who_body;
        const struct class_spell *spell = spell_by_index(&p->clazz->magic, spell_index);
        bool pious = streq(spell->realm->name, "divine");

        /* Set current spell */
        p->current_spell = spell_index;

        /* Save current inscription */
        p->current_item = (int16_t)note;

        /* Only fire in direction 5 if we have a target */
        if ((dir == DIR_TARGET) && !target_okay(p)) return false;

        source_player(who, get_player_index(get_connection(p->conn)), p);

        /* Projected */
        if (projected)
        {
            project_aimed(who, PROJ_PROJECT, dir, spell_index,
                PROJECT_STOP | PROJECT_KILL | PROJECT_PLAY, "killed");
        }

        /* Cast the spell */
        else
        {
            bool ident = false, used;
            struct beam_info beam;

            fill_beam_info(p, spell_index, &beam);

            if (spell->effect && spell->effect->other_msg)
                msg_print_near(p, (pious? MSG_PY_PRAYER: MSG_PY_SPELL), spell->effect->other_msg);
            target_fix(p);
            used = effect_do(spell->effect, who, &ident, true, dir, &beam, 0, note, NULL);
            target_release(p);
            if (!used) return false;
        }

        /* Reward COMBAT_REGEN with small HP recovery */
        if (player_has(p, PF_COMBAT_REGEN)) convert_mana_to_hp(p, spell->smana << 16);

        /* A spell was cast */
        // spells' effect's indexes can be found:
        // effects.c -> effect_subtype()
        if (streq(p->clazz->name, "Knight"))
            ; // Knight got separate sound (player_timed.txt)
        else if (spell->effect->index == EF_BALL || spell->effect->index == EF_BALL_OBVIOUS ||
            spell->effect->index == EF_STAR_BALL || spell->effect->index == EF_SWARM)
            sound(p, MSG_BALL);
        else if ((spell->effect->index >= EF_BOLT && spell->effect->index <= EF_BOLT_STATUS_DAM) ||
                  spell->effect->index == EF_BOLT_RADIUS)
                    sound(p, MSG_BOLT);
        else if (spell->effect->index == EF_DAMAGE) // curse
            sound(p, MSG_DAMAGE);
        else if (spell->effect->index == EF_EARTHQUAKE)
            sound(p, MSG_EARTHQUAKE);
        else if (spell->effect->index == EF_DESTRUCTION)
            sound(p, MSG_DESTRUCTION);
        else if (spell->effect->index == EF_BLAST || spell->effect->index == EF_BLAST_OBVIOUS)
            sound(p, MSG_BLAST);
        else if (spell->effect->index == EF_BEAM || spell->effect->index == EF_BEAM_OBVIOUS || 
                 spell->effect->index == EF_SHORT_BEAM || spell->effect->index == EF_LINE)
            sound(p, MSG_BEAM);
        else if (spell->effect->index == EF_ARC)
            sound(p, MSG_ARC);
        else if (spell->effect->index == EF_STRIKE)
            sound(p, MSG_STRIKE);
        else if (spell->effect->index == EF_LASH)
            sound(p, MSG_LASH);
        else if (spell->effect->index == EF_MELEE_BLOWS)
            sound(p, MSG_MELEE_BLOWS);
        else if (spell->effect->index == EF_SPOT)
            sound(p, MSG_SPOT);
        else if (spell->effect->index == EF_STAR)
            sound(p, MSG_STAR);
        else if (spell->effect->index == EF_TOUCH || spell->effect->index == EF_TOUCH_AWARE)
            sound(p, MSG_TOUCH);
        else if (spell->effect->index == EF_CREATE_WALLS)
            sound(p, MSG_CREATE_WALLS);
        else if (spell->effect->index == EF_CREATE_TREES)
            sound(p, MSG_CREATE_TREES);
        else if (spell->effect->index == EF_GLYPH)
            sound(p, MSG_GLYPH);
        else if (spell->effect->index <= EF_PROJECT_LOS_AWARE)
            sound(p, MSG_PROJECT);
        else if (pious)
            sound(p, MSG_PRAYER);
        else
            sound(p, MSG_SPELL); // including EF_PROJECT

        cast_spell_end(p);

        /* Put on cooldown */
        if (spell->cooldown)
        {
            p->spell_cooldown[spell->sidx] = spell->cooldown;
            p->upkeep->redraw |= (PR_SPELL);
        }
    }

    /* Use some mana */
    use_mana(p);

    return true;
}


/*
 * Unknown item hook for get_item()
 */
static bool item_tester_unknown(struct player *p, const struct object *obj)
{
    return (object_runes_known(obj)? false: true);
}


/*
 * Returns true if there are any objects available to identify (whether on floor or in gear).
 */
static bool spell_identify_unknown_available(struct player *p)
{
    int floor_max = z_info->floor_size;
    struct object **floor_list = mem_zalloc(floor_max * sizeof(*floor_list));
    int floor_num;
    struct object *obj;
    bool unidentified_gear = false;

    floor_num = scan_floor(p, chunk_get(&p->wpos), floor_list, floor_max,
        OFLOOR_TEST | OFLOOR_SENSE | OFLOOR_VISIBLE, item_tester_unknown);
    for (obj = p->gear; obj; obj = obj->next)
    {
        if (object_test(p, item_tester_unknown, obj))
        {
            unidentified_gear = true;
            break;
        }
    }
    mem_free(floor_list);

    return (unidentified_gear || (floor_num > 0));
}


/*
 * Cast a spell from a book
 */
bool do_cmd_cast(struct player *p, int book_index, int spell_index, int dir)
{
    const char *prompt;
    const struct class_spell *spell;
    struct object *obj = object_from_index(p, book_index, true, true);
    const struct class_book *book;
    int sidx;

    /* Check energy */
    if (!has_energy(p, true)) return false;

    /* Paranoia: requires an item */
    if (!obj)
    {
        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Clear current */
    current_clear(p);

    /* Check the player can cast spells at all */
    if (player_cannot_cast(p, true))
    {
        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Check preventive inscription '^m' */
    if (check_prevent_inscription(p, INSCRIPTION_CAST))
    {
        msg(p, "The item's inscription prevents it.");

        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Restricted by choice */
    if (!object_is_carried(p, obj) && !is_owner(p, obj))
    {
        msg(p, "This item belongs to someone else!");

        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Must meet level requirement */
    if (!object_is_carried(p, obj) && !has_level_req(p, obj))
    {
        msg(p, "You don't have the required level!");

        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Get the book */
    book = player_object_to_book(p, obj);

    /* Requires a spellbook */
    if (!book)
    {
        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Check preventive inscription '!m' */
    if (object_prevent_inscription(p, obj, INSCRIPTION_CAST, false))
    {
        msg(p, "The item's inscription prevents it.");

        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Restrict ghosts */
    /* One exception: players in undead form can cast spells (from pack only) */
    if (p->ghost && !(p->dm_flags & DM_GHOST_HANDS) &&
        !(player_undead(p) && object_is_carried(p, obj)))
    {
        msg(p, "You cannot %s that %s.", book->realm->verb, book->realm->spell_noun);

        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Ask for a spell */
    prompt = format("You cannot %s that %s.", book->realm->verb, book->realm->spell_noun);
    sidx = get_spell(p, obj, spell_index, prompt, true);
    if (sidx == -1)
    {
        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Get the spell */
    spell = spell_by_index(&p->clazz->magic, sidx);

    /* Check for unknown objects to prevent wasted player turns. */
    if (spell_is_identify(p, sidx) && !spell_identify_unknown_available(p))
    {
        msg(p, "You have nothing to identify.");

        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Check mana */
    if ((spell->smana > p->csp) && !OPT(p, risky_casting))
    {
        /* Warning */
        msg(p, "You do not have enough mana to %s this %s.", spell->realm->verb,
            spell->realm->spell_noun);

        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Check cooldown */
    if (p->spell_cooldown[spell->sidx])
    {
        // Warning.. added there CD value
        msg(p, "This %s is on cooldown for %d turns.", spell->realm->spell_noun, p->spell_cooldown[spell->sidx]);

        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

            
    /* Antimagic field (no effect on psi powers which are not "magical") */
    // also some classes got skills, not spells
    if (strcmp(book->realm->name, "psi") && strcmp(p->clazz->name, "Knight") &&
        strcmp(p->clazz->name, "Fighter") && strcmp(p->clazz->name, "Scavenger") &&
        check_antimagic(p, chunk_get(&p->wpos), NULL))
    {
        use_energy(p);

        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Spell cost */
    p->spell_cost = spell->smana;

    /* Cast a spell */
    if (!spell_cast(p, sidx, dir, obj->note,
        (spell_index >= p->clazz->magic.total_spells)? true: false))
    {
        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Take a turn, or 75% a turn if fast casting */
    if (p->timed[TMD_FASTCAST]) use_energy_aux(p, 75);
    else use_energy(p);

    /* Repeat */
    if (p->firing_request) cmd_cast(p, book_index, spell_index, dir);
    return true;
}


/*
 * Using items the traditional way
 */


/* Basic tval testers */
static bool item_tester_hook_use(struct player *p, struct object *obj)
{
    /* Non-staves are out */
    if (!tval_is_staff(obj)) return false;

    /* Notice empty staves */
    if (obj->pval <= 0)
    {
        if (obj->number == 1) msg(p, "The staff has no charges left.");
        else msg(p, "The staves have no charges left.");
        return false;
    }

    /* Otherwise OK */
    return true;
}


static bool item_tester_hook_aim(struct player *p, struct object *obj)
{
    /* Non-wands are out */
    if (!tval_is_wand(obj)) return false;

    /* Notice empty wands */
    if (obj->pval <= 0)
    {
        if (obj->number == 1) msg(p, "The wand has no charges left.");
        else msg(p, "The wands have no charges left.");
        return false;
    }

    /* Otherwise OK */
    return true;
}


static bool item_tester_hook_eat(struct player *p, struct object *obj)
{
    return tval_is_edible(obj);
}


static bool item_tester_hook_quaff(struct player *p, struct object *obj)
{
    return tval_is_potion(obj);
}


static bool item_tester_hook_read(struct player *p, struct object *obj)
{
    return tval_is_scroll(obj);
}


/* Determine if an object is zappable */
static bool item_tester_hook_zap(struct player *p, struct object *obj)
{
    /* Non-rods are out */
    if (!tval_is_rod(obj)) return false;

    /* All still charging? */
    if (number_charging(obj) == obj->number)
    {
        msg(p, "The rod is still charging.");
        return false;
    }

    /* Otherwise OK */
    return true;
}


/* Determine if an object is activatable */
static bool item_tester_hook_activate(struct player *p, struct object *obj)
{
    /* Check the recharge */
    if (obj->timeout)
    {
        msg(p, "The item is still charging.");
        return false;
    }

    /* Check effect */
    if (object_effect(obj)) return true;

    /* Assume not */
    return false;
}


/* List of commands */
enum
{
    CMD_EAT = 0,
    CMD_QUAFF,
    CMD_READ,
    CMD_USE,
    CMD_AIM,
    CMD_ZAP,
    CMD_ACTIVATE
};


/* Types of item use */
enum
{
    USE_TIMEOUT,
    USE_CHARGE,
    USE_SINGLE
};


/* Command parameters */
typedef struct
{
    uint32_t dm_flag;
    bool player_undead;
    const char *msg_ghost;
    int p_note;
    int eq_only;
    int g_note;
    bool check_antimagic;
    int use;
    int snd;
    bool (*item_tester_hook)(struct player *, struct object *);
} cmd_param;


/* List of command parameters */
static cmd_param cmd_params[] =
{
    {DM_GHOST_BODY, false, "You need a tangible body to eat food!", INSCRIPTION_EAT, 0,
        INSCRIPTION_EAT, false, USE_SINGLE, MSG_EAT, item_tester_hook_eat},
    {DM_GHOST_BODY, false, "You need a tangible body to quaff potions!", INSCRIPTION_QUAFF, 0,
        INSCRIPTION_QUAFF, false, USE_SINGLE, MSG_QUAFF, item_tester_hook_quaff},
    {DM_GHOST_HANDS, true, "You cannot read scrolls!", INSCRIPTION_READ, 0,
        INSCRIPTION_READ, false, USE_SINGLE, MSG_GENERIC, item_tester_hook_read},
    {DM_GHOST_HANDS, true, "You cannot use staves!", INSCRIPTION_USE, 0,
        INSCRIPTION_USE, true, USE_CHARGE, MSG_USE_STAFF, item_tester_hook_use},
    {DM_GHOST_HANDS, true, "You cannot aim wands!", INSCRIPTION_AIM, 0,
        INSCRIPTION_AIM, true, USE_CHARGE, MSG_GENERIC, item_tester_hook_aim},
    {DM_GHOST_HANDS, true, "You cannot zap rods!", INSCRIPTION_ZAP, 0,
        INSCRIPTION_ZAP, true, USE_TIMEOUT, MSG_ZAP_ROD, item_tester_hook_zap},
    {DM_GHOST_BODY, true, "You need a tangible body to activate items!", -1, 1,
        INSCRIPTION_ACTIVATE, true, USE_TIMEOUT, MSG_ACT_ARTIFACT, item_tester_hook_activate}
};


/*
 * Use an item in the pack or on the floor. Returns true if the item has been completely
 * used up, false otherwise.
 */
static bool do_cmd_use_end(struct player *p, struct object *obj, bool ident, bool used, int use)
{
    bool none_left = false;

    /* Paranoia: requires an item */
    if (!obj) return false;

    /* Only take a turn if used */
    if (used) use_energy(p);

    /* ID the object by use if appropriate, otherwise, mark it as "tried" */
    if (ident && !p->was_aware)
        object_learn_on_use(p, obj);
    else if (used)
        object_flavor_tried(p, obj);

    /* Some uses are "free" */
    if (used)
    {
        /* Chargeables act differently to single-used items when not used up */
        if (use == USE_CHARGE)
        {
            /* Use a single charge */
            obj->pval--;

            /* Describe charges */
            if (object_is_carried(p, obj))
                inven_item_charges(p, obj);

            /* Redraw */
            else
                redraw_floor(&p->wpos, &obj->grid, obj);
        }
        else if (use == USE_TIMEOUT)
        {
            /* Rods: drain the charge */
            if (tval_can_have_timeout(obj))
            {
                obj->timeout += randcalc(obj->time, 0, RANDOMISE);

                /* Redraw */
                if (!object_is_carried(p, obj))
                    redraw_floor(&p->wpos, &obj->grid, obj);
            }

            /* Other activatable items */
            else
                obj->timeout = randcalc(obj->time, 0, RANDOMISE);
        }
        else if (use == USE_SINGLE) // potions, scrolls, food etc
        {
            // zeitnot consume Phase Door in x% cases
            bool useScroll = true;
            
            if (OPT(p, birth_zeitnot))
            {
                // zeitnot preserve Phase Doors
                if (obj->tval == TV_SCROLL)
                {
                    if (obj->kind == lookup_kind_by_name(TV_SCROLL, "Phase Door")) {
                        int chance = 30 - (p->lev / 2); 
                        useScroll = magik(chance); // 30% -> 5%
                        // eg magik(25) has 25% chance to return true
                    }
                }
                // zeitnot preserve healing potions
                else if (obj->tval == TV_POTION)
                {
                    int preserve_chance = 0;
                    
                    if (obj->kind == lookup_kind_by_name(TV_POTION, "Cure Light Wounds"))
                        preserve_chance = MIN(65 + p->lev, 95); // 65% + 1% per level, capped at 95%
                    else if (obj->kind == lookup_kind_by_name(TV_POTION, "Cure Serious Wounds"))
                        preserve_chance = MIN(60 + p->lev, 95);
                    else if (obj->kind == lookup_kind_by_name(TV_POTION, "Cure Critical Wounds"))
                        preserve_chance = MIN(55 + p->lev, 95);
                    else if (obj->kind == lookup_kind_by_name(TV_POTION, "Healing"))
                        preserve_chance = MIN(50 + p->lev, 95);
                    
                    if (preserve_chance > 0)
                        useScroll = !magik(preserve_chance);
                }
            }

            // Regular use of single-use objects
            if (useScroll)
            {
                /* Log ownership change (in case we use item from the floor) */
                object_audit(p, obj);

                /* Destroy an item */
                none_left = use_object(p, obj, 1, true);
            }
        }
    }

    /* Mark as tried and redisplay */
    p->upkeep->notice |= (PN_COMBINE);
    set_redraw_equip(p, obj);
    set_redraw_inven(p, obj);

    /* Hack -- delay pack updates when an item request is pending */
    if (p->current_value == ITEM_PENDING) p->upkeep->notice |= PN_WAIT;
    else p->upkeep->notice &= ~(PN_WAIT);

    return none_left;
}


// apply light damage to vampires
static void vampire_light_damage(struct player *p)
{
    // damage but don't kill the player
    int dmg = p->mhp / 5;
    p->chp -= dmg;
    if (p->chp < 1)
        p->chp = 1;
    
    p->upkeep->redraw |= (PR_HP);
    msg(p, "The bright light burns your undead flesh!");
}


bool execute_effect(struct player *p, struct object **obj_address, struct effect *effect, int dir,
    const char *inscription, bool *ident, bool *used, bool *notice)
{
    struct beam_info beam;
    int boost, level;
    uint16_t tval;
    quark_t note;
    bool no_ident = false;
    struct effect *e = effect;
    struct source who_body;
    struct source *who = &who_body;

    /* Get the level difficulty */
    level = get_object_level(p, *obj_address, true);

    /* Boost damage effects if skill > difficulty */
    boost = MAX((p->state.skills[SKILL_DEVICE] - level) / 2, 0);

    /* Various hacks */
    tval = (*obj_address)->tval;
    note = (*obj_address)->note;
    while (e && !no_ident)
    {
        switch (e->index)
        {
            /* Altering and teleporting */
            case EF_ALTER_REALITY:
            case EF_BREATH:
            case EF_GLYPH:
            case EF_TELEPORT:
            case EF_TELEPORT_LEVEL:
            case EF_TELEPORT_TO:
            {
                /* Use up the item first */
                if (tval_is_staff(*obj_address))
                    do_cmd_use_staff_discharge(p, *obj_address, true, true);
                else if (tval_is_scroll(*obj_address) &&
                    do_cmd_read_scroll_end(p, *obj_address, true, true))
                {
                    *obj_address = NULL;
                }
                else if (tval_is_potion(*obj_address) &&
                    do_cmd_use_end(p, *obj_address, true, true, USE_SINGLE))
                {
                    *obj_address = NULL;
                }

                /* Already used up, don't call do_cmd_use_end again */
                no_ident = true;
                break;
            }

            /* Experience gain */
            case EF_GAIN_EXP:
            {
                /* Limit the effect of the Potion of Experience */
                if (((*obj_address)->owner && (p->id != (*obj_address)->owner)) ||
                    ((*obj_address)->origin == ORIGIN_STORE) || ((*obj_address)->askprice == 1))
                {
                    e->subtype = 1;
                }

                break;
            }

            /* Polymorphing */
            case EF_POLY_RACE:
            {
                /* Monster race */
                boost = (*obj_address)->modifiers[OBJ_MOD_POLY_RACE];

                break;
            }

            // when vamp uses light LINE items
            case EF_LINE:
            {
                // vampires take damage from using light objects
                if (e->subtype == PROJ_LIGHT_WEAK && streq(p->race->name, "Vampire"))
                    vampire_light_damage(p);
                break;
            }
        }

        e = e->next;
    }

    fill_beam_info(NULL, tval, &beam);
    my_strcpy(beam.inscription, inscription, sizeof(beam.inscription));

    /* Do effect */
    if (effect->other_msg) msg_misc(p, effect->other_msg);
    source_player(who, get_player_index(get_connection(p->conn)), p);
    target_fix(p);
    *used = effect_do(effect, who, ident, p->was_aware, dir, &beam, boost, note, NULL);
    target_release(p);

    /* Notice */
    if (*ident) *notice = true;
    if (no_ident) *ident = false;

    /* Quit if the item wasn't used and no knowledge was gained */
    return (!*used && (p->was_aware || !*ident));
}


/*
 * Use an object the right way.
 *
 * Returns true if repeated commands may continue.
 */
static bool use_aux(struct player *p, int item, int dir, cmd_param *p_cmd)
{
    struct object *obj = object_from_index(p, item, true, true);
    struct effect *effect;
    bool ident = false, used = false;
    int can_use = 1;
    bool notice = false;
    int aim;

    /* Paranoia: requires an item */
    if (!obj) return false;

    /* Clear current */
    current_clear(p);

    /* Set current item */
    p->current_item = item;

    /* Restrict ghosts */
    /* Sometimes players in undead form can use items (from pack only) */
    if (p->ghost && !(p->dm_flags & p_cmd->dm_flag) &&
        !(p_cmd->player_undead && player_undead(p) && object_is_carried(p, obj)))
    {
        msg(p, p_cmd->msg_ghost);
        return false;
    }

    /* Check preventive inscription */
    if ((p_cmd->p_note >= 0) && check_prevent_inscription(p, p_cmd->p_note))
    {
        msg(p, "The item's inscription prevents it.");
        return false;
    }

    /* Hack -- restrict to equipped items */
    if (p_cmd->eq_only && !object_is_equipped(p->body, obj)) return false;

    /* Some checks */
    if (!object_is_carried(p, obj))
    {
        /* Restricted by choice */
        if (!is_owner(p, obj))
        {
            msg(p, "This item belongs to someone else!");
            return false;
        }

        /* Must meet level requirement */
        if (!has_level_req(p, obj))
        {
            msg(p, "You don't have the required level!");
            return false;
        }

        /* Check preventive inscription '!g' */
        if (object_prevent_inscription(p, obj, INSCRIPTION_PICKUP, false))
        {
            msg(p, "The item's inscription prevents it.");
            return false;
        }
    }

    /* Paranoia: requires a proper object */
    if (!p_cmd->item_tester_hook(p, obj)) return false;

    /* Check preventive inscription */
    if (object_prevent_inscription(p, obj, p_cmd->g_note, false))
    {
        msg(p, "The item's inscription prevents it.");
        return false;
    }

    /* Antimagic field (except horns which are not magical) */
    if (p_cmd->check_antimagic && !tval_is_horn(obj) &&
        check_antimagic(p, chunk_get(&p->wpos), NULL))
    {
        use_energy(p);
        return false;
    }

    // no potions for djinni
    if (obj->tval == TV_POTION && streq(p->race->name, "Djinn"))
    {
        use_energy(p);
        return false;
    }
    // no scrolls for troll
    else if (obj->tval == TV_SCROLL && streq(p->race->name, "Troll") &&
             !(obj->kind == lookup_kind_by_name(TV_SCROLL, "Word of Recall")))
    {
        use_energy(p);
        return false;
    }

    /* The player is aware of the object's flavour */
    p->was_aware = object_flavor_is_aware(p, obj);

    /* Track the object used */
    track_object(p->upkeep, obj);

    /* Figure out effect to use */
    effect = object_effect(obj);

    /* Verify effect */
    my_assert(effect);

    /* Check for unknown objects to prevent wasted player turns. */
    /* PWMAngband: allow to ID the effect by use */
    if ((effect->index == EF_IDENTIFY) && p->was_aware && !spell_identify_unknown_available(p))
    {
        msg(p, "You have nothing to identify.");
        return false;
    }

    aim = obj_needs_aim(p, obj);
    if (aim != AIM_NONE)
    {
        /* Determine whether we know an item needs to be aimed */
        bool known_aim = (aim == AIM_NORMAL);

        /* Unknown things with no obvious aim get a random direction */
        if (!known_aim) dir = ddd[randint0(8)];

        /* Confusion wrecks aim */
        player_confuse_dir(p, &dir);
    }

    /* Check for use if necessary */
    if ((p_cmd->use == USE_CHARGE) || (p_cmd->use == USE_TIMEOUT))
        can_use = check_devices(p, obj);

    /* Execute the effect */
    if (can_use > 0)
    {
        /* Sound and/or message */
        sound(p, p_cmd->snd);
        activation_message(p, obj);

        if (execute_effect(p, &obj, effect, dir, "", &ident, &used, &notice)) return false;
    }

    /* Take a turn if device failed */
    else
        use_energy(p);

    if (obj->kind == lookup_kind_by_name(TV_FOOD, "Draught of the Ents") &&
        streq(p->race->name, "Ent"))
            player_inc_timed(p, TMD_FOOD, 5000, false, false);

/// potion of water. gives additional satiation (+ to object.txt) if hungry
    if (obj->tval == TV_POTION)
    {
        if (obj->kind == lookup_kind_by_name(TV_POTION, "Water") &&
            !streq(p->race->name, "Vampire") && !streq(p->race->name, "Undead") &&
            !streq(p->race->name, "Golem") && !streq(p->race->name, "Wraith") && 
            !streq(p->race->name, "Djinn"))
        {
            int satiation = 0;
            
            // Calculate base satiation bonus based on hunger level
            if (p->timed[TMD_FOOD] < 100) { // starving
                satiation += 400;  // regular water: 300
            } else if (p->timed[TMD_FOOD] < 400) { // faint
                satiation += 300;  // regular water: 200
            } else if (p->timed[TMD_FOOD] < 800) { // weak
                satiation += 200;  // regular water: 100
            } else if ((OPT(p, birth_zeitnot) || // zeitnot
                (OPT(p, birth_no_recall) && OPT(p, birth_force_descend))) &&
                 p->timed[TMD_FOOD] < 8000)
                    satiation += 1000; // regular water: 750
            
            if (streq(p->race->name, "Ent"))
            {   // main source of food
                player_inc_timed(p, TMD_FOOD, 300 + satiation, false, false);
                hp_player(p, p->wpos.depth / 2);
            }
            else if (streq(p->race->name, "Merfolk"))
            {
                player_inc_timed(p, TMD_FOOD, 200 + satiation, false, false);
                hp_player(p, p->wpos.depth);
            }
            else // all other races
                player_inc_timed(p, TMD_FOOD, 100 + satiation, false, false);
        }
        else if (obj->kind == lookup_kind_by_name(TV_POTION, "Death"))
        {
            // cheated death? :)
            if (p->chp == 1)
                p->chp = p->mhp;
            else
                p->chp = (p->chp / 2) + 1; // don't kill player

            // heal and feed vamps and corpses
            if (streq(p->race->name, "Vampire") || streq(p->race->name, "Undead"))
            {
                p->chp = p->mhp;
                player_inc_timed(p, TMD_FOOD, 2334, false, false);
            }

            /* Display the hitpoints */
            p->upkeep->redraw |= (PR_HP);
        }
        else if (streq(p->race->name, "Ent"))
        {
           int ent_food = 0;
           int c0st = obj->kind->cost;

           if      (c0st <= 10)   ent_food = 2;
           else if (c0st <= 25)   ent_food = 4;
           else if (c0st <= 50)   ent_food = 8;
           else if (c0st <= 100)  ent_food = 15;
           else if (c0st <= 200)  ent_food = 30;
           else if (c0st <= 500)  ent_food = 50;
           else if (c0st <= 1000) ent_food = 75;
           else if (c0st <= 5000) ent_food = 100;
           else if (c0st > 5000)  ent_food = 150;

           player_inc_timed(p, TMD_FOOD, ent_food, false, false);
        }
        // healing potions spend satiation if not too hungry
        else if (p->timed[TMD_FOOD] >= 700)
        {
            if (obj->kind == lookup_kind_by_name(TV_POTION, "Cure Light Wounds") &&
                one_in_(4))
                player_dec_timed(p, TMD_FOOD, 100, false);
            else if (obj->kind == lookup_kind_by_name(TV_POTION, "Cure Serious Wounds") &&
                one_in_(3))
                player_dec_timed(p, TMD_FOOD, 100, false);
            else if (obj->kind == lookup_kind_by_name(TV_POTION, "Cure Critical Wounds") &&
                one_in_(2))
                player_dec_timed(p, TMD_FOOD, 100, false);
            else if (obj->kind == lookup_kind_by_name(TV_POTION, "Healing"))
                player_dec_timed(p, TMD_FOOD, 100, false);
        }
        // drinking any other potions when hungry helps a bit
        else
        {
            if (p->timed[TMD_FOOD] < 100) // starving
                player_inc_timed(p, TMD_FOOD, 200, false, false);
            else if (p->timed[TMD_FOOD] < 400) // faint
                player_inc_timed(p, TMD_FOOD, 150, false, false);
            else if (p->timed[TMD_FOOD] < 700) // (weak is 800, but we use 700 there)
                player_inc_timed(p, TMD_FOOD, 100, false, false);
        }
    }

    if (streq(obj->kind->name, "Scrap of Flesh"))
    {
        if (streq(p->race->name, "Hydra") || streq(p->race->name, "Vampire") ||
            streq(p->race->name, "Undead"))
            player_inc_timed(p, TMD_FOOD, 2000, false, false);
        else if (one_in_(3))
            player_dec_timed(p, TMD_FOOD, 2000, false);
        else
            player_inc_timed(p, TMD_FOOD, 2000, false, false);
    }

    /* If the item is a null pointer or has been wiped, be done now */
    if (!obj) return false;

    if (notice) object_notice_effect(p, obj);

    // notice: there MD will lose it's change:
    /* Use the object, check if none left */
    if (do_cmd_use_end(p, obj, ident, used, p_cmd->use)) return false;

    /* Hack -- rings of polymorphing get destroyed when activated */
    if (tval_is_poly(obj) && used)
    {
        msg(p, "Your ring explodes in a bright flash of light!");
        use_object(p, obj, 1, true);
        return false;
    }

    return (can_use == 0);
}


/* Use a staff */
bool do_cmd_use_staff(struct player *p, int item, int dir)
{
    /* Cancel repeat */
    if (!p->device_request) return true;

    /* Check energy */
    if (!has_energy(p, true)) return false;

    /* Use the object, disable autorepetition when successful */
    if (!use_aux(p, item, dir, &cmd_params[CMD_USE]))
        p->device_request = 0;

    /* Repeat */
    if (p->device_request > 0) p->device_request--;
    if (p->device_request > 0) cmd_use(p, item, dir);
    return true;
}


/* Aim a wand */
bool do_cmd_aim_wand(struct player *p, int item, int dir)
{
    /* Cancel repeat */
    if (!p->device_request) return true;

    /* Check energy */
    if (!has_energy(p, true)) return false;

    /* Use the object, disable autorepetition when successful */
    if (!use_aux(p, item, dir, &cmd_params[CMD_AIM]))
        p->device_request = 0;

    /* Repeat */
    if (p->device_request > 0) p->device_request--;
    if (p->device_request > 0) cmd_aim_wand(p, item, dir);
    return true;
}


/* Zap a rod */
bool do_cmd_zap_rod(struct player *p, int item, int dir)
{
    /* Cancel repeat */
    if (!p->device_request) return true;

    /* Check energy */
    if (!has_energy(p, true)) return false;

    /* Use the object, disable autorepetition when successful */
    if (!use_aux(p, item, dir, &cmd_params[CMD_ZAP]))
        p->device_request = 0;

    /* Repeat */
    if (p->device_request > 0) p->device_request--;
    if (p->device_request > 0) cmd_zap(p, item, dir);
    return true;
}


/* Activate a wielded object */
bool do_cmd_activate(struct player *p, int item, int dir)
{
    /* Cancel repeat */
    if (!p->device_request) return true;

    /* Check energy */
    if (!has_energy(p, true)) return false;

    /* Use the object, disable autorepetition when successful */
    if (!use_aux(p, item, dir, &cmd_params[CMD_ACTIVATE]))
        p->device_request = 0;

    /* Repeat */
    if (p->device_request > 0) p->device_request--;
    if (p->device_request > 0) cmd_activate(p, item, dir);
    return true;
}


/* Eat some food */
void do_cmd_eat_food(struct player *p, int item)
{
    /* Use the object */
    use_aux(p, item, 0, &cmd_params[CMD_EAT]);
}


/* Quaff a potion (from the pack or the floor) */
void do_cmd_quaff_potion(struct player *p, int item, int dir)
{
    /* Use the object */
    use_aux(p, item, dir, &cmd_params[CMD_QUAFF]);
}


/* Determine if the player can read scrolls. */
static bool can_read_scroll(struct player *p)
{
    if (p->timed[TMD_BLIND] || p->timed[TMD_BLIND_REAL])
    {
        msg(p, "You can't see anything.");
        return false;
    }

    if (no_light(p))
    {
        msg(p, "You have no light to read by.");
        return false;
    }

    if (p->timed[TMD_CONFUSED] || p->timed[TMD_CONFUSED_REAL])
    {
        msg(p, "You are too confused to read!");
        return false;
    }

    if (one_in_(2) && p->timed[TMD_AMNESIA])
    {
        msg(p, "You can't remember how to read!");
        return false;
    }

    return true;
}


/* Read a scroll (from the pack or floor) */
void do_cmd_read_scroll(struct player *p, int item, int dir)
{
    /* Check some conditions */
    if (!can_read_scroll(p)) return;

    /* Use the object */
    use_aux(p, item, dir, &cmd_params[CMD_READ]);
}


/* Use an item */
bool do_cmd_use_any(struct player *p, int item, int dir)
{
    struct object *obj = object_from_index(p, item, true, true);

    /* Paranoia: requires an item */
    if (!obj) return true;

    /* Check energy */
    if (!has_energy(p, true)) return false;

    /*
     * If this is not a staff, wand, rod, or activatable item, always
     * disable autorepetition. The functions for handling a staff, wand
     * rod, or activatable item take care of autorepetition for those
     * objects.
     */

    /* Fire a missile */
    if (obj->tval == p->state.ammo_tval)
    {
        do_cmd_fire(p, dir, item);
        p->device_request = 0;
    }

    /* Eat some food */
    else if (item_tester_hook_eat(p, obj))
    {
        do_cmd_eat_food(p, item);
        p->device_request = 0;
    }

    /* Quaff a potion */
    else if (item_tester_hook_quaff(p, obj))
    {
        do_cmd_quaff_potion(p, item, dir);
        p->device_request = 0;
    }

    /* Read a scroll */
    else if (item_tester_hook_read(p, obj))
    {
        do_cmd_read_scroll(p, item, dir);
        p->device_request = 0;
    }

    /* Use a staff */
    else if (item_tester_hook_use(p, obj)) do_cmd_use_staff(p, item, dir);

    /* Aim a wand */
    else if (item_tester_hook_aim(p, obj)) do_cmd_aim_wand(p, item, dir);

    /* Zap a rod */
    else if (item_tester_hook_zap(p, obj)) do_cmd_zap_rod(p, item, dir);

    /* Activate a wielded object */
    else if (object_is_equipped(p->body, obj) && item_tester_hook_activate(p, obj))
        do_cmd_activate(p, item, dir);

    /* Oops */
    else
    {
        msg(p, "You cannot use that!");
        p->device_request = 0;
    }

    return true;
}


// Number of cases HARDCODED in consume_magic_items()
const char* consume_magic_message(int rng)
{
    switch(rng)
    {
        case 0: return "The arcane essence flows into my being like desert wind.";
        case 1: return "Ancient power courses through me, granting strength.";
        case 2: return "My mystical hunger is satisfied by this enchantment.";
        case 3: return "The magic tastes of starlight and forgotten wishes.";
        case 4: return "Ethereal flames dance within, illuminating my essence.";
        case 5: return "Warmth of a thousand suns fills my spectral form.";
        case 6: return "The spell's wisdom clears the mists from my mind.";
        case 7: return "I resonate with the consumed arcane frequencies.";
        case 8: return "Lightning of the ancients sparks within my core.";
        case 9: return "Mystical energy awakens every fiber of my being.";
        case 10: return "The magic dissolves sweetly upon my astral tongue.";
        case 11: return "Power eternal banishes the weariness from my soul.";
        case 12: return "This arcane morsel awakens my slumbering might.";
        case 13: return "The void within me fills with captured enchantment.";
        case 14: return "Raw power flows down my throat like liquid starfire.";
        case 15: return "The magic intoxicates me with its pure essence.";
        case 16: return "Arcane energy seeps into my bones like morning dew.";
        case 17: return "The spell's nectar makes my spirit dance with joy.";
        case 18: return "This magical feast strengthens my ethereal form.";
        case 19: return "My inner light blazes anew with consumed power.";
        case 20: return "A single taste of magic brings bliss to my essence.";
        case 21: return "The arcane substance thrills every part of my being.";
        case 22: return "Magic settles within me like precious amber.";
        case 23: return "I drink deep of the glowing enchantment's power.";
        case 24: return "This delicious spark prepares me for great deeds.";
        case 25: return "Pure energy slides down my throat like desert honey.";
        case 26: return "The magic suffuses my limbs with renewed vigor.";
        case 27: return "Euphoria bubbles within as the power settles.";
        case 28: return "I release the magic's essence with satisfaction.";
        default: return "The consumed magic brings peace to my ancient soul.";
    }
}


// Djinn race can use wands and staves for nourishment
bool consume_magic_items(struct player *p)
{
    bool mdevice_found = false;
    int rng;
    int i;

    // Calculate the maximum safe amount of food to add
    int max_safe_food;
    if (p->timed[TMD_FOOD] >= PY_FOOD_FULL) {
        // Already full, be careful about overfeeding
        max_safe_food = PY_FOOD_MAX - p->timed[TMD_FOOD];
        if (max_safe_food <= 0) {
            msg(p, "You are full of arcane energy already.");
            return false;  // Already at maximum, don't consume magic
        }
    } else {
        // Not yet full, can consume normally
        max_safe_food = PY_FOOD_MAX - p->timed[TMD_FOOD];
    }

    // search for wands and staves
    for (i = 0; i < z_info->pack_size; i++)
    {
        struct object *obj = p->upkeep->inven[i];

        if (!obj) continue;
        if (!object_is_carried(p, obj)) continue;

        // wands
        if (obj->tval == TV_WAND)
        {
            int food_amount = 10; // Base food value for wands
            int charge_bonus = 0;
            
            // Add bonus based on remaining charges
            if (obj->pval > 0) {
                charge_bonus = obj->pval * 10; // 10 food per charge
            }
            
            food_amount += charge_bonus;
            
            // Limit consumption to avoid overfeeding
            food_amount = MIN(food_amount, max_safe_food);
            
            use_object(p, obj, 1, false);
            // nourish and heal
            player_inc_timed(p, TMD_FOOD, food_amount, false, false);
            hp_player(p, p->wpos.depth / 3);
            mdevice_found = true;
            break;
        }
        // staves
        else if (obj->tval == TV_STAFF)
        {
            int food_amount = 15; // Base food value for staves
            int charge_bonus = 0;
            
            // Add bonus based on remaining charges
            if (obj->pval > 0) {
                charge_bonus = obj->pval * 15; // 15 food per charge
            }
            
            food_amount += charge_bonus;
            
            // Limit consumption to avoid overfeeding
            food_amount = MIN(food_amount, max_safe_food);
            
            use_object(p, obj, 1, false);
            // nourish and heal
            player_inc_timed(p, TMD_FOOD, food_amount, false, false);
            hp_player(p, p->wpos.depth / 3);
            mdevice_found = true;
            break;
        }
    }

    // no magic items found -> quit
    if (!mdevice_found) return false;

    // generate message
    rng = randint0(29); // Total number of messages in get_magic_message()
    msg(p, consume_magic_message(rng));

    // refresh inventory
    p->upkeep->redraw |= (PR_INVEN);

    return true;
}


// Number of cases HARDCODED in use_oil()
const char* get_oil_message(int rng)
{
    switch(rng)
    {
        case 0: return "Oil. Consumed. Nourishment. Detected.";
        case 1: return "Lubricant. Absorbed. System. Rejuvenated.";
        case 2: return "Fuel. Consumed. Mechanism. Recharged.";
        case 3: return "Oil. Imbibed. Vitality. Boosted.";
        case 4: return "Hydraulic. Fluid. Assimilated. Strength. Renewed.";
        case 5: return "Energy. Infused. Golem. Reactivated.";
        case 6: return "Lubrication. Consumed. Power. Amplified.";
        case 7: return "Circuitry. Replenished. Oil. Utilized.";
        case 8: return "Machinery. Refueled. Golem. Empowered.";
        case 9: return "Oil. Lubricated. Vitality. Restored.";
        case 10: return "Grease. Digested. Components. Energized.";
        case 11: return "Golem. Reinvigorated. Oil. Consumed.";
        case 12: return "Lubricant. Processed. Energy. Restored.";
        case 13: return "Oil. Ingested. Mechanism. Rebooted.";
        case 14: return "Fuel. Consumed. Power. Regenerated.";
        case 15: return "Oil. Imbibed. Golem. Reenergized.";
        case 16: return "Hydraulic. Fluid. Assimilated. Function. Optimized.";
        case 17: return "Energy. Infused. System. Restarted.";
        case 18: return "Lubrication. Consumed. Machinery. Reinforced.";
        case 19: return "Circuitry. Replenished. Golem. Reactivated.";
        case 20: return "Golem. Refueled. Vitality. Boosted.";
        case 21: return "Grease. Digested. Power. Amplified.";
        case 22: return "Oil. Consumed. Function. Enhanced.";
        case 23: return "Fuel. Processed. Golem. Regenerated.";
        case 24: return "Hydraulic. Fluid. Ingested. Energy. Renewed.";
        case 25: return "Lubricant. Consumed. System. Recharged.";
        case 26: return "Circuitry. Replenished. Vitality. Restored.";
        case 27: return "Machinery. Reinvigorated. Oil. Absorbed.";
        case 28: return "Petroleum. Ingested. Golem. Revitalized.";
        default: return "Oil consumed."; // Fallback message
    }
}


// Golem race can use oil for good
bool use_oil(struct player *p)
{
    bool fuel_found = false;
    int rng;
    int i;

    // Calculate the maximum safe amount of food to add
    int max_safe_food;
    if (p->timed[TMD_FOOD] >= PY_FOOD_FULL) {
        // Already full, be careful about overfeeding
        max_safe_food = PY_FOOD_MAX - p->timed[TMD_FOOD];
        if (max_safe_food <= 0) {
            msg(p, "Your oil reservoir is already full. Consumption aborted.");
            return false;  // Already at maximum, don't consume oil
        }
    } else {
        // Not yet full, can consume normally
        max_safe_food = PY_FOOD_MAX - p->timed[TMD_FOOD];
    }

    // search for oil
    for (i = 0; i < z_info->pack_size; i++)
    {
        struct object *obj = p->upkeep->inven[i];

        if (!obj) continue;
        if (!object_is_carried(p, obj)) continue;

        // oil flasks
        if (obj->tval == TV_FLASK)
        {
            int food_amount = 750;
            
            // Limit consumption to avoid overfeeding
            food_amount = MIN(food_amount, max_safe_food);
            
            use_object(p, obj, 1, false);
            // nourish and heal
            player_inc_timed(p, TMD_FOOD, food_amount, false, false);
            hp_player(p, p->wpos.depth / 2);
            fuel_found = true;
            break;
        }
        // Special food items
        else if (obj->tval == TV_FOOD)
        {
            int food_amount = 0;

            if (obj->kind == lookup_kind_by_name(TV_FOOD, "Slime Mold")) {
                food_amount = 2500;
            } else if (obj->kind == lookup_kind_by_name(TV_FOOD, "Sprig of Athelas")) {
                food_amount = 500;
            } else if (obj->kind == lookup_kind_by_name(TV_FOOD, "Petty-dwarf roots")) {
                food_amount = 1400; // (boss loot)
            }

            if (food_amount > 0) {
                // Limit consumption to avoid overfeeding
                food_amount = MIN(food_amount, max_safe_food);
                
                use_object(p, obj, 1, false);
                // nourish (no heal)
                player_inc_timed(p, TMD_FOOD, food_amount, false, false);
                fuel_found = true;
                break;
            }
        }
        // Special mushrooms
        else if (obj->tval == TV_MUSHROOM)
        {
            int food_amount = 0;
            
            // Shadows mushroom
            if (obj->kind == lookup_kind_by_name(TV_MUSHROOM, "Shadows")) {
                food_amount = 1000;  // Oily ones
            }
            // Stoneskin mushroom
            else if (obj->kind == lookup_kind_by_name(TV_MUSHROOM, "Stoneskin")) {
                food_amount = 300;  // Base food value for mushroom
            }

            if (food_amount > 0) {
                // Limit consumption to avoid overfeeding
                food_amount = MIN(food_amount, max_safe_food);
                
                use_object(p, obj, 1, false);
                // nourish (no heal)
                player_inc_timed(p, TMD_FOOD, food_amount, false, false);
                fuel_found = true;
                break;
            }
        }
        // light with oil (lantern, lamp...)
        else if (obj->tval == TV_LIGHT)
        {
            int fuel_amount;

            // exclude empty lights and NO_FUEL ones
            if (of_has(obj->flags, OF_NO_FUEL) || obj->timeout < 1)
                continue;
                
            fuel_amount = obj->timeout / 10;
            if (obj->timeout < 10) {
                fuel_amount = 1;
            }

            // Check if the light source has ego properties
            if (obj->ego && obj->ego->name) {
                // Ego item handling
                if (streq(obj->ego->name, "of Brightness")) {
                    fuel_amount *= 3;
                    use_object(p, obj, 1, false); // destroy to prevent cheese
                }
                else if (streq(obj->ego->name, "of True Sight")) {
                    fuel_amount *= 2;
                    use_object(p, obj, 1, false); // destroy to prevent cheese
                }
                else if (streq(obj->ego->name, "of Shadows")) {
                    fuel_amount *= 5;
                    use_object(p, obj, 1, false); // destroy to prevent cheese
                }
            }
            // Not an ego item, check if it's an Old Lantern
            else if (obj->kind == lookup_kind_by_name(TV_LIGHT, "Old Lantern")) {
                // Old lanterns which sold in NPC store got bad quality oil
                // (tradesman dilutes it)..
                // While old lanterns found in the dungeon are good.
                if (obj->origin == ORIGIN_STORE)
                    fuel_amount /= 5;
                
                // destroy 'Old lantern' after use
                use_object(p, obj, 1, false);
            }
            else {
                // all other lights
                // reduce oil amount inside of the light source
                obj->timeout = 0;
            }

            // Limit consumption to avoid overfeeding
            fuel_amount = MIN(fuel_amount, max_safe_food);
            
            // nourish and heal
            player_inc_timed(p, TMD_FOOD, fuel_amount, false, false);
            hp_player(p, p->wpos.depth / 2);
            fuel_found = true;
            break;
        }
    }

    // no fuel found -> quit
    if (!fuel_found) return false;

    // generate message
    rng = randint0(29); // Total number of messages in get_oil_message()
    msg(p, get_oil_message(rng));

    // refresh inventory
    p->upkeep->redraw |= (PR_INVEN);

    return true;
}


/*
 * Hook for refilling lamps
 */
static bool item_tester_refill_lamp(struct object *obj)
{
    /* Flasks of oil are okay */
    if (tval_is_fuel(obj)) return true;

    /* Non-empty, non-everburning lamps are okay */
    if (tval_is_light(obj) && of_has(obj->flags, OF_TAKES_FUEL) && (obj->timeout > 0) &&
        !of_has(obj->flags, OF_NO_FUEL))
    {
        return true;
    }

    /* Assume not okay */
    return false;
}


/*
 * Refill the players lamp (from the pack or floor)
 */
static void refill_lamp(struct player *p, struct object *lamp, struct object *obj)
{
    /* Refuel */
    lamp->timeout += (obj->timeout? obj->timeout: obj->pval);

    /* Message */
    msg(p, "You fuel your lamp.");

    /* Comment */
    if (lamp->timeout >= z_info->fuel_lamp)
    {
        lamp->timeout = z_info->fuel_lamp;
        msg(p, "Your lamp is full.");
    }

    /* Refilled from a lamp */
    if (of_has(obj->flags, OF_TAKES_FUEL))
    {
        bool unstack = false;

        /* Unstack if necessary */
        if (obj->number > 1)
        {
            /* Obtain a local object, split */
            struct object *used = object_split(obj, 1);
            struct chunk *c = chunk_get(&p->wpos);

            /* Remove fuel */
            used->timeout = 0;

            /* Carry or drop */
            if (object_is_carried(p, obj) && inven_carry_okay(p, used))
                inven_carry(p, used, true, true);
            else
                drop_near(p, c, &used, 0, &p->grid, false, DROP_FADE, true);

            unstack = true;
        }

        /* Empty a single lamp */
        else
            obj->timeout = 0;

        /* Combine the pack (later) */
        p->upkeep->notice |= (PN_COMBINE);

        /* Redraw */
        set_redraw_inven(p, unstack? NULL: obj);
    }

    /* Refilled from a flask */
    else
    {
        /* Decrease the item */
        use_object(p, obj, 1, true);
    }

    /* Recalculate torch */
    p->upkeep->update |= (PU_BONUS);

    /* Redraw */
    set_redraw_equip(p, lamp);
}


/*
 * Refill the players light source
 */
void do_cmd_refill(struct player *p, int item)
{
    struct object *light = equipped_item_by_slot_name(p, "light");
    struct object *obj = object_from_index(p, item, true, true);

    /* Paranoia: requires an item */
    if (!obj) return;

    /* Restrict ghosts */
    if (p->ghost && !(p->dm_flags & DM_GHOST_BODY))
    {
        msg(p, "You need a tangible body to refill light sources!");
        return;
    }

    /* Check preventive inscription '!F' */
    if (object_prevent_inscription(p, obj, INSCRIPTION_REFILL, false))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Some checks */
    if (!object_is_carried(p, obj))
    {
        /* Restricted by choice */
        if (!is_owner(p, obj))
        {
            msg(p, "This item belongs to someone else!");
            return;
        }

        /* Must meet level requirement */
        if (!has_level_req(p, obj))
        {
            msg(p, "You don't have the required level!");
            return;
        }

        /* Check preventive inscription '!g' */
        if (object_prevent_inscription(p, obj, INSCRIPTION_PICKUP, false))
        {
            msg(p, "The item's inscription prevents it.");
            return;
        }
    }

    /* Paranoia: requires a refill */
    if (!item_tester_refill_lamp(obj)) return;

    /* Check what we're wielding. */
    if (!light || !tval_is_light(light))
    {
        msg(p, "You are not wielding a light.");
        return;
    }
    if (of_has(light->flags, OF_NO_FUEL) || !of_has(light->flags, OF_TAKES_FUEL))
    {
        msg(p, "Your light cannot be refilled.");
        return;
    }

    /* Take half a turn */
    use_energy_aux(p, 50);

    refill_lamp(p, light, obj);
}


/*
 * Use a scroll, check if none left
 */
bool do_cmd_read_scroll_end(struct player *p, struct object *obj, bool ident, bool used)
{
    return do_cmd_use_end(p, obj, ident, used, USE_SINGLE);
}


void do_cmd_use_staff_discharge(struct player *p, struct object *obj, bool ident, bool used)
{
    do_cmd_use_end(p, obj, ident, used, USE_CHARGE);
}


void do_cmd_zap_rod_end(struct player *p, struct object *obj, bool ident, bool used)
{
    do_cmd_use_end(p, obj, ident, used, USE_TIMEOUT);
}


void do_cmd_activate_end(struct player *p, struct object *obj, bool ident, bool used)
{
    do_cmd_use_end(p, obj, ident, used, USE_TIMEOUT);
}


