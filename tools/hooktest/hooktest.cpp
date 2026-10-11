// Emulated-game test of DefaultSettings.SA.asi (NOT the real game).
//
// Maps a fake "gta_sa" memory image at the real addresses (probe dword, E8 call sites, ControlsManager,
// bInvertMouseY) with fake game functions that implement the semantics documented by the community
// (ResetSettingOrder / SetControllerKeyAssociatedWithAction / ClearSettingsAssociatedWithAction).
// Then loads the real .asi and drives: startup (ReinitControls deferred, first menu draw), and the
// "restore defaults" sequence (SetDefaultPreferences, ReinitControls). It checks the hook mechanics
// (calling conventions, stack cleanup, unhooking) and the rule logic. It cannot prove the real game
// behaves like the model - that is what the in-game test plan in the README is for.
//
//   hooktest.exe <path-to-asi> <shipped|custom> <file-exists 0|1>
//   (build: tools\hooktest\run.bat)
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "../../src/actions.h"
#include "../../src/keys.h"
#include "../../src/settings.h"

// The loader maps this exe at 0x400000 (/BASE:0x400000 /FIXED) and the .fake section covers the rest of the
// game's address range, so nothing else can occupy it. The first function in .text sits at 0x401000 and is a
// never-called dummy: its first 4 bytes are overwritten with the v1.0 US probe dword.
#pragma optimize("", off)
__declspec(noinline) static void ProbeHolder() { volatile int x = 0; x = x + 1; x = x + 2; x = x + 3; }
#pragma optimize("", on)
static void (*volatile g_keepProbeHolder)() = &ProbeHolder;
#pragma warning(suppress : 4330)
#pragma section(".fake", read, write, execute)
__declspec(allocate(".fake")) static char g_fakeImage[0x850000];

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++g_fail; std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// ---- fake game state ------------------------------------------------------------------
struct Key { unsigned code, prio; };
struct Act { Key keys[4]; };
static Act&  Actn(int a) { return reinterpret_cast<Act*>(0xB70198 + 0xB70)[a]; }
static Key&  K(int a, int slot) { return Actn(a).keys[slot]; }
static volatile uint8_t& InvertByte() { return *reinterpret_cast<volatile uint8_t*>(0xBA6745); }
static volatile float&   SensValue() { return *reinterpret_cast<volatile float*>(0xB6EC1C); }

static bool Used(int t, const Key& k) { return t < 2 ? k.code != KEY_NULL : k.code != 0; }
static unsigned NumSettings(int a) { unsigned n = 0; for (int t = 0; t < 4; ++t) if (Used(t, K(a, t))) ++n; return n; }

static void ResetSettingOrder(int a) {                  // 0x52F5F0
    for (unsigned order = 1; order <= 4; ++order) {
        bool exists = false;
        for (int t = 0; t < 4; ++t) if (K(a, t).prio == order) exists = true;
        if (exists) continue;
        bool found = false; int result = 0;
        for (int t = 0; t < 4; ++t) {
            const unsigned p = K(a, t).prio;
            if (p > order && p != 0 && (!found || p < K(a, result).prio)) { result = t; found = true; }
        }
        if (found) K(a, result).prio = order;
    }
}
static void __fastcall F_Clear(void*, void*, int a, int t) {          // 0x52FD70 thiscall(action, type), ret 8
    K(a, t).code = t < 2 ? KEY_NULL : 0; K(a, t).prio = 0; ResetSettingOrder(a);
}
static void __fastcall F_Set(void*, void*, int a, unsigned key, int t) { // 0x530490 thiscall(action, key, type), ret 0xC
    ResetSettingOrder(a);
    const unsigned n = NumSettings(a);
    K(a, t).code = key; K(a, t).prio = n + 1;
}
static void __fastcall F_Reinit(void* self, void*) {                   // 0x531F20: clear all, set stock defaults
    for (int t = 0; t < 4; ++t) for (int a = 0; a < NUM_ACTIONS; ++a) F_Clear(self, nullptr, a, t);
    F_Set(self, nullptr, 0, 1, 2);                                     // a mouse button, so the priority count has company
    for (const ActionInfo& ai : kActions) {
        unsigned k;
        if (ParseKey(ai.stock0, k) && k != KEY_NULL) F_Set(self, nullptr, ai.id, k, 0);
        if (ParseKey(ai.stock1, k) && k != KEY_NULL) F_Set(self, nullptr, ai.id, k, 1);
    }
}
// Writes the stock value of every setting of a reset page into the fake game, like the game's reset code does.
static void WriteFakeStored(const SettingInfo& si, double v) {
    if (si.size == 1) *reinterpret_cast<volatile uint8_t*>(si.addr) = static_cast<uint8_t>(v);
    else if (si.type == VType::Float) *reinterpret_cast<volatile float*>(si.addr) = static_cast<float>(v);
    else *reinterpret_cast<volatile int32_t*>(si.addr) = static_cast<int32_t>(v);
}
static double ReadFakeStored(const SettingInfo& si) {
    if (si.size == 1) return *reinterpret_cast<volatile uint8_t*>(si.addr);
    if (si.type == VType::Float) return *reinterpret_cast<volatile float*>(si.addr);
    return *reinterpret_cast<volatile int32_t*>(si.addr);
}
static void ResetScope(unsigned scope) {
    for (const SettingInfo& si : kSettings)
        if ((si.scope & scope) && si.addr) WriteFakeStored(si, ToStored(si, si.stock));
}
static char __fastcall F_SetDef(void*, void*, int page) {              // 0x573AE0 thiscall(page), ret 4
    if (page == 3) ResetScope(SCOPE_AUDIO);
    else if (page == 4 || page == 27) { ResetScope(SCOPE_DISPLAY); *reinterpret_cast<volatile float*>(0x8CD800) = 1.2f; }
    else if (page == 36) ResetScope(SCOPE_MOUSE);
    return 0;
}
// the game's apply functions: just record the argument
static float g_gamma = -1; static int g_fx = -1, g_music = -1, g_sfx = -1, g_bass = -1, g_auto = -1;
static void __fastcall F_Gamma(void*, void*, float v, bool) { g_gamma = v; }                // 0x747200
static void __fastcall F_Fx(void*, void*, int v)            { g_fx = v; }                    // 0x49EA40
static void __fastcall F_Music(void*, void*, int v)         { g_music = v; }                 // 0x506DE0
static void __fastcall F_Sfx(void*, void*, int v)           { g_sfx = v; }                   // 0x506E10
static void __fastcall F_Bass(void*, void*, int v)          { g_bass = v; }                  // 0x506F90
static void __fastcall F_Auto(void*, void*, int v)          { g_auto = v; }                  // 0x506F80
static const SettingInfo& S(const char* section, const char* name) { return kSettings[FindSetting(section, name)]; }
static void __fastcall F_Menu(void*, void*) {}                         // 0x57C290
static void __cdecl    F_Tick() {}
// "another plugin" (SilentPatch, widescreen fixes, ...) that already redirected a call site and chains to the original
static int g_wrapperCalls = 0;
static void __fastcall W_Menu(void* self, void*)   { ++g_wrapperCalls; F_Menu(self, nullptr); }
static void __fastcall W_Reinit(void* self, void*) { ++g_wrapperCalls; F_Reinit(self, nullptr); }                                     // game process tick target

// ---- fake image -----------------------------------------------------------------------
static void Stub(uintptr_t at, void* fn) {                             // jmp rel32
    *reinterpret_cast<uint8_t*>(at) = 0xE9;
    *reinterpret_cast<int32_t*>(at + 1) = static_cast<int32_t>(reinterpret_cast<uintptr_t>(fn) - (at + 5));
}
static void PutCall(uintptr_t site, uintptr_t target) {
    *reinterpret_cast<uint8_t*>(site) = 0xE8;
    *reinterpret_cast<int32_t*>(site + 1) = static_cast<int32_t>(target - (site + 5));
}
static uintptr_t Target(uintptr_t site) { return site + 5 + *reinterpret_cast<int32_t*>(site + 1); }

constexpr uintptr_t CTRL = 0xB70198, MENU = 0xBA6748;
static void CallReinit(uintptr_t site) { reinterpret_cast<void(__thiscall*)(void*)>(Target(site))(reinterpret_cast<void*>(CTRL)); }
static void CallMenu(uintptr_t site)   { reinterpret_cast<void(__thiscall*)(void*)>(Target(site))(reinterpret_cast<void*>(MENU)); }
static void CallSetDef(int page)       { reinterpret_cast<char(__thiscall*)(void*, int)>(Target(0x57D273))(reinterpret_cast<void*>(MENU), page); }

static bool OrdersValid() {
    for (int a = 0; a < NUM_ACTIONS; ++a) {
        unsigned n = NumSettings(a), seen = 0;
        for (int t = 0; t < 4; ++t) {
            if (!Used(t, K(a, t))) { if (K(a, t).prio != 0) return false; continue; }
            const unsigned p = K(a, t).prio;
            if (p < 1 || p > n || (seen & (1u << p))) return false;
            seen |= 1u << p;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // keep the output of a crashing scenario
    if (argc < 4) { std::printf("usage: hooktest <asi> <shipped|custom> <file-exists 0|1> [chained]\n"); return 2; }
    const bool chained = argc > 4 && std::string(argv[4]) == "chained";
    const bool azerty = argc > 4 && std::string(argv[4]) == "azerty";   // plugin test build forces the AZERTY layout
    const std::string asi = argv[1];
    const bool custom = std::string(argv[2]) == "custom";
    const bool fileExists = argv[3][0] == '1';
    std::printf("scenario: ini=%s, gta_sa.set %s%s\n", argv[2], fileExists ? "EXISTS" : "missing",
                chained ? ", call sites pre-redirected by another plugin" : azerty ? ", layout forced to AZERTY" : "");

    {
        const uintptr_t lo = reinterpret_cast<uintptr_t>(g_fakeImage), hi = lo + sizeof(g_fakeImage);
        if (reinterpret_cast<uintptr_t>(&ProbeHolder) != 0x401000 || lo > 0x52F000 || hi < 0xBA8000) {
            std::printf("image layout unexpected (ProbeHolder %p, fake %p-%p): build with /BASE:0x400000 /FIXED\n",
                        reinterpret_cast<void*>(&ProbeHolder), reinterpret_cast<void*>(lo), reinterpret_cast<void*>(hi));
            return 2;
        }
        DWORD old;
        VirtualProtect(reinterpret_cast<void*>(0x401000), 16, PAGE_EXECUTE_READWRITE, &old);
        VirtualProtect(g_fakeImage, sizeof(g_fakeImage), PAGE_EXECUTE_READWRITE, &old);   // the linker drops the section's write flag
    }
    *reinterpret_cast<uint32_t*>(0x401000) = 0x53EC8B55;               // v1.0 US compact probe
    Stub(0x531F20, reinterpret_cast<void*>(&F_Reinit));
    Stub(0x573AE0, reinterpret_cast<void*>(&F_SetDef));
    Stub(0x52FD70, reinterpret_cast<void*>(&F_Clear));
    Stub(0x530490, reinterpret_cast<void*>(&F_Set));
    Stub(0x57C290, reinterpret_cast<void*>(&F_Menu));
    Stub(0x540000, reinterpret_cast<void*>(&F_Tick));
    Stub(0x747200, reinterpret_cast<void*>(&F_Gamma)); Stub(0x49EA40, reinterpret_cast<void*>(&F_Fx));
    Stub(0x506DE0, reinterpret_cast<void*>(&F_Music)); Stub(0x506E10, reinterpret_cast<void*>(&F_Sfx));
    Stub(0x506F90, reinterpret_cast<void*>(&F_Bass)); Stub(0x506F80, reinterpret_cast<void*>(&F_Auto));
    PutCall(0x53E981, 0x540000);
    PutCall(0x53E82D, 0x57C290); PutCall(0x53EB8C, 0x57C290);
    PutCall(0x57D298, 0x531F20); PutCall(0x7489C6, 0x531F20);
    PutCall(0x57D273, 0x573AE0);
    if (chained) {   // another plugin got there first
        PutCall(0x53E82D, reinterpret_cast<uintptr_t>(&W_Menu));   PutCall(0x53EB8C, reinterpret_cast<uintptr_t>(&W_Menu));
        PutCall(0x57D298, reinterpret_cast<uintptr_t>(&W_Reinit));
        PutCall(0x7489C6, reinterpret_cast<uintptr_t>(&W_Reinit));
    }

    // game boot, before the plugin exists: stock defaults
    CallReinit(0x7489C6);
    InvertByte() = 0;
    CHECK(K(13, 0).code == ' ' && K(12, 0).code == 1046 && K(12, 1).code == 1050, "model stock: sprint Space, jump LShift/RCtrl");
    CHECK(OrdersValid(), "model stock priorities valid");

    HMODULE m = LoadLibraryA(asi.c_str());
    CHECK(m != nullptr, "LoadLibrary failed (%lu)", GetLastError());
    if (!m) return 1;
    const uintptr_t reinitNow = chained ? reinterpret_cast<uintptr_t>(&W_Reinit) : 0x531F20;
    const uintptr_t menuNow   = chained ? reinterpret_cast<uintptr_t>(&W_Menu) : 0x57C290;
    CHECK(Target(0x7489C6) != reinitNow && Target(0x57D298) != reinitNow, "ReinitControls sites hooked");
    CHECK(Target(0x53E981) != 0x540000 && Target(0x53E82D) != menuNow && Target(0x53EB8C) != menuNow, "startup sites hooked");
    CHECK(Target(0x57D273) != 0x573AE0, "SetDefaultPreferences site hooked");

    const unsigned sc = MapVirtualKeyA('Y', MAPVK_VK_TO_VSC);
    const bool qwertz = sc == 0x2C, qwerty = sc == 0x15;

    // --- startup -----------------------------------------------------------------------
    std::printf(" startup\n");
    CallReinit(0x7489C6);                                              // game boot reinit: must be deferred
    CHECK(K(13, 0).code == ' ' && K(12, 0).code == 1046, "ReinitControls during startup must not apply rules");
    CallMenu(0x53E82D);                                                // first menu draw -> startup check
    CHECK(Target(0x53E981) == 0x540000, "tick hook removed after the check");
    CHECK(Target(0x53E82D) == (chained ? reinterpret_cast<uintptr_t>(&W_Menu) : 0x57C290) &&
          Target(0x53EB8C) == (chained ? reinterpret_cast<uintptr_t>(&W_Menu) : 0x57C290), "menu hooks removed, other plugin's redirect restored");
    if (fileExists) {
        CHECK(K(13, 0).code == ' ' && K(12, 0).code == 1046 && InvertByte() == 0, "gta_sa.set exists: nothing may change");
    } else if (!custom) {
        CHECK(K(13, 0).code == 1046 && K(12, 0).code == ' ', "swap applied at startup (sprint LShift, jump Space), got sprint=%u jump=%u", K(13, 0).code, K(12, 0).code);
        CHECK(InvertByte() == 1, "InvertY=0 (menu Off) -> byte 1, got %u", (unsigned)InvertByte());
        CHECK(SensValue() == 0.0f, "the shipped INI does not set a sensitivity (got %.7f)", (double)SensValue());
    } else {
        CHECK(SensValue() == 0.004f, "Sensitivity=80 applied at startup -> 0.004 (got %.7f)", (double)SensValue());
    }
    CHECK(OrdersValid(), "priorities valid after startup");
    auto checkDisplay = [&](const char* when) {
        CHECK(ReadFakeStored(S("Display", "Brightness")) == 300 && g_gamma == 300 / 512.0f, "%s: Brightness=300 stored and SetGamma(300/512) called (got %g / %g)", when, ReadFakeStored(S("Display", "Brightness")), (double)g_gamma);
        CHECK(ReadFakeStored(S("Display", "DrawDistance")) == 1.5f && *reinterpret_cast<volatile float*>(0x8CD800) == 1.5f, "%s: DrawDistance=1.5 stored and written to the LOD scale", when);
        CHECK(g_fx == 3, "%s: FxQuality=VERY_HIGH -> SetFxQuality(3) (got %d)", when, g_fx);
        CHECK(ReadFakeStored(S("Display", "FrameLimiter")) == 0 && ReadFakeStored(S("Display", "Hud")) == 0, "%s: FrameLimiter=0, Hud=0", when);
        CHECK(ReadFakeStored(S("Display", "RadarMode")) == 2, "%s: RadarMode=OFF -> 2", when);
    };
    auto checkAudio = [&](const char* when) {
        CHECK(ReadFakeStored(S("Audio", "SfxVolume")) == 32 && g_sfx == 32, "%s: SfxVolume=32 stored and applied (got %g / %d)", when, ReadFakeStored(S("Audio", "SfxVolume")), g_sfx);
        CHECK(ReadFakeStored(S("Audio", "RadioVolume")) == 10 && g_music == 10, "%s: RadioVolume=10 stored and applied", when);
        CHECK(ReadFakeStored(S("Audio", "RadioEq")) == 0 && g_bass == 0, "%s: RadioEq=0 stored and applied", when);
        CHECK(ReadFakeStored(S("Audio", "RadioAutoSelect")) == 0 && g_auto == 0, "%s: RadioAutoSelect=0 stored and applied", when);
        CHECK(ReadFakeStored(S("Audio", "RadioMode")) == 1, "%s: RadioMode=RANDOM -> 1", when);
    };
    auto checkMouse = [&](const char* when) {
        CHECK(ReadFakeStored(S("Gamepad", "InvertY1")) == 1 && ReadFakeStored(S("Mouse", "MouseSteering")) == 1, "%s: InvertY1=1, MouseSteering=1", when);
    };
    if (custom && !fileExists) {
        checkDisplay("startup"); checkAudio("startup"); checkMouse("startup");
        CHECK(SensValue() == 0.004f, "Sensitivity=80 applied at startup -> 0.004 (got %.7f)", (double)SensValue());
    }

    // --- restore defaults --------------------------------------------------------------
    std::printf(" restore defaults\n");
    InvertByte() = 0;
    CallSetDef(4);                                                     // display page: mouse rules must NOT run
    CHECK(InvertByte() == 0, "SetDefaultPreferences(4) must not touch bInvertMouseY");
    InvertByte() = 1;                                                  // user had changed it
    CallSetDef(36);                                                    // fake game sets 0 (stock), plugin applies InvertY
    CHECK(InvertByte() == (custom ? 0 : 1), "after page-36 reset byte should be %d, got %u", custom ? 0 : 1, (unsigned)InvertByte());
    if (custom) CHECK(SensValue() == 0.004f, "Sensitivity=80 -> 0.004 after the page-36 reset (got %.7f)", (double)SensValue());
    CallReinit(0x57D298);                                              // the game follows with ReinitControls
    CHECK(OrdersValid(), "priorities valid after restore defaults");
    if (!custom) {
        CHECK(K(13, 0).code == 1046 && K(12, 0).code == ' ', "swap applied by restore defaults");
        CHECK(K(12, 1).code == 1050 && K(12, 1).prio >= 1, "jump slot1 stays Right Ctrl");
        CHECK(K(13, 0).prio >= 1 && K(13, 0).prio <= 4, "sprint priority valid (%u)", K(13, 0).prio);
        if (azerty) {   // the game's French movement keys, applied by the AZERTY rules
            CHECK(K(4, 1).code == 'Z' && K(6, 1).code == 'Q' && K(3, 0).code == 'A' && K(9, 1).code == 'W', "AZERTY on foot: forward Z, left Q, cycle-left A, zoom-out W (got %s %s %s %s)",
                  KeyToName(K(4, 1).code).c_str(), KeyToName(K(6, 1).code).c_str(), KeyToName(K(3, 0).code).c_str(), KeyToName(K(9, 1).code).c_str());
            CHECK(K(20, 0).code == 'Q' && K(24, 0).code == 'Z' && K(34, 0).code == 'A' && K(30, 0).code == 233, "AZERTY in vehicle: steer-left Q, accelerate Z, look-left A, submissions 233");
            CHECK(K(5, 1).code == 'S' && K(7, 1).code == 'D', "AZERTY: S and D untouched");
        } else if (!custom) {   // QWERTZ/QWERTY machine: the AZERTY rules must be skipped
            CHECK(K(4, 1).code == 'W' && K(6, 1).code == 'A' && K(3, 0).code == 'Q' && K(24, 0).code == 'W', "non-AZERTY layout: AZERTY rules skipped, stock WASD kept");
        }
    } else {
        CHECK(K(15, 0).code == 1046, "PED_DUCK = LSHIFT");
        CHECK(K(12, 0).code == KEY_NULL && K(12, 0).prio == 0, "ClearDuplicates unbound PED_JUMPING slot0 (code %u prio %u)", K(12, 0).code, K(12, 0).prio);
        CHECK(K(4, 0).code == KEY_NULL && K(4, 0).prio == 0 && K(4, 1).code == 'W' && K(4, 1).prio == 1, "GO_FORWARD slot0 NONE, slot1 W compacted to priority 1 (got %u/%u)", K(4, 1).code, K(4, 1).prio);
        CHECK(K(16, 1).code == 1005, "PED_ANSWER_PHONE.Slot1 = F5");
        CHECK(K(48, 0).code == (qwertz ? 89u : 78u), "OnlyIfLayout=QWERTZ (layout scancode 0x%02X): CONVERSATION_NO = %s", sc, KeyToName(K(48, 0).code).c_str());
        CHECK(K(49, 0).code == (qwerty ? 74u : 71u), "OnlyIfLayout=QWERTY: GROUP_CONTROL_FWD = %s", KeyToName(K(49, 0).code).c_str());
        CHECK(K(13, 0).code == ' ', "unparsable rule (PED_SPRINT=NOTAKEY) ignored");
        CHECK(K(34, 0).code == (qwertz ? 65u : 88u), "general rule then layout exception: VEHICLE_LOOKLEFT = %s (QWERTZ: A, else X)", KeyToName(K(34, 0).code).c_str());
    }

    if (custom) {
        std::printf(" per-page resets\n");
        *reinterpret_cast<volatile int32_t*>(0xBA6748 + 0x3C) = 111;       // a display value the user changed: an AUDIO reset must not touch it
        CallSetDef(3);
        CHECK(*reinterpret_cast<volatile int32_t*>(0xBA6748 + 0x3C) == 111, "SetDefaultPreferences(3) must not touch display settings");
        checkAudio("page 3");
        g_gamma = -1; g_fx = -1;
        *reinterpret_cast<volatile uint8_t*>(0xBA6748 + 0x4F) = 7;         // an audio value the user changed: a DISPLAY reset must not touch it
        CallSetDef(4);
        CHECK(*reinterpret_cast<volatile uint8_t*>(0xBA6748 + 0x4F) == 7, "SetDefaultPreferences(4) must not touch audio settings");
        checkDisplay("page 4");
        g_gamma = -1; g_fx = -1;
        CallSetDef(27);
        checkDisplay("page 27");
        {
            volatile uint32_t& fps = *reinterpret_cast<volatile uint32_t*>(0xB6F000);
            volatile float& always = *reinterpret_cast<volatile float*>(0xB6F010);
            CHECK(fps == 60 && always == 1.5f, "[Memory] rules applied (got %u / %g)", (unsigned)fps, (double)always);
            fps = 0; always = 0;
            CallSetDef(3);
            CHECK(fps == 0 && always == 1.5f, "[Memory] When=DISPLAY not run by the audio reset, When=ALL is");
            CallSetDef(4);
            CHECK(fps == 60, "[Memory] When=DISPLAY run by the display reset");
        }
        CallSetDef(36);
        checkMouse("page 36");
        CHECK(SensValue() == 0.004f, "Sensitivity=80 re-applied by the page-36 reset");
    }

    // a second restore must work the same (hooks stay installed all session)
    CallReinit(0x57D298);
    CHECK(OrdersValid(), "priorities valid after a second reset");
    if (!custom) CHECK(K(13, 0).code == 1046 && K(12, 0).code == ' ', "swap re-applied by the second reset");

    if (chained) CHECK(g_wrapperCalls >= 5, "the other plugin's wrappers must still be called through our hooks (calls: %d)", g_wrapperCalls);
    FreeLibrary(m);
    std::printf(g_fail ? " %d FAILED\n" : " ok\n", g_fail);
    return g_fail ? 1 : 0;
}
