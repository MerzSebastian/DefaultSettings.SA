# Game settings: addresses and stock defaults (v1.0 US)

A reference of the user-adjustable settings whose default the game restores in its "reset to defaults" code (`SetDefaultPreferences`,
`0x573AE0`, called with page 3 = audio, 4 and 27 = display, 36 = controller). **Only the two mouse settings marked "plugin" are exposed by
DefaultSettings.SA today.** The rest is listed so more options can be added later.

- **Menu object:** `FrontEndMenuManager` at `0xBA6748`. "Menu +N" means address `0xBA6748 + N`.
- **Stock value:** what the game's reset code writes (read from the game executable, so it is the real stock default).
- **Names** of the fields come from community documentation of the game and may differ from other sources.
- **Saved** = written to `gta_sa.set`, so it is only a "default" until the user changes it.
- **Writing the byte may not be enough** where the table says "needs a call": the game applies some values through a function.

## Mouse and controls (page 36)

| Setting | Address | Type | Stock | Saved | Notes |
|---|---|---|---|---|---|
| **Mouse sensitivity (horizontal)** | `0xB6EC1C` | float | `0.0025` (50 %) | yes | **plugin: `[Mouse] Sensitivity`**. The menu slider shows `value / 0.005`; its handler clamps to `[1/3200, 1/200]` |
| Mouse sensitivity (vertical) | `0xB6EC18` | float | set once at game start (about `0.00075`) | no | not changed by the menu slider, not reset by the defaults code |
| **Invert Mouse Y** | `0xBA6745` | byte | `0` | yes | **plugin: `[Mouse] InvertY`**. The menu shows the *negated* byte: 0 = "On" |
| Invert Mouse X | `0xBA6744` | byte | not written by the reset | ? | not touched by the defaults code |
| Mouse steering (vehicles) | `0xC1CC02` | byte | `0` | ? | menu entry "Mouse Steering" (action 59) |
| Mouse flying (aircraft) | `0xC1CC03` | byte | `1` | ? | menu entry "Mouse Flying" (action 60) |
| Mouse look on foot (3rd person) | `0xB6EC2E` | byte | `1` | ? | |
| Control method | Menu +0xD0 (`0xBA6818`) | byte | `0` (mouse + keys) | yes | |
| Pad: invert X1 / Y1 / X2 / Y2, swap axis 1 / 2 | Menu +0xB1 .. +0xB6 (`0xBA67F9` .. `0xBA67FE`) | 6 bytes | `0` | yes | |

## Display and graphics (pages 4 and 27, the same reset code)

| Setting | Address | Type | Stock | Notes |
|---|---|---|---|---|
| Frame limiter (On/Off) | Menu +0x4C (`0xBA6794`) | byte | `1` (On) | menu shows the byte as is. The numeric cap is `RsGlobal`'s frame limit, set by code and not saved |
| Brightness | Menu +0x3C (`0xBA6784`) | int | `0x100` (256) | needs a call: the reset also calls a brightness function (`0x747200`) |
| Draw distance | Menu +0x40 (`0xBA6788`) | float | `1.2` | the reset also writes `1.2` to the LOD scale at `0x8CD800` |
| HUD | Menu +0x21 (`0xBA6769`) | byte | `1` | |
| Radar mode | Menu +0x24 (`0xBA676C`) | int | `0` | |
| Map legend | Menu +0x4A (`0xBA6792`) | byte | `0` | |
| Widescreen | Menu +0x4B (`0xBA6793`) | byte | `0` | |
| Subtitles | Menu +0x44 (`0xBA678C`) | byte | `0` in this exe | **unresolved:** other documentation says the default is on |
| Radar blips (5 kinds) | Menu +0x45 .. +0x49 (`0xBA678D` .. `0xBA6791`) | 5 bytes | `1` each | |
| MIP mapping | Menu +0xC4 (`0xBA680C`) | byte | `1` | |
| Anti-aliasing (prefs / current) | Menu +0xC8, +0xCC (`0xBA6810`, `0xBA6814`) | int | `1` each | changing it needs a video mode restart |
| Save photos | Menu +0xE8 (`0xBA6830`) | byte | `1` | |
| Effects quality | call `0x49EA40` on `0xA9AE00` | function | `2` | needs a call |

## Audio (page 3)

| Setting | Address | Type | Stock | Notes |
|---|---|---|---|---|
| Radio auto-select | Menu +0x4D (`0xBA6795`) | byte | `1` | needs a call to the audio engine to take effect |
| Audio output mode | Menu +0x4E (`0xBA6796`) | byte | `1` | |
| SFX volume | Menu +0x4F (`0xBA6797`) | byte | `64` (`0x40`) | needs a call: the reset passes it to the audio engine |
| Radio / music volume | Menu +0x50 (`0xBA6798`) | byte | `64` (`0x40`) | needs a call, as above |
| Radio EQ | Menu +0x51 (`0xBA6799`) | byte | `1` | needs a call |
| Radio mode | Menu +0xB0 (`0xBA67F8`) | byte | `0` | |
| User tracks auto-scan | Menu +0xC5 (`0xBA680D`) | byte | `0` | |

The audio engine object used by those calls is at `0xB6BC90`.

## How a new option would be added

1. A rule kind in `src/ini.h` (parser) and an apply function in `src/main.cpp` (read, write, log before/after).
2. Apply it at the same moments as the others: startup without `gta_sa.set`, and after the matching `SetDefaultPreferences` page (the plugin
   already hooks the restore-defaults call; page 36 is wired for the mouse, pages 3, 4 and 27 would be added the same way).
3. Settings marked "needs a call" must be applied through the game's function, not by writing the byte, or the live value would not change.
4. Add a commented line with the stock value to the INI generator (`tools\selftest.cpp`), tests in `tools\selftest.cpp` and the emulated game
   in `tools\hooktest`.
