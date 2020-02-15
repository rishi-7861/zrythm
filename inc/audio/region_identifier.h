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
  REGION_TYPE_MIDI = 0x01,
  REGION_TYPE_AUDIO = 0x02,
  REGION_TYPE_AUTOMATION = 0x04,
  REGION_TYPE_CHORD = 0x08,
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
  dest->type = src->type;
}

/**
 * @}
 */

#endif
