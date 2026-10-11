// Action table: names, ids and stock keyboard defaults. Pure data, no game access
// (shared with tools/selftest.cpp, which generates and checks the shipped INI).
//
// Ids are the controller action ids of the game (array of 59). A few actions have two accepted names:
//   - 17: SNEAK_ABOUT / PED_WALK
//   - 0/1/18/19: PED_FIREWEAPON etc. / PED_FIRE_WEAPON etc.
//   - 28 (VEHICLE_RADIO_TRACK_SKIP) and 58 (SWITCH_CAM_DEBUG_MENU) are unnamed in common header definitions;
//     the names come from community documentation. 53 is a marker, not an action.
// Stock defaults come from community documentation of the game's default control setup (0x530640).
// Shown here for a non-German, non-French game language. "NONE" = unbound.
#pragma once

struct ActionInfo {
    int         id;
    const char* name;    // canonical name
    const char* alias;   // alternative accepted name or nullptr
    const char* stock0;  // keyboard slot 0 (eControllerType CONTROLLER_KEYBOARD1)
    const char* stock1;  // keyboard slot 1 (CONTROLLER_KEYBOARD2, "optional extra key")
    const char* note;    // language-dependent stock defaults, or nullptr
    bool        common;  // false = name only known from community documentation
};

constexpr int NUM_ACTIONS = 59; // CControllerAction m_actions[59]

constexpr ActionInfo kActions[] = {
    {  0, "PED_FIREWEAPON",                      "PED_FIRE_WEAPON",         "PADINS", "LCTRL",  nullptr, true },
    {  1, "PED_FIREWEAPON_ALT",                  "PED_FIRE_WEAPON_ALT",     "\\",     "NONE",   "original code is '\\' (some community builds use '0')", true },
    {  2, "PED_CYCLE_WEAPON_RIGHT",              nullptr,                   "E",      "PADENTER", nullptr, true },
    {  3, "PED_CYCLE_WEAPON_LEFT",               nullptr,                   "Q",      "PADDEL", "French: A / PADDEL", true },
    {  4, "GO_FORWARD",                          nullptr,                   "UP",     "W",      "French slot 1: Z", true },
    {  5, "GO_BACK",                             nullptr,                   "DOWN",   "S",      nullptr, true },
    {  6, "GO_LEFT",                             nullptr,                   "LEFT",   "A",      "French slot 1: Q", true },
    {  7, "GO_RIGHT",                            nullptr,                   "RIGHT",  "D",      nullptr, true },
    {  8, "PED_SNIPER_ZOOM_IN",                  nullptr,                   "PGUP",   "X",      "German slot 1: Y", true },
    {  9, "PED_SNIPER_ZOOM_OUT",                 nullptr,                   "PGDN",   "Z",      "French slot 1: W", true },
    { 10, "VEHICLE_ENTER_EXIT",                  nullptr,                   "ENTER",  "F",      nullptr, true },
    { 11, "CAMERA_CHANGE_VIEW_ALL_SITUATIONS",   nullptr,                   "V",      "HOME",   nullptr, true },
    { 12, "PED_JUMPING",                         nullptr,                   "LSHIFT", "RCTRL",  nullptr, true },
    { 13, "PED_SPRINT",                          nullptr,                   "SPACE",  "NONE",   nullptr, true },
    { 14, "PED_LOOKBEHIND",                      nullptr,                   "PADEND", "NONE",   nullptr, true },
    { 15, "PED_DUCK",                            nullptr,                   "C",      "NONE",   nullptr, true },
    { 16, "PED_ANSWER_PHONE",                    nullptr,                   "TAB",    "NONE",   nullptr, true },
    { 17, "SNEAK_ABOUT",                         "PED_WALK",                "LALT",   "NONE",   nullptr, true },
    { 18, "VEHICLE_FIREWEAPON",                  "VEHICLE_FIRE_WEAPON",     "RCTRL",  "LALT",   nullptr, true },
    { 19, "VEHICLE_FIREWEAPON_ALT",              "VEHICLE_FIRE_WEAPON_ALT", "LCTRL",  "PADINS", nullptr, true },
    { 20, "VEHICLE_STEERLEFT",                   nullptr,                   "A",      "LEFT",   "French: Q / LEFT", true },
    { 21, "VEHICLE_STEERRIGHT",                  nullptr,                   "D",      "RIGHT",  nullptr, true },
    { 22, "VEHICLE_STEERUP",                     nullptr,                   "UP",     "NONE",   nullptr, true },
    { 23, "VEHICLE_STEERDOWN",                   nullptr,                   "DOWN",   "NONE",   nullptr, true },
    { 24, "VEHICLE_ACCELERATE",                  nullptr,                   "W",      "NONE",   "French: Z", true },
    { 25, "VEHICLE_BRAKE",                       nullptr,                   "S",      "NONE",   nullptr, true },
    { 26, "VEHICLE_RADIO_STATION_UP",            nullptr,                   "INS",    "4",      "French slot 1: `", true },
    { 27, "VEHICLE_RADIO_STATION_DOWN",          nullptr,                   "DEL",    "R",      nullptr, true },
    { 28, "VEHICLE_RADIO_TRACK_SKIP",            nullptr,                   "F5",     "NONE",   "id and name from community documentation", false },
    { 29, "VEHICLE_HORN",                        nullptr,                   "CAPSLK", "H",      nullptr, true },
    { 30, "TOGGLE_SUBMISSIONS",                  nullptr,                   "2",      "PLUS",   "French slot 0: key code 233", true },
    { 31, "VEHICLE_HANDBRAKE",                   nullptr,                   "SPACE",  "RCTRL",  nullptr, true },
    { 32, "PED_1RST_PERSON_LOOK_LEFT",           nullptr,                   "PADLEFT","NONE",   nullptr, true },
    { 33, "PED_1RST_PERSON_LOOK_RIGHT",          nullptr,                   "PADRIGHT","NONE",  nullptr, true },
    { 34, "VEHICLE_LOOKLEFT",                    nullptr,                   "Q",      "NONE",   "French: A", true },
    { 35, "VEHICLE_LOOKRIGHT",                   nullptr,                   "E",      "NONE",   nullptr, true },
    { 36, "VEHICLE_LOOKBEHIND",                  nullptr,                   "NONE",   "NONE",   nullptr, true },
    { 37, "VEHICLE_MOUSELOOK",                   nullptr,                   "NONE",   "NONE",   nullptr, true },
    { 38, "VEHICLE_TURRETLEFT",                  nullptr,                   "PADLEFT","NONE",   nullptr, true },
    { 39, "VEHICLE_TURRETRIGHT",                 nullptr,                   "PADRIGHT","NONE",  nullptr, true },
    { 40, "VEHICLE_TURRETUP",                    nullptr,                   "PADDOWN","END",    nullptr, true },
    { 41, "VEHICLE_TURRETDOWN",                  nullptr,                   "PADUP",  "DEL",    nullptr, true },
    { 42, "PED_CYCLE_TARGET_LEFT",               nullptr,                   "[",      "NONE",   nullptr, true },
    { 43, "PED_CYCLE_TARGET_RIGHT",              nullptr,                   "]",      "NONE",   nullptr, true },
    { 44, "PED_CENTER_CAMERA_BEHIND_PLAYER",     nullptr,                   "#",      "NONE",   "original code is '#' (some community builds use '3')", true },
    { 45, "PED_LOCK_TARGET",                     nullptr,                   "DEL",    "CAPSLK", nullptr, true },
    { 46, "NETWORK_TALK",                        nullptr,                   "NONE",   "NONE",   nullptr, true },
    { 47, "CONVERSATION_YES",                    nullptr,                   "Y",      "NONE",   "German: Z", true },
    { 48, "CONVERSATION_NO",                     nullptr,                   "N",      "NONE",   nullptr, true },
    { 49, "GROUP_CONTROL_FWD",                   nullptr,                   "G",      "NONE",   nullptr, true },
    { 50, "GROUP_CONTROL_BWD",                   nullptr,                   "H",      "NONE",   nullptr, true },
    { 51, "PED_1RST_PERSON_LOOK_UP",             nullptr,                   "PADDOWN","NONE",   nullptr, true },
    { 52, "PED_1RST_PERSON_LOOK_DOWN",           nullptr,                   "PADUP",  "NONE",   nullptr, true },
    // 53 is not an action (marker)
    { 54, "TOGGLE_DPAD",                         nullptr,                   "NONE",   "NONE",   nullptr, true },
    { 55, "SWITCH_DEBUG_CAM_ON",                 nullptr,                   "NONE",   "NONE",   nullptr, true },
    { 56, "TAKE_SCREEN_SHOT",                    nullptr,                   "NONE",   "NONE",   nullptr, true },
    { 57, "SHOW_MOUSE_POINTER_TOGGLE",           nullptr,                   "NONE",   "NONE",   nullptr, true },
    { 58, "SWITCH_CAM_DEBUG_MENU",               nullptr,                   "NONE",   "NONE",   "name from community documentation", false },
};
constexpr int kNumActionInfos = static_cast<int>(sizeof(kActions) / sizeof(kActions[0]));

// Cross-checks of the ids that are certain.
static_assert(kActions[ 4].id ==  4 && kActions[ 4].name[0] == 'G', "GO_FORWARD = 4");
static_assert(kActions[12].id == 12 && kActions[13].id == 13, "PED_JUMPING = 12, PED_SPRINT = 13");
static_assert(kActions[29].id == 29, "VEHICLE_HORN = 29");
static_assert(kActions[47].id == 47 && kActions[48].id == 48, "CONVERSATION_YES = 47, CONVERSATION_NO = 48");
static_assert(kNumActionInfos == NUM_ACTIONS - 1, "all actions except the id-53 marker");
