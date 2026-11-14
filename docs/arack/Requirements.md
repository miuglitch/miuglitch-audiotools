# ARAck requirements

Collected from Magnus, November 2026. Add to this as decisions are made.

## Product

- ARAck is an **ARA2 plug-in** (VST3) that hosts other plug-ins and adds utility processing
  and automation.
- **Main target host:** Ableton Live 13, once it ships with ARA2 support (expected early 2027).
- **Must work everywhere:** other ARA2 hosts (Nuendo is Magnus's main host today) and also as a
  plain, non-ARA VST3 in hosts without ARA.

## Roles

- **Nuendo is the host, ARAck is the ARA2 plug-in.** Testing is done with Nuendo hosting
  ARAck (later Ableton Live 13 too).
- The CHAIN slots host **regular VST3 plug-ins**. ARAck itself provides the ARA side; it does
  not need to host other ARA plug-ins (such as Melodyne).

## Interface

- **Left:** a list of **12 plug-in slots**. Each slot has an **enable/disable button to the left
  of the plug-in name**.
- **Disable means fully off:** a disabled plug-in is taken out of the processing chain, so it
  adds **no latency** and no CPU. ARAck reports the new total latency to the host when a slot
  is switched.
- **Right of the slot list:** a panel that shows the selected plug-in's (or built-in process's)
  interface.
- **Underneath:** a wide automation area with a lane for **every parameter that's been clicked**.
- Look and feel: see `DesignLanguage.md`. Mockups: `Mockups/arack-1…5`.

### Chosen layout: mockup 4 (Compact), revised as `Mockups/arack-4b-compact`

Header:
- No tempo readout. Instead, the **total clip length in milliseconds** (the ARA region ARAck
  is working on).
- No total-latency readout. Instead, the **name of the audio file** the ARA plug-in is working on.
- Undo and Redo as arrow icons, and a cog icon on the right for settings later.
- No "active slots" count and no chain-preset button in the header.

Chain list (left):
- The panel is titled **CHAIN** (all caps).
- **Drag handles** (three small lines) at the far left of each slot show that slots can be
  dragged to reorder.
- A single **vertical divider line** between the plug-in name and the latency value.
- A **LATENCY** button in the panel header toggles the latency column on and off.

Automation (bottom):
- **Tabs are parameters.** Each tab has a checkbox to show or hide that parameter; shown
  parameters are **overlaid** in one view (a Stack mode can show them as separate lanes).
- The **selected** parameter is drawn at full opacity and is the one you edit; the other shown
  parameters are faded.
- The clip's **waveform is drawn underneath** the automation curves.
- A **ruler along the bottom** shows a grid in **milliseconds**.
- **Hovering** shows a vertical line and a small box with the time position in the clip in
  milliseconds (and the value of the active parameter at that point).

## Build output

- Every plug-in build is installed straight to `C:\Program Files\Common Files\VST3\ARAck\`
  (as `ARAck.vst3`), so hosts pick up the newest build on rescan. This uses JUCE's
  `COPY_PLUGIN_AFTER_BUILD` with `VST3_COPY_DIR`. On HADES the VST3 folder already gives
  normal users write access, so no admin rights or terminal steps are needed.
- A copy also goes to `Bin\<Config>\` in the Dropbox ARAck folder, as before.
- Intermediate build files stay in `C:\Builds\ARAck`.

## Undo / redo

ARAck needs **full undo and redo** for everything the user does in it, built into the
architecture from the start rather than added later.

What must be undoable:
- Loading, removing, replacing and reordering plug-ins in slots.
- Enabling and disabling slots.
- Parameter changes, on ARAck's own processes and on hosted plug-ins. A knob drag is one undo
  step, not hundreds.
- Automation edits: adding and removing lanes, adding, moving and deleting points, changing curves.
- Preset and chain loads (one step each).

How it will be built:
- All of ARAck's document state lives in one `juce::ValueTree`, and every change goes through a
  single `juce::UndoManager`. Nothing changes state directly.
- Each user action starts a new transaction (`beginNewTransaction`) with a readable name, so the
  UI can show "Undo: Disable slot 4" and similar.
- Continuous gestures (dragging a knob, moving an automation point) are grouped into a single
  transaction between gesture start and end.
- Hosted plug-in parameter changes are captured through their parameter listeners and gesture
  callbacks. For plug-in changes that don't come through parameters (internal editor actions,
  preset browsing inside the plug-in), ARAck snapshots the plug-in's state before and after and
  stores that as the undo step.
- Undo history is per ARAck instance and is not saved with the project. The host's own undo is
  left alone. ARA document edits made by the host stay with the host.
- Keyboard shortcuts Ctrl+Z / Ctrl+Shift+Z (Cmd on Mac) when the ARAck window has focus, plus
  Undo and Redo buttons in the header.

## Prototype status (v0.1, 15 Nov 2026)

Built:
- ARA2 VST3 plug-in. In an ARA host it plays the clip's audio through the chain; as a plain
  VST3 it processes the track input.
- 12 CHAIN slots: load VST3s (scan once from the slot menu), enable/disable, drag to reorder,
  right-click to replace/remove. Disabled slots are not processed and not counted in latency;
  the host is told the new total latency.
- The selected plug-in's own window in the middle panel, or floating.
- Header: clip length in ms, ARA file name, undo/redo, settings (placeholder).
- Automation: touching a control in a hosted plug-in adds a lane; tabs with on/off checkbox;
  lanes overlaid on the waveform; selected lane full opacity and editable (click to add,
  drag to move, double-click to delete); ms ruler; hover time box; playback follows the clip.
- Undo/redo for loading, removing, enabling, moving, parameter changes (one step per knob drag)
  and every automation edit.

Not yet:
- Built-in ARAck utility processes.
- Undo for plug-in changes that don't go through parameters (e.g. browsing presets inside the
  plug-in's own window).
- Sample-rate conversion: the clip plays only if its sample rate and channel count match the
  project.
- Inter Display font (needs .ttf files); Segoe UI is used meanwhile.
- Plug-in scanning happens inside the host; a crashing plug-in during scan is remembered and
  skipped next time.
