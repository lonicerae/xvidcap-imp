/**
 * \file gnome_frame.c
 *
 * This file contains functions needed for creation and maintenance of the
 * frame enclosing the area to capture.
 */

/* 
 * Copyright (C) 2003-07 Karl H. Beckers, Frankfurt
 * EMail: khb@jarre-de-the.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * This file contains routines for setting up and handling the selection
 * rectangle. Both Xt and GTK2 versions are in here now.
 *
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define DEBUGFILE "gnome_frame.c"
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <X11/Intrinsic.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glade/glade.h>

#include "app_data.h"
#include "job.h"
#include "frame.h"
#include "gnome_frame.h"

/* 
 * file globals (static)
 *
 */
/** 
 * \brief remember the Display if already retrieved
 *
 * This assumes that the Display to capture from cannot change once xvidcap
 * has been started. We reuse any Display we once retrieved to avoid 
 * potential X server roundtrip
 */
static Display *xvc_dpy = NULL;

/** \brief make the frame parts available everywhere in this file */
static GtkWidget *gtk_frame_top,
    *gtk_frame_left, *gtk_frame_right, *gtk_frame_bottom, *gtk_frame_center;

/**
 * \brief gets the Display to capture from
 *
 * If the frame has been created we retrieve the Display from GDK. Otherwise,
 * (either we're running without GUI or we're doing this before the frame
 * has been created, which is the case if we pass --window) we use 
 * XOpenDisplay if xvc_dpy is not already set.
 *
 * @return a pointer to the Display to capture from. This Display can be
 *      expected to be always open and only closed on program exit.
 * @see xvc_frame_drop_capture_display
 */
Display *
xvc_frame_get_capture_display ()
{
#define DEBUGFUNCTION "xvc_frame_get_capure_display()"
    if (!(app->flags & FLG_NOGUI) && gtk_frame_top) {
        xvc_dpy = GDK_DRAWABLE_XDISPLAY (GTK_WIDGET (gtk_frame_top)->window);
    } else {
        if (!xvc_dpy)
            xvc_dpy = XOpenDisplay (NULL);
    }
    g_assert (xvc_dpy);
    return xvc_dpy;
#undef DEBUGFUNCTION
}

/**
 * \brief cleans up the Display to capture from, i. e. close and set to NULL
 */
void
xvc_frame_drop_capture_display ()
{
#define DEBUGFUNCTION "xvc_frame_drop_capture_display()"
    if (app->flags & FLG_NOGUI && xvc_dpy) {
        XCloseDisplay (xvc_dpy);
        xvc_dpy = NULL;
    }
#undef DEBUGFUNCTION
}

/**
 * \brief repositions the main control according to the frame's current 
 *      position
 *
 * @param toplevel a pointer to the main control
 */
void
do_reposition_control (GtkWidget * toplevel)
{
#define DEBUGFUNCTION "do_reposition_control()"
    int max_width = 0, max_height = 0;
    int pwidth = 0, pheight = 0;
    XRectangle *x_rect = xvc_get_capture_area ();
    int x = x_rect->x, y = x_rect->y;
    int width = x_rect->width, height = x_rect->height;
    GdkScreen *myscreen;
    GdkRectangle rect;

    myscreen = GTK_WINDOW (toplevel)->screen;
    g_assert (myscreen);

    max_width = gdk_screen_get_width (GDK_SCREEN (myscreen));
    max_height = gdk_screen_get_height (GDK_SCREEN (myscreen));

    gdk_window_get_frame_extents (GDK_WINDOW (toplevel->window), &rect);
    pwidth = rect.width + FRAME_OFFSET;
    pheight = rect.height + FRAME_OFFSET;

#ifdef DEBUG
    printf ("%s %s: x %i y %i pwidth %i pheight %i width %i height %i\n",
            DEBUGFILE, DEBUGFUNCTION, x, y, pwidth, pheight, width, height);
#endif     // DEBUG
    if ((y - pheight - FRAME_WIDTH) >= 0) {
        gtk_window_move (GTK_WINDOW (toplevel), x, (y - pheight - FRAME_WIDTH));
    } else {
        GladeXML *xml = NULL;
        GtkWidget *w = NULL;

        xml = glade_get_widget_tree (GTK_WIDGET (toplevel));
        g_assert (xml);

        w = glade_xml_get_widget (xml, "xvc_ctrl_lock_toggle");
        g_assert (w);

        gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), FALSE);

        if ((y + pheight + height + FRAME_WIDTH + FRAME_OFFSET) < max_height) {
            gtk_window_move (GTK_WINDOW (toplevel), x,
                             (y + height + FRAME_OFFSET + FRAME_WIDTH));
        } else {
            if (x > pwidth + FRAME_WIDTH + FRAME_OFFSET) {
                gtk_window_move (GTK_WINDOW (toplevel),
                                 (x - pwidth - FRAME_OFFSET - FRAME_WIDTH), y);
            } else {
                if ((x + width + pwidth + FRAME_OFFSET + FRAME_WIDTH) <
                    max_width) {
                    gtk_window_move (GTK_WINDOW (toplevel),
                                     (x + width + FRAME_OFFSET +
                                      FRAME_WIDTH), y);
                }
                // otherwise leave the UI where it is ...
            }
        }
    }
#undef DEBUGFUNCTION
}

/** 
 * \brief changes frame due to user input
 *
 * @param x x-position to change to
 * @param y y-position to change to
 * @param width new frame width
 * @param height new frame height
 * @param reposition_control TRUE for main control should be repositioned
 *      if frame moves, or FALSE if not
 */
void
xvc_change_gtk_frame (int x, int y, int width, int height,
                      Boolean reposition_control)
{
#define DEBUGFUNCTION "xvc_change_gtk_frame()"
    int max_width, max_height;
    extern GtkWidget *xvc_ctrl_main_window;
    Display *dpy;
    XRectangle *x_rect = xvc_get_capture_area ();

    // we have to adjust it to viewable areas
    dpy = xvc_frame_get_capture_display ();
    max_width = WidthOfScreen (DefaultScreenOfDisplay (dpy));
    max_height = HeightOfScreen (DefaultScreenOfDisplay (dpy));

#ifdef DEBUG
    printf ("%s %s: screen = %dx%d selection=%dx%d\n", DEBUGFILE,
            DEBUGFUNCTION, max_width, max_height, width, height);
#endif

    if (x < 0)
        x = 0;
    if (width > max_width)
        width = max_width;
    if (x + width > max_width)
        x = max_width - width;

    if (height > max_height)
        height = max_height;
    if (y + height > max_height)
        y = max_height - height;
    if (y < 0)
        y = 0;

    if ((app->flags & FLG_NOGUI) == 0) {
        // move the frame if not running without GUI
        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_top),
                                     (width + 2 * FRAME_WIDTH), FRAME_WIDTH);
        gtk_window_move (GTK_WINDOW (gtk_frame_top), (x - FRAME_WIDTH),
                         (y - FRAME_WIDTH));
        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_left),
                                     FRAME_WIDTH, height);
        gtk_window_move (GTK_WINDOW (gtk_frame_left), (x - FRAME_WIDTH), y);
        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_bottom),
                                     (width + 2 * FRAME_WIDTH), FRAME_WIDTH);
        gtk_window_move (GTK_WINDOW (gtk_frame_bottom), (x - FRAME_WIDTH),
                         (y + height));
        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_right),
                                     FRAME_WIDTH, height);
        gtk_window_move (GTK_WINDOW (gtk_frame_right), (x + width), y);

#ifdef HasVideo4Linux
        // if we have a v4l blind, move it, too
        if (gtk_frame_center != NULL)
            gtk_window_move (GTK_WINDOW (gtk_frame_center), x, y);
#endif     // HasVideo4Linux
    }
    // store coordinates in rectangle for further reference
    x_rect->x = x;
    x_rect->y = y;
    x_rect->width = width;
    x_rect->height = height;

    // if the frame is locked, we have a GUI, and we also want to
    // reposition the GUI (we don't want to except when repositioning the 
    // frame due too the cap_geometry cli parameter because the move of 
    // the frame would because triggered through a move of the control ... 
    // thus causing an infinite loop), move the control window, too
    if (((app->flags & FLG_NOGUI) == 0) && reposition_control)
        do_reposition_control (xvc_ctrl_main_window);

#undef DEBUGFUNCTION
}

/**
 * \brief callback for the configure event on the main control to move
 *      the frame if the main control moves
 */
static gint
on_gtk_frame_configure_event (GtkWidget * w, GdkEventConfigure * e)
{
#define DEBUGFUNCTION "on_gtk_frame_configure_event()"
    gint x, y, pwidth, pheight;
    XRectangle *x_rect = xvc_get_capture_area ();

    if (xvc_is_frame_locked ()) {
        x = ((GdkEventConfigure *) e)->x;
        y = ((GdkEventConfigure *) e)->y;
        pwidth = ((GdkEventConfigure *) e)->width;
        pheight = ((GdkEventConfigure *) e)->height;
        y += pheight + FRAME_OFFSET;
        xvc_change_gtk_frame (x, y, x_rect->width, x_rect->height, FALSE);
    }

    return FALSE;
#undef DEBUGFUNCTION
}

/**
 * \brief creates a frame around the area to capture
 *
 * @param toplevel a pointer to the main control
 * @param pwith width for the frame
 * @param pheight height for the frame
 * @param px x-position for the frame
 * @param py y-position for the frame
 */
void
xvc_create_gtk_frame (GtkWidget * toplevel, int pwidth, int pheight,
                      int px, int py)
{
#define DEBUGFUNCTION "xvc_create_gtk_frame()"
    gint x = 0, y = 0;
    GdkColor g_col;
    GdkColormap *colormap;
    GdkRectangle rect;
    int flags = app->flags;
    XRectangle *x_rect = xvc_get_capture_area ();

#ifdef DEBUG
    printf ("%s %s: x %d y %d width %d height %d\n", DEBUGFILE,
            DEBUGFUNCTION, px, py, pwidth, pheight);
#endif

    if (!(flags & FLG_NOGUI)) {
        g_assert (toplevel);

        if (px < 0 && py < 0) {
            // compute position for frame
            gdk_window_get_origin (GDK_WINDOW (toplevel->window), &x, &y);
            gdk_window_get_frame_extents (GDK_WINDOW (toplevel->window), &rect);

            if (x < 0)
                x = 0;
            y += rect.height + FRAME_OFFSET;
            if (y < 0)
                y = 0;
        } else {
            x = (px < 0 ? 0 : px);
            y = (py < 0 ? 0 : py);
        }
    } else {
        x = (px < 0 ? 0 : px);
        y = (py < 0 ? 0 : py);
    }

    // store rectangle properties for further reference
    g_assert (x_rect);

    x_rect->width = pwidth;
    x_rect->height = pheight;
    x_rect->x = x;
    x_rect->y = y;

    // create frame
    if (!(flags & FLG_NOGUI)) {

        // top of frame
        gtk_frame_top = gtk_dialog_new ();

        g_assert (gtk_frame_top);

        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_top),
                                     (pwidth + 2 * FRAME_WIDTH), FRAME_WIDTH);
        gtk_widget_set_sensitive (GTK_WIDGET (gtk_frame_top), FALSE);
        gtk_window_set_title (GTK_WINDOW (gtk_frame_top), "gtk_frame_top");
        GTK_WINDOW (gtk_frame_top)->type = GTK_WINDOW_POPUP;
        gtk_window_set_resizable (GTK_WINDOW (gtk_frame_top), FALSE);
        gtk_dialog_set_has_separator (GTK_DIALOG (gtk_frame_top), FALSE);
        colormap = gtk_widget_get_colormap (GTK_WIDGET (gtk_frame_top));

        g_assert (colormap);

        if (gdk_color_parse ("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame
            // color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP (colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show (gtk_frame_top);
        gtk_window_move (GTK_WINDOW (gtk_frame_top), (x - FRAME_WIDTH),
                         (y - FRAME_WIDTH));

        // left side of frame
        gtk_frame_left = gtk_dialog_new ();

        g_assert (gtk_frame_left);

        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_left),
                                     FRAME_WIDTH, pheight);
        gtk_widget_set_sensitive (GTK_WIDGET (gtk_frame_left), FALSE);
        gtk_window_set_title (GTK_WINDOW (gtk_frame_left), "gtk_frame_left");
        GTK_WINDOW (gtk_frame_left)->type = GTK_WINDOW_POPUP;
        gtk_window_set_resizable (GTK_WINDOW (gtk_frame_left), FALSE);
        gtk_dialog_set_has_separator (GTK_DIALOG (gtk_frame_left), FALSE);
        colormap = gtk_widget_get_colormap (GTK_WIDGET (gtk_frame_left));

        g_assert (colormap);

        if (gdk_color_parse ("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP (colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show (GTK_WIDGET (gtk_frame_left));
        gtk_window_move (GTK_WINDOW (gtk_frame_left), (x - FRAME_WIDTH), y);

        // bottom of frame
        gtk_frame_bottom = gtk_dialog_new ();

        g_assert (gtk_frame_bottom);

        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_bottom),
                                     (pwidth + 2 * FRAME_WIDTH), FRAME_WIDTH);
        gtk_widget_set_sensitive (GTK_WIDGET (gtk_frame_bottom), FALSE);
        gtk_window_set_title (GTK_WINDOW (gtk_frame_bottom),
                              "gtk_frame_bottom");
        GTK_WINDOW (gtk_frame_bottom)->type = GTK_WINDOW_POPUP;
        gtk_window_set_resizable (GTK_WINDOW (gtk_frame_bottom), FALSE);
        gtk_dialog_set_has_separator (GTK_DIALOG (gtk_frame_bottom), FALSE);
        colormap = gtk_widget_get_colormap (gtk_frame_bottom);

        g_assert (colormap);

        if (gdk_color_parse ("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP (colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show (GTK_WIDGET (gtk_frame_bottom));
        gtk_window_move (GTK_WINDOW (gtk_frame_bottom), (x - FRAME_WIDTH),
                         (y + pheight));

        // right side of frame
        gtk_frame_right = gtk_dialog_new ();

        g_assert (gtk_frame_right);

        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_right),
                                     FRAME_WIDTH, pheight);
        gtk_widget_set_sensitive (GTK_WIDGET (gtk_frame_right), FALSE);
        gtk_window_set_title (GTK_WINDOW (gtk_frame_right), "gtk_frame_right");
        GTK_WINDOW (gtk_frame_right)->type = GTK_WINDOW_POPUP;
        gtk_window_set_resizable (GTK_WINDOW (gtk_frame_right), FALSE);
        gtk_dialog_set_has_separator (GTK_DIALOG (gtk_frame_right), FALSE);
        colormap = gtk_widget_get_colormap (gtk_frame_right);

        g_assert (colormap);

        if (gdk_color_parse ("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP (colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show (GTK_WIDGET (gtk_frame_right));
        gtk_window_move (GTK_WINDOW (gtk_frame_right), (x + pwidth), y);

#ifdef HasVideo4Linux
        if (flags & FLG_USE_V4L) {
            gtk_frame_center = gtk_dialog_new ();

            g_assert (gtk_frame_center);

            gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_center),
                                         pwidth, pheight);
            gtk_widget_set_sensitive (GTK_WIDGET (gtk_frame_center), FALSE);
            gtk_window_set_title (GTK_WINDOW (gtk_frame_right),
                                  "gtk_frame_center");
            GTK_WINDOW (gtk_frame_center)->type = GTK_WINDOW_POPUP;
            gtk_window_set_resizable (GTK_WINDOW (gtk_frame_center), FALSE);
            gtk_dialog_set_has_separator (GTK_DIALOG (gtk_frame_center), FALSE);
            gtk_widget_show (GTK_WIDGET (gtk_frame_center));
            gtk_window_move (GTK_WINDOW (gtk_frame_center), x, y);

            // FIXME: right now the label for the Video Source is missing
            // gtk_frame_center = XtVaCreatePopupShell ("blind",
            // overrideShellWidgetClass, parent,
            // XtNx, x+FRAME_WIDTH,
            // XtNy, y+FRAME_WIDTH,
            // XtNwidth, width,
            // XtNheight, height,
            // NULL);
            // XtVaCreateManagedWidget ("text", xwLabelWidgetClass, blind,
            // XtNlabel, "Source: Video4Linux", NULL);
            // XtPopup(blind, XtGrabNone); 
        }
#endif     // HasVideo4Linux
        // connect event-handler to configure event of gtk control window
        // to redraw the selection frame if the control is moved and the 
        // frame is locked
        g_signal_connect ((gpointer) toplevel, "configure-event",
                          G_CALLBACK (on_gtk_frame_configure_event), NULL);

    }
    xvc_set_frame_locked (1);

    if (!(flags & FLG_NOGUI) && (px >= 0 || py >= 0))
        do_reposition_control (toplevel);

#undef DEBUGFUNCTION
}

/**
 * \brief destroys the frame around the capture area
 */
void
xvc_destroy_gtk_frame ()
{
#define DEBUGFUNCTION "xvc_destroy_gtk_frame()"

    gtk_widget_destroy (gtk_frame_bottom);
    gtk_widget_destroy (gtk_frame_right);
    gtk_widget_destroy (gtk_frame_left);
    gtk_widget_destroy (gtk_frame_top);
    if (gtk_frame_center) {
        gtk_widget_destroy (gtk_frame_center);
    }
#undef DEBUGFUNCTION
}
