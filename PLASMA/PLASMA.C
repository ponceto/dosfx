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

#define SCREEN_W 320
#define SCREEN_H 200
#define PLASMA_W 160
#define PLASMA_H 100

#define IGNORE(val) ((void)(val))
#define DOUBLE(val) ((double)(val))

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long  uint32_t;

/*
 * ---------------------------------------------------------------------------
 * globals
 * ---------------------------------------------------------------------------
 */

uint8_t far* g_screen = NULL;
uint8_t far* g_image1 = NULL;
uint8_t far* g_image2 = NULL;
uint8_t far* g_plasma = NULL;
uint16_t     g_pal_r  = 0;
uint16_t     g_pal_g  = 0;
uint16_t     g_pal_b  = 0;
short        g_sin_table[256];
short        g_cos_table[256];

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

void tables_init(void)
{
    int    index = 0;
    int    count = 256;
    double angle = 0.0;

    for(index = 0; index < count; ++index) {
        angle = DOUBLE(index) * (2.0 * M_PI) / DOUBLE(count);
        g_sin_table[index] = sin(angle) * 256;
        g_cos_table[index] = cos(angle) * 256;
    }
}

void tables_fini(void)
{
}

/*
 * ---------------------------------------------------------------------------
 * screen
 * ---------------------------------------------------------------------------
 */

void screen_init(void)
{
    if(g_screen == NULL) {
        vga_set_mode(0x13);
        g_screen = MK_FP(0xA000, 0x0000);
    }
    if(g_screen != NULL) {
        uint16_t index = 0;
        uint16_t count = 256;
        for(index = 0; index < count; ++index) {
            vga_set_color(index, index, index, index);
        }
    }
}

void screen_fini(void)
{
    if(g_screen != NULL) {
        vga_set_mode(0x03);
        g_screen = NULL;
    }
}

/*
 * ---------------------------------------------------------------------------
 * image1
 * ---------------------------------------------------------------------------
 */

void image1_init(void)
{
    uint32_t w = SCREEN_W, x = 0, dx = 0;
    uint32_t h = SCREEN_H, y = 0, dy = 0;
    uint8_t color = 0;

    if(g_image1 == NULL) {
        g_image1 = (uint8_t far*) malloc(h * w);
    }
    if(g_image1 != NULL) {
        for(y = 0; y < h; ++y) {
            for(x = 0; x < w; ++x) {
                dx = ((w / 2) - x);
                dy = ((h / 2) - y);
                color = sqrt((dx * dx) + (dy * dy)) * 10;
                g_image1[((y * w) + x)] = color;
            }
        }
    }
}

void image1_fini(void)
{
    if(g_image1 != NULL) {
        free(g_image1);
        g_image1 = NULL;
    }
}

void image1_blit(void)
{
    uint32_t w = SCREEN_W;
    uint32_t h = SCREEN_H;

    (void) memcpy(g_screen, g_image1, (w * h));
}

/*
 * ---------------------------------------------------------------------------
 * image2
 * ---------------------------------------------------------------------------
 */

void image2_init(void)
{
    uint32_t w = SCREEN_W, x = 0, dx = 0;
    uint32_t h = SCREEN_H, y = 0, dy = 0;
    uint8_t color = 0;

    if(g_image2 == NULL) {
        g_image2 = (uint8_t far*) malloc(h * w);
    }
    if(g_image2 != NULL) {
        for(y = 0; y < h; ++y) {
            for(x = 0; x < w; ++x) {
                dx = ((w / 2) - x);
                dy = ((h / 2) - y);
                color = (1 + sin(sqrt((dx * dx) + (dy * dy)) / 8)) * 128;
                g_image2[((y * w) + x)] = color;
            }
        }
    }
}

void image2_fini(void)
{
    if(g_image2 != NULL) {
        free(g_image2);
        g_image2 = NULL;
    }
}

void image2_blit(void)
{
    uint32_t w = SCREEN_W;
    uint32_t h = SCREEN_H;

    (void) memcpy(g_screen, g_image2, (w * h));
}

/*
 * ---------------------------------------------------------------------------
 * plasma
 * ---------------------------------------------------------------------------
 */

void plasma_init(void)
{
    uint32_t w = PLASMA_W;
    uint32_t h = PLASMA_H;

    if(g_plasma == NULL) {
        g_plasma = (uint8_t far*) malloc(h * w);
    }
}

void plasma_fini(void)
{
    if(g_plasma != NULL) {
        free(g_plasma);
        g_plasma = NULL;
    }
}

void plasma_cmap(void)
{
    uint16_t index = 0;
    uint16_t count = 256;
    uint16_t pal_r = g_pal_r, inc_r = 257;
    uint16_t pal_g = g_pal_g, inc_g = 127;
    uint16_t pal_b = g_pal_b, inc_b = 509;

    for(index = 0; index < count; ++index) {
        pal_r += inc_r;
        pal_g += inc_g;
        pal_b += inc_b;
        vga_set_color(index, (pal_r >> 8), (pal_g >> 8), (pal_b >> 8));
    }
    g_pal_r += inc_r;
    g_pal_g += inc_g;
    g_pal_b += inc_b;
}

void plasma_draw(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3)
{
    uint16_t w = PLASMA_W, x = 0;
    uint16_t h = PLASMA_H, y = 0;
    uint8_t far* dst  = g_plasma;
    uint8_t far* src1 = NULL;
    uint8_t far* src2 = NULL;
    uint8_t far* src3 = NULL;
    uint8_t far* img1 = &g_image1[((y1 * SCREEN_W) + x1)];
    uint8_t far* img2 = &g_image2[((y2 * SCREEN_W) + x2)];
    uint8_t far* img3 = &g_image2[((y3 * SCREEN_W) + x3)];

    for(y = 0; y < h; ++y) {
        src1 = img1; img1 += SCREEN_W;
        src2 = img2; img2 += SCREEN_W;
        src3 = img3; img3 += SCREEN_W;
        for(x = 0; x < w; ++x) {
            *dst++ = *src1++
                   + *src2++
                   + *src3++
                   ;
        }
    }
}

void plasma_blit(void)
{
    uint16_t w = PLASMA_W, x = 0;
    uint16_t h = PLASMA_H, y = 0;
    uint8_t color = 0;
    uint8_t far* src  = g_plasma;
    uint8_t far* dst1 = (g_screen + (0 * SCREEN_W));
    uint8_t far* dst2 = (g_screen + (1 * SCREEN_W));

    for(y = 0; y < h; ++y) {
        for(x = 0; x < w; ++x) {
            color = *src++;
            *dst1++ = color;
            *dst1++ = color;
            *dst2++ = color;
            *dst2++ = color;
        }
        dst1 += SCREEN_W;
        dst2 += SCREEN_W;
    }
}

/*
 * ---------------------------------------------------------------------------
 * demo
 * ---------------------------------------------------------------------------
 */

void demo_begin(void)
{
    tables_init();
    image1_init();
    image2_init();
    plasma_init();
    screen_init();
}

void demo_loop(void)
{
    uint16_t cx = (PLASMA_W / 2);
    uint16_t cy = (PLASMA_H / 2);
    uint16_t x1 = cx;
    uint16_t y1 = cy;
    uint16_t x2 = cx;
    uint16_t y2 = cy;
    uint16_t x3 = cx;
    uint16_t y3 = cy;
    uint16_t angle1 = 0;
    uint16_t angle2 = 0;
    uint16_t angle3 = 0;

    while(kbhit() == 0) {
        angle1 = ((angle1 + 255) & 255);
        angle2 = ((angle2 + 257) & 255);
        angle3 = ((angle3 + 258) & 255);
        x1 = cx + ((80 * g_cos_table[angle1]) / 256);
        y1 = cy + ((50 * g_sin_table[angle1]) / 256);
        x2 = cx + ((70 * g_cos_table[angle2]) / 256);
        y2 = cy + ((40 * g_sin_table[angle2]) / 256);
        x3 = cx + ((60 * g_cos_table[angle3]) / 256);
        y3 = cy + ((30 * g_sin_table[angle3]) / 256);
        plasma_draw(x1, y1, x2, y2, x3, y3);
        vga_wait_vbl();
        plasma_cmap();
        plasma_blit();
    }
}

void demo_end(void)
{
    screen_fini();
    plasma_fini();
    image2_fini();
    image1_fini();
    tables_fini();
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

    demo_begin();
    demo_loop();
    demo_end();

    return 0;
}

/*
 * ---------------------------------------------------------------------------
 * End-Of-File
 * ---------------------------------------------------------------------------
 */
