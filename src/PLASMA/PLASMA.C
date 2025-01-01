/*
 * plasma.c - Copyright (c) 2024-2025 - Olivier Poncet
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
#include <string.h>
#include <math.h>
#include <dos.h>

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

typedef void interrupt (*isr_t)(void);

/*
 * ---------------------------------------------------------------------------
 * some useful macros
 * ---------------------------------------------------------------------------
 */

#define IGNORE(expression)   ((void)(expression))
#define DOUBLE(expression)   ((double)(expression))
#define SIZE_T(expression)   ((size_t)(expression))
#define INT8_T(expression)   ((int8_t)(expression))
#define INT16_T(expression)  ((int16_t)(expression))
#define INT32_T(expression)  ((int32_t)(expression))
#define UINT8_T(expression)  ((uint8_t)(expression))
#define UINT16_T(expression) ((uint16_t)(expression))
#define UINT32_T(expression) ((uint32_t)(expression))

/*
 * ---------------------------------------------------------------------------
 * low level pit functions
 * ---------------------------------------------------------------------------
 */

#define PIT_TIMER0_REG  0x40
#define PIT_TIMER1_REG  0x41
#define PIT_TIMER2_REG  0x42
#define PIT_CONTROL_REG 0x43
#define PIC_CONTROL_REG 0x20

struct Timer0 {
    uint16_t ival;
    uint32_t msec;
    isr_t    old_isr;
} timer0 = {
    0,    /* ival    */
    0,    /* msec    */
    NULL  /* old_isr */
};

void interrupt timer0_isr(void)
{
    /* update msec */ {
        timer0.msec += timer0.ival;
    }
    /* acknowledge interrupt */ {
        outportb(PIC_CONTROL_REG, 0x20);
    }
}

void timer0_init(void)
{
    const uint32_t clock     = 14318180UL;
    const uint32_t scale     = 12UL;
    const uint32_t frequency = 50UL;
    const uint32_t period    = (clock / (scale * frequency));

    if(timer0.old_isr == NULL) {
        const uint16_t timer0_int = 0x08;
        /* disable interrupts */ {
            disable();
        }
        /* set ival/msec */ {
            timer0.ival = (((10000 / frequency) + 5) / 10);
            timer0.msec = 0;
        }
        /* set old handler */ {
            timer0.old_isr = getvect(timer0_int);
        }
        /* install new handler */ {
            setvect(timer0_int, timer0_isr);
            outportb(PIT_CONTROL_REG, 0x36);
            outportb(PIT_TIMER0_REG, ((period >> 0) & 0xff)); 
            outportb(PIT_TIMER0_REG, ((period >> 8) & 0xff));
        }
        /* enable interrupts */ {
            enable();
        }
    }
}

void timer0_fini(void)
{
    if(timer0.old_isr != NULL) {
        const uint16_t timer0_int = 0x08;
        /* disable interrupts */ {
            disable();
        }
        /* set ival/msec */ {
            timer0.ival = 0;
            timer0.msec = 0;
        }
        /* restore old handler */ {
            setvect(timer0_int, timer0.old_isr);
            outportb(PIT_CONTROL_REG, 0x36);
            outportb(PIT_TIMER0_REG, 0x00); 
            outportb(PIT_TIMER0_REG, 0x00);
        }
        /* set old handler */ {
            timer0.old_isr = NULL;
        }
        /* enable interrupts */ {
            enable();
        }
    }
}

uint32_t timer0_get_msec(void)
{
    uint32_t msec = 0;

    /* critical section */ {
        disable();
        msec = timer0.msec;
        enable();
    }
    return msec;
}

/*
 * ---------------------------------------------------------------------------
 * low level vga functions
 * ---------------------------------------------------------------------------
 */

#define VGA_DAC_WR_INDEX 0x3c8
#define VGA_DAC_WR_VALUE 0x3c9
#define VGA_IS1_RD_VALUE 0x3da

uint8_t far* vga_get_addr(void)
{
    return (uint8_t far*) MK_FP(0xA000, 0x0000);
}

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
    outportb(VGA_DAC_WR_INDEX, color);
    outportb(VGA_DAC_WR_VALUE, (r >> 2));
    outportb(VGA_DAC_WR_VALUE, (g >> 2));
    outportb(VGA_DAC_WR_VALUE, (b >> 2));
}

void vga_wait_next_hbl(void)
{
    uint8_t status = 0;

    /* already in hbl */ {
        do {
            status = inportb(VGA_IS1_RD_VALUE);
        } while((status & 0x01) != 0x00);
    }
    /* wait next hbl */ {
        do {
            status = inportb(VGA_IS1_RD_VALUE);
        } while((status & 0x01) == 0x00);
    }
}

void vga_wait_next_vbl(void)
{
    uint8_t status = 0;

    /* already in vbl */ {
        do {
            status = inportb(VGA_IS1_RD_VALUE);
        } while((status & 0x08) != 0x00);
    }
    /* wait next vbl */ {
        do {
            status = inportb(VGA_IS1_RD_VALUE);
        } while((status & 0x08) == 0x00);
    }
}

/*
 * ---------------------------------------------------------------------------
 * some useful functions
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
    uint16_t     pitch;
    uint8_t far* pixels;
};

struct _Effect
{
    uint16_t     dim_w;
    uint16_t     dim_h;
    uint16_t     pitch;
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
    uint16_t     pitch;
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
        0,    /* v_mode */
        0,    /* p_mode */
        320,  /* dim_w  */
        200,  /* dim_h  */
        320,  /* pitch  */
        NULL  /* pixels */
    },
    /* effect */ {
        160,  /* dim_w  */
        100,  /* dim_h  */
        160,  /* pitch  */
        0,    /* pal_r  */
        0,    /* pal_g  */
        0,    /* pal_b  */
        101,  /* inc_r  */
        127,  /* inc_g  */
        257,  /* inc_b  */
        NULL  /* pixels */
    },
    /* image1 */ {
        320,  /* dim_w  */
        200,  /* dim_h  */
        320,  /* pitch  */
        0,    /* pos_x  */
        0,    /* pos_y  */
        0,    /* angle  */
        -5,   /* speed  */
        NULL  /* pixels */
    },
    /* image2 */ {
        320,  /* dim_w  */
        200,  /* dim_h  */
        320,  /* pitch  */
        0,    /* pos_x  */
        0,    /* pos_y  */
        0,    /* angle  */
        +2,   /* speed  */
        NULL  /* pixels */
    },
    /* image3 */ {
        320,  /* dim_w  */
        200,  /* dim_h  */
        320,  /* pitch  */
        0,    /* pos_x  */
        0,    /* pos_y  */
        0,    /* angle  */
        +3,   /* speed  */
        NULL  /* pixels */
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
        screen->pixels = vga_get_addr();
    }
    if(screen->pixels != NULL) {
        uint16_t       index = 0;
        const uint16_t count = 256;
        Color          color = { 0, 0, 0 };
        for(index = 0; index < count; ++index) {
            color.r = index;
            color.g = index;
            color.b = index;
            vga_set_color(index, color.r, color.g, color.b);
        }
    }
    if(screen->pixels != NULL) {
        const uint16_t dst_w = screen->dim_w;
        const uint16_t dst_h = screen->dim_h;
        const uint16_t dst_s = screen->pitch;
        uint8_t far*   dst_p = screen->pixels;
        uint16_t       dst_x = 0;
        uint16_t       dst_y = 0;
        for(dst_y = 0; dst_y < dst_h; ++dst_y) {
            uint8_t far* dst_o = dst_p;
            for(dst_x = 0; dst_x < dst_w; ++dst_x) {
                *dst_p++ = UINT8_T(0);
            }
            dst_p = dst_o + dst_s;
        }
    }
}

void screen_fini(Screen* screen)
{
    if(screen->pixels != NULL) {
        screen->v_mode = screen->p_mode;
        screen->p_mode = vga_set_mode(screen->v_mode);
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
        effect->pixels = alloc_buffer(effect->dim_h, effect->pitch);
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

void effect_render(Effect* effect, Program* program)
{
    const uint16_t     bpl_1 = program->image1.pitch;
    const uint8_t far* src_1 = &program->image1.pixels[(program->image1.pos_y * bpl_1) + program->image1.pos_x];
    const uint16_t     bpl_2 = program->image2.pitch;
    const uint8_t far* src_2 = &program->image2.pixels[(program->image2.pos_y * bpl_2) + program->image2.pos_x];
    const uint16_t     bpl_3 = program->image3.pitch;
    const uint8_t far* src_3 = &program->image3.pixels[(program->image3.pos_y * bpl_3) + program->image3.pos_x];
    const uint16_t     dst_w = effect->dim_w;
    const uint16_t     dst_h = effect->dim_h;
    const uint16_t     dst_s = effect->pitch;
    uint8_t far*       dst_p = effect->pixels;
    uint16_t           cnt_x = 0;
    uint16_t           cnt_y = 0;

    /* update the effect */ {
        for(cnt_y = dst_h; cnt_y != 0; --cnt_y) {
            uint8_t far*       dst_o = dst_p;
            const uint8_t far* img_1 = src_1;
            const uint8_t far* img_2 = src_2;
            const uint8_t far* img_3 = src_3;
            for(cnt_x = dst_w; cnt_x != 0; --cnt_x) {
                *dst_p++ = *src_1++
                         + *src_2++
                         + *src_3++
                         ;
            }
            dst_p = dst_o + dst_s;
            src_1 = img_1 + bpl_1;
            src_2 = img_2 + bpl_2;
            src_3 = img_3 + bpl_3;
        }
    }
}

void effect_putscr(Effect* effect, Screen* screen)
{
    /* wait for vbl */ {
        vga_wait_next_vbl();
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
    /* blit the effect */ {
        const uint16_t     src_w = effect->dim_w;
        const uint16_t     src_h = effect->dim_h;
        const uint16_t     src_s = effect->pitch;
        const uint8_t far* src_p = effect->pixels;
        const uint16_t     dst_s = screen->pitch;
        uint8_t far*       dst_p = screen->pixels;
        uint16_t           cnt_x = 0;
        uint16_t           cnt_y = 0;
        for(cnt_y = src_h; cnt_y != 0; --cnt_y) {
            uint8_t far*       dst_o = dst_p;
            const uint8_t far* src_o = src_p;
            for(cnt_x = src_w, src_p = src_o; cnt_x != 0; --cnt_x) {
                const uint8_t pixel = *src_p++;
                *dst_p++ = dst_p[dst_s] = pixel;
                *dst_p++ = dst_p[dst_s] = pixel;
            }
            dst_p = dst_o + (dst_s << 1);
            src_p = src_o + (src_s << 0);
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
        image1->pixels = alloc_buffer(image1->dim_h, image1->pitch);
    }
    if(image1->pixels != NULL) {
        const uint16_t dst_w = image1->dim_w;
        const uint16_t dst_h = image1->dim_h;
        const uint16_t dst_s = image1->pitch;
        uint8_t far*   dst_p = image1->pixels;
        uint16_t       dst_x = 0;
        uint16_t       dst_y = 0;
        for(dst_y = 0; dst_y < dst_h; ++dst_y) {
            uint8_t far* dst_o = dst_p;
            for(dst_x = 0; dst_x < dst_w; ++dst_x) {
                const int32_t dx = (INT32_T(dst_x) - INT32_T(dst_w / 2));
                const int32_t dy = (INT32_T(dst_y) - INT32_T(dst_h / 2));
                *dst_p++ = UINT8_T(hypot(dx, dy) * 7.0);
            }
            dst_p = dst_o + dst_s;
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
        image2->pixels = alloc_buffer(image2->dim_h, image2->pitch);
    }
    if(image2->pixels != NULL) {
        const uint16_t dst_w = image2->dim_w;
        const uint16_t dst_h = image2->dim_h;
        const uint16_t dst_s = image2->pitch;
        uint8_t far*   dst_p = image2->pixels;
        uint16_t       dst_x = 0;
        uint16_t       dst_y = 0;
        for(dst_y = 0; dst_y < dst_h; ++dst_y) {
            uint8_t far* dst_o = dst_p;
            for(dst_x = 0; dst_x < dst_w; ++dst_x) {
                const int32_t dx = (INT32_T(dst_x) - INT32_T(dst_w / 2));
                const int32_t dy = (INT32_T(dst_y) - INT32_T(dst_h / 2));
                *dst_p++ = UINT8_T((1.0 + sin(hypot(dx, dy) / 11.0)) * 127.5);
            }
            dst_p = dst_o + dst_s;
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
        image3->pixels = alloc_buffer(image3->dim_h, image3->pitch);
    }
    if(image3->pixels != NULL) {
        const uint16_t dst_w = image3->dim_w;
        const uint16_t dst_h = image3->dim_h;
        const uint16_t dst_s = image3->pitch;
        uint8_t far*   dst_p = image3->pixels;
        uint16_t       dst_x = 0;
        uint16_t       dst_y = 0;
        for(dst_y = 0; dst_y < dst_h; ++dst_y) {
            uint8_t far* dst_o = dst_p;
            for(dst_x = 0; dst_x < dst_w; ++dst_x) {
                const int32_t dx = (INT32_T(dst_x) - INT32_T(dst_w / 2));
                const int32_t dy = (INT32_T(dst_y) - INT32_T(dst_h / 2));
                *dst_p++ = UINT8_T((1.0 + sin(hypot(dx, dy) / 19.0)) * 127.5);
            }
            dst_p = dst_o + dst_s;
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
    timer0_init();
    screen_init(&program->screen);
    effect_init(&program->effect);
    image1_init(&program->image1);
    image2_init(&program->image2);
    image3_init(&program->image3);
}

void program_loop(Program* program)
{
    const uint16_t fps      = 35;
    const uint16_t duration = (((10000 / fps) + 5) / 10);
    uint32_t       now      = timer0_get_msec();
    uint32_t       deadline = now + duration;
    const int16_t  px       = ((program->effect.dim_w / 2) + 0);
    const int16_t  py       = ((program->effect.dim_h / 2) + 0);
    const int16_t  dw       = ((program->effect.dim_w / 2) - 1);
    const int16_t  dh       = ((program->effect.dim_h / 2) - 1);


    while(kbhit() == 0) {
        image1_update(&program->image1, px, py, dw, dh);
        image2_update(&program->image2, px, py, dw, dh);
        image3_update(&program->image3, px, py, dw, dh);
        effect_update(&program->effect);
        while((now = timer0_get_msec()) < deadline) {
            vga_wait_next_hbl();
        }
        if((deadline += duration) > now) {
            effect_render(&program->effect, program);
            effect_putscr(&program->effect, &program->screen);
        }
        else {
            deadline += duration;
        }
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
    timer0_fini();
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
