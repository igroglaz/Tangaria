/*
 * File: list-player-timed.h
 * Purpose: Timed player properties
 *
 * Fields:
 * symbol - the effect name
 * flag_redraw - the things to be redrawn when the effect is set
 * flag_update - the things to be updated when the effect is set
 */

/* symbol  flag_redraw  flag_update */
TMD(FAST, PR_STATUS, PU_BONUS)
TMD(SLOW, PR_STATUS, PU_BONUS)
TMD(BLIND, PR_STATUS | PR_MAP | PR_FLOOR, PU_UPDATE_VIEW | PU_MONSTERS)
TMD(PARALYZED, PR_STATUS, 0)
TMD(CONFUSED, PR_STATUS, PU_BONUS)
TMD(AFRAID, PR_STATUS, PU_BONUS)
TMD(IMAGE, PR_STATUS | PR_MAP | PR_MONLIST | PR_ITEMLIST, PU_MONSTERS | PU_BONUS)
TMD(POISONED, PR_STATUS, PU_BONUS)
TMD(CUT, PR_STATUS, 0)
TMD(STUN, PR_STATUS, PU_BONUS)
TMD(FOOD, PR_STATUS, PU_BONUS)
TMD(PROTEVIL, PR_STATUS, 0)
TMD(INVULN, PR_STATUS | PR_MAP, PU_BONUS | PU_MONSTERS)
TMD(HERO, PR_STATUS, PU_BONUS)
TMD(SHERO, PR_STATUS, PU_BONUS)
TMD(SHIELD, PR_STATUS, PU_BONUS)
TMD(BLESSED, PR_STATUS, PU_BONUS)
TMD(SINVIS, PR_STATUS, PU_BONUS | PU_MONSTERS)
TMD(SINFRA, PR_STATUS, PU_BONUS | PU_MONSTERS)
TMD(OPP_ACID, PR_STATUS, PU_BONUS)
TMD(OPP_ELEC, PR_STATUS, PU_BONUS)
TMD(OPP_FIRE, PR_STATUS, PU_BONUS)
TMD(OPP_COLD, PR_STATUS, PU_BONUS)
TMD(OPP_POIS, PR_STATUS, PU_BONUS)
TMD(OPP_CONF, PR_STATUS, PU_BONUS)
TMD(AMNESIA, PR_STATUS, PU_BONUS)
TMD(ESP, PR_STATUS, PU_BONUS | PU_MONSTERS)
TMD(STONESKIN, PR_STATUS, PU_BONUS)
TMD(TERROR, PR_STATUS, PU_BONUS)
TMD(SPRINT, PR_STATUS, PU_BONUS)
TMD(BOLD, PR_STATUS, PU_BONUS)
TMD(SCRAMBLE, PR_STATUS, PU_BONUS)
TMD(TRAPSAFE, PR_STATUS, 0)
TMD(FASTCAST, PR_STATUS, 0)
TMD(ATT_ACID, PR_STATUS, 0)
TMD(ATT_ELEC, PR_STATUS, 0)
TMD(ATT_FIRE, PR_STATUS, 0)
TMD(ATT_COLD, PR_STATUS, 0)
TMD(ATT_POIS, PR_STATUS, 0)
TMD(ATT_CONF, PR_STATUS, 0)
TMD(ATT_EVIL, PR_STATUS, 0)
TMD(ATT_DEMON, PR_STATUS, 0)
TMD(ATT_VAMP, PR_STATUS, 0)
TMD(HEAL, PR_STATUS, 0)
TMD(ATT_RUN, PR_STATUS, 0)
TMD(COVERTRACKS, PR_STATUS, 0)
TMD(TAUNT, PR_STATUS, 0)
TMD(BLOODLUST, PR_STATUS, 0)
TMD(BLACKBREATH, PR_STATUS, 0)
TMD(STEALTH, PR_STATUS, PU_BONUS)
TMD(FREE_ACT, PR_STATUS, PU_BONUS)
TMD(WRAITHFORM, PR_STATUS, 0)
TMD(MEDITATE, PR_STATUS, PU_BONUS)
TMD(MANASHIELD, PR_STATUS | PR_MAP, PU_MONSTERS)
TMD(INVIS, PR_STATUS, PU_MONSTERS)
TMD(MIMIC, PR_STATUS, PU_MONSTERS)
TMD(BOWBRAND, 0, 0)
TMD(ANCHOR, PR_STATUS, PU_BONUS)
TMD(PROBTRAVEL, PR_STATUS, 0)
TMD(ADRENALINE, 0, 0)
TMD(BIOFEEDBACK, PR_STATUS, 0)
TMD(SOUL, PR_STATUS, 0)
TMD(DEADLY, PR_STATUS | PR_MAP, PU_MONSTERS)
TMD(EPOWER, PR_STATUS, 0)
TMD(ICY_AURA, PR_STATUS, 0)
TMD(FARSIGHT, PR_STATUS, PU_BONUS)
TMD(ZFARSIGHT, PR_STATUS, PU_BONUS)
TMD(REGEN, PR_STATUS, 0)
TMD(HARMONY, 0, 0)
TMD(ANTISUMMON, PR_STATUS, 0)
TMD(GROWTH, PR_STATUS, PU_BONUS | PU_MONSTERS)
TMD(REVIVE, PR_STATUS, 0)
TMD(HOLD_LIFE, PR_STATUS, PU_BONUS)
TMD(HOLD_WEAPON, PR_STATUS, 0)
TMD(SAFE, PR_STATUS, PU_BONUS)
TMD(DESPAIR, PR_STATUS, 0)
TMD(FLIGHT, PR_STATUS, PU_BONUS)
TMD(SAFELOGIN, 0, 0)
TMD(OPP_AMNESIA, PR_STATUS, PU_BONUS)
TMD(MOVE_FAST, PR_STATUS, PU_BONUS)
TMD(GOLEM, 0, 0)
TMD(SENTRY, 0, 0)
TMD(OCCUPIED, PR_STATUS, 0)
TMD(REGEN_PET, PR_STATUS, 0)
TMD(UNSUMMON_MINIONS, PR_STATUS, 0)
TMD(BALANCED_STANCE, PR_STATUS, PU_BONUS)
TMD(DEFENSIVE_STANCE, PR_STATUS, PU_BONUS)
TMD(OFFENSIVE_STANCE, PR_STATUS, PU_BONUS)
TMD(SPEEDY, PR_STATUS, PU_BONUS)
TMD(FIERY_STANCE, PR_STATUS, PU_BONUS)
TMD(COLDY_STANCE, PR_STATUS, PU_BONUS)
TMD(ELECTRY_STANCE, PR_STATUS, PU_BONUS)
TMD(ACIDY_STANCE, PR_STATUS, PU_BONUS)
TMD(BLIND_REAL, PR_STATUS | PR_MAP | PR_FLOOR, PU_UPDATE_VIEW | PU_MONSTERS)
TMD(SLOW_REAL, PR_STATUS, PU_BONUS)
TMD(CONFUSED_REAL, PR_STATUS, PU_BONUS)
TMD(IMAGE_REAL, PR_STATUS | PR_MAP | PR_MONLIST | PR_ITEMLIST, PU_MONSTERS | PU_BONUS)
