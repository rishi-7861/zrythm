/*
 * Copyright (C) 2018-2019 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * The timeline containing regions and other
 * objects.
 */

#include "actions/undoable_action.h"
#include "actions/create_timeline_selections_action.h"
#include "actions/edit_timeline_selections_action.h"
#include "actions/undo_manager.h"
#include "actions/duplicate_timeline_selections_action.h"
#include "actions/move_timeline_selections_action.h"
#include "audio/automation_lane.h"
#include "audio/automation_track.h"
#include "audio/automation_tracklist.h"
#include "audio/audio_track.h"
#include "audio/bus_track.h"
#include "audio/channel.h"
#include "audio/chord_object.h"
#include "audio/chord_track.h"
#include "audio/instrument_track.h"
#include "audio/marker_track.h"
#include "audio/master_track.h"
#include "audio/midi_region.h"
#include "audio/mixer.h"
#include "audio/scale_object.h"
#include "audio/track.h"
#include "audio/tracklist.h"
#include "audio/transport.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/automation_curve.h"
#include "gui/widgets/automation_lane.h"
#include "gui/widgets/automation_point.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/chord_object.h"
#include "gui/widgets/color_area.h"
#include "gui/widgets/inspector.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/marker.h"
#include "gui/widgets/midi_arranger.h"
#include "gui/widgets/midi_arranger_bg.h"
#include "gui/widgets/midi_region.h"
#include "gui/widgets/piano_roll.h"
#include "gui/widgets/pinned_tracklist.h"
#include "gui/widgets/midi_note.h"
#include "gui/widgets/region.h"
#include "gui/widgets/scale_object.h"
#include "gui/widgets/ruler.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/timeline_bg.h"
#include "gui/widgets/timeline_ruler.h"
#include "gui/widgets/track.h"
#include "gui/widgets/tracklist.h"
#include "project.h"
#include "settings/settings.h"
#include "utils/arrays.h"
#include "utils/cairo.h"
#include "utils/flags.h"
#include "utils/objects.h"
#include "utils/ui.h"
#include "zrythm.h"

#include <gtk/gtk.h>

G_DEFINE_TYPE (TimelineArrangerWidget,
               timeline_arranger_widget,
               ARRANGER_WIDGET_TYPE)

/**
 * To be called from get_child_position in parent widget.
 *
 * Used to allocate the overlay children.
 */
void
timeline_arranger_widget_set_allocation (
  TimelineArrangerWidget * self,
  GtkWidget *          widget,
  GdkRectangle *       allocation)
{
  if (Z_IS_REGION_WIDGET (widget))
    {
      RegionWidget * rw = Z_REGION_WIDGET (widget);
      REGION_WIDGET_GET_PRIVATE (rw);
      TrackLane * lane = rw_prv->region->lane;
      Track * track =
        TRACKLIST->tracks[
          rw_prv->region->track_pos];

      if (!track->widget)
        track->widget = track_widget_new (track);

      allocation->x =
        ui_pos_to_px_timeline (
          &rw_prv->region->start_pos,
          1);
      allocation->width =
        (ui_pos_to_px_timeline (
          &rw_prv->region->end_pos,
          1) - allocation->x) - 1;

      TRACK_WIDGET_GET_PRIVATE (track->widget);

      gint wx, wy;
      ArrangerObjectInfo * nfo =
        &rw_prv->region->obj_info;
      if (arranger_object_info_is_lane (nfo))
        {
          if (!track->lanes_visible)
            return;

          gtk_widget_translate_coordinates(
            GTK_WIDGET (lane->widget),
            GTK_WIDGET (self),
            0, 0,
            &wx, &wy);

          allocation->y = wy;

          allocation->height =
            gtk_widget_get_allocated_height (
              GTK_WIDGET (lane->widget));
        }
      else
        {
          gtk_widget_translate_coordinates(
            GTK_WIDGET (track->widget),
            GTK_WIDGET (self),
            0, 0,
            &wx, &wy);

          allocation->y = wy;

          allocation->height =
            gtk_widget_get_allocated_height (
              GTK_WIDGET (tw_prv->top_grid));
        }
    }
  else if (Z_IS_AUTOMATION_POINT_WIDGET (widget))
    {
      AutomationPointWidget * ap_widget =
        Z_AUTOMATION_POINT_WIDGET (widget);
      AutomationPoint * ap =
        ap_widget->automation_point;
      /*Automatable * a = ap->at->automatable;*/

      gint wx, wy;
      if (!ap->at->al ||
          !ap->at->track->bot_paned_visible)
        return;
      gtk_widget_translate_coordinates (
        GTK_WIDGET (ap->at->al->widget),
        GTK_WIDGET (self),
        0, 0, &wx, &wy);

      allocation->x =
        ui_pos_to_px_timeline (&ap->pos, 1) -
        AP_WIDGET_POINT_SIZE / 2;
      allocation->y =
        (wy + automation_point_get_y_in_px (ap)) -
        AP_WIDGET_POINT_SIZE / 2;
      allocation->width = AP_WIDGET_POINT_SIZE;
      allocation->height = AP_WIDGET_POINT_SIZE;
    }
  else if (Z_IS_AUTOMATION_CURVE_WIDGET (widget))
    {
      AutomationCurveWidget * acw =
        Z_AUTOMATION_CURVE_WIDGET (widget);
      AutomationCurve * ac = acw->ac;
      /*Automatable * a = ap->at->automatable;*/

      gint wx, wy;
      if (!ac->at->al ||
          !ac->at->track->bot_paned_visible)
        return;
      gtk_widget_translate_coordinates (
        GTK_WIDGET (ac->at->al->widget),
        GTK_WIDGET (self),
        0, 0, &wx, &wy);
      AutomationPoint * prev_ap =
        automation_track_get_ap_before_curve (
          ac->at, ac);
      AutomationPoint * next_ap =
        automation_track_get_ap_after_curve (
          ac->at, ac);
      if (!prev_ap || !next_ap)
        g_return_if_reached ();

      allocation->x =
        ui_pos_to_px_timeline (
          &prev_ap->pos,
          1) - AC_Y_HALF_PADDING;
      int prev_y =
        automation_point_get_y_in_px (prev_ap);
      int next_y =
        automation_point_get_y_in_px (next_ap);
      allocation->y =
        (wy + (prev_y > next_y ? next_y : prev_y) -
         AC_Y_HALF_PADDING);
      allocation->width =
        (ui_pos_to_px_timeline (
          &next_ap->pos,
          1) - allocation->x) + AC_Y_HALF_PADDING;
      allocation->height =
        (prev_y > next_y ?
         prev_y - next_y :
         next_y - prev_y) + AC_Y_PADDING;
    }
  else if (Z_IS_CHORD_OBJECT_WIDGET (widget))
    {
      ChordObjectWidget * cw =
        Z_CHORD_OBJECT_WIDGET (widget);
      ChordObject * co =
        cw->chord_object;
      Track * track = P_CHORD_TRACK;

      gint wx, wy;
      gtk_widget_translate_coordinates (
        GTK_WIDGET (track->widget),
        GTK_WIDGET (self),
        0, 0, &wx, &wy);

      allocation->x =
        ui_pos_to_px_timeline (
          &co->pos, 1);
      char * chord_str =
        chord_descriptor_to_string (co->descr);
      int textw, texth;
      z_cairo_get_text_extents_for_widget (
        widget, chord_str, &textw, &texth);
      g_free (chord_str);
      allocation->width =
        textw + CHORD_OBJECT_WIDGET_TRIANGLE_W +
        Z_CAIRO_TEXT_PADDING * 2;

      int track_height =
        gtk_widget_get_allocated_height (
          GTK_WIDGET (track->widget));
      int obj_height =
        texth + Z_CAIRO_TEXT_PADDING * 2;
      allocation->y =
        ((wy + track_height) - obj_height) -
        track_height / 2;
      allocation->height = obj_height;
    }
  else if (Z_IS_SCALE_OBJECT_WIDGET (widget))
    {
      ScaleObjectWidget * cw =
        Z_SCALE_OBJECT_WIDGET (widget);
      ScaleObject * so = cw->scale_object;
      Track * track = P_CHORD_TRACK;

      gint wx, wy;
      gtk_widget_translate_coordinates (
        GTK_WIDGET (track->widget),
        GTK_WIDGET (self),
        0, 0, &wx, &wy);

      allocation->x =
        ui_pos_to_px_timeline (
          &so->pos, 1);
      char * scale_str =
        musical_scale_to_string (so->scale);
      int textw, texth;
      z_cairo_get_text_extents_for_widget (
        widget, scale_str, &textw, &texth);
      g_free (scale_str);
      allocation->width =
        textw + SCALE_OBJECT_WIDGET_TRIANGLE_W +
        Z_CAIRO_TEXT_PADDING * 2;

      int track_height =
        gtk_widget_get_allocated_height (
          GTK_WIDGET (track->widget));
      int obj_height =
        texth + Z_CAIRO_TEXT_PADDING * 2;
      allocation->y =
        (wy + track_height) - obj_height;
      allocation->height = obj_height;
    }
  else if (Z_IS_MARKER_WIDGET (widget))
    {
      MarkerWidget * mw =
        Z_MARKER_WIDGET (widget);
      Track * track = P_MARKER_TRACK;

      gint wx, wy;
      gtk_widget_translate_coordinates (
        GTK_WIDGET (track->widget),
        GTK_WIDGET (self),
        0, 0, &wx, &wy);

      allocation->x =
        ui_pos_to_px_timeline (
          &mw->marker->pos, 1);
      int textw, texth;
      z_cairo_get_text_extents_for_widget (
        widget, mw->marker->name, &textw, &texth);
      allocation->width =
        textw + MARKER_WIDGET_TRIANGLE_W +
        Z_CAIRO_TEXT_PADDING * 2;

      int track_height =
        gtk_widget_get_allocated_height (
          GTK_WIDGET (track->widget));
      int obj_height =
        texth + Z_CAIRO_TEXT_PADDING * 2;
      allocation->y =
        (wy + track_height) - obj_height;
      allocation->height = obj_height;
    }
}

/**
 * Returns the appropriate cursor based on the
 * current hover_x and y.
 */
ArrangerCursor
timeline_arranger_widget_get_cursor (
  TimelineArrangerWidget * self,
  UiOverlayAction action,
  Tool            tool)
{
  ArrangerCursor ac = ARRANGER_CURSOR_SELECT;

  ARRANGER_WIDGET_GET_PRIVATE (self);

  switch (action)
    {
    case UI_OVERLAY_ACTION_NONE:
      if (P_TOOL == TOOL_SELECT_NORMAL)
        {
          RegionWidget * rw =
            timeline_arranger_widget_get_hit_region (
              self,
              ar_prv->hover_x,
              ar_prv->hover_y);
          ChordObjectWidget * cw =
            timeline_arranger_widget_get_hit_chord (
              self,
              ar_prv->hover_x,
              ar_prv->hover_y);
          ScaleObjectWidget * sw =
            timeline_arranger_widget_get_hit_scale (
              self,
              ar_prv->hover_x,
              ar_prv->hover_y);

          REGION_WIDGET_GET_PRIVATE (rw);
          /* TODO aps */

          int is_hit = rw || cw || sw;
          int is_resize_l =
            rw && rw_prv->resize_l;
          int is_resize_r =
            rw && rw_prv->resize_r;

          if (is_hit && is_resize_l)
            {
              return ARRANGER_CURSOR_RESIZING_L;
            }
          else if (is_hit && is_resize_r)
            {
              return ARRANGER_CURSOR_RESIZING_R;
            }
          else if (is_hit)
            {
              return ARRANGER_CURSOR_GRAB;
            }
          else
            {
              Track * track =
                timeline_arranger_widget_get_track_at_y (
                self, ar_prv->hover_y);

              if (track)
                {
                  if (track_widget_is_cursor_in_top_half (
                        track->widget,
                        ar_prv->hover_y))
                    {
                      /* set cursor to normal */
                      return ARRANGER_CURSOR_SELECT;
                    }
                  else
                    {
                      /* set cursor to range selection */
                      return ARRANGER_CURSOR_RANGE;
                    }
                }
              else
                {
                  /* set cursor to normal */
                  return ARRANGER_CURSOR_SELECT;
                }
            }
        }
      else if (P_TOOL == TOOL_SELECT_STRETCH)
        {
        }
      else if (P_TOOL == TOOL_EDIT)
        ac = ARRANGER_CURSOR_EDIT;
      else if (P_TOOL == TOOL_ERASER)
        ac = ARRANGER_CURSOR_ERASER;
      else if (P_TOOL == TOOL_RAMP)
        ac = ARRANGER_CURSOR_RAMP;
      else if (P_TOOL == TOOL_AUDITION)
        ac = ARRANGER_CURSOR_AUDITION;
      break;
    case UI_OVERLAY_ACTION_STARTING_DELETE_SELECTION:
    case UI_OVERLAY_ACTION_DELETE_SELECTING:
    case UI_OVERLAY_ACTION_ERASING:
      ac = ARRANGER_CURSOR_ERASER;
      break;
    case UI_OVERLAY_ACTION_STARTING_MOVING_COPY:
    case UI_OVERLAY_ACTION_MOVING_COPY:
      ac = ARRANGER_CURSOR_GRABBING_COPY;
      break;
    case UI_OVERLAY_ACTION_STARTING_MOVING:
    case UI_OVERLAY_ACTION_MOVING:
      ac = ARRANGER_CURSOR_GRABBING;
      break;
    case UI_OVERLAY_ACTION_STARTING_MOVING_LINK:
    case UI_OVERLAY_ACTION_MOVING_LINK:
      ac = ARRANGER_CURSOR_GRABBING_LINK;
      break;
    case UI_OVERLAY_ACTION_RESIZING_L:
      if (self->resizing_range)
        ac = ARRANGER_CURSOR_RANGE;
      else
        ac = ARRANGER_CURSOR_RESIZING_L;
      break;
    case UI_OVERLAY_ACTION_RESIZING_R:
      if (self->resizing_range)
        ac = ARRANGER_CURSOR_RANGE;
      else
        ac = ARRANGER_CURSOR_RESIZING_R;
      break;
    default:
      ac = ARRANGER_CURSOR_SELECT;
      break;
    }

  return ac;
}

TrackLane *
timeline_arranger_widget_get_track_lane_at_y (
  TimelineArrangerWidget * self,
  double y)
{
  int i, j;
  Track * track;
  TrackLane * lane;
  for (i = 0; i < TRACKLIST->num_tracks; i++)
    {
      track = TRACKLIST->tracks[i];

      for (j = 0; j < track->num_lanes; j++)
        {
          lane = track->lanes[j];

          if (!lane->widget ||
              !GTK_IS_WIDGET (lane->widget))
            continue;

          if (ui_is_child_hit (
                GTK_CONTAINER (self),
                GTK_WIDGET (lane->widget),
                0, 1, 0, y, 0, 0))
            return lane;
        }
    }

  return NULL;
}

Track *
timeline_arranger_widget_get_track_at_y (
  TimelineArrangerWidget * self,
  double y)
{
  Track * track;
  for (int i = 0; i < TRACKLIST->num_tracks; i++)
    {
      track = TRACKLIST->tracks[i];

      if (ui_is_child_hit (
            GTK_CONTAINER (self),
            GTK_WIDGET (track->widget),
            0, 1, 0, y, 0, 0))
        return track;
    }

  return NULL;
}

/**
 * Returns the hit AutomationTrack at y.
 */
AutomationTrack *
timeline_arranger_widget_get_automation_track_at_y (
  TimelineArrangerWidget * self,
  double                   y)
{
  int i, j;
  for (i = 0; i < TRACKLIST->num_tracks; i++)
    {
      Track * track = TRACKLIST->tracks[i];
      AutomationTracklist * atl =
        track_get_automation_tracklist (track);
      if (!atl ||
          !track->bot_paned_visible ||
          (track->pinned && (
             self == MW_TIMELINE)) ||
          (!track->pinned && (
             self == MW_PINNED_TIMELINE)))
        continue;

      AutomationLane * al;
      for (j = 0; j < atl->num_als; j++)
        {
          al = atl->als[j];

          /* TODO check the rest */
          if (al->visible && al->widget &&
              ui_is_child_hit (
                GTK_CONTAINER (MW_TIMELINE),
                GTK_WIDGET (al->widget),
                0, 1, 0, y, 0, 0))
            return al->at;
        }
    }

  return NULL;
}

#define GET_HIT_WIDGET(caps,cc,sc) \
cc##Widget * \
timeline_arranger_widget_get_hit_##sc ( \
  TimelineArrangerWidget *  self, \
  double                    x, \
  double                    y) \
{ \
  GtkWidget * widget = \
    ui_get_hit_child ( \
      GTK_CONTAINER (self), x, y, \
      caps##_WIDGET_TYPE); \
  if (widget) \
    { \
      return Z_##caps##_WIDGET (widget); \
    } \
  return NULL; \
}

GET_HIT_WIDGET (
  CHORD_OBJECT, ChordObject, chord);
GET_HIT_WIDGET (
  SCALE_OBJECT, ScaleObject, scale);
GET_HIT_WIDGET (
  MARKER, Marker, marker);
GET_HIT_WIDGET (
  REGION, Region, region);
GET_HIT_WIDGET (
  AUTOMATION_POINT, AutomationPoint, ap);
GET_HIT_WIDGET (
  AUTOMATION_CURVE, AutomationCurve, curve);

#undef GET_HIT_WIDGET

void
timeline_arranger_widget_select_all (
  TimelineArrangerWidget *  self,
  int                       select)
{
  timeline_selections_clear (TL_SELECTIONS);

  /* select everything else */
  Region * r;
  Track * track;
  AutomationPoint * ap;
  AutomationLane * al;
  for (int i = 0; i < TRACKLIST->num_tracks; i++)
    {
      track = TRACKLIST->tracks[i];

      if (!track->visible)
        continue;

      AutomationTracklist * atl =
        track_get_automation_tracklist (
          track);

      if (track->lanes_visible)
        {
          TrackLane * lane;
          for (int j = 0;
               j < track->num_lanes; j++)
            {
              lane = track->lanes[j];

              for (int k = 0;
                   k < lane->num_regions; k++)
                {
                  r = lane->regions[k];

                  region_widget_select (
                    r->widget, select);
                }
            }
        }

      if (!track->bot_paned_visible)
        continue;

      for (int j = 0;
           j < atl->num_als;
           j++)
        {
          al = atl->als[j];
          if (al->visible)
            {
              for (int k = 0;
                   k < al->at->num_aps;
                   k++)
                {
                  ap =
                    al->at->aps[k];

                  automation_point_widget_select (
                    ap->widget, select);
                }
            }
        }
    }

  /* select chords */
  Track * ct = P_CHORD_TRACK;
  for (int i = 0; i < ct->num_chords; i++)
    {
      ChordObject * chord = ct->chords[i];
      chord_object_widget_select (
        chord->widget, select);
    }

  /* select scales */
  for (int i = 0; i < ct->num_scales; i++)
    {
      ScaleObject * scale = ct->scales[i];
      scale_object_widget_select (
        scale->widget, select);
    }

  /**
   * Deselect range if deselecting all.
   */
  if (!select)
    {
      project_set_has_range (0);
    }
}

/**
 * Shows context menu.
 *
 * To be called from parent on right click.
 */
void
timeline_arranger_widget_show_context_menu (
  TimelineArrangerWidget * self,
  gdouble              x,
  gdouble              y)
{
  GtkWidget *menu, *menuitem;

  /*RegionWidget * clicked_region =*/
    /*timeline_arranger_widget_get_hit_region (*/
      /*self, x, y);*/
  /*ChordWidget * clicked_chord =*/
    /*timeline_arranger_widget_get_hit_chord (*/
      /*self, x, y);*/
  /*AutomationPointWidget * clicked_ap =*/
    /*timeline_arranger_widget_get_hit_ap (*/
      /*self, x, y);*/
  /*AutomationCurveWidget * ac =*/
    /*timeline_arranger_widget_get_hit_curve (*/
      /*self, x, y);*/

  menu = gtk_menu_new();

  menuitem = gtk_menu_item_new_with_label("Do something");

  /*g_signal_connect(menuitem, "activate",*/
                   /*(GCallback) view_popup_menu_onDoSomething, treeview);*/

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

  gtk_widget_show_all(menu);

  gtk_menu_popup_at_pointer (GTK_MENU(menu), NULL);
}

/**
 * Sets the visibility of the transient and non-
 * transient objects, lane and non-lane.
 *
 * E.g. when moving regions, it hides the original
 * ones.
 */
void
timeline_arranger_widget_update_visibility (
  TimelineArrangerWidget * self)
{
  ARRANGER_SET_OBJ_VISIBILITY_ARRAY (
    TL_SELECTIONS->regions,
    TL_SELECTIONS->num_regions,
    Region, region);
  ARRANGER_SET_OBJ_VISIBILITY_ARRAY (
    TL_SELECTIONS->automation_points,
    TL_SELECTIONS->num_automation_points,
    AutomationPoint, automation_point);
  ARRANGER_SET_OBJ_VISIBILITY_ARRAY (
    TL_SELECTIONS->chord_objects,
    TL_SELECTIONS->num_chord_objects,
    ChordObject, chord_object);
  ARRANGER_SET_OBJ_VISIBILITY_ARRAY (
    TL_SELECTIONS->scale_objects,
    TL_SELECTIONS->num_scale_objects,
    ScaleObject, scale_object);
  ARRANGER_SET_OBJ_VISIBILITY_ARRAY (
    TL_SELECTIONS->markers,
    TL_SELECTIONS->num_markers,
    Marker, marker);
}

void
timeline_arranger_widget_on_drag_begin_region_hit (
  TimelineArrangerWidget * self,
  double                   start_x,
  RegionWidget *           rw)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);
  REGION_WIDGET_GET_PRIVATE (rw);

  Region * region =
    region_get_main_region (rw_prv->region);

  /* open piano roll */
  clip_editor_set_region (region);

  /* if double click bring up piano roll */
  if (ar_prv->n_press == 2 &&
      !ar_prv->ctrl_held)
    SHOW_CLIP_EDITOR;

  /* get x as local to the region */
  gint wx, wy;
  gtk_widget_translate_coordinates (
    GTK_WIDGET (self),
    GTK_WIDGET (rw),
    start_x, 0, &wx, &wy);

  self->start_region = region;

  /* update arranger action */
  switch (P_TOOL)
    {
    case TOOL_ERASER:
      ar_prv->action =
        UI_OVERLAY_ACTION_ERASING;
      break;
    case TOOL_AUDITION:
      ar_prv->action =
        UI_OVERLAY_ACTION_AUDITIONING;
      break;
    case TOOL_SELECT_NORMAL:
      if (region_widget_is_resize_l (rw, wx))
        ar_prv->action =
          UI_OVERLAY_ACTION_RESIZING_L;
      else if (region_widget_is_resize_r (rw, wx))
        ar_prv->action =
          UI_OVERLAY_ACTION_RESIZING_R;
      else
        ar_prv->action =
          UI_OVERLAY_ACTION_STARTING_MOVING;
      break;
    case TOOL_SELECT_STRETCH:
      if (region_widget_is_resize_l (rw, wx))
        ar_prv->action =
          UI_OVERLAY_ACTION_STRETCHING_L;
      else if (region_widget_is_resize_r (rw, wx))
        ar_prv->action =
          UI_OVERLAY_ACTION_STRETCHING_R;
      else
        ar_prv->action =
          UI_OVERLAY_ACTION_STARTING_MOVING;
      break;
    case TOOL_EDIT:
      if (region_widget_is_resize_l (rw, wx))
        ar_prv->action =
          UI_OVERLAY_ACTION_RESIZING_L;
      else if (region_widget_is_resize_r (rw, wx))
        ar_prv->action =
          UI_OVERLAY_ACTION_RESIZING_R;
      else
        ar_prv->action =
          UI_OVERLAY_ACTION_STARTING_MOVING;
      break;
    case TOOL_RAMP:
      /* TODO */
      break;
    }

  int selected = region_is_selected (region);
  self->start_region_was_selected = selected;

  /* select region if unselected */
  if (P_TOOL == TOOL_SELECT_NORMAL ||
      P_TOOL == TOOL_SELECT_STRETCH ||
      P_TOOL == TOOL_EDIT)
    {
      /* if ctrl held & not selected, add to
       * selections */
      if (ar_prv->ctrl_held &&
          !selected)
        {
          ARRANGER_WIDGET_SELECT_REGION (
            self, region, F_SELECT, F_APPEND);
        }
      /* if ctrl not held & not selected, make it
       * the only selection */
      else if (!ar_prv->ctrl_held &&
               !selected)
        {
          ARRANGER_WIDGET_SELECT_REGION (
            self, region, F_SELECT, F_NO_APPEND);
        }
    }
}

void
timeline_arranger_widget_on_drag_begin_chord_hit (
  TimelineArrangerWidget * self,
  double                   start_x,
  ChordObjectWidget *      cw)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  ChordObject * chord = cw->chord_object;
  self->start_chord = chord;

  /* update arranger action */
  ar_prv->action =
    UI_OVERLAY_ACTION_STARTING_MOVING;
  /* FIXME cursor should be set automatically */
  ui_set_cursor_from_name (
    GTK_WIDGET (cw), "grabbing");

  int selected = chord_object_is_selected (chord);

  /* select chord if unselected */
  if (P_TOOL == TOOL_SELECT_NORMAL ||
      P_TOOL == TOOL_SELECT_STRETCH ||
      P_TOOL == TOOL_EDIT)
    {
      /* if ctrl held & not selected, add to
       * selections */
      if (ar_prv->ctrl_held && !selected)
        {
          ARRANGER_WIDGET_SELECT_CHORD (
            self, chord, F_SELECT, F_APPEND);
        }
      /* if ctrl not held & not selected, make it
       * the only selection */
      else if (!ar_prv->ctrl_held &&
               !selected)
        {
          ARRANGER_WIDGET_SELECT_CHORD (
            self, chord, F_SELECT, F_NO_APPEND);
        }
    }
}

void
timeline_arranger_widget_on_drag_begin_scale_hit (
  TimelineArrangerWidget * self,
  double                   start_x,
  ScaleObjectWidget *      cw)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  ScaleObject * scale = cw->scale_object;
  self->start_scale = scale;

  /* update arranger action */
  ar_prv->action =
    UI_OVERLAY_ACTION_STARTING_MOVING;
  /* FIXME cursor should be set automatically */
  ui_set_cursor_from_name (
    GTK_WIDGET (cw), "grabbing");

  int selected = scale_object_is_selected (scale);

  /* select scale if unselected */
  if (P_TOOL == TOOL_SELECT_NORMAL ||
      P_TOOL == TOOL_SELECT_STRETCH ||
      P_TOOL == TOOL_EDIT)
    {
      /* if ctrl held & not selected, add to
       * selections */
      if (ar_prv->ctrl_held && !selected)
        {
          ARRANGER_WIDGET_SELECT_SCALE (
            self, scale, F_SELECT, F_APPEND);
        }
      /* if ctrl not held & not selected, make it
       * the only selection */
      else if (!ar_prv->ctrl_held &&
               !selected)
        {
          ARRANGER_WIDGET_SELECT_SCALE (
            self, scale, F_SELECT, F_NO_APPEND);
        }
    }
}

void
timeline_arranger_widget_on_drag_begin_marker_hit (
  TimelineArrangerWidget * self,
  double                   start_x,
  MarkerWidget *           mw)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  Marker * marker = mw->marker;
  self->start_marker = marker;

  /* update arranger action */
  ar_prv->action =
    UI_OVERLAY_ACTION_STARTING_MOVING;
  /* FIXME cursor should be set automatically */
  ui_set_cursor_from_name (
    GTK_WIDGET (mw), "grabbing");

  int selected = marker_is_selected (marker);

  /* select scale if unselected */
  if (P_TOOL == TOOL_SELECT_NORMAL ||
      P_TOOL == TOOL_SELECT_STRETCH ||
      P_TOOL == TOOL_EDIT)
    {
      /* if ctrl held & not selected, add to
       * selections */
      if (ar_prv->ctrl_held && !selected)
        {
          ARRANGER_WIDGET_SELECT_MARKER (
            self, marker, F_SELECT, F_APPEND);
        }
      /* if ctrl not held & not selected, make it
       * the only selection */
      else if (!ar_prv->ctrl_held &&
               !selected)
        {
          ARRANGER_WIDGET_SELECT_MARKER (
            self, marker, F_SELECT, F_NO_APPEND);
        }
    }

}

void
timeline_arranger_widget_on_drag_begin_ap_hit (
  TimelineArrangerWidget * self,
  double                   start_x,
  AutomationPointWidget *  ap_widget)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  AutomationPoint * ap =
    ap_widget->automation_point;
  ar_prv->start_pos_px = start_x;
  self->start_ap = ap;
  if (!array_contains (
        TL_SELECTIONS->automation_points,
        TL_SELECTIONS->num_automation_points,
        ap))
    {
      TL_SELECTIONS->automation_points[0] =
        ap;
      TL_SELECTIONS->num_automation_points =
        1;
    }

  self->start_ap = ap;

  /* update arranger action */
  ar_prv->action =
    UI_OVERLAY_ACTION_STARTING_MOVING;
  ui_set_cursor_from_name (GTK_WIDGET (ap_widget),
                 "grabbing");

  /* update selection */
  if (ar_prv->ctrl_held)
    {
      ARRANGER_WIDGET_SELECT_AUTOMATION_POINT (
        self, ap, F_SELECT, F_APPEND);
    }
  else
    {
      timeline_arranger_widget_select_all (self, 0);
      ARRANGER_WIDGET_SELECT_AUTOMATION_POINT (
        self, ap, F_SELECT, F_NO_APPEND);
    }
}

/**
 * Create an AutomationPointat the given Position
 * in the given Track's AutomationTrack.
 *
 * @param pos The pre-snapped position.
 */
void
timeline_arranger_widget_create_ap (
  TimelineArrangerWidget * self,
  AutomationTrack *  at,
  const Position *   pos,
  const double       start_y)
{
  g_warn_if_fail (at);
  ARRANGER_WIDGET_GET_PRIVATE (self);

  float value =
    automation_lane_widget_get_fvalue_at_y (
      at->al->widget, start_y);

  ar_prv->action =
    UI_OVERLAY_ACTION_CREATING_MOVING;

  /* create a new ap */
  AutomationPoint * ap =
    automation_point_new_float (
      value, pos, F_MAIN);
  self->start_ap = ap;

  /* add it to automation track */
  automation_track_add_ap (
    at, ap, F_GEN_WIDGET, F_GEN_CURVE_POINTS);

  /* set visibility */
  arranger_object_info_set_widget_visibility_and_state (
    &ap->obj_info, 1);

  /* set position to all counterparts */
  automation_point_set_pos (
    ap, pos, F_NO_TRANS_ONLY);

  EVENTS_PUSH (
    ET_AUTOMATION_POINT_CREATED, ap);
  ARRANGER_WIDGET_SELECT_AUTOMATION_POINT (
    self, ap, F_SELECT,
    F_NO_APPEND);
}

/**
 * Create a Region at the given Position in the
 * given Track's given TrackLane.
 *
 * @param pos The pre-snapped position.
 */
void
timeline_arranger_widget_create_region (
  TimelineArrangerWidget * self,
  Track *            track,
  TrackLane *        lane,
  const Position *         pos)
{
  if (track->type == TRACK_TYPE_AUDIO)
    return;

  ARRANGER_WIDGET_GET_PRIVATE (self);

  ar_prv->action =
    UI_OVERLAY_ACTION_CREATING_RESIZING_R;

  /* create a new region */
  Region * region = NULL;
  if (track->type == TRACK_TYPE_INSTRUMENT)
    {
      region =
        midi_region_new (
          pos, pos, 1);
    }

  Position tmp;
  self->start_region = region;
  position_set_min_size (
    &region->start_pos,
    &tmp,
    ar_prv->snap_grid);
  region_set_end_pos (
    region, &tmp, F_NO_TRANS_ONLY,
    F_NO_VALIDATE);
  long length =
    region_get_full_length_in_ticks (region);
  position_from_ticks (
    &region->true_end_pos, length);
  region_set_true_end_pos (
    region, &region->true_end_pos);
  position_init (&tmp);
  region_set_clip_start_pos (
    region, &tmp);
  region_set_loop_start_pos (
    region, &tmp);
  region_set_loop_end_pos (
    region, &region->true_end_pos);

  /** add it to a lane */
  if (track->type == TRACK_TYPE_INSTRUMENT)
    {
      track_add_region (
        track, region,
        lane ? lane->pos :
        (track->num_lanes == 1 ?
         0 : track->num_lanes - 2), F_GEN_NAME,
        F_GEN_WIDGET);

      /* set visibility */
      arranger_object_info_set_widget_visibility_and_state (
        &region->obj_info, 1);
    }
  EVENTS_PUSH (ET_REGION_CREATED,
               region);
  region_set_cache_end_pos (
    region, &region->end_pos);
  ARRANGER_WIDGET_SELECT_REGION (
    self, region, F_SELECT,
    F_NO_APPEND);
}

/**
 * Wrapper for
 * timeline_arranger_widget_create_chord() or
 * timeline_arranger_widget_create_scale().
 *
 * @param y the y relative to the
 *   TimelineArrangerWidget.
 */
void
timeline_arranger_widget_create_chord_or_scale (
  TimelineArrangerWidget * self,
  Track *                  track,
  double                   y,
  const Position *         pos)
{
  int track_height =
    gtk_widget_get_allocated_height (
      GTK_WIDGET (track->widget));
  gint wy;
  gtk_widget_translate_coordinates (
    GTK_WIDGET (self),
    GTK_WIDGET (track->widget),
    0, y, NULL, &wy);

  if (y >= track_height / 2.0)
    timeline_arranger_widget_create_scale (
      self, track, pos);
  else
    timeline_arranger_widget_create_chord (
      self, track, pos);
}

/**
 * Create a ChordObject at the given Position in the
 * given Track.
 *
 * @param pos The pre-snapped position.
 */
void
timeline_arranger_widget_create_chord (
  TimelineArrangerWidget * self,
  Track *            track,
  const Position *         pos)
{
  g_warn_if_fail (track->type == TRACK_TYPE_CHORD);

  ARRANGER_WIDGET_GET_PRIVATE (self);

  ar_prv->action =
    UI_OVERLAY_ACTION_CREATING_MOVING;

  /* create a new chord */
  ChordDescriptor * descr =
    chord_descriptor_new (
      NOTE_B, 1, NOTE_B, CHORD_TYPE_MIN,
      CHORD_ACC_7, 0);
  ChordObject * chord =
    chord_object_new (
      descr, 1);

  /* add it to chord track */
  chord_track_add_chord (track, chord, 1);

  /* set visibility */
  arranger_object_info_set_widget_visibility_and_state (
    &chord->obj_info, 1);

  chord_object_set_pos (
    chord, pos, F_NO_TRANS_ONLY);

  EVENTS_PUSH (ET_CHORD_OBJECT_CREATED, chord);
  ARRANGER_WIDGET_SELECT_CHORD (
    self, chord, F_SELECT,
    F_NO_APPEND);
}

/**
 * Create a ScaleObject at the given Position in the
 * given Track.
 *
 * @param pos The pre-snapped position.
 */
void
timeline_arranger_widget_create_scale (
  TimelineArrangerWidget * self,
  Track *            track,
  const Position *         pos)
{
  g_warn_if_fail (track->type == TRACK_TYPE_CHORD);

  ARRANGER_WIDGET_GET_PRIVATE (self);

  ar_prv->action =
    UI_OVERLAY_ACTION_CREATING_MOVING;

  /* create a new scale */
  MusicalScale * descr =
    musical_scale_new (
      SCALE_AEOLIAN, NOTE_A);
  ScaleObject * scale =
    scale_object_new (
      descr, 1);

  /* add it to scale track */
  chord_track_add_scale (track, scale, 1);

  /* set visibility */
  arranger_object_info_set_widget_visibility_and_state (
    &scale->obj_info, 1);

  scale_object_set_pos (
    scale, pos, F_NO_TRANS_ONLY);

  EVENTS_PUSH (ET_SCALE_OBJECT_CREATED, scale);
  ARRANGER_WIDGET_SELECT_SCALE (
    self, scale, F_SELECT,
    F_NO_APPEND);
}

/**
 * Create a Marker at the given Position in the
 * given Track.
 *
 * @param pos The pre-snapped position.
 */
void
timeline_arranger_widget_create_marker (
  TimelineArrangerWidget * self,
  Track *            track,
  const Position *         pos)
{
  g_warn_if_fail (
    track->type == TRACK_TYPE_MARKER);

  ARRANGER_WIDGET_GET_PRIVATE (self);

  ar_prv->action =
    UI_OVERLAY_ACTION_CREATING_MOVING;

  /* create a new marker */
  Marker * marker =
    marker_new (
      "Custom Marker", 1);

  /* add it to marker track */
  marker_track_add_marker (
    track, marker, F_GEN_WIDGET);

  /* set visibility */
  arranger_object_info_set_widget_visibility_and_state (
    &marker->obj_info, 1);

  marker_set_pos (
    marker, pos, F_NO_TRANS_ONLY);

  EVENTS_PUSH (ET_MARKER_CREATED, marker);
  ARRANGER_WIDGET_SELECT_MARKER (
    self, marker, F_SELECT,
    F_NO_APPEND);
}

/**
 * Determines the selection time (objects/range)
 * and sets it.
 */
void
timeline_arranger_widget_set_select_type (
  TimelineArrangerWidget * self,
  double                   y)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  Track * track =
    timeline_arranger_widget_get_track_at_y (
      self, y);

  if (track)
    {
      if (track_widget_is_cursor_in_top_half (
            track->widget,
            y))
        {
          /* select objects */
          self->resizing_range = 0;
        }
      else
        {

          /* set resizing range flags */
          self->resizing_range = 1;
          self->resizing_range_start = 1;
          ar_prv->action =
            UI_OVERLAY_ACTION_RESIZING_R;
        }
    }
  else
    {
      /* TODO something similar as above based on
       * visible space */
      self->resizing_range = 0;
    }

  arranger_widget_refresh_all_backgrounds ();
  gtk_widget_queue_allocate (
    GTK_WIDGET (MW_RULER));
}

/**
 * First determines the selection type (objects/
 * range), then either finds and selects items or
 * selects a range.
 *
 * @param[in] delete If this is a select-delete
 *   operation
 */
void
timeline_arranger_widget_select (
  TimelineArrangerWidget * self,
  double                   offset_x,
  double                   offset_y,
  int                      delete)
{
  int i;

  ARRANGER_WIDGET_GET_PRIVATE (self);

  if (!delete)
    /* deselect all */
    arranger_widget_select_all (
      Z_ARRANGER_WIDGET (self), 0);

#define FIND_ENCLOSED_WIDGETS_OF_TYPE( \
  caps,cc,sc) \
  cc * sc; \
  cc##Widget * sc##_widget; \
  GtkWidget *  sc##_widgets[800]; \
  int          num_##sc##_widgets = 0; \
  arranger_widget_get_hit_widgets_in_range ( \
    Z_ARRANGER_WIDGET (self), \
    caps##_WIDGET_TYPE, \
    ar_prv->start_x, \
    ar_prv->start_y, \
    offset_x, \
    offset_y, \
    sc##_widgets, \
    &num_##sc##_widgets)

  FIND_ENCLOSED_WIDGETS_OF_TYPE (
    REGION, Region, region);
  for (i = 0; i < num_region_widgets; i++)
    {
      region_widget =
        Z_REGION_WIDGET (region_widgets[i]);
      REGION_WIDGET_GET_PRIVATE (region_widget);

      region =
        region_get_main_region (
          rw_prv->region);

      if (delete)
        {
          /* delete the enclosed region */
          track_remove_region (
            region->lane->track, region,
            F_FREE);
        }
      else
        {
          /* select the enclosed region */
          ARRANGER_WIDGET_SELECT_REGION (
            self, region,
            F_SELECT, F_APPEND);
        }
    }

  FIND_ENCLOSED_WIDGETS_OF_TYPE (
    CHORD_OBJECT, ChordObject, chord_object);
  for (i = 0; i < num_chord_object_widgets; i++)
    {
      chord_object_widget =
        Z_CHORD_OBJECT_WIDGET (
          chord_object_widgets[i]);

      chord_object =
        chord_object_get_main_chord_object (
          chord_object_widget->chord_object);

      if (delete)
        chord_track_remove_chord (
          P_CHORD_TRACK,
          chord_object, F_FREE);
      else
        ARRANGER_WIDGET_SELECT_CHORD (
          self, chord_object, F_SELECT, F_APPEND);
    }

  FIND_ENCLOSED_WIDGETS_OF_TYPE (
    SCALE_OBJECT, ScaleObject, scale_object);
  for (i = 0; i < num_scale_object_widgets; i++)
    {
      scale_object_widget =
        Z_SCALE_OBJECT_WIDGET (
          scale_object_widgets[i]);

      scale_object =
        scale_object_get_main_scale_object (
          scale_object_widget->scale_object);

      if (delete)
        chord_track_remove_scale (
          P_CHORD_TRACK,
          scale_object, F_FREE);
      else
        ARRANGER_WIDGET_SELECT_SCALE (
          self, scale_object, F_SELECT, F_APPEND);
    }

  FIND_ENCLOSED_WIDGETS_OF_TYPE (
    MARKER, Marker, marker);
  for (i = 0; i < num_marker_widgets; i++)
    {
      marker_widget =
        Z_MARKER_WIDGET (
          marker_widgets[i]);

      marker =
        marker_get_main_marker (
          marker_widget->marker);

      if (delete)
        marker_track_remove_marker (
          P_MARKER_TRACK, marker, F_FREE);
      else
        ARRANGER_WIDGET_SELECT_MARKER (
          self, marker, F_SELECT, F_APPEND);
    }

  /* find enclosed automation_points */
  FIND_ENCLOSED_WIDGETS_OF_TYPE (
    AUTOMATION_POINT, AutomationPoint,
    automation_point);
  for (i = 0; i < num_automation_point_widgets; i++)
    {
      automation_point_widget =
        Z_AUTOMATION_POINT_WIDGET (
          automation_point_widgets[i]);

      automation_point =
        automation_point_get_main_automation_point (
          automation_point_widget->automation_point);

      if (delete)
        automation_track_remove_ap (
          automation_point->at,
          automation_point, F_FREE);
      else
        ARRANGER_WIDGET_SELECT_AUTOMATION_POINT (
          self, automation_point, F_SELECT, F_APPEND);
    }

#undef FIND_ENCLOSED_WIDGETS_OF_TYPE
}

/**
 * Snaps the region's start point.
 *
 * @param new_start_pos Position to snap to.
 * @parram dry_run Don't resize notes; just check
 *   if the resize is allowed (check if invalid
 *   resizes will happen)
 *
 * @return 0 if the operation was successful,
 *   nonzero otherwise.
 */
static inline int
snap_region_l (
  TimelineArrangerWidget * self,
  Region *                 region,
  Position *               new_start_pos,
  int                      dry_run)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  if (SNAP_GRID_ANY_SNAP (ar_prv->snap_grid) &&
        !ar_prv->shift_held)
    position_snap (
      NULL, new_start_pos, region->lane->track,
      NULL, ar_prv->snap_grid);

  if (position_is_after_or_equal (
        new_start_pos, &region->end_pos))
    return -1;
  else if (!dry_run)
    region_set_start_pos (
      region, new_start_pos,
      F_NO_TRANS_ONLY, F_VALIDATE);

  return 0;
}

/**
 * Snaps both the transients (to show in the GUI)
 * and the actual regions.
 *
 * @param pos Absolute position in the timeline.
 * @parram dry_run Don't resize notes; just check
 *   if the resize is allowed (check if invalid
 *   resizes will happen)
 *
 * @return 0 if the operation was successful,
 *   nonzero otherwise.
 */
int
timeline_arranger_widget_snap_regions_l (
  TimelineArrangerWidget * self,
  Position *               pos,
  int                      dry_run)
{

  /* get delta with first clicked region's start
   * pos */
  long delta;
  delta =
    position_to_ticks (pos) -
    position_to_ticks (
      &self->start_region->cache_start_pos);

  /* new start pos for each region, calculated by
   * adding delta to the region's original start
   * pos */
  Position new_start_pos;

  Region * region;
  int ret;
  for (int i = 0;
       i < TL_SELECTIONS->num_regions;
       i++)
    {
      /* main trans region */
      region =
        region_get_main_trans_region (
          TL_SELECTIONS->regions[i]);

      /* caclulate new start position */
      position_set_to_pos (
        &new_start_pos,
        &region->cache_start_pos);
      position_add_ticks (
        &new_start_pos, delta);

      ret =
        snap_region_l (
          self, region, &new_start_pos, dry_run);

      if (ret)
        return ret;

      /* lane trans region */
      region =
        ((Region *) region->obj_info.lane_trans);

      ret =
        snap_region_l (
          self, region, &new_start_pos, dry_run);

      if (ret)
        return ret;
    }

  EVENTS_PUSH (ET_REGION_POSITIONS_CHANGED, NULL);

  return 0;
}

/**
 * Snaps the region's end point.
 *
 * @param new_end_pos Position to snap to.
 * @parram dry_run Don't resize notes; just check
 *   if the resize is allowed (check if invalid
 *   resizes will happen)
 *
 * @return 0 if the operation was successful,
 *   nonzero otherwise.
 */
static inline int
snap_region_r (
  TimelineArrangerWidget * self,
  Region * region,
  Position * new_end_pos,
  int        dry_run)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  if (SNAP_GRID_ANY_SNAP (ar_prv->snap_grid) &&
        !ar_prv->shift_held)
    position_snap (
      NULL, new_end_pos, region->lane->track,
      NULL, ar_prv->snap_grid);

  if (position_is_before_or_equal (
        new_end_pos, &region->start_pos))
    return -1;
  else if (!dry_run)
    {
      region_set_end_pos (
        region, new_end_pos, F_NO_TRANS_ONLY,
        F_NO_VALIDATE);

      /* if creating also set the loop points
       * appropriately */
      if (ar_prv->action ==
            UI_OVERLAY_ACTION_CREATING_RESIZING_R)
        {
          long full_size =
            region_get_full_length_in_ticks (
              region);
          Position tmp;
          position_set_to_pos (
            &tmp, &region->loop_start_pos);
          position_add_ticks (
            &tmp, full_size);
          region_set_true_end_pos (
            region, &tmp);

          /* use the setters */
          region_set_true_end_pos (
            region, &region->true_end_pos);
          region_set_loop_end_pos (
            region, &region->true_end_pos);
        }
    }

  return 0;
}

/**
 * Snaps both the transients (to show in the GUI)
 * and the actual regions.
 *
 * @param pos Absolute position in the timeline.
 * @parram dry_run Don't resize notes; just check
 *   if the resize is allowed (check if invalid
 *   resizes will happen)
 *
 * @return 0 if the operation was successful,
 *   nonzero otherwise.
 */
int
timeline_arranger_widget_snap_regions_r (
  TimelineArrangerWidget * self,
  Position *               pos,
  int                      dry_run)
{
  /* get delta with first clicked region's end
   * pos */
  long delta =
    position_to_ticks (pos) -
    position_to_ticks (
      &self->start_region->cache_end_pos);

  /* new end pos for each region, calculated by
   * adding delta to the region's original end
   * pos */
  Position new_end_pos;

  Region * region;
  int ret;
  for (int i = 0;
       i < TL_SELECTIONS->num_regions;
       i++)
    {
      region =
        TL_SELECTIONS->regions[i];

      /* main region */
      region =
        region_get_main_region (region);

      position_set_to_pos (
        &new_end_pos,
        &region->cache_end_pos);
      position_add_ticks (
        &new_end_pos, delta);

      ret =
        snap_region_r (
          self, region, &new_end_pos, dry_run);

      if (ret)
        return ret;
    }

  EVENTS_PUSH (ET_REGION_POSITIONS_CHANGED, NULL);

  return 0;
}

void
timeline_arranger_widget_snap_range_r (
  Position *               pos)
{
  ARRANGER_WIDGET_GET_PRIVATE (MW_TIMELINE);

  if (MW_TIMELINE->resizing_range_start)
    {
      /* set range 1 at current point */
      ui_px_to_pos_timeline (
        ar_prv->start_x,
        &PROJECT->range_1,
        1);
      if (SNAP_GRID_ANY_SNAP (
            ar_prv->snap_grid) &&
          !ar_prv->shift_held)
        position_snap_simple (
          &PROJECT->range_1,
          SNAP_GRID_TIMELINE);
      position_set_to_pos (
        &PROJECT->range_2,
        &PROJECT->range_1);

      MW_TIMELINE->resizing_range_start = 0;
    }

  /* set range */
  if (SNAP_GRID_ANY_SNAP (ar_prv->snap_grid) &&
      !ar_prv->shift_held)
    position_snap_simple (
      pos,
      SNAP_GRID_TIMELINE);
  position_set_to_pos (
    &PROJECT->range_2,
    pos);
  project_set_has_range (1);

  EVENTS_PUSH (ET_RANGE_SELECTION_CHANGED,
               NULL);

  arranger_widget_refresh_all_backgrounds ();
}

/**
 * Moves the TimelineSelections by the given
 * amount of ticks.
 *
 * @param ticks_diff Ticks to move by.
 * @param copy_moving 1 if copy-moving.
 */
void
timeline_arranger_widget_move_items_x (
  TimelineArrangerWidget * self,
  long                     ticks_diff,
  int                      copy_moving)
{
  timeline_selections_add_ticks (
    TL_SELECTIONS, ticks_diff, F_USE_CACHED,
    F_TRANS_ONLY);

  /* for MIDI arranger ruler */
  EVENTS_PUSH (ET_REGION_POSITIONS_CHANGED,
               NULL);
  /* for arranger refresh */
  EVENTS_PUSH (ET_TIMELINE_OBJECTS_IN_TRANSIT,
               NULL);
}

/**
 * Sets width to ruler width and height to
 * tracklist height.
 */
void
timeline_arranger_widget_set_size (
  TimelineArrangerWidget * self)
{
  // set the size
  int ww, hh;
  if (self->is_pinned)
    gtk_widget_get_size_request (
      GTK_WIDGET (MW_PINNED_TRACKLIST),
      &ww,
      &hh);
  else
    gtk_widget_get_size_request (
      GTK_WIDGET (MW_TRACKLIST),
      &ww,
      &hh);
  RULER_WIDGET_GET_PRIVATE (MW_RULER);
  gtk_widget_set_size_request (
    GTK_WIDGET (self),
    rw_prv->total_px,
    hh);
}

#define COMPARE_AND_SET(pos) \
  if ((pos)->bars > self->last_timeline_obj_bars) \
    self->last_timeline_obj_bars = (pos)->bars;

/**
 * Updates last timeline objet so that timeline can be
 * expanded/contracted accordingly.
 */
static int
update_last_timeline_object ()
{
  if (!MAIN_WINDOW || !GTK_IS_WIDGET (MAIN_WINDOW))
    return G_SOURCE_CONTINUE;

  TimelineArrangerWidget * self = MW_TIMELINE;

  int prev = self->last_timeline_obj_bars;
  self->last_timeline_obj_bars = 0;

  COMPARE_AND_SET (&TRANSPORT->end_marker_pos);

  Track * track;
  Region * region;
  AutomationPoint * ap;
  for (int i = 0; i < TRACKLIST->num_tracks; i++)
    {
      track = TRACKLIST->tracks[i];

      region =
        track_get_last_region (track);
      ap =
        track_get_last_automation_point (track);

      if (region)
        COMPARE_AND_SET (&region->end_pos);

      if (ap)
        COMPARE_AND_SET (&ap->pos);
    }

  if (prev != self->last_timeline_obj_bars &&
      self->last_timeline_obj_bars > 127)
    EVENTS_PUSH (ET_LAST_TIMELINE_OBJECT_CHANGED,
                 NULL);

  return G_SOURCE_CONTINUE;
}

#undef COMPARE_AND_SET

/**
 * To be called once at init time.
 */
void
timeline_arranger_widget_setup (
  TimelineArrangerWidget * self)
{
  timeline_arranger_widget_set_size (
    self);

  g_timeout_add (
    1000,
    update_last_timeline_object,
    NULL);
}

void
timeline_arranger_widget_move_items_y (
  TimelineArrangerWidget * self,
  double                   offset_y)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  if (TL_SELECTIONS->num_regions)
    {
      /* check if should be moved to new track */
      Track * track =
        timeline_arranger_widget_get_track_at_y (
        self, ar_prv->start_y + offset_y);
      Track * old_track =
        self->start_region->lane->track;
      if (track)
        {
          Track * pt =
            tracklist_get_prev_visible_track (
              TRACKLIST, old_track);
          Track * nt =
            tracklist_get_next_visible_track (
              TRACKLIST, old_track);
          Track * tt =
            tracklist_get_first_visible_track (
              TRACKLIST);
          Track * bt =
            tracklist_get_last_visible_track (
              TRACKLIST);
          /* highest selected track */
          Track * hst =
            timeline_selections_get_highest_track (
              TL_SELECTIONS,
              arranger_widget_is_in_moving_operation (
                Z_ARRANGER_WIDGET (self)));
          /* lowest selected track */
          Track * lst =
            timeline_selections_get_lowest_track (
              TL_SELECTIONS,
              arranger_widget_is_in_moving_operation (
                Z_ARRANGER_WIDGET (self)));

          if (self->start_region->lane->track !=
              track)
            {
              /* if new track is lower and bot region is not at the lowest track */
              if (track == nt &&
                  lst != bt)
                {
                  /* shift all selected regions to their next track */
                  for (int i = 0; i < TL_SELECTIONS->num_regions; i++)
                    {
                      Region * region =
                        TL_SELECTIONS->regions[i];
                      region = region_get_main_trans_region (region);
                      nt =
                        tracklist_get_next_visible_track (
                          TRACKLIST,
                          region->lane->track);
                      old_track = region->lane->track;
                      if (old_track->type == nt->type)
                        {
                          if (nt->type ==
                                TRACK_TYPE_INSTRUMENT)
                            {
                              track_remove_region (
                                old_track,
                                region, F_NO_FREE);
                              track_add_region (
                                nt, region, 0,
                                F_NO_GEN_NAME,
                                F_NO_GEN_WIDGET);
                            }
                          else if (nt->type ==
                                     TRACK_TYPE_AUDIO)
                            {
                              /* TODO */

                            }
                        }
                    }
                }
              else if (track == pt &&
                       hst != tt)
                {
                  /*g_message ("track %s top region track %s tt %s",*/
                             /*track->channel->name,*/
                             /*self->top_region->track->channel->name,*/
                             /*tt->channel->name);*/
                  /* shift all selected regions to their prev track */
                  for (int i = 0; i < TL_SELECTIONS->num_regions; i++)
                    {
                      Region * region = TL_SELECTIONS->regions[i];
                      pt =
                        tracklist_get_prev_visible_track (
                          TRACKLIST,
                          region->lane->track);
                      old_track = region->lane->track;
                      if (old_track->type == pt->type)
                        {
                          if (pt->type ==
                                TRACK_TYPE_INSTRUMENT)
                            {
                              track_remove_region (
                                old_track,
                                region, F_NO_FREE);
                              track_add_region (
                                pt, region, 0,
                                F_NO_GEN_NAME,
                                F_NO_GEN_WIDGET);
                            }
                          else if (pt->type ==
                                     TRACK_TYPE_AUDIO)
                            {
                              /* TODO */

                            }
                        }
                    }
                }
            }
        }
    }
  else if (TL_SELECTIONS->num_automation_points)
    {
      for (int i = 0;
           i < TL_SELECTIONS->
             num_automation_points; i++)
        {
          AutomationPoint * ap =
            TL_SELECTIONS->
              automation_points[i];

          ap = automation_point_get_main_automation_point (ap);

          float fval =
            automation_lane_widget_get_fvalue_at_y (
              ap->at->al->widget,
              ar_prv->start_y + offset_y);
          automation_point_update_fvalue (
            ap, fval, F_TRANS_ONLY);
        }
      automation_point_widget_update_tooltip (
        self->start_ap->widget, 1);
    }
}

/**
 * Returns the ticks objects were moved by since
 * the start of the drag.
 *
 * FIXME not really needed, can use
 * timeline_selections_get_start_pos and the
 * arranger's earliest_obj_start_pos.
 */
static long
get_moved_diff (
  TimelineArrangerWidget * self)
{
#define GET_DIFF(sc,pos_name) \
  if (TL_SELECTIONS->num_##sc##s) \
    { \
      return \
        position_to_ticks ( \
          &sc##_get_main_trans_##sc ( \
            TL_SELECTIONS->sc##s[0])->pos_name) - \
        position_to_ticks ( \
          &sc##_get_main_##sc ( \
            TL_SELECTIONS->sc##s[0])->pos_name); \
    }

  GET_DIFF (region, start_pos);
  GET_DIFF (marker, pos);
  GET_DIFF (chord_object, pos);
  GET_DIFF (scale_object, pos);
  GET_DIFF (automation_point, pos);

  g_return_val_if_reached (0);
}

/**
 * Sets the default cursor in all selected regions and
 * intializes start positions.
 */
void
timeline_arranger_widget_on_drag_end (
  TimelineArrangerWidget * self)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  AutomationPoint * ap;
  for (int i = 0;
       i < TL_SELECTIONS->
             num_automation_points; i++)
    {
      ap =
        TL_SELECTIONS->automation_points[i];
      automation_point_widget_update_tooltip (
        ap->widget, 0);
    }

  if (ar_prv->action ==
        UI_OVERLAY_ACTION_RESIZING_L)
    {
      if (!self->resizing_range)
        {
          Region * main_region =
            TL_SELECTIONS->regions[0]->obj_info.main;
          Region * main_trans_region =
            TL_SELECTIONS->regions[0]->obj_info.
              main_trans;
          UndoableAction * ua =
            (UndoableAction *)
            edit_timeline_selections_action_new (
              TL_SELECTIONS,
              ETS_RESIZE_L,
              position_to_ticks (
                &main_trans_region->start_pos) -
              position_to_ticks (
                &main_region->start_pos),
              NULL, NULL);
          undo_manager_perform (
            UNDO_MANAGER, ua);
        }
    }
  else if (ar_prv->action ==
        UI_OVERLAY_ACTION_RESIZING_R)
    {
      if (!self->resizing_range)
        {
          Region * main_region =
            TL_SELECTIONS->regions[0]->obj_info.main;
          Region * main_trans_region =
            TL_SELECTIONS->regions[0]->obj_info.
              main_trans;
          UndoableAction * ua =
            (UndoableAction *)
            edit_timeline_selections_action_new (
              TL_SELECTIONS,
              ETS_RESIZE_R,
              position_to_ticks (
                &main_trans_region->end_pos) -
              position_to_ticks (
                &main_region->end_pos),
              NULL, NULL);
          undo_manager_perform (
            UNDO_MANAGER, ua);
        }
    }
  else if (ar_prv->action ==
        UI_OVERLAY_ACTION_STARTING_MOVING)
    {
      /* if something was clicked with ctrl without
       * moving*/
      if (ar_prv->ctrl_held)
        {
          if (self->start_region &&
              self->start_region_was_selected)
            {
              /* deselect it */
              ARRANGER_WIDGET_SELECT_REGION (
                self, self->start_region,
                F_NO_SELECT, F_APPEND);
            }
        }
      else if (ar_prv->n_press == 2)
        {
          /* double click on object */
          /*g_message ("DOUBLE CLICK");*/
        }
    }
  else if (ar_prv->action ==
             UI_OVERLAY_ACTION_MOVING)
    {
      Position earliest_trans_pos;
      timeline_selections_get_start_pos (
        TL_SELECTIONS,
        &earliest_trans_pos, 1);
      UndoableAction * ua =
        (UndoableAction *)
        move_timeline_selections_action_new (
          TL_SELECTIONS,
          position_to_ticks (
            &earliest_trans_pos) -
          position_to_ticks (
            &ar_prv->earliest_obj_start_pos),
          timeline_selections_get_highest_track (
            TL_SELECTIONS, F_TRANSIENTS) -
          timeline_selections_get_highest_track (
            TL_SELECTIONS, F_NO_TRANSIENTS));
      undo_manager_perform (
        UNDO_MANAGER, ua);
    }
  /* if copy/link-moved */
  else if (ar_prv->action ==
             UI_OVERLAY_ACTION_MOVING_COPY ||
           ar_prv->action ==
             UI_OVERLAY_ACTION_MOVING_LINK)
    {
      Position earliest_trans_pos;
      timeline_selections_get_start_pos (
        TL_SELECTIONS,
        &earliest_trans_pos, 1);
      UndoableAction * ua =
        (UndoableAction *)
        duplicate_timeline_selections_action_new (
          TL_SELECTIONS,
          position_to_ticks (
            &earliest_trans_pos) -
          position_to_ticks (
            &ar_prv->earliest_obj_start_pos),
          timeline_selections_get_highest_track (
            TL_SELECTIONS, F_TRANSIENTS) -
          timeline_selections_get_highest_track (
            TL_SELECTIONS, F_NO_TRANSIENTS));
      timeline_selections_reset_transient_poses (
        TL_SELECTIONS);
      timeline_selections_clear (
        TL_SELECTIONS);
      undo_manager_perform (
        UNDO_MANAGER, ua);
    }
  else if (ar_prv->action ==
             UI_OVERLAY_ACTION_NONE ||
           ar_prv->action ==
             UI_OVERLAY_ACTION_STARTING_SELECTION)
    {
      timeline_selections_clear (
        TL_SELECTIONS);
    }
  /* if something was created */
  else if (ar_prv->action ==
             UI_OVERLAY_ACTION_CREATING_MOVING ||
           ar_prv->action ==
             UI_OVERLAY_ACTION_CREATING_RESIZING_R)
    {
      timeline_selections_set_to_transient_poses (
        TL_SELECTIONS);
      timeline_selections_set_to_transient_values (
        TL_SELECTIONS);

      UndoableAction * ua =
        (UndoableAction *)
        create_timeline_selections_action_new (
          TL_SELECTIONS);
      undo_manager_perform (
        UNDO_MANAGER, ua);
    }
  /* if didn't click on something */
  else
    {
    }
  ar_prv->action = UI_OVERLAY_ACTION_NONE;
  timeline_arranger_widget_update_visibility (
    self);

  self->resizing_range = 0;
  self->resizing_range_start = 0;
  self->start_region = NULL;
  self->start_ap = NULL;
  self->start_region_was_selected = 0;
  self->start_ap = NULL;

  EVENTS_PUSH (ET_TL_SELECTIONS_CHANGED,
               NULL);
}

static void
add_children_from_channel_track (
  ChannelTrack * ct)
{
  if (!((Track *)ct)->bot_paned_visible)
    return;

  int i,j,k;

  AutomationTracklist * atl =
    &ct->automation_tracklist;
  AutomationLane * al;
  for (i = 0; i < atl->num_als; i++)
    {
      al = atl->als[i];
      if (al->visible)
        {
          for (j = 0; j < al->at->num_aps; j++)
            {
              AutomationPoint * ap =
                al->at->aps[j];

              for (k = 0; k < 2; k++)
                {
                  if (k == 0)
                    ap = automation_point_get_main_automation_point (ap);
                  else if (k == 1)
                    ap = automation_point_get_main_trans_automation_point (ap);

                  if (!GTK_IS_WIDGET (ap->widget))
                    ap->widget =
                      automation_point_widget_new (ap);

                  gtk_overlay_add_overlay (
                    GTK_OVERLAY (MW_TIMELINE),
                    GTK_WIDGET (ap->widget));
                }
            }
          for (j = 0; j < al->at->num_acs; j++)
            {
              AutomationCurve * ac =
                al->at->acs[j];
              if (!GTK_IS_WIDGET (ac->widget))
                ac->widget =
                  automation_curve_widget_new (ac);

              gtk_overlay_add_overlay (
                GTK_OVERLAY (MW_TIMELINE),
                GTK_WIDGET (ac->widget));
            }
        }
    }
}

static void
add_children_from_chord_track (
  TimelineArrangerWidget * self,
  ChordTrack *             ct)
{
  int i, k;
  for (i = 0; i < ct->num_chords; i++)
    {
      ChordObject * c = ct->chords[i];
      for (k = 0 ; k < 2; k++)
        {
          if (k == 0)
            c = chord_object_get_main_chord_object (c);
          else if (k == 1)
            c = chord_object_get_main_trans_chord_object (c);

          if (!c->widget)
            c->widget =
              chord_object_widget_new (c);

          gtk_overlay_add_overlay (
            GTK_OVERLAY (self),
            GTK_WIDGET (c->widget));
        }
    }

  for (i = 0; i < ct->num_scales; i++)
    {
      ScaleObject * c = ct->scales[i];
      for (k = 0 ; k < 2; k++)
        {
          if (k == 0)
            c = scale_object_get_main_scale_object (c);
          else if (k == 1)
            c = scale_object_get_main_trans_scale_object (c);

          if (!c->widget)
            c->widget =
              scale_object_widget_new (c);

          gtk_overlay_add_overlay (
            GTK_OVERLAY (self),
            GTK_WIDGET (c->widget));
        }
    }
}

static void
add_children_from_marker_track (
  TimelineArrangerWidget * self,
  Track *                  track)
{
  int i, k;
  for (i = 0; i < track->num_markers; i++)
    {
      Marker * m = track->markers[i];
      for (k = 0 ; k < 2; k++)
        {
          if (k == 0)
            m = marker_get_main_marker (m);
          else if (k == 1)
            m = marker_get_main_trans_marker (m);

          if (!m->widget)
            m->widget =
              marker_widget_new (m);

          gtk_overlay_add_overlay (
            GTK_OVERLAY (self),
            GTK_WIDGET (m->widget));
        }
    }
}

static inline void
add_children_from_instrument_track (
  TimelineArrangerWidget * self,
  Track *                  it)
{
  TrackLane * lane;
  Region * r;
  int i, j, k;
  for (j = 0; j < it->num_lanes; j++)
    {
      lane = it->lanes[j];

      for (i = 0; i < lane->num_regions; i++)
        {
          r = lane->regions[i];

          for (k = 0 ; k < 4; k++)
            {
              if (k == 0)
                r = region_get_main_region (r);
              else if (k == 1)
                r = region_get_main_trans_region (r);
              else if (k == 2)
                r = region_get_lane_region (r);
              else if (k == 3)
                r = region_get_lane_trans_region (r);

              if (!r->widget)
                r->widget =
                  Z_REGION_WIDGET (
                    midi_region_widget_new (
                      r));

              gtk_overlay_add_overlay (
                GTK_OVERLAY (self),
                GTK_WIDGET (r->widget));
            }
        }
    }
  add_children_from_channel_track (it);
}

static void
add_children_from_audio_track (
  TimelineArrangerWidget * self,
  AudioTrack *             at)
{
  TrackLane * lane;
  Region * r;
  int i, j;
  for (j = 0; j < at->num_lanes; j++)
    {
      lane = at->lanes[j];

      for (i = 0; i < lane->num_regions; i++)
        {
          r = lane->regions[i];
          gtk_overlay_add_overlay (
            GTK_OVERLAY (self),
            GTK_WIDGET (r->widget));
        }
    }
  add_children_from_channel_track (at);
}

static void
add_children_from_master_track (
  TimelineArrangerWidget * self,
  MasterTrack *            mt)
{
  ChannelTrack * ct = (ChannelTrack *) mt;
  add_children_from_channel_track (ct);
}

static void
add_children_from_bus_track (
  TimelineArrangerWidget * self,
  BusTrack *               mt)
{
  ChannelTrack * ct = (ChannelTrack *) mt;
  add_children_from_channel_track (ct);
}

/**
 * Refreshes visibility of children.
 */
void
timeline_arranger_widget_refresh_visibility (
  TimelineArrangerWidget * self)
{
  GList *children, *iter;
  children =
    gtk_container_get_children (
      GTK_CONTAINER (self));
  GtkWidget * w;
  RegionWidget * rw;
  Region * region;
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      w = GTK_WIDGET (iter->data);

      if (Z_IS_REGION_WIDGET (w))
        {
          rw = Z_REGION_WIDGET (w);
          REGION_WIDGET_GET_PRIVATE (rw);
          region = rw_prv->region;

          arranger_object_info_set_widget_visibility_and_state (
            &region->obj_info, 1);
        }
    }
  g_list_free (children);
}

/**
 * Readd children.
 */
void
timeline_arranger_widget_refresh_children (
  TimelineArrangerWidget * self)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  /* remove all children except bg && playhead */
  GList *children, *iter;

  children =
    gtk_container_get_children (
      GTK_CONTAINER (self));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      GtkWidget * widget = GTK_WIDGET (iter->data);
      if (widget != (GtkWidget *) ar_prv->bg &&
          widget != (GtkWidget *) ar_prv->playhead)
        {
          /*g_object_ref (widget);*/
          gtk_container_remove (
            GTK_CONTAINER (self),
            widget);
        }
    }
  g_list_free (children);

  /* add tracks */
  for (int i = 0; i < TRACKLIST->num_tracks; i++)
    {
      Track * track = TRACKLIST->tracks[i];
      if (track->visible &&
          track->pinned == self->is_pinned)
        {
          switch (track->type)
            {
            case TRACK_TYPE_CHORD:
              add_children_from_chord_track (
                self, track);
              break;
            case TRACK_TYPE_INSTRUMENT:
              add_children_from_instrument_track (
                self,
                (InstrumentTrack *) track);
              break;
            case TRACK_TYPE_MASTER:
              add_children_from_master_track (
                self,
                (MasterTrack *) track);
              break;
            case TRACK_TYPE_AUDIO:
              add_children_from_audio_track (
                self,
                (AudioTrack *) track);
              break;
            case TRACK_TYPE_BUS:
              add_children_from_bus_track (
                self,
                (BusTrack *) track);
              break;
            case TRACK_TYPE_MARKER:
              add_children_from_marker_track (
                self, track);
              break;
            case TRACK_TYPE_GROUP:
              /* TODO */
              break;
            default:
              /* TODO */
              break;
            }
        }
    }

  timeline_arranger_widget_refresh_visibility (
    self);

  gtk_overlay_reorder_overlay (
    GTK_OVERLAY (self),
    (GtkWidget *) ar_prv->playhead, -1);
}

/**
 * Scroll to the given position.
 */
void
timeline_arranger_widget_scroll_to (
  TimelineArrangerWidget * self,
  Position *               pos)
{
  /* TODO */

}

static gboolean
on_focus (GtkWidget       *widget,
          gpointer         user_data)
{
  /*g_message ("timeline focused");*/
  MAIN_WINDOW->last_focused = widget;

  return FALSE;
}

static void
timeline_arranger_widget_class_init (
  TimelineArrangerWidgetClass * klass)
{
}

static void
timeline_arranger_widget_init (
  TimelineArrangerWidget *self )
{
  g_signal_connect (
    self, "grab-focus",
    G_CALLBACK (on_focus), self);
}
