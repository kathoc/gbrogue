#include <gb/gb.h>
#include "render.h"
#include "input.h"
#include "game.h"
#include "farcopy.h"
#include "sfx.h"
#include "bgm.h"

void main(void) {
    farcopy_init();
    render_init();
    input_init();
    sfx_init();
    game_run();
}
