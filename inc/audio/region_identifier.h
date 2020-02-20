/*
 * Copyright (C) 2020 Alexandros Theodotou <alex at zrythm dot org>
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
 * Region identifier.
 *
 * This is in its own file to avoid recursive
 * inclusion.
 */

#ifndef __AUDIO_REGION_IDENTIFIER_H__
#define __AUDIO_REGION_IDENTIFIER_H__

#include "utils/yaml.h"

/**
 * @addtogroup audio
 *
 * @{
 */

/**
 * Type of Region.
 *
 * Bitfield instead of plain enum so multiple
 * values can be passed to some functions (eg to
 * collect all Regions of the given types in a
 * Track).
 */
typedef enum RegionType
{
  REGION_TYPE_MIDI = 1 << 0,
  REGION_TYPE_AUDIO = 1 << 1,
  REGION_TYPE_AUTOMATION = 1 << 2,
  REGION_TYPE_CHORD = 1 << 3,
} RegionType;

static const cyaml_bitdef_t
region_type_bitvals[] =
{
  { .name = "midi", .offset =  0, .bits =  1 },
  { .name = "audio", .offset =  1, .bits =  1 },
  { .name = "automation", .offset = 2, .bits = 1 },
  { .name = "chord", .offset = 3, .bits = 1 },
};

/**
 * Index/identifier for a Region, so we can
 * get Region objects quickly with it without
 * searching by name.
 */
typedef struct RegionIdentifier
{
  RegionType type;

  int        track_pos;
  int        lane_pos;

  /** Automation track index in the automation
   * tracklist, if automation region. */
  int        at_idx;

  /** Index inside lane or automation track. */
  int        idx;
} RegionIdentifier;

static const cyaml_schema_field_t
region_identifier_fields_schema[] =
{
  CYAML_FIELD_BITFIELD (
    "type", CYAML_FLAG_DEFAULT,
    RegionIdentifier, type, region_type_bitvals,
    CYAML_ARRAY_LEN (region_type_bitvals)),
  CYAML_FIELD_INT (
    "track_pos", CYAML_FLAG_DEFAULT,
    RegionIdentifier, track_pos),
  CYAML_FIELD_INT (
    "lane_pos", CYAML_FLAG_DEFAULT,
    RegionIdentifier, lane_pos),
  CYAML_FIELD_INT (
    "at_idx", CYAML_FLAG_DEFAULT,
    RegionIdentifier, at_idx),
  CYAML_FIELD_INT (
    "idx", CYAML_FLAG_DEFAULT,
    RegionIdentifier, idx),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
region_identifier_schema = {
  CYAML_VALUE_MAPPING (CYAML_FLAG_POINTER,
    RegionIdentifier,
    region_identifier_fields_schema),
};

static inline int
region_identifier_is_equal (
  const RegionIdentifier * a,
  const RegionIdentifier * b)
{
  return
    a->idx == b->idx &&
    a->track_pos == b->track_pos &&
    a->lane_pos == b->lane_pos &&
    a->at_idx == b->at_idx &&
    a->type == b->type;
}

static inline void
region_identifier_copy (
  RegionIdentifier * dest,
  const RegionIdentifier * src)
{
  dest->idx = src->idx;
  dest->track_pos = src->track_pos;
  dest->lane_pos = src->lane_pos;
  dest->at_idx = src->at_idx;
  dest->type = src->type;
}

static inline const char *
region_identifier_get_region_type_name (
  RegionType type)
{
  switch (type)
    {
    case REGION_TYPE_MIDI:
      return
        region_type_bitvals[0].name;
    case REGION_TYPE_AUDIO:
      return
        region_type_bitvals[1].name;
    case REGION_TYPE_AUTOMATION:
      return
        region_type_bitvals[2].name;
    case REGION_TYPE_CHORD:
      return
        region_type_bitvals[3].name;
    default:
      g_warn_if_reached ();
      break;
    }
  g_return_val_if_reached (NULL);
}

static inline void
region_identifier_print (
  const RegionIdentifier * self)
{
  g_message (
    "Region identifier: "
    "type: %s, track pos %d, lane pos %d, "
    "at index %d, index %d",
    region_identifier_get_region_type_name (
      self->type),
    self->track_pos, self->lane_pos, self->at_idx,
    self->idx);
}

/**
 * @}
 */

#endif
