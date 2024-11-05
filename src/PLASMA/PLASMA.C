/*
 * plasma.c - Copyright (c) 2024 - Olivier Poncet
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
typedef struct _Buffer  Image1;
typedef struct _Buffer  Image2;
typedef struct _Buffer  Image3;
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
    uint16_t     pal_r;
    uint16_t     pal_g;
    uint16_t     pal_b;
    uint16_t     inc_r;
    uint16_t     inc_g;
    uint16_t     inc_b;
    uint8_t far* pixels;
};

struct _Buffer
{
    uint16_t     dim_w;
    uint16_t     dim_h;
    uint16_t     pos_x;
    uint16_t     pos_y;
    uint16_t     angle;
    uint16_t     speed;
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
    Image1 image1;
    Image2 image2;
    Image3 image3;
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
        160, /* dim_w  */
        100, /* dim_h  */
        0,   /* pal_r  */
        0,   /* pal_g  */
        0,   /* pal_b  */
        101, /* inc_r  */
        127, /* inc_g  */
        257, /* inc_b  */
        NULL /* pixels */
    },
    /* image1 */ {
        320, /* dim_w  */
        200, /* dim_h  */
        0,   /* pos_x  */
        0,   /* pos_y  */
        0,   /* angle  */
        -5,  /* speed  */
        NULL /* pixels */
    },
    /* image2 */ {
        320, /* dim_w  */
        200, /* dim_h  */
        0,   /* pos_x  */
        0,   /* pos_y  */
        0,   /* angle  */
        +2,  /* speed  */
        NULL /* pixels */
    },
    /* image3 */ {
        320, /* dim_w  */
        200, /* dim_h  */
        0,   /* pos_x  */
        0,   /* pos_y  */
        0,   /* angle  */
        +3,  /* speed  */
        NULL /* pixels */
    },
};

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
        for(index = 0; index < count; ++index) {
            Color color;
            color.r = index;
            color.g = index;
            color.b = index;
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

void effect_update(Effect* effect, Program* program)
{
    const uint16_t     dim_1 = program->image1.dim_w;
    const uint16_t     dim_2 = program->image2.dim_w;
    const uint16_t     dim_3 = program->image3.dim_w;
    const uint8_t far* img_1 = &program->image1.pixels[(program->image1.pos_y * dim_1) + program->image1.pos_x];
    const uint8_t far* img_2 = &program->image2.pixels[(program->image2.pos_y * dim_2) + program->image2.pos_x];
    const uint8_t far* img_3 = &program->image3.pixels[(program->image3.pos_y * dim_3) + program->image3.pos_x];

    /* render the effect */ {
        const uint16_t dst_w = effect->dim_w;
        const uint16_t dst_h = effect->dim_h;
        uint16_t       dst_x = 0;
        uint16_t       dst_y = 0;
        uint8_t far*   dst_p = effect->pixels;
        for(dst_y = dst_h; dst_y != 0; --dst_y) {
            const uint8_t far* src_1 = img_1;
            const uint8_t far* src_2 = img_2;
            const uint8_t far* src_3 = img_3;
            for(dst_x = dst_w; dst_x != 0; --dst_x) {
                *dst_p++ = *src_1++
                         + *src_2++
                         + *src_3++
                         ;
            }
            img_1 += dim_1;
            img_2 += dim_2;
            img_3 += dim_3;
        }
    }
}

void effect_render(Effect* effect, Screen* screen)
{
    /* wait for vbl */ {
        vga_wait_vbl();
    }
    /* update_colors */ {
        uint16_t       index = 0;
        const uint16_t count = 256;
        const uint16_t inc_r = effect->inc_r;
        const uint16_t inc_g = effect->inc_g;
        const uint16_t inc_b = effect->inc_b;
        uint16_t       pal_r = effect->pal_r + inc_r;
        uint16_t       pal_g = effect->pal_g + inc_g;
        uint16_t       pal_b = effect->pal_b + inc_b;
        effect->pal_r = pal_r;
        effect->pal_g = pal_g;
        effect->pal_b = pal_b;
        for(index = 0; index < count; ++index) {
            vga_set_color(index, (pal_r >> 8), (pal_g >> 8), (pal_b >> 8));
            pal_r += inc_r;
            pal_g += inc_g;
            pal_b += inc_b;
        }
    }
    /* render the effect */ {
        const uint16_t     src_w = effect->dim_w;
        const uint16_t     src_h = effect->dim_h;
        const uint8_t far* src_p = effect->pixels;
        uint8_t far*       dst_p = screen->pixels;
        uint16_t           cnt_x = 0;
        uint16_t           cnt_y = 0;
        for(cnt_y = src_h; cnt_y != 0; --cnt_y) {
            const uint8_t far* src_o = src_p;
            for(cnt_x = src_w, src_p = src_o; cnt_x != 0; --cnt_x) {
                const uint8_t pixel = *src_p++;
                *dst_p++ = pixel;
                *dst_p++ = pixel;
            }
            for(cnt_x = src_w, src_p = src_o; cnt_x != 0; --cnt_x) {
                const uint8_t pixel = *src_p++;
                *dst_p++ = pixel;
                *dst_p++ = pixel;
            }
        }
    }
}

/*
 * ---------------------------------------------------------------------------
 * image1
 * ---------------------------------------------------------------------------
 */

void image1_init(Image1* image1)
{
    if(image1->pixels == NULL) {
        image1->pixels = alloc_buffer(image1->dim_h, image1->dim_w);
    }
    if(image1->pixels != NULL) {
        const uint16_t dst_w = image1->dim_w;
        const uint16_t dst_h = image1->dim_h;
        uint8_t far*   dst_p = image1->pixels;
        uint16_t       dst_x = 0;
        uint16_t       dst_y = 0;
        for(dst_y = 0; dst_y < dst_h; ++dst_y) {
            for(dst_x = 0; dst_x < dst_w; ++dst_x) {
                const int32_t dx = (INT32_T(dst_x) - INT32_T(dst_w / 2));
                const int32_t dy = (INT32_T(dst_y) - INT32_T(dst_h / 2));
                *dst_p++ = UINT8_T(sqrt((dx * dx) + (dy * dy)) * 7.0);
            }
        }
    }
}

void image1_fini(Image1* image1)
{
    if(image1->pixels != NULL) {
        image1->pixels = free_buffer(image1->pixels);
    }
}

void image1_update(Image1* image1, int16_t px, int16_t py, int16_t dw, int16_t dh)
{
    const uint16_t angle = ((image1->angle + image1->speed) & 1023);

    image1->angle = angle;
    image1->pos_x = (px + ((dw * g_globals.cos[angle]) >> 8));
    image1->pos_y = (py + ((dh * g_globals.sin[angle]) >> 8));
}

/*
 * ---------------------------------------------------------------------------
 * image2
 * ---------------------------------------------------------------------------
 */

void image2_init(Image2* image2)
{
    if(image2->pixels == NULL) {
        image2->pixels = alloc_buffer(image2->dim_h, image2->dim_w);
    }
    if(image2->pixels != NULL) {
        const uint16_t dst_w = image2->dim_w;
        const uint16_t dst_h = image2->dim_h;
        uint8_t far*   dst_p = image2->pixels;
        uint16_t       dst_x = 0;
        uint16_t       dst_y = 0;
        for(dst_y = 0; dst_y < dst_h; ++dst_y) {
            for(dst_x = 0; dst_x < dst_w; ++dst_x) {
                const int32_t dx = (INT32_T(dst_x) - INT32_T(dst_w / 2));
                const int32_t dy = (INT32_T(dst_y) - INT32_T(dst_h / 2));
                *dst_p++ = UINT8_T((1.0 + sin(sqrt((dx * dx) + (dy * dy)) / 11.0)) * 127.5);
            }
        }
    }
}

void image2_fini(Image2* image2)
{
    if(image2->pixels != NULL) {
        image2->pixels = free_buffer(image2->pixels);
    }
}

void image2_update(Image2* image2, int16_t px, int16_t py, int16_t dw, int16_t dh)
{
    const uint16_t angle = ((image2->angle + image2->speed) & 1023);

    image2->angle = angle;
    image2->pos_x = (px + ((dw * g_globals.cos[angle]) >> 8));
    image2->pos_y = (py + ((dh * g_globals.sin[angle]) >> 8));
}

/*
 * ---------------------------------------------------------------------------
 * image3
 * ---------------------------------------------------------------------------
 */

void image3_init(Image3* image3)
{
    if(image3->pixels == NULL) {
        image3->pixels = alloc_buffer(image3->dim_h, image3->dim_w);
    }
    if(image3->pixels != NULL) {
        const uint16_t dst_w = image3->dim_w;
        const uint16_t dst_h = image3->dim_h;
        uint8_t far*   dst_p = image3->pixels;
        uint16_t       dst_x = 0;
        uint16_t       dst_y = 0;
        for(dst_y = 0; dst_y < dst_h; ++dst_y) {
            for(dst_x = 0; dst_x < dst_w; ++dst_x) {
                const int32_t dx = (INT32_T(dst_x) - INT32_T(dst_w / 2));
                const int32_t dy = (INT32_T(dst_y) - INT32_T(dst_h / 2));
                *dst_p++ = UINT8_T((1.0 + sin(sqrt((dx * dx) + (dy * dy)) / 19.0)) * 127.5);
            }
        }
    }
}

void image3_fini(Image3* image3)
{
    if(image3->pixels != NULL) {
        image3->pixels = free_buffer(image3->pixels);
    }
}

void image3_update(Image3* image3, int16_t px, int16_t py, int16_t dw, int16_t dh)
{
    const uint16_t angle = ((image3->angle + image3->speed) & 1023);

    image3->angle = angle;
    image3->pos_x = (px + ((dw * g_globals.cos[angle]) >> 8));
    image3->pos_y = (py + ((dh * g_globals.sin[angle]) >> 8));
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
    image1_init(&program->image1);
    image2_init(&program->image2);
    image3_init(&program->image3);
}

void program_loop(Program* program)
{
    const int16_t px = ((program->effect.dim_w / 2) + 0);
    const int16_t py = ((program->effect.dim_h / 2) + 0);
    const int16_t dw = ((program->effect.dim_w / 2) - 1);
    const int16_t dh = ((program->effect.dim_h / 2) - 1);

    while(kbhit() == 0) {
        image1_update(&program->image1, px, py, dw, dh);
        image2_update(&program->image2, px, py, dw, dh);
        image3_update(&program->image3, px, py, dw, dh);
        effect_update(&program->effect, program);
        effect_render(&program->effect, &program->screen);
    }
    while(kbhit() != 0) {
        (void) getch();
    }
}

void program_end(Program* program)
{
    image3_fini(&program->image3);
    image2_fini(&program->image2);
    image1_fini(&program->image1);
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
