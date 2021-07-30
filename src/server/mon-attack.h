/*
 * File: mon-attack.h
 * Purpose: Monster attacks
 */

#ifndef MONSTER_ATTACK_H
#define MONSTER_ATTACK_H

extern int chance_of_monster_hit_base(const struct monster_race *race,
    const struct blow_effect *effect);
extern bool make_ranged_attack(struct source *who, struct chunk *c, struct monster *mon,
    int target_m_dis);
extern bool check_hit(struct player *p, int to_hit);
extern int adjust_dam_armor(int damage, int ac);
extern bool make_attack_normal(struct monster *mon, struct source *who);
extern int get_cut(random_value dice, int d_dam);
extern int get_stun(random_value dice, int d_dam);

#endif /* MONSTER_ATTACK_H */
