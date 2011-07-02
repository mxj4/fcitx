/***************************************************************************
 *   Copyright (C) 2010~2011 by CSSlayer                                   *
 *   wengxt@gmail.com                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <cairo.h>
#include <fcitx-utils/utf8.h>
#include <fcitx-config/fcitx-config.h>
#include "cairostuff.h"

#ifdef _ENABLE_PANGO
#include <pango/pangocairo.h>
#endif

int
StringWidth(const char *str, const char *font, int fontSize)
{
    if (!str || str[0] == 0)
        return 0;
    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, 10, 10);
    cairo_t        *c = cairo_create(surface);
    SetFontContext(c, font, fontSize);

    int             width = StringWidthWithContext(c, str);
    ResetFontContext();

    cairo_destroy(c);
    cairo_surface_destroy(surface);

    return width;
}

#ifdef _ENABLE_PANGO
int
StringWidthWithContextReal(cairo_t * c, PangoFontDescription* fontDesc, const char *str)
{
    if (!str || str[0] == 0)
        return 0;
    if (!utf8_check_string(str))
        return 0;

    int             width;
    PangoLayout *layout = pango_cairo_create_layout (c);
    pango_layout_set_text (layout, str, -1);
    pango_layout_set_font_description (layout, fontDesc);
    pango_layout_get_pixel_size(layout, &width, NULL);
    g_object_unref (layout);

    return width;
}
#else

int
StringWidthWithContextReal(cairo_t * c, const char *str)
{
    if (!str || str[0] == 0)
        return 0;
    if (!utf8_check_string(str))
        return 0;
    cairo_text_extents_t extents;
    cairo_text_extents(c, str, &extents);
    int             width = extents.x_advance;
    return width;
}
#endif

int
FontHeight(const char *font, int fontSize)
{
    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, 10, 10);
    cairo_t        *c = cairo_create(surface);

    SetFontContext(c, font, fontSize);
    int             height = FontHeightWithContext(c);
    ResetFontContext();

    cairo_destroy(c);
    cairo_surface_destroy(surface);
    return height;
}

#ifdef _ENABLE_PANGO
int
FontHeightWithContextReal(cairo_t* c, PangoFontDescription* fontDesc)
{
    int height;

    if (pango_font_description_get_size_is_absolute(fontDesc)) /* it must be this case */
    {
        height = pango_font_description_get_size(fontDesc);
        height /= PANGO_SCALE;
    }
    else
        height = 0;

    return height;
}
#else

int
FontHeightWithContextReal(cairo_t * c)
{
    cairo_matrix_t matrix;
    cairo_get_font_matrix (c, &matrix);

    int             height = matrix.xx;
    return height;
}
#endif

/*
 * 以指定的颜色在窗口的指定位置输出字串
 */
void
OutputString(cairo_t * c, const char *str, const char *font, int fontSize, int x,
             int y, ConfigColor * color)
{
    if (!str || str[0] == 0)
        return;

    cairo_save(c);

    cairo_set_source_rgb(c, color->r, color->g, color->b);
    SetFontContext(c, font, fontSize);
    OutputStringWithContext(c, str, x, y);
    ResetFontContext();

    cairo_restore(c);
}

#ifdef _ENABLE_PANGO
void
OutputStringWithContextReal(cairo_t * c, PangoFontDescription* desc, const char *str, int x, int y)
{
    if (!str || str[0] == 0)
        return;
    if (!utf8_check_string(str))
        return;
    cairo_save(c);

    PangoLayout *layout;

    layout = pango_cairo_create_layout (c);
    pango_layout_set_text (layout, str, -1);
    pango_layout_set_font_description (layout, desc);
    cairo_move_to(c, x, y);
    pango_cairo_show_layout (c, layout);

    cairo_restore(c);
    g_object_unref (layout);
}
#else

void
OutputStringWithContextReal(cairo_t * c, const char *str, int x, int y)
{
    if (!str || str[0] == 0)
        return;
    if (!utf8_check_string(str))
        return;
    cairo_save(c);
    int             height = FontHeightWithContextReal(c);
    cairo_move_to(c, x, y + height);
    cairo_show_text(c, str);
    cairo_restore(c);
}
#endif


#ifdef _ENABLE_PANGO
PangoFontDescription* GetPangoFontDescription(const char* font, int size)
{
    PangoFontDescription* desc;
    desc = pango_font_description_new ();
    pango_font_description_set_absolute_size(desc, size * PANGO_SCALE);
    pango_font_description_set_family(desc, font);
    return desc;
}
#endif