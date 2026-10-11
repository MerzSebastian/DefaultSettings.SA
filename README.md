# DefaultSettings.SA

An ASI plugin for **GTA San Andreas v1.0 US** (32-bit, compact or Hoodlum exe) that lets you change the game's **default** controls and
mouse, display and audio settings with a simple INI file, **without ever overriding the choices you make yourself**.

## Why this exists

This is a personal tool, shared as it is. The goal: **keep the game files untouched** (no edited `gta_sa.exe`, no replaced data files)
and still get **modern defaults whenever the game runs on a new machine**, where it would otherwise start with its 2004 settings
(jump on Left Shift, inverted mouse-look, ...). The plugin applies the INI only when the game is on its stock defaults, so a machine you
already set up is never touched, and removing the plugin leaves nothing behind.

## Requirements

- GTA San Andreas **1.0 US** (32-bit, compact or Hoodlum exe). Other versions are detected and left alone.
- An **ASI loader** that loads `.asi` files from `scripts\` (for example the Ultimate ASI Loader). It is not included.

## Default settings (as shipped)

The shipped `DefaultSettings.SA.ini` sets these defaults. Some apply to everybody, some only to one **keyboard layout**
(the layout, not the game language, decides). Everything else stays at the game's stock value.

**For every keyboard**

| Setting | Stock | Shipped default | Why |
|---|---|---|---|
| Sprint | Space | **Left Shift** | Modern games sprint with Left Shift |
| Jump | Left Shift | **Space** | Modern games jump with Space |
| Invert Mouse Y (menu entry) | On | **Off** | Modern games do not invert the vertical look by default |

**Only on some keyboards**

| Your keyboard | Extra defaults |
|---|---|
| **US / UK English (QWERTY)**, and any layout not listed below | none: only the table above |
| **QWERTZ** (German, Austrian, Swiss, Czech, Slovak, Hungarian, Slovenian, Croatian) | **Conversation Yes** on **Y** (a German-language game puts it on Z, which sits in the wrong place on a QWERTZ keyboard) |
| **AZERTY** (French, Belgian) | **ZQSD** for moving (as in the game's own French version) instead of the scattered WASD letters: Z forward and Q left as second keys on foot, Q steer left and Z accelerate in vehicles, plus the keys that would clash with them (previous weapon on A, look left on A, sniper zoom out on W, submissions on the é key) |

**Mouse sensitivity is not part of the shipped defaults, but it is recommended to lower it.** Modern mice have a much higher DPI than the
mice the game was made for, so the stock 50 % is far too fast. Lower it (for example `Sensitivity=15` in the `[Mouse]` section) **only
if you have a mod that fixes the vertical sensitivity, such as SilentPatch**: in the plain game the vertical sensitivity is always 25 % of
the horizontal one, so a low slider value makes the vertical look very slow. That is why this plugin does not set it for you.

Every line is a plain rule in the INI, so you can change or remove any of it, and add rules for any other control.

## Install / uninstall

- **Install:** unpack the zip into the game folder so that `DefaultSettings.SA.asi` and `DefaultSettings.SA.ini` end up together in `scripts` (or next to `gta_sa.exe`).
- **Uninstall:** delete the `.asi`, `.ini` and `.log`. Anything the game already saved into your `gta_sa.set` stays as it is.

## When are the settings applied?

The INI is applied **only when the game is on its stock defaults**, so it never fights your own settings:

| Situation | What happens |
|---|---|
| **First start without a `gta_sa.set`** (new PC, or you deleted the file) | All INI rules are applied. The game then saves them into its own `gta_sa.set`, so from then on they are your settings. |
| **Start with an existing `gta_sa.set`** | Nothing is applied. Your saved settings win, even if the INI says something else. |
| **Options > Controller Setup > restore defaults** | The mouse/gamepad rules are applied right after the game resets them, and the key rules right after it resets the controls (two steps in the game). The game then saves again, so the INI values are your new defaults. |
| **Display Setup / Audio Setup > restore defaults** | The rules of that page's section (`[Display]` or `[Audio]`) are applied right after the reset. |

The plugin itself never reads or edits `gta_sa.set`. There is no marker file and no saved state.

## Editing the INI

Every option is listed with its stock value, commented out with `;`, so you only uncomment and change what you want. Active rules are kept
with their stock line right below them (commented out). To get the stock value back, **comment out the active line**.

```ini
[Mouse]
InvertY=0                  ; the menu entry "Invert Mouse Y": 1 = On, 0 = Off (stock: On)
Sensitivity=70             ; the menu slider "Mouse Sensitivity" in percent, 0-100 (stock: 50)

[Memory]
Frame limit 60 fps = 0xBA6794 u8 4 When=DISPLAY   ; Widescreen Fix only: its fps selector index (4 = 60 fps)

[Keys]
PED_SPRINT=LSHIFT          ; slot 0, the main key
PED_JUMPING=SPACE
PED_JUMPING.Slot1=NONE     ; slot 1, the "optional extra key"; NONE = leave it unbound
CONVERSATION_YES=Y OnlyIfLayout=QWERTZ
PED_DUCK=LSHIFT ClearDuplicates
```

- **Keys:** a character (`A`, `5`, `[`) or a name: `SPACE`, `LSHIFT`, `RCTRL`, `UP`, `F5`, `PADENTER`, ... (the full list is in the INI header).
  A number 32-255 or 1000-1056 is taken as a raw key code. Keys are the *characters* the key types on your layout, so `Y` is always the key
  labeled Y. A character you cannot type without Shift/AltGr on your layout cannot be pressed in the game.
- **Actions:** one commented line per action and slot is in the INI (`PED_JUMPING`, `GO_FORWARD`, `VEHICLE_HORN`, ...).
- **`OnlyIfLayout=QWERTZ,QWERTY,AZERTY,ANY`:** apply a rule only on these layouts (a list, separated by `,`, no spaces). The layout is detected from
  the keyboard: QWERTZ = the `Y` key at scancode `0x2C`, AZERTY = the `A` key at `0x10`, QWERTY = `Y` at `0x15` and `A` at `0x1E`. Any other layout
  only gets the rules without `OnlyIfLayout`.
- **`ClearDuplicates`:** also unbinds that key from other actions in the same slot (never from slots another rule of your INI sets). Without it, a
  key used twice is only reported with a `NOTE:` in the log.
- **Combining rules:** rules run in file order and every rule whose conditions match is applied, so for the same action and slot the *later*
  matching line wins. Put the general line first and the exceptions after it:
  ```ini
  VEHICLE_LOOKLEFT=X                        ; every layout
  VEHICLE_LOOKLEFT=A OnlyIfLayout=AZERTY    ; AZERTY: replaces the X
  PED_DUCK=C OnlyIfLayout=QWERTZ,QWERTY     ; several layouts in one rule
  PED_DUCK.Slot1=NONE                       ; slots are independent
  ```
  (In the opposite order the general line would come last and win everywhere.) Nothing checks that combinations make sense.
- **Invert Mouse Y:** the game's menu shows the *opposite* of what it stores. `InvertY` is the state you see in the menu; the plugin writes the
  right value for you.
- **Mouse sensitivity:** `Sensitivity` is the position of the "Mouse Sensitivity" slider in percent (stock 50). The menu limits the slider to
  about 6-100 %, and so does the plugin. Like the slider, it only sets the saved (horizontal) value; the vertical sensitivity is not touched
  (the game's own menu never changes it either). It is commented out in the shipped INI, so the default stays stock until you set it;
  see the note above about when a lower value is a good idea. A fix for the sensitivity affecting only one axis is deliberately **not**
  part of this plugin; use a separate plugin for that.
- **More settings (not yet tested in the game):** the INI also has `[Gamepad]`, `[Display]` (brightness, draw distance, effects quality, radar, HUD,
  frame limiter, ...) and `[Audio]` (volumes, radio) sections. Every option is there with its stock value and range, commented out. They are applied
  at the same moments as the rest: at startup without a `gta_sa.set`, and right after the matching menu page is reset to defaults. Each page
  is only touched by its own reset (resetting Audio never changes your display settings). Change one option at a time and check the log, which
  shows the value before and after.
- **`[Memory]` (advanced):** `Label = ADDRESS TYPE VALUE [When=...]` writes a number straight into the game's memory at the same
  moments, e.g. a setting of another mod that resets itself. `When=` limits it to some moments (`DISPLAY`, `AUDIO`, `MOUSE`, `KEYS`, or `ALL`).
  The address must be inside the game image (0x400000-0xFFFFFF) and the type one of `u8 i8 u16 i16 u32 i32 float double`. A wrong address or type
  can crash the game; every write is logged with the value before and after.
- Unknown actions, key names, options or sections: that line is logged and ignored. A missing INI means nothing is done.

## Examples

The shipped INI already does the right thing for most people (see "What changes for you"). These are typical edits.
Everything goes into `DefaultSettings.SA.ini`; to test a change, back up and delete `gta_sa.set`, start the game and
read `DefaultSettings.SA.log` (one `APPLIED`, `SKIPPED` or `FAILED` line per rule).

**A different mouse feel**

```ini
[Mouse]
InvertY=0            ; the vertical mouse-look is not inverted
Sensitivity=15       ; the "Mouse Sensitivity" slider at 15 % (recommended only with a vertical-sensitivity fix, e.g. SilentPatch)
```

**One key for some layouts only**

```ini
[Keys]
PED_DUCK=C OnlyIfLayout=QWERTZ,QWERTY    ; duck on C, but not on AZERTY
PED_DUCK.Slot1=NONE                      ; leave the second slot unbound
```

**A general rule with an exception**

```ini
[Keys]
VEHICLE_LOOKLEFT=X                        ; every layout
VEHICLE_LOOKLEFT=A OnlyIfLayout=AZERTY    ; AZERTY: replaces the X (the later matching line wins)
```

**Go back to the stock value**

Put a `;` in front of the active line. Active lines keep their stock line right below them, commented out.

## Log

`DefaultSettings.SA.log` next to the `.asi` is rewritten on every launch: every hook, every INI rule as `APPLIED` / `SKIPPED` / `FAILED` with the
reason and the value before and after, and a dump of all keyboard bindings. **If the game is in `C:\Program Files (x86)`, Windows redirects it
to `%LOCALAPPDATA%\VirtualStore\Program Files (x86)\...\scripts\DefaultSettings.SA.log`.**

## Compatibility and safety

- Works alongside other plugins that hook the same places (it chains through them).
- Fail safe: wrong game version, an unexpected code layout, a bad INI or any exception: the reason is logged and nothing (further) is done.
- No dependencies beyond Windows system DLLs (static CRT).

## Build

Visual Studio with the C++ x86 toolset. Release | Win32, static CRT (`/MT`).

- `build.bat` -> `out\DefaultSettings.SA.asi`; also builds and runs `tools\selftest.cpp` and regenerates `out\DefaultSettings.SA.ini`.
- or `cmake -S . -B build -A Win32 && cmake --build build --config Release`

The checked-in `data\DefaultSettings.SA.ini` is **generated** by `selftest.exe --emit-ini` from `src\actions.h` (the table the plugin uses), so the stock
comments cannot drift from the code. Change the generator (`tools\selftest.cpp`), not the INI.

## Tests

- `tools\selftest.cpp` (run by `build.bat`): INI parser, key names, layout lists, the action table, and that every shipped stock comment parses to
  the stock key.
- `tools\hooktest\run.bat`: an **emulated game**: the test exe's own PE image is mapped at `0x400000` and carries fake game functions at the real
  addresses and real `E8` call sites. It loads test builds of the plugin (settings file forced present/missing, layout forced, via compile-time
  macros that are not in the shipped binary) and drives startup and restore defaults. Scenarios: shipped INI without/with `gta_sa.set`, a custom
  INI (`ClearDuplicates`, `NONE`, slot 1, layout conditions, combined rules, bad lines), call sites already redirected by another plugin, and a
  forced AZERTY layout. It checks calling conventions, stack cleanup, deferral, unhooking and rule logic.
- A plugin load into a non-GTA process logs "not GTA SA v1.0 US", patches nothing and unloads cleanly.

## How it works

- **Setting a key:** the plugin calls the game's own functions to clear a slot (`0x52FD70`) and to set a key (`0x530490`); these maintain the
  key priorities ("set order", 1..4). The slot is read back and logged as `APPLIED` only if key and priority are as expected, else `FAILED`.
- **Hooks (`E8` call sites, chained to whatever is there):** the game process tick `0x53E981` and the two menu-draw calls `0x53E82D`, `0x53EB8C`
  (startup check, removed after it ran); `ReinitControls` (`0x531F20`) at `0x57D298` (restore defaults) and `0x7489C6` (startup); and the restore-defaults call to
  `SetDefaultPreferences` at `0x57D273` (page 36 = controller/mouse). During startup `ReinitControls` runs before `gta_sa.set` is loaded, so it is
  **deferred** to the startup check, which decides by whether the file exists.
- **Mouse:** `bInvertMouseY` at `0xBA6745`; the menu entry (action 46, label `FEC_IVV`) shows `FEM_ON` when the byte is 0.

## Verification status

Structure layouts, enum values, key codes and several addresses were checked at compile time against published game header definitions (kept as a
local check, not part of this repository). Everything else comes from community documentation of the game or from analysing the game executable
and is re-checked at runtime where possible:

- Action ids 28 and 58 and the alias names (`PED_WALK`, `PED_FIRE_WEAPON`, ...); id 53 is a marker, not an action.
- The clear/set functions above (calling convention, argument order, `ret 8` / `ret 0xC`, the unbound marker `{1056, 0}`), the priority rule
  (new key gets the number of bound slots + 1), and the `ReinitControls` call sites (found by scanning the exe).
- The restore-defaults call site `0x57D273` and the fact that page 36 resets the mouse settings and sets the `bInvertMouseY` byte to 0.
- The polarity of Invert Mouse Y (from the menu draw code); the text of the label was not read from the game's text files.
- The stock default keys (INI comments): for a non-German, non-French game language, with the language variants noted in the INI. Several
  constants are obfuscated in the Hoodlum exe, so they were not re-derived from it. **The first-run log dumps the real values; compare them.**
- Keys are stored as plain (uppercase) ASCII; the game maps a pressed key with its unshifted character on the current layout.
- The order of events at startup (`ReinitControls` before the settings are loaded, menu draw/tick after both); the plugin waits until the
  controls are initialised and defers rather than depends on it.
- A missing `gta_sa.set` means defaults (the game only reads it and never creates it); the Documents path is
  `SHGetFolderPath(CSIDL_PERSONAL)\GTA San Andreas User Files`.

## Test plan (in game)

Back up `Documents\GTA San Andreas User Files\gta_sa.set` first.

1. **Fresh `gta_sa.set`:** rename/delete it and start the game. The log shows `gta_sa.set does not exist` and `APPLIED` rules; Controller Setup
   shows sprint = Left Shift and jump = Space, Invert Mouse Y = Off. Quit; the game saves the new `gta_sa.set`.
2. **Existing `gta_sa.set`:** start again. The log says `gta_sa.set exists -> user settings, nothing applied.` Change a binding, restart: it is kept.
3. **Restore defaults:** Controller Setup > the defaults entry > Yes. The log shows a mouse moment and a controls moment with `APPLIED`.
4. **Uninstall:** delete the `.asi`; the game uses what `gta_sa.set` holds. Restore defaults gives stock values.
5. **Layout rule:** `OnlyIfLayout` for the other layout shows `SKIPPED ... layout condition not met`.
6. **Bad INI line:** logged and ignored; the other rules still apply.

## License

Copyright (C) 2026 Sebastian Merz. Free software under the **GNU General Public License, version 3 or (at your
option) any later version** - see [LICENSE](LICENSE). The plugin comes with no warranty. It is a fan project and not
affiliated with Rockstar Games or Take-Two Interactive. It contains no game assets or game code.
