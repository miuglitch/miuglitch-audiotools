# ARAck requirements

Collected from Magnus, November 2026. Add to this as decisions are made.

## Product

- ARAck is an **ARA2 plug-in** (VST3) that hosts other plug-ins and adds utility processing
  and automation.
- **Main target host:** Ableton Live 13, once it ships with ARA2 support (expected early 2027).
- **Must work everywhere:** other ARA2 hosts (Nuendo is Magnus's main host today) and also as a
  plain, non-ARA VST3 in hosts without ARA.

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
