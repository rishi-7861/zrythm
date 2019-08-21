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

#include <stdlib.h>

#include "actions/edit_tracks_action.h"
#include "actions/undo_manager.h"
#include "audio/audio_track.h"
#include "audio/automation_point.h"
#include "audio/automation_track.h"
#include "audio/bus_track.h"
#include "audio/channel.h"
#include "audio/chord_track.h"
#include "audio/group_track.h"
#include "audio/instrument_track.h"
#include "audio/master_track.h"
#include "audio/midi_track.h"
#include "audio/instrument_track.h"
#include "audio/track.h"
#include "gui/backend/events.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/audio_region.h"
#include "gui/widgets/channel.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/midi_region.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/track.h"
#include "project.h"
#include "utils/arrays.h"
#include "utils/flags.h"
#include "utils/objects.h"

#include <glib/gi18n.h>

void
track_init_loaded (Track * track)
{
  int i,j;
  TrackLane * lane;
  for (j = 0; j < track->num_lanes; j++)
    {
      lane = track->lanes[j];
      lane->track = track;
      track_lane_init_loaded (lane);
    }
  ScaleObject * scale;
  for (i = 0; i < track->num_scales; i++)
    {
      scale = track->scales[i];
      scale_object_init_loaded (scale);
    }
  Marker * marker;
  for (i = 0; i < track->num_markers; i++)
    {
      marker = track->markers[i];
      marker_init_loaded (marker);
    }

  /* init loaded channel */
  Channel * ch;
  if (track->channel)
    {
      ch = track->channel;
      ch->track = track;
      channel_init_loaded (ch);
    }

  /* set track to automation tracklist */
  AutomationTracklist * atl;
  atl = &track->automation_tracklist;
  atl->track = track;
  automation_tracklist_init_loaded (atl);
}

/**
 * Adds a new TrackLane to the Track.
 */
static void
track_add_lane (
  Track * self)
{
  array_double_size_if_full (
    self->lanes, self->num_lanes,
    self->lanes_size, TrackLane *);
  self->lanes[self->num_lanes++] =
    track_lane_new (self, self->num_lanes);

  EVENTS_PUSH (ET_TRACK_LANE_ADDED,
               self->lanes[self->num_lanes - 1]);
}

void
track_init (Track * self)
{
  self->visible = 1;
  self->handle_pos = 1;
  track_add_lane (self);
}

/**
 * Creates a track with the given label and returns
 * it.
 *
 * If the TrackType is one that needs a Channel,
 * then a Channel is also created for the track.
 */
Track *
track_new (
  TrackType type,
  char * label)
{
  Track * track =
    calloc (1, sizeof (Track));

  track_init (track);

  track->name = g_strdup (label);

  ChannelType ct;
  switch (type)
    {
    case TRACK_TYPE_INSTRUMENT:
      instrument_track_init (track);
      ct = CT_MIDI;
      break;
    case TRACK_TYPE_AUDIO:
      audio_track_init (track);
      ct = CT_AUDIO;
      break;
    case TRACK_TYPE_MASTER:
      master_track_init (track);
      ct = CT_MASTER;
      break;
    case TRACK_TYPE_BUS:
      bus_track_init (track);
      ct = CT_BUS;
      break;
    case TRACK_TYPE_GROUP:
      group_track_init (track);
      ct = CT_BUS;
      break;
    case TRACK_TYPE_MIDI:
      midi_track_init (track);
      ct = CT_MIDI;
      break;
    case TRACK_TYPE_CHORD:
      break;
    default:
      g_return_val_if_reached (NULL);
    }

  automation_tracklist_init (
    &track->automation_tracklist,
    track);

  /* if should have channel */
  if (type != TRACK_TYPE_CHORD)
    {
      Channel * ch =
        channel_new (ct, track);
      track->channel = ch;

      ch->track = track;

      channel_generate_automation_tracks (ch);
    }

  return track;
}

/**
 * Clones the track and returns the clone.
 */
Track *
track_clone (Track * track)
{
  int j;
  Track * new_track =
    track_new (
      track->type,
      track->name);

#define COPY_MEMBER(a) \
  new_track->a = track->a

  COPY_MEMBER (type);
  COPY_MEMBER (bot_paned_visible);
  COPY_MEMBER (visible);
  COPY_MEMBER (handle_pos);
  COPY_MEMBER (mute);
  COPY_MEMBER (solo);
  COPY_MEMBER (recording);
  COPY_MEMBER (active);
  COPY_MEMBER (color.red);
  COPY_MEMBER (color.green);
  COPY_MEMBER (color.blue);
  COPY_MEMBER (color.alpha);
  COPY_MEMBER (pos);

  Channel * ch =
    channel_clone (track->channel, new_track);
  new_track->channel = ch;
  ch->track = track;

  TrackLane * lane, * new_lane;
  for (j = 0; j < track->num_lanes; j++)
    {
      /* clone lane */
       lane = track->lanes[j];
       new_lane =
         track_lane_clone (lane);

       /* add to new track */
       array_append (
         new_track->lanes,
         new_track->num_lanes,
         new_lane);
    }

  automation_tracklist_clone (
    &track->automation_tracklist,
    &new_track->automation_tracklist);

#undef COPY_MEMBER

  return new_track;
}

/**
 * Sets recording and connects/disconnects the JACK ports.
 */
void
track_set_recording (Track *   track,
                     int       recording)
{
  Channel * channel =
    track_get_channel (track);

  if (!channel)
    {
      ui_show_notification_idle (
        "Recording not implemented yet for this "
        "track.");
      return;
    }

  if (recording)
    {
      port_connect (
        AUDIO_ENGINE->stereo_in->l,
        channel->stereo_in->l, 1);
      port_connect (
        AUDIO_ENGINE->stereo_in->r,
        channel->stereo_in->r, 1);
      port_connect (
        AUDIO_ENGINE->midi_in,
        channel->midi_in, 1);
    }
  else
    {
      port_disconnect (
        AUDIO_ENGINE->stereo_in->l,
        channel->stereo_in->l);
      port_disconnect (
        AUDIO_ENGINE->stereo_in->r,
        channel->stereo_in->r);
      port_disconnect (
        AUDIO_ENGINE->midi_in,
        channel->midi_in);
    }

  track->recording = recording;

  mixer_recalc_graph (MIXER);

  EVENTS_PUSH (ET_TRACK_STATE_CHANGED,
               track);
}

/**
 * Sets track muted and optionally adds the action
 * to the undo stack.
 */
void
track_set_muted (Track * track,
                 int     mute,
                 int     trigger_undo)
{
  UndoableAction * action =
    edit_tracks_action_new (
      EDIT_TRACK_ACTION_TYPE_MUTE,
      track,
      TRACKLIST_SELECTIONS,
      0.f, 0.f, 0, mute);
  g_message ("setting mute to %d",
             mute);
  if (trigger_undo)
    {
      undo_manager_perform (UNDO_MANAGER,
                            action);
    }
  else
    {
      edit_tracks_action_do (
        (EditTracksAction *) action);
    }
}

/**
 * Fills in the array with all the velocities in
 * the project that are within or outside the
 * range given.
 *
 * @param inside Whether to find velocities inside
 *   the range (1) or outside (0).
 */
void
track_get_velocities_in_range (
  const Track *    track,
  const Position * start_pos,
  const Position * end_pos,
  Velocity ***     velocities,
  int *            num_velocities,
  int *            velocities_size,
  int              inside)
{
  if (track->type != TRACK_TYPE_MIDI &&
      track->type != TRACK_TYPE_INSTRUMENT)
    return;

  TrackLane * lane;
  Region * region;
  MidiNote * mn;
  Position global_start_pos;
  for (int i = 0; i < track->num_lanes; i++)
    {
      lane = track->lanes[i];
      for (int j = 0; j < lane->num_regions; j++)
        {
          region = lane->regions[j];
          for (int k = 0;
               k < region->num_midi_notes; k++)
            {
              mn = region->midi_notes[k];
              midi_note_get_global_start_pos (
                mn, &global_start_pos);

#define ADD_VELOCITY \
  array_double_size_if_full ( \
    *velocities, *num_velocities, \
    *velocities_size, Velocity *); \
  (*velocities)[(* num_velocities)++] = \
    mn->vel

              if (inside &&
                  position_is_after_or_equal (
                    &global_start_pos, start_pos) &&
                  position_is_before_or_equal (
                    &global_start_pos, end_pos))
                {
                  ADD_VELOCITY;
                }
              else if (!inside &&
                  (position_is_before (
                    &global_start_pos, start_pos) ||
                  position_is_after (
                    &global_start_pos, end_pos)))
                {
                  ADD_VELOCITY;
                }
#undef ADD_VELOCITY
            }
        }
    }
}

/**
 * Sets track soloed, updates UI and optionally
 * adds the action to the undo stack.
 */
void
track_set_soloed (Track * track,
                  int     solo,
                  int     trigger_undo)
{
  UndoableAction * action =
    edit_tracks_action_new (
      EDIT_TRACK_ACTION_TYPE_SOLO,
      track,
      TRACKLIST_SELECTIONS,
      0.f, 0.f, solo, 0);
  if (trigger_undo)
    {
      undo_manager_perform (UNDO_MANAGER,
                            action);
    }
  else
    {
      edit_tracks_action_do (
        (EditTracksAction *) action);
    }
}

/**
 * Returns if Track is in TracklistSelections.
 */
int
track_is_selected (Track * self)
{
  if (tracklist_selections_contains_track (
        TRACKLIST_SELECTIONS, self))
    return 1;

  return 0;
}

/**
 * Returns the last region in the track, or NULL.
 */
Region *
track_get_last_region (
  Track * track)
{
  int i, j;
  Region * last_region = NULL, * r;
  Position tmp;
  position_init (&tmp);

  if (track->type == TRACK_TYPE_AUDIO ||
      track->type == TRACK_TYPE_INSTRUMENT)
    {
      TrackLane * lane;
      for (i = 0; i < track->num_lanes; i++)
        {
          lane = track->lanes[i];

          for (j = 0; j < lane->num_regions; j++)
            {
              r = lane->regions[j];
              if (position_is_after (
                    &r->end_pos,
                    &tmp))
                {
                  last_region = r;
                  position_set_to_pos (
                    &tmp, &r->end_pos);
                }
            }
        }
    }

  AutomationTracklist * atl =
    &track->automation_tracklist;
  AutomationTrack * at;
  for (i = 0; i < atl->num_ats; i++)
    {
      at = atl->ats[i];
      r = automation_track_get_last_region (at);

      if (!r)
        continue;

      if (position_is_after (
            &r->end_pos, &tmp))
        {
          last_region = r;
          position_set_to_pos (
            &tmp, &r->end_pos);
        }
    }

  return last_region;
}

/**
 * Wrapper.
 */
void
track_setup (Track * track)
{
#define SETUP_TRACK(uc,sc) \
  case TRACK_TYPE_##uc: \
    sc##_track_setup (track); \
    break;

  switch (track->type)
    {
    SETUP_TRACK (INSTRUMENT, instrument);
    SETUP_TRACK (MASTER, master);
    SETUP_TRACK (AUDIO, audio);
    SETUP_TRACK (BUS, bus);
    SETUP_TRACK (GROUP, group);
    case TRACK_TYPE_CHORD:
    /* TODO */
    default:
      break;
    }

#undef SETUP_TRACK
}

/**
 * Adds a Region to the given lane of the track.
 *
 * The Region must be the main region (see
 * ArrangerObjectInfo).
 *
 * @param at The AutomationTrack of this Region, if
 *   automation region.
 * @param lane_pos The position of the lane to add
 *   to, if applicable.
 * @param gen_name Generate a unique region name or
 *   not. This will be 0 if the caller already
 *   generated a unique name.
 */
void
track_add_region (
  Track * track,
  Region * region,
  AutomationTrack * at,
  int      lane_pos,
  int      gen_name)
{
  g_warn_if_fail (
    region->obj_info.counterpart ==
    AOI_COUNTERPART_MAIN);
  g_warn_if_fail (
    region->obj_info.main &&
    region->obj_info.main_trans &&
    region->obj_info.lane &&
    region->obj_info.lane_trans);

  if (region->type == REGION_TYPE_AUTOMATION)
    {
      track = at->track;
    }
  g_warn_if_fail (track);

  if (gen_name)
    {
      int count = 1;
      char * name = g_strdup (track->name);
      while (region_find_by_name (name))
        {
          g_free (name);
          name =
            g_strdup_printf ("%s %d",
                             track->name,
                             count++);
        }
      region_set_name (
        region, name);
      g_message ("reigon name: %s", name);
      g_free (name);
    }

  int add_lane = 0, add_at = 0, add_chord = 0;
  switch (region->type)
    {
    case REGION_TYPE_MIDI:
      add_lane = 1;
      break;
    case REGION_TYPE_AUDIO:
      add_lane = 1;
      break;
    case REGION_TYPE_AUTOMATION:
      add_at = 1;
      break;
    case REGION_TYPE_CHORD:
      add_chord = 1;
      break;
    }

  if (add_lane)
    {
      track_lane_add_region (
        track->lanes[lane_pos],
        region);

      /* enable extra lane if necessary */
      if (lane_pos == track->num_lanes - 1)
        {
          track_add_lane (track);
        }
    }

  if (add_at)
    {
      automation_track_add_region (
        at, region);
    }

  if (add_chord)
    {
      g_warn_if_fail (track == P_CHORD_TRACK);
      array_double_size_if_full (
        track->chord_regions,
        track->num_chord_regions,
        track->chord_regions_size, Region *);
      array_append (track->chord_regions,
                    track->num_chord_regions,
                    region);
    }

  EVENTS_PUSH (ET_REGION_CREATED,
               region);
}

/**
 * Updates position in the tracklist and also
 * updates the information in the lanes.
 */
void
track_set_pos (
  Track * track,
  int     pos)
{
  track->pos = pos;

  for (int i = 0; i < track->num_lanes; i++)
    {
      track->lanes[i]->track_pos = pos;
    }
  automation_tracklist_update_track_pos (
    &track->automation_tracklist, track);

  /* update port identifier track positions */
  if (track->channel)
    {
      Channel * ch = track->channel;
      ch->stereo_out->l->identifier.track_pos = pos;
      ch->stereo_out->r->identifier.track_pos = pos;
      ch->stereo_in->l->identifier.track_pos = pos;
      ch->stereo_in->r->identifier.track_pos = pos;
      ch->midi_in->identifier.track_pos = pos;
      ch->piano_roll->identifier.track_pos = pos;
    }
}

/**
 * Only removes the region from the track.
 *
 * @pararm free Also free the Region.
 */
void
track_remove_region (
  Track * track,
  Region * region,
  int     free)
{
  region_disconnect (region);

  g_warn_if_fail (region->lane_pos >= 0);

  array_delete (
    track->lanes[region->lane_pos]->regions,
    track->lanes[region->lane_pos]->num_regions,
    region);

  if (free)
    free_later (region, region_free_all);

  EVENTS_PUSH (ET_REGION_REMOVED, track);
}

/**
 * Adds and connects a Modulator to the Track.
 */
void
track_add_modulator (
  Track * track,
  Modulator * modulator)
{
  array_double_size_if_full (
    track->modulators, track->num_modulators,
    track->modulators_size, Modulator *);
  array_append (track->modulators,
                track->num_modulators,
                modulator);

  mixer_recalc_graph (MIXER);

  EVENTS_PUSH (ET_MODULATOR_ADDED, modulator);
}

/**
 * Wrapper for each track type.
 */
void
track_free (Track * track)
{

  /* remove regions */
  /* FIXME move inside *_track_free */
  int i;
  for (i = 0; i < track->num_lanes; i++)
    track_lane_free (track->lanes[i]);

  /* remove automation points, curves, tracks,
   * lanes*/
  /* FIXME move inside *_track_free */
  automation_tracklist_free_members (
    &track->automation_tracklist);

#define _FREE_TRACK(type_caps,sc) \
  case TRACK_TYPE_##type_caps: \
    sc##_track_free (track); \
    break

  switch (track->type)
    {
      _FREE_TRACK (INSTRUMENT, instrument);
      _FREE_TRACK (MASTER, master);
      _FREE_TRACK (AUDIO, audio);
      _FREE_TRACK (CHORD, chord);
      _FREE_TRACK (BUS, bus);
      _FREE_TRACK (GROUP, group);
    default:
      /* TODO */
      break;
    }

#undef _FREE_TRACK

  channel_free (track->channel);

  if (track->widget && GTK_IS_WIDGET (track->widget))
    gtk_widget_destroy (
      GTK_WIDGET (track->widget));

  free (track);
}

/**
 * Returns the automation tracklist if the track type has one,
 * or NULL if it doesn't (like chord tracks).
 */
AutomationTracklist *
track_get_automation_tracklist (Track * track)
{
  switch (track->type)
    {
    case TRACK_TYPE_CHORD:
    case TRACK_TYPE_MARKER:
      break;
    case TRACK_TYPE_BUS:
    case TRACK_TYPE_GROUP:
    case TRACK_TYPE_INSTRUMENT:
    case TRACK_TYPE_AUDIO:
    case TRACK_TYPE_MASTER:
    case TRACK_TYPE_MIDI:
        {
          return &track->automation_tracklist;
        }
    default:
      g_warn_if_reached ();
      break;
    }

  return NULL;
}

/**
 * Wrapper for track types that have fader automatables.
 *
 * Otherwise returns NULL.
 */
Automatable *
track_get_fader_automatable (Track * track)
{
  switch (track->type)
    {
    case TRACK_TYPE_CHORD:
      break;
    case TRACK_TYPE_BUS:
    case TRACK_TYPE_GROUP:
    case TRACK_TYPE_AUDIO:
    case TRACK_TYPE_MASTER:
    case TRACK_TYPE_INSTRUMENT:
        {
          ChannelTrack * bt = (ChannelTrack *) track;
          return channel_get_fader_automatable (bt->channel);
        }
    default:
      break;
    }

  return NULL;
}

/**
 * Returns the channel of the track, if the track type has
 * a channel,
 * or NULL if it doesn't.
 */
Channel *
track_get_channel (Track * track)
{
  g_warn_if_fail (track);

  switch (track->type)
    {
    case TRACK_TYPE_MASTER:
    case TRACK_TYPE_INSTRUMENT:
    case TRACK_TYPE_AUDIO:
    case TRACK_TYPE_BUS:
    case TRACK_TYPE_GROUP:
    case TRACK_TYPE_MIDI:
      return track->channel;
    default:
      g_return_val_if_reached (NULL);
    }
}

/**
 * Returns the region at the given position, or NULL.
 */
Region *
track_get_region_at_pos (
  const Track *    track,
  const Position * pos)
{
  int i, j;

  if (track->type == TRACK_TYPE_INSTRUMENT ||
      track->type == TRACK_TYPE_AUDIO)
    {
      TrackLane * lane;
      Region * r;
      for (i = 0; i < track->num_lanes; i++)
        {
          lane = track->lanes[i];

          for (j = 0; j < lane->num_regions; j++)
            {
              r = lane->regions[j];
              if (position_is_after_or_equal (
                    pos, &r->start_pos) &&
                  position_is_before_or_equal (
                    pos, &r->end_pos))
                return r;
            }
        }
    }
  else if (track->type == TRACK_TYPE_CHORD)
    {
      Region * r;

      for (j = 0; j < track->num_chord_regions; j++)
        {
          r = track->chord_regions[j];
          if (position_is_after_or_equal (
                pos, &r->start_pos) &&
              position_is_before_or_equal (
                pos, &r->end_pos))
            return r;
        }
    }

  return NULL;
}

char *
track_stringize_type (
  TrackType type)
{
  switch (type)
    {
    case TRACK_TYPE_INSTRUMENT:
      return g_strdup (
        _("Instrument"));
    case TRACK_TYPE_AUDIO:
      return g_strdup (
        _("Audio"));
    case TRACK_TYPE_MIDI:
      return g_strdup (
        _("MIDI"));
    case TRACK_TYPE_BUS:
      return g_strdup (
        _("Bus"));
    case TRACK_TYPE_MASTER:
      return g_strdup (
        _("Master"));
    case TRACK_TYPE_CHORD:
      return g_strdup (
        _("Chord"));
    case TRACK_TYPE_GROUP:
      return g_strdup (
        _("Group"));
    case TRACK_TYPE_MARKER:
      return g_strdup (
        _("Group"));
    default:
      g_warn_if_reached ();
      return NULL;
    }
}

/**
 * Updates the frames of each position in each child
 * of the track recursively.
 */
void
track_update_frames (
  Track * self)
{
  int i;
  for (i = 0; i < self->num_lanes; i++)
    {
      track_lane_update_frames (self->lanes[i]);
    }
  for (i = 0; i < self->num_chord_regions; i++)
    {
      region_update_frames (
        self->chord_regions[i]);
    }
  for (i = 0; i < self->num_scales; i++)
    {
      scale_object_update_frames (
        self->scales[i]);
    }
  for (i = 0; i < self->num_markers; i++)
    {
      marker_update_frames (
        self->markers[i]);
    }

  automation_tracklist_update_frames (
    &self->automation_tracklist);
}

/**
 * Wrapper to get the track name.
 */
const char *
track_get_name (Track * track)
{
  return track->name;
}

/**
 * Setter for the track name.
 *
 * If the track name is duplicate, it discards the
 * new name.
 *
 * Must only be called from the GTK thread.
 */
void
track_set_name (
  Track * track,
  const char *  name)
{
  if (tracklist_track_name_is_unique (
        TRACKLIST, name))
    {
      if (track->name)
        g_free (track->name);
      track->name =
        g_strdup (name);

      EVENTS_PUSH (ET_TRACK_NAME_CHANGED,
                   track);
    }
}
