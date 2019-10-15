/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * Marker widget.
 */

#ifndef __GUI_WIDGETS_MARKER_H__
#define __GUI_WIDGETS_MARKER_H__

#include <gtk/gtk.h>

#define MARKER_WIDGET_TYPE \
  (marker_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  MarkerWidget,
  marker_widget,
  Z, MARKER_WIDGET,
  GtkBox);

typedef struct Marker Marker;

/**
 * @addtogroup widgets
 *
 * @{
 */

#define MARKER_WIDGET_TRIANGLE_W 10

/**
 * Widget for markers inside the MarkerTrack.
 */
typedef struct _MarkerWidget
{
  GtkBox           parent_instance;
  GtkDrawingArea * drawing_area;
  Marker *         marker;

  /** For double click. */
  GtkGestureMultiPress * mp;

  PangoLayout *    layout;

  /** Cache text H extents and W extents for
   * the widget. */
  int              textw;
  int              texth;

  /** Set to 1 to redraw. */
  int                redraw;

  cairo_t *          cached_cr;

  cairo_surface_t *  cached_surface;
} MarkerWidget;

/**
 * Creates a marker widget.
 */
MarkerWidget *
marker_widget_new (
  Marker * marker);

void
marker_widget_recreate_pango_layouts (
  MarkerWidget * self);

void
marker_widget_force_redraw (
  MarkerWidget * self);

/**
 * @}
 */

#endif
