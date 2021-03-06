.. This is part of the Zrythm Manual.
   Copyright (C) 2019-2020 Alexandros Theodotou <alex at zrythm dot org>
   See the file index.rst for copying conditions.

.. _track-inspector:

Track Inspector
===============

Each track will display its page in the inspector when
selected. Depending on the track, the properties shown
will be different.

.. image:: /_static/img/track-inspector.png
   :align: center

Track Properties
----------------

Track properties are basic properties that the track
has.

.. image:: /_static/img/track-properties.png
   :align: center

Track Name
  Name of the track
Direct Out
  The track that this track routes its output to.
Instrument
  If the track is an :ref:`instrument-track`, the instrument
  plugin for this track.

.. _track-inputs:

Inputs
------

If the track takes input, there will be an input
selection section.

.. image:: /_static/img/track-inputs.png
   :align: center

MIDI FX/Inserts
---------------

These are slots for dropping audio or MIDI effects that will
be applied to the signal as it passes through the track.

.. image:: /_static/img/midi-fx-inserts.png
   :align: center

MIDI FX are processed after the input and piano roll events
and before the instruments in instrument tracks, or
before the inserts in MIDI tracks.

.. note:: Only MIDI and Instrument tracks support MIDI FX.

Inserts are processed in order. For instrument tracks,
the inserts will be added onto the signal coming from
the instrument, and for other tracks they will be added
on the incoming signal.

.. _track-sends:

Aux Sends
---------

These are
`aux sends <https://en.wikipedia.org/wiki/Aux-send>`_ to
other tracks or plugin
side-chain inputs. These are generally useful for
side-chaining or applying additional effects to
channels, such as reverb.

.. image:: /_static/img/track-sends.png
   :align: center

The first 6 slots are for pre-fader sends and the
last 3 slots are for post-fader sends.
The pre-fader slots will send the signal before
the fader is processed, and the post-fader slots
will send the signal after the fader is applied.

Fader
-----
Fader section to control the volume and stereo balance.

.. image:: /_static/img/track-fader.png
   :align: center

To change the fader or stereo balance amount, click and
drag their respective widgets. You can reset them to their
default positions by right clicking and selecting
:zbutton:`Reset`.

The meter displays the amplitude of the signal in dBFS as
it is processed live. The following values are displayed
next to the meter for additional reference.

Peak
  Peak signal value.
RMS
  Root Mean Square of the signal value.

The following controls are available for controlling the
signal flow.

Record
  Arm the track for recording.
Mute
  Mutes the track, meaning no sound will be sent to its
  direct out.

.. note:: The track will still be processed, so if you
  are looking to decrease DSP usage, try disabling
  plugins instead.

Solo
  Soloes the track. If any track is soloed, only the soloed
  tracks will produce sounds.
Listen
  Similar to solo, except it dims the volume of other tracks
  instead of muting them. The dim amount can be controlled
  in the control room described in :ref:`control-room`.
  Not operational at the moment.
Channel Settings
  Not operational at the moment.

Comments
--------
User comments.

.. image:: /_static/img/track-comments.png
   :align: center

.. note:: This is a TODO feature.
