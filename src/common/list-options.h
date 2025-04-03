/*
 * File: list-options.h
 * Purpose: Options
 */

/* normal - whether the option is ON by default */
/* server - whether the option is relevant for server 
(note: it doesn't work for hardcoded stuff, eg birth_no_ghost */

/* name  description  type  normal  server */
OP(none, "", MAX, false, false)
OP(rogue_like_commands, "Use the roguelike command keyset", INTERFACE, false, true)
OP(use_old_target, "Use old target by default", INTERFACE, false, true)
OP(pickup_always, "Always pickup items", INTERFACE, false, true)
OP(pickup_inven, "Always pickup items matching inventory", INTERFACE, true, true)
OP(notify_recharge, "Notify on object recharge", INTERFACE, false, true)
OP(show_flavors, "Show flavors in object descriptions", INTERFACE, false, true)
OP(center_player, "Center map continuously", INTERFACE, false, true)
OP(disturb_near, "Disturb whenever viewable monster moves", INTERFACE, true, true)
OP(show_damage, "Show damage player deals to monsters", INTERFACE, false, true)
OP(view_yellow_light, "Color: Illuminate torchlight in yellow", INTERFACE, false, true)
OP(animate_flicker, "Color: Shimmer multi-colored things", INTERFACE, false, true)
OP(hp_changes_color, "Color: Player color indicates % hit points", INTERFACE, true, true)
OP(purple_uniques, "Color: Show unique monsters in purple", INTERFACE, false, true)
OP(solid_walls, "Show walls as solid blocks", INTERFACE, false, true)
OP(hybrid_walls, "Show walls with shaded background", INTERFACE, false, true)
OP(use_sound, "Use sound", INTERFACE, true, false)
OP(effective_speed, "Show effective speed as multiplier", INTERFACE, false, true)
OP(view_orange_light, "Color: Illuminate torchlight in orange", MANGBAND, false, true)
OP(highlight_leader, "Use special color for party leader", MANGBAND, false, true)
OP(disturb_panel, "Disturb whenever map panel changes", MANGBAND, true, true)
OP(auto_accept, "Always say Yes to Yes/No prompts", MANGBAND, false, false)
OP(pause_after_detect, "Freeze screen after detecting monsters", MANGBAND, true, true)
OP(wrap_messages, "Wrap long messages in sub-windows", MANGBAND, false, false)
OP(expand_inspect, "Compare equipment when examining items", MANGBAND, false, true)
OP(birth_ironman, "Ironman: zeitnot no-recall force descent", BIRTH, false, true)
OP(birth_force_descend, "Force player descent", BIRTH, false, true)
OP(birth_no_recall, "Word of Recall has no effect", BIRTH, false, true)
OP(birth_no_artifacts, "Restrict creation of artifacts", BIRTH, false, true)
OP(birth_feelings, "Show level feelings", BIRTH, true, true)
OP(birth_no_selling, "---- (cannot be disabled: More $ drop, no selling)", BIRTH, true, true)
OP(birth_start_kit, "Start with a kit of useful gear", BIRTH, true, true)
OP(birth_no_stores, "Restrict the use of stores/home", BIRTH, false, true)
OP(birth_no_ghost, "---- (cannot be disabled: Death is permanent)", BIRTH, true, true)
OP(birth_fruit_bat, "Play as a fruit bat", BIRTH, false, true)
OP(birth_hardcore, "HC: no extra gold, 2ndChance & low-HP heal", BIRTH, false, true)
OP(disturb_icky, "Get out of icky screens when disturbed", ADVANCED, false, true)
OP(active_auto_retaliator, "Active auto-retaliator", ADVANCED, true, true)
OP(disturb_bash, "Disturb whenever monsters bash down doors", ADVANCED, true, true)
OP(fire_till_kill, "Activate fire-till-kill mode", ADVANCED, false, true)
OP(risky_casting, "Risky casting", ADVANCED, false, true)
OP(quick_floor, "Use single items from floor instantly", ADVANCED, false, false)
OP(hide_terrain, "Hide terrain description on status line", ADVANCED, false, true)
OP(disable_enter, "Disable Enter menu", ADVANCED, false, false)
OP(sort_exp, "Sort monsters by experience", ADVANCED, false, true)
OP(ascii_mon, "Display monsters in ASCII", ADVANCED, false, true)
OP(disturb_nomove, "Nonmoving monsters disturb running", ADVANCED, true, true)
OP(disturb_effect_end, "Disturb when effects end", ADVANCED, true, true)
OP(confirm_recall, "Confirm recall out of non-reentrable dungeons", ADVANCED, true, true)
OP(hide_slaves, "Hide controlled monsters in monster lists", ADVANCED, false, true)
OP(highlight_players, "Highlight players on mini map", ADVANCED, true, true)
OP(slash_fx, "Show monsters attack (Graphical mode)", EXTRA, true, false)
OP(slash_fx_ascii, "Show monsters attack (ASCII mode)", EXTRA, true, false)
OP(animations, "Animations", EXTRA, true, false)
OP(weather_display, "Show weather", EXTRA, true, false)
