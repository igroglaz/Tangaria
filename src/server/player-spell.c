/*
 * File: player-spell.c
 * Purpose: Spell and prayer casting/praying
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2024 MAngband and PWMAngband Developers
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
 * Used by get_spell_info() to pass information as it iterates through effects.
 */
struct spell_info_iteration_state
{
    random_value shared_rv;
    uint8_t have_shared;
};


/*
 * Initialize player spells
 */
void player_spells_init(struct player *p)
{
    int i, num_spells = p->clazz->magic.total_spells;

    /* None */
    if (!num_spells) return;

    /* Allocate */
    p->spell_flags = mem_zalloc(num_spells * sizeof(uint8_t));
    p->spell_order = mem_zalloc(num_spells * sizeof(uint8_t));
    p->spell_power = mem_zalloc(num_spells * sizeof(uint8_t));
    p->spell_cooldown = mem_zalloc(num_spells * sizeof(uint8_t));

    /* None of the spells have been learned yet */
    for (i = 0; i < num_spells; i++) p->spell_order[i] = 99;
}


/*
 * Free player spells
 */
void player_spells_free(struct player *p)
{
    mem_free(p->spell_flags);
    p->spell_flags = NULL;
    mem_free(p->spell_order);
    p->spell_order = NULL;
    mem_free(p->spell_power);
    p->spell_power = NULL;
    mem_free(p->spell_cooldown);
    p->spell_cooldown = NULL;
}


/*
 * Get the spellbook structure from any object which is a book
 */
const struct class_book *object_kind_to_book(const struct object_kind *kind)
{
    struct player_class *clazz = classes;

    while (clazz)
    {
        int i;

        for (i = 0; i < clazz->magic.num_books; i++)
        {
            if ((kind->tval == clazz->magic.books[i].tval) &&
                (kind->sval == clazz->magic.books[i].sval))
            {
                return &clazz->magic.books[i];
            }
        }

        clazz = clazz->next;
    }

    return NULL;
}


/*
 * Get the spellbook structure from an object which is a book the player can
 * cast from
 */
const struct class_book *player_object_to_book(struct player *p, const struct object *obj)
{
    int i;

    for (i = 0; i < p->clazz->magic.num_books; i++)
    {
        if ((obj->tval == p->clazz->magic.books[i].tval) &&
            (obj->sval == p->clazz->magic.books[i].sval))
        {
            return &p->clazz->magic.books[i];
        }
    }

    return NULL;
}


/*
 * Get the spellbook structure index from an object which is a book the player can
 * cast from
 */
int object_to_book_index(struct player *p, const struct object *obj)
{
    int i;

    for (i = 0; i < p->clazz->magic.num_books; i++)
    {
        if ((obj->tval == p->clazz->magic.books[i].tval) &&
            (obj->sval == p->clazz->magic.books[i].sval))
        {
            return i;
        }
    }

    return -1;
}


const struct class_spell *spell_by_index(const struct class_magic *magic, int index)
{
    int book = 0, count = 0;

    /* Check index validity */
    if ((index < 0) || (index >= magic->total_spells)) return NULL;

    /* Find the book, count the spells in previous books */
    while (count + magic->books[book].num_spells - 1 < index)
        count += magic->books[book++].num_spells;

    /* Find the spell */
    return &magic->books[book].spells[index - count];
}


/*
 * Spell failure adjustment by casting stat level
 */
static int fail_adjust(struct player *p, const struct class_spell *spell)
{
    int stat = spell->realm->stat;

    return adj_mag_stat[p->state.stat_ind[stat]];
}


/*
 * Spell minimum failure by casting stat level
 */
static int min_fail(struct player *p, const struct class_spell *spell)
{
    int stat = spell->realm->stat;

    return adj_mag_fail[p->state.stat_ind[stat]];
}


/*
 * Returns chance of failure for a spell
 */
int16_t spell_chance(struct player *p, int spell_index)
{
    int chance = 100, minfail;
    const struct class_spell *spell;

    /* Paranoia -- must be literate */
    if (!p->clazz->magic.total_spells) return chance;

    /* Get the spell */
    spell = spell_by_index(&p->clazz->magic, spell_index);
    if (!spell) return chance;

    /* Extract the base spell failure rate */
    chance = spell->sfail;

    /* Reduce failure rate by "effective" level adjustment */
    chance -= 3 * (p->lev - spell->slevel);

    /* Reduce failure rate by casting stat level adjustment */
    chance -= fail_adjust(p, spell);

    /* Not enough mana to cast */
    if (spell->smana > p->csp)
        chance += 5 * (spell->smana - p->csp);

    /* Extract the minimum failure rate */
    minfail = min_fail(p, spell);

    /* Non zero-fail characters never get better than 5 percent */
    if (!player_has(p, PF_ZERO_FAIL) && (minfail < 5)) minfail = 5;

    /* Fear makes spells harder (before minfail) */
    if (player_of_has(p, OF_AFRAID)) chance += 20;

    /* Minimal and maximal failure rate */
    if (chance < minfail) chance = minfail;
    if (chance > 50) chance = 50;

    /* Stunning makes spells harder */
    if (p->timed[TMD_STUN] > 50) chance += 25;
    else if (p->timed[TMD_STUN]) chance += 15;

    /* Amnesia makes spells very difficult */
    if (p->timed[TMD_AMNESIA]) chance = 50 + chance / 2;

    /* Always a 5 percent chance of working */
    if (chance > 95) chance = 95;
    
    if (((streq(p->clazz->name, "Priest"  ))  ||
         (streq(p->clazz->name, "Paladin" ))  ||
         (streq(p->clazz->name, "Rogue"   ))  ||
         (streq(p->clazz->name, "Telepath"))  ||
         (streq(p->clazz->name, "Summoner"))) && (chance > 94))
        chance -= (p->state.stat_ind[STAT_CHR] / 8);
        

    /* Return the chance */
    return (chance);
}


bool spell_is_identify(struct player *p, int spell_index)
{
    const struct class_spell *spell = spell_by_index(&p->clazz->magic, spell_index);

    return (spell->effect->index == EF_IDENTIFY);
}


static size_t append_random_value_string(char *buffer, size_t size, random_value *rv)
{
    size_t offset = 0;

    if (rv->base > 0)
    {
        offset += strnfmt(buffer + offset, size - offset, "%d", rv->base);

        if (rv->dice > 0 && rv->sides > 0)
            offset += strnfmt(buffer + offset, size - offset, "+");
    }

    if (rv->dice == 1 && rv->sides > 0)
        offset += strnfmt(buffer + offset, size - offset, "d%d", rv->sides);
    else if (rv->dice > 1 && rv->sides > 0)
        offset += strnfmt(buffer + offset, size - offset, "%dd%d", rv->dice, rv->sides);

    return offset;
}


static void spell_effect_append_value_info(struct player *p, const struct effect *effect, char *buf,
    size_t len, const struct class_spell *spell, size_t *offset,
    struct spell_info_iteration_state *ist)
{
    random_value rv;
    const char *type;
    char special[40];
    struct source actor_body;
    struct source *data = &actor_body;

    source_player(data, 0, p);

    if (effect->index == EF_CLEAR_VALUE)
        ist->have_shared = 0;
    else if (effect->index == EF_SET_VALUE && effect->dice)
    {
        int16_t current_spell;

        ist->have_shared = 1;

        /* Hack -- set current spell (for effect_value_base_by_name) */
        current_spell = p->current_spell;
        p->current_spell = spell->sidx;

        dice_roll(effect->dice, (void *)data, &ist->shared_rv);

        /* Hack -- reset current spell */
        p->current_spell = current_spell;
    }

    type = effect_info(effect, spell->realm->name);
    if (type == NULL) return;

    memset(&rv, 0, sizeof(rv));
    special[0] = '\0';

    /* Hack -- mana drain (show real value) */
    if ((effect->index == EF_BOLT_AWARE) && (effect->subtype == PROJ_DRAIN_MANA))
    {
        rv.base = 6;
        rv.dice = 3;
        rv.sides = p->lev;
    }

    /* Normal case -- use dice */
    else if (effect->dice != NULL)
    {
        int16_t current_spell;

        /* Hack -- set current spell (for effect_value_base_by_name) */
        current_spell = p->current_spell;
        p->current_spell = spell->sidx;

        dice_roll(effect->dice, (void *)data, &rv);

        /* Hack -- reset current spell */
        p->current_spell = current_spell;
    }
    else if (ist->have_shared)
    {
        /* PWMAngband: don't repeat shared info */
        if (ist->have_shared == 1)
        {
            memcpy(&rv, &ist->shared_rv, sizeof(random_value));
            ist->have_shared = 2;
        }
        else return;
    }

    /* Handle some special cases where we want to append some additional info. */
    switch (effect->index)
    {
        case EF_HEAL_HP:
        {
            /* Append percentage only, as the fixed value is always displayed */
            if (rv.m_bonus) strnfmt(special, sizeof(special), "/%d%%", rv.m_bonus);
            break;
        }
        case EF_BALL:
        {
            /* Append number of projectiles. */
            if (rv.m_bonus) strnfmt(special, sizeof(special), "x%d", rv.m_bonus);

            /* Append radius */
            else
            {
                int rad = (effect->radius? effect->radius: 2);
                struct beam_info beam;

                if (effect->other) rad += p->lev / effect->other;
                if (p->poly_race && monster_is_powerful(p->poly_race)) rad++;

                fill_beam_info(p, spell->sidx, &beam);

                rad = rad + beam.spell_power / 2;
                rad = rad * (20 + beam.elem_power) / 20;

                strnfmt(special, sizeof(special), ", rad %d", rad);
            }

            break;
        }
        case EF_BLAST:
        {
            /* Append radius */
            int rad = (effect->radius? effect->radius: 2);
            struct beam_info beam;

            if (effect->other) rad += p->lev / effect->other;
            if (p->poly_race && monster_is_powerful(p->poly_race)) rad++;

            fill_beam_info(p, spell->sidx, &beam);

            rad = rad + beam.spell_power / 2;
            rad = rad * (20 + beam.elem_power) / 20;

            strnfmt(special, sizeof(special), ", rad %d", rad);
            break;
        }
        case EF_STRIKE:
        {
            /* Append radius */
            if (effect->radius) strnfmt(special, sizeof(special), ", rad %d", effect->radius);
            break;
        }
        case EF_SHORT_BEAM:
        {
            /* Append length of beam */
            strnfmt(special, sizeof(special), ", len %d", effect->radius);
            break;
        }
        case EF_BOLT_OR_BEAM:
        case EF_STAR:
        case EF_STAR_BALL:
        case EF_SWARM:
        {
            /* Append number of projectiles. */
            if (rv.m_bonus) strnfmt(special, sizeof(special), "x%d", rv.m_bonus);
            break;
        }
        case EF_BOW_BRAND_SHOT:
        {
            /* Append "per shot" */
            my_strcpy(special, "/shot", sizeof(special));
            break;
        }
        case EF_TIMED_INC:
        {
            if (rv.m_bonus)
            {
                /* Append percentage only, as the fixed value is always displayed */
                if (effect->subtype == TMD_EPOWER)
                    strnfmt(special, sizeof(special), "/+%d%%", rv.m_bonus);

                /* Append the bonus only, since the duration is always displayed. */
                else
                    strnfmt(special, sizeof(special), "/+%d", rv.m_bonus);
            }
            break;
        }
        default:
            break;
    }

    if ((rv.base > 0) || (rv.dice > 0 && rv.sides > 0))
    {
        if (!*offset) *offset = strnfmt(buf, len, " %s ", type);
        else *offset += strnfmt(buf + *offset, len - *offset, "+");
        *offset += append_random_value_string(buf + *offset, len - *offset, &rv);

        if (special[0])
            strnfmt(buf + *offset, len - *offset, "%s", special);
    }
}


void get_spell_info(struct player *p, int spell_index, char *buf, size_t len)
{
    struct effect *effect;
    struct spell_info_iteration_state ist;
    const struct player_class *c = p->clazz;
    const struct class_spell *spell;
    size_t offset = 0;

    ist.have_shared = 0;

    if (p->ghost && !player_can_undead(p)) c = lookup_player_class("Ghost");
    spell = spell_by_index(&c->magic, spell_index);
    effect = spell->effect;

    /* Blank 'buf' first */
    buf[0] = '\0';

    while (effect)
    {
        spell_effect_append_value_info(p, effect, buf, len, spell, &offset, &ist);

        /* Hack -- if next effect has the same tip, also append that info */
        if (effect->next)
        {
            const char *type = effect_info(effect, spell->realm->name);
            const char *nexttype = effect_info(effect->next, spell->realm->name);

            if (type && nexttype && strcmp(nexttype, type)) return;
        }

        effect = effect->next;
    }
}


void cast_spell_end(struct player *p)
{
    int spell_index = p->current_spell;
    const struct class_spell *spell;
    const struct player_class *c = p->clazz;

    if (p->ghost && !player_can_undead(p)) c = lookup_player_class("Ghost");

    /* Access the spell */
    spell = spell_by_index(&c->magic, spell_index);

    /* A spell was cast */
    if (!(p->spell_flags[spell_index] & PY_SPELL_WORKED))
    {
        int e = spell->sexp;

        /* The spell worked */
        p->spell_flags[spell_index] |= PY_SPELL_WORKED;

        /* Gain experience */
        player_exp_gain(p, e * spell->slevel);

        /* Redraw */
        p->upkeep->redraw |= (PR_SPELL);
    }
}


/*
 * Send the ghost spell info to the client.
 */
void show_ghost_spells(struct player *p)
{
    struct player_class *c = lookup_player_class("Ghost");
    const struct class_book *book = &c->magic.books[0];
    struct class_spell *spell;
    int i;
    char out_val[NORMAL_WID];
    char out_desc[MSG_LEN], out_name[NORMAL_WID];
    uint8_t line_attr;
    char help[20];
    const char *comment = help;
    spell_flags flags;

    flags.line_attr = COLOUR_WHITE;
    flags.flag = RSF_NONE;
    flags.dir_attr = 0;
    flags.proj_attr = 0;

    /* Wipe the spell array */
    Send_spell_info(p, 0, 0, "", &flags, 0);

    Send_book_info(p, 0, book->realm->name);

    /* Check each spell */
    for (i = 0; i < book->num_spells; i++)
    {
        /* Access the spell */
        spell = &book->spells[i];

        /* Get extra info */
        get_spell_info(p, spell->sidx, help, sizeof(help));

        /* Assume spell is known and tried */
        comment = help;
        line_attr = COLOUR_WHITE;

        /* Format information */
        strnfmt(out_val, sizeof(out_val), "%-30s%2d %4d %3d%%%s", spell->name, spell->slevel,
            spell->smana, 0, comment);
        spell_description(p, spell->sidx, -1, false, out_desc, sizeof(out_desc));
        my_strcpy(out_name, spell->name, sizeof(out_name));

        flags.line_attr = line_attr;
        flags.flag = RSF_NONE;
        flags.dir_attr = effect_aim(spell->effect);
        flags.proj_attr = spell->sproj;

        /* Send it */
        Send_spell_info(p, 0, i, out_val, &flags, spell->smana);
        Send_spell_desc(p, 0, i, out_desc, out_name);
    }
}


/*
 * Get antimagic field from an object
 */
int antimagic_field(const struct object *obj, bitflag flags[OF_SIZE])
{
    /* Base antimagic field */
    return 10 * obj->modifiers[OBJ_MOD_ANTI_MAGIC];
}


/*
 * Check if the antimagic field around a player will disrupt the caster's spells.
 */
bool check_antimagic(struct player *p, struct chunk *c, struct monster *who)
{
    int16_t id;
    int i, amchance, amrad, dist;
    struct loc grid;

    /* The caster is a monster */
    if (who)
    {
        id = who->master;
        loc_copy(&grid, &who->grid);
    }

    /* The caster is the player */
    else
    {
        id = p->id;
        loc_copy(&grid, &p->grid);
    }

    /* Check each player */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *q = player_get(i);
        struct object *obj;
        int r_adj = 0, c_adj, adj;

        /* Skip players not on this level */
        if (!wpos_eq(&q->wpos, &p->wpos)) continue;

        /* Handle class modifier and polymorphed players */
        c_adj = class_modifier(q->clazz, OBJ_MOD_ANTI_MAGIC, q->lev);
        if (c_adj < 0) c_adj = 0;
        if (q->poly_race && rf_has(q->poly_race->flags, RF_ANTI_MAGIC))
            r_adj = q->poly_race->level / 2;
        adj = max(r_adj, c_adj);

        /* Antimagic class modifier is capped at 50% */
        if (adj > 50) adj = 50;

        /* Apply field */
        amchance = adj;
        amrad = 1 + adj / 10;

        /* Add racial modifier */
        adj = race_modifier(q->race, OBJ_MOD_ANTI_MAGIC, q->lev, q->poly_race? true: false);
        if (adj > 0)
        {
            /* Antimagic racial modifier is capped at 10% */
            if (adj > 10) adj = 10;

            /* Apply field */
            amchance = amchance + adj;
            amrad++;
        }

        /* Dark swords can disrupt magic attempts too */
        obj = equipped_item_by_slot_name(q, "weapon");
        if (obj)
        {
            int field = antimagic_field(obj, obj->flags);

            /* Apply field */
            amchance = amchance + field;
            amrad = amrad + field / 10;
        }

        /* Paranoia */
        if (amchance < 0) amchance = 0;
        if (amrad < 0) amrad = 0;

        /* Own antimagic field */
        if (p == q)
        {
            /* Antimagic field is capped at 90% */
            if (amchance > 90) amchance = 90;

            /* Check antimagic */
            if (magik(amchance))
            {
                if (who)
                {
                    char m_name[NORMAL_WID];

                    monster_desc(p, m_name, sizeof(m_name), who, MDESC_CAPITAL);
                    msgt(p, MSG_DISRUPT, "%s fails to cast a spell.", m_name);
                }
                else
                    // Inquisitor can cast his weird spells
                    if (streq(p->clazz->name, "Inquisitor"))
                        return false;
                    // others classes will fail
                    msgt(p, MSG_DISRUPT, "Your anti-magic field disrupts your attempt.");
                return true;
            }
        }

        /* Antimagic field from other players */
        else
        {
            /* Lower effect if not hostile (greatly) */
            if (!master_is_hostile(id, q->id)) amchance >>= 2;

            /* Antimagic field is capped at 90% */
            if (amchance > 90) amchance = 90;

            /* Compute distance */
            dist = distance(&grid, &q->grid);
            if (dist > amrad) amchance = 0;

            /* Check antimagic */
            if (magik(amchance))
            {
                if (who)
                {
                    char m_name[NORMAL_WID];

                    monster_desc(p, m_name, sizeof(m_name), who, MDESC_CAPITAL);
                    msgt(p, MSG_DISRUPT, "%s fails to cast a spell.", m_name);
                }
                else
                {
                    if (player_is_visible(p, i))
                        msgt(p, MSG_DISRUPT, "%s's anti-magic field disrupts your attempt.", q->name);
                    else
                        msgt(p, MSG_DISRUPT, "An anti-magic field disrupts your attempt.");
                }
                return true;
            }
        }
    }

    /* Monsters don't disrupt other monsters' spells, that would be cheezy */
    if (who) return false;

    /* Check each monster */
    for (i = 1; i < cave_monster_max(c); i++)
    {
        struct monster *mon = cave_monster(c, i);
        struct monster_lore *lore;

        /* Paranoia -- skip dead monsters */
        if (!mon->race) continue;

        /* Learn about antimagic field */
        lore = get_lore(p, mon->race);
        if (monster_is_visible(p, i)) rf_on(lore->flags, RF_ANTI_MAGIC);

        /* Skip monsters without antimagic field */
        if (!rf_has(mon->race->flags, RF_ANTI_MAGIC)) continue;

        /* Compute the probability of a monster to disrupt any magic attempts */
        amchance = 25 + mon->level;
        amrad = 3 + (mon->level / 10);

        /* Lower effect if not hostile (greatly) */
        if (!master_is_hostile(id, mon->master)) amchance >>= 2;

        /* Antimagic field is capped at 90% */
        if (amchance > 90) amchance = 90;

        /* Compute distance */
        dist = distance(&grid, &mon->grid);
        if (dist > amrad) amchance = 0;

        /* Check antimagic */
        if (magik(amchance))
        {
            if (monster_is_visible(p, i))
            {
                char m_name[NORMAL_WID];

                monster_desc(p, m_name, sizeof(m_name), mon, MDESC_CAPITAL);
                msgt(p, MSG_DISRUPT, "%s's anti-magic field disrupts your attempt.", m_name);
            }
            else
                msgt(p, MSG_DISRUPT, "An anti-magic field disrupts your attempt.");

            return true;
        }
    }

    /* Assume no antimagic */
    return false;
}


/*
 * Check if the antisummon field around a player will disrupt the caster's summoning spells.
 */
bool check_antisummon(struct player *p, struct monster *mon)
{
    int16_t id;
    int i, amchance, amrad, dist;
    struct loc grid;

    /* The caster is a monster */
    if (mon)
    {
        id = mon->master;
        loc_copy(&grid, &mon->grid);
    }

    /* The caster is the player */
    else
    {
        id = p->id;
        loc_copy(&grid, &p->grid);
    }

    /* Check each player */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *q = player_get(i);

        /* Skip players not on this level */
        if (!wpos_eq(&q->wpos, &p->wpos)) continue;

        /* No antisummon */
        if (!q->timed[TMD_ANTISUMMON]) continue;

        /* Compute the probability of a summoner to disrupt any summon attempts */
        /* This value ranges from 60% (clvl 35) to 90% (clvl 50) */
        amchance = q->lev * 2 - 10;

        /* Range of the antisummon field (8-11 squares for a max sight of 20 squares) */
        amrad = 1 + z_info->max_sight * q->lev / 100;

        /* Own antisummon field */
        if (p == q)
        {
            /* Check antisummon */
            if (magik(amchance))
            {
                if (mon)
                {
                    char m_name[NORMAL_WID];

                    monster_desc(p, m_name, sizeof(m_name), mon, MDESC_CAPITAL);
                    msgt(p, MSG_DISRUPT, "%s fails to cast a spell.", m_name);
                }
                else
                    msgt(p, MSG_DISRUPT, "Your anti-summon field disrupts your attempt.");
                return true;
            }
        }

        /* Antisummon field from other players */
        else
        {
            /* Lower effect if not hostile (greatly) */
            if (!master_is_hostile(id, q->id)) amchance >>= 2;

            /* Compute distance */
            dist = distance(&grid, &q->grid);
            if (dist > amrad) amchance = 0;

            /* Check antisummon */
            if (magik(amchance))
            {
                if (mon)
                {
                    char m_name[NORMAL_WID];

                    monster_desc(p, m_name, sizeof(m_name), mon, MDESC_CAPITAL);
                    msgt(p, MSG_DISRUPT, "%s fails to cast a spell.", m_name);
                }
                else
                {
                    if (player_is_visible(p, i))
                        msgt(p, MSG_DISRUPT, "%s's anti-summon field disrupts your attempt.", q->name);
                    else
                        msgt(p, MSG_DISRUPT, "An anti-summon field disrupts your attempt.");
                }
                return true;
            }
        }
    }

    /* Assume no antisummon */
    return false;
}


/*
 * Send the mimic spell info to the client.
 */
void show_mimic_spells(struct player *p)
{
    const struct class_book *book = &p->clazz->magic.books[0];
    struct class_spell *spell;
    int i, j = 0, k = 0;
    char out_val[NORMAL_WID];
    char out_desc[MSG_LEN], out_name[NORMAL_WID];
    uint8_t line_attr;
    char help[20];
    const char *comment = help;
    int flag;
    spell_flags flags;

    flags.line_attr = COLOUR_WHITE;
    flags.flag = RSF_NONE;
    flags.dir_attr = 0;
    flags.proj_attr = 0;

    /* Wipe the spell array */
    Send_spell_info(p, 0, 0, "", &flags, 0);

    Send_book_info(p, 0, book->realm->name);

    /* Check each spell */
    for (i = 0; i < book->num_spells; i++)
    {
        /* Access the spell */
        spell = &book->spells[i];

        /* Access the spell flag */
        flag = spell->effect->flag;

        /* Check spell availability */
        if (!(p->poly_race && rsf_has(p->poly_race->spell_flags, flag))) continue;

        /* Get extra info */
        get_spell_info(p, spell->sidx, help, sizeof(help));

        /* Assume spell is known and tried */
        comment = help;
        line_attr = COLOUR_WHITE;

        /* Format information */
        strnfmt(out_val, sizeof(out_val), "%-30s%2d %4d %3d%%%s", spell->name, 0, spell->smana,
            spell->sfail, comment);
        spell_description(p, spell->sidx, flag, false, out_desc, sizeof(out_desc));
        my_strcpy(out_name, spell->name, sizeof(out_name));

        flags.line_attr = line_attr;
        flags.flag = flag;
        flags.dir_attr = effect_aim(spell->effect);
        flags.proj_attr = spell->sproj;

        /* Send it */
        Send_spell_info(p, k, j, out_val, &flags, spell->smana);
        Send_spell_desc(p, k, j, out_desc, out_name);

        /* Next spell */
        j++;
        if (j == MAX_SPELLS_PER_PAGE)
        {
            j = 0;
            k++;

            Send_book_info(p, k, book->realm->name);
        }
    }
}


/*
 * Project a spell on someone.
 *
 * p is the target of the spell
 * cidx is the class index of the caster of the spell
 * spell is the spell index
 * silent is true when no message is displayed
 */
bool cast_spell_proj(struct player *p, int cidx, int spell_index, bool silent)
{
    const struct player_class *c = player_id2class(cidx);
    const struct class_spell *spell = spell_by_index(&c->magic, spell_index);
    bool pious = streq(spell->realm->name, "divine");
    bool ident = false, used;
    struct source who_body;
    struct source *who = &who_body;

    /* Clear current */
    current_clear(p);

    /* Set current spell */
    p->current_spell = spell_index;

    /* Hack -- save the class of the caster */
    p->current_item = 0 - cidx;

    /* Message */
    if (spell->effect && spell->effect->other_msg && !silent)
    {
        /* Hack -- formatted message */
        switch (spell->effect->flag)
        {
            case RSF_HEAL:
            case RSF_TELE_TO:
            case RSF_TELE_LEVEL:
            case RSF_FORGET:
            case RSF_S_KIN:
            {
                msg_format_near(p, MSG_PY_SPELL, spell->effect->other_msg, player_poss(p));
                break;
            }
            default:
            {
                msg_print_near(p, (pious? MSG_PY_PRAYER: MSG_PY_SPELL), spell->effect->other_msg);
                break;
            }
        }
    }

    source_player(who, get_player_index(get_connection(p->conn)), p);
    target_fix(p);
    used = effect_do(spell->effect, who, &ident, true, 0, NULL, 0, 0, NULL);
    target_release(p);
    return used;
}


/*
 * Return the chance of an effect beaming, given a tval.
 */
static int beam_chance_tval(int tval)
{
    switch (tval)
    {
        case TV_WAND: return 20;
        case TV_ROD:  return 10;
    }

    return 0;
}


static int beam_chance(struct player* p)
{
    int plev = p->lev;

    return (player_has(p, PF_BEAM)? plev: plev / 2);
}


void fill_beam_info(struct player *p, int spell_index, struct beam_info *beam)
{
    const struct player_class *c;
    const struct class_spell *spell;

    /* Initialize */
    memset(beam, 0, sizeof(struct beam_info));

    /* Use the spell parameter as a tval */
    if (!p)
    {
        beam->beam = beam_chance_tval(spell_index);
        return;
    }

    /* Use the spell parameter as a spell */
    beam->beam = beam_chance(p);

    c = p->clazz;
    if (p->ghost && !player_can_undead(p)) c = lookup_player_class("Ghost");
    spell = spell_by_index(&c->magic, spell_index);

    /* Hack -- elemental spells */
    if (streq(spell->realm->name, "elemental"))
    {
        int i, j;

        /* Spell power */
        beam->spell_power = p->spell_power[spell_index];

        /* Beam chance */
        if (spell->bidx < c->magic.num_books - 1)
            beam->beam += beam->spell_power * 10;
        else
            beam->beam += beam->spell_power * 5;

        /* Elemental power */
        if (p->timed[TMD_EPOWER])
        {
            for (i = 0; i < c->magic.num_books; i++)
            {
                for (j = 0; j < c->magic.books[i].num_spells; j++)
                {
                    spell = &c->magic.books[i].spells[j];

                    if ((spell->effect->index == EF_TIMED_INC) &&
                        (spell->effect->subtype == TMD_EPOWER))
                    {
                        beam->elem_power = p->spell_power[spell->sidx];
                        break;
                    }
                }
            }
        }
    }
}


/*
 * Get spell description
 */
void spell_description(struct player *p, int spell_index, int flag, bool need_know, char *out_desc,
    int size)
{
    int num_damaging = 0;
    struct effect *e;
    const struct player_class *c;
    const struct class_spell *spell;
    struct source actor_body;
    struct source *data = &actor_body;
    char buf[NORMAL_WID];
    bool valid;
    int16_t current_spell;

    source_player(data, 0, p);

    c = p->clazz;
    if (p->ghost && !player_can_undead(p)) c = lookup_player_class("Ghost");
    spell = spell_by_index(&c->magic, spell_index);

    /* Hack -- set current spell (for effect_value_base_by_name) */
    current_spell = p->current_spell;
    p->current_spell = spell->sidx;

    /* Spell description */
    if (flag == -1) my_strcpy(out_desc, spell->text, size);
    else strnfmt(out_desc, size, spell->text, flag);

    /* To summarize average damage, count the damaging effects */
    for (e = spell->effect; e != NULL; e = effect_next(e, data))
    {
        if (effect_damages(e, data, spell->realm->name)) num_damaging++;
    }

    /* Now enumerate the effects' damage and type if not forgotten */
    valid = (need_know? (p->spell_flags && (p->spell_flags[spell_index] & PY_SPELL_WORKED) &&
        !(p->spell_flags[spell_index] & PY_SPELL_FORGOTTEN)): true);
    if ((num_damaging > 0) && valid)
    {
        int i = 0;
        bool have_shared = false;
        random_value shared_rv;

        my_strcat(out_desc, " Inflicts an average of", size);
        for (e = spell->effect; e != NULL; e = effect_next(e, data))
        {
            if (e->index == EF_CLEAR_VALUE)
                have_shared = false;
            else if (e->index == EF_SET_VALUE && e->dice)
            {
                have_shared = true;
                dice_random_value(e->dice, data, &shared_rv);
            }

            if (effect_damages(e, data, spell->realm->name))
            {
                const char *projection = effect_projection(e, data);

                if ((num_damaging > 2) && (i > 0))
                    my_strcat(out_desc, ",", size);
                if ((num_damaging > 1) && (i == num_damaging - 1))
                    my_strcat(out_desc, " and", size);
                strnfmt(buf, sizeof(buf), " {%d}",
                    effect_avg_damage(e, data, spell->realm->name, have_shared? &shared_rv: NULL));
                my_strcat(out_desc, buf, size);
                if (strlen(projection) > 0)
                {
                    strnfmt(buf, sizeof(buf), " %s", projection);
                    my_strcat(out_desc, buf, size);
                }
                i++;
            }
        }
        my_strcat(out_desc, " damage.", size);
    }

    /* Hack -- reset current spell */
    p->current_spell = current_spell;
}
