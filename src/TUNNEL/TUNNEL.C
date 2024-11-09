/*
 * tunnel.c - Copyright (c) 2024 - Olivier Poncet
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
 * low level bios functions
 * ---------------------------------------------------------------------------
 */

const volatile uint32_t* BIOS_TICKS = (const volatile uint32_t*) MK_FP(0x0040, 0x006C);

uint32_t get_bios_ticks(void)
{
    uint32_t ticks = 0;

    disable();
    ticks = *BIOS_TICKS;
    enable();

    return ticks;
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
    uint8_t status = 0;
    /* wait vbl */ {
        do {
            status = inportb(0x3da);
        } while((status & 0x08) == 0x00);
    }
}

void vga_wait_next_vbl(void)
{
    uint8_t status = 0;
    /* already in vbl */ {
        do {
            status = inportb(0x3da);
        } while((status & 0x08) != 0x00);
    }
    /* wait next vbl */ {
        do {
            status = inportb(0x3da);
        } while((status & 0x08) == 0x00);
    }
}

/*
 * ---------------------------------------------------------------------------
 * pcx file format
 * ---------------------------------------------------------------------------
 */

typedef struct _PCX_Header PCX_Header;
typedef struct _PCX_Footer PCX_Footer;
typedef struct _PCX_Reader PCX_Reader;

struct _PCX_Header
{
    uint8_t signature;
    uint8_t version;
    uint8_t encoding;
    uint8_t bits_per_plane;
    uint8_t x_min_l;
    uint8_t x_min_h;
    uint8_t y_min_l;
    uint8_t y_min_h;
    uint8_t x_max_l;
    uint8_t x_max_h;
    uint8_t y_max_l;
    uint8_t y_max_h;
    uint8_t horz_dpi_l;
    uint8_t horz_dpi_h;
    uint8_t vert_dpi_l;
    uint8_t vert_dpi_h;
    uint8_t palette[48];
    uint8_t reserved;
    uint8_t number_of_planes;
    uint8_t bytes_per_line_l;
    uint8_t bytes_per_line_h;
    uint8_t palette_info_l;
    uint8_t palette_info_h;
    uint8_t horz_screen_size_l;
    uint8_t horz_screen_size_h;
    uint8_t vert_screen_size_l;
    uint8_t vert_screen_size_h;
    uint8_t padding[54];
};

struct _PCX_Footer
{
    uint8_t signature;
    uint8_t palette[768];
};

struct _PCX_Reader
{
    int          status;
    PCX_Header   header;
    PCX_Footer   footer;
    uint16_t     dim_w;
    uint16_t     dim_h;
    uint16_t     stride;
    uint8_t far* pixels;
};

enum _PCX_Errors
{
    PCX_SUCCESS                     = 0x0000,
    PCX_FAILURE                     = 0x0100,
    PCX_BAD_FILENAME                = 0x0101,
    PCX_BAD_FILE                    = 0x0102,
    PCX_BAD_ALLOC                   = 0x0103,
    PCX_BAD_HEADER                  = 0x0104,
    PCX_BAD_HEADER_SIGNATURE        = 0x0105,
    PCX_BAD_HEADER_VERSION          = 0x0106,
    PCX_BAD_HEADER_ENCODING         = 0x0107,
    PCX_BAD_HEADER_BITS_PER_PLANE   = 0x0108,
    PCX_BAD_HEADER_X_MIN            = 0x0109,
    PCX_BAD_HEADER_Y_MIN            = 0x010a,
    PCX_BAD_HEADER_X_MAX            = 0x010b,
    PCX_BAD_HEADER_Y_MAX            = 0x010c,
    PCX_BAD_HEADER_HORZ_DPI         = 0x010d,
    PCX_BAD_HEADER_VERT_DPI         = 0x010e,
    PCX_BAD_HEADER_PALETTE          = 0x010f,
    PCX_BAD_HEADER_NUMBER_OF_PLANE  = 0x0110,
    PCX_BAD_HEADER_BYTES_PER_LINE   = 0x0111,
    PCX_BAD_HEADER_PALETTE_INFO     = 0x0112,
    PCX_BAD_HEADER_HORZ_SCREEN_SIZE = 0x0113,
    PCX_BAD_HEADER_VERT_SCREEN_SIZE = 0x0114,
    PCX_BAD_FOOTER                  = 0x0115,
    PCX_BAD_FOOTER_SIGNATURE        = 0x0116,
    PCX_BAD_FOOTER_PALETTE          = 0x0117,
};

void pcx_reader_init(PCX_Reader* reader)
{
    reader->status = 0;
    reader->dim_w  = 0;
    reader->dim_h  = 0;
    reader->stride = 0;
    reader->pixels = NULL;
}

void pcx_reader_fini(PCX_Reader* reader)
{
    reader->status = ~0;
    reader->dim_w  = ~0;
    reader->dim_h  = ~0;
    reader->stride = ~0;
    reader->pixels = free_buffer(reader->pixels);
}

void pcx_reader_load(PCX_Reader* reader, const char* filename)
{
    FILE* stream = NULL;

    /* check filename */ {
        if(reader->status == PCX_SUCCESS) {
            if((filename == NULL) || (*filename == '\0')) {
                reader->status = PCX_BAD_FILENAME;
            }
        }
    }
    /* open stream */ {
        if(reader->status == PCX_SUCCESS) {
            stream = fopen(filename, "rb");
            if(stream == NULL) {
                reader->status = PCX_BAD_FILE;
            }
        }
    }
    /* read header */ {
        PCX_Header* header = &reader->header;
        if(reader->status == PCX_SUCCESS) {
            const size_t header_size = sizeof(*header);
            const size_t header_read = fread(header, 1, header_size, stream);
            if(header_read != header_size) {
                reader->status = PCX_BAD_HEADER;
            }
        }
        if(reader->status == PCX_SUCCESS) {
            const uint16_t max_w  = (UINT16_T(0x10) << 8)
                                  | (UINT16_T(0x00) << 0)
                                  ;
            const uint16_t max_h  = (UINT16_T(0x10) << 8)
                                  | (UINT16_T(0x00) << 0)
                                  ;
            const uint16_t pal_i  = (UINT16_T(header->palette_info_h) << 8)
                                  | (UINT16_T(header->palette_info_l) << 0)
                                  ;
            const uint16_t min_x  = (UINT16_T(header->x_min_h) << 8)
                                  | (UINT16_T(header->x_min_l) << 0)
                                  ;
            const uint16_t min_y  = (UINT16_T(header->y_min_h) << 8)
                                  | (UINT16_T(header->y_min_l) << 0)
                                  ;
            const uint16_t max_x  = (UINT16_T(header->x_max_h) << 8)
                                  | (UINT16_T(header->x_max_l) << 0)
                                  ;
            const uint16_t max_y  = (UINT16_T(header->y_max_h) << 8)
                                  | (UINT16_T(header->y_max_l) << 0)
                                  ;
            const uint16_t stride = (UINT16_T(header->bytes_per_line_h) << 8)
                                  | (UINT16_T(header->bytes_per_line_l) << 0)
                                  ;
            do {
                if(header->signature != 0x0a) {
                    reader->status = PCX_BAD_HEADER_SIGNATURE;
                    break;
                }
                if(header->version != 0x05) {
                    reader->status = PCX_BAD_HEADER_VERSION;
                    break;
                }
                if(header->encoding != 0x01) {
                    reader->status = PCX_BAD_HEADER_ENCODING;
                    break;
                }
                if(header->bits_per_plane != 0x08) {
                    reader->status = PCX_BAD_HEADER_BITS_PER_PLANE;
                    break;
                }
                if(header->number_of_planes != 0x01) {
                    reader->status = PCX_BAD_HEADER_NUMBER_OF_PLANE;
                    break;
                }
                if(pal_i != 0x01) {
                    reader->status = PCX_BAD_HEADER_PALETTE_INFO;
                    break;
                }
                if(min_x > max_x) {
                    reader->status = PCX_BAD_HEADER_X_MIN;
                    break;
                }
                if(min_y > max_y) {
                    reader->status = PCX_BAD_HEADER_Y_MIN;
                    break;
                }
                if((reader->dim_w = ((max_x - min_x) + 1)) > max_w) {
                    reader->status = PCX_BAD_HEADER_X_MAX;
                    break;
                }
                if((reader->dim_h = ((max_y - min_y) + 1)) > max_h) {
                    reader->status = PCX_BAD_HEADER_Y_MAX;
                    break;
                }
                if((reader->stride = stride) < reader->dim_w) {
                    reader->status = PCX_BAD_HEADER_BYTES_PER_LINE;
                    break;
                }
            } while(0);
        }
    }
    /* alloc pixels */ {
        if(reader->status == PCX_SUCCESS) {
            if(reader->pixels != NULL) {
                reader->status = PCX_FAILURE;
            }
        }
        if(reader->status == PCX_SUCCESS) {
            reader->pixels = alloc_buffer(reader->dim_h, reader->stride);
            if(reader->pixels == NULL) {
                reader->status = PCX_BAD_ALLOC;
            }
        }
    }
    /* read pixels */ {
        if(reader->status == PCX_SUCCESS) {
            int          value = 0;
            uint8_t      count = 0;
            uint8_t      pixel = 0;
            uint32_t     bytes = (UINT32_T(reader->dim_h) * UINT32_T(reader->stride));
            uint8_t far* image = reader->pixels;
            while(bytes != 0) {
                if((value = fgetc(stream)) != EOF) {
                    count = 1;
                    pixel = (value & 0xff);
                }
                else {
                    reader->status = PCX_FAILURE;
                    break;
                }
                if((pixel & 0xc0) == 0xc0) {
                    if((value = fgetc(stream)) != EOF) {
                        count = (pixel & 0x3f);
                        pixel = (value & 0xff);
                    }
                    else {
                        reader->status = PCX_FAILURE;
                        break;
                    }
                }
                while((bytes != 0) && (count != 0)) {
                    *image++ = pixel;
                    --bytes;
                    --count;
                }
            }
        }
    }
    /* read footer */ {
        PCX_Footer* footer = &reader->footer;
        if(reader->status == PCX_SUCCESS) {
            const size_t footer_size = sizeof(*footer);
            const size_t footer_read = fread(footer, 1, footer_size, stream);
            if(footer_read != footer_size) {
                reader->status = PCX_BAD_FOOTER;
            }
        }
        if(reader->status == PCX_SUCCESS) {
            do {
                if(footer->signature != 0x0c) {
                    reader->status = PCX_BAD_FOOTER_SIGNATURE;
                    break;
                }
            } while(0);
        }
    }
    /* close stream */ {
        if(stream != NULL) {
            (void) fclose(stream);
            stream = NULL;
        }
    }
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
    uint16_t     off_x;
    uint16_t     off_y;
    uint16_t     inc_x;
    uint16_t     inc_y;
    uint8_t far* pixels;
    uint8_t far* depth;
    uint8_t far* angle;
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
        320,  /* dim_w  */
        200,  /* dim_h  */
        0,    /* off_x  */
        0,    /* off_y  */
        3,    /* inc_x  */
        1,    /* inc_y  */
        NULL, /* pixels */
        NULL, /* depth  */
        NULL  /* angle  */
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
        screen->pixels = MK_FP(0xA000, 0x0000);
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
        PCX_Reader reader = { 0 };
        pcx_reader_init(&reader);
        pcx_reader_load(&reader, "image.pcx");
        if(reader.status == PCX_SUCCESS) {
            effect->dim_w  = reader.dim_w;
            effect->dim_h  = reader.dim_h;
            effect->pixels = reader.pixels;
            reader.dim_w   = 0;
            reader.dim_h   = 0;
            reader.pixels  = NULL;
        }
        if(reader.status == PCX_SUCCESS) {
            uint16_t       index = 0;
            const uint16_t count = 256;
            uint8_t far*   value = reader.footer.palette;
            Color          color = { 0, 0, 0 };
            for(index = 0; index < count; ++index) {
                color.r = *value++;
                color.g = *value++;
                color.b = *value++;
                vga_set_color(index, color.r, color.g, color.b);
            }
        }
        pcx_reader_fini(&reader);
    }
    if(effect->pixels == NULL) {
        effect->pixels = alloc_buffer(effect->dim_h, effect->dim_w);
    }
    if(effect->depth == NULL) {
        effect->depth = alloc_buffer(effect->dim_h, effect->dim_w);
    }
    if(effect->depth != NULL) {
        const uint16_t dst_w = effect->dim_w;
        const uint16_t dst_h = effect->dim_h;
        const uint16_t mid_w = (dst_w / 2);
        const uint16_t mid_h = (dst_h / 2);
        const double   max_d = hypot(DOUBLE(mid_w), DOUBLE(mid_h));
        uint8_t far*   dst_p = effect->depth;
        uint16_t       dst_x = 0;
        uint16_t       dst_y = 0;
        for(dst_y = 0; dst_y < dst_h; ++dst_y) {
            const double dy = DOUBLE(INT16_T(dst_y) - INT16_T(mid_h));
            for(dst_x = 0; dst_x < dst_w; ++dst_x) {
                const double dx = DOUBLE(INT16_T(dst_x) - INT16_T(mid_w));
                const double dz = hypot(dx, dy) / max_d;
                *dst_p++ = 1 + UINT8_T(254.0 * dz);
            }
        }
    }
    if(effect->angle == NULL) {
        effect->angle = alloc_buffer(effect->dim_h, effect->dim_w);
    }
    if(effect->angle != NULL) {
        const uint16_t dst_w = effect->dim_w;
        const uint16_t dst_h = effect->dim_h;
        const uint16_t mid_w = (dst_w / 2);
        const uint16_t mid_h = (dst_h / 2);
        uint8_t far*   dst_p = effect->angle;
        uint16_t       dst_x = 0;
        uint16_t       dst_y = 0;
        for(dst_y = 0; dst_y < dst_h; ++dst_y) {
            const double dy = DOUBLE(INT16_T(dst_y) - INT16_T(mid_h));
            for(dst_x = 0; dst_x < dst_w; ++dst_x) {
                const double dx = DOUBLE(INT16_T(dst_x) - INT16_T(mid_w));
                if((dx != 0.0) || (dy != 0.0)) {
                    *dst_p++ = UINT8_T(128.0 * (atan2(dy, dx) / M_PI));
                }
                else {
                    *dst_p++ = UINT8_T(0);
                }
            }
        }
    }
}

void effect_fini(Effect* effect)
{
    if(effect->angle != NULL) {
        effect->angle = free_buffer(effect->angle);
    }
    if(effect->depth != NULL) {
        effect->depth = free_buffer(effect->depth);
    }
    if(effect->pixels != NULL) {
        effect->pixels = free_buffer(effect->pixels);
    }
}

void effect_update(Effect* effect)
{
    effect->off_x = (effect->off_x + effect->inc_x);
    effect->off_y = (effect->off_y + effect->inc_y);
}

void effect_render(Effect* effect, Screen* screen)
{
    const uint16_t     off_x = effect->off_x;
    const uint16_t     off_y = effect->off_y;
    const uint16_t     src_w = effect->dim_w;
    const uint16_t     src_h = effect->dim_h;
    const uint16_t     src_s = ((effect->dim_w + 1) & ~1);
    const uint8_t far* src_p = effect->pixels;
    const uint8_t far* src_d = effect->depth;
    const uint8_t far* src_a = effect->angle;
    const uint16_t     dst_w = screen->dim_w;
    const uint16_t     dst_h = screen->dim_h;
    const uint16_t     dst_s = ((screen->dim_w + 1) & ~1);
    uint8_t far*       dst_p = screen->pixels;
    uint16_t           cnt_x = 0;
    uint16_t           cnt_y = 0;

    /* wait for vbl */ {
        vga_wait_next_vbl();
    }
    /* render the effect */ {
        for(cnt_y = dst_h; cnt_y != 0; --cnt_y) {
            uint8_t far* dst_o = dst_p;
            for(cnt_x = dst_w; cnt_x != 0; --cnt_x) {
                const uint16_t tex_d = UINT16_T(*src_d++);
                const uint16_t tex_a = UINT16_T(*src_a++);
                if(tex_d > 16) {
                    const uint16_t tex_x = off_x + UINT16_T(16384) / UINT16_T(tex_d);
                    const uint16_t tex_y = off_y + tex_a;
                    const uint8_t  texel = src_p[(tex_y % src_h) * src_s + (tex_x % src_w)];
                    *dst_p++ = UINT16_T(texel);
                }
                else {
                    *dst_p++ = UINT16_T(0);
                }
            }
            dst_p = dst_o + dst_s;
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
