/***************************************************************************
 *   Copyright (C) 2002~2005 by Yuking                                     *
 *   yuking_net@sohu.com                                                   *
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
/**
 * @file   MainWindow.h
 * @author Yuking yuking_net@sohu.com
 * @date   2008-1-16
 * 
 * @brief  主窗口
 * 
 * 
 */

#ifndef _MAIN_WINDOW_H
#define _MAIN_WINDOW_H
#include <X11/Xlib.h>
#include <cairo.h>
#include <fcitx-config/fcitx-config.h>

struct FcitxSkin;
struct FcitxClassicUI;

typedef enum _HIDE_MAINWINDOW {
    HM_SHOW = 0,
    HM_AUTO = 1,
    HM_HIDE = 2
} HIDE_MAINWINDOW;

typedef struct MainWindow
{
    Display* dpy;
    Window window;
    Pixmap pm_main_bar;
    cairo_surface_t* cs_main_bar;
    GC main_win_gc;
    boolean bMainWindowHidden;
    struct FcitxSkin* skin;
    
    cairo_surface_t* bar;
    cairo_surface_t* logo;
    cairo_surface_t* english;
    cairo_surface_t* otherim;
    struct FcitxClassicUI* owner;
} MainWindow;

MainWindow* CreateMainWindow (struct FcitxClassicUI* classicui);
void CloseMainWindow(MainWindow *mainWindow);
void DestroyMainWindow(MainWindow* mainWindow);
void DrawMainWindow (MainWindow* mainWindow);

#endif