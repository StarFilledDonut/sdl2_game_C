#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_rwops.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define GRAVITY 0.8f

typedef enum { COINS, MUSHROOM, FIRE_FLOWER, STAR } ItemType;

typedef enum { NOTHING, FULL, EMPTY } BlockType;

typedef struct {
  float x, y, dx, dy;
  u_short w, h;
  _Bool tall, firePower, invincible, onSurface, holdingJump, onJump;
  u_short frame;
} Character;

typedef struct {
  float x, y, dx, dy;
  u_short w, h;
  ItemType type;
  _Bool isFree, isVisible;
} Item;

typedef struct {
  SDL_Rect rect;
  float y, initY;
  _Bool gotHit;
  BlockType type;
  Item item;
  u_short frame;
} Block;

typedef struct {
  u_int w, h;
  u_short tile;
  float dt;
} Screen;

typedef struct {
  SDL_Texture *mario;
  SDL_Texture *objs;
  SDL_Texture *items;
  SDL_Texture *effects;
  SDL_Rect srcmario[20];
  SDL_Rect srcsobjs[4];
  SDL_Rect srcitems[20];
} Sheets;

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  Block blocks[20];
  SDL_Rect objs[20];
  Sheets sheets;
  Screen screen;
  Character player;
} GameState;

/* Destroy everything that was initialized from SDL then exit the program.
 * @param *state Your instance of GameState
 * @param __status The status shown after exting
 */
void quit(GameState *state, u_short __status) {
  Sheets *sheets = &state->sheets;
  if (sheets->mario) SDL_DestroyTexture(sheets->mario);
  if (state->renderer) SDL_DestroyRenderer(state->renderer);
  if (state->window) SDL_DestroyWindow(state->window);
  IMG_Quit();
  SDL_Quit();
  exit(__status);
}

// Remember to free the str after using it, needs state in case of an allocation
// error
// @return char * path + file
char *catpath(GameState *state, const char *path, const char *file) {
  char *result = malloc(strlen(path) + strlen(file) + 1);
  if (result == NULL) {
    printf("Could not allocate memory for the file path\n");
    quit(state, 1);
  }
  strcpy(result, path);
  strcat(result, file);
  return result;
}

// Get the srcs of the specific frames of a spritesheet.
// The row is an index, which starts at 0.
void getsrcs(
  SDL_Rect srcs[],
  const u_short frames,
  const u_short startIndex,
  const char row,
  const u_short w,
  const u_short h
) {
  const u_short tile = 16;
  u_int x = tile, y = tile * (row - 1);
  u_short j = startIndex;
  for (u_short i = 0; i < frames; i++) {
    SDL_Rect *frame = &srcs[j];
    frame->x = x * i;
    frame->y = y;
    frame->w = tile * w;
    frame->h = tile * h;
    j++;
  }
}

// Initialize the textures on the state.sheets, as well as the srcs.
void initTextures(GameState *state) {
  const char *path = "./assets/sprites/";
  Sheets *sheets = &state->sheets;
  char *files[] = {"mario.png", "objs.png", "items.png", "effects.png"};

  for (u_int i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
    char *filePath = catpath(state, path, files[i]);
    SDL_RWops *fileRW = SDL_RWFromFile(filePath, "r");
    if (!fileRW) {
      printf("Could not load the sprites! SDL_Error: %s\n", SDL_GetError());
      quit(state, 1);
    }
    SDL_Texture *fileTexture =
        IMG_LoadTextureTyped_RW(state->renderer, fileRW, 1, "PNG");
    if (i == 0) sheets->mario = fileTexture;
    else if (i == 1) sheets->objs = fileTexture;
    else if (i == 2) sheets->items = fileTexture;
    else if (i == 3) sheets->effects = fileTexture;
    if (!fileTexture) {
      printf("Could not place the sprites! SDL_Error: %s\n", SDL_GetError());
      quit(state, 1);
    }
    free(filePath);
  }
  getsrcs(sheets->srcmario, 7, 0, 1, 1, 1);
  getsrcs(sheets->srcmario, 7, 7, 2, 1, 2);
  getsrcs(sheets->srcsobjs, 4, 0, 1, 1, 1);
  getsrcs(sheets->srcitems, 6, 0, 1, 1, 1);
}

void initObjs(GameState *state) {
  Block *blocks = state->blocks;
  Screen *screen = &state->screen;
  u_short tile = screen->tile;
  blocks[0].rect.x = screen->w / 2.0f - screen->tile;
  blocks[0].y = screen->h - tile * 5;
  blocks[0].initY = blocks[0].y;
  blocks[0].gotHit = false;
  blocks[0].type = FULL;
  blocks[0].item.x = blocks[0].rect.x;
  blocks[0].item.y = blocks[0].y;
  blocks[0].item.w = tile;
  blocks[0].item.h = tile;
  blocks[0].item.isFree = false;
  blocks[0].item.isVisible = true;
  blocks[0].item.type = MUSHROOM;
  blocks[0].frame = 3;

  blocks[1].y = screen->h - tile * 3;
  blocks[1].rect.w = tile;
  blocks[1].rect.h = tile;
  blocks[1].rect.x = tile;
  blocks[1].rect.y = blocks[1].y;
  blocks[1].initY = blocks[1].y;
  blocks[1].gotHit = false;
  blocks[1].type = NOTHING;
}

void initGame(GameState *state) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("Could not initialize SDL! SDL_Error: %s\n", SDL_GetError());
    exit(1);
  }
  if (IMG_Init(IMG_INIT_PNG) < 0) {
    printf("Could not initialize IMG! IMG_Error: %s\n", SDL_GetError());
    exit(1);
  }
  state->screen.w = 640; // For later screen resizing
  state->screen.h = 480;
  state->screen.tile = 64;
  state->screen.dt = 0; // DeltaTime

  SDL_Window *window = SDL_CreateWindow(
      "Mario copy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      state->screen.w, state->screen.h, SDL_WINDOW_SHOWN);
  if (!window) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }
  state->window = window;

  SDL_Renderer *renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
    quit(state, 1);
  }
  state->renderer = renderer;

  Character player;
  player.w = state->screen.tile;
  player.h = state->screen.tile;
  player.x = state->screen.w / 2.0f - player.w;
  player.y = state->screen.h - player.h - state->screen.tile * 2;
  player.dx = 0;
  player.dy = 0;
  player.tall = false;
  player.firePower = false;
  player.invincible = false;
  player.onSurface = false;
  player.holdingJump = false;
  player.onJump = false;
  player.frame = 0;
  state->player = player;
  initTextures(state);
  initObjs(state);
}

// Takes care of all the events of the game.
void handleEvents(GameState *state) {
  SDL_Event event;
  Character *player = &state->player;
  const short MAX_JUMP = -15, MAX_SPEED = 7;
  const float JUMP_FORCE = 2.5f, SPEED = 0.2f, FRIC = 0.85f;

  if (player->onSurface) player->onJump = false;

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT | SDL_WINDOWEVENT_CLOSE:
      quit(state, 0);
      break;
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_ESCAPE:
        quit(state, 0);
        break;
      }
    case SDL_KEYUP:
      if (event.key.repeat == 0) {
        const SDL_Keycode keyup = event.key.keysym.sym;
        if (keyup == SDLK_SPACE || keyup == SDLK_w || keyup == SDLK_UP) {
          if (player->dy < 0)
            player->dy *= FRIC;
          player->holdingJump = false;
          player->onJump = false;
        }
      }
    }
  }
  const Uint8 *key = SDL_GetKeyboardState(NULL);
  if (key[SDL_SCANCODE_LEFT] || key[SDL_SCANCODE_A]) {
    if (player->dx > 0) player->dx *= FRIC;
    if (player->dx > MAX_SPEED * -1) player->dx -= SPEED;
  } else if (key[SDL_SCANCODE_RIGHT] || key[SDL_SCANCODE_D]) {
    if (player->dx < 0) player->dx *= FRIC;
    if (player->dx < MAX_SPEED) player->dx += SPEED;
  } else {
    if (player->dx) {
      player->dx *= FRIC;
      if (fabsf(player->dx) < 0.1f) player->dx = 0;
    }
  }
  if (((!player->holdingJump && player->onSurface) ||
       (!player->onSurface && player->onJump)) &&
      (key[SDL_SCANCODE_SPACE] || key[SDL_SCANCODE_W] ||
       key[SDL_SCANCODE_UP])) {
    player->dy -= JUMP_FORCE;
    if (player->dy < MAX_JUMP) player->onJump = false;
    else player->onJump = true;
    player->holdingJump = true;
  }
  // NOTES: TEMPORARY CEILING
  if (player->y < 0) player->y = 0;
}

// Handles the collision a axis per time, call this first with dx only,
// then call it again for the dy.
// This function will only run for things that are displayed on screen.
void handleCollision(
  float dx,
  float dy,
  GameState *state,
  const u_int blength,
  const u_int objslength
) {
  Character *player = &state->player;
  Block *blocks = state->blocks;
  const u_short pw = player->w, ph = player->h;
  _Bool onBlock = false, onObj = false;
  float *px = &player->x, *py = &player->y;
  const float BLOCK_SPEED = 1.5f;

  for (u_int i = 0; i < blength; i++) {
    Item *item = &blocks[i].item;
    _Bool *gotHit = &blocks[i].gotHit, *isVisible = &item->isVisible;
    const BlockType blockType = blocks[i].type;
    const ItemType itemType = item->type;
    float ix = item->x, iy = item->y;
    // NOTES: Don't use these if inside == NOTHING
    const u_short iw = item->w, ih = item->h;

    SDL_Rect brect = blocks[i].rect;
    float bx = brect.x, by = blocks[i].y;
    const u_short bw = brect.w, bh = brect.h;

    if (*isVisible && item->isFree) {
      const _Bool itemCollision =
          (*px<ix + iw && * px + pw> ix) && (*py<iy + ih && * py + ph> iy);
      if (itemCollision) {
        *isVisible = false;
        if (itemType == MUSHROOM && !player->firePower) player->tall = true;
        else if (itemType == FIRE_FLOWER) player->firePower = true;
        else if (itemType == STAR) player->invincible = true;
      }
    }

    const _Bool blockOffScreen = (bx + bw < 0 || bx > state->screen.w) ||
                                 (by + bh < 0 || by > state->screen.h);
    if (blockOffScreen) continue;

    const _Bool blockCollision =
        (*px<bx + bw && * px + pw> bx) && (*py<by + bh && * py + ph> by);
    const float initY = blocks[i].initY;

    if (blockCollision) {
      if (dx > 0) *px = bx - pw;
      else if (dx < 0) *px = bx + bw;
      else if (dy > 0) {
        *py = by - ph;
        onBlock = true;
      } else if (dy < 0) {
        *py = by + bh;
        if ((!player->tall && blockType == NOTHING) ||
            (blockType == FULL))
          *gotHit = true;
        // TODO: Add a mechanic to type == COINS
        // to only become empty after 10 coins/hits
        if (itemType > COINS) {
          item->isFree = true;
          blocks[i].frame = 2;
        }
      }
      if (dx) player->dx = 0;
      else player->dy = 0;
    }
    if (*gotHit) {
      const float bjump = initY + bh - state->screen.tile / 4.0f;
      if (by + bh > bjump) blocks[i].y -= BLOCK_SPEED;
      else *gotHit = false;
    } else if (by != initY) blocks[i].y += BLOCK_SPEED;
    if (itemType > COINS && item->isFree) {
      if (item->y > initY - state->screen.tile) item->y -= BLOCK_SPEED;
      else blocks[i].type = EMPTY;
    }
  }
  for (u_int i = 0; i < objslength; i++) {
    SDL_Rect obj = state->objs[i];
    const _Bool collision = (*px<obj.x + obj.w && * px + pw> obj.x) &&
                            (*py<obj.y + obj.h && * py + ph> obj.y);
    const _Bool objOffScreen = (obj.x + obj.w < 0 || obj.x > state->screen.w) ||
                               (obj.y + obj.h < 0 || obj.y > state->screen.h);
    if (objOffScreen) continue;

    if (collision) {
      if (dx > 0) *px = obj.x - pw;
      else if (dx < 0) *px = obj.x + obj.w;
      else if (dy > 0) {
        *py = obj.y - ph;
        onObj = true;
      } else if (dy < 0) *py = obj.y + obj.h;
      if (dx) player->dx = 0;
      else player->dy = 0;
    }
  }
  player->onSurface = onBlock || onObj;
}

// Apply physics to the player, the objects, and the eneyms.
void physics(GameState *state) {
  Character *player = &state->player;
  const float *dt = &state->screen.dt;
  const float MAX_GRAVITY = 20, TARGET_FRAMERATE = 60,
              MAX_DELTA_TIME = 1 / TARGET_FRAMERATE;
  const u_int blength = 2;    // NOTES: Update these lengths accordingly
  const u_int objsLength = 6;

  if (*dt && *dt < MAX_DELTA_TIME)
    player->x += player->dx * TARGET_FRAMERATE * *dt;
  else
    player->x += player->dx * TARGET_FRAMERATE * MAX_DELTA_TIME;
  handleCollision(player->dx, 0, state, blength, objsLength);

  if (player->dy < MAX_GRAVITY && *dt && *dt < MAX_DELTA_TIME) {
    player->dy += GRAVITY * TARGET_FRAMERATE * *dt;
    player->y += player->dy * TARGET_FRAMERATE * *dt;
  } else {
    player->dy += GRAVITY * TARGET_FRAMERATE * MAX_DELTA_TIME;
    player->y += player->dy * TARGET_FRAMERATE * MAX_DELTA_TIME;
  }
  handleCollision(0, player->dy, state, blength, objsLength);

  // NOTES: Placeholder code below, prevent from falling into endeless pit
  if (player->y - player->h > state->screen.h) {
    player->y = (float)0 - player->h;
    player->x = state->screen.h / 2.0f - player->w;
  }
}

// Handles animations and wich frames all moving parts of the game to be in.
void handleFrames(GameState *state) {
  Character *player = &state->player;
  const u_short tile = state->screen.tile;
  const _Bool isSmall = !player->tall && !player->firePower;
  // NOTES: This enum is defined here cause it does not needs global scope for now
  typedef enum {
    STILL,
    WALK_1,
    WALK_2,
    WALK_3,
    TURNING,
    JUMP,
    DYING,
    TALL_STILL,
    TALL_WALK_1,
    TALL_WALK_2,
    TALL_WALK_3,
    TALL_TURNING,
    TALL_JUMPING,
    TALL_SQUATTING
  } Sprites;
  if (player->tall || player->firePower) player->h = tile * 2;
  else player->h = tile;

  if (isSmall && player->holdingJump)
    player->frame = JUMP;
  else if (!player->firePower && player->holdingJump)
    player->frame = TALL_JUMPING;
  else if (isSmall && player->onSurface)
    player->frame = STILL;
  else if (!player->firePower && player->onSurface)
    player->frame = TALL_STILL;
}

// Renders to the screen
void render(GameState *state) {
  handleFrames(state);
  Character *player = &state->player;
  Sheets *sheets = &state->sheets;
  Screen *screen = &state->screen;
  Block *blocks = state->blocks;
  u_short tile = screen->tile;

  SDL_SetRenderDrawColor(state->renderer, 92, 148, 252, 255);
  SDL_RenderClear(state->renderer);

  SDL_SetRenderDrawColor(state->renderer, 255, 0, 0, 255);
  // NOTES: Delmiter of the bottom of the screen
  SDL_RenderDrawLine(state->renderer, 0, screen->h, screen->w, screen->h);

  SDL_Rect dstplayer = {player->x, player->y, player->w, player->h};
  SDL_Rect dstblock = {blocks[0].rect.x, blocks[0].y, tile, tile};
  blocks[0].rect = dstblock;
  Item *item = &blocks[0].item;
  SDL_Rect dstitem = {item->x, item->y, item->w, item->h};

  SDL_Rect srcground = {0, 16, 32, 32};
  SDL_Rect dstground = {0, screen->h - tile * 2, tile * 2, tile * 2};

  if (item->isVisible)
    SDL_RenderCopy(state->renderer, sheets->items, &sheets->srcitems[0], &dstitem);
  SDL_RenderCopy(state->renderer, sheets->mario, &sheets->srcmario[player->frame], &dstplayer);
  SDL_RenderCopy(state->renderer, sheets->objs, &sheets->srcsobjs[blocks[0].frame], &blocks[0].rect);
  SDL_RenderCopy(state->renderer, sheets->objs, &sheets->srcsobjs[1], &blocks[1].rect);
  for (u_int i = 0; i < 6; i++) {
    state->objs[i] = dstground;
    SDL_RenderCopy(state->renderer, sheets->objs, &srcground, &dstground);
    dstground.x += dstground.w;
  }

  SDL_RenderPresent(state->renderer);
}

int main(void) {
  GameState state;
  initGame(&state);
  Uint32 currentTime = SDL_GetTicks(), lastTime = 0;

  while (true) {
    lastTime = currentTime;
    currentTime = SDL_GetTicks();
    state.screen.dt = (currentTime - lastTime) / 1000.0f;
    handleEvents(&state);
    physics(&state);
    render(&state);
  }
}
// TODO: Change to fire Mario when firePower == true
// TODO: Make mushroom and star to move arround
