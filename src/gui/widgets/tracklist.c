/*
 * Copyright (C) 2018-2019 Alexandros Theodotou <alex at zrythm dot org>
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

/** \file
 */

#include "actions/copy_tracks_action.h"
#include "actions/move_tracks_action.h"
#include "audio/audio_bus_track.h"
#include "audio/channel.h"
#include "audio/chord_track.h"
#include "audio/instrument_track.h"
#include "audio/mixer.h"
#include "audio/scale.h"
#include "audio/track.h"
#include "audio/tracklist.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/channel.h"
#include "gui/widgets/chord_track.h"
#include "gui/widgets/drag_dest_box.h"
#include "gui/widgets/inspector.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/mixer.h"
#include "gui/widgets/tracklist.h"
#include "gui/widgets/track.h"
#include "project.h"
#include "utils/arrays.h"
#include "utils/flags.h"
#include "utils/gtk.h"
#include "utils/ui.h"

G_DEFINE_TYPE (TracklistWidget,
               tracklist_widget,
               DZL_TYPE_MULTI_PANED)

static void
on_resize_end (TracklistWidget * self,
               GtkWidget *       child)
{
  if (!Z_IS_TRACK_WIDGET (child))
    return;

  TrackWidget * tw = Z_TRACK_WIDGET (child);
  TRACK_WIDGET_GET_PRIVATE (tw);
  Track * track = tw_prv->track;
  GValue a = G_VALUE_INIT;
  g_value_init (&a, G_TYPE_INT);
  gtk_container_child_get_property (
    GTK_CONTAINER (self),
    GTK_WIDGET (tw),
    "position",
    &a);
  track->handle_pos = g_value_get_int (&a);
}

static void
on_drag_leave (
  GtkWidget      *widget,
  GdkDragContext *context,
  guint           time,
  TracklistWidget * self)
{
  Track * track;
  for (int i = 0; i < TRACKLIST->num_tracks; i++)
    {
      track = TRACKLIST->tracks[i];
      if (track->visible &&
          track->widget)
        {
          track_widget_do_highlight (
            track->widget, 0, 0, 0);
        }

    }
}

static void
on_drag_motion (
  GtkWidget *widget,
  GdkDragContext *context,
  gint x,
  gint y,
  guint time,
  TracklistWidget * self)
{
  GdkModifierType mask;
  z_gtk_widget_get_mask (
    widget, &mask);
  if (mask & GDK_CONTROL_MASK)
    gdk_drag_status (context, GDK_ACTION_COPY, time);
  else
    gdk_drag_status (context, GDK_ACTION_MOVE, time);

  /* get track widget at the x,y point */
  GList *children, *iter;
  children =
    gtk_container_get_children (
      GTK_CONTAINER (self));
  TrackWidget * hit_tw = NULL;
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      if (Z_IS_TRACK_WIDGET (iter->data))
        {
          TrackWidget * tw =
            Z_TRACK_WIDGET (iter->data);

          if (ui_is_child_hit (
                GTK_WIDGET (self),
                GTK_WIDGET (tw),
                1, 1, x, y, 0, 1))
            {
              hit_tw = tw;
            }
          else
            {
              track_widget_do_highlight (
                tw, x, y, 0);
            }
        }
    }
  g_list_free(children);

  if (hit_tw)
    {
      GtkAllocation allocation;
      gtk_widget_get_allocation (
        GTK_WIDGET (hit_tw),
        &allocation);

      gint wx, wy;
      gtk_widget_translate_coordinates (
        GTK_WIDGET (self),
        GTK_WIDGET (hit_tw),
        (int) x, (int) y, &wx, &wy);
      track_widget_do_highlight (
        hit_tw, wx, wy, 1);
    }
}

static void
on_drag_data_received (
  GtkWidget        *widget,
  GdkDragContext   *context,
  gint              x,
  gint              y,
  GtkSelectionData *data,
  guint             info,
  guint             time,
  TracklistWidget * self)
{
  g_message ("drag data received on tracklist");

  /* get track widget at the x,y point */
  GList *children, *iter;
  children =
    gtk_container_get_children (
      GTK_CONTAINER (self));
  TrackWidget * hit_tw = NULL;
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      if (Z_IS_TRACK_WIDGET (iter->data))
        {
          TrackWidget * tw =
            Z_TRACK_WIDGET (iter->data);

          if (ui_is_child_hit (
                GTK_WIDGET (self),
                GTK_WIDGET (tw),
                1, 1, x, y, 0, 1))
            {
              hit_tw = tw;
            }
          else
            {
              track_widget_do_highlight (
                tw, x, y, 0);
            }
        }
    }
  g_list_free(children);

  g_return_if_fail (hit_tw);

  TRACK_WIDGET_GET_PRIVATE (hit_tw);
  Track * this = tw_prv->track;

  /* determine if moving or copying */
  GdkDragAction action =
    gdk_drag_context_get_selected_action (
      context);

  GtkAllocation allocation;
  gtk_widget_get_allocation (
    GTK_WIDGET (hit_tw),
    &allocation);

  gint wx, wy;
  gtk_widget_translate_coordinates (
    GTK_WIDGET (self),
    GTK_WIDGET (hit_tw),
    (int) x, (int) y, &wx, &wy);

  int h =
    gtk_widget_get_allocated_height (
      GTK_WIDGET (hit_tw));

  /* determine position to move to */
  int pos;
  if (wy < h / 2)
    {
      if (this->pos <=
          MW_MIXER->start_drag_track->pos)
        pos = this->pos;
      else
        {
          Track * prev =
            tracklist_get_prev_visible_track (
              TRACKLIST, this);
          pos =
            prev ? prev->pos : this->pos;
        }
    }
  else
    {
      if (this->pos >=
          MW_MIXER->start_drag_track->pos)
        pos = this->pos;
      else
        {
          Track * next =
            tracklist_get_next_visible_track (
              TRACKLIST, this);
          pos =
            next ? next->pos : this->pos;
        }
    }

  UndoableAction * ua = NULL;
  if (action == GDK_ACTION_COPY)
    {
      ua =
        copy_tracks_action_new (
          TRACKLIST_SELECTIONS, pos);
    }
  else if (action == GDK_ACTION_MOVE)
    {
      ua =
        move_tracks_action_new (
          TRACKLIST_SELECTIONS, pos);
    }

  g_warn_if_fail (ua);

  undo_manager_perform (
    UNDO_MANAGER, ua);
}

TrackWidget *
tracklist_widget_get_hit_track (
  TracklistWidget *  self,
  double            x,
  double            y)
{
  /* go through each child */
  for(int i = 0;
      i < self->tracklist->num_tracks; i++)
    {
      Track * track = self->tracklist->tracks[i];
      if (!track->visible || track->pinned)
        continue;

      TrackWidget * tw = track->widget;

      /* return it if hit */
      if (ui_is_child_hit (
            GTK_WIDGET (self),
            GTK_WIDGET (tw),
            0, 1, x, y, 0, 0))
        return tw;
    }
  return NULL;
}

static gboolean
on_key_action (
  GtkWidget *       widget,
  GdkEventKey  *    event,
  TracklistWidget * self)
{
  if (event->state & GDK_CONTROL_MASK &&
      event->type == GDK_KEY_PRESS &&
      event->keyval == GDK_KEY_a)
    {
      tracklist_selections_select_all (
        TRACKLIST_SELECTIONS, 1);
    }

  return FALSE;
}

static void
on_size_allocate (
  GtkWidget *       widget,
  GdkRectangle *    allocation,
  TracklistWidget * self)
{
  if (!TRACKLIST)
    return;

  EVENTS_PUSH (
    ET_TRACKS_RESIZED, self);
  g_message ("pushed");
}

void
tracklist_widget_hard_refresh (
  TracklistWidget * self)
{
  /* remove ddbox */
  g_object_ref (self->ddbox);
  gtk_container_remove (
    GTK_CONTAINER (self),
    GTK_WIDGET (self->ddbox));

  /* remove all tracks */
  z_gtk_container_remove_all_children (
    GTK_CONTAINER (self));

  /* add tracks */
  for (int i = 0;
       i < self->tracklist->num_tracks; i++)
    {
      Track * track = self->tracklist->tracks[i];

      if (track->pinned ||
          !track->visible)
        continue;

      /* create widget */
      if (!GTK_IS_WIDGET (track->widget))
        track->widget = track_widget_new (track);

      track_widget_refresh (track->widget);

      /* add to tracklist widget */
      gtk_container_add (
        GTK_CONTAINER (self),
        GTK_WIDGET (track->widget));
    }

  /* re-add ddbox */
  gtk_container_add (GTK_CONTAINER (self),
                     GTK_WIDGET (self->ddbox));
  g_object_unref (self->ddbox);

  /* set handle position.
   * this is done because the position resets to -1
   * every time a child is added or deleted */
  GList *children, *iter;
  children =
    gtk_container_get_children (GTK_CONTAINER (self));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      if (Z_IS_TRACK_WIDGET (iter->data))
        {
          TrackWidget * tw = Z_TRACK_WIDGET (iter->data);
          TRACK_WIDGET_GET_PRIVATE (tw);
          Track * track = tw_prv->track;
          GValue a = G_VALUE_INIT;
          g_value_init (&a, G_TYPE_INT);
          g_value_set_int (&a, track->handle_pos);
          gtk_container_child_set_property (
            GTK_CONTAINER (self),
            GTK_WIDGET (tw),
            "position",
            &a);
        }
    }
  g_list_free(children);
}

void
tracklist_widget_setup (TracklistWidget * self,
                        Tracklist * tracklist)
{
  g_warn_if_fail (tracklist);
  self->tracklist = tracklist;
  tracklist->widget = self;

  tracklist_widget_hard_refresh (self);
}

/**
 * Makes sure all the tracks for channels marked as
 * visible are visible.
 */
void
tracklist_widget_soft_refresh (TracklistWidget *self)
{
  gtk_widget_show (GTK_WIDGET (self));
  Track * track;
  for (int i = 0; i < TRACKLIST->num_tracks; i++)
    {
      track = TRACKLIST->tracks[i];

      if (!GTK_IS_WIDGET (track->widget))
        track->widget = track_widget_new (track);

      gtk_widget_set_visible (
        GTK_WIDGET (track->widget), track->visible);

      if (track->visible)
        track_widget_refresh (
          track->widget);
    }
  if (self->ddbox)
    gtk_widget_show (GTK_WIDGET (self->ddbox));
}

static void
tracklist_widget_init (TracklistWidget * self)
{
  self->ddbox =
    drag_dest_box_widget_new (
      GTK_ORIENTATION_VERTICAL,
      0,
      DRAG_DEST_BOX_TYPE_TRACKLIST);
  gtk_container_add (GTK_CONTAINER (self),
                   GTK_WIDGET (self->ddbox));

  /* make widget able to notify */
  gtk_widget_add_events (
    GTK_WIDGET (self),
    GDK_ALL_EVENTS_MASK);

  char * entry_track = g_strdup (TARGET_ENTRY_TRACK);
  GtkTargetEntry entries[] = {
    {
      entry_track, GTK_TARGET_SAME_APP,
      symap_map (ZSYMAP, TARGET_ENTRY_TRACK),
    },
  };

  /* set as drag dest for track (the track will
   * be moved based on which half it was dropped in,
   * top or bot) */
  gtk_drag_dest_set (
    GTK_WIDGET (self),
    GTK_DEST_DEFAULT_MOTION |
      GTK_DEST_DEFAULT_DROP,
    entries, G_N_ELEMENTS (entries),
    GDK_ACTION_MOVE | GDK_ACTION_COPY);
  g_free (entry_track);

  g_signal_connect (
    GTK_WIDGET (self), "drag-data-received",
    G_CALLBACK(on_drag_data_received), self);
  g_signal_connect (
    GTK_WIDGET (self), "drag-motion",
    G_CALLBACK (on_drag_motion), self);
  g_signal_connect (
    GTK_WIDGET (self), "drag-leave",
    G_CALLBACK (on_drag_leave), self);
  g_signal_connect (
    G_OBJECT (self), "key-press-event",
    G_CALLBACK (on_key_action), self);
  g_signal_connect (
    G_OBJECT (self), "key-release-event",
    G_CALLBACK (on_key_action), self);
  g_signal_connect (
    G_OBJECT (self), "resize-drag-end",
    G_CALLBACK (on_resize_end), self);
  g_signal_connect (
    G_OBJECT (self), "size-allocate",
    G_CALLBACK (on_size_allocate), self);
}

static void
tracklist_widget_class_init (TracklistWidgetClass * _klass)
{
  GtkWidgetClass * klass = GTK_WIDGET_CLASS (_klass);
  gtk_widget_class_set_css_name (klass,
                                 "tracklist");
}
