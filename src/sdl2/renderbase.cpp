#include "renderbase.hpp"
#include <iostream>

RenderBase::RenderBase()
{
    surface       = NULL;
    screen_pixels = NULL;

    orig_width  = 0;
    orig_height = 0;
}

// Setup screen size
bool RenderBase::sdl_screen_size()
{
    if (orig_width == 0 || orig_height == 0)
    {
	SDL_DisplayMode info;

	SDL_GetCurrentDisplayMode(0, &info);
        
        orig_width  = info.w; 
        orig_height = info.h;
    }

    scn_width  = orig_width;
    scn_height = orig_height;

    return true;
}

// See: SDL_PixelFormat
#define CURRENT_RGB() (r << Rshift) | (g << Gshift) | (b << Bshift);

void RenderBase::convert_palette(uint32_t adr, uint32_t r, uint32_t g, uint32_t b)
{
    adr >>= 1;

    r = (r * (255 / 31));
    g = (g * (255 / 31));
    b = (b * (255 / 31));
    
    rgb[adr] = (r << 8 | g << 3 | b >> 3);
      
    // Create shadow / highlight colours at end of RGB array
    // The resultant values are the same as MAME
    r = (r * 0.791);
    g = (g * 0.791);
    b = (b * 0.791);
    
    rgb[adr + S16_PALETTE_ENTRIES] = rgb[adr + (S16_PALETTE_ENTRIES * 2)] = (r << 8| g << 3| b >> 3);
}
