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
#define INT8_T(value)   ((int8_t)(value))
#define INT16_T(value)  ((int16_t)(value))
#define INT32_T(value)  ((int32_t)(value))
#define UINT8_T(value)  ((uint8_t)(value))
#define UINT16_T(value) ((uint16_t)(value))
#define UINT32_T(value) ((uint32_t)(value))

/*
 * ---------------------------------------------------------------------------
 * basic types
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
 * types
 * ---------------------------------------------------------------------------
 */

typedef struct _Tables Tables;
typedef struct _Screen Screen;
typedef struct _Effect Effect;
typedef struct _Buffer Image1;
typedef struct _Buffer Image2;
typedef struct _Buffer Image3;
typedef struct _Plasma Plasma;

struct _Tables
{
    int16_t sin[1024];
    int16_t cos[1024];
};

struct _Screen
{
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
    uint16_t     pos_x;
    uint16_t     pos_y;
    uint16_t     dim_w;
    uint16_t     dim_h;
    uint16_t     angle;
    uint16_t     speed;
    uint8_t far* pixels;
};

struct _Plasma
{
    Screen screen;
    Effect effect;
    Image1 image1;
    Image2 image2;
    Image3 image3;
};

/*
 * ---------------------------------------------------------------------------
 * globals
 * ---------------------------------------------------------------------------
 */

Tables g_tables = {
    { 0 }, /* sin */
    { 0 }  /* cos */
};

Plasma g_plasma = {
    /* screen */ {
        320,  /* dim_w  */
        200,  /* dim_h  */
        NULL  /* pixels */
    },
    /* effect */ {
        160,  /* dim_w  */
        100,  /* dim_h  */
        0,    /* pal_r  */
        0,    /* pal_g  */
        0,    /* pal_b  */
        101,  /* inc_r  */
        127,  /* inc_g  */
        257,  /* inc_b  */
        NULL  /* pixels */
    },
    /* image1 */ {
        0,    /* pos_x  */
        0,    /* pos_y  */
        320,  /* dim_w  */
        200,  /* dim_h  */
        0,    /* angle  */
        -5,   /* speed  */
        NULL  /* pixels */
    },
    /* image2 */ {
        0,    /* pos_x  */
        0,    /* pos_y  */
        320,  /* dim_w  */
        200,  /* dim_h  */
        0,    /* angle  */
        +2,   /* speed  */
        NULL  /* pixels */
    },
    /* image3 */ {
        0,    /* pos_x  */
        0,    /* pos_y  */
        320,  /* dim_w  */
        200,  /* dim_h  */
        0,    /* angle  */
        +3,   /* speed  */
        NULL  /* pixels */
    },
};

/*
 * ---------------------------------------------------------------------------
 * low level vga functions
 * ---------------------------------------------------------------------------
 */

void vga_set_mode(uint8_t mode)
{
    union REGS regs;

    regs.h.ah = 0x00;
    regs.h.al = mode;

    (void) int86(0x10, &regs, &regs);
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
 * tables
 * ---------------------------------------------------------------------------
 */

void tables_init(Tables* tables)
{
    int       index = 0;
    const int count = 1024;
    double    angle = 0.0;

    for(index = 0; index < count; ++index) {
        angle = DOUBLE(index) * (2.0 * M_PI) / DOUBLE(count);
        tables->sin[index] = INT16_T(sin(angle) * 256.0);
        tables->cos[index] = INT16_T(cos(angle) * 256.0);
    }
}

void tables_fini(Tables* tables)
{
    (void) memset(tables, 0, sizeof(Tables));
}

/*
 * ---------------------------------------------------------------------------
 * screen
 * ---------------------------------------------------------------------------
 */

void screen_init(Screen* screen)
{
    const uint16_t dim_w = screen->dim_w;
    const uint16_t dim_h = screen->dim_h;

    if(screen->pixels == NULL) {
        vga_set_mode(0x13);
        screen->pixels = MK_FP(0xA000, 0x0000);
    }
    if(screen->pixels != NULL) {
        uint16_t pos_x = 0;
        uint16_t pos_y = 0;
        uint8_t far* pixel = screen->pixels;
        for(pos_y = 0; pos_y < dim_h; ++pos_y) {
            for(pos_x = 0; pos_x < dim_w; ++pos_x) {
                *pixel++ = UINT8_T(0);
            }
        }
    }
}

void screen_fini(Screen* screen)
{
    if(screen->pixels != NULL) {
        vga_set_mode(0x03);
        screen->pixels = NULL;
    }
}

void screen_update(Screen* screen, Plasma* plasma)
{
    const int update_colors = 1;
    const int update_screen = 1;

    vga_wait_vbl();

    if(update_colors) {
        uint16_t index = 0;
        uint16_t count = 256;
        uint16_t pal_r = plasma->effect.pal_r, inc_r = plasma->effect.inc_r;
        uint16_t pal_g = plasma->effect.pal_g, inc_g = plasma->effect.inc_g;
        uint16_t pal_b = plasma->effect.pal_b, inc_b = plasma->effect.inc_b;

        plasma->effect.pal_r += inc_r;
        plasma->effect.pal_g += inc_g;
        plasma->effect.pal_b += inc_b;
        for(index = 0; index < count; ++index) {
            vga_set_color(index, (pal_r >> 8), (pal_g >> 8), (pal_b >> 8));
            pal_r += inc_r;
            pal_g += inc_g;
            pal_b += inc_b;
        }
    }

    if(update_screen) {
        uint16_t dim_w = plasma->effect.dim_w, pos_x = 0;
        uint16_t dim_h = plasma->effect.dim_h, pos_y = 0;
        uint16_t row_stride = screen->dim_w;
        uint8_t pixel = 0;
        uint8_t far* data = plasma->effect.pixels;
        uint8_t far* row1 = (screen->pixels + (0 * row_stride));
        uint8_t far* row2 = (screen->pixels + (1 * row_stride));

        for(pos_y = dim_h; pos_y != 0; --pos_y) {
            for(pos_x = dim_w; pos_x != 0; --pos_x) {
                pixel = *data++;
                *row1++ = pixel;
                *row1++ = pixel;
                *row2++ = pixel;
                *row2++ = pixel;
            }
            row1 += row_stride;
            row2 += row_stride;
        }
    }
}

/*
 * ---------------------------------------------------------------------------
 * effect
 * ---------------------------------------------------------------------------
 */

void effect_init(Effect* effect)
{
    const uint16_t dim_w = effect->dim_w;
    const uint16_t dim_h = effect->dim_h;

    if(effect->pixels == NULL) {
        effect->pixels = (uint8_t far*) malloc(dim_h * dim_w);
    }
    if(effect->pixels != NULL) {
        uint16_t pos_x = 0;
        uint16_t pos_y = 0;
        uint8_t far* pixel = effect->pixels;
        for(pos_y = 0; pos_y < dim_h; ++pos_y) {
            for(pos_x = 0; pos_x < dim_w; ++pos_x) {
                *pixel++ = UINT8_T(0);
            }
        }
    }
}

void effect_fini(Effect* effect)
{
    if(effect->pixels != NULL) {
        free(effect->pixels);
        effect->pixels = NULL;
    }
}

void effect_update(Effect* effect, Plasma* plasma)
{
    uint16_t dim_w = effect->dim_w, pos_x = 0;
    uint16_t dim_h = effect->dim_h, pos_y = 0;
    uint8_t far* dst  = effect->pixels;
    uint8_t far* src1 = NULL;
    uint8_t far* src2 = NULL;
    uint8_t far* src3 = NULL;
    uint8_t far* row1 = &plasma->image1.pixels[((plasma->image1.pos_y * plasma->image1.dim_w) + plasma->image1.pos_x)];
    uint8_t far* row2 = &plasma->image2.pixels[((plasma->image2.pos_y * plasma->image2.dim_w) + plasma->image2.pos_x)];
    uint8_t far* row3 = &plasma->image3.pixels[((plasma->image3.pos_y * plasma->image3.dim_w) + plasma->image3.pos_x)];

    for(pos_y = dim_h; pos_y != 0; --pos_y) {
        src1 = row1; row1 += plasma->image1.dim_w;
        src2 = row2; row2 += plasma->image2.dim_w;
        src3 = row3; row3 += plasma->image3.dim_w;
        for(pos_x = dim_w; pos_x != 0; --pos_x) {
            *dst++ = *src1++
                   + *src2++
                   + *src3++
                   ;
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
    const uint16_t dim_w = image1->dim_w;
    const uint16_t dim_h = image1->dim_h;

    if(image1->pixels == NULL) {
        image1->pixels = (uint8_t far*) malloc(dim_h * dim_w);
    }
    if(image1->pixels != NULL) {
        uint16_t pos_x = 0;
        uint16_t pos_y = 0;
        uint8_t far* pixel = image1->pixels;
        for(pos_y = 0; pos_y < dim_h; ++pos_y) {
            for(pos_x = 0; pos_x < dim_w; ++pos_x) {
                const int32_t dx = (INT32_T(pos_x) - INT32_T(dim_w / 2));
                const int32_t dy = (INT32_T(pos_y) - INT32_T(dim_h / 2));
                *pixel++ = UINT8_T(sqrt((dx * dx) + (dy * dy)) * 7.0);
            }
        }
    }
}

void image1_fini(Image1* image1)
{
    if(image1->pixels != NULL) {
        free(image1->pixels);
        image1->pixels = NULL;
    }
}

void image1_update(Image1* image1, int16_t pos_x, int16_t pos_y, int16_t dim_w, int16_t dim_h)
{
    image1->angle = ((image1->angle + image1->speed) & 1023);
    image1->pos_x = (pos_x + ((dim_w * g_tables.cos[image1->angle]) >> 8));
    image1->pos_y = (pos_y + ((dim_h * g_tables.sin[image1->angle]) >> 8));
}

/*
 * ---------------------------------------------------------------------------
 * image2
 * ---------------------------------------------------------------------------
 */

void image2_init(Image2* image2)
{
    const uint16_t dim_w = image2->dim_w;
    const uint16_t dim_h = image2->dim_h;

    if(image2->pixels == NULL) {
        image2->pixels = (uint8_t far*) malloc(dim_h * dim_w);
    }
    if(image2->pixels != NULL) {
        uint16_t pos_x = 0;
        uint16_t pos_y = 0;
        uint8_t far* pixel = image2->pixels;
        for(pos_y = 0; pos_y < dim_h; ++pos_y) {
            for(pos_x = 0; pos_x < dim_w; ++pos_x) {
                const int32_t dx = (INT32_T(pos_x) - INT32_T(dim_w / 2));
                const int32_t dy = (INT32_T(pos_y) - INT32_T(dim_h / 2));
                *pixel++ = UINT8_T((1.0 + sin(sqrt((dx * dx) + (dy * dy)) / 11.0)) * 127.5);
            }
        }
    }
}

void image2_fini(Image2* image2)
{
    if(image2->pixels != NULL) {
        free(image2->pixels);
        image2->pixels = NULL;
    }
}

void image2_update(Image2* image2, int16_t pos_x, int16_t pos_y, int16_t dim_w, int16_t dim_h)
{
    image2->angle = ((image2->angle + image2->speed) & 1023);
    image2->pos_x = (pos_x + ((dim_w * g_tables.cos[image2->angle]) >> 8));
    image2->pos_y = (pos_y + ((dim_h * g_tables.sin[image2->angle]) >> 8));
}

/*
 * ---------------------------------------------------------------------------
 * image3
 * ---------------------------------------------------------------------------
 */

void image3_init(Image3* image3)
{
    const uint16_t dim_w = image3->dim_w;
    const uint16_t dim_h = image3->dim_h;

    if(image3->pixels == NULL) {
        image3->pixels = (uint8_t far*) malloc(dim_h * dim_w);
    }
    if(image3->pixels != NULL) {
        uint16_t pos_x = 0;
        uint16_t pos_y = 0;
        uint8_t far* pixel = image3->pixels;
        for(pos_y = 0; pos_y < dim_h; ++pos_y) {
            for(pos_x = 0; pos_x < dim_w; ++pos_x) {
                const int32_t dx = (INT32_T(pos_x) - INT32_T(dim_w / 2));
                const int32_t dy = (INT32_T(pos_y) - INT32_T(dim_h / 2));
                *pixel++ = UINT8_T((1.0 + sin(sqrt((dx * dx) + (dy * dy)) / 19.0)) * 127.5);
            }
        }
    }
}

void image3_fini(Image3* image3)
{
    if(image3->pixels != NULL) {
        free(image3->pixels);
        image3->pixels = NULL;
    }
}

void image3_update(Image3* image3, int16_t pos_x, int16_t pos_y, int16_t dim_w, int16_t dim_h)
{
    image3->angle = ((image3->angle + image3->speed) & 1023);
    image3->pos_x = (pos_x + ((dim_w * g_tables.cos[image3->angle]) >> 8));
    image3->pos_y = (pos_y + ((dim_h * g_tables.sin[image3->angle]) >> 8));
}

/*
 * ---------------------------------------------------------------------------
 * plasma
 * ---------------------------------------------------------------------------
 */

void plasma_begin(Plasma* plasma)
{
    screen_init(&plasma->screen);
    effect_init(&plasma->effect);
    image1_init(&plasma->image1);
    image2_init(&plasma->image2);
    image3_init(&plasma->image3);
}

void plasma_loop(Plasma* plasma)
{
    const int16_t pos_x = (plasma->effect.dim_w / 2);
    const int16_t pos_y = (plasma->effect.dim_h / 2);
    const int16_t dim_w = (plasma->effect.dim_w / 2);
    const int16_t dim_h = (plasma->effect.dim_h / 2);

    while(kbhit() == 0) {
        image1_update(&plasma->image1, pos_x, pos_y, dim_w, dim_h);
        image2_update(&plasma->image2, pos_x, pos_y, dim_w, dim_h);
        image3_update(&plasma->image3, pos_x, pos_y, dim_w, dim_h);
        effect_update(&plasma->effect, plasma);
        screen_update(&plasma->screen, plasma);
    }
    while(kbhit() != 0) {
        (void) getch();
    }
}

void plasma_end(Plasma* plasma)
{
    image3_fini(&plasma->image3);
    image2_fini(&plasma->image2);
    image1_fini(&plasma->image1);
    effect_fini(&plasma->effect);
    screen_fini(&plasma->screen);
}

void plasma_main(Plasma* plasma)
{
    plasma_begin(plasma);
    plasma_loop(plasma);
    plasma_end(plasma);
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

    tables_init(&g_tables);
    plasma_main(&g_plasma);
    tables_fini(&g_tables);

    return EXIT_SUCCESS;
}

/*
 * ---------------------------------------------------------------------------
 * End-Of-File
 * ---------------------------------------------------------------------------
 */
