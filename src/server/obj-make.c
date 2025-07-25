/*
 * File: obj-make.c
 * Purpose: Object generation functions
 *
 * Copyright (c) 1987-2007 Angband contributors
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
 * This table provides for different gold drop rates at different dungeon depths.
 */
uint16_t level_golds[128];


/** Arrays holding an index of objects to generate for a given level */
static uint32_t *obj_total;
static uint8_t *obj_alloc;


static uint32_t *obj_total_great;
static uint8_t *obj_alloc_great;


static int16_t alloc_ego_size = 0;
static alloc_entry *alloc_ego_table;


struct money
{
    char *name;
    int type;
};


static struct money *money_type;
static int num_money_types;


static uint8_t get_artifact_rarity(int32_t value, struct object_kind *kind)
{
    int32_t alloc = 50000000 / (value * (kind->alloc_prob? kind->alloc_prob: 100));

    if (alloc > 990) return 99;
    if (alloc < 10) return 1;

    return (uint8_t)(((alloc - (alloc / 10) * 10) >= 5)? alloc / 10 + 1: alloc / 10);
}


/*
 * Initialize object allocation info
 */
static void alloc_init_objects(void)
{
    int item, lev;
    int k_max = z_info->k_max;
    int i;

    /* Allocate and wipe */
    obj_alloc = mem_zalloc((z_info->max_obj_depth + 1) * k_max * sizeof(uint8_t));
    obj_alloc_great = mem_zalloc((z_info->max_obj_depth + 1) * k_max * sizeof(uint8_t));
    obj_total = mem_zalloc(z_info->max_depth * sizeof(uint32_t));
    obj_total_great = mem_zalloc(z_info->max_depth * sizeof(uint32_t));

    /* Init allocation data */
    for (item = 0; item < k_max; item++)
    {
        const struct object_kind *kind = &k_info[item];
        int min = kind->alloc_min;
        int max = kind->alloc_max;

        /* If an item doesn't have a rarity, move on */
        if (!kind->alloc_prob) continue;

        /* Go through all the dungeon levels */
        for (lev = 0; lev <= z_info->max_obj_depth; lev++)
        {
            int rarity = kind->alloc_prob;

            /* Save the probability in the standard table */
            if ((lev < min) || (lev > max)) rarity = 0;
            obj_total[lev] += rarity;
            obj_alloc[(lev * k_max) + item] = rarity;

            /* Save the probability in the "great" table if relevant */
            if (!kind_is_good(kind)) rarity = 0;
            obj_total_great[lev] += rarity;
            obj_alloc_great[(lev * k_max) + item] = rarity;
        }
    }

    /* Automatically compute art rarities for PWMAngband's artifacts */
    for (i = 0; i < z_info->a_max; i++)
    {
        struct artifact *art = &a_info[i];
        struct object *fake;
        int32_t value;

        /* PWMAngband artifacts don't have a rarity */
        if (art->alloc_prob) continue;

        /* Create a "forged" artifact */
        if (!make_fake_artifact(&fake, art)) continue;

        /* Get the value */
        value = (int32_t)object_value_real(NULL, fake, 1);

        /* Allocation probability */
        art->alloc_prob = get_artifact_rarity(value, fake->kind);

        object_delete(&fake);
    }
}


/*
 * Initialize ego-item allocation info
 *
 * The ego allocation probabilities table (alloc_ego_table) is sorted in
 * order of minimum depth. Precisely why, I'm not sure! But that is what
 * the code below is doing with the arrays 'num' and 'level_total'.
 */
static void alloc_init_egos(void)
{
    int *num = mem_zalloc(z_info->max_depth * sizeof(int));
    int *level_total = mem_zalloc(z_info->max_depth * sizeof(int));
    int i;

    for (i = 0; i < z_info->e_max; i++)
    {
        struct ego_item *ego = &e_info[i];

        /* Legal items */
        if (ego->alloc_prob)
        {
            /* Count the entries */
            alloc_ego_size++;

            /* Group by level */
            num[ego->alloc_min]++;
        }
    }

    /* Collect the level indexes */
    for (i = 1; i < z_info->max_depth; i++)
    {
        /* Group by level */
        num[i] += num[i - 1];
    }

    /* Allocate the alloc_ego_table */
    alloc_ego_table = mem_zalloc(alloc_ego_size * sizeof(alloc_entry));

    /* Scan the ego items */
    for (i = 0; i < z_info->e_max; i++)
    {
        struct ego_item *ego = &e_info[i];

        /* Count valid pairs */
        if (ego->alloc_prob)
        {
            int min_level = ego->alloc_min;

            /* Skip entries preceding our locale */
            int y = ((min_level > 0)? num[min_level - 1]: 0);

            /* Skip previous entries at this locale */
            int z = y + level_total[min_level];

            /* Load the entry */
            alloc_ego_table[z].index = i;
            alloc_ego_table[z].level = min_level;   /* Unused */
            alloc_ego_table[z].prob1 = ego->alloc_prob;
            alloc_ego_table[z].prob2 = ego->alloc_prob;
            alloc_ego_table[z].prob3 = ego->alloc_prob;

            /* Another entry complete for this locale */
            level_total[min_level]++;
        }
    }

    mem_free(level_total);
    mem_free(num);
}


/*
 * Initialize money info
 */
static void init_money_svals(void)
{
    int *money_svals;
    int i;

    /* Count the money types and make a list */
    num_money_types = tval_sval_count("gold");
    money_type = mem_zalloc(num_money_types * sizeof(struct money));
    money_svals = mem_zalloc(num_money_types * sizeof(struct money));
    tval_sval_list("gold", money_svals, num_money_types);

    /* List the money types */
    for (i = 0; i < num_money_types; i++)
    {
        struct object_kind *kind = lookup_kind(TV_GOLD, money_svals[i]);

        money_type[i].name = string_make(kind->name);
        money_type[i].type = money_svals[i];
    }

    mem_free(money_svals);
}


static void init_obj_make(void)
{
    alloc_init_objects();
    alloc_init_egos();
    init_money_svals();
}


static void cleanup_obj_make(void)
{
    int i;

    for (i = 0; i < num_money_types; i++) string_free(money_type[i].name);
    mem_free(money_type);
    money_type = NULL;
    mem_free(alloc_ego_table);
    alloc_ego_table = NULL;
    mem_free(obj_total_great);
    obj_total_great = NULL;
    mem_free(obj_total);
    obj_total = NULL;
    mem_free(obj_alloc_great);
    obj_alloc_great = NULL;
    mem_free(obj_alloc);
    obj_alloc = NULL;
}


/*** Make an ego item ***/


/*
 * This is a safe way to choose a random new flag to add to an object.
 * It takes the existing flags and an array of new flags,
 * and returns an entry from newf, or 0 if there are no
 * new flags available.
 */
static int get_new_attr(bitflag flags[OF_SIZE], bitflag newf[OF_SIZE])
{
    int i, options = 0, flag = 0;

    for (i = of_next(newf, FLAG_START); i != FLAG_END; i = of_next(newf, i + 1))
    {
        /* Skip this one if the flag is already present */
        if (of_has(flags, i)) continue;

        /*
         * Each time we find a new possible option, we have a 1-in-N chance of
         * choosing it and an (N-1)-in-N chance of keeping a previous one
         */
        if (one_in_(++options)) flag = i;
    }

    return flag;
}


/*
 * Obtain extra power
 */
static int get_new_power(bitflag flags[OF_SIZE])
{
    int i, options = 0, flag = 0;
    bitflag newf[OF_SIZE];

    create_obj_flag_mask(newf, 0, OFT_PROT, OFT_MISC, OFT_MAX);

    for (i = of_next(newf, FLAG_START); i != FLAG_END; i = of_next(newf, i + 1))
    {
        /* Skip this one if the flag is already present */
        if (of_has(flags, i)) continue;

        /*
         * Each time we find a new possible option, we have a 1-in-N chance of
         * choosing it and an (N-1)-in-N chance of keeping a previous one
         */
        if (one_in_(++options)) flag = i;
    }

    return flag;
}


/*
 * Get a random new base resist on an item
 */
static int random_base_resist(struct object *obj, int *resist)
{
    int i, r, count = 0;

    /* Count the available base resists */
    for (i = ELEM_BASE_MIN; i < ELEM_HIGH_MIN; i++)
    {
        if (obj->el_info[i].res_level[0] == 0) count++;
    }

    if (count == 0) return false;

    /* Pick one */
    r = randint0(count);

    /* Find the one we picked */
    for (i = ELEM_BASE_MIN; i < ELEM_HIGH_MIN; i++)
    {
        if (obj->el_info[i].res_level[0] != 0) continue;
        if (r == 0)
        {
            *resist = i;
            return true;
        }
        r--;
    }

    return false;
}


/*
 * Get a random new high resist on an item
 */
static int random_high_resist(struct object *obj, int *resist)
{
    int i, r, count = 0;

    /* Count the available high resists */
    for (i = ELEM_HIGH_MIN; i <= ELEM_HIGH_MAX; i++)
    {
        if (obj->el_info[i].res_level[0] == 0) count++;
    }

    if (count == 0) return false;

    /* Pick one */
    r = randint0(count);

    /* Find the one we picked */
    for (i = ELEM_HIGH_MIN; i <= ELEM_HIGH_MAX; i++)
    {
        if (obj->el_info[i].res_level[0] != 0) continue;
        if (r == 0)
        {
            *resist = i;
            return true;
        }
        r--;
    }

    return false;
}


/*
 * Generate random powers
 */
static void do_powers(struct object *obj, bitflag kind_flags[KF_SIZE])
{
    int resist = 0, pick = 0;
    bitflag newf[OF_SIZE];

    /* Resist or power? */
    if (kf_has(kind_flags, KF_RAND_RES_POWER)) pick = randint1(3);

    /* Extra powers */
    if (kf_has(kind_flags, KF_RAND_SUSTAIN))
    {
        create_obj_flag_mask(newf, 0, OFT_SUST, OFT_MAX);
        of_on(obj->flags, get_new_attr(obj->flags, newf));
    }
    if (kf_has(kind_flags, KF_RAND_POWER) || (pick == 1))
    {
        int esp_flag = get_new_esp(obj->flags);
        int power_flag = get_new_power(obj->flags);

        /* PWMAngband: 1/11 chance of having a random ESP */
        if (one_in_(11) && esp_flag)
            of_on(obj->flags, esp_flag);
        else
            of_on(obj->flags, power_flag);
    }
    if (kf_has(kind_flags, KF_RAND_ESP))
    {
        int esp_flag = get_new_esp(obj->flags);

        if (esp_flag)
            of_on(obj->flags, esp_flag);
    }
    if (kf_has(kind_flags, KF_RAND_BASE_RES) || (pick > 1))
    {
        /* Get a base resist if available, mark it as random */
        if (random_base_resist(obj, &resist))
        {
            obj->el_info[resist].res_level[0] = 1;
            obj->el_info[resist].flags |= (EL_INFO_RANDOM | EL_INFO_IGNORE);
        }
    }
    if (kf_has(kind_flags, KF_RAND_HI_RES))
    {
        /* Get a high resist if available, mark it as random */
        if (random_high_resist(obj, &resist))
        {
            obj->el_info[resist].res_level[0] = 1;
            obj->el_info[resist].flags |= (EL_INFO_RANDOM | EL_INFO_IGNORE);
        }
    }
}


static int get_power_flags(const struct object *obj, bitflag flags[OF_SIZE])
{
    bitflag *kind_flags;

    of_wipe(flags);

    /* Set ego extra powers */
    if (obj->ego) kind_flags = obj->ego->kind_flags;

    /* Set extra powers */
    else kind_flags = obj->kind->kind_flags;

    /* Get power flags */
    if (kf_has(kind_flags, KF_RAND_SUSTAIN))
        create_obj_flag_mask(flags, 0, OFT_SUST, OFT_MAX);
    if (kf_has(kind_flags, KF_RAND_POWER) || kf_has(kind_flags, KF_RAND_RES_POWER))
        create_obj_flag_mask(flags, 0, OFT_PROT, OFT_MISC, OFT_ESP, OFT_MAX);
    if (kf_has(kind_flags, KF_RAND_ESP))
        create_obj_flag_mask(flags, 0, OFT_ESP, OFT_MAX);

    /* Get resists */
    if (kf_has(kind_flags, KF_RAND_BASE_RES) || kf_has(kind_flags, KF_RAND_RES_POWER))
        return ELEM_ACID;
    if (kf_has(kind_flags, KF_RAND_HI_RES))
        return ELEM_POIS;

    return -1;
}


void init_powers(const struct object *obj, int *power, int *resist)
{
    bitflag flags[OF_SIZE];
    bitflag *kind_flags = (obj->ego? obj->ego->kind_flags: obj->kind->kind_flags);

    *resist = get_power_flags(obj, flags);
    *power = of_next(flags, FLAG_START);

    /* Set extra power only for RAND_RES_POWER items */
    if (kf_has(kind_flags, KF_RAND_RES_POWER)) *resist = -1;
}


void dec_power(const struct object *obj, int *power)
{
    bitflag flags[OF_SIZE];
    int flag, prevflag;

    get_power_flags(obj, flags);
    flag = of_next(flags, FLAG_START);

    /* Item must have an extra power */
    if (*power == FLAG_END) return;

    /* Min bound */
    if (*power == flag) return;

    /* Decrease extra power */
    do
    {
        prevflag = flag;
        flag = of_next(flags, prevflag + 1);
    }
    while (*power != flag);
    *power = prevflag;
}


void dec_resist(const struct object *obj, int *resist)
{
    /* Item must have an extra resist */
    if (*resist == -1) return;

    /* Min bound */
    if (*resist == ELEM_ACID) return;
    if (*resist == ELEM_POIS) return;

    /* Decrease extra resist */
    (*resist)--;
}


void inc_power(const struct object *obj, int *power, int *resist)
{
    bitflag flags[OF_SIZE];
    int flag;

    get_power_flags(obj, flags);

    /* Item must have an extra power */
    if (*power == FLAG_END)
    {
        bitflag *kind_flags = (obj->ego? obj->ego->kind_flags: obj->kind->kind_flags);

        /* Set extra power only for RAND_RES_POWER items */
        if (kf_has(kind_flags, KF_RAND_RES_POWER))
        {
            *power = of_next(flags, FLAG_START);
            *resist = -1;
        }

        return;
    }

    /* Max bound */
    flag = of_next(flags, *power + 1);
    if (flag == FLAG_END) return;

    /* Increase extra power */
    *power = flag;
}


void inc_resist(const struct object *obj, int *power, int *resist)
{
    /* Item must have an extra resist */
    if (*resist == -1)
    {
        bitflag *kind_flags = (obj->ego? obj->ego->kind_flags: obj->kind->kind_flags);

        /* Set extra resist only for RAND_RES_POWER items */
        if (kf_has(kind_flags, KF_RAND_RES_POWER))
        {
            *power = FLAG_END;
            *resist = ELEM_ACID;
        }

        return;
    }

    /* Max bound */
    if (*resist == ELEM_COLD) return;
    if (*resist == ELEM_DISEN) return;

    /* Increase extra resist */
    (*resist)++;
}


void do_fixed_powers(struct object *obj, int power, int resist)
{
    if (power != FLAG_END) of_on(obj->flags, power);
    if (resist != -1) obj->el_info[resist].res_level[0] = 1;
}


void undo_fixed_powers(struct object *obj, int power, int resist)
{
    if (power != FLAG_END) of_off(obj->flags, power);
    if (resist != -1) obj->el_info[resist].res_level[0] = 0;
}


static const char *get_power_desc(int power)
{
    int i;

    for (i = 1; i < OF_MAX; i++)
    {
        struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_FLAG, i);

        if ((prop->subtype != OFT_SUST) && (prop->subtype != OFT_PROT) &&
            (prop->subtype != OFT_MISC) && (prop->subtype != OFT_ESP))
        {
            continue;
        }
        if (prop->index == power) return prop->short_desc;
    }

    return "";
}


static const char *get_resist_desc(int resist)
{
    int i;

    for (i = 0; i < ELEM_MAX; i++)
    {
        struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_RESIST, i);

        if (prop->index == resist) return prop->short_desc;
    }

    return "";
}


void get_power_descs(int power, int resist, char *buf, int len)
{
    if (power != FLAG_END)
    {
        my_strcpy(buf, get_power_desc(power), len);
        if (resist != -1)
        {
            my_strcat(buf, "/", len);
            my_strcat(buf, get_resist_desc(resist), len);
        }
    }
    else if (resist != -1)
        my_strcpy(buf, get_resist_desc(resist), len);
    else
        my_strcpy(buf, "Regular", len);
}


/*
 * Select an ego-item that fits the object's tval and sval.
 */
static struct ego_item *ego_find_random(struct object *obj, int level)
{
    int i;
    long total = 0L;
    alloc_entry *table = alloc_ego_table;

    /* Go through all possible ego items and find ones which fit this item */
    for (i = 0; i < alloc_ego_size; i++)
    {
        struct ego_item *ego = &e_info[table[i].index];

        /* Reset any previous probability of this type being picked */
        table[i].prob3 = 0;

        if (level <= ego->alloc_max)
        {
            int ood_chance = MAX(2, (ego->alloc_min - level) / 3);

            if ((level >= ego->alloc_min) || one_in_(ood_chance))
            {
                struct poss_item *poss;

                for (poss = ego->poss_items; poss; poss = poss->next)
                {
                    if (poss->kidx == obj->kind->kidx)
                    {
                        table[i].prob3 = table[i].prob2;
                        break;
                    }
                }

                /* Total */
                total += table[i].prob3;
            }
        }
    }

    if (total)
    {
        long value = randint0(total);

        for (i = 0; i < alloc_ego_size; i++)
        {
            /* Found the entry */
            if (value < table[i].prob3) return &e_info[table[i].index];

            /* Decrement */
            value = value - table[i].prob3;
        }
    }

    return NULL;
}


/*
 * Apply minimum standards for ego-items.
 */
static void ego_apply_minima(struct object *obj)
{
    int i;

    if (!obj->ego) return;

    if ((obj->ego->min_to_h != NO_MINIMUM) && (obj->to_h < obj->ego->min_to_h))
        obj->to_h = obj->ego->min_to_h;
    if ((obj->ego->min_to_d != NO_MINIMUM) && (obj->to_d < obj->ego->min_to_d))
        obj->to_d = obj->ego->min_to_d;
    if ((obj->ego->min_to_a != NO_MINIMUM) && (obj->to_a < obj->ego->min_to_a))
        obj->to_a = obj->ego->min_to_a;

    for (i = 0; i < OBJ_MOD_MAX; i++)
    {
        if (obj->modifiers[i] && (obj->ego->min_modifiers[i] != NO_MINIMUM) &&
            (obj->modifiers[i] < obj->ego->min_modifiers[i]))
        {
            obj->modifiers[i] = obj->ego->min_modifiers[i];
        }
    }
}


/*
 * Apply generation magic to an ego-item.
 *
 * Returns the amount to increase the level rating by
 */
void ego_apply_magic(struct object *obj, int level)
{
    int i, x;

    /* Apply extra ego bonuses (except on dark swords) */
    if (!tval_is_dark_sword(obj))
    {
        obj->to_h += randcalc(obj->ego->to_h, level, RANDOMISE);
        obj->to_d += randcalc(obj->ego->to_d, level, RANDOMISE);
        obj->to_a += randcalc(obj->ego->to_a, level, RANDOMISE);
    }

    /* Apply modifiers */
    for (i = 0; i < OBJ_MOD_MAX; i++)
    {
        x = randcalc(obj->ego->modifiers[i], level, RANDOMISE);
        obj->modifiers[i] += x;
    }

    /* Apply flags */
    of_union(obj->flags, obj->ego->flags);

    /* Add slays, brands and curses */
    copy_slays(&obj->slays, obj->ego->slays);
    copy_brands(&obj->brands, obj->ego->brands);
    copy_curses(obj, obj->ego->curses);

    /* Add resists */
    for (i = 0; i < ELEM_MAX; i++)
    {
        /* Take the larger of ego and base object resist levels */
        obj->el_info[i].res_level[0] =
            MAX(obj->ego->el_info[i].res_level[0], obj->el_info[i].res_level[0]);

        /* Union of flags so as to know when ignoring is notable */
        obj->el_info[i].flags |= obj->ego->el_info[i].flags;
    }

    /* Apply minima */
    ego_apply_minima(obj);

    /* Add activation (ego activation will trump object activation, when there are any) */
    if (obj->ego->activation)
    {
        obj->activation = obj->ego->activation;
        memcpy(&obj->time, &obj->ego->time, sizeof(random_value));
    }
}


/*
 * Try to find an ego-item for an object, setting obj->ego if successful and
 * applying various bonuses.
 */
static void make_ego_item(struct object *obj, int level)
{
    /* Cannot further improve artifacts or ego items */
    if (obj->artifact || obj->ego) return;

    /* Occasionally boost the generation level of an item */
    if ((level > 0) && one_in_(z_info->great_ego))
    {
        /* The bizarre calculation again */
        level = 1 + (level * z_info->max_depth / randint1(z_info->max_depth));

        /* Ensure valid allocation level */
        if (level >= z_info->max_depth) level = z_info->max_depth - 1;
    }

    /* Try to get a legal ego type for this item */
    obj->ego = ego_find_random(obj, level);

    /* Actually apply the ego template to the item */
    if (obj->ego)
    {
        /* Extra powers */
        do_powers(obj, obj->ego->kind_flags);

        ego_apply_magic(obj, level);
    }
}


/*** Make an artifact ***/


/*
 * Copy artifact data to a normal object.
 */
void copy_artifact_data(struct object *obj, const struct artifact *art)
{
    int i;
    struct object_kind *kind = lookup_kind(art->tval, art->sval);

    /* Extract the data */
    for (i = 0; i < OBJ_MOD_MAX; i++)
        obj->modifiers[i] = art->modifiers[i];
    obj->ac = art->ac;
    obj->dd = art->dd;
    obj->ds = art->ds;
    obj->to_a = art->to_a;
    obj->to_h = art->to_h;
    obj->to_d = art->to_d;
    obj->weight = art->weight;

    /* Activations can come from the artifact or the kind */
    /* PWMAngband: don't add activation if NO_ACTIVATION is defined on the artifact */
    if (art->activation)
    {
        obj->activation = art->activation;
        memcpy(&obj->time, &art->time, sizeof(random_value));
    }
    // When the template artifact has no activation,
    // inherit it from the base kind with 50% chance (was 100% before)
    else if ((kind->activation && one_in_(2)) && !of_has(art->flags, OF_NO_ACTIVATION))
    {
        obj->activation = kind->activation;
        memcpy(&obj->time, &kind->time, sizeof(random_value));
    }
    else
    {
        obj->activation = NULL;
        memset(&obj->time, 0, sizeof(random_value));
    }

    of_union(obj->flags, art->flags);
    copy_slays(&obj->slays, art->slays);
    copy_brands(&obj->brands, art->brands);
    copy_curses(obj, art->curses);
    for (i = 0; i < ELEM_MAX; i++)
    {
        /* Use any non-zero artifact resist level */
        if (art->el_info[i].res_level[0] != 0)
            obj->el_info[i].res_level[0] = art->el_info[i].res_level[0];

        /* Union of flags so as to know when ignoring is notable */
        obj->el_info[i].flags |= art->el_info[i].flags;
    }
}


/*
 * Create a fake artifact directly from a blank object
 *
 * This function is used for describing artifacts, and for creating them for
 * debugging.
 *
 * Since this is now in no way marked as fake, we must make sure this function
 * is never used to create an actual game object
 */
bool make_fake_artifact(struct object **obj_address, const struct artifact *artifact)
{
    struct object_kind *kind;

    /* Don't bother with empty artifacts */
    if (!artifact->tval) return false;

    /* Get the "kind" index */
    kind = lookup_kind(artifact->tval, artifact->sval);
    if (!kind) return false;

    /* Create the artifact */
    *obj_address = object_new();
    object_prep(NULL, NULL, *obj_address, kind, 0, MAXIMISE);
    (*obj_address)->artifact = &a_info[artifact->aidx];
    copy_artifact_data(*obj_address, artifact);

    /* Identify object to get real value */
    object_notice_everything_aux(NULL, *obj_address, true, false);

    /* Success */
    return true;
}


static bool artifact_pass_checks(const struct artifact *art, int depth)
{
    /* Enforce minimum "depth" (loosely) */
    if (art->alloc_min > depth)
    {
        /* Get the "out-of-depth factor" */
        int d = (art->alloc_min - depth) * 2;

        /* Roll for out-of-depth creation */
        if (randint0(d)) return false;
    }

    /* Enforce maximum depth (strictly) */
    if (art->alloc_max < depth) return false;

    /* We must make the "rarity roll" */
    if (!magik(art->alloc_prob)) return false;

    return true;
}


/*
 * Attempt to create one of the "Special Objects"
 *
 * We are only called from "make_object()"
 *
 * Note -- see "make_artifact()" and "apply_magic()"
 */
static struct object *make_artifact_special(struct player *p, struct chunk *c, int level, int tval)
{
    int i;
    struct object *new_obj = NULL;

    /* No artifacts, do nothing */
    if (p && (cfg_no_artifacts || OPT(p, birth_no_artifacts))) return NULL;

    /* No artifacts in the towns or on special levels */
    if (forbid_special(&c->wpos)) return NULL;

    /* Check the special artifacts */
    /* PWMAngband: winners don't generate true artifacts */
    if (!(p && p->total_winner))
    {
        for (i = 0; i < z_info->a_max; i++)
        {
            const struct artifact *art = &a_info[i];
            struct object_kind *kind = lookup_kind(art->tval, art->sval);
            char o_name[NORMAL_WID];

            /* Skip "empty" artifacts */
            if (!art->name) continue;

            /* Make sure the kind was found */
            if (!kind) continue;

            /* Skip non-special artifacts */
            if (!kf_has(kind->kind_flags, KF_INSTA_ART)) continue;

            /* Cannot make an artifact twice */
            if (is_artifact_created(art)) continue;

            /* Cannot generate an artifact if disallowed by preservation mode  */
            if (p && (p->art_info[i] > cfg_preserve_artifacts)) continue;

            /* Must have the correct fields */
            if (tval && (art->tval != tval)) continue;

            /* We must pass depth and rarity checks */
            if (!artifact_pass_checks(art, c->wpos.depth)) continue;

            /* Assign the template */
            new_obj = object_new();
            object_prep(p, c, new_obj, kind, art->alloc_min, RANDOMISE);

            /* Mark the item as an artifact */
            new_obj->artifact = art;
            object_desc(NULL, o_name, sizeof(o_name), new_obj, ODESC_PREFIX | ODESC_BASE);
            plog_fmt("Special artifact %s created", o_name);

            /* Copy across all the data from the artifact struct */
            copy_artifact_data(new_obj, art);

            /* Mark the artifact as "created" */
            mark_artifact_created(art, true);
            if (p)
            {
                /* Mark the artifact as "generated" if dungeon is ready */
                if (!ht_zero(&c->generated))
                    set_artifact_info(p, new_obj, ARTS_GENERATED);

                /* Otherwise, preserve artifacts from dungeon generation errors */
                else p->art_info[new_obj->artifact->aidx] += ARTS_CREATED;
            }

            if (object_has_standard_to_h(new_obj)) new_obj->known->to_h = 1;
            if (object_flavor_is_aware(p, new_obj)) object_id_set_aware(new_obj);

            /* Success */
            return new_obj;
        }
    }

    /* An extra chance at being a randart */
    if (cfg_random_artifacts && p)
    {
        for (i = 0; i < z_info->a_max; i++)
        {
            const struct artifact *art = &a_info[i];
            struct object_kind *kind = lookup_kind(art->tval, art->sval);

            /* Skip "empty" artifacts */
            if (!art->name) continue;

            /* Make sure the kind was found */
            if (!kind) continue;

            /* Skip non-special artifacts */
            if (!kf_has(kind->kind_flags, KF_INSTA_ART)) continue;

            /* Must have the correct fields */
            if (tval && (art->tval != tval)) continue;

            /* Attempt to change the object into a random artifact */
            if (!create_randart_drop(p, c, &new_obj, i, true)) continue;

            if (object_has_standard_to_h(new_obj)) new_obj->known->to_h = 1;
            if (object_flavor_is_aware(p, new_obj)) object_id_set_aware(new_obj);

            /* Success */
            return new_obj;
        }
    }

    /* Failure */
    return NULL;
}


bool create_randart_drop(struct player *p, struct chunk *c, struct object **obj_address, int a_idx,
    bool check)
{
    int32_t randart_seed;
    struct artifact *art;

    /* Cannot make a randart twice */
    if (p->randart_created[a_idx]) return false;

    /* Cannot generate a randart if disallowed by preservation mode */
    if (p->randart_info[a_idx] > cfg_preserve_artifacts) return false;

    /* Piece together a 32-bit random seed */
    randart_seed = randint0(0xFFFF) << 16;
    randart_seed += randint0(0xFFFF);

    /* Attempt to change the object into a random artifact */
    art = do_randart(p, randart_seed, &a_info[a_idx]);

    /* Skip "empty" items */
    if (!art) return false;

    /* We must pass depth and rarity checks */
    if (check && !artifact_pass_checks(art, c->wpos.depth))
    {
        free_artifact(art);
        return false;
    }

    /* Assign the template */
    if (*obj_address == NULL)
    {
        *obj_address = object_new();
        object_prep(p, c, *obj_address, lookup_kind(art->tval, art->sval), art->alloc_min, RANDOMISE);
    }

    /* Mark the item as a random artifact */
    make_randart(p, c, *obj_address, art, randart_seed);

    /* Success */
    free_artifact(art);
    return true;
}


static bool create_randart_aux(struct player *p, struct chunk *c, struct object *obj,
    bool check)
{
    int i;

    for (i = 0; i < z_info->a_max; i++)
    {
        const struct artifact *art = &a_info[i];
        struct object_kind *kind = lookup_kind(art->tval, art->sval);

        /* Skip "empty" items */
        if (!art->name) continue;

        /* Make sure the kind was found */
        if (!kind) continue;

        /* Skip special artifacts */
        if (kf_has(kind->kind_flags, KF_INSTA_ART) || kf_has(kind->kind_flags, KF_EPIC)) continue;

        /* Must have the correct fields */
        if (art->tval != obj->tval) continue;
        if (art->sval != obj->sval) continue;

        /* Attempt to change the object into a random artifact */
        if (!create_randart_drop(p, c, &obj, i, check)) continue;

        /* Success */
        return true;
    }

    /* Failure */
    return false;
}


/*
 * Attempt to change an object into an artifact.  If the object is already
 * set to be an artifact, use that, or otherwise use a suitable randomly-
 * selected artifact.
 *
 * This routine should only be called by "apply_magic()"
 *
 * Note -- see "make_artifact_special()" and "apply_magic()"
 */
static bool make_artifact(struct player *p, struct chunk *c, struct object *obj)
{
    int i;

    /* Make sure birth no artifacts isn't set */
    if (p && (cfg_no_artifacts || OPT(p, birth_no_artifacts))) return false;

    /* No artifacts in the towns or on special levels */
    if (forbid_special(&c->wpos)) return false;

    /* Paranoia -- no "plural" artifacts */
    if (obj->number != 1) return false;

    /* Check the artifact list (skip the "specials") */
    /* PWMAngband: winners don't generate true artifacts */
    if (!restrict_winner(p, obj))
    {
        for (i = 0; !obj->artifact && (i < z_info->a_max); i++)
        {
            const struct artifact *art = &a_info[i];
            struct object_kind *kind = lookup_kind(art->tval, art->sval);
            char o_name[NORMAL_WID];

            /* Skip "empty" items */
            if (!art->name) continue;

            /* Make sure the kind was found */
            if (!kind) continue;

            /* Skip special artifacts */
            if (kf_has(kind->kind_flags, KF_INSTA_ART) || kf_has(kind->kind_flags, KF_EPIC)) continue;

            /* Cannot make an artifact twice */
            if (is_artifact_created(art)) continue;

            /* Cannot generate an artifact if disallowed by preservation mode  */
            if (p && (p->art_info[i] > cfg_preserve_artifacts)) continue;

            /* Must have the correct fields */
            if (art->tval != obj->tval) continue;
            if (art->sval != obj->sval) continue;

            /* We must pass depth and rarity checks */
            if (!artifact_pass_checks(art, c->wpos.depth)) continue;

            /* Mark the item as an artifact */
            obj->artifact = art;
            object_desc(NULL, o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_BASE);
            plog_fmt("Artifact %s created", o_name);
        }

        if (obj->artifact)
        {
            /* Copy across all the data from the artifact struct */
            copy_artifact_data(obj, obj->artifact);

            /* Mark the artifact as "created" */
            mark_artifact_created(obj->artifact, true);
            if (p)
            {
                /* Mark the artifact as "generated" if dungeon is ready */
                if (!ht_zero(&c->generated))
                    set_artifact_info(p, obj, ARTS_GENERATED);

                /* Otherwise, preserve artifacts from dungeon generation errors */
                else p->art_info[obj->artifact->aidx] += ARTS_CREATED;
            }

            /* Success */
            return true;
        }
    }

    /* An extra chance at being a randart */
    if (cfg_random_artifacts && p)
    {
        if (create_randart_aux(p, c, obj, true))
        {
            /* Success */
            return true;
        }
    }

    /* Failure */
    return false;
}


/*** Apply magic to an item ***/


/*
 * Apply magic to a weapon
 */
static void apply_magic_weapon(struct object *obj, int level, int power)
{
    if (power <= 0) return;

    obj->to_h += randint1(5) + m_bonus(5, level);
    obj->to_d += randint1(5) + m_bonus(5, level);

    if (power > 1)
    {
        obj->to_h += m_bonus(10, level);
        obj->to_d += m_bonus(10, level);

        if (tval_is_melee_weapon(obj))
        {
            /* Super-charge the damage dice */
            while (one_in_(4 * obj->dd * obj->ds))
            {
                /* More dice or sides means more likely to get still more */
                if (randint0(obj->dd + obj->ds) < obj->dd)
                {
                    int newdice = randint1(2 + obj->dd / obj->ds);

                    while (((obj->dd + 1) * obj->ds <= 40) && newdice)
                    {
                        if (!one_in_(3)) obj->dd++;
                        newdice--;
                    }
                }
                else
                {
                    int newsides = randint1(2 + obj->ds / obj->dd);

                    while ((obj->dd * (obj->ds + 1) <= 40) && newsides)
                    {
                        if (!one_in_(3)) obj->ds++;
                        newsides--;
                    }
                }
            }
        }
        else if (tval_is_ammo(obj))
        {
            /* Up to two chances to enhance damage dice. */
            if (one_in_(6) == 1)
            {
                obj->ds++;
                if (one_in_(10) == 1) obj->ds++;
            }
        }
    }
}


/*
 * Apply magic to armour
 */
static void apply_magic_armour(struct object *obj, int level, int power)
{
    if (power <= 0) return;

    obj->to_a += randint1(5) + m_bonus(5, level);
    if (power > 1) obj->to_a += m_bonus(10, level);

    /* Bad */
    if (obj->to_a < 0)
    {
        size_t i;

        /* Reverse base bonuses */
        for (i = 0; i < OBJ_MOD_MAX; i++)
        {
            if (obj->modifiers[i] > 0) obj->modifiers[i] = 0 - obj->modifiers[i];
        }
    }
}


/*
 * Wipe an object clean and make it a standard object of the specified kind.
 */
void object_prep(struct player *p, struct chunk *c, struct object *obj, struct object_kind *k,
    int lev, aspect rand_aspect)
{
    int i;

    /* Clean slate */
    object_wipe(obj);

    /* Assign the kind and copy across data */
    obj->kind = k;
    obj->tval = k->tval;
    obj->sval = k->sval;
    obj->ac = k->ac;
    obj->dd = k->dd;
    obj->ds = k->ds;
    obj->weight = k->weight;

    obj->effect = k->effect;
    obj->activation = k->activation;
    memcpy(&obj->time, &k->time, sizeof(random_value));

    /* Default number */
    obj->number = 1;

    /* Copy flags */
    if (k->base) of_copy(obj->flags, k->base->flags);
    of_union(obj->flags, k->flags);

    /* Assign modifiers */
    for (i = 0; i < OBJ_MOD_MAX; i++)
        obj->modifiers[i] = randcalc(k->modifiers[i], lev, rand_aspect);

    /* Amulets of speed can't give very much, and are rarely +5 */
    if (tval_is_amulet(obj) && (obj->sval == lookup_sval(obj->tval, "Speed")))
        obj->modifiers[OBJ_MOD_SPEED] = randint1(obj->modifiers[OBJ_MOD_SPEED]);

    /* Rings of polymorphing get a random race */
    if (tval_is_poly(obj))
    {
        /* Pick a race based on current depth */
        bool in_dungeon = (p && c && (c->wpos.depth > 0));
        struct monster_race *race = (in_dungeon? get_mon_num_poly(&c->wpos): NULL);

        /* Handle failure smartly: create a useless ring of <player> */
        if (!race) race = &r_info[0];

        obj->modifiers[OBJ_MOD_POLY_RACE] = race->ridx;
    }

    /* Assign charges (wands/staves only) */
    if (tval_can_have_charges(obj))
        obj->pval = randcalc(k->charge, lev, rand_aspect);

    /* Assign flagless pval for food or oil */
    if (tval_can_have_nourishment(obj) || tval_is_fuel(obj) || tval_is_launcher(obj))
        obj->pval = randcalc(k->pval, lev, rand_aspect);

    /* Default fuel */
    if (tval_is_light(obj)) fuel_default(obj);

    /* Default magic */
    obj->to_h = randcalc(k->to_h, lev, rand_aspect);
    obj->to_d = randcalc(k->to_d, lev, rand_aspect);
    obj->to_a = randcalc(k->to_a, lev, rand_aspect);

    /* Default slays, brands and curses */
    copy_slays(&obj->slays, k->slays);
    copy_brands(&obj->brands, k->brands);
    copy_curses(obj, k->curses);

    /* Default resists */
    for (i = 0; i < ELEM_MAX; i++)
    {
        obj->el_info[i].res_level[0] = k->el_info[i].res_level[0];
        obj->el_info[i].flags = k->el_info[i].flags;
        if (k->base) obj->el_info[i].flags |= k->base->el_info[i].flags;
    }

    object_set_base_known(p, obj);
}


/*
 * Applying magic to an object, which includes creating ego-items, and applying
 * random bonuses.
 *
 * The `good` argument forces the item to be at least `good`, and the `great`
 * argument does likewise.  Setting `allow_artifacts` to true allows artifacts
 * to be created here.
 *
 * If `good` or `great` are not set, then the `lev` argument controls the
 * quality of item.
 *
 * Returns 0 if a normal object, 1 if a good object, 2 if an ego item, 3 if an
 * artifact.
 *
 * PWMAngband: returns -1 if invalid (not a "good" drop)
 */
int apply_magic(struct player *p, struct chunk *c, struct object *obj, int lev,
    bool allow_artifacts, bool good, bool great, bool extra_roll)
{
    int i;
    int16_t power = 0;

    /* Chance of being `good` and `great` */
    int good_chance = MIN(z_info->good_obj + lev, 100);
    int great_chance = z_info->ego_obj;

    /* Normal magic ammo are always +0 +0 (not a "good" drop) */
    if (tval_is_ammo(obj) && of_has(obj->flags, OF_AMMO_MAGIC)) return ((good || great)? -1: 0);

    /* Roll for "good" */
    if (good || magik(good_chance))
    {
        /* Assume "good" */
        power = 1;

        /* Roll for "great" */
        if (great || magik(great_chance)) power = 2;
    }

    /* Roll for artifact creation */
    if (allow_artifacts)
    {
        int rolls = 0;

        /* Get one roll if excellent */
        if (power >= 2) rolls = 1;

        /* Get two rolls if forced great */
        if (great) rolls = 2;

        /* Give some extra rolls for uniques and acq scrolls */
        if (extra_roll) rolls += 2;

        /* Roll for artifacts if allowed */
        for (i = 0; i < rolls; i++)
        {
            if (make_artifact(p, c, obj)) return 3;
        }
    }

    /* Try to make an ego item */
    if (power == 2) make_ego_item(obj, lev);

    /* Give it a chance to be cursed */
    if (one_in_(20) && tval_is_wearable(obj))
        lev = apply_curse(obj, lev, obj->tval);
    if (lev >= z_info->max_depth) lev = z_info->max_depth - 1;

    /* Apply magic */
    if (tval_is_tool(obj) || tval_is_mstaff(obj) || tval_is_dark_sword(obj))
    {
        /* Not a "great" drop */
        if (great && !obj->ego) return -1;

        if (tval_is_dark_sword(obj))
        {
            apply_magic_weapon(obj, lev, power);
            obj->to_h = 0;
            obj->to_d = 0;
        }

        /* Not a "good" drop */
        if (good && !obj->ego) return -1;
    }
    else if (tval_is_enchantable_weapon(obj))
    {
        /* Not a "great" drop */
        if (great && !obj->ego) return -1;

        apply_magic_weapon(obj, lev, power);

        /* Not a "good" drop */
        if (good && !obj->ego && ((obj->to_h <= 0) || (obj->to_d <= 0))) return -1;
    }
    else if (tval_is_armor(obj))
    {
        /* Not a "great" drop */
        if (great && !obj->ego) return -1;

        apply_magic_armour(obj, lev, power);

        /* Not a "good" drop */
        if (good && !obj->ego && (obj->to_a <= 0)) return -1;
    }
    else if (tval_is_ring(obj))
    {
        if (obj->sval == lookup_sval(obj->tval, "Speed"))
        {
            /* Super-charge the ring */
            while (one_in_(2)) obj->modifiers[OBJ_MOD_SPEED]++;
        }
    }
    else if (tval_is_amulet(obj))
    {
        /* Extra powers */
        /* TODO: should be global to all objects */
        do_powers(obj, obj->kind->kind_flags);
    }
    else if (tval_is_light(obj))
    {
        /* Not a "great" drop */
        if (great && !obj->ego) return -1;

        /* Not a "good" drop */
        if (good && !obj->ego) return -1;
    }
    else if (tval_is_chest(obj))
    {
        /* Get a random, level-dependent set of chest traps */
        obj->pval = pick_chest_traps(obj);
    }

    return power;
}


/*** Generate a random object ***/


/*
 * Determine if a template is "good"
 */
bool kind_is_good(const struct object_kind *kind)
{
    /* Some item types are (almost) always good */
    switch (kind->tval)
    {
        /* Armor */
        case TV_HARD_ARMOR:
        case TV_SOFT_ARMOR:
        case TV_SHIELD:
        case TV_CLOAK:
        case TV_BOOTS:
        case TV_GLOVES:
        case TV_HELM:
        case TV_CROWN: return true;

        /* Weapons */
        case TV_BOW:
        case TV_SWORD:
        case TV_HAFTED:
        case TV_POLEARM:
        case TV_MSTAFF: return true;

        /* Tools */
        case TV_DIGGING:
        case TV_BELT:
        case TV_HORN: return true;

        /* Ammo */
        case TV_ROCK:
        case TV_BOLT:
        case TV_ARROW:
        case TV_SHOT: return true;

        /* Light sources */
        case TV_LIGHT: return true;
    }

    /* Other item types */
    return kind_is_good_other(kind);
}


/*
 * Choose an object kind of a given tval given a dungeon level.
 */
static struct object_kind *get_obj_num_by_kind(int level, bool good, int tval)
{
    size_t ind, item;
    uint32_t value;
    int total = 0;
    uint8_t *objects = (good? obj_alloc_great: obj_alloc);

    /* This is the base index into obj_alloc for this dlev */
    ind = level * z_info->k_max;

    /* Get new total */
    for (item = 0; item < (size_t)z_info->k_max; item++)
    {
        if (k_info[item].tval == tval) total += objects[ind + item];
    }

    /* No appropriate items of that tval */
    if (!total) return NULL;

    value = randint0(total);

    /* Pick an object */
    for (item = 0; item < (size_t)z_info->k_max; item++)
    {
        if (k_info[item].tval == tval)
        {
            if (value < (uint32_t)objects[ind + item]) break;
            value -= objects[ind + item];
        }
    }

    /* Return the item index */
    return &k_info[item];
}


/*
 * Choose an object kind given a dungeon level to choose it for.
 * If tval = 0, we can choose an object of any type.
 * Otherwise we can only choose one of the given tval.
 */
struct object_kind *get_obj_num(int level, bool good, int tval)
{
    size_t ind, item;
    uint32_t value;

    /* Occasional level boost */
    if ((level > 0) && one_in_(z_info->great_obj))
    {
        /* What a bizarre calculation */
        level = 1 + (level * z_info->max_obj_depth / randint1(z_info->max_obj_depth));
    }

    /* Paranoia */
    level = MIN(level, z_info->max_obj_depth);
    level = MAX(level, 0);

    if (tval) return get_obj_num_by_kind(level, good, tval);

    /* This is the base index into obj_alloc for this dlev */
    ind = level * z_info->k_max;

    /* Pick an object */
    if (!good)
    {
        value = randint0(obj_total[level]);
        for (item = 0; item < (size_t)z_info->k_max; item++)
        {
            /* Found it */
            if (value < (uint32_t)obj_alloc[ind + item]) break;

            /* Decrement */
            value -= obj_alloc[ind + item];
        }
    }
    else
    {
        value = randint0(obj_total_great[level]);
        for (item = 0; item < (size_t)z_info->k_max; item++)
        {
            /* Found it */
            if (value < (uint32_t)obj_alloc_great[ind + item]) break;

            /* Decrement */
            value -= obj_alloc_great[ind + item];
        }
    }

    /* Paranoia */
    if (item == (size_t)z_info->k_max) return NULL;

    /* Return the item index */
    return &k_info[item];
}


/*
 * Create a fake (identified) object from a real object.
 *
 * This function should only be used to get the real value of an unidentified object.
 */
static struct object *make_fake_object(const struct object *obj)
{
    /* Create the object */
    struct object *fake = object_new();

    /* Get a copy of the object */
    object_copy(fake, obj);

    /* Identify object to get real value */
    object_notice_everything_aux(NULL, fake, true, false);

    /* Success */
    return fake;
}


/*
 * Attempt to make an object
 *
 * c is the current dungeon level
 * lev is the creation level of the object (not necessarily == depth)
 * good is whether the object is to be good
 * great is whether the object is to be great
 * extra_roll is whether we get an extra roll in apply_magic()
 * value is the value to be returned to the calling function
 * tval is the desired tval, or 0 if we allow any tval
 *
 * Returns a pointer to the newly allocated object, or NULL on failure.
 */
static struct object *make_object_aux(struct player *p, struct chunk *c, int lev, bool good,
    bool great, bool extra_roll, int32_t *value, int tval)
{
    int base;
    struct object_kind *kind = NULL;
    struct object *new_obj;
    int i;
    int tries = 1;
    bool ok = false;
    int olvl;

    /* Try to make a special artifact */
    if (one_in_(good? 10: 1000))
    {
        new_obj = make_artifact_special(p, c, lev, tval);
        if (new_obj)
        {
            if (value)
            {
                /* PWMAngband: we use a fake (identified) object to get the real value */
                struct object *fake = make_fake_object(new_obj);

                *value = (int32_t)object_value_real(p, fake, 1);
                object_delete(&fake);
            }
            return new_obj;
        }

        /* If we failed to make an artifact, the player gets a good item */
        good = true;
    }

    /* Base level for the object */
    base = (good? (lev + 10): lev);

    /* Try harder to generate a "good" object */
    if (good || great) tries = 3;

    for (i = 1; i <= tries; i++)
    {
        int16_t res;
        int reroll = 3;
        bool in_dungeon = (p && c && (c->wpos.depth > 0));

        /* Try to choose an object kind */
        kind = get_obj_num(base, good || great, tval);
        if (!kind) return NULL;

        /* Reject most books the player can't read */
        while (tval_is_book_k(kind) && !obj_kind_can_browse(p, kind) && reroll)
        {
            if (one_in_(5)) break;
            reroll--;
            kind = get_obj_num(base, good || great, tval);
            if (!kind) return NULL;
        }

        /* No rings of polymorphing outside the dungeon */
        if (tval_is_poly_k(kind) && !in_dungeon) continue;

        /* Make the object, prep it and apply magic */
        new_obj = object_new();
        object_prep(p, c, new_obj, kind, lev, RANDOMISE);
        res = apply_magic(p, c, new_obj, lev, true, good, great, extra_roll);

        /* Reroll "good" objects of incorrect kind (diggers, light sources...) */
        /* Reroll "great" objects when make_ego_item fails */
        if (res == -1)
        {
            /* Handle failure */
            object_delete(&new_obj);
            continue;
        }

        if (object_has_standard_to_h(new_obj)) new_obj->known->to_h = 1;
        if (object_flavor_is_aware(p, new_obj)) object_id_set_aware(new_obj);

        /* We have a valid object */
        ok = true;
        break;
    }

    /* Handle failure */
    if (!ok) return false;

    /* Generate multiple items */
    if (!new_obj->artifact)
    {
        if ((new_obj->kind->gen_mult_prob >= 100) || (new_obj->kind->gen_mult_prob >= randint1(100)))
            new_obj->number = randcalc(new_obj->kind->stack_size, lev, RANDOMISE);
    }

    if (new_obj->number > new_obj->kind->base->max_stack)
        new_obj->number = new_obj->kind->base->max_stack;

    /* Get the value */
    if (value)
    {
        /* PWMAngband: we use a fake (identified) object to get the real value */
        struct object *fake = make_fake_object(new_obj);

        *value = (int32_t)object_value_real(p, fake, fake->number);
        object_delete(&fake);
    }

    /* Boost of 20% per level OOD for uncursed objects */
    olvl = object_level(&c->wpos);
    if (!new_obj->curses && (new_obj->kind->alloc_min > olvl) && value)
    {
        int32_t ood = new_obj->kind->alloc_min - olvl;
        int32_t frac = MAX(*value, 0) / 5;
        int32_t adj;

        if (frac <= INT32_MAX / ood) adj = ood * frac;
        else adj = INT32_MAX;

        if (*value <= INT32_MAX - adj) *value += adj;
        else *value = INT32_MAX;
    }

    // Apply speed limit based on player level
    // (note: we have separate value for randart. Search: "sp33d")
    if (new_obj->modifiers[OBJ_MOD_SPEED] > 0)
    {
        int max_speed;
        if (p->lev <= 10) max_speed = 3;
        else if (p->lev <= 20) max_speed = 4;
        else if (p->lev <= 25) max_speed = 5;
        else if (p->lev <= 30) max_speed = 6;
        else if (p->lev <= 35) max_speed = 8;
        else if (p->lev <= 40) max_speed = 11;
        
        if (new_obj->modifiers[OBJ_MOD_SPEED] > max_speed)
            new_obj->modifiers[OBJ_MOD_SPEED] = max_speed;
    }

    return new_obj;
}


/*
 * Attempt to make an object
 *
 * c is the current dungeon level
 * lev is the creation level of the object (not necessarily == depth)
 * good is whether the object is to be good
 * great is whether the object is to be great
 * extra_roll is whether we get an extra roll in apply_magic()
 * value is the value to be returned to the calling function
 * tval is the desired tval, or 0 if we allow any tval
 *
 * Returns a pointer to the newly allocated object, or NULL on failure.
 */
struct object *make_object(struct player *p, struct chunk *c, int lev, bool good, bool great,
    bool extra_roll, int32_t *value, int tval)
{
    struct object *new_obj = make_object_aux(p, c, lev, good, great, extra_roll, value, tval);

#ifdef DEBUG_MODE
    if (new_obj)
    {
        char o_name[NORMAL_WID];

        object_desc(p, o_name, sizeof(o_name), new_obj, ODESC_PREFIX | ODESC_FULL);
        cheat(format("%s %s", (new_obj->artifact? "+a":"+o"), o_name));
    }
#endif

    return new_obj;
}


/*
 * Scatter some "great" objects near the player
 */
void acquirement(struct player *p, struct chunk *c, int num, quark_t quark)
{
    struct object *nice_obj;

    /* Acquirement */
    while (num--)
    {
        /* Make a good (or great) object (if possible) */
        nice_obj = make_object(p, c, object_level(&p->wpos), true, true, true, NULL, 0);
        if (!nice_obj) continue;

        set_origin(nice_obj, ORIGIN_ACQUIRE, p->wpos.depth, NULL);
        if (quark > 0) nice_obj->note = quark;

        /* Drop the object */
        drop_near(p, c, &nice_obj, 0, &p->grid, true, DROP_CARRY, false);
    }
}


/*** Make a gold item ***/


/*
 * Get a money kind by name, or level-appropriate
 */
struct object_kind *money_kind(const char *name, int value)
{
    int rank;

    /*
     * (Roughly) the largest possible gold drop at max depth - the precise
     * value is derivable from the calculations in make_gold(), but this is
     * near enough
     */
    int max_gold_drop = 3 * z_info->max_depth + 30;

    /* Check for specified treasure variety */
    for (rank = 0; rank < num_money_types; rank++)
    {
        if (streq(name, money_type[rank].name)) break;
    }

    /* Pick a treasure variety scaled by level */
    if (rank == num_money_types)
        rank = (((value * 100) / max_gold_drop) * num_money_types) / 100;

    /* Do not create illegal treasure types */
    if (rank >= num_money_types) rank = num_money_types - 1;

    return lookup_kind(TV_GOLD, money_type[rank].type);
}


/*
 * Make a money object
 *
 * lev the dungeon level
 * coin_type the name of the type of money object to make
 *
 * Returns a pointer to the newly minted cash (cannot fail)
 */
struct object *make_gold(struct player *p, struct chunk *c, int lev, const char *coin_type)
{
    /* This average is 16 at dlev0, 80 at dlev40, 176 at dlev100. */
    int avg = (16 * lev) / 10 + 16;
    int spread = lev + 10;
    int value = rand_spread(avg, spread);
    struct object *new_gold = object_new();

    /* Increase the range to infinite, moving the average to 110% */
    while (one_in_(100) && (value * 10 <= SHRT_MAX)) value *= 10;

    /* Prepare a gold object */
    object_prep(p, c, new_gold, money_kind(coin_type, value), lev, RANDOMISE);

    /* If we're playing with no_selling, increase the value */
    if (p && (cfg_limited_stores || OPT(p, birth_no_selling)))
    {
        /* Classic method: multiply by 5 in the dungeon */
        if (cfg_gold_drop_vanilla && (p->wpos.depth > 0)) value *= 5;

        /* PWMAngband method: multiply by a depth dependent factor */
        else
        {
            value = (value * level_golds[p->wpos.depth]) / 10;
            if (streq(p->clazz->name, "Trader") || streq(p->clazz->name, "Scavenger"))
                value /= 2;
        }
    }

    /* Cap gold at max short (or alternatively make pvals int32_t) */
    if (value >= SHRT_MAX) value = SHRT_MAX - randint0(200);

    new_gold->pval = value;

    return new_gold;
}


void make_randart(struct player *p, struct chunk *c, struct object *obj, struct artifact *art,
    int32_t randart_seed)
{
    char o_name[NORMAL_WID];

    /* Mark the item as a random artifact */
    obj->artifact = &a_info[art->aidx];
    obj->randart_seed = randart_seed;
    object_desc(NULL, o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_BASE);
    plog_fmt("Random artifact %s created", o_name);

    /* Copy across all the data from the artifact struct */
    copy_artifact_data(obj, art);

    /* Mark the randart as "created" */
    p->randart_created[obj->artifact->aidx] = 1;
    obj->creator = p->id;

    /* Mark the artifact as "generated" if dungeon is ready */
    if (!ht_zero(&c->generated))
        set_artifact_info(p, obj, ARTS_GENERATED);

    /* Otherwise, preserve artifacts from dungeon generation errors */
    else
        p->randart_info[obj->artifact->aidx] += ARTS_CREATED;
}


void fuel_default(struct object *obj)
{
    /* Default fuel levels */
    if (of_has(obj->flags, OF_BURNS_OUT))
        obj->timeout = z_info->fuel_torch;
    else if (of_has(obj->flags, OF_TAKES_FUEL))
        obj->timeout = z_info->default_lamp;
}


/*
 * Create a random artifact on the spot.
 *
 * The current square must contain an unique object that is non ego, non artifact, and not cursed.
 */
void create_randart(struct player *p, struct chunk *c)
{
    struct object *obj;

    if (!cfg_random_artifacts)
    {
        msg(p, "You cannot create random artifacts.");
        return;
    }

    /* Use the first object on the floor */
    obj = square_object(c, &p->grid);
    if (!obj)
    {
        msg(p, "There is nothing on the floor.");
        return;
    }

    /* Object must be unique, non ego, non artifact, and not cursed */
    if ((obj->number > 1) || obj->ego || obj->artifact || obj->curses)
    {
        msg(p, "The object is not suited for artifact creation.");
        return;
    }

    /* Set unidentified */
    if (obj->known) object_free(obj->known);
    object_set_base_known(p, obj);

    if (create_randart_aux(p, c, obj, false))
    {
        if (object_has_standard_to_h(obj)) obj->known->to_h = 1;
        if (object_flavor_is_aware(p, obj)) object_id_set_aware(obj);

        player_know_floor(p, c);

        /* Success */
        msg(p, "You manage to create a random artifact.");
        return;
    }

    /* Failure */
    msg(p, "You don't manage to create a random artifact.");
}


/*
 * Reroll a random artifact on the spot.
 *
 * The current square must contain an unique random artifact.
 */
void reroll_randart(struct player *p, struct chunk *c)
{
    struct object *obj;
    struct artifact *art;
    int32_t randart_seed;
    uint8_t origin;
    int16_t origin_depth;
    const struct monster_race *origin_race;

    if (!cfg_random_artifacts)
    {
        msg(p, "You cannot reroll random artifacts.");
        return;
    }

    /* Use the first object on the floor */
    obj = square_object(c, &p->grid);
    if (!obj)
    {
        msg(p, "There is nothing on the floor.");
        return;
    }

    /* Object must be a random artifact */
    if (!(obj->artifact && obj->randart_seed))
    {
        msg(p, "The object is not a random artifact.");
        return;
    }

    /* Reroll (with same seed) */
    art = do_randart(p, obj->randart_seed, obj->artifact);

    /* Skip "empty" items */
    if (!art)
    {
        msg(p, "You don't manage to reroll the random artifact.");
        return;
    }

    /* Save some info for later */
    randart_seed = obj->randart_seed;
    origin = obj->origin;
    origin_depth = obj->origin_depth;
    origin_race = obj->origin_race;

    /* We need to start from a clean object, so we delete the old one */
    square_delete_object(c, &p->grid, obj, false, false);

    /* Assign the template */
    obj = object_new();
    object_prep(p, c, obj, lookup_kind(art->tval, art->sval), art->alloc_min, RANDOMISE);
    set_origin(obj, origin, origin_depth, origin_race);

    /* Mark the item as a random artifact */
    obj->artifact = &a_info[art->aidx];
    obj->randart_seed = randart_seed;

    /* Copy across all the data from the artifact struct */
    copy_artifact_data(obj, art);

    /* Mark the randart as "created" */
    obj->creator = p->id;

    /* Success */
    free_artifact(art);
    msg(p, "You manage to reroll the random artifact.");

    if (object_has_standard_to_h(obj)) obj->known->to_h = 1;
    if (object_flavor_is_aware(p, obj)) object_id_set_aware(obj);

    drop_near(p, c, &obj, 0, &p->grid, false, DROP_FADE, true);
}


struct init_module obj_make_module =
{
    "obj-make",
    init_obj_make,
    cleanup_obj_make
};
