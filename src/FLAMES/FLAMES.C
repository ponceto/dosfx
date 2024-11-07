/*
 * flames.c - Copyright (c) 2024 - Olivier Poncet
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <dos.h>

/*
 * ---------------------------------------------------------------------------
 * some useful macros
 * ---------------------------------------------------------------------------
 */

#ifndef countof
#define countof(array) (sizeof(array) / sizeof(array[0]))
#endif

#define IGNORE(value)   ((void)(value))
#define DOUBLE(value)   ((double)(value))
#define SIZE_T(value)   ((size_t)(value))
#define INT8_T(value)   ((int8_t)(value))
#define INT16_T(value)  ((int16_t)(value))
#define INT32_T(value)  ((int32_t)(value))
#define UINT8_T(value)  ((uint8_t)(value))
#define UINT16_T(value) ((uint16_t)(value))
#define UINT32_T(value) ((uint32_t)(value))

/*
 * ---------------------------------------------------------------------------
 * some useful types
 * ---------------------------------------------------------------------------
 */

typedef signed char    int8_t;
typedef signed short   int16_t;
typedef signed long    int32_t;
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long  uint32_t;

/*
 * ---------------------------------------------------------------------------
 * some useful utilities
 * ---------------------------------------------------------------------------
 */

uint8_t far* alloc_buffer(size_t row, size_t col)
{
    const size_t   bytes = (SIZE_T(row)   *   SIZE_T(col));
    const uint32_t total = (UINT32_T(row) * UINT32_T(col));

    if((bytes == total) && (bytes != 0)) {
        uint8_t far* buffer = (uint8_t far*) malloc(bytes);
        if(buffer != NULL) {
            (void) memset(buffer, 0, bytes);
            return buffer;
        }
    }
    return NULL;
}

uint8_t far* free_buffer(uint8_t far* buffer)
{
    if(buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }
    return buffer;
}

/*
 * ---------------------------------------------------------------------------
 * low level vga functions
 * ---------------------------------------------------------------------------
 */

uint8_t vga_set_mode(uint8_t mode)
{
    uint8_t prev = 0x00;

    /* get old mode */ {
        union REGS regs;
        regs.h.ah = 0x0f;
        regs.h.al = prev;
        (void) int86(0x10, &regs, &regs);
        prev = regs.h.al;
    }
    /* set new mode */ {
        union REGS regs;
        regs.h.ah = 0x00;
        regs.h.al = mode;
        (void) int86(0x10, &regs, &regs);
        mode = regs.h.al;
    }
    return prev;
}

void vga_set_color(uint8_t color, uint8_t r, uint8_t g, uint8_t b)
{
    outportb(0x3c8, color);
    outportb(0x3c9, (r >> 2));
    outportb(0x3c9, (g >> 2));
    outportb(0x3c9, (b >> 2));
}

void vga_wait_vbl(void)
{
    uint8_t status;

    do {
        status = inportb(0x3da);
    } while((status & 8) == 0x00);
}

/*
 * ---------------------------------------------------------------------------
 * types
 * ---------------------------------------------------------------------------
 */

typedef struct _Color   Color;
typedef struct _Screen  Screen;
typedef struct _Effect  Effect;
typedef struct _Globals Globals;
typedef struct _Program Program;

struct _Color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct _Screen
{
    uint8_t      v_mode;
    uint8_t      p_mode;
    uint16_t     dim_w;
    uint16_t     dim_h;
    uint8_t far* pixels;
};

struct _Effect
{
    uint16_t     dim_w;
    uint16_t     dim_h;
    uint16_t     random;
    uint8_t far* pixels;
};

struct _Globals
{
    int16_t sin[1024];
    int16_t cos[1024];
};

struct _Program
{
    Screen screen;
    Effect effect;
};

/*
 * ---------------------------------------------------------------------------
 * global variables
 * ---------------------------------------------------------------------------
 */

Globals g_globals = {
    { 0 }, /* sin */
    { 0 }, /* cos */
};

Program g_program = {
    /* screen */ {
        0,   /* v_mode */
        0,   /* p_mode */
        0,   /* dim_w  */
        0,   /* dim_h  */
        NULL /* pixels */
    },
    /* effect */ {
        320, /* dim_w  */
        200, /* dim_h  */
        0,   /* random */
        NULL /* pixels */
    },
};

/*
 * ---------------------------------------------------------------------------
 * color
 * ---------------------------------------------------------------------------
 */

double hue2rgb(double p, double q, double t)
{
    if(t < 0.0) {
        t += 1.0;
    }
    if(t > 1.0) {
        t -= 1.0;
    }
    if(t < (1.0 / 6.0)) {
        return p + (q - p) * 6.0 * t;
    }
    if(t < (1.0 / 2.0)) {
        return q;
    }
    if(t < (2.0 / 3.0)) {
        return p + (q - p) * ((2.0 / 3.0) - t) * 6.0;
    }
    return p;
}

void color_init_rgb(Color* color, double r, double g, double b)
{
    if(r <= 0.0) { r = 0.0; }
    if(r >= 1.0) { r = 1.0; }
    if(g <= 0.0) { g = 0.0; }
    if(g >= 1.0) { g = 1.0; }
    if(b <= 0.0) { b = 0.0; }
    if(b >= 1.0) { b = 1.0; }

    color->r = UINT8_T(255.0 * r);
    color->g = UINT8_T(255.0 * g);
    color->b = UINT8_T(255.0 * b);
}

void color_init_hsl(Color* color, double h, double s, double l)
{
    if(h <= 0.0) { h = 0.0; }
    if(h >= 1.0) { h = 1.0; }
    if(s <= 0.0) { s = 0.0; }
    if(s >= 1.0) { s = 1.0; }
    if(l <= 0.0) { l = 0.0; }
    if(l >= 1.0) { l = 1.0; }

    if(s == 0.0) {
        color_init_rgb(color, l, l, l);
    }
    else {
        const double q = (l < 0.5 ? (l * (1.0 + s)) : (l + s - l * s));
        const double p = ((2.0 * l) - q);
        const double r = hue2rgb(p, q, h + (1.0 / 3.0));
        const double g = hue2rgb(p, q, h);
        const double b = hue2rgb(p, q, h - (1.0 / 3.0));
        color_init_rgb(color, r, g, b);
    }
}

/*
 * ---------------------------------------------------------------------------
 * screen
 * ---------------------------------------------------------------------------
 */

void screen_init(Screen* screen)
{
    if(screen->pixels == NULL) {
        screen->v_mode = 0x13;
        screen->p_mode = vga_set_mode(screen->v_mode);
        screen->dim_w  = 320;
        screen->dim_h  = 200;
        screen->pixels = MK_FP(0xA000, 0x0000);
    }
    if(screen->pixels != NULL) {
        uint16_t       index = 0;
        const uint16_t count = 256;
        Color          color = { 0, 0, 0 };
        for(index = 0; index < count; ++index) {
            const double h = DOUBLE(index) / 255.0 / 6.0;
            const double s = DOUBLE(1.0);
            const double l = DOUBLE(index) / 255.0 * 1.1;
            color_init_hsl(&color, h, s, l);
            vga_set_color(index, color.r, color.g, color.b);
        }
    }
    if(screen->pixels != NULL) {
        const uint16_t dst_w = screen->dim_w;
        const uint16_t dst_h = screen->dim_h;
        uint8_t far*   dst_p = screen->pixels;
        uint16_t       dst_x = 0;
        uint16_t       dst_y = 0;
        for(dst_y = 0; dst_y < dst_h; ++dst_y) {
            for(dst_x = 0; dst_x < dst_w; ++dst_x) {
                *dst_p++ = UINT8_T(0);
            }
        }
    }
}

void screen_fini(Screen* screen)
{
    if(screen->pixels != NULL) {
        screen->v_mode = screen->p_mode;
        screen->p_mode = vga_set_mode(screen->v_mode);
        screen->dim_w  = 0;
        screen->dim_h  = 0;
        screen->pixels = NULL;
    }
}

/*
 * ---------------------------------------------------------------------------
 * effect
 * ---------------------------------------------------------------------------
 */

void effect_init(Effect* effect)
{
    if(effect->pixels == NULL) {
        effect->pixels = alloc_buffer(effect->dim_h, effect->dim_w);
    }
}

void effect_fini(Effect* effect)
{
    if(effect->pixels != NULL) {
        effect->pixels = free_buffer(effect->pixels);
    }
}

void effect_update(Effect* effect)
{
    IGNORE(effect);
}

void effect_render(Effect* effect, Screen* screen)
{
    const uint16_t dst_w = screen->dim_w;
    const uint16_t dst_h = screen->dim_h;
    const uint16_t dst_s = ((screen->dim_w + 1) & ~1);
    uint8_t far*   dst_p = screen->pixels;
    uint16_t       cnt_x = 0;
    uint16_t       cnt_y = 0;

    /* wait for vbl */ {
        vga_wait_vbl();
    }
    /* render the effect */ {
        for(cnt_y = (dst_h - 1); cnt_y != 0; --cnt_y) {
            uint8_t far* dst_o = dst_p;
            /* left column*/ {
                const uint16_t v1 = dst_p[dst_s];
                const uint16_t v2 = dst_p[dst_s + 1];
                const uint16_t v3 = dst_p[dst_s + 1];
                const uint16_t v4 = dst_p[dst_s * 2];
                *dst_p++ = UINT8_T(((v1 + v2 + v3 + v4) * 31) >> 7);
            }
            for(cnt_x = (dst_w - 2); cnt_x != 0; --cnt_x) {
                const uint16_t v1 = dst_p[dst_s];
                const uint16_t v2 = dst_p[dst_s - 1];
                const uint16_t v3 = dst_p[dst_s + 1];
                const uint16_t v4 = dst_p[dst_s * 2];
                *dst_p++ = UINT8_T(((v1 + v2 + v3 + v4) * 31) >> 7);
            }
            /* right column*/ {
                const uint16_t v1 = dst_p[dst_s];
                const uint16_t v2 = dst_p[dst_s - 1];
                const uint16_t v3 = dst_p[dst_s - 1];
                const uint16_t v4 = dst_p[dst_s * 2];
                *dst_p++ = UINT8_T(((v1 + v2 + v3 + v4) * 31) >> 7);
            }
            dst_p = dst_o + dst_s;
        }
    }
    /* render the last line */ {
        uint16_t random = effect->random;
        for(cnt_x = dst_w; cnt_x != 0; --cnt_x) {
            random = ((random * 137) + 187);
            *dst_p++ = 128 + (UINT8_T(random >> 9) & 0x7f);
        }
        effect->random = random;
    }
}

/*
 * ---------------------------------------------------------------------------
 * globals
 * ---------------------------------------------------------------------------
 */

void globals_init(Globals* globals)
{
    int       index = 0;
    const int count = 1024;
    double    angle = 0.0;

    for(index = 0; index < count; ++index) {
        angle = DOUBLE(index) * (2.0 * M_PI) / DOUBLE(count);
        globals->sin[index] = INT16_T(sin(angle) * 256.0);
        globals->cos[index] = INT16_T(cos(angle) * 256.0);
    }
}

void globals_fini(Globals* globals)
{
    (void) memset(globals, 0, sizeof(*globals));
}

/*
 * ---------------------------------------------------------------------------
 * program
 * ---------------------------------------------------------------------------
 */

void program_begin(Program* program)
{
    screen_init(&program->screen);
    effect_init(&program->effect);
}

void program_loop(Program* program)
{
    while(kbhit() == 0) {
        effect_update(&program->effect);
        effect_render(&program->effect, &program->screen);
    }
    while(kbhit() != 0) {
        (void) getch();
    }
}

void program_end(Program* program)
{
    effect_fini(&program->effect);
    screen_fini(&program->screen);
}

void program_main(Program* program)
{
    program_begin(program);
    program_loop(program);
    program_end(program);
}

/*
 * ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------
 */

int main(int argc, char* argv[])
{
    IGNORE(argc);
    IGNORE(argv);

    globals_init(&g_globals);
    program_main(&g_program);
    globals_fini(&g_globals);

    return EXIT_SUCCESS;
}

/*
 * ---------------------------------------------------------------------------
 * End-Of-File
 * ---------------------------------------------------------------------------
 */
