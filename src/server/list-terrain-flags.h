/*
 * File: list-terrain-flags.h
 * Purpose: Terrain flags
 */

/* symbol descr */
TF(NONE, "")
TF(LOS, "Allows line of sight")
TF(PROJECT, "Allows projections to pass through")
TF(PASSABLE, "Can be passed through by all creatures")
TF(INTERESTING, "Is noticed on looking around")
TF(PERMANENT, "Is permanent")
TF(EASY, "Is easily passed through")
TF(TRAP, "Can hold a trap")
TF(NO_SCENT, "Cannot store scent")
TF(NO_FLOW, "No flow through")
TF(OBJECT, "Can hold objects")
TF(TORCH, "Becomes bright when torch-lit")
/*TF(HIDDEN, "Can be found by searching")*/
TF(GOLD, "Contains treasure")
TF(CLOSABLE, "Can be closed")
TF(FLOOR, "Is a clear floor")
TF(WALL, "Is a solid wall")
TF(ROCK, "Is rocky")
TF(GRANITE, "Is a granite rock wall")
TF(DOOR_ANY, "Is any door")
TF(DOOR_CLOSED, "Is a closed door")
TF(SHOP, "Is a shop")
/*TF(DOOR_JAMMED, "Is a jammed door")*/
TF(DOOR_LOCKED, "Is a locked door")
TF(MAGMA, "Is a magma seam")
TF(QUARTZ, "Is a quartz seam")
TF(STAIR, "Is a stair")
TF(UPSTAIR, "Is an up staircase")
TF(DOWNSTAIR, "Is a down staircase")
TF(SMOOTH, "Should have smooth boundaries")
TF(BRIGHT, "Is internally lit")
TF(FIERY, "Is fire-based")

/* PWMAngband */
TF(FLOOR_SAFE, "Is safe floor")
TF(FLOOR_OTHER, "Is other floor")
TF(BORING, "Is boring")
TF(PIT, "Part of a pit")
TF(BORDER, "Is a border wall")
TF(ARENA, "Is an arena wall")
TF(DOOR_HOME, "Is a home door")
TF(TREE, "Is a tree")
TF(WITHERED, "Is withered")
TF(DIRT, "Is dirt")
TF(GRASS, "Is grass")
TF(CROP, "Is crop")
TF(WATER, "Is water")
TF(LAVA, "Is lava")
TF(MOUNTAIN, "Is mountain")
TF(FOUNTAIN, "Is a fountain")
TF(DRIED, "Is dried out")
TF(NOTICEABLE, "Is noticed when pathfinding")
TF(PLOT, "Part of a plot")
TF(METAMAP, "Only displayed on the metamap")
TF(SAND, "Is a sand wall")
TF(ICE, "Is an ice wall")
TF(WEB, "Is web")
TF(DARK, "Is a dark wall")
TF(NETHER, "Is nether mist")
TF(NO_STAIRS, "Cannot bear stairs")
TF(NO_GENERATION, "Prevent monster or object generation")
TF(CROP_BASE, "Is base terrain for crops")
TF(NO_HOUSE, "Building houses not allowed")
TF(HOUSE, "Is house")
TF(WINDOW, "Is window")
TF(SHALLOW_WATER, "Is shallow water")
TF(BAD_WATER, "Is muddy water")
TF(FOUL_WATER, "Is water.. or even kinda strange liquid with foul smell")
TF(T_FARM_FIELD, "Is farm field")
