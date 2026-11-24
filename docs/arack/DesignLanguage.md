# ARAck design language

Taken from Magnus's **Musical Reference Chart** (github.com/miuglitch/musicalrefchart,
`chart.html` and its README §6 / CLAUDE.md "Style"), and translated for a JUCE app/plug-in.
Anything marked **(new)** does not exist in the chart and is a proposal in the same spirit.

---

## 1. The idea in one paragraph

Teenage Engineering K.O. II hardware look. A **black display strip** across the top with
big, light-weight white readouts, sitting over a **light warm-grey body** (black in dark
mode). **Dark grey frames** separate areas. Colour is used sparingly as **signal accents**:
orange, red, blue and dark cyan. Type is **Inter Display** at several weights with
**tabular numerals**. Clean contrast, flat surfaces, **no glow, no gradients, no
skeuomorphic shading**. It should feel like studio gear, not software.

---

## 2. Colour tokens

### Fixed (same in both themes)

| Token | Hex | Use |
|---|---|---|
| `orange` | `#ff5a1f` | Primary accent. Tempo readout, focus ring, primary button, "selected", triplets |
| `red` | `#e0322b` | Secondary accent. Warnings, clip, record-arm **(new)** |
| `blue` | `#3a86d4` | Frequency / rate values, reference markers, sharps |
| `cyan` | `#0f8a94` | Dark cyan. Time/cycle values, dotted divisions |
| `frame` | `#3b3b3b` | Dark grey frames: header underline (6 px), dividers between panels (6 px) |
| `display` | `#0f0f0f` | The black display strip (header) |
| `displayText` | `#f4f4f2` | Text on the display |
| `displayLabel` | `#8a8a86` | Small caps labels and units on the display |
| `displayDivider` | `#262626` | 1 px vertical separators between header blocks (`#2e2e2e` next to the logo) |
| `displayControl` | `#1d1d1d` | Dropdown background on the display |
| `logo` | `#c9c7c2` | Miu logo on the display |

### Themed (body)

| Token | Light | Dark | Use |
|---|---|---|---|
| `bg` | `#dad8d3` | `#0d0d0d` | Body background |
| `fg` | `#1a1a1a` | `#f2f2f0` | Body text |
| `panelHeaderBg` | `#c9c7c2` | `#0d0d0d` | Table/panel header rows |
| `panelHeaderText` | `#141414` | `#ffffff` | Panel header text |
| `panelHeaderSub` | `#5c5b58` | `#7c7c78` | Second line / units in a header |
| `panelHeaderLine` | `#141414` | `#3b3b3b` | 3 px rule under a header row |
| `rowLine` | `rgba(0,0,0,.07)` | `#1e1e1e` | 1 px rule between rows |
| `hover` | `rgba(255,255,255,.45)` | `#232323` | Row / item hover |
| `bandA` / `bandB` | `#e3e1dc` / `#dad8d3` | `#121212` / `#0d0d0d` | Alternating bands (lanes, rows, groups) |
| `highlightWarm` | `#ffd9c9` | `#3a1a0e` | Tinted row for an orange-marked item |
| `highlightCool` | `#cfe0f3` | `#0f2236` | Tinted row for a blue-marked item |
| `groupLine` | `#9d9b96` | `#3b3b3b` | 2 px heavier rule at a group start |
| `muted` | `#a5a39e` | `#3f3f3d` | Disabled / N/A values |
| `ctrlBg` | `#c9c7c2` | `#0d0d0d` | Side-strip / control panel background |
| `buttonBorder` | `#b3b1ac` | `#3b3b3b` | Toggle button border |
| `buttonBg` | `#eceae6` | transparent | Toggle button off |
| `buttonText` | `#141414` | `#8c8c88` | Toggle button text off |
| `scrollThumb` | `#a9a7a2` | `#3b3b3b` | Scrollbar thumb (9 px, round 5 px) |

### Value text colours

On light grey the accents go **darker**, on black they go **brighter**, so text stays
readable. Use these for coloured numbers and labels, never the raw accent.

| Accent | On light | On dark |
|---|---|---|
| orange | `#d8460f` | `#ff7a45` |
| red | `#c0261f` | `#ff5a52` |
| blue | `#2364ad` (sharps `#2a6fc0`) | `#62a6ee` (sharps `#5aa2ea`) |
| cyan | `#0b6f78` | `#20b9c4` (underline `#13a6b2`) |

### Colour rules

- **Headers are neutral; colour lives in the 3 px underline.** A column/panel header's
  text is near-black (white in dark mode); its accent shows only in the rule beneath it.
- One meaning per colour across the app. Keep the chart's mapping where it applies:
  orange = tempo/time-in-ms and "active", blue = frequency/rate, cyan = cycle/secondary
  timing, red = samples/danger.
- Every new colour is added to both themes at once.

---

## 3. Typography

- **Family:** Inter Display (SIL OFL). The chart embeds it as a private family so an
  installed Inter can't interfere. In JUCE: embed the `.ttf`/`.otf` files (not woff2,
  which JUCE can't read) as BinaryData and load with `Typeface::createSystemTypefaceFor`.
  Weights needed: 300, 400, 600, 700 (500 and 800 optional).
- **Numbers always tabular** (`tnum`) so columns and changing readouts don't jitter.
  Inter's default figures are proportional, so use a font build with tabular figures
  as the default (pyftsubset/fonttools can freeze `tnum`) because JUCE can't switch
  OpenType features at runtime.
- Every displayed number is formatted to a fixed number of decimals for its kind.
  Never print a raw float.

| Style | Size | Weight | Tracking | Case | Use |
|---|---|---|---|---|---|
| Readout XL | 34 | 300 | −1% | | Big numbers on the display (tempo, rates) |
| Readout L | 24 | 300 | | | Song / session name on the display |
| Body value | 18 | 400 | | | Values in tables and lists |
| Body | 17 | 400 | | | General text, dropdowns (600) |
| Logo sub | 14 | 600 | +12% | CAPS | Product name under the logo, orange |
| Button | 13 | 600 | +8% | CAPS | All buttons, centred |
| Header label | 12 | 600 | +7% | CAPS | Panel / column header |
| Header sub | 12 | 400 | | | Units under a header label |
| Display label | 11 | 600 | +9% | CAPS | Field labels on the black display |
| Badge | 10 | 700 | +5% | CAPS | Small marker chips |

Sizes are CSS px; at 100% scaling treat them as JUCE points 1:1.

---

## 4. Spacing, shape and layout

- **Corner radius:** 6 for buttons and inputs, 3 for badges, 5 for scrollbar thumbs.
  Panels and frames are square.
- **Frames:** 6 px `frame` bar under the header and between major panels. 3 px rule under
  header rows, 2 px at group starts, 1 px between rows and between header blocks.
- **Header strip:** 16 px vertical padding; blocks separated by 1 px dividers with 16 px
  side padding; label above value with a 6 px gap.
- **Panel header rows:** fixed 60 px so neighbouring panels line up.
- **Rows:** 6 px vertical / 18 px horizontal padding.
- **Side strip:** 10 px padding, 6 px gap between buttons, 22 px gap between groups.
  Buttons 8 × 14 px padding.
- **Content is centred as a group**, with generous gutters (the chart uses 80% width).
  Numbers right-aligned so decimals line up; names centred or left.
- Compact mode exists for narrow widths (chart: phones ≤ 700 px). For ARAck this maps
  to a small plug-in window size **(new)**.

---

## 5. Components

| Component | Look | JUCE mapping |
|---|---|---|
| **Display header** | Black strip, logo block at left (Miu logo + orange product name), then label-over-value blocks split by 1 px dividers, 6 px frame underneath | Custom `Component` drawn with `display` colours; same in both themes |
| **Readout** | Big 34/300 number, transparent until hover (1 px `#2e2e2e` border), orange focus ring. Tempo-style **drag to change**: click-drag up/down, 1 unit per 4 px, Shift = fine, plain click to type | `Slider` (`LinearBarVertical` drag, text box only) or a custom `Label` + mouse drag. `LookAndFeel::drawLabel` / `createSliderTextBox` |
| **Dropdown on display** | `#1d1d1d` fill, 17/600 text, small grey down-triangle at right, radius 6 | `ComboBox`, `drawComboBox`; popup menu light (white bg, dark text) as in the chart |
| **Toggle button** | Caps 13/600, radius 6. Off: light fill + grey border (dark: transparent + grey border, grey text). On: solid black (dark: white text + light border). Accent toggles turn cyan/orange when on (dark: coloured text + border) | `ToggleButton`/`TextButton` with `setClickingTogglesState(true)`; `drawButtonBackground` + `drawButtonText` |
| **Primary action** | Solid orange, white text (dark mode: orange outline + orange text, fills on hover) | `TextButton` with a `primary` property read by the LookAndFeel |
| **Panel header** | 60 px, `panelHeaderBg`, caps label + sub line, 3 px coloured underline | `TableHeaderComponent` via `drawTableHeaderBackground` / `drawTableHeaderColumn`, or a custom header |
| **List / table** | Banded rows, 1 px row lines, hover tint, tinted highlight rows, heavier rule at group start, coloured values | `TableListBox` / `ListBox` with custom `paintRowBackground` and `paintCell` |
| **Badge** | Tiny caps chip, white text on orange or blue, radius 3 | Small custom painter |
| **Scrollbar** | 9 px, round thumb, track = background | `drawScrollbar` |
| **Knob (new)** | Flat circle in `buttonBg`, 2 px `frame` ring, orange value arc, thin white pointer on dark. No shadows | `drawRotarySlider` |
| **Meter (new)** | Flat black display segment, value in orange, peak/clip in red, no glow | Custom component |
| **Automation lane (new)** | Banded lanes like the octave bands, curve in the lane's accent, points as small squares | Custom component |

---

## 6. Themes

- Two themes, **light (default)** and **dark**. The display header is identical in both;
  only the body tokens change.
- Remember the choice (`ApplicationProperties` in the app; plug-in state or a shared
  settings file in the plug-in).
- Swap themes by swapping the token set and calling `sendLookAndFeelChange()` so every
  component repaints. No per-component colour code.

---

## 7. JUCE implementation plan

1. **`Theme` struct** (`Source/UI/Theme.h`): all tokens above as `juce::Colour` members,
   with `static Theme light()` and `static Theme dark()`. One place to add colours.
2. **`ARAckLookAndFeel : LookAndFeel_V4`** (`Source/UI/ARAckLookAndFeel.h/.cpp`): holds a
   `Theme`, maps it to JUCE colour IDs (`ResizableWindow::backgroundColourId`,
   `TextButton::*`, `ComboBox::*`, `PopupMenu::*`, `Slider::*`, `ListBox::*`, scrollbar…)
   and overrides the draw methods listed in §5. `setTheme(Theme)` reapplies colours.
3. **Fonts:** add Inter Display `.ttf` files to `Resources/Fonts`, bundle with
   `juce_add_binary_data`, return them from `getTypefaceForFont` and set as default
   sans-serif. A small `Fonts` helper gives the type styles from §3 by name.
4. **Shared widgets:** `DisplayHeader`, `Readout` (drag-to-edit number), `StripButton`,
   `PanelHeader`, `Badge`. Build these once and reuse in both the standalone app and the
   plug-in editor.
5. **Logo:** `Resources/miu-logo.svg` (already extracted from the chart), loaded with
   `Drawable::createFromSVG` and tinted with `logo` colour via `replaceColour`.
6. **Check** each screen in light and dark, at normal and compact sizes, with screenshots.

---

## 8. Rules to keep

- No glow, no gradients, no drop shadows, no bevels.
- Capital letters with tracking for labels and buttons; sentence case for values and names.
- Colour means something; neutral by default.
- Tabular figures everywhere numbers change.
- Same header in both themes.
- When in doubt, look at `chart.html` and the mockups in
  `D:\Dropbox\Claude\Musical Reference Chart\mockups` (design-1…5, mobile-1…4).
