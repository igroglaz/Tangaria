/*
 * File: player.c
 * Purpose: Player implementation
 *
 * Copyright (c) 2011 elly+angband@leptoquark.net. See COPYING.
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


static const char *stat_name_list[] =
{
    #define STAT(a, b, c) #a,
    #include "../common/list-stats.h"
    #undef STAT
    NULL
};


int stat_name_to_idx(const char *name)
{
    int i;

    for (i = 0; stat_name_list[i]; i++)
    {
        if (!my_stricmp(name, stat_name_list[i])) return i;
    }

    return -1;
}


const char *stat_idx_to_name(int type)
{
    my_assert(type >= 0);
    my_assert(type < STAT_MAX);

    return stat_name_list[type];
}


// to find out how class powerful - used for checking account points
int class_power(const char* clazz) {
    // very strong
    if (streq(clazz, "Warrior") || streq(clazz, "Monk") ||
        streq(clazz, "Shapechanger") || streq(clazz, "Unbeliever"))
        return 1;
    // strong
    else if (streq(clazz, "Rogue") || streq(clazz, "Paladin") ||
             streq(clazz, "Blackguard") || streq(clazz, "Archer"))
        return 2;
    // medium
    else if (streq(clazz, "Mage") || streq(clazz, "Sorceror") ||
             streq(clazz, "Tamer") || streq(clazz, "Necromancer") ||
             streq(clazz, "Wizard"))
        return 3;
    // all others
    else
        return 0;
}


/*
 * Increases a stat
 */
bool player_stat_inc(struct player *p, int stat)
{
    int v = p->stat_cur[stat];

    /* Cannot go above 18/100 */
    if (v >= 18+100) return false;

    /* Increase linearly */
    if (v < 18) p->stat_cur[stat]++;
    else if (v < 18+90)
    {
        int gain = (((18+100) - v) / 2 + 3) / 2;

        /* Paranoia */
        if (gain < 1) gain = 1;

        /* Apply the bonus */
        p->stat_cur[stat] += randint1(gain) + gain / 2;

        /* Maximal value */
        if (p->stat_cur[stat] > 18+99) p->stat_cur[stat] = 18+99;
    }
    else p->stat_cur[stat] = 18+100;

    /* Bring up the maximum too */
    if (p->stat_cur[stat] > p->stat_max[stat]) p->stat_max[stat] = p->stat_cur[stat];

    /* Recalculate bonuses */
    p->upkeep->update |= (PU_BONUS);

    return true;
}


/*
 * Decreases a stat
 */
bool player_stat_dec(struct player *p, int stat, bool permanent)
{
    int cur, max, res;

    cur = p->stat_cur[stat];
    max = p->stat_max[stat];

    /* Damage "current" value */
    if (cur > 18+10) cur -= 10;
    else if (cur > 18) cur = 18;
    else if (cur > 3) cur -= 1;

    res = (cur != p->stat_cur[stat]);

    /* Damage "max" value */
    if (permanent)
    {
        if (max > 18+10) max -= 10;
        else if (max > 18) max = 18;
        else if (max > 3) max -= 1;

        res = (max != p->stat_max[stat]);
    }

    /* Apply changes */
    if (res)
    {
        p->stat_cur[stat] = cur;
        p->stat_max[stat] = max;
        p->upkeep->update |= (PU_BONUS);
        p->upkeep->redraw |= (PR_STATS);
    }

    return res;
}


// extra gold reward system for account progress
static void award_gold_for_account_points(struct player *p) {

    int extra_gold = 0;

    switch (p->max_lev)
    {
        case 2:
            extra_gold = MIN(p->account_score * 1, 100);
            break;

        case 3:
            extra_gold = MIN(p->account_score * 1.2, 120);
            break;

        case 4:
            extra_gold = MIN(p->account_score * 1.4, 140);
            break;

        case 5:
            extra_gold = MIN(p->account_score * 1.6, 160);
            break;

        case 6:
            extra_gold = MIN(p->account_score * 1.8, 180);
            break;

        case 7:
            extra_gold = MIN(p->account_score * 2, 200);
            break;

        case 8:
            extra_gold = MIN(p->account_score * 2.2, 220);
            break;

        case 9:
            extra_gold = MIN(p->account_score * 2.4, 240);
            break;

        case 10:
            extra_gold = MIN(p->account_score * 3, 300);
            break;

        case 11:
            extra_gold = MIN(p->account_score * 3.5, 350);
            break;

        case 12:
            extra_gold = MIN(p->account_score * 4, 400);
            break;

        case 13:
            extra_gold = MIN(p->account_score * 4.5, 450);
            break;

        case 14:
            extra_gold = MIN(p->account_score * 5, 500);
            break;

        case 15:
            extra_gold = MIN(p->account_score * 5.5, 550);
            break;

        case 16:
            extra_gold = MIN(p->account_score * 6, 600);
            break;

        case 17:
            extra_gold = MIN(p->account_score * 6.5, 650);
            break;

        case 18:
            extra_gold = MIN(p->account_score * 7, 700);
            break;

        case 19:
            extra_gold = MIN(p->account_score * 7.5, 750);
            break;

        case 20:
            extra_gold = MIN(p->account_score * 8, 1000);
            break;

        case 21:
            extra_gold = MIN(p->account_score * 8.5, 1100);
            break;

        case 22:
            extra_gold = MIN(p->account_score * 9, 1200);
            break;

        case 23:
            extra_gold = MIN(p->account_score * 9.5, 1300);
            break;

        case 24:
            extra_gold = MIN(p->account_score * 10, 1400);
            break;

        case 25:
            extra_gold = MIN(p->account_score * 10.5, 1500);
            break;

        case 26:
            extra_gold = MIN(p->account_score * 11, 1600);
            break;

        case 27:
            extra_gold = MIN(p->account_score * 11.5, 1700);
            break;

        case 28:
            extra_gold = MIN(p->account_score * 12, 1800);
            break;

        case 29:
            extra_gold = MIN(p->account_score * 12.5, 1900);
            break;

        case 30:
            extra_gold = MIN(p->account_score * 13, 2400);
            break;

        case 31:
            extra_gold = MIN(p->account_score * 14, 3000);
            break;

        case 32:
            extra_gold = MIN(p->account_score * 15, 3200);
            break;

        case 33:
            extra_gold = MIN(p->account_score * 16, 3400);
            break;

        case 34:
            extra_gold = MIN(p->account_score * 17, 3600);
            break;

        case 35:
            extra_gold = MIN(p->account_score * 18, 3800);
            break;

        case 36:
            extra_gold = MIN(p->account_score * 19, 4000);
            break;

        case 37:
            extra_gold = MIN(p->account_score * 20, 4200);
            break;

        case 38:
            extra_gold = MIN(p->account_score * 21, 4400);
            break;

        case 39:
            extra_gold = MIN(p->account_score * 22, 4600);
            break;

        case 40:
            extra_gold = MIN(p->account_score * 23, 6000);
            break;

        case 41:
            extra_gold = MIN(p->account_score * 24, 7000);
            break;

        case 42:
            extra_gold = MIN(p->account_score * 25, 8000);
            break;

        case 43:
            extra_gold = MIN(p->account_score * 26, 9000);
            break;

        case 44:
            extra_gold = MIN(p->account_score * 27, 10000);
            break;

        case 45:
            extra_gold = MIN(p->account_score * 28, 11000);
            break;

        case 46:
            extra_gold = MIN(p->account_score * 29, 12000);
            break;

        case 47:
            extra_gold = MIN(p->account_score * 30, 13000);
            break;

        case 48:
            extra_gold = MIN(p->account_score * 31, 14000);
            break;

        case 49:
            extra_gold = MIN(p->account_score * 32, 15000);
            break;

        case 50:
            extra_gold = MIN(p->account_score * 33, 20000);
            break;

        default:
            extra_gold = 0;
            break;
    }

    p->gold += extra_gold;

    if (extra_gold > 0) {
        msg(p, "You've earned %d extra gold for having %d account points!", extra_gold, p->account_score);
    }
}


/*
 * Advance experience levels and print experience
 */
static void adjust_level(struct player *p)
{
    char buf[NORMAL_WID];
    bool redraw = false;

    /* Hack -- lower limit */
    if (p->exp < 0) p->exp = 0;

    /* Hack -- lower limit */
    if (p->max_exp < 0) p->max_exp = 0;

    /* Hack -- upper limit */
    if (p->exp > PY_MAX_EXP) p->exp = PY_MAX_EXP;

    /* Hack -- upper limit */
    if (p->max_exp > PY_MAX_EXP) p->max_exp = PY_MAX_EXP;

    /* Hack -- maintain "max" experience */
    if (p->exp > p->max_exp) p->max_exp = p->exp;

    /* Redraw experience */
    p->upkeep->redraw |= (PR_EXP);

    /* Update stuff */
    update_stuff(p, chunk_get(&p->wpos));

    /* Lose levels while possible */
    while ((p->lev > 1) && (p->exp < adv_exp(p->lev - 1, p->expfact)))
    {
        /* Lose a level */
        p->lev--;

        /* Permanently polymorphed characters */
        if (player_has(p, PF_PERM_SHAPE))
        {
            if (player_has(p, PF_DRAGON)) poly_dragon(p, true);
            else poly_shape(p, true);
        }

        /* Redraw */
        redraw = true;
    }

    /* Gain levels while possible */
    while ((p->lev < PY_MAX_LEVEL) && (p->exp >= adv_exp(p->lev, p->expfact)))
    {
        /* Gain a level */
        p->lev++;

        /* Permanently polymorphed characters */
        if (player_has(p, PF_PERM_SHAPE))
        {
            if (player_has(p, PF_DRAGON)) poly_dragon(p, true);
            else poly_shape(p, true);
        }

        /* Save the highest level */
        if (p->lev > p->max_lev)
        {
            bool extraPoint = false;
            struct source who_body;
            struct source *who = &who_body;

            p->max_lev = p->lev;

            // restore life every 10th lvl
            if ((p->lives == 0) && !(p->max_lev % 10)) {
                p->lives = 1;
            }

            // account score when gain lvls
            // it's in rotation with mon-util.c (killing uniques)

            if (p->max_lev == 50)
            {
                p->account_score += 5;
                msgt(p, MSG_FANFARE, "You've earned 5 account points! You have %lu points.", p->account_score);
            }
            else if (p->account_score == 0 && p->max_lev == 3)
            {
                p->account_score++;
                msgt(p, MSG_FANFARE, "You've earned 1 account point! These points preserve even after death.");
                msg(p, "To earn account points - earn levels and defeat unique monsters.");
                msg(p, "Account points allows to buy bigger houses, increase storage space,");
                msg(p, "give access to more races/classes and provide other advantages.");
                msg(p, "You will get next account point after getting 3 levels more.");
            }
            else if (p->account_score <= 5)
            {
                if (!(p->max_lev % 3))
                {
                    p->account_score++;
                    msgt(p, MSG_FANFARE, "You've earned account point! Now you have %lu account points.", p->account_score);
                    msg(p, "You will get next account point after getting 3 levels more.");
                    if (p->account_score == 2)
                    {
                        msg(p, "Hint: later on it will be every 5 levels or even less often");
                        msg(p, "(the more points you have - the harder it is to get new ones).");
                    }
                }
            }
            else if (p->account_score <= 10)
            {
                if (!(p->max_lev % 5))
                {
                    p->account_score++;
                    msgt(p, MSG_FANFARE, "You've earned account point! Now you have %lu points.", p->account_score);
                    if (p->account_score != 10)
                    {
                        msg(p, "You will get next account point after getting 5 levels more");
                        if (p->account_score == 6)
                            msg(p, "(Hint: the more points you have - the harder it is to get new ones).");
                    }
                }
                // gratoz
                if (p->account_score == 10)
                {
                    msg(p, "Great job! Now you can build your own house!");
                    msg(p, "(check the guide on the website to find out how to do it).");
                    msg(p, "Also please note that from now on it will be harder to earn points:");
                    msg(p, "you will get even/odd account point for levels and");
                    msg(p, "for defeating unique monster (please check website for details).");
                }
            }
            // only at even score
            else if (!(p->account_score % 2))
            {
                if (p->account_score < 25)
                {
                    if (!(p->max_lev % 10))
                    {
                        p->account_score++;
                        msgt(p, MSG_FANFARE, "You've earned account point! Now you have %lu points.", p->account_score);
                        msg(p, "You will get even/odd account point for levels and");
                        msg(p, "for defeating unique monster.");
                    }
                }
                else if (p->account_score < 50)
                {
                    if (one_in_(51 - p->max_lev) || !(p->max_lev % 15))
                    {
                        p->account_score++;
                        msgt(p, MSG_FANFARE, "You've earned account point! You have %lu points.", p->account_score);
                    }
                }
                else if (p->account_score < 100)
                {
                    if (p->max_lev >= 10 && (one_in_(51 - p->max_lev) || !(p->max_lev % 20)))
                    {
                        p->account_score++;
                        msgt(p, MSG_FANFARE, "You've earned account point! You have %lu points.", p->account_score);
                    }
                }
                else if (p->account_score < 200)
                {
                    if (p->max_lev >= 15 && (one_in_(51 - p->max_lev) || !(p->max_lev % 25)))
                    {
                        p->account_score++;
                        msgt(p, MSG_FANFARE, "You've earned account point! You have %lu points.", p->account_score);
                    }
                }
                else if (p->account_score < 500)
                {
                    if (p->max_lev >= 20 && (one_in_(51 - p->max_lev) || !(p->max_lev % 30)))
                    {
                        p->account_score++;
                        msgt(p, MSG_FANFARE, "You've earned account point! You have %lu points.", p->account_score);
                    }
                }
                else if (p->account_score < 999)
                {
                    if (p->max_lev >= 25 && (one_in_(51 - p->max_lev) || !(p->max_lev % 35)))
                    {
                        p->account_score++;
                        msgt(p, MSG_FANFARE, "You've earned account point! You have %lu points.", p->account_score);
                    }
                }
                else if (p->max_lev >= 30)
                {
                    if (one_in_(51 - p->max_lev) || !(p->max_lev % 40))
                    {
                        p->account_score++;
                        msgt(p, MSG_FANFARE, "You've earned account point! You have %lu points.", p->account_score);
                    }
                }
            }

            // award an extra point for hardcore heroes
            if (OPT(p, birth_hardcore)) {
                int classPower = class_power(p->clazz->name);
                if (classPower == 3) {
                    if (p->max_lev == 29 || p->max_lev == 39 || p->max_lev == 50)
                            extraPoint = true;
                } else if (classPower == 2) {
                    if (p->max_lev == 19 || p->max_lev == 29 || 
                        p->max_lev == 39 || p->max_lev == 50)
                            extraPoint = true;
                } else if (classPower == 1) {
                    if (p->max_lev == 19 || p->max_lev == 29 ||
                        p->max_lev == 39 || p->max_lev == 49 || p->max_lev == 50)
                            extraPoint = true;
                } else {
                    if (p->max_lev == 14 || p->max_lev == 24 || 
                        p->max_lev == 34 || p->max_lev == 44 ||
                        p->max_lev == 49 || p->max_lev == 50)
                            extraPoint = true;
                }
            }

            // ironman characters - award an extra points
            if (OPT(p, birth_ironman)) {
                int classPower = class_power(p->clazz->name);
                if (classPower == 3) {
                    if (p->max_lev == 20 || p->max_lev == 30 || 
                        p->max_lev == 40 || p->max_lev == 50)
                            extraPoint = true;
                } else if (classPower == 2) { // 15-20-25-30-35-40-45-50
                    if (p->max_lev >= 15 && p->max_lev % 5 == 0)
                            extraPoint = true;
                } else { // no need classPower == 1 check for this mode.. too hard
                    if (p->max_lev >= 10 && p->max_lev % 5 == 0)
                            extraPoint = true;
                }
            // brave got extra points too
            } else if (OPT(p, birth_no_recall) && OPT(p, birth_force_descend)) {
                int classPower = class_power(p->clazz->name);
                if (classPower == 3) {
                    if (p->max_lev == 24 || p->max_lev == 34 ||
                        p->max_lev == 44 || p->max_lev == 50)
                            extraPoint = true;
                } else if (classPower == 2) { // 20-25-30-35-40-45-50
                    if (p->max_lev >= 20 && p->max_lev % 5 == 0)
                            extraPoint = true;
                } else { // no classPower==1 check. // 15-20-25-30-35-40-45-50
                    if (p->max_lev >= 15 && p->max_lev % 5 == 0)
                            extraPoint = true;
                }
            }

            if (extraPoint) {
                msg(p, "Extra point awarded for your hard-mode challenge! You have %lu points.", ++p->account_score);
            }
            
            // extra gold for account points
            award_gold_for_account_points(p);

            /* Message */
            if (p->max_lev == 50)
                msgt(p, MSG_FANFARE, "Ding! Welcome to level %d.", p->lev);
            else
                msgt(p, MSG_LEVEL, "Welcome to level %d.", p->lev);
            strnfmt(buf, sizeof(buf), "%s has attained level %d.", p->name, p->lev);
            msg_broadcast(p, buf, MSG_BROADCAST_LEVEL);

            /* Restore stats */
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_simple(EF_RESTORE_STAT, who, "0", STAT_STR, 0, 0, 0, 0, NULL);
            effect_simple(EF_RESTORE_STAT, who, "0", STAT_INT, 0, 0, 0, 0, NULL);
            effect_simple(EF_RESTORE_STAT, who, "0", STAT_WIS, 0, 0, 0, 0, NULL);
            effect_simple(EF_RESTORE_STAT, who, "0", STAT_DEX, 0, 0, 0, 0, NULL);
            effect_simple(EF_RESTORE_STAT, who, "0", STAT_CON, 0, 0, 0, 0, NULL);
            effect_simple(EF_RESTORE_STAT, who, "0", STAT_CHR, 0, 0, 0, 0, NULL);

            /* Record this event in the character history */
            if (!(p->lev % 5))
            {
                strnfmt(buf, sizeof(buf), "Reached level %d", p->lev);
                history_add_unique(p, buf, HIST_GAIN_LEVEL);
            }

            /* Player learns innate runes */
            player_learn_innate(p);
        }

        /* Redraw */
        redraw = true;
    }

    /* Redraw - Do it only once to avoid socket buffer overflow */
    if (redraw)
    {
        /* Update some stuff */
        p->upkeep->update |= (PU_BONUS | PU_SPELLS | PU_MONSTERS);

        /* Redraw some stuff */
        p->upkeep->redraw |= (PR_LEV | PR_TITLE | PR_EXP | PR_STATS | PR_SPELL | PR_PLUSSES);
        set_redraw_equip(p, NULL);
    }

    /* Update stuff */
    update_stuff(p, chunk_get(&p->wpos));
}


/*
 * Gain experience
 */
void player_exp_gain(struct player *p, int32_t amount)
{
    // Rogue class get exp faster (which make gameplay a bit harder)
    if (streq(p->clazz->name, "Rogue") && p->lev < 50)
        amount = (amount * 10) / 9;

    // Endgame exp factors
    if (p->lev > 48 && amount > 10)
    {
        // ... races:
        if      (streq(p->race->name, "Titan") || streq(p->race->name, "Djinn") ||
                 streq(p->race->name, "Dragon"))
            amount /= 4;
        else if (streq(p->race->name, "Ent") || streq(p->race->name, "Maiar") ||
                 streq(p->race->name, "Beholder") || streq(p->race->name, "Wisp") ||
                 streq(p->race->name, "Wraith"))
            amount /= 3;
        else if (streq(p->race->name, "High-Elf") || streq(p->race->name, "Thunderlord") ||
                 streq(p->race->name, "Troll") || streq(p->race->name, "Naga") ||
                 streq(p->race->name, "Balrog") || streq(p->race->name, "Half-Giant") ||
                 streq(p->race->name, "Gargoyle") || streq(p->race->name, "Golem"))
            amount /= 2;
        else if (streq(p->race->name, "Hydra") || streq(p->race->name, "Minotaur") ||
                 streq(p->race->name, "Half-Troll") || streq(p->race->name, "Vampire"))
            amount = (amount * 2) / 3;
        else if (streq(p->race->name, "Black Numenorean") || streq(p->race->name, "Dunadan") ||
                 streq(p->race->name, "Dark Elf") || streq(p->race->name, "Draconian"))
            amount = (amount * 3) / 4;
        // buff
        else if (streq(p->race->name, "Human"))
            amount = (amount * 3) / 2;

        // ... classes:
        if (streq(p->clazz->name, "Warrior") || streq(p->clazz->name, "Monk") ||
                 streq(p->clazz->name, "Shapechanger") || streq(p->clazz->name, "Unbeliever"))
            amount /= 2;
        else if (streq(p->clazz->name, "Rogue") || streq(p->clazz->name, "Paladin") ||
                 streq(p->clazz->name, "Blackguard") || streq(p->clazz->name, "Archer"))
            amount = (amount * 2) / 3;
        else if (streq(p->clazz->name, "Mage") || streq(p->clazz->name, "Sorceror") ||
                 streq(p->clazz->name, "Tamer") || streq(p->clazz->name, "Necromancer") ||
                 streq(p->clazz->name, "Wizard"))
            amount = (amount * 3) / 4;
    }

    /* Gain some experience */
    p->exp += amount;

    /* Slowly recover from experience drainage */
    if (p->exp < p->max_exp)
    {
        /* Gain max experience (10%) */
        p->max_exp += amount / 10;
    }

    /* Adjust experience levels */
    adjust_level(p);
}


/*
 * Lose experience
 */
void player_exp_lose(struct player *p, int32_t amount, bool permanent)
{
    /* Never drop below zero experience */
    if (amount > p->exp) amount = p->exp;

    /* Lose some experience */
    p->exp -= amount;
    if (permanent) p->max_exp -= amount;

    // loose some satiation too
    if (p->lev < 50)
        player_dec_timed(p, TMD_FOOD, 20, false);

    /* Adjust experience levels */
    adjust_level(p);
}


/*
 * Obtain the "flags" for the player as if he was an item
 */
void player_flags(struct player *p, bitflag f[OF_SIZE])
{
    int i;

    /* Unencumbered monks get nice abilities */
    bool restrict_ = (player_has(p, PF_MARTIAL_ARTS) && !monk_armor_ok(p));

    /* Clear */
    of_wipe(f);

    /* Add racial/class flags */
    for (i = 1; i < OF_MAX; i++)
    {
        if (of_has(p->race->flags, i) && (p->lev >= p->race->flvl[i])) of_on(f, i);
        if (of_has(p->clazz->flags, i) && (p->lev >= p->clazz->flvl[i]) && !restrict_) of_on(f, i);
    }

    /* Ghost */
    if (p->ghost)
    {
        of_on(f, OF_SEE_INVIS);
        of_on(f, OF_HOLD_LIFE);
        of_on(f, OF_FREE_ACT);
        of_on(f, OF_PROT_FEAR);

        /* PWMAngband */
        of_on(f, OF_PROT_BLIND);
        of_on(f, OF_PROT_CONF);
        of_on(f, OF_PROT_STUN);
        of_on(f, OF_FEATHER);
        of_on(f, OF_SUST_STR);
        of_on(f, OF_SUST_INT);
        of_on(f, OF_SUST_WIS);
        of_on(f, OF_SUST_DEX);
        of_on(f, OF_SUST_CON);
        of_on(f, OF_SUST_CHR);
    }

    /* Handle polymorphed players */
    if (p->poly_race)
    {
        int m;

        //// druid's forms flags ////
        if (streq(p->poly_race->name, "rat-form"))
            of_on(f, OF_HOLD_LIFE);
        else if (streq(p->poly_race->name, "boar-form"))
            of_on(f, OF_SLOW_DIGEST);
        else if (streq(p->poly_race->name, "cat-form"))
        {
            of_on(f, OF_REGEN);
            of_on(f, OF_IMPAIR_MANA);
        }
        /////////////////////////////

        for (m = 0; m < z_info->mon_blows_max; m++)
        {
            /* Skip non-attacks */
            if (!p->poly_race->blow[m].method) continue;

            if (streq(p->poly_race->blow[m].effect->name, "EXP_10")) of_on(f, OF_HOLD_LIFE);
            if (streq(p->poly_race->blow[m].effect->name, "EXP_20")) of_on(f, OF_HOLD_LIFE);
            if (streq(p->poly_race->blow[m].effect->name, "EXP_40")) of_on(f, OF_HOLD_LIFE);
            if (streq(p->poly_race->blow[m].effect->name, "EXP_80")) of_on(f, OF_HOLD_LIFE);
        }

        /* Monster race flags */
        if (rf_has(p->poly_race->flags, RF_REGENERATE)) of_on(f, OF_REGEN);
        if (rf_has(p->poly_race->flags, RF_FRIGHTENED)) of_on(f, OF_AFRAID);
        if (rf_has(p->poly_race->flags, RF_IM_NETHER)) of_on(f, OF_HOLD_LIFE);
        if (rf_has(p->poly_race->flags, RF_IM_WATER))
        {
            of_on(f, OF_PROT_CONF);
            of_on(f, OF_PROT_STUN);
        }
        if (rf_has(p->poly_race->flags, RF_IM_PLASMA)) of_on(f, OF_PROT_STUN);
        if (rf_has(p->poly_race->flags, RF_NO_FEAR)) of_on(f, OF_PROT_FEAR);
        if (rf_has(p->poly_race->flags, RF_NO_STUN)) of_on(f, OF_PROT_STUN);
        if (rf_has(p->poly_race->flags, RF_NO_CONF)) of_on(f, OF_PROT_CONF);
        if (rf_has(p->poly_race->flags, RF_NO_SLEEP)) of_on(f, OF_FREE_ACT);
        if (rf_has(p->poly_race->flags, RF_LEVITATE)) of_on(f, OF_FEATHER);

        /* Monster spell flags */
        if (rsf_has(p->poly_race->spell_flags, RSF_BR_NETH)) of_on(f, OF_HOLD_LIFE);
        if (rsf_has(p->poly_race->spell_flags, RSF_BR_LIGHT)) of_on(f, OF_PROT_BLIND);
        if (rsf_has(p->poly_race->spell_flags, RSF_BR_DARK)) of_on(f, OF_PROT_BLIND);
        if (rsf_has(p->poly_race->spell_flags, RSF_BR_SOUN)) of_on(f, OF_PROT_STUN);
        if (rsf_has(p->poly_race->spell_flags, RSF_BR_CHAO)) of_on(f, OF_PROT_CONF);
        if (rsf_has(p->poly_race->spell_flags, RSF_BR_INER)) of_on(f, OF_FREE_ACT);
        if (rsf_has(p->poly_race->spell_flags, RSF_BR_GRAV))
        {
            of_on(f, OF_FEATHER);
            of_on(f, OF_PROT_STUN);
        }
        if (rsf_has(p->poly_race->spell_flags, RSF_BR_PLAS)) of_on(f, OF_PROT_STUN);
        if (rsf_has(p->poly_race->spell_flags, RSF_BR_WALL)) of_on(f, OF_PROT_STUN);
        if (rsf_has(p->poly_race->spell_flags, RSF_BR_WATE))
        {
            of_on(f, OF_PROT_CONF);
            of_on(f, OF_PROT_STUN);
        }
        // players can fly for real in bird or bat forms
        if (p->poly_race->base == lookup_monster_base("bird") || p->poly_race->base == lookup_monster_base("bat"))
            of_on(f, OF_FLYING);
    }
}


/*
 * Combine any flags due to timed effects on the player into those in f.
 */
void player_flags_timed(struct player *p, bitflag f[OF_SIZE])
{
    int i;

    for (i = 0; i < TMD_MAX; ++i)
    {
        if (p->timed[i] && timed_effects[i].oflag_dup != OF_NONE && i != TMD_TRAPSAFE)
            of_on(f, timed_effects[i].oflag_dup);
    }
/* old PWMA code (might be useful for customization)
    if (p->timed[TMD_BOLD]) of_on(f, OF_PROT_FEAR);
    if (p->timed[TMD_HOLD_LIFE]) of_on(f, OF_HOLD_LIFE);
    if (p->timed[TMD_FLIGHT]) of_on(f, OF_FLYING);
    if (p->timed[TMD_ESP]) of_on(f, OF_ESP_ALL);
    if (p->timed[TMD_SINVIS]) of_on(f, OF_SEE_INVIS);
    if (p->timed[TMD_FREE_ACT]) of_on(f, OF_FREE_ACT);
    if (p->timed[TMD_AFRAID] || p->timed[TMD_TERROR]) of_on(f, OF_AFRAID);
    if (p->timed[TMD_OPP_CONF]) of_on(f, OF_PROT_CONF);
    if (p->timed[TMD_OPP_AMNESIA]) of_on(f, OF_PROT_AMNESIA);
*/
}


/*
 * Number of connected players
 */
int NumPlayers;


/*
 * An array for player structures
 *
 * Player index is in [1..NumPlayers]
 */
static struct player **Players;


void init_players(void)
{
    Players = mem_zalloc(MAX_PLAYERS * sizeof(struct player*));
}


void free_players(void)
{
    mem_free(Players);
    Players = NULL;
}


struct player *player_get(int id)
{
    return (((id > 0) && (id < MAX_PLAYERS))? Players[id]: NULL);
}


void player_set(int id, struct player *p)
{
    if ((id > 0) && (id < MAX_PLAYERS)) Players[id] = p;
}


/*
 * Record the original (pre-ghost) cause of death
 */
void player_death_info(struct player *p, const char *died_from)
{
    my_strcpy(p->death_info.title, get_title(p), sizeof(p->death_info.title));
    p->death_info.max_lev = p->max_lev;
    p->death_info.lev = p->lev;
    p->death_info.max_exp = p->max_exp;
    p->death_info.exp = p->exp;
    p->death_info.au = p->au;
    p->death_info.max_depth = p->max_depth;
    memcpy(&p->death_info.wpos, &p->wpos, sizeof(struct worldpos));
    my_strcpy(p->death_info.died_from, died_from, sizeof(p->death_info.died_from));
    time(&p->death_info.time);
    my_strcpy(p->death_info.ctime, ctime(&p->death_info.time), sizeof(p->death_info.ctime));
}


/*
 * Return a version of the player's name safe for use in filesystems.
 */
void player_safe_name(char *safe, size_t safelen, const char *name)
{
    size_t i;
    size_t limit = 0;

    if (name) limit = strlen(name);

    /* Limit to maximum size of safename buffer */
    limit = MIN(limit, safelen);

    for (i = 0; i < limit; i++)
    {
        char c = name[i];

        /* Convert all non-alphanumeric symbols */
        if (!isalpha((unsigned char)c) && !isdigit((unsigned char)c)) c = '_';

        /* Build "base_name" */
        safe[i] = c;
    }

    /* Terminate */
    safe[i] = '\0';

    /* Require a "base" name */
    if (!safe[0]) my_strcpy(safe, "PLAYER", safelen);
}


void player_cave_new(struct player *p, int height, int width)
{
    struct loc grid;

    if (p->cave->allocated) player_cave_free(p);

    p->cave->height = height;
    p->cave->width = width;

    p->cave->squares = mem_zalloc(p->cave->height * sizeof(struct player_square*));
    p->cave->noise.grids = mem_zalloc(p->cave->height * sizeof(uint16_t*));
    p->cave->scent.grids = mem_zalloc(p->cave->height * sizeof(uint16_t*));
    for (grid.y = 0; grid.y < p->cave->height; grid.y++)
    {
        p->cave->squares[grid.y] = mem_zalloc(p->cave->width * sizeof(struct player_square));
        for (grid.x = 0; grid.x < p->cave->width; grid.x++)
            square_p(p, &grid)->info = mem_zalloc(SQUARE_SIZE * sizeof(bitflag));
        p->cave->noise.grids[grid.y] = mem_zalloc(p->cave->width * sizeof(uint16_t));
        p->cave->scent.grids[grid.y] = mem_zalloc(p->cave->width * sizeof(uint16_t));
    }
    p->cave->allocated = true;
}


/*
 * Initialize player struct
 */
void init_player(struct player *p, int conn, bool old_history, bool ironman, bool no_recall, bool force_descend)
{
    int i;
    char history[N_HIST_LINES][N_HIST_WRAP];
    connection_t *connp = get_connection(conn);

    /* Free player structure */
    cleanup_player(p);

    /* Wipe the player */
    if (old_history) memcpy(history, p->history, N_HIST_LINES * N_HIST_WRAP);
    memset(p, 0, sizeof(struct player));
    if (old_history) memcpy(p->history, history, N_HIST_LINES * N_HIST_WRAP);

    p->scr_info = mem_zalloc((z_info->dungeon_hgt + ROW_MAP + 1) * sizeof(cave_view_type*));
    p->trn_info = mem_zalloc((z_info->dungeon_hgt + ROW_MAP + 1) * sizeof(cave_view_type*));
    for (i = 0; i < z_info->dungeon_hgt + ROW_MAP + 1; i++)
    {
        p->scr_info[i] = mem_zalloc((z_info->dungeon_wid + COL_MAP) * sizeof(cave_view_type));
        p->trn_info[i] = mem_zalloc((z_info->dungeon_wid + COL_MAP) * sizeof(cave_view_type));
    }

    /* Allocate player sub-structs */
    p->upkeep = mem_zalloc(sizeof(struct player_upkeep));
    p->upkeep->inven = mem_zalloc((z_info->pack_size + 1) * sizeof(struct object *));
    p->upkeep->quiver = mem_zalloc(z_info->quiver_size * sizeof(struct object *));
    p->timed = mem_zalloc(TMD_MAX * sizeof(int16_t));
    p->obj_k = object_new();
    p->obj_k->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
    p->obj_k->slays = mem_zalloc(z_info->slay_max * sizeof(bool));
    p->obj_k->curses = mem_zalloc(z_info->curse_max * sizeof(struct curse_data));

    /* Allocate memory for lore array */
    p->lore = mem_zalloc(z_info->r_max * sizeof(struct monster_lore));
    for (i = 0; i < z_info->r_max; i++)
    {
        p->lore[i].blows = mem_zalloc(z_info->mon_blows_max * sizeof(uint8_t));
        p->lore[i].blow_known = mem_zalloc(z_info->mon_blows_max * sizeof(bool));
    }
    p->current_lore.blows = mem_zalloc(z_info->mon_blows_max * sizeof(uint8_t));
    p->current_lore.blow_known = mem_zalloc(z_info->mon_blows_max * sizeof(uint8_t));

    /* Allocate memory for artifact array */
    p->art_info = mem_zalloc(z_info->a_max * sizeof(uint8_t));

    /* Allocate memory for randart arrays */
    p->randart_info = mem_zalloc((z_info->a_max + 9) * sizeof(uint8_t));
    p->randart_created = mem_zalloc((z_info->a_max + 9) * sizeof(uint8_t));

    /* Allocate memory for dungeon flags array */
    p->kind_aware = mem_zalloc(z_info->k_max * sizeof(bool));
    p->note_aware = mem_zalloc(z_info->k_max * sizeof(quark_t));
    p->kind_tried = mem_zalloc(z_info->k_max * sizeof(bool));
    p->kind_ignore = mem_zalloc(z_info->k_max * sizeof(uint8_t));
    p->kind_everseen = mem_zalloc(z_info->k_max * sizeof(uint8_t));
    p->ego_ignore_types = mem_zalloc(z_info->e_max * sizeof(uint8_t*));
    for (i = 0; i < z_info->e_max; i++)
        p->ego_ignore_types[i] = mem_zalloc(ITYPE_MAX * sizeof(uint8_t));
    p->ego_everseen = mem_zalloc(z_info->e_max * sizeof(uint8_t));

    /* Allocate memory for visuals */
    p->f_attr = mem_zalloc(FEAT_MAX * sizeof(byte_lit));
    p->f_char = mem_zalloc(FEAT_MAX * sizeof(char_lit));
    p->t_attr = mem_zalloc(z_info->trap_max * sizeof(byte_lit));
    p->t_char = mem_zalloc(z_info->trap_max * sizeof(char_lit));
    p->k_attr = mem_zalloc(z_info->k_max * sizeof(uint8_t));
    p->k_char = mem_zalloc(z_info->k_max * sizeof(char));
    p->d_attr = mem_zalloc(z_info->k_max * sizeof(uint8_t));
    p->d_char = mem_zalloc(z_info->k_max * sizeof(char));
    p->r_attr = mem_zalloc(z_info->r_max * sizeof(uint8_t));
    p->r_char = mem_zalloc(z_info->r_max * sizeof(char));

    /* Allocate memory for object and monster lists */
    p->mflag = mem_zalloc(z_info->level_monster_max * MFLAG_SIZE * sizeof(bitflag));
    p->mon_det = mem_zalloc(z_info->level_monster_max * sizeof(uint8_t));

    /* Allocate memory for current cave grid info */
    p->cave = mem_zalloc(sizeof(struct player_cave));

    /* Allocate memory for wilderness knowledge */
    p->wild_map = mem_zalloc((2 * radius_wild + 1) * sizeof(uint8_t *));
    for (i = 0; i <= 2 * radius_wild; i++)
        p->wild_map[i] = mem_zalloc((2 * radius_wild + 1) * sizeof(uint8_t));

    /* Allocate memory for home storage */
    p->home = mem_zalloc(sizeof(struct store));
    memcpy(p->home, &stores[z_info->store_max - 2], sizeof(struct store));
    p->home->stock = NULL;

    /* Analyze every object */
    for (i = 0; i < z_info->k_max; i++)
    {
        struct object_kind *kind = &k_info[i];

        /* Skip "empty" objects */
        if (!kind->name) continue;

        /* No flavor yields aware */
        if (!kind->flavor) p->kind_aware[i] = true;
    }

    /* Always start with a well fed player */
    p->timed[TMD_FOOD] = PY_FOOD_FULL - 2000;

    /* Assume no feeling */
    p->feeling = -1;

    /* Update the wilderness map */
    if ((cfg_diving_mode > 1) || (no_recall && force_descend))
    {
        wild_set_explored(p, base_wpos());
    } else if (ironman)
    {
        wild_set_explored(p, ironman_wpos());
    } else
    {
        wild_set_explored(p, start_wpos());

        /* On "fast" wilderness servers, we also know the location of the base town */
        if (cfg_diving_mode == 1) wild_set_explored(p, base_wpos());
    }

    /* Copy channels pointer */
    p->on_channel = Conn_get_console_channels(conn);

    /* Clear old channels */
    for (i = 0; i < MAX_CHANNELS; i++)
        p->on_channel[i] = 0;

    /* Listen on the default chat channel */
    p->on_channel[0] |= UCM_EAR;

    /* Copy his connection info */
    p->conn = conn;

    /* Default to the first race/class in the edit file */
    p->race = player_id2race(0);
    p->clazz = player_id2class(0);

    monmsg_init(p);
    monster_list_init(p);
    object_list_init(p);

    /* Initialize extra parameters */
    for (i = ITYPE_NONE; i < ITYPE_MAX; i++) p->opts.ignore_lvl[i] = IGNORE_NONE;

    for (i = 0; i < z_info->k_max; i++)
        add_autoinscription(p, i, connp->Client_setup.note_aware[i]);

    p->cancel_firing = true;
}


/*
 * Free player struct
 */
void cleanup_player(struct player *p)
{
    int i;

    if (!p) return;

    /* Free the things that are always initialised */
    if (p->obj_k)
    {
        object_free(p->obj_k);
        p->obj_k = NULL;
    }
    mem_free(p->timed);
    p->timed = NULL;
    if (p->upkeep)
    {
        mem_free(p->upkeep->inven);
        mem_free(p->upkeep->quiver);
    }
    mem_free(p->upkeep);
    p->upkeep = NULL;

    /* Free the things that are only sometimes initialised */
    player_spells_free(p);
    object_pile_free(p->gear);
    free_body(p);

    /* Stop all file perusal and interactivity */
    string_free(p->interactive_file);
    p->interactive_file = NULL;

    /* PWMAngband */
    for (i = 0; p->scr_info && (i < z_info->dungeon_hgt + ROW_MAP + 1); i++)
    {
        mem_free(p->scr_info[i]);
        mem_free(p->trn_info[i]);
    }
    mem_free(p->scr_info);
    p->scr_info = NULL;
    mem_free(p->trn_info);
    p->trn_info = NULL;
    for (i = 0; i < N_HISTORY_FLAGS; i++)
    {
        mem_free(p->hist_flags[i]);
        p->hist_flags[i] = NULL;
    }
    for (i = 0; p->lore && (i < z_info->r_max); i++)
    {
        mem_free(p->lore[i].blows);
        mem_free(p->lore[i].blow_known);
    }
    mem_free(p->lore);
    p->lore = NULL;
    mem_free(p->current_lore.blows);
    p->current_lore.blows = NULL;
    mem_free(p->current_lore.blow_known);
    p->current_lore.blow_known = NULL;
    mem_free(p->art_info);
    p->art_info = NULL;
    mem_free(p->randart_info);
    p->randart_info = NULL;
    mem_free(p->randart_created);
    p->randart_created = NULL;
    mem_free(p->kind_aware);
    p->kind_aware = NULL;
    mem_free(p->note_aware);
    p->note_aware = NULL;
    mem_free(p->kind_tried);
    p->kind_tried = NULL;
    mem_free(p->kind_ignore);
    p->kind_ignore = NULL;
    mem_free(p->kind_everseen);
    p->kind_everseen = NULL;
    for (i = 0; p->ego_ignore_types && (i < z_info->e_max); i++)
        mem_free(p->ego_ignore_types[i]);
    mem_free(p->ego_ignore_types);
    p->ego_ignore_types = NULL;
    mem_free(p->ego_everseen);
    p->ego_everseen = NULL;
    mem_free(p->f_attr);
    p->f_attr = NULL;
    mem_free(p->f_char);
    p->f_char = NULL;
    mem_free(p->t_attr);
    p->t_attr = NULL;
    mem_free(p->t_char);
    p->t_char = NULL;
    mem_free(p->k_attr);
    p->k_attr = NULL;
    mem_free(p->k_char);
    p->k_char = NULL;
    mem_free(p->d_attr);
    p->d_attr = NULL;
    mem_free(p->d_char);
    p->d_char = NULL;
    mem_free(p->r_attr);
    p->r_attr = NULL;
    mem_free(p->r_char);
    p->r_char = NULL;
    mem_free(p->mflag);
    p->mflag = NULL;
    mem_free(p->mon_det);
    p->mon_det = NULL;
    for (i = 0; p->wild_map && (i <= 2 * radius_wild); i++)
        mem_free(p->wild_map[i]);
    mem_free(p->wild_map);
    p->wild_map = NULL;
    if (p->home) object_pile_free(p->home->stock);
    mem_free(p->home);
    p->home = NULL;

    /* Free the history */
    history_clear(p);

    /* Free the cave */
    if (p->cave)
    {
        player_cave_free(p);
        mem_free(p->cave);
        p->cave = NULL;
    }

    monmsg_cleanup(p);
    monster_list_finalize(p);
    object_list_finalize(p);
}


void player_cave_free(struct player *p)
{
    struct loc grid;

    if (!p->cave->allocated) return;

    for (grid.y = 0; grid.y < p->cave->height; grid.y++)
    {
        for (grid.x = 0; grid.x < p->cave->width; grid.x++)
        {
            mem_free(square_p(p, &grid)->info);
            square_p(p, &grid)->info = NULL;
            square_forget_pile(p, &grid);
            square_forget_trap(p, &grid);
        }
        mem_free(p->cave->squares[grid.y]);
        mem_free(p->cave->noise.grids[grid.y]);
        mem_free(p->cave->scent.grids[grid.y]);
    }
    mem_free(p->cave->squares);
    p->cave->squares = NULL;
    mem_free(p->cave->noise.grids);
    p->cave->noise.grids = NULL;
    mem_free(p->cave->scent.grids);
    p->cave->scent.grids = NULL;
    p->cave->allocated = false;
}


/*
 * Clear the flags for each cave grid
 */
void player_cave_clear(struct player *p, bool full)
{
    struct loc begin, end;
    struct loc_iterator iter;

    /* Assume no feeling */
    if (full) p->feeling = -1;

    /* Reset number of feeling squares */
    if (full) p->cave->feeling_squares = 0;

    loc_init(&begin, 0, 0);
    loc_init(&end, p->cave->width, p->cave->height);
    loc_iterator_first(&iter, &begin, &end);

    /* Clear flags and flow information. */
    do
    {
        /* Erase feat */
        square_forget(p, &iter.cur);

        /* Erase object */
        square_forget_pile(p, &iter.cur);

        /* Erase trap */
        square_forget_trap(p, &iter.cur);

        /* Erase flags */
        if (full)
            sqinfo_wipe(square_p(p, &iter.cur)->info);
        else
        {
            /* Erase flags (no bounds checking) */
            sqinfo_off(square_p(p, &iter.cur)->info, SQUARE_SEEN);
            sqinfo_off(square_p(p, &iter.cur)->info, SQUARE_VIEW);
            sqinfo_off(square_p(p, &iter.cur)->info, SQUARE_DTRAP);
        }

        /* Erase flow */
        if (full)
        {
            p->cave->noise.grids[iter.cur.y][iter.cur.x] = 0;
            p->cave->scent.grids[iter.cur.y][iter.cur.x] = 0;
        }
    }
    while (loc_iterator_next_strict(&iter));

    /* Memorize the content of owned houses */
    memorize_houses(p);
}


bool player_square_in_bounds(struct player *p, struct loc *grid)
{
    return ((grid->x >= 0) && (grid->x < p->cave->width) &&
        (grid->y >= 0) && (grid->y < p->cave->height));
}


bool player_square_in_bounds_fully(struct player *p, struct loc *grid)
{
    return ((grid->x > 0) && (grid->x < p->cave->width - 1) &&
        (grid->y > 0) && (grid->y < p->cave->height - 1));
}


struct player *player_from_id(int id)
{
    int i;

    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        if (id == p->id) return p;
    }

    return NULL;
}
