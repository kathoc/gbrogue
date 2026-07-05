/* Monster stat table, banked (HOME has no room for const data).
   Accessed through mkind() in monsters.c via the far-copy trampoline. */
#include <gb/gb.h>
#include "../src/monsters.h"

#pragma bank 2

BANKREF(mkind_data)
const mkind_t MKIND_ROM[MKIND_COUNT] = {
    /* A aquator   */ {  5,  2,   20, MFL_MEAN,              {0,0, 0,0, 0,0} },
    /* B bat       */ {  1,  3,    1, MFL_FLY,               {1,2, 0,0, 0,0} },
    /* C centaur   */ {  4,  4,   17, 0,                     {1,2, 1,5, 1,5} },
    /* D dragon    */ { 10, -1, 5000, MFL_MEAN,              {1,8, 1,8, 3,10} },
    /* E emu       */ {  1,  7,    2, MFL_MEAN,              {1,2, 0,0, 0,0} },
    /* F flytrap   */ {  8,  3,   80, MFL_MEAN,              {0,0, 0,0, 0,0} },
    /* G griffin   */ { 13,  2, 2000, MFL_MEAN|MFL_FLY|MFL_REGEN, {4,3, 3,5, 0,0} },
    /* H hobgoblin */ {  1,  5,    3, MFL_MEAN,              {1,8, 0,0, 0,0} },
    /* I icemonstr */ {  1,  9,    5, 0,                     {0,0, 0,0, 0,0} },
    /* J jabberwok */ { 15,  6, 3000, 0,                     {2,12, 2,4, 0,0} },
    /* K kestrel   */ {  1,  7,    1, MFL_MEAN|MFL_FLY,      {1,4, 0,0, 0,0} },
    /* L leprechaun*/ {  3,  8,   10, 0,                     {1,1, 0,0, 0,0} },
    /* M medusa    */ {  8,  2,  200, MFL_MEAN,              {3,4, 3,4, 2,5} },
    /* N nymph     */ {  3,  9,   37, 0,                     {0,0, 0,0, 0,0} },
    /* O orc       */ {  1,  6,    5, MFL_GREEDY,            {1,8, 0,0, 0,0} },
    /* P phantom   */ {  8,  3,  120, MFL_INVIS,             {4,4, 0,0, 0,0} },
    /* Q quagga    */ {  3,  3,   15, MFL_MEAN,              {1,5, 1,5, 0,0} },
    /* R rattlesnk */ {  2,  3,    9, MFL_MEAN,              {1,6, 0,0, 0,0} },
    /* S snake     */ {  1,  5,    2, MFL_MEAN,              {1,3, 0,0, 0,0} },
    /* T troll     */ {  6,  4,  120, MFL_MEAN|MFL_REGEN,    {1,8, 1,8, 2,6} },
    /* U blk unicrn*/ {  7, -2,  190, MFL_MEAN,              {1,9, 1,9, 2,9} },
    /* V vampire   */ {  8,  1,  350, MFL_MEAN|MFL_REGEN,    {1,10, 0,0, 0,0} },
    /* W wraith    */ {  5,  4,   55, 0,                     {1,6, 0,0, 0,0} },
    /* X xeroc     */ {  7,  7,  100, 0,                     {4,4, 0,0, 0,0} },
    /* Y yeti      */ {  4,  6,   50, 0,                     {1,6, 1,6, 0,0} },
    /* Z zombie    */ {  2,  8,    6, MFL_MEAN,              {1,8, 0,0, 0,0} },
};
