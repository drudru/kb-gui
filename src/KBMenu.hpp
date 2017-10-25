
#pragma once

#include "common.h"

#include "NXCanvas.hpp"
#include "KBScreen.hpp"

#include "NXProtoIterator.hpp"
#include "NXUnixPacketSocket.hpp"

struct KBMenu
{
    KBScreen           * _screen     = nullptr;
    NXCanvas           * _canvas     = nullptr;
    NXUnixPacketSocket * _events     = nullptr;
private:
    //NXRect      * screen_rect = nullptr;
    //NXFontAtlas * font        = nullptr;
    //NXFontAtlas * bold_font   = nullptr;

    U8 curr_choice;        
    U8 last_choice;        

    //NXColor fg;
    //NXColor bg;

public:
    KBMenu(KBScreen * screen, NXUnixPacketSocket * evts)
    {
        _screen = screen;
        _canvas = _screen->canvas();
        _events = evts;
    }

    ~KBMenu()
    {
        // TODO: fix leaks
    }

    int render (const char * title, NXProtoIterator<char *> * pchoices, bool allow_cancel)
    {
        while (true)
        {
            draw_bkgnd(allow_cancel);
            draw_title(title);

            curr_choice = 0;
            last_choice = 0;
            draw_choices(pchoices);

            _screen->flush();

            // event loop
            while (true)
            {
                _events->send_msg("wait");
                auto msg = _events->recv_msg();

                fprintf(stderr, "KBMenu msg %s\n", msg._str);
                if (msg == "b0")
                {
                    if (curr_choice != 0)
                        curr_choice--;
                }
                else
                if (msg == "b1")
                {
                    if (curr_choice != last_choice)
                        curr_choice++;
                }
                else
                if (msg == "b2")
                {
                    if (allow_cancel)
                        return -1;
                }
                else
                if (msg == "b3")
                {
                    return curr_choice;
                }
                else
                if (msg == "wake")
                {
                    // redraw screen
                    break;
                }
                else
                if (msg == "")
                {
                    // kb-gui died, restart
                    usleep(200000);
                    exit(1);
                    break;
                }
                else
                {
                    fprintf(stderr, "KBMenu unhandled msg\n");
                }

                draw_choices(pchoices);
                _screen->flush();
            } // event loop

        } // draw menu loop
        
        return 0;
    }

    // Display a simple Status message (like a dialog)
    void display_status(const char * title)
    {
        NXColor prev_fg   = _canvas->state.fg;
        NXColor prev_bg   = _canvas->state.bg;
        _canvas->state.fg = prev_bg;
        _canvas->state.bg = prev_fg;

        // Text Rect Grid
        NXRect txt_grid = {{0,0},{12,2}};

        txt_grid = txt_grid.center_in(_screen->text_rect);

        // Todo - fix the text rect/gfx rect - simplify
        // Move towards gfx rect and eliminate txt grid
        // This will pave the way for non monospace font
        NXRect pix_grid = _canvas->font_rect_convert(&_screen->font, txt_grid);
        _canvas->fill_rect(&pix_grid, _canvas->state.bg);

        _canvas->draw_font_rect(&_screen->font, txt_grid);

        NXPoint pt = _screen->screen_rect.origin;
        pt.x += (txt_grid.origin.x + 1) * _screen->font.char_size.w;
        pt.y += (txt_grid.origin.y + 1) * _screen->font.char_size.h;
        _canvas->draw_font(&_screen->font, pt, title);

        _canvas->state.fg = prev_fg;
        _canvas->state.bg = prev_bg;
        _screen->flush();

        usleep(200000);
    }

    void draw_title(const char * title)
    {
        NXPoint pt = {0, 0};

        char tmp[] = ".";

        pt.x = ( 2) * _screen->font.char_size.w;
        //pt.y = ( 3) * font->char_size.h;

        tmp[0] = 180; 
        _canvas->draw_font(&_screen->font, pt, tmp);

        pt.x += ( 1) * _screen->font.char_size.w;

        _canvas->draw_font(&_screen->font, pt, title);

        pt.x += (strlen(title)) * _screen->font.char_size.w;
        tmp[0] = 195; 
        _canvas->draw_font(&_screen->font, pt, tmp);
    }

    void draw_bkgnd(bool allow_cancel)
    {
        _canvas->fill_rect(&_screen->screen_rect, _canvas->state.bg);

        // Text Rect Grid
        _canvas->draw_font_rect(&_screen->font, _screen->text_rect);

        /*
        // Draw menu choices at bottom
        char str[] = "X";

        NXPoint pt = _screen->screen_rect.origin;
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
        */
    }

    void draw_choices(NXProtoIterator<char *> * pchoices)
    {
        U8 index = 0;

        NXPoint pt = {0, 0};

        NXColor prev_fg   = _canvas->state.fg;
        NXColor prev_bg   = _canvas->state.bg;
        _canvas->state.fg = prev_bg;
        _canvas->state.bg = prev_fg;

        pt.x = ( 2) * _screen->font.char_size.w;
        pt.y = ( 3) * _screen->font.char_size.h;

        fprintf(stderr, "draw_choices\n");

        pchoices->rewind();

        char * choice = pchoices->get_next();
        while (choice)
        {
            if (index == curr_choice)
            {
                // Swap colors
                _canvas->state.fg = prev_bg;
                _canvas->state.bg = prev_fg;
            }

            _canvas->draw_font(&_screen->font, pt, choice);

            if (index == curr_choice)
            {
                // Restore normal colors
                _canvas->state.fg = prev_fg;
                _canvas->state.bg = prev_bg;
            }

            pt.y += (2) * _screen->font.char_size.h;
            index++;
            choice = pchoices->get_next();
        }

        last_choice = index - 1;

        _canvas->state.fg = prev_fg;
        _canvas->state.bg = prev_bg;
    }
};
