#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include "raylib.h"
#include "stretchy_buffer.h"


struct Sprite {
    int type;
    Texture2D texture;
    Vector2 origin;
    Rectangle src_rect, dest_rect;
    Vector2 velocity;
    float rotation;
    float rotation_delta;
    float scale;
    Color tint;
};

Texture2D* loaded_textures = nullptr;
Sprite* sprites = nullptr;

int LoadIndexedTexture(const char* filename) {
    sb_push(loaded_textures, LoadTexture(filename));
    return sb_count(loaded_textures) - 1;
}

Sprite CreateSprite(int texture_idx) {
    Sprite spr = Sprite();
    spr.texture = loaded_textures[texture_idx];
    spr.type = texture_idx;
    spr.origin = { .x = spr.texture.width / 2.f, .y = spr.texture.height / 2.f };
    spr.velocity = { .x = 0.f, .y = 0.f };
    spr.rotation = 0.f;
    spr.rotation_delta = 0.f;
    spr.scale = 1.f;
    spr.src_rect = { .x = 0.f, .y = 0.f, .width = (float) spr.texture.width, .height = (float) spr.texture.height };
    spr.dest_rect = { .x = 0.f, .y = 0.f, .width = (float) spr.texture.width, .height = (float) spr.texture.height };
    spr.tint = WHITE;
    return spr;
}

int AddSprite(Sprite sprite) {
    for(int i = 0; i < sb_count(sprites); i++) {
        if(sprites[i].type < 0) {
            sprites[i] = sprite;
            return i;
        }
    }
    sb_push(sprites, sprite);
    return sb_count(sprites) - 1;
}

int ExplodeSprite(int old_idx, int explosion_texture_idx) {
    int new_idx = AddSprite(CreateSprite(explosion_texture_idx));
    sprites[new_idx].dest_rect.x = sprites[old_idx].dest_rect.x;
    sprites[new_idx].dest_rect.y = sprites[old_idx].dest_rect.y;
    sprites[new_idx].rotation = (float) GetRandomValue(0, 360);
    sprites[new_idx].rotation_delta = 90.f;
    sprites[new_idx].tint = { .r=208, .g=255, .b=208, .a=255 };
    sprites[old_idx].type *= -1;
    return new_idx;
}


Sound* loaded_sounds = nullptr;
int LoadIndexedSound(const char* filename, const float volume) {
    Sound snd = LoadSound(filename);
    SetSoundVolume(snd, volume);
    sb_push(loaded_sounds, snd);
    return sb_count(loaded_sounds) - 1;
}

void PlayIndexedSound(int sound_idx) {
    if(IsAudioDeviceReady()) {
        PlaySoundMulti(loaded_sounds[sound_idx]);
    }
}


int main() {
    const int WND_W = 600;
    const int WND_H = 600;
    const float WND_DIAM = sqrtf((WND_W * WND_W) + (WND_H * WND_H));

    const Color COLOR_BACKGROUND = { .r = 0x25, .g = 0x2e, .b = 0x34, .a = 0xff };
    const Color COLOR_MOUSE_TARGET = { .r = 253, .g = 249, .b = 0, .a = 96 };

    const int STATE_TITLE = 100;
    const int STATE_TITLE_FADE = 110;
    const int STATE_PLAYING = 200;
    const int STATE_IS_RUNNING = 1000;
    const int STATE_END_ZOOM = STATE_IS_RUNNING + 300;
    const int STATE_END_FADE = STATE_IS_RUNNING + 400;
    const int STATE_END_CHOICE = STATE_IS_RUNNING + 500;

    SetTraceLogLevel(LOG_ERROR);
    InitWindow(WND_W, WND_H, "Solar Commander  < Ludum Dare 46 >");
    InitAudioDevice();
    SetTargetFPS(60);

    TraceLog(LOG_INFO, "Current directory: %s", GetWorkingDirectory());
    const int TEXTURE_IDX_SUN = LoadIndexedTexture("assets/sun.png");
    const int TEXTURE_IDX_EARTH = LoadIndexedTexture("assets/earthwithclouds.png");
    const int TEXTURE_IDX_FLARE = LoadIndexedTexture("assets/flare.png");
    const int TEXTURE_IDX_ASTEROID = LoadIndexedTexture("assets/icyasteroid.png");
    const int TEXTURE_IDX_SCORCHED = LoadIndexedTexture("assets/scorchedearth.png");
    const int TEXTURE_IDX_EXPLOSION = LoadIndexedTexture("assets/explosion.png");
    TraceLog(LOG_INFO, "Loaded %d textures\n", sb_count(loaded_textures));

    Sprite sun_sprite = CreateSprite(TEXTURE_IDX_SUN);
    sun_sprite.dest_rect.x = WND_W / 2.f;
    sun_sprite.dest_rect.y = WND_H / 2.f;
    Sprite earth_sprite = CreateSprite(TEXTURE_IDX_EARTH);

    const int SOUND_IDX_START = LoadIndexedSound("assets/start_1.wav", 1.f);
    const int SOUND_IDX_EXPL_1 = LoadIndexedSound("assets/explosion_1.wav", 0.8f);
    const int SOUND_IDX_EXPL_2 = LoadIndexedSound("assets/explosion_2.wav", 0.8f);
    const int SOUND_IDX_EXPL_3 = LoadIndexedSound("assets/explosion_3.wav", 0.8f);
    const int SOUND_IDX_FLARE = LoadIndexedSound("assets/flare.wav", 0.8f);
    const int SOUND_IDX_SCORCHED_ASTEROID = LoadIndexedSound("assets/scorched_asteroid.wav", 0.8f);
    const int SOUND_IDX_SCORCHED_FLARE = LoadIndexedSound("assets/scorched_flare.wav", 0.8f);
    const int SOUND_IDX_END = LoadIndexedSound("assets/end_3.wav", 1.f);
    const int SOUND_EXPL_IDXS[] = { SOUND_IDX_EXPL_1, SOUND_IDX_EXPL_2, SOUND_IDX_EXPL_3 };

    const int STAR_COUNT = 100;
    Vector3 stars[STAR_COUNT];
    for(int i = 0; i < STAR_COUNT; i++) {
        // .x, .y are position, .z is velocity
        stars[i].x = (float) GetRandomValue(0, WND_W-1);
        stars[i].y = (float) GetRandomValue(0, WND_H-1);
        stars[i].z = (float) GetRandomValue(1, 3) / 2.f;
    }

    const float sun_rotation_delta = 15.f;      // Degrees per second
    const float earth_revolve_delta = 18.f;     // Degrees per second
    const float earth_revolve_radius = 180.f;   // Pixels
    float earth_revolve_angle = 45.f;           // Degrees starting position
    const double earth_revolve_time = 360. / earth_revolve_delta;

    const int mouse_init_x = GetMouseX();
    const int mouse_init_y = GetMouseY();
    bool mouse_has_moved = false;
    float mouse_target_x = 0.f;
    float mouse_target_y = 0.f;

    const float title_fade_delta = 128.f;       // Alpha per second
    float title_fade_alpha = 255.f;
    double playing_start_time = 0.;
    double earth_revolve_count = 0.;            // Presented as "years"
    double max_earth_revolve_count = 0.;
    const float flare_speed = 250.f;            // Pixels per second
    const float explosion_fade_delta = 255.f;   // Alpha per second

    const float ambient_asteroid_speed = 35.f;
    const float targeted_asteroid_speed = 100.f;
    float add_targeted_asteroid_time = 0.f;
    float add_ambient_asteroid_time = 0.f;
    const Color target_asteroid_tint = { 255, 208, 208, 255 };



    const float end_zoom_period = 4.f;
    const float end_zoom_scale_target = 4.f;
    const float end_zoom_scale_delta = end_zoom_scale_target / end_zoom_period;
    float end_zoom_earth_target_x = 0.f;
    float end_zoom_earth_target_y = 0.f;

    const float end_fade_delta = 96.f;         // Alpha per second
    float end_fade_alpha = 0.f;
    char* end_message = nullptr;



    // For collisions
    Vector2 sun_pos = { .x = sun_sprite.dest_rect.x, .y = sun_sprite.dest_rect.y };
    float sun_radius = sun_sprite.dest_rect.width / 3.f;
    Vector2 earth_pos;
    float earth_radius;

    int current_state = STATE_TITLE;
    while(!WindowShouldClose()) {
        const float frame_time = GetFrameTime();

        // Update inputs, Earth, Sun
        if(current_state <= STATE_IS_RUNNING) {
            // Update mouse targeting; do this before handling mouse input
            float mouse_x = (float) GetMouseX();
            float mouse_y = (float) GetMouseY();
            if(!mouse_has_moved && ((int) mouse_x != mouse_init_x || (int) mouse_y != mouse_init_y)) {
                mouse_has_moved = true;
            }
            float mouse_angle = RAD2DEG * atan2f(mouse_y - sun_sprite.dest_rect.y, mouse_x - sun_sprite.dest_rect.x);
            mouse_target_x = (WND_DIAM * cosf(DEG2RAD * mouse_angle)) + sun_sprite.dest_rect.x;
            mouse_target_y = (WND_DIAM * sinf(DEG2RAD * mouse_angle)) + sun_sprite.dest_rect.y;

            // Handle mouse clicks
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if(current_state == STATE_TITLE) {
                    current_state = STATE_TITLE_FADE;
                    title_fade_alpha = 255.f;
                }
                int idx = AddSprite(CreateSprite(TEXTURE_IDX_FLARE));
                sprites[idx].dest_rect.x = sun_sprite.dest_rect.x;
                sprites[idx].dest_rect.y = sun_sprite.dest_rect.y;
                sprites[idx].velocity.x = cosf(DEG2RAD * mouse_angle) * flare_speed;
                sprites[idx].velocity.y = sinf(DEG2RAD * mouse_angle) * flare_speed;
                sprites[idx].rotation = mouse_angle + 90.f;
                // TraceLog(LOG_INFO, " -- added flare idx=%d, pos=(%d, %d), vel=[%.2f,%.2f], rotation=%d",
                //          idx, (int) sprites[idx].dest_rect.x, (int) sprites[idx].dest_rect.y,
                //          sprites[idx].velocity.x, sprites[idx].velocity.y, (int) sprites[idx].rotation);
                PlayIndexedSound(SOUND_IDX_FLARE);
            }

            // Update the stars
            for(int i = 0; i < STAR_COUNT; i++) {
                stars[i].x -= stars[i].z;
                if(stars[i].x < 0.f) {
                    stars[i].x = (float) WND_W;
                    stars[i].y = (float) GetRandomValue(0, WND_H-1);
                    stars[i].z = (float) GetRandomValue(1, 3) / 2.f;
                }
            }

            // Update Earth revolution
            earth_revolve_angle -= earth_revolve_delta * frame_time;
            if(earth_revolve_angle < 0.f) { earth_revolve_angle += 360.f; }
            earth_sprite.dest_rect.x = (earth_revolve_radius * cosf(DEG2RAD * earth_revolve_angle)) + sun_sprite.dest_rect.x;
            earth_sprite.dest_rect.y = (earth_revolve_radius * sinf(DEG2RAD * earth_revolve_angle)) + sun_sprite.dest_rect.y;
            earth_pos.x = earth_sprite.dest_rect.x;
            earth_pos.y = earth_sprite.dest_rect.y;
            earth_radius = earth_sprite.dest_rect.width / 4.f;
            if(current_state == STATE_PLAYING) {
                earth_revolve_count = (GetTime() - playing_start_time) / earth_revolve_time;
                if(earth_revolve_count > max_earth_revolve_count) {
                    max_earth_revolve_count = earth_revolve_count;
                }
            }

            // Update Sun rotation
            sun_sprite.rotation += sun_rotation_delta * frame_time;
            if(sun_sprite.rotation > 360.f) { sun_sprite.rotation -= 360.f; }
        }

        // Update title fade
        if(current_state == STATE_TITLE_FADE) {
            title_fade_alpha -= title_fade_delta * frame_time;
            if(title_fade_alpha <= 0.f) {
                current_state = STATE_PLAYING;
                PlayIndexedSound(SOUND_IDX_START);
                playing_start_time = GetTime();
                add_ambient_asteroid_time = 0.f;
                add_targeted_asteroid_time = 0.5f;
            }
        }

        // Update asteroids in play
        if(current_state == STATE_PLAYING) {
            add_ambient_asteroid_time -= frame_time;
            if(add_ambient_asteroid_time <= 0.f) {
                int idx = AddSprite(CreateSprite(TEXTURE_IDX_ASTEROID));
                int side = GetRandomValue(0, 3);
                float start_x = 0, start_y = 0, angle = 0;
                if(side == 0) {  // LEFT
                    start_x = 0.f;
                    start_y = (float) GetRandomValue(0, WND_H);
                    angle = (float) GetRandomValue(-80, 80);
                } else if(side == 1) {  // TOP
                    start_x = (float) GetRandomValue(0, WND_W);
                    start_y = 0.f;
                    angle = (float) GetRandomValue(170, 10);
                } else if(side == 2) {  // RIGHT
                    start_x = WND_W;
                    start_y = (float) GetRandomValue(0, WND_H);
                    angle = (float) GetRandomValue(100, 260);
                } else {  // BOTTOM
                    start_x = (float) GetRandomValue(0, WND_W);
                    start_y = WND_H;
                    angle = (float) GetRandomValue(-10, -170);
                }
                sprites[idx].dest_rect.x = start_x;
                sprites[idx].dest_rect.y = start_y;
                sprites[idx].velocity.x = cosf(DEG2RAD * angle) * ambient_asteroid_speed;
                sprites[idx].velocity.y = sinf(DEG2RAD * angle) * ambient_asteroid_speed;
                sprites[idx].rotation = (float) GetRandomValue(0, 360);
                sprites[idx].rotation_delta = (float) GetRandomValue(30, 50);
                add_ambient_asteroid_time = earth_revolve_time / (float) ((int) earth_revolve_count + 7);
                // TraceLog(LOG_INFO, " -- added amb ast idx=%d, side=%d, pos=(%d, %d), angle=%d, vel=[%.2f, %.2f], rotation=%d, next_in=%d",
                //          idx, side, (int) sprites[idx].dest_rect.x, (int) sprites[idx].dest_rect.y, (int) angle,
                //          sprites[idx].velocity.x, sprites[idx].velocity.y, (int) sprites[idx].rotation, (int) add_ambient_asteroid_time);
            }

            add_targeted_asteroid_time -= frame_time;
            if(add_targeted_asteroid_time <= 0.f) {
                int idx = AddSprite(CreateSprite(TEXTURE_IDX_ASTEROID));
                int side = GetRandomValue(0, 3);
                float start_x, start_y;
                if(side == 0) {  // LEFT
                    start_x = 0.f;
                    start_y = (float) GetRandomValue(0, WND_H);
                } else if(side == 1) {  // TOP
                    start_x = (float) GetRandomValue(0, WND_W);
                    start_y = 0.f;
                } else if(side == 2) {  // RIGHT
                    start_x = WND_W;
                    start_y = (float) GetRandomValue(0, WND_H);
                } else {  // BOTTOM
                    start_x = (float) GetRandomValue(0, WND_W);
                    start_y = WND_H;
                }

                float angle_to_earth = RAD2DEG * atan2f(earth_sprite.dest_rect.y - start_y, earth_sprite.dest_rect.x - start_x);
                sprites[idx].dest_rect.x = start_x;
                sprites[idx].dest_rect.y = start_y;
                sprites[idx].velocity.x = cosf(DEG2RAD * angle_to_earth) * targeted_asteroid_speed;
                sprites[idx].velocity.y = sinf(DEG2RAD * angle_to_earth) * targeted_asteroid_speed;
                sprites[idx].rotation = (float) GetRandomValue(0, 360);
                sprites[idx].rotation_delta = (float) GetRandomValue(30, 50);
                sprites[idx].tint = target_asteroid_tint;
                add_targeted_asteroid_time = earth_revolve_time / (float) ((int) earth_revolve_count + 4);
                // TraceLog(LOG_INFO, " -- added target ast idx=%d, side=%d, pos=(%d, %d), angle=%d, vel=[%.2f, %.2f], rotation=%d, next_in=%d",
                //          idx, side, (int) sprites[idx].dest_rect.x, (int) sprites[idx].dest_rect.y, (int) angle_to_earth,
                //          sprites[idx].velocity.x, sprites[idx].velocity.y, (int) sprites[idx].rotation, (int) add_ambient_asteroid_time);
            }
        }

        // Update moving sprites
        if(current_state <= STATE_IS_RUNNING) {
            for(int i = 0; i < sb_count(sprites); i++) {
                if(sprites[i].type < 0) { continue; }
                sprites[i].dest_rect.x += sprites[i].velocity.x * frame_time;
                sprites[i].dest_rect.y += sprites[i].velocity.y * frame_time;

                sprites[i].rotation += sprites[i].rotation_delta * frame_time;
                if(sprites[i].rotation < 0.f) { sprites[i].rotation += 360.f; }
                if(sprites[i].rotation > 360.f) { sprites[i].rotation -= 360.f; }

                // Fade out explosions
                bool is_faded = false;
                if(sprites[i].type == TEXTURE_IDX_EXPLOSION) {
                    float exp_alpha = (float) sprites[i].tint.a;
                    exp_alpha -= explosion_fade_delta * frame_time;
                    if(exp_alpha <= 0) {
                        sprites[i].tint.a = 0;
                        is_faded = true;
                    } else {
                        sprites[i].tint.a = (unsigned char) roundf(exp_alpha);
                    }
                }

                bool is_oob =  // Out of bounds
                        sprites[i].dest_rect.x + sprites[i].dest_rect.width < 0 ||
                        sprites[i].dest_rect.x - sprites[i].dest_rect.width > WND_W ||
                        sprites[i].dest_rect.y + sprites[i].dest_rect.height < 0 ||
                        sprites[i].dest_rect.y - sprites[i].dest_rect.height > WND_W;
                if(is_faded || is_oob) {
                    // TraceLog(LOG_INFO, " -- removing sprite at idx=%d (type=%d)", i, sprites[i].type);
                    sprites[i].type *= -1;
                }
            }
        }

        // Collisions
        if(current_state == STATE_PLAYING) {
            bool earth_dead = false;
            bool earth_pk = false;

            // Check asteroid collisions
            Vector2 roid_pos, other_pos;
            float roid_radius, other_radius;
            for(int i = 0; i < sb_count(sprites) && !earth_dead; i++) {
                if(sprites[i].type != TEXTURE_IDX_ASTEROID) { continue; }
                roid_pos.x = sprites[i].dest_rect.x;
                roid_pos.y = sprites[i].dest_rect.y;
                roid_radius = sprites[i].dest_rect.width / 3.f;

                // Check collision with Sun -- explode current asteroid
                if(CheckCollisionCircles(roid_pos, roid_radius, sun_pos, sun_radius)) {
                    TraceLog(LOG_INFO, "Collision: asteroid (idx=%d) & sun", i);
                    ExplodeSprite(i, TEXTURE_IDX_EXPLOSION);
                    PlayIndexedSound(SOUND_EXPL_IDXS[GetRandomValue(0, 2)]);
                    goto next_roid;
                }

                // Check collision with Earth -- explode asteroid, scorch Earth
                if(CheckCollisionCircles(roid_pos, roid_radius, earth_pos, earth_radius)) {
                    TraceLog(LOG_INFO, "Collision: asteroid (idx=%d) & EARTH!!", i);
                    ExplodeSprite(i, TEXTURE_IDX_EXPLOSION);
                    PlayIndexedSound(SOUND_EXPL_IDXS[GetRandomValue(0, 2)]);
                    earth_sprite.texture = loaded_textures[TEXTURE_IDX_SCORCHED];
                    earth_dead = true;
                    current_state = STATE_END_ZOOM;
                    end_message = (char *) "You kept Earth alive for %.1f years";
                    goto next_roid;
                }

                // Check collision with other asteroids & flares -- explode them on contact
                for(int j = 0; j < sb_count(sprites); j++) {
                    if(i == j) { continue; }
                    if(sprites[j].type < 0) { continue; }
                    other_pos.x = sprites[j].dest_rect.x;
                    other_pos.y = sprites[j].dest_rect.y;
                    other_radius = fmin(sprites[j].dest_rect.width, sprites[j].dest_rect.height) / 3.f;
                    if(!CheckCollisionCircles(roid_pos, roid_radius, other_pos, other_radius)) { continue; }
                    TraceLog(LOG_INFO, "Collision: asteroid (idx=%d) & %s (idx=%d)", i,
                             sprites[j].type == TEXTURE_IDX_FLARE ? "flare" :
                                 sprites[j].type == TEXTURE_IDX_EXPLOSION ? "explosion" : "other asteroid", j);

                    // Explode primary asteroid
                    ExplodeSprite(i, TEXTURE_IDX_EXPLOSION);
                    PlayIndexedSound(SOUND_EXPL_IDXS[GetRandomValue(0, 2)]);

                    // If other is also asteroid, explode it too
                    if(sprites[j].type == TEXTURE_IDX_ASTEROID) {
                        ExplodeSprite(j, TEXTURE_IDX_EXPLOSION);
                    }
                    goto next_roid;
                }
                next_roid:;
            }

            // Check flare collisions with Earth
            for(int i = 0; i < sb_count(sprites) && !earth_dead; i++) {
                if(sprites[i].type != TEXTURE_IDX_FLARE) { continue; }
                // Just call it a 'roid for now
                roid_pos.x = sprites[i].dest_rect.x;
                roid_pos.y = sprites[i].dest_rect.y;
                roid_radius = sprites[i].dest_rect.width / 3.f;

                // Check collision with Earth -- remove flare, scorch Earth
                if(CheckCollisionCircles(roid_pos, roid_radius, earth_pos, earth_radius)) {
                    TraceLog(LOG_INFO, "Collision: flare (idx=%d) & EARTH!!", i);
                    ExplodeSprite(i, TEXTURE_IDX_EXPLOSION);
                    PlayIndexedSound(SOUND_EXPL_IDXS[GetRandomValue(0, 2)]);
                    earth_sprite.texture = loaded_textures[TEXTURE_IDX_SCORCHED];
                    earth_dead = true;
                    earth_pk = true;
                    current_state = STATE_END_ZOOM;
                    end_message = (char *)"You killed the Earth after just %.1f years";
                    goto next_flare;
                }
                next_flare:;
            }

            // Transition to ending screen
            if(earth_dead) {
                TraceLog(LOG_INFO, "Starting transition to end zoom");
                current_state = STATE_END_ZOOM;
                PlayIndexedSound(earth_pk ? SOUND_IDX_SCORCHED_FLARE : SOUND_IDX_SCORCHED_ASTEROID);

                // Calculate earth velocity
                end_zoom_earth_target_x = WND_W / 2.f;
                end_zoom_earth_target_y = WND_H / 2.f;
                earth_sprite.dest_rect.x -= earth_sprite.dest_rect.width / 2.f;
                earth_sprite.dest_rect.y -= earth_sprite.dest_rect.height / 2.f;
                earth_sprite.velocity.x = (end_zoom_earth_target_x - earth_sprite.dest_rect.x) / end_zoom_period;
                earth_sprite.velocity.y = (end_zoom_earth_target_y - earth_sprite.dest_rect.y) / end_zoom_period;
                TraceLog(LOG_INFO, "Earth move starting at (%d, %d), moving toward (%d, %d), so delta=[%d, %d], and velocity=[%.2f, %.2f]",
                         (int) earth_sprite.dest_rect.x, (int) earth_sprite.dest_rect.y,
                         (int) end_zoom_earth_target_x, (int) end_zoom_earth_target_y,
                         (int) earth_sprite.dest_rect.x - (int) end_zoom_earth_target_x, (int) earth_sprite.dest_rect.y - (int) end_zoom_earth_target_y,
                         earth_sprite.velocity.x, earth_sprite.velocity.y);
            }
        }

        // Update ending zoom
        if(current_state == STATE_END_ZOOM) {
            earth_sprite.dest_rect.x += earth_sprite.velocity.x * frame_time;
            earth_sprite.dest_rect.y += earth_sprite.velocity.y * frame_time;
            // TraceLog(LOG_INFO, " -- earth_pos updated to (%.2f, %.2f)", earth_sprite.dest_rect.x, earth_sprite.dest_rect.y);
            earth_sprite.scale += end_zoom_scale_delta * frame_time;
            if(earth_sprite.scale >= end_zoom_scale_target) {
                TraceLog(LOG_INFO, "Starting transition to end fade");
                current_state = STATE_END_FADE;
                PlayIndexedSound(SOUND_IDX_END);
                earth_sprite.velocity = { 0.f, 0.f };
                earth_sprite.scale = end_zoom_scale_target;
                earth_sprite.dest_rect.x = end_zoom_earth_target_x;
                earth_sprite.dest_rect.y = end_zoom_earth_target_y;
                end_fade_alpha = 0.f;
            }
        }

        // Update ending fade
        if(current_state == STATE_END_FADE) {
            end_fade_alpha += end_fade_delta * frame_time;
            if(end_fade_alpha >= 255.f) {
                TraceLog(LOG_INFO, "Moving to end choice");
                current_state = STATE_END_CHOICE;
                end_fade_alpha = 255.f;
            }
        }

        // Handle mouse input on end choice state
        if(current_state == STATE_END_CHOICE) {
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                current_state = STATE_PLAYING;
                PlayIndexedSound(SOUND_IDX_START);
                playing_start_time = GetTime();
                add_ambient_asteroid_time = 0.f;
                add_targeted_asteroid_time = 0.5f;

                for(int i = 0; i < sb_count(sprites); i++) {
                    if(sprites[i].type > 0) {
                        sprites[i].type *= -1;
                    }
                }
                earth_revolve_count = 0.;
                earth_revolve_angle = 45.f;
                earth_sprite.scale = 1.f;
                earth_sprite.texture = loaded_textures[TEXTURE_IDX_EARTH];
                earth_sprite.velocity = { 0, 0 };
            }
        }

        BeginDrawing();
        {
            ClearBackground(COLOR_BACKGROUND);

            // Draw the stars
            if(current_state <= STATE_IS_RUNNING) {
                for(int i = 0; i < STAR_COUNT; i++) {
                    if(stars[i].z > 1.f) {
                        DrawLine((int) stars[i].x, (int) stars[i].y, (int) stars[i].x + 3, (int) stars[i].y, WHITE);
                    } else {
                        DrawPixel((int) stars[i].x, (int) stars[i].y, WHITE);
                    }
                }
            }

            // Draw target line under Sun
            if(mouse_has_moved && current_state <= STATE_IS_RUNNING) {
                DrawLine(sun_sprite.dest_rect.x, sun_sprite.dest_rect.y, mouse_target_x, mouse_target_y, COLOR_MOUSE_TARGET);
            }

            if(current_state <= STATE_IS_RUNNING) {
                DrawTexturePro(sun_sprite.texture, sun_sprite.src_rect, sun_sprite.dest_rect, sun_sprite.origin, sun_sprite.rotation, WHITE);
                DrawTexturePro(earth_sprite.texture, earth_sprite.src_rect, earth_sprite.dest_rect, earth_sprite.origin, earth_sprite.rotation, WHITE);
            } else {
                earth_pos = { .x=earth_sprite.dest_rect.x - (earth_sprite.dest_rect.width / 2.f),
                              .y=earth_sprite.dest_rect.y - (earth_sprite.dest_rect.height / 2.f) };
                DrawTextureEx(earth_sprite.texture, earth_pos, earth_sprite.rotation, earth_sprite.scale, earth_sprite.tint);
            }

            if(current_state == STATE_TITLE || current_state == STATE_TITLE_FADE) {
                unsigned char title_alpha = (unsigned char) title_fade_alpha;
                Color title_red = (Color) { RED.r, RED.g, RED.b, title_alpha};
                Color title_white = (Color) { WHITE.r, WHITE.g, WHITE.b, title_alpha};
                Color title_yellow = (Color) { YELLOW.r, YELLOW.g, YELLOW.b, title_alpha};
                DrawText("SOLAR", 14, 14, 80, title_red);
                DrawText("SOLAR", 10, 10, 80, title_white);
                DrawText("COMMANDER", 14, 104, 80, title_red);
                DrawText("COMMANDER", 10, 100, 80, title_white);
                DrawText("Keep Earth Alive  < Ludum Dare 46 >", 10, 190, 20, title_yellow);

                DrawText("Protect Earth from asteroids", 10, WND_H - 80, 20, title_yellow);
                DrawText("Use mouse to shoot solar flares", 10, WND_H - 50, 20, title_yellow);
            }

            if(current_state <= STATE_IS_RUNNING) {
                for(int i = 0; i < sb_count(sprites); i++) {
                    if(sprites[i].type < 0) { continue; }
                    DrawTexturePro(sprites[i].texture, sprites[i].src_rect, sprites[i].dest_rect,
                                   sprites[i].origin, sprites[i].rotation, sprites[i].tint);
                }
            }

            if(current_state == STATE_PLAYING) {
                DrawText(TextFormat("Earth alive: %0.2f years", earth_revolve_count), 10, 10, 20, YELLOW);
            }

            if(current_state == STATE_END_FADE || current_state == STATE_END_CHOICE) {
                unsigned char end_alpha = (unsigned char) end_fade_alpha;
                Color title_red = (Color) { RED.r, RED.g, RED.b, end_alpha};
                Color title_white = (Color) { WHITE.r, WHITE.g, WHITE.b, end_alpha};
                Color title_yellow = (Color) { YELLOW.r, YELLOW.g, YELLOW.b, end_alpha};
                DrawText("SCORCHED", 14, 14, 80, title_red);
                DrawText("SCORCHED", 10, 10, 80, title_white);
                DrawText("EARTH", 14, 104, 80, title_red);
                DrawText("EARTH", 10, 100, 80, title_white);
                DrawText(TextFormat(end_message, earth_revolve_count), 10, 190, 20, title_yellow);
                DrawText(TextFormat("Record: %.1f years", max_earth_revolve_count), 10, 220, 20, title_yellow);

                if(current_state == STATE_END_CHOICE) {
                    DrawText("Click to play again", 10, WND_H / 2, 20, title_yellow);
                    DrawText("Code: Steve Blackwell", 10, WND_H - 80, 20, title_yellow);
                    DrawText("Art & sound: Connie Ma", 10, WND_H - 50, 20, title_yellow);
                }
            }
        }
        EndDrawing();
    }


    for(int i = 0; i < sb_count(loaded_textures); i++) {
        UnloadTexture(loaded_textures[i]);
    }
    for(int i = 0; i < sb_count(loaded_sounds); i++) {
        UnloadSound(loaded_sounds[i]);
    }
    return 0;
}
