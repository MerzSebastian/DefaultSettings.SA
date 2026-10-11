// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Sebastian Merz. See LICENSE.

// Table of the non-key settings the INI can set (mouse, gamepad, display, audio). Pure data, no game access
// (shared with tools/selftest.cpp, which generates the INI and tests the parser from it).
//
// Where each value lives and what its stock default is was read from the game's own "reset to defaults" code
// (SetDefaultPreferences, 0x573AE0: page 3 = audio, 4 and 27 = display, 36 = controller). Names of the fields come
// from community documentation. See docs/SETTINGS.md for the notes per setting.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include "keys.h"   // ToUpper

// Which game reset a setting is applied after.
enum Scope : unsigned { SCOPE_KEYS = 1, SCOPE_MOUSE = 2, SCOPE_DISPLAY = 4, SCOPE_AUDIO = 8, SCOPE_ALL = 15 };

enum class VType { Bool, Int, Float };

// How the INI value relates to the value stored in the game.
enum class Conv {
    Raw,        // stored as is
    Negated,    // Bool: the menu shows the negated byte (Invert Mouse Y: byte 0 = menu On)
    MouseSens,  // INI: slider percent 0..100; stored: float in [1/3200, 1/200] (stock 0.0025 = 50 %)
};


// Extra step the game itself performs when the value changes (writing the stored value alone would not be enough).
enum class After {
    None,
    Gamma,            // brightness: gamma.SetGamma(value / 512, true)                    0x747200 on 0xC92134
    DrawDistance,     // also store the value in the LOD distance scale                   0x8CD800
    FxQuality,        // effects quality: FxSystem.SetFxQuality(value)                    0x49EA40 on 0xA9AE00 (no direct storage)
    MusicVolume,      // AudioEngine.SetMusicMasterVolume(value)                          0x506DE0 on 0xB6BC90
    SfxVolume,        // AudioEngine.SetEffectsMasterVolume(value)                        0x506E10
    BassEnhance,      // AudioEngine.SetBassEnhanceOnOff(value)                           0x506F90
    RadioAutoRetune,  // AudioEngine.SetRadioAutoRetuneOnOff(value)                       0x506F80
};

constexpr uintptr_t ADDR_Menu = 0xBA6748;   // FrontEndMenuManager; "Menu +N" settings live at ADDR_Menu + N

struct SettingInfo {
    const char* section;   // INI section: Mouse, Gamepad, Display, Audio
    const char* name;      // INI key
    unsigned    scope;     // the reset after which it is applied
    uintptr_t   addr;      // where it is stored (0 = no direct storage, only the After step)
    int         size;      // bytes at addr: 1 or 4
    VType       type;
    Conv        conv;
    double      min, max;  // allowed INI values
    double      stock;     // stock default, in INI units
    After       after;
    const char* names;     // comma separated names for the values min, min+1, ... (or nullptr)
    const char* note;      // shown in the INI
};

constexpr SettingInfo kSettings[] = {
    // ---- [Mouse] (page 36)
    { "Mouse", "InvertY",       SCOPE_MOUSE,   0xBA6745, 1, VType::Bool,  Conv::Negated,   0, 1, 1, After::None, nullptr,
      "menu entry \"Invert Mouse Y\": 1 = On, 0 = Off. The menu shows the negated byte; the plugin handles that" },
    { "Mouse", "Sensitivity",   SCOPE_MOUSE,   0xB6EC1C, 4, VType::Float, Conv::MouseSens, 0, 100, 50, After::None, nullptr,
      "slider \"Mouse Sensitivity\" in percent; the menu limits it to about 6-100. Only the saved (horizontal) value is set, like the slider. "
      "A lower value suits a modern high-DPI mouse, but only use it with a mod that fixes the vertical sensitivity (such as SilentPatch): "
      "in the plain game the vertical sensitivity is always 25% of the horizontal one" },
    { "Mouse", "MouseSteering", SCOPE_MOUSE,   0xC1CC02, 1, VType::Bool,  Conv::Raw,       0, 1, 0, After::None, nullptr,
      "menu entry \"Mouse Steering\" (steer vehicles with the mouse)" },
    { "Mouse", "MouseFlying",   SCOPE_MOUSE,   0xC1CC03, 1, VType::Bool,  Conv::Raw,       0, 1, 1, After::None, nullptr,
      "menu entry \"Mouse Flying\" (fly aircraft with the mouse)" },
    // ---- [Gamepad] (page 36)
    { "Gamepad", "InvertX1",    SCOPE_MOUSE,   ADDR_Menu + 0xB1, 1, VType::Bool, Conv::Raw, 0, 1, 0, After::None, nullptr, "invert the left stick, horizontal" },
    { "Gamepad", "InvertY1",    SCOPE_MOUSE,   ADDR_Menu + 0xB2, 1, VType::Bool, Conv::Raw, 0, 1, 0, After::None, nullptr, "invert the left stick, vertical" },
    { "Gamepad", "InvertX2",    SCOPE_MOUSE,   ADDR_Menu + 0xB3, 1, VType::Bool, Conv::Raw, 0, 1, 0, After::None, nullptr, "invert the right stick, horizontal" },
    { "Gamepad", "InvertY2",    SCOPE_MOUSE,   ADDR_Menu + 0xB4, 1, VType::Bool, Conv::Raw, 0, 1, 0, After::None, nullptr, "invert the right stick, vertical" },
    { "Gamepad", "SwapAxis1",   SCOPE_MOUSE,   ADDR_Menu + 0xB5, 1, VType::Bool, Conv::Raw, 0, 1, 0, After::None, nullptr, "swap the axes of the left stick" },
    { "Gamepad", "SwapAxis2",   SCOPE_MOUSE,   ADDR_Menu + 0xB6, 1, VType::Bool, Conv::Raw, 0, 1, 0, After::None, nullptr, "swap the axes of the right stick" },
    // ---- [Display] (pages 4 and 27)
    { "Display", "FrameLimiter", SCOPE_DISPLAY, ADDR_Menu + 0x4C, 1, VType::Bool,  Conv::Raw, 0, 1, 1, After::None, nullptr,
      "menu entry \"Frame Limiter\" (on = 30 fps; a plugin that offers more fps choices uses the same byte, see [Memory])" },
    { "Display", "Brightness",   SCOPE_DISPLAY, ADDR_Menu + 0x3C, 4, VType::Int,   Conv::Raw, 0, 384, 256, After::Gamma, nullptr,
      "slider \"Brightness\", raw value (the slider shows value / 384)" },
    { "Display", "DrawDistance", SCOPE_DISPLAY, ADDR_Menu + 0x40, 4, VType::Float, Conv::Raw, 0.925, 1.8, 1.2, After::DrawDistance, nullptr,
      "slider \"Draw Distance\", raw value (the slider shows (value - 0.925) / 0.875)" },
    { "Display", "Hud",          SCOPE_DISPLAY, ADDR_Menu + 0x21, 1, VType::Bool,  Conv::Raw, 0, 1, 1, After::None, nullptr, "menu entry \"HUD Mode\"" },
    { "Display", "RadarMode",    SCOPE_DISPLAY, ADDR_Menu + 0x24, 4, VType::Int,   Conv::Raw, 0, 2, 0, After::None, "MAPS_AND_BLIPS,BLIPS_ONLY,OFF",
      "menu entry \"Radar Mode\"; a number or a name" },
    { "Display", "MapLegend",    SCOPE_DISPLAY, ADDR_Menu + 0x4A, 1, VType::Bool,  Conv::Raw, 0, 1, 0, After::None, nullptr, "menu entry \"Map Legend\"" },
    { "Display", "Widescreen",   SCOPE_DISPLAY, ADDR_Menu + 0x4B, 1, VType::Bool,  Conv::Raw, 0, 1, 0, After::None, nullptr, "menu entry \"Widescreen\" (HUD/menu scaling)" },
    { "Display", "Subtitles",    SCOPE_DISPLAY, ADDR_Menu + 0x44, 1, VType::Bool,  Conv::Raw, 0, 1, 0, After::None, nullptr,
      "menu entry \"Subtitles\"; the stock value is what this game version's reset code writes" },
    { "Display", "BlipLocations", SCOPE_DISPLAY, ADDR_Menu + 0x45, 1, VType::Bool, Conv::Raw, 0, 1, 1, After::None, nullptr, "map blips: locations" },
    { "Display", "BlipContacts",  SCOPE_DISPLAY, ADDR_Menu + 0x46, 1, VType::Bool, Conv::Raw, 0, 1, 1, After::None, nullptr, "map blips: contacts" },
    { "Display", "BlipMissions",  SCOPE_DISPLAY, ADDR_Menu + 0x47, 1, VType::Bool, Conv::Raw, 0, 1, 1, After::None, nullptr, "map blips: missions" },
    { "Display", "BlipOther",     SCOPE_DISPLAY, ADDR_Menu + 0x48, 1, VType::Bool, Conv::Raw, 0, 1, 1, After::None, nullptr, "map blips: other" },
    { "Display", "BlipGangAreas", SCOPE_DISPLAY, ADDR_Menu + 0x49, 1, VType::Bool, Conv::Raw, 0, 1, 1, After::None, nullptr, "map blips: gang areas" },
    { "Display", "MipMapping",   SCOPE_DISPLAY, ADDR_Menu + 0xC4, 1, VType::Bool,  Conv::Raw, 0, 1, 1, After::None, nullptr,
      "menu entry \"MIP Mapping\"; the game applies it when it starts a game or in the main menu" },
    { "Display", "SavePhotos",   SCOPE_DISPLAY, ADDR_Menu + 0xE8, 1, VType::Bool,  Conv::Raw, 0, 1, 1, After::None, nullptr, "menu entry \"Save Photos\"" },
    { "Display", "FxQuality",    SCOPE_DISPLAY, 0,                1, VType::Int,   Conv::Raw, 0, 3, 2, After::FxQuality, "LOW,MEDIUM,HIGH,VERY_HIGH",
      "menu entry \"Effects Quality\"; a number or a name" },
    // ---- [Audio] (page 3)
    { "Audio", "SfxVolume",        SCOPE_AUDIO, ADDR_Menu + 0x4F, 1, VType::Int,  Conv::Raw, 0, 64, 64, After::SfxVolume, nullptr, "slider \"SFX Volume\", raw value (the slider shows value / 64)" },
    { "Audio", "RadioVolume",      SCOPE_AUDIO, ADDR_Menu + 0x50, 1, VType::Int,  Conv::Raw, 0, 64, 64, After::MusicVolume, nullptr, "slider \"Radio Volume\", raw value (the slider shows value / 64)" },
    { "Audio", "RadioEq",          SCOPE_AUDIO, ADDR_Menu + 0x51, 1, VType::Bool, Conv::Raw, 0, 1, 1, After::BassEnhance, nullptr, "menu entry \"Radio Equalizer\"" },
    { "Audio", "RadioAutoSelect",  SCOPE_AUDIO, ADDR_Menu + 0x4D, 1, VType::Bool, Conv::Raw, 0, 1, 1, After::RadioAutoRetune, nullptr, "menu entry \"Radio Auto Select\"" },
    { "Audio", "RadioMode",        SCOPE_AUDIO, ADDR_Menu + 0xB0, 1, VType::Int,  Conv::Raw, 0, 2, 0, After::None, "RADIO,RANDOM,SEQUENTIAL", "menu entry \"User Tracks Play Mode\"; a number or a name" },
    { "Audio", "TracksAutoScan",   SCOPE_AUDIO, ADDR_Menu + 0xC5, 1, VType::Bool, Conv::Raw, 0, 1, 0, After::None, nullptr, "menu entry \"User Tracks Auto Scan\"" },
};
constexpr int kNumSettings = static_cast<int>(sizeof(kSettings) / sizeof(kSettings[0]));

// The game's mouse sensitivity value (CCamera::m_fMouseAccelHorzntal): the menu slider shows value / 0.005, and its
// handler clamps the value to [1/3200, 1/200]. The stock default is 0.0025 (= 50 %).
inline float MouseAccelFromPercent(double pct) {
    const double v = pct / 100.0 * 0.005;
    return static_cast<float>((std::min)((std::max)(v, 1.0 / 3200.0), 1.0 / 200.0));   // parenthesised: windows.h defines min/max macros
}

// Index of a setting by section and key (case-insensitive), or -1.
inline int FindSetting(const std::string& section, const std::string& name) {
    const std::string s = ToUpper(section), n = ToUpper(name);
    for (int i = 0; i < kNumSettings; ++i)
        if (s == ToUpper(kSettings[i].section) && n == ToUpper(kSettings[i].name)) return i;
    return -1;
}

inline bool IsSettingSection(const std::string& section) {
    const std::string s = ToUpper(section);
    for (const SettingInfo& i : kSettings) if (s == ToUpper(i.section)) return true;
    return false;
}

// A name from info.names (comma separated, value = min + position), case-insensitive.
inline bool ParseSettingName(const SettingInfo& info, const std::string& token, double& value) {
    if (!info.names) return false;
    const std::string t = ToUpper(token);
    std::string list = info.names;
    int idx = 0;
    size_t b = 0;
    for (;;) {
        const size_t e = list.find(',', b);
        if (ToUpper(list.substr(b, e == std::string::npos ? std::string::npos : e - b)) == t) { value = info.min + idx; return true; }
        if (e == std::string::npos) break;
        b = e + 1; ++idx;
    }
    return false;
}

// INI value -> the number the game stores (byte, int or float, as a double).
inline double ToStored(const SettingInfo& info, double v) {
    switch (info.conv) {
        case Conv::Negated:   return v != 0 ? 0 : 1;
        case Conv::MouseSens: return MouseAccelFromPercent(v);
        case Conv::Raw:       break;
    }
    if (info.type == VType::Int || info.type == VType::Bool) return static_cast<double>(static_cast<long long>(v + (v < 0 ? -0.5 : 0.5)));
    return static_cast<float>(v);
}

// The number the game stores -> INI value (for the log).
inline double FromStored(const SettingInfo& info, double raw) {
    switch (info.conv) {
        case Conv::Negated:   return raw != 0 ? 0 : 1;
        case Conv::MouseSens: return raw / 0.005 * 100.0;
        case Conv::Raw:       break;
    }
    return raw;
}
