
#pragma once

#include "common.h"

#include "NXCanvas.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include "mmapGpio.hpp"

#include "ProtoIterator.hpp"

struct KBMenu
{
    NXCanvas * canvas      = nullptr;
private:
    NXRect   * screen_rect = nullptr;
    NXFontAtlas * font = nullptr;
    NXFontAtlas * bold_font = nullptr;

    U8 curr_choice;        
    U8 last_choice;        

    NXColor fg;
    NXColor bg;

    mmapGpio _rpiGpio;

public:
    KBMenu(NXCanvas * cnvs)
    {
        canvas = cnvs;

        // https://en.wikipedia.org/wiki/Video_Graphics_Array#Color_palette
        bg = NXColor{   0,   0, 170, 255}; // Blue
        fg = NXColor{ 255, 255,  85, 255}; // Yellow
        canvas->state.bg = bg;
        canvas->state.fg = fg;
        canvas->state.mono_color_txform = true;

        screen_rect = &cnvs->bitmap.rect;

        // Load image with 1 byte per pixel
        int stb_width, stb_height, stb_bpp;
        //unsigned char* font = stbi_load( "5271font.png", &stb_width, &stb_height, &stb_bpp, 1 );

        // Wyse Font
        char font_path[] = "wy700font.png";
        unsigned char* font_stb_bmp = stbi_load(font_path, &stb_width, &stb_height, &stb_bpp, 1 );
        if (!font_stb_bmp) {
            fprintf(stderr, "missing font file %s\n", font_path);
            exit(1);
        }

        NXBitmap * font_bmp    = nullptr;
        int16_t width  = stb_width;
        int16_t height = stb_height;
        int8_t  chans  = stb_bpp;
        font_bmp =  new NXBitmap { (uint8_t *)font_stb_bmp, {0, 0, width, height}, NXColorChan::GREY1 };
        //printf("font: %d x %d\n", width, height);

        // Invert the Font Atlas
        {
            U8 * tmp_mem = (U8 *)malloc(width * height);
            memset(tmp_mem, 0xff, width * height);
            NXBitmap tmp_bmp =  { tmp_mem, {0, 0, width, height}, NXColorChan::GREY1 };

            NXCanvasROP tmp_rop = canvas->state.rop;
            canvas->state.rop = NXCanvasROP::XOR;

            NXBlit::blit(&tmp_bmp, &font_bmp->rect, // src
                         font_bmp, &font_bmp->rect, // dst
                         &canvas->state);

            canvas->state.rop = tmp_rop;
            free(tmp_mem);
        }

        font = new NXFontAtlas();
        font->atlas = font_bmp;
        font->rect  = { { 0, 128 }, { 512, 128 } };
        font->size  = { 32, 8 };
        font->init();

        bold_font = new NXFontAtlas();
        bold_font->atlas = font_bmp;
        bold_font->rect  = { { 0, 0 }, { 512, 128 } };
        bold_font->size  = { 32, 8 };
        bold_font->init();

        printf("font char: %d x %d\n", bold_font->char_size.w, bold_font->char_size.h);


        _rpiGpio.setPinDir(17, mmapGpio::INPUT);
        _rpiGpio.setPinPUD(17, mmapGpio::PUD_UP);

        _rpiGpio.setPinDir(22, mmapGpio::INPUT);
        _rpiGpio.setPinPUD(22, mmapGpio::PUD_UP);

        _rpiGpio.setPinDir(23, mmapGpio::INPUT);
        _rpiGpio.setPinPUD(23, mmapGpio::PUD_UP);

        _rpiGpio.setPinDir(27, mmapGpio::INPUT);
        _rpiGpio.setPinPUD(27, mmapGpio::PUD_UP);
    }

    ~KBMenu()
    {
        // TODO: fix leaks
    }

    int render (const char * title, ProtoIterator<char *> * pchoices, bool allow_cancel)
    {
        // nano-event-loop
        bool sleep_mode = false;
        while (true)
        {
            draw_bkgnd(allow_cancel);
            draw_title(title);

            curr_choice = 0;
            last_choice = 0;
            draw_choices(pchoices);

            unsigned int counter = 0;

            int button = -1;
            while(!sleep_mode)
            {
                usleep(100000); //delay for 0.1 seconds

                if (_rpiGpio.readPin(17) == mmapGpio::LOW)
                    button = 0;
                else
                if (_rpiGpio.readPin(22) == mmapGpio::LOW)
                    button = 1;
                else
                if (_rpiGpio.readPin(23) == mmapGpio::LOW)
                    button = 2;
                else
                if (_rpiGpio.readPin(27) == mmapGpio::LOW)
                    button = 3;


                if (button == -1)
                {
                    counter++;
                    if (counter > 100)
                    {
                        sleep_mode = true;
                        canvas->fill_rect(screen_rect, NXColor{0,0,0,1});
                    }
                }
                else
                {
                    counter = 0;
                    printf("Button! %d\n", button);

                    // Wait for all to go to HIGH
                    // Ghetto debouncing...
                    while (true)
                    {
                        usleep(100000); //delay for 0.1 seconds
                        if ( true
                                && (_rpiGpio.readPin(17) == mmapGpio::HIGH)
                                && (_rpiGpio.readPin(22) == mmapGpio::HIGH)
                                && (_rpiGpio.readPin(23) == mmapGpio::HIGH)
                                && (_rpiGpio.readPin(27) == mmapGpio::HIGH)
                           )
                            break;
                    }

                    if ((button == 0) && (curr_choice != 0))
                        curr_choice--;
                    else
                    if ((button == 1) && (curr_choice != last_choice))
                        curr_choice++;

                    if (allow_cancel && (button == 2))
                        return -1;
                    if (button == 3)
                        return curr_choice;

                    draw_choices(pchoices);

                    // Reset button
                    button = -1;
                }
            }

            while (sleep_mode)
            {
                usleep(100000); //delay for 0.1 seconds
                if ( false
                     || (_rpiGpio.readPin(17) == mmapGpio::LOW)
                     || (_rpiGpio.readPin(22) == mmapGpio::LOW)
                     || (_rpiGpio.readPin(23) == mmapGpio::LOW)
                     || (_rpiGpio.readPin(27) == mmapGpio::LOW)
                         )
                 sleep_mode = false;
            }
        }
        
        return 0;
    }

    void display_status(const char * title)
    {
        NXColor prev_bg = canvas->state.bg;
        canvas->state.bg = NXColor{ 0xff, 0x55, 0x55, 0xff}; // Yellow

        // Text Rect Grid
        NXRect txt_grid = {{4,6},{12,3}};

        // Todo - fix the text rect/gfx rect - simplify
        // Move towards gfx rect and eliminate txt grid
        // This will pave the way for non monospace font
        NXRect pix_grid = canvas->font_rect_convert(font, txt_grid);
        canvas->fill_rect(&pix_grid, canvas->state.bg);

        canvas->draw_font_rect(font, txt_grid);

        NXPoint pt = screen_rect->origin;
        pt.x += ( 5) * font->char_size.w;
        pt.y += ( 7) * font->char_size.h;
        canvas->draw_font(font, pt, title);

        canvas->state.bg = prev_bg;
        usleep(500000);
    }

    void draw_title(const char * title)
    {
        NXPoint pt = {0, 0};

        char tmp[] = ".";

        pt.x = ( 2) * font->char_size.w;
        //pt.y = ( 3) * font->char_size.h;

        tmp[0] = 180; 
        canvas->draw_font(bold_font, pt, tmp);

        pt.x += ( 1) * font->char_size.w;

        canvas->draw_font(bold_font, pt, title);

        pt.x += (strlen(title)) * font->char_size.w;
        tmp[0] = 195; 
        canvas->draw_font(bold_font, pt, tmp);
    }

    void draw_bkgnd(bool allow_cancel)
    {
        //canvas->fill_rect(screen_rect, NXColor{0,0,255,1});
        //canvas->draw_font(font, NXPoint{0,0}, "Testing 1 2 3");

        canvas->fill_rect(screen_rect, canvas->state.bg);

        // Text Rect Grid
        canvas->draw_font_rect(font, NXRect{{0,0},{20,15}});

        char str[] = "X";

        NXPoint pt = screen_rect->origin;
        pt.x += ( 2) * font->char_size.w;
        pt.y += (14) * font->char_size.h;
        str[0] = 30; // Up
        canvas->draw_font(font, pt, str);

        pt.x += ( 4) * font->char_size.w;
        str[0] = 31; // Down
        canvas->draw_font(font, pt, str);

        pt.x += ( 6) * font->char_size.w;
        str[0] = 17; // Left - Cancel
        if (allow_cancel)
            canvas->draw_font(font, pt, str);

        pt.x += ( 4) * font->char_size.w;
        canvas->draw_font(font, pt, "ok");
    }

    void draw_choices(ProtoIterator<char *> * pchoices)
    {
        U8 index = 0;

        NXPoint pt = {0, 0};

        pt.x = ( 2) * font->char_size.w;
        pt.y = ( 3) * font->char_size.h;

        printf("draw_choices\n");

        pchoices->rewind();

        char * choice = pchoices->get_next();
        while (choice)
        {
            if (index == curr_choice)
            {
                // Swap colors
                canvas->state.fg = bg;
                canvas->state.bg = fg;
            }

            canvas->draw_font(font, pt, choice);

            if (index == curr_choice)
            {
                // Restore normal colors
                canvas->state.fg = fg;
                canvas->state.bg = bg;
            }

            pt.y += (2) * font->char_size.h;
            index++;
            choice = pchoices->get_next();
        }

        last_choice = index - 1;
    }
};
