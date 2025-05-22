/*
 * File: player-birth.c
 * Purpose: Character creation
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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


/* Current player ID */
int32_t player_id;


/* Basic sort algorithm */
static void sort_stats(int16_t* stats, int16_t* stat_order)
{
    int i, j;

    /* I use a bubble sort because I'm lazy at the moment */
    for (i = 0; i < STAT_MAX; i++)
    {
        for (j = 0; j < STAT_MAX - 1; j++)
        {
            if (stats[j] < stats[j + 1])
            {
                int16_t stat, order;

                /* Swap stats */
                stat = stats[j];
                stats[j] = stats[j + 1];
                stats[j + 1] = stat;

                /* Swap stat order */
                if (stat_order)
                {
                    order = stat_order[j];
                    stat_order[j] = stat_order[j + 1];
                    stat_order[j + 1] = order;
                }
            }
        }
    }
}


/* Roll some stats */
static void roll_stats(int16_t* stats)
{
    int i, j;
    int dice[3 * STAT_MAX];

    /* Roll and verify some stats */
    while (true)
    {
        /* Roll some dice */
        for (j = i = 0; i < 3 * STAT_MAX; i++)
        {
            /* Roll the dice */
            dice[i] = randint1(3 + i % 3);

            /* Collect the maximum */
            j += dice[i];
        }

        /* Verify totals */
        if ((j > 7 * STAT_MAX) && (j < 9 * STAT_MAX)) break;
    }

    /* Roll the stats */
    for (i = 0; i < STAT_MAX; i++)
    {
        /* Extract 5 + 1d3 + 1d4 + 1d5 */
        j = 5 + dice[3 * i] + dice[3 * i + 1] + dice[3 * i + 2];

        /* Save that value */
        stats[i] = j;
    }
}


/*
 * Initial stat costs (initial stats always range from 10 to 18 inclusive).
 */
static const int birth_stat_costs[9] = {0, 1, 2, 3, 4, 5, 6, 8, 12};


/* Pool of available points */
#define MAX_BIRTH_POINTS 20


static int get_birth_stat_cost(int16_t stat)
{
    return birth_stat_costs[stat - 10] - birth_stat_costs[stat - 11];
}


static void reset_stats(int16_t stats_local[STAT_MAX], int points_spent_local[STAT_MAX],
    int points_inc_local[STAT_MAX], int *points_left_local)
{
    int i;

    /* Calculate and signal initial stats and points totals. */
    *points_left_local = MAX_BIRTH_POINTS;

    /* Initial stats are all 10 and costs are zero */
    for (i = 0; i < STAT_MAX; i++)
    {
        stats_local[i] = 10;
        points_spent_local[i] = 0;
        points_inc_local[i] = get_birth_stat_cost(stats_local[i] + 1);
    }
}


static bool buy_stat(int choice, int16_t stats_local[STAT_MAX], int points_spent_local[STAT_MAX],
    int points_inc_local[STAT_MAX], int *points_left_local)
{
    /* Must be a valid stat, and have a "base" of below 18 to be adjusted */
    if (!(choice >= STAT_MAX || choice < 0) && (stats_local[choice] < 18))
    {
        /* Get the cost of buying the extra point (beyond what it has already cost to get this far). */
        int stat_cost = get_birth_stat_cost(stats_local[choice] + 1);

        my_assert(stat_cost == points_inc_local[choice]);
        if (stat_cost <= *points_left_local)
        {
            stats_local[choice]++;
            points_spent_local[choice] += stat_cost;
            points_inc_local[choice] = get_birth_stat_cost(stats_local[choice] + 1);
            *points_left_local -= stat_cost;

            return true;
        }
    }

    /* Didn't adjust stat. */
    return false;
}


static bool sell_stat(int choice, int16_t stats_local[STAT_MAX], int points_spent_local[STAT_MAX],
    int points_inc_local[STAT_MAX], int *points_left_local)
{
    /* Must be a valid stat, and we can't "sell" stats below the base of 10. */
    if (!(choice >= STAT_MAX || choice < 0) && (stats_local[choice] > 10))
    {
        int stat_cost = get_birth_stat_cost(stats_local[choice]);

        stats_local[choice]--;
        points_spent_local[choice] -= stat_cost;
        points_inc_local[choice] = get_birth_stat_cost(stats_local[choice] + 1);
        *points_left_local += stat_cost;

        return true;
    }

    /* Didn't adjust stat. */
    return false;
}


/*
 * This picks some reasonable starting values for stats based on the
 * current race/class combo, etc...
 *
 * 0. buy base STR 17
 * 1. buy base DEX of up to 17, stopping at the last breakpoint for blows
 * 2. spend up to half remaining points on each of spell-stat and con,
 *    but only up to max base of 16 unless a pure class
 *    [mage or priest or warrior]
 * 3. If there are any points left, spend as much as possible in order
 *    on DEX and then the non-spell-stat.
 */
static void generate_stats(struct player *p, int16_t st[STAT_MAX], int spent[STAT_MAX],
    int inc[STAT_MAX], int *left)
{
    int step = 0;
    bool maxed[STAT_MAX];

    /* Hack - for now, just use stat of first book */
    int spell_stat = (p->clazz->magic.total_spells? p->clazz->magic.books[0].realm->stat: 0);

    bool caster = (p->clazz->max_attacks < 5? true: false);
    bool warrior = (p->clazz->max_attacks > 5? true: false);
    int blows = 10;
    int dex_break = 10;
    int i, weight = 0;
    const struct start_item *si;

    for (i = 0; i < STAT_MAX; i++) maxed[i] = false;

    /* PWMAngband: compute weight of starting weapon */
    for (si = p->clazz->start_items; si; si = si->next)
    {
        struct object_kind *kind = lookup_kind(si->tval, si->sval);

        if ((si->tval == TV_SWORD) || (si->tval == TV_HAFTED) || (si->tval == TV_POLEARM))
        {
            weight = kind->weight;
            break;
        }
    }

    while (*left && step >= 0)
    {
        switch (step)
        {
            /* Buy base STR 17 */
            case 0:
            {
                if (!maxed[STAT_STR] && st[STAT_STR] < 17)
                {
                    if (!buy_stat(STAT_STR, st, spent, inc, left))
                        maxed[STAT_STR] = true;
                }
                else
                {
                    step++;

                    /* If pure caster skip to step 3 */
                    if (caster) step = 3;
                }

                break;
            }

            /* Buy base DEX of 17, record best breakpoint */
            case 1:
            {
                if (!maxed[STAT_DEX] && st[STAT_DEX] < 17)
                {
                    int num_blows;

                    if (!buy_stat(STAT_DEX, st, spent, inc, left))
                        maxed[STAT_DEX] = true;

                    /* PWMAngband: calculate the expected number of blows per round */
                    num_blows = calc_blows_expected(p, weight, st[STAT_STR], st[STAT_DEX]);

                    if (num_blows / 10 > blows)
                    {
                        blows = num_blows / 10;
                        dex_break = st[STAT_DEX];
                    }
                }
                else
                    step++;

                break;
            }

            /* Sell back DEX that isn't getting us an extra blow. */
            case 2:
            {
                while (st[STAT_DEX] > dex_break)
                {
                    sell_stat(STAT_DEX, st, spent, inc, left);
                    maxed[STAT_DEX] = false;
                }
                step++;
                break;
            }

            /*
             * Spend up to half remaining points on each of spell-stat and
             * con, but only up to max base of 16 unless a pure class
             * [caster or warrior]
             */
            case 3:
            {
                int points_trigger = *left / 2;

                if (warrior)
                    points_trigger = *left;
                else
                {
                    while (!maxed[spell_stat] && (caster || st[spell_stat] < 18) &&
                        spent[spell_stat] < points_trigger)
                    {
                        if (!buy_stat(spell_stat, st, spent, inc, left))
                            maxed[spell_stat] = true;

                        if (spent[spell_stat] > points_trigger)
                        {
                            sell_stat(spell_stat, st, spent, inc, left);
                            maxed[spell_stat] = true;
                        }
                    }
                }

                while (!maxed[STAT_CON] && st[STAT_CON] < 16 && spent[STAT_CON] < points_trigger)
                {
                    if (!buy_stat(STAT_CON, st, spent, inc, left))
                        maxed[STAT_CON] = true;

                    if (spent[STAT_CON] > points_trigger)
                    {
                        sell_stat(STAT_CON, st, spent, inc, left);
                        maxed[STAT_CON] = true;
                    }
                }

                step++;
                break;
            }

            /*
             * If there are any points left, spend as much as possible in
             * order on DEX, and the non-spell-stat.
             */
            case 4:
            {
                int next_stat;

                if (!maxed[STAT_DEX])
                    next_stat = STAT_DEX;
                else if (!maxed[STAT_INT] && spell_stat != STAT_INT)
                    next_stat = STAT_INT;
                else if (!maxed[STAT_WIS] && spell_stat != STAT_WIS)
                    next_stat = STAT_WIS;
                else
                {
                    step++;
                    break;
                }

                /* Buy until we can't buy any more. */
                while (buy_stat(next_stat, st, spent, inc, left));
                maxed[next_stat] = true;

                break;
            }

            default:
            {
                step = -1;
                break;
            }
        }
    }
}


/*
 * Roll for a characters stats using either point-based or standard roller
 *
 * Returns true if stats were rolled, false otherwise (in this case, apply default roller)
 */
static bool get_stats_aux(struct player *p, int16_t* stat_roll)
{
    int i, j;
    int16_t stats[STAT_MAX], stat_order[STAT_MAX], stat_limit[STAT_MAX], stat_ok[STAT_MAX];

    /* Default roller */
    if (stat_roll[STAT_MAX] == BR_DEFAULT) return false;

    /* Point-based roller */
    if (stat_roll[STAT_MAX] == BR_POINTBASED)
    {
        int cost = 0;

        /* Check over the given stats */
        for (i = 0; i < STAT_MAX; i++)
        {
            /* Check data */
            if ((stat_roll[i] < 10) || (stat_roll[i] > 18))
            {
                /* Incorrect data: use default roller */
                return false;
            }

            /* Total cost */
            cost += birth_stat_costs[stat_roll[i] - 10];
        }

        /* Incorrect data: use default roller */
        if (cost > MAX_BIRTH_POINTS) return false;

        /* Stats are given by "stat_roll" directly */
        for (i = 0; i < STAT_MAX; i++) p->stat_max[i] = stat_roll[i];

        return true;
    }

    /* Standard roller */

    /* Clear "stats" array */
    for (i = 0; i < STAT_MAX; i++) stats[i] = 0;

    /* Stat order is given by "stat_roll" directly */
    for (i = 0; i < STAT_MAX; i++) stat_order[i] = stat_roll[i];

    /*
     * Ensure a minimum value of 17 for the first stat, 15 for the second
     * stat and 12 for the third stat; other stats have the legal minimum
     * value of 8
     */
    stat_limit[0] = 17;
    stat_limit[1] = 15;
    stat_limit[2] = 12;
    for (i = 3; i < STAT_MAX; i++) stat_limit[i] = 8;

    /* Clear "stat_ok" array */
    for (i = 0; i < STAT_MAX; i++) stat_ok[i] = 0;

    /* Check over the given stat order */
    for (i = 0; i < STAT_MAX; i++)
    {
        /* Check data */
        if ((stat_order[i] < 0) || (stat_order[i] >= STAT_MAX))
        {
            /* Incorrect data: use default roller */
            return false;
        }

        /* Increment "stat_ok" */
        stat_ok[stat_order[i]]++;
    }

    /* Check for duplicated or missing entries */
    for (i = 0; i < STAT_MAX; i++)
    {
        /* Check "stat_ok" flag */
        if (stat_ok[i] != 1)
        {
            /* Incorrect order: use default roller */
            return false;
        }
    }

    /* Roll */
    while (true)
    {
        bool accept = true;

        /* Roll and verify some stats */
        roll_stats(stats);

        /* Clear "stat_ok" array */
        for (i = 0; i < STAT_MAX; i++) stat_ok[i] = 0;

        /* Count acceptable stats */
        for (i = 0; i < STAT_MAX; i++)
        {
            /* Increment count of acceptable stats */
            for (j = 0; j < STAT_MAX; j++)
            {
                /* This stat is okay */
                if (stats[j] >= stat_limit[i]) stat_ok[i]++;
            }
        }

        /* Check acceptable stats */
        for (i = 0; i < STAT_MAX; i++)
        {
            /* This stat is not okay */
            if (stat_ok[i] <= i)
            {
                accept = false;
                break;
            }
        }

        /* Break if "happy" */
        if (accept) break;
    }

    /* Sort the stats */
    sort_stats(stats, NULL);

    /* Put stats in the correct order */
    for (i = 0; i < STAT_MAX; i++) p->stat_max[stat_order[i]] = stats[i];

    return true;
}


/*
 * Roll for a characters stats
 *
 * For efficiency, we include a chunk of "calc_bonuses()".
 */
static void get_stats(struct player *p, int16_t* stat_roll)
{
    int i;

    /* Default roller */
    if (!get_stats_aux(p, stat_roll))
    {
        int16_t stats[STAT_MAX];
        int points_spent[STAT_MAX];
        int points_inc[STAT_MAX];
        int points_left;

        reset_stats(stats, points_spent, points_inc, &points_left);
        generate_stats(p, stats, points_spent, points_inc, &points_left);
        for (i = 0; i < STAT_MAX; i++) p->stat_max[i] = stats[i];
    }

    /* Save the stats */
    for (i = 0; i < STAT_MAX; i++)
    {
        /* Start fully healed */
        p->stat_cur[i] = p->stat_max[i];

        /* Start with unscrambled stats */
        p->stat_map[i] = i;

        /* Save birth stats */
        p->stat_birth[i] = p->stat_max[i];
    }
}


static void roll_hp(struct player *p)
{
    int i, j, min_value, max_value;

    /* Minimum hitpoints at highest level */
    min_value = (PY_MAX_LEVEL * (p->hitdie - 1) * 3) / 8;
    min_value += PY_MAX_LEVEL;

    /* Maximum hitpoints at highest level */
    max_value = (PY_MAX_LEVEL * (p->hitdie - 1) * 5) / 8;
    max_value += PY_MAX_LEVEL;

    /* Roll out the hitpoints */
    while (true)
    {
        /* Roll the hitpoint values */
        for (i = 1; i < PY_MAX_LEVEL; i++)
        {
            j = randint1(p->hitdie);
            p->player_hp[i] = p->player_hp[i - 1] + j;
        }

        /* XXX Could also require acceptable "mid-level" hitpoints */

        /* Require "valid" hitpoints at highest level */
        if (p->player_hp[PY_MAX_LEVEL - 1] < min_value) continue;
        if (p->player_hp[PY_MAX_LEVEL - 1] > max_value) continue;

        /* Acceptable */
        break;
    }
}


/*
 * Calculate the bonuses and hitpoints. Don't send messages to the client.
 */
static void get_bonuses(struct player *p)
{
    /* Calculate the bonuses and hitpoints */
    p->upkeep->update |= (PU_BONUS);

    /* Update stuff */
    update_stuff(p, chunk_get(&p->wpos));

    /* Fully healed */
    p->chp = p->mhp;

    /* Fully rested */
    p->csp = p->msp;
}


#define g_strcat(P, T) \
    n = strlen((T)); my_strcpy((P), (T), n + 1); (P) += n


/*
 * Get the racial history, and social class, using the "history charts".
 */
static void get_history(struct player *p)
{
    struct history_entry *entry;
    int i, n;
    char *s, *t;
    char buf[240];
    struct history_chart *chart;

    /* Clear the previous history strings */
    for (i = 0; i < N_HIST_LINES; i++) p->history[i][0] = '\0';

    /* Clear the history text */
    buf[0] = '\0';

    /* Set pointer */
    t = &buf[0];

    /* Starting place */
    chart = p->race->history;

    /* Process the history */
    while (chart)
    {
        /* Roll for nobility */
        int roll = randint1(100);

        /* Get the proper entry in the table */
        for (entry = chart->entries; entry; entry = entry->next)
        {
            if (roll <= entry->roll) break;
        }
        my_assert(entry);

        /* Get the textual history */
        my_strcat(buf, entry->text, sizeof(buf));
        for (s = entry->text; *s; s++)
        {
            switch (*s)
            {
                case '$':
                case '~':
                    s++;
                    switch (*s)
                    {
                        case 'u': {g_strcat(t, "You"); break;}
                        case 'r': {g_strcat(t, "Your"); break;}
                        case 'a': {g_strcat(t, "are"); break;}
                        case 'h': {g_strcat(t, "have"); break;}
                        case 'w': {g_strcat(t, "were"); break;}
                        default: continue;
                    }
                    break;
                default:
                    *t++ = *s;
            }
        }
        *t = '\0';

        /* Enter the next chart */
        chart = entry->succ;
    }

    /* Skip leading spaces */
    for (s = buf; *s == ' '; s++) /* loop */;

    /* Get apparent length */
    n = strlen(s);

    /* Kill trailing spaces */
    while ((n > 0) && (s[n - 1] == ' ')) s[--n] = '\0';

    /* Start at first line */
    i = 0;

    /* Collect the history */
    while (true)
    {
        /* Extract remaining length */
        n = strlen(s);

        /* All done */
        if (n < N_HIST_WRAP)
        {
            /* Save one line of history */
            my_strcpy(p->history[i++], s, sizeof(p->history[0]));

            /* All done */
            break;
        }

        /* Find a reasonable break-point */
        for (n = N_HIST_WRAP - 1; ((n > 0) && (s[n - 1] != ' ')); n--) /* loop */;

        /* Save next location */
        t = s + n;

        /* Save one line of history */
        my_strcpy(p->history[i++], s, n + 1);

        /* Start next line */
        s = t;
    }
}


/*
 * Computes character's age, height, and weight
 */
static void get_ahw(struct player *p)
{
    /* Calculate the age */
    p->age = p->race->b_age + randint1(p->race->m_age);

    /* Calculate the height/weight */
    p->ht = Rand_normal(p->race->base_hgt, p->race->mod_hgt);
    p->wt = Rand_normal(p->race->base_wgt, p->race->mod_wgt);
}


/*
 * Get the player's starting money
 */
static void get_money(struct player *p, bool no_recall, bool force_descend)
{
    p->au = z_info->start_gold;

    // 10g for each point in charisma
    if (p->stat_birth[STAT_CHR] > 10)
        p->au += 10*(p->stat_birth[STAT_CHR] - 10);

    // nominal boni from account points
    if (p->account_score >= 10 && !OPT(p, birth_hardcore))
        p->au += p->account_score / 10;

    /* Give double starting gold to no_recall characters */
    if ((cfg_diving_mode == 3) || (no_recall && force_descend)) p->au *= 2;
}


/*
 * Try to wield everything wieldable in the inventory.
 */
static void wield_all(struct player *p)
{
    struct object *obj, *new_pile = NULL;
    int slot;

    /* Scan through the slots */
    for (obj = p->gear; obj; obj = obj->next)
    {
        struct object *obj_temp;

        /* Make sure we can wield it */
        if (!item_tester_hook_wear(p, obj)) continue;
        slot = wield_slot(p, obj);
        obj_temp = slot_object(p, slot);
        if (obj_temp) continue;

        /* Split if necessary */
        if (obj->number > 1)
        {
            /* All but one go to the new object */
            struct object *obj_new = object_split(obj, obj->number - 1);

            /* Add to the pile of new objects to carry */
            pile_insert(&new_pile, obj_new);
        }

        /* Wear the new stuff */
        obj->oidx = z_info->pack_size + slot;
        p->body.slots[slot].obj = obj;

        /* Increment the equip counter by hand */
        p->upkeep->equip_cnt++;
    }

    /* Now add the unwielded split objects to the gear */
    if (new_pile)
        pile_insert_end(&p->gear, new_pile);
}


static void player_outfit_aux(struct player *p, struct object_kind *k, uint8_t number, bool gift)
{
    struct object *obj = object_new();

    /* PWMAngband: food and light are free, as well as gifts */
    bool free = (tval_is_food_k(k) || tval_is_light_k(k) || gift);

    /* Prepare the item */
    object_prep(p, chunk_get(&p->wpos), obj, k, 0, MINIMISE);
    if (number) obj->number = number;

    /* Hack -- ring of speed (for DM) */
    if (tval_is_ring(obj) && (obj->sval == lookup_sval(obj->tval, "Speed")))
        obj->modifiers[OBJ_MOD_SPEED] = 30;

    /* Set origin */
    set_origin(obj, ORIGIN_BIRTH, 0, NULL);

    /* Object is known */
    object_notice_everything_aux(p, obj, false, false);

    /* Bypass auto-ignore */
    obj->ignore_protect = 1;

    /* Deduct the cost of the item from starting cash */
    if (!free) p->au -= (int32_t)object_value(p, obj, obj->number);

    /* Carry the item */
    inven_carry(p, obj, true, false);
    p->kind_everseen[k->kidx] = 1;
}


/*
 * Init players with some belongings
 *
 * Having an item identifies it and makes the player "aware" of its purpose.
 */
static void player_outfit(struct player *p, bool options[OPT_MAX])
{
    int i;
    const struct start_item *si;
    const struct gift *g;

    /* Player learns innate runes */
    player_learn_innate(p);

    /* Give the player obvious object knowledge */
    p->obj_k->dd = 1;
    p->obj_k->ds = 1;
    p->obj_k->ac = 1;
    p->obj_k->to_a = 1;
    p->obj_k->to_h = 1;
    p->obj_k->to_d = 1;
    for (i = 1; i < OF_MAX; i++)
    {
        struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_FLAG, i);

        if (prop->subtype == OFT_LIGHT) of_on(p->obj_k->flags, i);
        if (prop->subtype == OFT_DIG) of_on(p->obj_k->flags, i);
        if (prop->subtype == OFT_THROW) of_on(p->obj_k->flags, i);
    }

    /* Give the player starting equipment */
    for (si = p->clazz->start_items; si; si = si->next)
    {
        int num = rand_range(si->min, si->max);
        struct object_kind *kind = lookup_kind(si->tval, si->sval);

        my_assert(kind);

        /* Without start_kit, only start with food and light */
        if (!options[OPT_birth_start_kit] && !tval_is_food_k(kind) && !tval_is_light_k(kind))
            continue;

        /* Exclude if configured to do so based on birth options. */
        if (si->eopts)
        {
            bool included = true;
            int eind = 0;

            while (si->eopts[eind] && included)
            {
                if (si->eopts[eind] > 0)
                {
                    if (options[si->eopts[eind]]) included = false;

                    /* Don't give unnecessary starting equipment on no_recall servers */
                    if ((si->eopts[eind] == OPT_birth_no_recall) && (cfg_diving_mode == 3))
                        included = false;
                }
                else if (!options[-si->eopts[eind]]) included = false;
                ++eind;
            }
            if (!included) continue;
        }

        player_outfit_aux(p, kind, (uint8_t)num, false);
    }

    /* Sanity check */
    if (p->au < 0) p->au = 0;

    /*
     * Without start_kit, start at least with the amount of gold we would need for buying
     * the items we don't get
     */
    if (!options[OPT_birth_start_kit])
    {
      int value = 0;

      for (si = p->clazz->start_items; si; si = si->next)
      {
          struct object *obj;
          struct object_kind *kind = lookup_kind(si->tval, si->sval);

          /* Skip food and light (we get them) */
          if (tval_is_food_k(kind) || tval_is_light_k(kind)) continue;

          /* Exclude if configured to do so based on birth options. */
          if (si->eopts)
          {
              bool included = true;
              int eind = 0;

              while (si->eopts[eind] && included)
              {
                  if (si->eopts[eind] > 0)
                  {
                      if (options[si->eopts[eind]]) included = false;

                      /* Skip starting equipment no_recall characters don't get */
                      if ((si->eopts[eind] == OPT_birth_no_recall) && (cfg_diving_mode == 3))
                          included = false;
                  }
                  else if (!options[-si->eopts[eind]]) included = false;
                  ++eind;
              }
              if (!included) continue;
          }

          /* Prepare the item */
          obj = object_new();
          object_prep(p, chunk_get(&p->wpos), obj, kind, 0, MINIMISE);
          obj->number = si->min;
          object_notice_everything_aux(p, obj, false, false);

          /* Add the value */
          value += object_value(p, obj, obj->number);
          object_delete(&obj);
      }
      if (p->au < value) p->au = value;
    }

    /* Give the player racial gifts */
    for (g = p->race->gifts; g; g = g->next)
    {
        int num = rand_range(g->min, g->max);
        struct object_kind *kind = lookup_kind(g->tval, g->sval);

        my_assert(kind);

        /* Hack -- money gift */
        if (tval_is_money_k(kind)) p->au += num;
        else player_outfit_aux(p, kind, (uint8_t)num, true);
    }

    // some got too much gold
    if (streq(p->clazz->name, "Phaseblade") || streq(p->clazz->name, "Telepath") ||
        streq(p->clazz->name, "Timeturner") || streq(p->race->name, "Djinn") ||
        streq(p->race->name, "Half-Giant") || streq(p->race->name, "Titan"))
            p->au /= 2;

    if ((cfg_diving_mode > 0) || options[OPT_birth_no_recall] || is_dm_p(p)) return;

    // extra items for zeitnot
    if (options[OPT_birth_zeitnot])
    {
        int sval = lookup_sval(TV_POTION, "Cure Light Wounds");
        struct object_kind *kind = lookup_kind(TV_POTION, sval);
        my_assert(kind);
        player_outfit_aux(p, kind, 10, true);

        sval = lookup_sval(TV_SCROLL, "Phase Door");
        kind = lookup_kind(TV_SCROLL, sval);
        my_assert(kind);
        player_outfit_aux(p, kind, 10, true);
    }

    // TODO: give zeitnot food, light, etc
    // as in T houses become permanent - no need to give free stuff
    /*
    // Give the player a deed of property
    player_outfit_aux(p, lookup_kind_by_name(TV_DEED, "Deed of Property"), 1, true);

    // Give the player a house creation scroll
    player_outfit_aux(p, lookup_kind_by_name(TV_SCROLL, "Small House Creation"), 1, true);
    */
}


/*
 * Init the DM with some belongings
 */
static void player_outfit_dm(struct player *p)
{
    int i;
    const struct start_item *si;

    /* Initialize the DM with special powers */
    if (is_dm_p(p))
    {
        p->exp = p->max_exp = 50000000;
        if (player_has(p, PF_PERM_SHAPE))
        {
            for (i = 1; i <= PY_MAX_LEVEL; i++)
            {
                p->lev = p->max_lev = i;
                if (player_has(p, PF_DRAGON)) poly_dragon(p, false);
                else poly_shape(p, false);
            }
        }
        else
            p->lev = p->max_lev = PY_MAX_LEVEL;
        if (p->dm_flags & DM_INVULNERABLE)
        {
            p->timed[TMD_INVULN] = -1;
            p->upkeep->update |= (PU_MONSTERS);
            p->upkeep->redraw |= (PR_MAP | PR_STATUS);
        }
        if (!player_has(p, PF_PERM_SHAPE)) set_ghost_flag(p, 1, false);
        p->noscore = 1;
        get_bonuses(p);
        p->timed[TMD_TRAPSAFE] = -1;
        // to test housing
        p->account_score = 10000;
    }

    /*
     * Give the DM some interesting stuff
     * In debug mode, everyone gets all that stuff for testing purposes
     */
#if !(defined(DEBUG_MODE) || defined(TEST_MODE))
    if (!is_dm_p(p)) return;
#endif

    /* All books */
    for (i = 0; i < p->clazz->magic.num_books; i++)
    {
        struct class_book *book = &p->clazz->magic.books[i];

        if (book->realm->book_noun)
            player_outfit_aux(p, lookup_kind(book->tval, book->sval), 1, true);
    }

    /* Other useful stuff */
    for (si = dm_start_items; si; si = si->next)
    {
        struct object_kind *kind = lookup_kind(si->tval, si->sval);

        my_assert(kind);
        player_outfit_aux(p, kind, (uint8_t)si->min, true);
    }

    /* Max recall depth */
    p->max_depth = z_info->max_depth - 1;

    /* A ton of gold */
    p->au = 50000000;
}


/*
 * This fleshes out a full player based on the choices currently made,
 * and so is called whenever things like race or class are chosen.
 */
static void player_generate(struct player *p, uint8_t psex, const struct player_race *r,
    const struct player_class *c)
{
    int i;

    p->psex = psex;
    p->clazz = c;
    p->race = r;

    /* Initialize the spells */
    player_spells_init(p);

    p->sex = &sex_info[p->psex];

    /* Level 1 */
    p->max_lev = p->lev = 1;

    /* Experience factor */
    p->expfact = p->race->r_exp;

    /* Hitdice */
    p->hitdie = p->race->r_mhp + p->clazz->c_mhp;

    /* Pre-calculate level 1 hitdice */
    p->player_hp[0] = p->hitdie;

    /*
     * Fill in overestimates of hitpoints for additional levels. Do not
     * do the actual rolls so the player can not reset the birth screen
     * to get a desirable set of initial rolls.
     */
    for (i = 1; i < p->lev; i++) p->player_hp[i] = p->player_hp[i - 1] + p->hitdie;

    /* Initial hitpoints */
    p->mhp = p->player_hp[p->lev - 1];
}


static int count_players(struct player *p)
{
    int i, count = 0;

    /* Count players on this level */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *player = player_get(i);

        /* Skip this player */
        if (player == p) continue;

        /* Count */
        if (wpos_eq(&player->wpos, &p->wpos)) count++;
    }

    return count;
}


static bool depth_is_valid(struct wild_type *w_ptr, int depth)
{
    if (depth == 0) return true;
    if ((depth >= w_ptr->min_depth) && (depth < w_ptr->max_depth)) return true;
    return false;
}


// Load character account score
static int birth_load_account_score(struct player *p)
{
    ang_file *fh;
    char filename[MSG_LEN];
    char filebuf[MSG_LEN];
    char score_buf[MSG_LEN];
    unsigned long score_tmp = 0;
    uint32_t new_score = 0;
    bool name_ok = false;
    char str;

    /* Check account file */
    path_build(filename, sizeof(filename), ANGBAND_DIR_SCORES, "ladder.raw");
    if (file_exists(filename))
    {
        /* Open the file */
        fh = file_open(filename, MODE_READ, FTYPE_TEXT);
        if (!fh)
        {
            plog("Failed to open ladder file!");
            return 0L;
        }

        /* Process the file */
        while (file_getl(fh, filebuf, sizeof(filebuf)))
        {
            /* Compare account name with buffer */
            name_ok = !my_stricmp(filebuf, p->account_name);
            
            /* Account name match with entry in the ladder file */
            if (name_ok)
            {
                // get next line after account name
                file_getl(fh, score_buf, sizeof(score_buf));
                // as there is no strtoui() - make manual conversion from long to uint32_t
                score_tmp = strtoul(score_buf, NULL, 10);
                new_score = (uint32_t)score_tmp;
                // fill the score in p
                p->account_score = new_score;
                file_close(fh);
                // done!
                return 0L;
            }
        }
        // nothing found.. so close 'MODE_READ' file
        file_close(fh);
    }

    // If we get here, account was not found or file doesn't exist
    p->account_score = 0;
    return 0L;
}


static void player_setup(struct player *p, int id, uint32_t account, bool zeitnot, bool no_recall, bool force_descend)
{
    struct wild_type *w_ptr = get_wt_info_at(&p->wpos.grid);
    bool reposition = false, push_up = false;
    int i, k, d;
    hturn death_turn;
    struct chunk *c;

    /* Paranoia: catch bad player coordinates */

    /* Invalid wilderness coordinates */
    if (!w_ptr) reposition = true;

    /* Invalid depth */
    else if (!depth_is_valid(w_ptr, p->wpos.depth)) reposition = true;

    /* Default location if just starting */
    else if (wpos_null(&p->wpos) && loc_is_zero(&p->grid)) reposition = true;

    /* Don't allow placement inside an arena */
    else if (pick_arena(&p->wpos, &p->grid) != -1)
    {
        reposition = true;

        /* Unstatic the old level */
        chunk_set_player_count(&p->wpos, count_players(p));
    }

    /* Hack -- DM redesigning the level */
    else if (chunk_inhibit_players(&p->wpos))
    {
        reposition = true;

        // No-recall/zeitnot players are simply pushed up one level (should be safe)
        if ((cfg_diving_mode == 3) || no_recall || zeitnot) push_up = true;
    }

    /*
     * Don't allow placement inside a house if someone is shopping or
     * if we don't own it (anti-exploit)
     */
    else
    {
        struct player *q;

        for (i = 0; i < houses_count(); i++)
        {
            /* Are we inside this house? */
            if (!house_inside(p, i)) continue;

            /* If we don't own it, get out of it */
            if (!house_owned_by(p, i))
            {
                reposition = true;

                /* Unstatic the old level */
                chunk_set_player_count(&p->wpos, count_players(p));

                break;
            }

            /* Is anyone shopping in it? */
            for (k = 1; k <= NumPlayers; k++)
            {
                q = player_get(k);
                if (q && (p != q))
                {
                    /* Someone in here? */
                    if (q->player_store_num == i)
                    {
                        struct store *s = store_at(q);

                        if (s && (s->feat == FEAT_STORE_PLAYER))
                        {
                            reposition = true;

                            /* Unstatic the old level */
                            chunk_set_player_count(&p->wpos, count_players(p));
                        }

                        break;
                    }
                }
            }

            break;
        }
    }

    /* Reset */
    p->arena_num = -1;

    /* If we need to reposition the player, do it */
    if (reposition)
    {
        /* Hack -- DM redesigning the level (no_recall players) */
        if (push_up) p->wpos.depth = dungeon_get_next_level(p, p->wpos.depth, -1);

        // zeitnot: start zeitnot_timer and put us in zeitnot town
        else if (zeitnot) {
            p->zeitnot_timer = 5; // almost immediately after respawn - teleport to jail
            memcpy(&p->wpos, zeitnot_wpos(), sizeof(struct worldpos));
        }

        /* brave: put us in base town */
        else if ((cfg_diving_mode > 1) || (no_recall && force_descend))
            memcpy(&p->wpos, base_wpos(), sizeof(struct worldpos));

        /* Put us in starting town */
        else
            memcpy(&p->wpos, start_wpos(), sizeof(struct worldpos));
    }

    /* Make sure the server doesn't think the player is in a store */
    p->store_num = -1;

    c = chunk_get(&p->wpos);

    /* Rebuild the level if necessary */
    if (!c)
    {
        /* Generate a dungeon level there */
        c = prepare_next_level(p);

        /* Player is now on the level */
        chunk_increase_player_count(&p->wpos);

        wild_deserted_message(p);

        /* Paranoia: update the player's wilderness map */
        if (p->wpos.depth == 0) wild_set_explored(p, &p->wpos);
    }

    /* Apply illumination */
    else
    {
        bool done = false;
        bool quit_daytime = is_daytime_turn(&p->quit_turn);
        bool join_daytime = is_daytime();

        /* If we need to reposition the player, do it */
        if (reposition)
        {
            /* Clear the flags for each cave grid (cave dimensions may have changed) */
            player_cave_new(p, c->height, c->width);
            player_cave_clear(p, true);
            player_place_feeling(p, c);
            done = true;
        }

        /*
         * Make sure he's supposed to be here -- if not, then the level has
         * been unstaticed and so he should forget his memory of the old level.
         */
        if (ht_cmp(&c->generated, &p->quit_turn) > 0)
        {
            /* Clear the flags for each cave grid (cave dimensions may have changed) */
            player_cave_new(p, c->height, c->width);
            player_cave_clear(p, true);
            done = true;

            /* Player is now on the level */
            chunk_increase_player_count(&p->wpos);
        }

        /* Hack -- night time in wilderness */
        if (in_wild(&p->wpos) && !join_daytime)
        {
            player_cave_clear(p, false);
            done = true;
        }

        /* Hack -- player that saved during day and comes back at night (or vice versa) */
        if ((quit_daytime && !join_daytime) || (!quit_daytime && join_daytime))
        {
            player_cave_clear(p, false);
            done = true;
        }

        /* Memorize the content of owned houses */
        if (!done) memorize_houses(p);

        /* Illuminate */
        cave_illuminate(p, c, join_daytime);
    }

    /* Player gets to go first */
    set_energy(p, &p->wpos);

    /* If we need to reposition the player, do it */
    if (reposition)
    {
        /* Put us in the tavern */
        loc_copy(&p->grid, &c->join->down);
    }

    /* Be sure the player is in bounds */
    if (!square_in_bounds_fully(c, &p->grid))
    {
        p->grid.x = MIN(MAX(p->grid.x, 1), c->width - 2);
        p->grid.y = MIN(MAX(p->grid.y, 1), c->height - 2);
    }

    /* Pick a location */
    /* Players should NEVER be placed on top of other stuff */
    /* Simply move the player away until a proper location is found */
    /* If no location can be found (VERY unlikely), then simply use the initial location */
    for (i = 0; i < 3000; i++)
    {
        struct loc new_grid;

        /* Increase distance (try 10 times for each step) */
        d = (i + 9) / 10;

        /* Pick a location (skip LOS test) */
        if (!scatter(c, &new_grid, &p->grid, d, false)) continue;

        /* Require an "empty" floor grid */
        if (square_isemptyfloor(c, &new_grid) && !square_isno_stairs(c, &new_grid))
        {
            /* Set the player's location */
            loc_copy(&p->grid, &new_grid);

            break;
        }
    }

    /* Hack -- set previous player location */
    loc_copy(&p->old_grid, &p->grid);

    /* Add the player */
    square_set_mon(c, &p->grid, 0 - id);

    /* Initialize bubble speed */
    p->bubble_speed = NORMAL_TIME;
    p->blink_speed = (uint32_t)cfg_fps;

    /* Redraw */
    square_light_spot(c, &p->grid);

    /*
     * Delete him from the player name database
     *
     * This is useful for fault tolerance, as it is possible to have
     * two entries for one player name, if the server crashes hideously
     * or the machine has a power outage or something.
     * This is also useful when the savefile has been manually deleted.
     */
    delete_player_name(p->name);

    /* Verify player ID */
    if (!p->id || lookup_player(p->id)) p->id = player_id++;

    /* Add him to the player name database */
    ht_reset(&death_turn);
    add_player_name(p->id, account, p->name, &death_turn);
    plog_fmt("Player Name is [%s], id is %d", p->name, (int)p->id);

    /* Set his "current activities" variables */
    current_clear(p);
    p->current_house = p->current_selling = -1;
    loc_init(&p->old_offset_grid, -1, -1);

    /* Make sure his party still exists */
    if (p->party &&
        ((parties[p->party].num == 0) || (ht_cmp(&parties[p->party].created, &p->quit_turn) > 0)))
    {
        /* Reset to neutral */
        p->party = 0;
    }

    /* Hack -- give 2 turns of invulnerability */
    p->timed[TMD_SAFELOGIN] = 2;

    /* Update and redraw stuff (all of these are probably not needed...) */

    /* Update stuff */
    p->upkeep->update |= (PU_BONUS);

    /* Fully update the visuals (and monster distances) */
    p->upkeep->update |= (PU_UPDATE_VIEW | PU_DISTANCE);

    /* Redraw dungeon */
    p->upkeep->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP);
    set_redraw_equip(p, NULL);
    set_redraw_inven(p, NULL);

    /* Redraw "statusy" things */
    p->upkeep->redraw |= (PR_MONSTER | PR_MONLIST | PR_ITEMLIST);

    /* MAngband */
    p->upkeep->redraw |= (PR_SPELL | PR_PLUSSES);

    /* This guy is alive now */
    p->alive = true;

    /* Hack -- player position is valid now */
    p->placed = true;

    /* Default width for monster list subwindow */
    p->monwidth = NORMAL_WID - 5;
}


static void player_admin(struct player *p)
{
    /* Hack -- set Dungeon Master flags */
#ifdef DEBUG_MODE
    p->dm_flags |= (DM___MENU | DM_CAN_MUTATE_SELF);
#endif

    if (cfg_dungeon_master && !my_stricmp(p->name, cfg_dungeon_master))
    {
        /* All DM powers! */
        p->dm_flags = 0xFFFFFFFF;
        if (!cfg_secret_dungeon_master) p->dm_flags ^= DM_SECRET_PRESENCE;
    }
}


/*
 * Handle quick-start creation
 */
static void quickstart_roll(struct player *p, bool character_existed, uint8_t *pridx, uint8_t *pcidx,
    uint8_t *psex, bool *old_history, int16_t *stat_roll)
{
    /* A character existed in the savefile */
    if (character_existed)
    {
        int i;

        /* Use previous info */
        *pridx = p->race->ridx;
        *pcidx = p->clazz->cidx;
        *psex = p->psex;
        *old_history = true;

        /* Use point-based roller with previous birth stats */
        for (i = 0; i < STAT_MAX; i++) stat_roll[i] = p->stat_birth[i];
        stat_roll[STAT_MAX] = BR_POINTBASED;
    }

    /* New character */
    else
    {
        /* Roll a male half-troll warrior */
        *pridx = 7;
        *pcidx = 0;
        *psex = 1;
        *old_history = false;

        /* Use standard roller with STR CON DEX WIS INT as stat order */
        stat_roll[0] = STAT_STR;
        stat_roll[1] = STAT_CON;
        stat_roll[2] = STAT_DEX;
        stat_roll[3] = STAT_WIS;
        stat_roll[4] = STAT_INT;
        stat_roll[5] = STAT_CHR;
        stat_roll[STAT_MAX] = BR_NORMAL;
    }
}


/*
 * Set the savefile name.
 */
bool savefile_set_name(struct player *p)
{
    char path[128];

    player_safe_name(path, sizeof(path), p->name);

    /* Error */
    if (strlen(path) > MAX_NAME_LEN)
    {
        Destroy_connection(p->conn, "Your name is too long!");
        return false;
    }

    /* Build the filename */
    path_build(p->savefile, MSG_LEN, ANGBAND_DIR_SAVE, path);
    path_build(p->panicfile, MSG_LEN, ANGBAND_DIR_PANIC, path);

    return true;
}


/*
 * Get the savefile name.
 */
const char *savefile_get_name(char *savefile, char *panicfile)
{
    if (panicfile[0] && file_exists(panicfile))
    {
        /* Use panic save */
        if (!(savefile[0] && file_exists(savefile)) || file_newer(panicfile, savefile))
            return panicfile;

        /* Remove the out-of-date panic save. */
        file_delete(panicfile);

        /* Use normal save */
        return savefile;
    }

    /* Use normal save */
    if (savefile[0] && file_exists(savefile)) return savefile;

    return NULL;
}


/*
 * Handle dynastic quick start creation
 *
 * Returns 1 if quick start is possible, 0 if quick start is not possible, -1 if an error occurs.
 */
static int quickstart_ok(struct player *p, const char *name, int conn, bool zeitnot, bool no_recall, bool force_descend)
{
    char previous[NORMAL_WID];
    const char *loadpath;

    /* Get last incarnation */
    my_strcpy(previous, name, sizeof(previous));
    if (!get_previous_incarnation(previous, sizeof(previous))) return 0;

    /* Clear old information */
    init_player(p, conn, false, zeitnot, no_recall, force_descend);

    /* Copy his name */
    my_strcpy(p->name, previous, sizeof(p->name));

    /* Verify his name and create a savefile name */
    if (!savefile_set_name(p)) return -1;

    /* Try to load the savefile */
    p->is_dead = true;
    loadpath = savefile_get_name(p->savefile, p->panicfile);
    if (!loadpath)
    {
        /* Last incarnation: if "Foo I" doesn't exist, try "Foo" */
        my_strcpy(p->name, strip_suffix(previous), sizeof(p->name));
        if (!savefile_set_name(p)) return -1;
        loadpath = savefile_get_name(p->savefile, p->panicfile);
        if (!loadpath) return 0;
    }
    if (!load_player(p, loadpath)) return -1;

    /* Still alive */
    if (!p->is_dead) return 0;

    /* Success */
    return 1;
}


/*
 * Create a character.  Then wait for a moment.
 *
 * The delay may be reduced, but is recommended to keep players
 * from continuously rolling up characters, which can be VERY
 * expensive CPU wise.
 *
 * Note that we may be called with "junk" leftover in the various
 * fields, so we must be sure to clear them first.
 */
struct player *player_birth(int id, uint32_t account, const char *name, const char *pass, int conn,
    uint8_t ridx, uint8_t cidx, uint8_t psex, int16_t* stat_roll, bool options[OPT_MAX])
{
    struct player *p;
    int i;
    bool character_existed = false;
    bool old_history = false;
    const char *loadpath;

    /* Do some consistency checks */
    if (ridx >= player_rmax()) ridx = 0;
    if (cidx >= player_cmax()) cidx = 0;
    if (psex >= MAX_SEXES) psex = SEX_MALE;

    /* Allocate player and set pointer */
    p = mem_zalloc(sizeof(struct player));
    player_set(id, p);

    /* Handle dynastic quick start */
    if (stat_roll[STAT_MAX] == BR_QDYNA)
    {
        int ret = quickstart_ok(p, name, conn, options[OPT_birth_zeitnot],
            options[OPT_birth_no_recall], options[OPT_birth_force_descend]);

        if (ret == -1)
        {
            cleanup_player(p);
            mem_free(p);
            player_set(id, NULL);
            return NULL;
        }
        quickstart_roll(p, (ret? true: false), &ridx, &cidx, &psex, &old_history, stat_roll);
    }

    /* Clear old information */
    init_player(p, conn, old_history, options[OPT_birth_zeitnot], options[OPT_birth_no_recall],
        options[OPT_birth_force_descend]);

    /* Copy his name */
    my_strcpy(p->name, name, sizeof(p->name));
    my_strcpy(p->pass, pass, sizeof(p->pass));

    /* DM powers? */
    player_admin(p);

    /* Verify his name and create a savefile name */
    if (!savefile_set_name(p))
    {
        cleanup_player(p);
        mem_free(p);
        player_set(id, NULL);
        return NULL;
    }

    /*** Try to load the savefile ***/

    p->is_dead = true;
    loadpath = savefile_get_name(p->savefile, p->panicfile);

    /* Try loading */
    if (loadpath)
    {
        if (!load_player(p, loadpath))
        {
            cleanup_player(p);
            mem_free(p);
            player_set(id, NULL);
            return NULL;
        }

        /* Important: check password */
        if (strcmp(p->pass, pass))
        {
            plog("Invalid password");
            Destroy_connection(p->conn, "Invalid password");
            cleanup_player(p);
            mem_free(p);
            player_set(id, NULL);
            return NULL;
        }

        /* Player is dead */
        if (p->is_dead)
        {
            /* A character existed in this savefile. */
            character_existed = true;
        }
    }

    /* No living character loaded */
    if (p->is_dead)
    {
        /* Make new player */
        p->is_dead = false;

        // hack: do not allow having both zeitnot and brave modes
        if (options[OPT_birth_zeitnot] &&
            (options[OPT_birth_no_recall] || options[OPT_birth_force_descend]))
        {
            options[OPT_birth_no_recall] = false;
            options[OPT_birth_force_descend] = false;
        }

        /* Handle quick start */
        if (stat_roll[STAT_MAX] == BR_QUICK)
            quickstart_roll(p, character_existed, &ridx, &cidx, &psex, &old_history, stat_roll);

        /* Hack -- rewipe the player info if load failed */
        init_player(p, conn, old_history, options[OPT_birth_zeitnot], options[OPT_birth_no_recall],
            options[OPT_birth_force_descend]);

        /* Copy his name and connection info */
        my_strcpy(p->name, name, sizeof(p->name));
        my_strcpy(p->pass, pass, sizeof(p->pass));
        // also copy account name
        my_strcpy(p->account_name, get_connection(p->conn)->nick_account, sizeof(p->account_name));
        // store account ID for housing
        p->account_id = account;
        // init ladder file (if it's not there yet)..
        // ..or load player account_points
        birth_load_account_score(p);
        // give 1 life
        p->lives = 1;

        /* Reprocess his name */
        if (!savefile_set_name(p))
        {
            cleanup_player(p);
            mem_free(p);
            player_set(id, NULL);
            return NULL;
        }

        /* DM powers? */
        player_admin(p);

        /* Set his ID */
        p->id = player_id++;

/* TEST: intercept ridx and cind on server side

        if (p->account_score == 0 && (ridx > 3 || cidx > 3))
        {
            plog("Wrong race/class combo");
            Destroy_connection(p->conn, "You don't have enough account points to choose such race/class combination");
            cleanup_player(p);
            mem_free(p);
            player_set(id, NULL);
            return NULL;
        }
*/

        /* Actually Generate */
        player_generate(p, psex, player_id2race(ridx), player_id2class(cidx));

        /* Get a new character */
        get_stats(p, stat_roll);

        /* Update stats with bonuses, etc. */
        get_bonuses(p);

        /* Roll for age/height/weight */
        get_ahw(p);

        /* Roll for social class */
        if (!old_history) get_history(p);

        roll_hp(p);

        /* Embody */
        player_embody(p);

        /* Give the player some money */
        get_money(p, options[OPT_birth_no_recall], options[OPT_birth_force_descend]);

        /* Outfit the player, if they can sell the stuff */
        player_outfit(p, options);
        player_outfit_dm(p);

        /* Now try wielding everything */
        wield_all(p);

        /* Permanently polymorphed characters */
        if (player_has(p, PF_PERM_SHAPE))
        {
            if (player_has(p, PF_DRAGON)) poly_dragon(p, false);
            else poly_shape(p, false);
            get_bonuses(p);
        }

        /* Set his location, panel, etc. */
        player_setup(p, id, account, options[OPT_birth_zeitnot], options[OPT_birth_no_recall],
            options[OPT_birth_force_descend]);

        /* Add new starting message */
        history_add_unique(p, "Began the quest to destroy Morgoth", HIST_PLAYER_BIRTH);

        /* Give the DM full knowledge */
        if (is_dm_p(p))
        {
            /* Every item in the game */
            for (i = 0; i < z_info->k_max; i++) p->kind_everseen[i] = 1;
            for (i = 0; i < z_info->e_max; i++) p->ego_everseen[i] = 1;

            /* Every monster in the game */
            for (i = 1; i < z_info->r_max; i++)
            {
                p->lore[i].pseen = 1;
                p->lore[i].pkills = z_info->max_depth;
                lore_update(&r_info[i], &p->lore[i]);
            }

            /* Every rune in the game */
            player_learn_everything(p);
        }

        /* Success */
        return p;
    }

    /* Paranoia: ensure that permanently polymorphed characters have the proper race when logging */
    if (player_has(p, PF_PERM_SHAPE))
    {
        if (player_has(p, PF_DRAGON)) poly_dragon(p, false);
        else poly_shape(p, false);
        get_bonuses(p);
    }

    /* Loading succeeded */
    player_setup(p, id, account, options[OPT_birth_zeitnot], options[OPT_birth_no_recall],
        options[OPT_birth_force_descend]);
    return p;
}


/*
 * We are starting a "brand new" server.
 * This function is only called if the server state savefile could not be loaded.
 */
void server_birth(void)
{
    /* Set party zero's name to "Neutral" */
    my_strcpy(parties[0].name, "Neutral", sizeof(parties[0].name));

    /* Seed for flavors */
    seed_flavor = randint0(0x10000000);
    flavor_init();

    /* Seed for wilderness layout */
    seed_wild = randint0(0x10000000);

    /* Hack -- enter the world */
    ht_reset(&turn);
    ht_add(&turn, 1);

    /* First player's ID should be 1 */
    player_id = 1;

    /* Initialize the stores */
    store_reset();
}


/*  
 * Check if the given connection type is valid.
 */
uint16_t connection_type_ok(uint16_t conntype)
{
    if (conntype == CONNTYPE_PLAYER) return CONNTYPE_PLAYER;
    if (conntype == 8202 || conntype == 8205) return CONNTYPE_CONSOLE;

    /* PuTTY via Telnet */
    if (conntype == 65531) return CONNTYPE_CONSOLE;

    return CONNTYPE_ERROR;
}
