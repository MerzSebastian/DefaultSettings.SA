// Host-side self-test and INI generator (no game needed).
//   selftest.exe                 run the checks
//   selftest.exe --emit-ini F    write the default DefaultSettings.SA.ini to F (generated from src/actions.h,
//                                so the "stock" comments cannot drift from the table the plugin uses)
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdarg>
#include "../src/ini.h"
#include "../src/settings.h"

// Defaults this project ships ACTIVE (everything else is a commented stock line). One table drives the
// generator and the tests. `options` is appended after the key (e.g. " OnlyIfLayout=AZERTY").
struct Override { int id; int slot; const char* key; const char* options; const char* header; };

static const char* kHdrSwap =
    "; SWAP: sprint and jump exchanged (stock: sprint = Space, jump = Left Shift). Modern games jump with Space and\n"
    "; sprint with Left Shift, so that is the default here.\n";
static const char* kHdrQwertz =
    "; QWERTZ FIX: a German-language game defaults \"Conversation Yes\" to the key labeled Z. On a QWERTZ\n"
    "; keyboard Y and Z are swapped compared to QWERTY, so that default lands on the wrong key. This makes\n"
    "; it the key labeled Y (the stock default of all other languages). Applies on QWERTZ keyboards only.\n";
static const char* kHdrAzerty =
    "; AZERTY FIX: on an AZERTY keyboard A<->Q and W<->Z are swapped compared to QWERTY, so the stock WASD keys\n"
    "; (the game only uses ZQSD when its language is French) end up scattered. These rules apply only on an AZERTY\n"
    "; layout and give ZQSD, mirroring the game's own French defaults. Not tested on real AZERTY hardware.\n"
    "; (The French defaults also use the character ` for radio-up; it needs AltGr on AZERTY, so it is left out.)\n";

static const Override kOverrides[] = {
    {  3, 0, "A",      " OnlyIfLayout=AZERTY", nullptr },   // PED_CYCLE_WEAPON_LEFT   (Q is taken by moving left)
    {  4, 1, "Z",      " OnlyIfLayout=AZERTY", kHdrAzerty },// GO_FORWARD slot 1
    {  6, 1, "Q",      " OnlyIfLayout=AZERTY", nullptr },   // GO_LEFT slot 1
    {  9, 1, "W",      " OnlyIfLayout=AZERTY", nullptr },   // PED_SNIPER_ZOOM_OUT slot 1
    { 12, 0, "SPACE",  "",                     kHdrSwap },  // PED_JUMPING
    { 13, 0, "LSHIFT", "",                     nullptr },   // PED_SPRINT
    { 20, 0, "Q",      " OnlyIfLayout=AZERTY", nullptr },   // VEHICLE_STEERLEFT
    { 24, 0, "Z",      " OnlyIfLayout=AZERTY", nullptr },   // VEHICLE_ACCELERATE
    { 30, 0, "233",    " OnlyIfLayout=AZERTY", nullptr },   // TOGGLE_SUBMISSIONS (key code 233 = the e-acute key)
    { 34, 0, "A",      " OnlyIfLayout=AZERTY", nullptr },   // VEHICLE_LOOKLEFT
    { 47, 0, "Y",      " OnlyIfLayout=QWERTZ", kHdrQwertz },// CONVERSATION_YES
};
constexpr int kNumOverrides = static_cast<int>(sizeof(kOverrides) / sizeof(kOverrides[0]));

static const Override* FindOverride(int id, int slot) {
    for (const Override& o : kOverrides) if (o.id == id && o.slot == slot) return &o;
    return nullptr;
}

// Non-key settings shipped ACTIVE (value in INI units). Everything else stays a commented stock line.
struct SettingOverride { const char* name; double value; const char* header; };
static const char* kHdrInvertY =
    "; Invert Mouse Y Off: modern games do not invert the vertical look by default (the game's stock value is On).\n";

static const SettingOverride kSettingOverrides[] = {
    { "InvertY", 0, kHdrInvertY },
};
constexpr int kNumSettingOverrides = static_cast<int>(sizeof(kSettingOverrides) / sizeof(kSettingOverrides[0]));

static const SettingOverride* FindSettingOverride(const char* name) {
    for (const SettingOverride& o : kSettingOverrides) if (!std::strcmp(o.name, name)) return &o;
    return nullptr;
}

static std::string Fmt(const char* f, ...) {
    char buf[256]; va_list ap; va_start(ap, f); vsnprintf(buf, sizeof(buf), f, ap); va_end(ap); return buf;
}

static void EmitSettings(std::ostringstream& o) {
    const char* order[] = { "Mouse", "Gamepad", "Display", "Audio" };
    const char* intro[] = {
        "; Mouse (applied after the controller page is reset to defaults, and at startup without gta_sa.set).\n",
        "; Gamepad (same moment as the mouse).\n",
        "; Display and graphics (applied after the display pages are reset).\n"
        "; NEW and not yet tested in the game: everything in [Gamepad], [Display] and [Audio]. They are listed with their\n"
        "; stock values; change one at a time and check DefaultSettings.SA.log (it shows the value before and after).\n",
        "; Audio (applied after the audio page is reset).\n",
    };
    for (int s = 0; s < 4; ++s) {
        o << "[" << order[s] << "]\n" << intro[s];
        for (const SettingInfo& si : kSettings) {
            if (std::strcmp(si.section, order[s])) continue;
            std::string range = si.type == VType::Bool ? "0 or 1" : Fmt("%g to %g", si.min, si.max);
            if (si.names) range += std::string(", or a name: ") + si.names;
            if (const SettingOverride* ov = FindSettingOverride(si.name)) {
                if (ov->header) o << ov->header;
                o << si.name << "=" << Fmt("%g", ov->value) << "\n";
            }
            o << ";" << si.name << "=" << Fmt("%g", si.stock) << "      ; " << si.note << " (" << range << ")\n";
        }
        o << "\n";
    }
}

static std::string BuildIni() {
    std::ostringstream o;
    o << "; DefaultSettings.SA.ini - change the game's DEFAULT controls/settings.\n"
         ";\n"
         "; The rules below are applied ONLY when the game is on its stock defaults:\n"
         ";   - at startup, if Documents\\GTA San Andreas User Files\\gta_sa.set does not exist\n"
         ";   - right after the game resets them (Options > Controller Setup > the defaults entry)\n"
         "; This plugin never reads or edits gta_sa.set, and does nothing at startup if it exists (your saved settings win).\n"
         ";   Restore Defaults is the exception by design: the game then saves its settings, so gta_sa.set\n"
         ";   is rewritten by the game with the INI values as the new defaults.\n"
         "; Everything is logged to DefaultSettings.SA.log.\n"
         ";\n"
         "; Every option is listed with its stock value, commented out (;) except the active lines described below.\n"
         "; Remove the leading ';' and change the value to use an option. Anything after a ' ;' is a comment.\n"
         "; Bad lines are logged and ignored.\n"
         ";\n"
         "; Key rule:   ACTION=KEY                 keyboard slot 0 (the main key)\n"
         ";             ACTION.Slot1=KEY           keyboard slot 1 (the \"optional extra key\")\n"
         ";   options after KEY (space separated):\n"
         ";     OnlyIfLayout=QWERTZ,QWERTY,AZERTY,ANY   apply only on these layouts (a list separated by ',', no spaces).\n"
         ";                                       Detected from the keyboard: QWERTZ = 'Y' key at scancode 0x2C, AZERTY = 'A' at 0x10\n"
         ";     ClearDuplicates                   also unbind this key from other actions in the same slot\n"
         ";                                       (never from slots that another rule of this INI sets)\n"
         ";   KEY: a letter/digit/punctuation character (A, 5, [), or a name: SPACE ESC F1..F12 INS DEL HOME END\n"
         ";        PGUP PGDN UP DOWN LEFT RIGHT DIVIDE TIMES PLUS MINUS PADDEL PADEND PADDOWN PADPGDN PADLEFT PAD5\n"
         ";        NUMLOCK PADRIGHT PADHOME PADUP PADPGUP PADINS PADENTER SCROLL PAUSE BACKSP TAB CAPSLK ENTER\n"
         ";        LSHIFT RSHIFT SHIFT LCTRL RCTRL LALT RALT LWIN RWIN APPS, or NONE (leave the slot unbound).\n"
         ";        A number (decimal or 0x..) 32-255 / 1000-1056 is taken as a raw key code.\n"
         ";\n"
         "; Combining rules: rules run top to bottom and every rule whose conditions match is applied, so for the same\n"
         "; action and slot the LATER matching line wins. That makes a general rule plus layout exceptions easy:\n"
         ";\n"
         ";   VEHICLE_LOOKLEFT=X                        -> X on every layout\n"
         ";   VEHICLE_LOOKLEFT=A OnlyIfLayout=AZERTY    -> ...but A on AZERTY (the line above is applied first, then replaced)\n"
         ";\n"
         ";   PED_DUCK=C OnlyIfLayout=QWERTZ,QWERTY     -> one rule for several layouts\n"
         ";   PED_DUCK.Slot1=NONE                       -> slots are independent: slot 0 and slot 1 do not affect each other\n"
         ";\n"
         "; The order is the order in this file, not the action order. Put the general line FIRST and the exceptions after it.\n"
         "; Nothing checks that combinations make sense (e.g. two actions on one key): the log lists both applied rules and a\n"
         "; NOTE for keys that end up on two actions; add ClearDuplicates to a rule if you want the other binding removed.\n"
         ";\n"
         "; Active as shipped: sprint and jump swapped, \"Invert Mouse Y\" Off, the QWERTZ fix for Conversation Yes and\n"
         "; ZQSD movement on AZERTY keyboards. Comment a line out with ';' to get the game's stock value back.\n"
         "; Stock values below are for a non-German, non-French game language; where the language changes them\n"
         "; the line carries a note. Stock values come from community documentation; the first-run log shows the real ones.\n"
         ";\n"
         "; Under every active line the original (stock) line is kept, commented out, for reference. To get the stock value\n"
         "; back, comment out the ACTIVE line. Do not uncomment the stock line below it: it would come later in the file and\n"
         "; override the rule above it (see \"Combining rules\").\n"
         "\n"
"";
    EmitSettings(o);
    o << "[Memory]\n"
         "; Advanced: write any value into the game's memory at a defaults moment, e.g. a setting of another mod\n"
         "; that resets itself when you restore the defaults. A wrong address or type can crash the game or corrupt it.\n"
         ";\n"
         ";   Label = ADDRESS TYPE VALUE [When=...] [OnlyIfLayout=...]\n"
         ";\n"
         ";   Label    any name you like (only shown in the log)\n"
         ";   ADDRESS  0x400000 - 0xFFFFFF (the game image and its data), hex (0x..) or decimal\n"
         ";   TYPE     u8 i8 u16 i16 u32 i32 float double   (aliases: byte, word, short, dword, int)\n"
         ";   VALUE    a number that fits the type (hex 0x.. allowed for integers)\n"
         ";   When     which defaults moments write it: ALL (default: startup without gta_sa.set and every reset page),\n"
         ";            or a list of KEYS, MOUSE, DISPLAY, AUDIO separated by ',' (no spaces)\n"
         ";            The moments are: startup = all; Controller Setup defaults = KEYS and MOUSE; Display Setup = DISPLAY;\n"
         ";            Audio Setup = AUDIO.\n"
         ";   Rules run in file order together with the other sections; the log shows the value before and after.\n"
         ";\n"
         ";   Example: default FPS limit 60 for the Widescreen Fix (GTASA.WidescreenFix.asi). It turns the menu entry \"Frame Limiter\"\n"
         ";   into an fps selector that keeps its choice in the game's own frame limiter byte (0xBA6794) as an index:\n"
         ";   0 = off, 1 = 30, 2 = 40, 3 = 50, 4 = 60, 5 = 75, 6 = 100, 7 = 120, 8 = 144, 9 = 165, 10 = 200, 11 = 240.\n"
         ";   Only use this with that mod: the plain game only knows 0 and 1, and the number is meaningless without it.\n"
         "; Frame limit 60 fps = 0xBA6794 u8 4 When=DISPLAY\n"
         "\n"
         "[Keys]\n";
    for (const ActionInfo& a : kActions) {
        for (int slot = 0; slot < 2; ++slot) {
            const char* stock = slot == 0 ? a.stock0 : a.stock1;
            const std::string name = std::string(a.name) + (slot == 0 ? "" : ".Slot1");
            if (const Override* ov = FindOverride(a.id, slot)) {
                if (ov->header) o << ov->header;
                o << name << "=" << ov->key << ov->options << "\n";
            }
            // the stock line is always listed (commented out); under an active line it is kept for reference
            o << ";" << name << "=" << stock;
            if (slot == 0 && a.note) o << "      ; " << a.note;
            o << "\n";
        }
    }
    return o.str();
}

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++g_fail; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static IniData Parse(const std::string& text, std::string* msgs = nullptr) {
    IniData d;
    ParseIni(text, d, [&](const std::string& m) { if (msgs) *msgs += m + "\n"; });
    return d;
}

int main(int argc, char** argv) {
    const std::string ini = BuildIni();
    if (argc == 3 && !std::strcmp(argv[1], "--emit-ini")) {
        std::ofstream f(argv[2], std::ios::binary);
        f << ini;
        std::printf("wrote %s\n", argv[2]);
        return f ? 0 : 1;
    }

    // 1. as shipped: the setting overrides plus exactly the key override table, no parser messages
    {
        std::string msgs;
        IniData d = Parse(ini, &msgs);
        CHECK(msgs.empty(), "shipped INI produced parser messages:\n%s", msgs.c_str());
        CHECK(d.rules.size() == (size_t)(kNumSettingOverrides + kNumOverrides), "expected %d active rules, got %u", kNumSettingOverrides + kNumOverrides, (unsigned)d.rules.size());
        size_t keyRules = 0, settingRules = 0;
        for (const Rule& r : d.rules) {
            if (r.kind == Rule::Setting) {
                ++settingRules;
                const SettingOverride* ov = FindSettingOverride(kSettings[r.setting].name);
                CHECK(ov && ov->value == r.value, "active setting %s = %g", kSettings[r.setting].name, r.value);
                continue;
            }
            ++keyRules;
            const Override* ov = FindOverride(r.action, r.slot);
            CHECK(ov != nullptr, "unexpected active rule for action %d slot %d", r.action, r.slot);
            if (!ov) continue;
            unsigned want = 0;
            CHECK(ParseKey(ov->key, want) && want == r.key, "action %d slot %d key %s", r.action, r.slot, KeyToName(r.key).c_str());
            const unsigned wantMask = std::strstr(ov->options, "AZERTY") ? LAYOUT_AZERTY : std::strstr(ov->options, "QWERTZ") ? LAYOUT_QWERTZ : LAYOUT_ANY;
            CHECK(r.layouts == wantMask, "action %d layout mask %u, expected %u", r.action, r.layouts, wantMask);
        }
        CHECK(keyRules == (size_t)kNumOverrides && settingRules == (size_t)kNumSettingOverrides, "rule counts %u/%u", (unsigned)keyRules, (unsigned)settingRules);
        // the two swapped rules and the QWERTZ fix, by name
        auto find = [&](int id) { for (const Rule& r : d.rules) if (r.kind == Rule::Key && r.action == id && r.slot == 0) return &r; return (const Rule*)nullptr; };
        CHECK(find(12) && find(12)->key == 32 && find(13) && find(13)->key == 1046, "sprint/jump swap");
        CHECK(find(47) && find(47)->key == 'Y' && find(47)->layouts == LAYOUT_QWERTZ, "CONVERSATION_YES=Y on QWERTZ only");
    }

    // 2. uncomment every commented rule line: all must parse, to exactly the stock value
    {
        std::string un, line; std::istringstream in(ini);
        size_t commented = 0;
        while (std::getline(in, line)) {
            const bool rule = line.size() > 2 && line[0] == ';' &&
                              (std::isupper(static_cast<unsigned char>(line[1])) || std::isdigit(static_cast<unsigned char>(line[1]))) &&
                              line.find('=') != std::string::npos && line.find(' ') > line.find('=');   // "ACTION=..." and not prose
            if (rule) { un += line.substr(1) + "\n"; ++commented; } else un += line + "\n";
        }
        std::string msgs;
        IniData d = Parse(un, &msgs);
        CHECK(msgs.empty(), "uncommented INI produced parser messages:\n%s", msgs.c_str());
        CHECK(commented == 2u * kNumActionInfos + kNumSettings, "commented rule lines: %u", (unsigned)commented);   // every stock key line + every setting
        size_t keyRules = 0;
        std::vector<bool> settingSeen(kNumSettings, false);
        for (const Rule& r : d.rules) {
            if (r.kind == Rule::Setting) {
                const SettingInfo& si = kSettings[r.setting];
                const SettingOverride* ov = FindSettingOverride(si.name);
                CHECK(r.value == si.stock || (ov && r.value == ov->value), "%s: parsed %g is neither stock %g nor the override", si.name, r.value, si.stock);
                if (r.value == si.stock) settingSeen[r.setting] = true;
                continue;
            }
            ++keyRules;
            const ActionInfo* a = nullptr;
            for (const ActionInfo& x : kActions) if (x.id == r.action) a = &x;
            CHECK(a != nullptr, "rule for unknown action %d", r.action);
            if (!a) continue;
            // every rule is either the stock line or the active override of that slot
            const Override* ov = FindOverride(r.action, r.slot);
            unsigned stock = 0, over = 0;
            CHECK(ParseKey(r.slot == 0 ? a->stock0 : a->stock1, stock), "stock of %s", a->name);
            const bool isOverride = ov && ParseKey(ov->key, over) && over == r.key;
            CHECK(r.key == stock || isOverride, "%s slot%d: parsed key %s is neither stock nor the override", a->name, r.slot, KeyToName(r.key).c_str());
        }
        for (int i = 0; i < kNumSettings; ++i) CHECK(settingSeen[i], "setting %s has no stock line in the INI", kSettings[i].name);
        CHECK(keyRules == 2u * kNumActionInfos + kNumOverrides, "key rules after uncommenting: %u", (unsigned)keyRules);   // stock lines + active lines
    }

    // 3. key names round trip (names, chars, numbers); unknown rejected
    {
        for (const KeyName& k : kKeyNames) { unsigned c = 0; CHECK(ParseKey(k.name, c) && c == k.code, "name %s", k.name); CHECK(KeyToName(k.code) == k.name || k.code == 32, "reverse %s", k.name); }
        unsigned c = 0;
        CHECK(ParseKey("y", c) && c == 'Y', "lowercase letter -> uppercase ASCII");
        CHECK(ParseKey("lshift", c) && c == 1046, "case-insensitive name");
        CHECK(ParseKey("0x5C", c) && c == 92, "hex");
        CHECK(ParseKey("233", c) && c == 233, "decimal");
        CHECK(!ParseKey("BOGUS", c) && !ParseKey("5", c) == false, "bogus rejected, single digit accepted");
        CHECK(!ParseKey("3", c) || c == '3', "digit is ASCII");
        CHECK(!ParseKey("7000", c), "out of range rejected");
        CHECK(!ParseKey("1", c) == false, "single digit 1 is the character '1', not the mouse button code");
    }

    // 4. parser behaviour
    {
        std::string msgs;
        IniData d = Parse("[Keys]\n"
                          "PED_SPRINT=LSHIFT\n"
                          "PED_JUMPING = SPACE ; swap\n"
                          "ped_jumping.slot1=NONE ClearDuplicates\n"
                          "PED_WALK=X OnlyIfLayout=QWERTY\n"
                          "NOPE=A\nGO_LEFT=BOGUS\nGO_RIGHT.Slot9=A\nGO_BACK=A OnlyIfLayout=DVORAK\nGO_BACK=A Wat\nno equals\n"
                          "[Mouse]\nInvertY=0\nInvertY=maybe\nFoo=1\n[Weird]\nA=B\n", &msgs);
        CHECK(d.rules.size() == 5, "rule count %u", (unsigned)d.rules.size());
        if (d.rules.size() == 5) {
            CHECK(d.rules[1].key == 32 && d.rules[1].action == 12, "inline comment stripped");
            CHECK(d.rules[2].key == KEY_NULL && d.rules[2].slot == 1 && d.rules[2].clearDuplicates, "NONE + ClearDuplicates + case-insensitive slot");
            CHECK(d.rules[3].action == 17 && d.rules[3].layouts == LAYOUT_QWERTY, "alias PED_WALK = SNEAK_ABOUT (17)");
            CHECK(d.rules[4].kind == Rule::Setting && !std::strcmp(kSettings[d.rules[4].setting].name, "InvertY") && d.rules[4].value == 0, "InvertY=0");
        }
        const char* expect[] = { "unknown action 'NOPE'", "unknown key name 'BOGUS'", "bad slot", "bad OnlyIfLayout", "unknown option 'Wat'",
                                 "no '='", "InvertY needs 0 or 1", "unknown [Mouse] key", "unknown section" };
        for (const char* e : expect) CHECK(msgs.find(e) != std::string::npos, "missing parser message: %s", e);
    }

    // 4a. settings: numbers, names, bools, ranges, unknown keys, conversions
    {
        std::string msgs;
        IniData d = Parse("[Mouse]\nSensitivity=80\nSensitivity=0 OnlyIfLayout=QWERTZ\nSensitivity=abc\nSensitivity=101\nSensitivity=-5\nSensitivity=12.5\nBogus=1\nInvertY=maybe\n"
                          "[Display]\nRadarMode=off\nRadarMode=1\nRadarMode=3\nFxQuality=VERY_HIGH\nBrightness=300\nBrightness=400\nDrawDistance=1.5\nHud=0\n"
                          "[Audio]\nRadioMode=sequential\nSfxVolume=32\nRadioEq=on\n[Gamepad]\nSwapAxis1=1\n", &msgs);
        auto val = [&](size_t i) { return d.rules[i].value; };
        CHECK(d.rules.size() == 13, "settings rule count %u", (unsigned)d.rules.size());
        if (d.rules.size() == 13) {
            CHECK(!std::strcmp(kSettings[d.rules[0].setting].name, "Sensitivity") && val(0) == 80, "Sensitivity=80");
            CHECK(val(1) == 0 && d.rules[1].layouts == LAYOUT_QWERTZ, "Sensitivity=0 with a layout");
            CHECK(val(2) == 12.5, "decimal percent");
            CHECK(val(3) == 2, "RadarMode=off -> 2");
            CHECK(val(4) == 1, "RadarMode=1");
            CHECK(val(5) == 3, "FxQuality=VERY_HIGH -> 3");
            CHECK(val(6) == 300, "Brightness=300");
            CHECK(val(7) == 1.5, "DrawDistance=1.5");
            CHECK(val(8) == 0, "Hud=0");
            CHECK(val(9) == 2, "RadioMode=sequential -> 2");
            CHECK(val(10) == 32 && val(11) == 1 && val(12) == 1, "SfxVolume=32, RadioEq=on, SwapAxis1=1");
        }
        const char* expect[] = { "Sensitivity needs a number", "unknown [Mouse] key 'Bogus'", "InvertY needs 0 or 1", "RadarMode needs a number",
                                 "Brightness needs a number" };
        for (const char* e : expect) CHECK(msgs.find(e) != std::string::npos, "missing parser message: %s", e);
        // conversions: 50 % = the stock 0.0025, 100 % = 0.005, the menu clamp is [1/3200, 1/200]; the menu shows the negated invert byte
        CHECK(MouseAccelFromPercent(50) == 0.0025f, "50%% -> 0.0025 (got %.7f)", MouseAccelFromPercent(50));
        CHECK(MouseAccelFromPercent(100) == 0.005f, "100%% -> 0.005");
        CHECK(MouseAccelFromPercent(0) == static_cast<float>(1.0 / 3200.0), "0%% clamps to the menu minimum 1/3200");
        CHECK(MouseAccelFromPercent(1000) == 0.005f, "above 100%% clamps to 0.005");
        const int inv = FindSetting("mouse", "invertY"), sens = FindSetting("Mouse", "Sensitivity");
        CHECK(inv >= 0 && sens >= 0, "FindSetting is case-insensitive");
        if (inv >= 0 && sens >= 0) {
            CHECK(ToStored(kSettings[inv], 1) == 0 && ToStored(kSettings[inv], 0) == 1, "Invert Y: menu On -> byte 0, Off -> byte 1");
            CHECK(FromStored(kSettings[inv], 0) == 1 && FromStored(kSettings[inv], 1) == 0, "Invert Y byte back to menu state");
            CHECK(ToStored(kSettings[sens], 50) == 0.0025f && FromStored(kSettings[sens], 0.0025f) > 49.99 && FromStored(kSettings[sens], 0.0025f) < 50.01, "sensitivity round trip");
        }
    }

    // 4c. [Memory]: raw writes
    {
        std::string msgs;
        IniData d = Parse("[Memory]\n"
                          "Fps = 0xC1704C u32 60 When=DISPLAY\n"
                          "Hex = 0x8CD800 float 1.5\n"
                          "Neg = 5000000 i16 -3\n"
                          "Byte = 0xBA6745 byte 0x7F When=MOUSE,AUDIO OnlyIfLayout=QWERTZ\n"
                          "Low = 0x1000 i32 1\n"
                          "High = 0x2000000 i32 1\n"
                          "Big = 0xC1704C u8 300\n"
                          "NoType = 0xC1704C wat 1\n"
                          "Short = 0xC1704C i32\n"
                          "BadWhen = 0xC1704C i32 1 When=SOMETIMES\n"
                          "NotAnInt = 0xC1704C i32 1.5x\n"
                          "WrongOpt = 0xC1704C i32 1 ClearDuplicates\n", &msgs);
        CHECK(d.rules.size() == 4, "memory rule count %u\n%s", (unsigned)d.rules.size(), msgs.c_str());
        if (d.rules.size() == 4) {
            CHECK(d.rules[0].kind == Rule::Memory && d.rules[0].memAddr == 0xC1704C && d.rules[0].memType == MemType::U32 && d.rules[0].value == 60 && d.rules[0].when == SCOPE_DISPLAY, "Fps rule");
            CHECK(d.rules[1].memType == MemType::Float && d.rules[1].value == 1.5 && d.rules[1].when == SCOPE_ALL, "float rule, default When=ALL");
            CHECK(d.rules[2].memType == MemType::I16 && d.rules[2].value == -3 && d.rules[2].memAddr == 5000000, "negative i16, decimal address");
            CHECK(d.rules[3].memType == MemType::U8 && d.rules[3].value == 127 && d.rules[3].when == (SCOPE_MOUSE | SCOPE_AUDIO) && d.rules[3].layouts == LAYOUT_QWERTZ, "byte with When list and layout");
        }
        const char* expect[] = { "address must be", "value '300' is not valid", "unknown type 'wat'", "needs ADDRESS TYPE VALUE", "bad When value",
                                 "value '1.5x' is not valid", "only for [Keys]" };
        for (const char* e : expect) CHECK(msgs.find(e) != std::string::npos, "missing parser message: %s", e);
        unsigned char b[8];
        CHECK(EncodeMem(MemType::U32, 60, b) && b[0] == 60 && b[1] == 0 && b[3] == 0 && DecodeMem(MemType::U32, b) == 60, "u32 round trip");
        CHECK(EncodeMem(MemType::I16, -3, b) && b[0] == 0xFD && b[1] == 0xFF && DecodeMem(MemType::I16, b) == -3, "i16 round trip");
        CHECK(EncodeMem(MemType::Float, 1.5, b) && DecodeMem(MemType::Float, b) == 1.5, "float round trip");
        CHECK(EncodeMem(MemType::Double, 0.25, b) && DecodeMem(MemType::Double, b) == 0.25, "double round trip");
        CHECK(EncodeMem(MemType::U32, 4294967295.0, b) && DecodeMem(MemType::U32, b) == 4294967295.0, "u32 max");
        CHECK(!EncodeMem(MemType::I8, 128, b) && !EncodeMem(MemType::U16, -1, b), "range checks");
    }

    // 4a2. the settings table itself
    {
        for (int i = 0; i < kNumSettings; ++i) {
            const SettingInfo& si = kSettings[i];
            CHECK(si.size == 1 || si.size == 4, "%s size", si.name);
            CHECK(si.addr != 0 || si.after != After::None, "%s has neither storage nor an apply step", si.name);
            CHECK(si.min <= si.max, "%s range", si.name);
            if (si.conv == Conv::Raw) CHECK(si.stock >= si.min && si.stock <= si.max, "%s: stock %g outside %g..%g", si.name, si.stock, si.min, si.max);
            if (si.type == VType::Bool) CHECK(si.min == 0 && si.max == 1, "%s bool range", si.name);
            if (si.names) {
                int n = 1; for (const char* c = si.names; *c; ++c) if (*c == ',') ++n;
                CHECK(n == (int)(si.max - si.min) + 1, "%s: %d names for range %g..%g", si.name, n, si.min, si.max);
            }
            for (int j = i + 1; j < kNumSettings; ++j)
                CHECK(std::strcmp(si.name, kSettings[j].name) != 0, "duplicate setting name %s", si.name);
            if (si.addr && si.size == 4) CHECK(si.type != VType::Bool, "%s: a bool is a byte", si.name);
        }
    }

    // 4b. layout lists
    {
        std::string msgs;
        IniData d = Parse("[Keys]\nGO_BACK=S OnlyIfLayout=QWERTZ,AZERTY\nGO_BACK=S OnlyIfLayout=qwerty|azerty\nGO_BACK=S OnlyIfLayout=ANY\n"
                          "GO_BACK=S OnlyIfLayout=QWERTZ,BOGUS\n", &msgs);
        CHECK(d.rules.size() == 3, "layout list rule count %u", (unsigned)d.rules.size());
        if (d.rules.size() == 3) {
            CHECK(d.rules[0].layouts == (LAYOUT_QWERTZ | LAYOUT_AZERTY), "QWERTZ,AZERTY");
            CHECK(d.rules[1].layouts == (LAYOUT_QWERTY | LAYOUT_AZERTY), "qwerty|azerty");
            CHECK(d.rules[2].layouts == LAYOUT_ANY, "ANY");
        }
        CHECK(msgs.find("bad OnlyIfLayout") != std::string::npos, "bad list entry reported");
    }

    // 5. table sanity: unique ids/names, ids in range, 53 absent
    {
        for (int i = 0; i < kNumActionInfos; ++i) {
            CHECK(kActions[i].id >= 0 && kActions[i].id < NUM_ACTIONS && kActions[i].id != 53, "id %d", kActions[i].id);
            for (int j = i + 1; j < kNumActionInfos; ++j) {
                CHECK(kActions[i].id != kActions[j].id, "dup id %d", kActions[i].id);
                CHECK(std::strcmp(kActions[i].name, kActions[j].name) != 0, "dup name %s", kActions[i].name);
            }
            unsigned c;
            CHECK(ParseKey(kActions[i].stock0, c) && ParseKey(kActions[i].stock1, c), "stock of %s", kActions[i].name);
        }
    }

    std::printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
    return g_fail ? 1 : 0;
}
