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
    uint16_t freq;
    uint16_t period;
    uint16_t counter;
    isr_t    old_isr;
} timer0 = {
    35,   /* freq    */
    0,    /* period  */
    0,    /* counter */
    NULL  /* old_isr */
};

void interrupt timer0_isr(void)
{
    /* increment counter */ {
        ++timer0.counter;
    }
    /* acknowledge interrupt */ {
        outportb(PIC_CONTROL_REG, 0x20);
    }
}

void timer0_init(void)
{
    const uint32_t clock     = 14318180UL;
    const uint32_t scale     = 12UL;
    const uint32_t frequency = timer0.freq;
    const uint32_t period    = (clock / (scale * frequency));

    if(timer0.old_isr == NULL) {
        const uint16_t timer0_int = 0x08;
        /* disable interrupts */ {
            disable();
        }
        /* set period/counter */ {
            timer0.period  = period;
            timer0.counter = 0;
        }
        /* set old handler */ {
            timer0.old_isr = getvect(timer0_int);
        }
        /* install new handler */ {
            setvect(timer0_int, timer0_isr);
            outportb(PIT_CONTROL_REG, 0x36);
            outportb(PIT_TIMER0_REG, ((timer0.period >> 0) & 0xff)); 
            outportb(PIT_TIMER0_REG, ((timer0.period >> 8) & 0xff));
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
        /* set period/counter */ {
            timer0.period  = 0;
            timer0.counter = 0;
        }
        /* restore old handler */ {
            setvect(timer0_int, timer0.old_isr);
            outportb(PIT_CONTROL_REG, 0x36);
            outportb(PIT_TIMER0_REG, ((timer0.period >> 0) & 0xff)); 
            outportb(PIT_TIMER0_REG, ((timer0.period >> 8) & 0xff));
        }
        /* set old handler */ {
            timer0.old_isr = NULL;
        }
        /* enable interrupts */ {
            enable();
        }
    }
}

uint16_t timer0_get(void)
{
    uint16_t counter = 0;

    /* critical section */ {
        disable();
        counter = timer0.counter;
        enable();
    }
    return counter;
}

/*
 * ---------------------------------------------------------------------------
 * low level vga functions
 * ---------------------------------------------------------------------------
 */

#define VGA_DAC_WR_INDEX 0x3c8
#define VGA_DAC_WR_VALUE 0x3c9
#define VGA_IS1_RD_VALUE 0x3da

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
        0,    /* v_mode */
        0,    /* p_mode */
        320,  /* dim_w  */
        200,  /* dim_h  */
        NULL  /* pixels */
    },
    /* effect */ {
        160,  /* dim_w  */
        104,  /* dim_h  */
        0,    /* random */
        NULL  /* pixels */
    },
};

/*
 * ---------------------------------------------------------------------------
 * color
 * ---------------------------------------------------------------------------
 */

double clamp(double val, double min, double max)
{
    if(val < min) {
        val = min;
    }
    if(val > max) {
        val = max;
    }
    return val;
}

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
    color->r = UINT8_T(255.0 * clamp(r, 0.0, 1.0));
    color->g = UINT8_T(255.0 * clamp(g, 0.0, 1.0));
    color->b = UINT8_T(255.0 * clamp(b, 0.0, 1.0));
}

void color_init_hsl(Color* color, double h, double s, double l)
{
    h = clamp(h, 0.0, 1.0);
    s = clamp(s, 0.0, 1.0);
    l = clamp(l, 0.0, 1.0);

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
        screen->pixels = MK_FP(0xA000, 0x0000);
    }
    if(screen->pixels != NULL) {
        uint16_t       index = 0;
        const uint16_t count = 256;
        const double   min_h = 0.0;
        const double   max_h = 1.0 / 6.0;
        const double   min_s = 0.5;
        const double   max_s = 1.0;
        const double   min_l = 0.0;
        const double   max_l = 1.0;
        Color          color = { 0, 0, 0 };
        for(index = 0; index < count; ++index) {
            const double v = DOUBLE(index) / 255.0;
            const double h = clamp((1.3 * (v / 6.0)), min_h, max_h);
            const double s = clamp((4.0 * (v / 1.0)), min_s, max_s);
            const double l = clamp((1.2 * (v / 1.0)), min_l, max_l);
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
    const uint16_t dst_w = effect->dim_w;
    const uint16_t dst_h = effect->dim_h;
    const uint16_t dst_s = ((effect->dim_w + 1) & ~1);
    uint8_t far*   dst_p = effect->pixels;
    uint16_t       cnt_x = 0;
    uint16_t       cnt_y = 0;

    /* update the effect */ {
        const uint16_t offset1 = (dst_s + 0);
        const uint16_t offset2 = (dst_s + 1);
        const uint16_t offset3 = (dst_s - 1);
        const uint16_t offset4 = (dst_s * 2);
        for(cnt_y = (dst_h - 2); cnt_y != 0; --cnt_y) {
            uint8_t far* dst_o = dst_p;
            /* left column*/ {
                const uint16_t v1 = dst_p[offset1];
                const uint16_t v2 = dst_p[offset2];
                const uint16_t v3 = dst_p[offset2];
                const uint16_t v4 = dst_p[offset4];
                *dst_p++ = UINT8_T(((v1 + v2 + v3 + v4) * 61) >> 8);
            }
            for(cnt_x = (dst_w - 2); cnt_x != 0; --cnt_x) {
                const uint16_t v1 = dst_p[offset1];
                const uint16_t v2 = dst_p[offset2];
                const uint16_t v3 = dst_p[offset3];
                const uint16_t v4 = dst_p[offset4];
                *dst_p++ = UINT8_T(((v1 + v2 + v3 + v4) * 61) >> 8);
            }
            /* right column*/ {
                const uint16_t v1 = dst_p[offset1];
                const uint16_t v2 = dst_p[offset3];
                const uint16_t v3 = dst_p[offset3];
                const uint16_t v4 = dst_p[offset4];
                *dst_p++ = UINT8_T(((v1 + v2 + v3 + v4) * 61) >> 8);
            }
            dst_p = dst_o + dst_s;
        }
    }
    /* update the two last lines */ {
        uint16_t random = effect->random;
        for(cnt_y = 2; cnt_y != 0; --cnt_y) {
            uint8_t far* dst_o = dst_p;
            for(cnt_x = dst_w; cnt_x != 0; --cnt_x) {
                random = ((random * 137) + 187);
                *dst_p++ = (128 + (UINT8_T(random >> 9) & 0x7f));
            }
            dst_p = dst_o + dst_s;
        }
        effect->random = random;
    }
}

void effect_render(Effect* effect, Screen* screen)
{
    const uint16_t     src_w = effect->dim_w - 0;
    const uint16_t     src_h = effect->dim_h - 4;
    const uint16_t     src_s = ((src_w + 1) & ~1);
    const uint8_t far* src_p = effect->pixels;
    const uint16_t     dst_w = screen->dim_w;
    const uint16_t     dst_h = screen->dim_h;
    const uint16_t     dst_s = ((dst_w + 1) & ~1);
    uint8_t far*       dst_p = screen->pixels;
    uint16_t           cnt_x = 0;
    uint16_t           cnt_y = 0;
    uint16_t           err_x = 0;
    uint16_t           err_y = 0;

    /* wait for vbl */ {
        vga_wait_next_vbl();
    }
    /* blit the effect */ {
        for(cnt_y = dst_h; cnt_y != 0; --cnt_y) {
            uint8_t far*       dst_o = dst_p;
            const uint8_t far* src_o = src_p;
            for(cnt_x = dst_w; cnt_x != 0; --cnt_x) {
                *dst_p++ = *src_p;
                if((err_x += src_w) >= dst_w) {
                    err_x -= dst_w;
                    src_p += 1;
                }
            }
            dst_p = dst_o + dst_s;
            src_p = src_o;
            if((err_y += src_h) >= dst_h) {
                err_y -= dst_h;
                src_p += src_s;
            }
        }
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
    timer0_init();
    screen_init(&program->screen);
    effect_init(&program->effect);
}

void program_loop(Program* program)
{
    uint16_t timestamp = 0;

    while(kbhit() == 0) {
        timestamp = timer0_get();
        effect_update(&program->effect);
        while(timer0_get() == timestamp) {
            vga_wait_next_hbl();
        }
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
