# Changelog

## 1.0.0

First release.

- Changes the game's **default** controls, mouse, display and audio settings from an INI file, and never overrides
  what the player chose: the rules only apply when the game is on its stock defaults (no `gta_sa.set` at startup,
  or right after "restore defaults" in the menu).
- The shipped INI depends on the keyboard layout (QWERTY, QWERTZ, AZERTY): sprint on Left Shift, jump on Space,
  Invert Mouse Y off; Conversation Yes on Y for QWERTZ; ZQSD for AZERTY.
- Rules for keys (two slots per action), mouse, gamepad, display, audio and raw memory values, with layout conditions,
  `ClearDuplicates` and combined rules.
- Log `DefaultSettings.SA.log` next to the plugin: every rule as `APPLIED`, `SKIPPED` or `FAILED`, with the value before
  and after.

Known limits: only GTA San Andreas 1.0 US. The `[Gamepad]`, `[Display]` and `[Audio]` rules are not yet
tested in the game.
