// DefaultSettings.SA.asi - GTA San Andreas (v1.0 US, 32-bit, compact or Hoodlum exe)
//
// Changes the game's DEFAULT controls/settings through DefaultSettings.SA.ini
// without ever overriding the user's own choices. The INI is applied only at
// moments when the game is on its stock defaults:
//   (a) at startup, if Documents\GTA San Andreas User Files\gta_sa.set does not exist
//   (b) right after the game resets controls / mouse settings to defaults at runtime
//       (menu "restore defaults")
// A loaded gta_sa.set is never touched. No marker, no saved state.
//
// Struct layouts, enum values and the addresses 0xB70198, 0xBA6745, 0x53E981,
// 0x573AE0, 0x52F5F0 are documented game structures; everything else is flagged
// in README.md ("Verification status") and re-checked at runtime.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>

#include "actions.h"
#include "ini.h"
#include "keys.h"
#include "settings.h"

constexpr const char* kVersion = "1.0.0";

// ---------------------------------------------------------------------------
// Game definitions
// ---------------------------------------------------------------------------
namespace game {

// CControllerKey / CControllerAction
struct ControllerKey {
    unsigned int keyCode;   // RsKeyCodes value; rsNULL (1056) = unbound
    unsigned int priority;  // "set order": 0 = none, 1..4 (eContSetOrder)
};
struct ControllerAction {
    ControllerKey keys[4];  // indexed by eControllerType
};
struct ControllerConfigManager {
    char             before[0xB70];
    ControllerAction m_actions[NUM_ACTIONS];
    char             after[0x12E4 - 0xB70 - NUM_ACTIONS * 0x20];
};
static_assert(sizeof(ControllerKey) == 0x8, "size of CControllerKey");
static_assert(sizeof(ControllerAction) == 0x20, "size of CControllerAction");
static_assert(offsetof(ControllerConfigManager, m_actions) == 0xB70, "m_actions at 0xB70");
static_assert(sizeof(ControllerConfigManager) == 0x12E4, "size of CControllerConfigManager");

// eControllerType
constexpr int CONTROLLER_KEYBOARD1 = 0;
constexpr int CONTROLLER_KEYBOARD2 = 1;
// e_ControllerAction
constexpr int GO_FORWARD       = 4;
constexpr int CONVERSATION_NO  = 48;
// CMenuManager: MENUPAGE_CONTROLLER_PC = 36
constexpr int MENUPAGE_AUDIO_SETTINGS = 3;
constexpr int MENUPAGE_DISPLAY_SETTINGS = 4;
constexpr int MENUPAGE_DISPLAY_ADVANCED = 27;
constexpr int MENUPAGE_CONTROLLER_PC = 36;

constexpr uintptr_t ADDR_ControlsManager   = 0xB70198; // ControlsManager
// game functions that apply settings (found in the game's reset-to-defaults code); see settings.h / docs\SETTINGS.md
constexpr uintptr_t ADDR_Gamma             = 0xC92134; // gamma object; SetGamma(float value / 512, bool) is thiscall at 0x747200
constexpr uintptr_t ADDR_SetGamma          = 0x747200;
constexpr uintptr_t ADDR_LodScale          = 0x8CD800; // float, also set when the draw distance changes
constexpr uintptr_t ADDR_FxSystem          = 0xA9AE00; // SetFxQuality(int) thiscall at 0x49EA40
constexpr uintptr_t ADDR_SetFxQuality      = 0x49EA40;
constexpr uintptr_t ADDR_AudioEngine       = 0xB6BC90; // thiscall(int) setters below
constexpr uintptr_t ADDR_SetMusicVolume    = 0x506DE0;
constexpr uintptr_t ADDR_SetSfxVolume      = 0x506E10;
constexpr uintptr_t ADDR_SetBassEnhance    = 0x506F90;
constexpr uintptr_t ADDR_SetRadioAutoRetune = 0x506F80;
constexpr uintptr_t ADDR_GameProcessCall   = 0x53E981; // game process event call site
constexpr uintptr_t ADDR_SetDefaultPrefs   = 0x573AE0; // CMenuManager::SetDefaultPreferences(char page), thiscall
constexpr uintptr_t ADDR_ResetSettingOrder = 0x52F5F0; // CControllerConfigManager::ResetSettingOrder
constexpr uintptr_t ADDR_VersionProbe      = 0x401000; // v1.0 US: compact exe 0x53EC8B55, Hoodlum exe 0x16197BE9
// from analysing the game exe / community documentation, re-checked at runtime:
constexpr uintptr_t ADDR_MenuDraw          = 0x57C290; // menu draw callee
constexpr uintptr_t ADDR_MenuDrawCalls[]   = { 0x53E82D, 0x53EB8C };
constexpr uintptr_t ADDR_ReinitControls    = 0x531F20; // CControllerConfigManager::ReinitControls (thiscall)
constexpr uintptr_t ADDR_ReinitCalls[]     = { 0x57D298, 0x7489C6 };   // (0x57330E, in the language-change code, is only reached with reInitControls = true; no caller passes that)
constexpr uintptr_t ADDR_ClearSettings     = 0x52FD70; // ClearSettingsAssociatedWithAction(action, type), thiscall, ret 8
constexpr uintptr_t ADDR_SetKey            = 0x530490; // SetControllerKeyAssociatedWithAction(action, key, type), thiscall, ret 0xC
constexpr uintptr_t ADDR_SetDefaultCall    = 0x57D273; // menu "restore defaults": SetDefaultPreferences(page of the confirmed screen), followed by ReinitControls (0x57D298) for page 36

inline ControllerConfigManager& Controls() { return *reinterpret_cast<ControllerConfigManager*>(ADDR_ControlsManager); }

using ClearFn = void(__thiscall*)(void*, int action, int type);
using SetKeyFn = void(__thiscall*)(void*, int action, unsigned key, int type);

} // namespace game

// ---------------------------------------------------------------------------
// Logging (DefaultSettings.SA.log next to the .asi)
// ---------------------------------------------------------------------------
static HMODULE     g_module = nullptr;
static std::string g_dir;          // directory of the .asi, with trailing backslash
static HANDLE      g_log = INVALID_HANDLE_VALUE;

static void LogRaw(const std::string& s) {
    if (g_log == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME t; GetLocalTime(&t);
    char stamp[48];
    _snprintf_s(stamp, sizeof(stamp), _TRUNCATE, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    std::string line = stamp + s + "\r\n";
    DWORD written;
    WriteFile(g_log, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
}

static void Log(const char* fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    LogRaw(buf);
}

static void OpenLog() {
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(g_module, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    g_dir.assign(path, n);
    size_t slash = g_dir.find_last_of("\\/");
    g_dir.erase(slash == std::string::npos ? 0 : slash + 1);
    g_log = CreateFileA((g_dir + "DefaultSettings.SA.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

// ---------------------------------------------------------------------------
// Environment checks
// ---------------------------------------------------------------------------

// CMenuManager::LoadSettings only reads gta_sa.set and never creates it, so a
// missing file at startup means the game runs on its built-in defaults.
// Fail safe: if the path cannot be resolved, assume the file exists.
static bool SettingsFileExists() {
#ifdef DA_TEST_SETTINGS_FILE   // test builds only (tools\hooktest): check this path instead of Documents
    return GetFileAttributesA(DA_TEST_SETTINGS_FILE) != INVALID_FILE_ATTRIBUTES;
#endif
    char docs[MAX_PATH], path[MAX_PATH];
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs))) return true;
    _snprintf_s(path, MAX_PATH, _TRUNCATE, "%s\\GTA San Andreas User Files\\gta_sa.set", docs);
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

// Keyboard layout from the scancodes of the keys with virtual keys 'Y' and 'A':
//   QWERTZ: 'Y' at 0x2C   AZERTY: 'A' at 0x10 (where QWERTY has Q)   QWERTY: 'Y' at 0x15 (and 'A' at 0x1E)
// Returns a LayoutBit, or 0 for any other layout (such a layout only matches rules without OnlyIfLayout / ANY).
static unsigned CurrentLayout(unsigned& scY, unsigned& scA) {
    scY = MapVirtualKeyA('Y', MAPVK_VK_TO_VSC);
    scA = MapVirtualKeyA('A', MAPVK_VK_TO_VSC);
#ifdef DA_TEST_LAYOUT   // test builds only (tools\hooktest): force the layout
    return DA_TEST_LAYOUT;
#endif
    if (scY == 0x2C) return LAYOUT_QWERTZ;
    if (scA == 0x10) return LAYOUT_AZERTY;
    if (scY == 0x15 && scA == 0x1E) return LAYOUT_QWERTY;
    return 0;
}

static const char* LayoutName(unsigned bit) {
    return bit == LAYOUT_QWERTZ ? "QWERTZ" : bit == LAYOUT_AZERTY ? "AZERTY" : bit == LAYOUT_QWERTY ? "QWERTY" : "other";
}

static bool LayoutMatches(unsigned ruleMask, unsigned current) { return ruleMask == LAYOUT_ANY || (ruleMask & current) != 0; }

// ---------------------------------------------------------------------------
// INI (loaded once, lazily, on the game thread)
// ---------------------------------------------------------------------------
static IniData g_ini;
static bool    g_iniLoaded = false;   // load attempted
static bool    g_iniUsable = false;   // INI found and parsed

static void LoadIniOnce() {
    if (g_iniLoaded) return;
    g_iniLoaded = true;

    const std::string path = g_dir + "DefaultSettings.SA.ini";
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        Log("INI not found (%s): nothing to apply.", path.c_str());
        return;
    }
    std::string content;
    char buf[4096]; DWORD got = 0;
    while (ReadFile(f, buf, sizeof(buf), &got, nullptr) && got) content.append(buf, got);
    CloseHandle(f);

    ParseIni(content, g_ini, [](const std::string& m) { LogRaw("INI: " + m); });
    g_iniUsable = true;

    size_t nKey = 0, nSetting = 0;
    for (const Rule& r : g_ini.rules) (r.kind == Rule::Key ? nKey : nSetting)++;
    Log("INI loaded: %s (%u key rules, %u setting rules)", path.c_str(), static_cast<unsigned>(nKey), static_cast<unsigned>(nSetting));
}

// ---------------------------------------------------------------------------
// Game access (every raw memory / game call is SEH guarded; POD-only functions)
// ---------------------------------------------------------------------------
static bool GameClear(int action, int type) {
    __try {
        reinterpret_cast<game::ClearFn>(game::ADDR_ClearSettings)(&game::Controls(), action, type);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool GameSetKey(int action, unsigned key, int type) {
    __try {
        reinterpret_cast<game::SetKeyFn>(game::ADDR_SetKey)(&game::Controls(), action, key, type);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool ReadSlot(int action, int slot, unsigned& key, unsigned& prio) {
    __try {
        const game::ControllerKey& k = game::Controls().m_actions[action].keys[slot];
        key = k.keyCode; prio = k.priority;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Stored value of a setting as a number (byte, int32 or float).
static bool ReadStored(const SettingInfo& si, double& v) {
    __try {
        if (si.size == 1) v = *reinterpret_cast<volatile uint8_t*>(si.addr);
        else if (si.type == VType::Float) v = *reinterpret_cast<volatile float*>(si.addr);
        else v = *reinterpret_cast<volatile int32_t*>(si.addr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool WriteStored(const SettingInfo& si, double v) {
    __try {
        if (si.size == 1) *reinterpret_cast<volatile uint8_t*>(si.addr) = static_cast<uint8_t>(v);
        else if (si.type == VType::Float) *reinterpret_cast<volatile float*>(si.addr) = static_cast<float>(v);
        else *reinterpret_cast<volatile int32_t*>(si.addr) = static_cast<int32_t>(v);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool WriteFloatAt(uintptr_t addr, float v) {
    __try { *reinterpret_cast<volatile float*>(addr) = v; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// thiscall(int) on a game object: the audio engine setters and the effects quality.
static bool CallThis1(uintptr_t thisPtr, uintptr_t fn, int arg) {
    __try { reinterpret_cast<void(__thiscall*)(void*, int)>(fn)(reinterpret_cast<void*>(thisPtr), arg); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool CallSetGamma(float v) {
    __try { reinterpret_cast<void(__thiscall*)(void*, float, bool)>(game::ADDR_SetGamma)(reinterpret_cast<void*>(game::ADDR_Gamma), v, true); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static std::string Slot(int action, int slot) {
    unsigned k = 0, p = 0;
    if (!ReadSlot(action, slot, k, p)) return "?";
    return KeyToName(k) + "(p" + std::to_string(p) + ")";
}

static const char* ActionName(int id) {
    for (const ActionInfo& a : kActions) if (a.id == id) return a.name;
    return "?";
}

// Full table of keyboard bindings: lets the user (and us) see the real stock defaults.
static void DumpBindings(const char* label) {
    Log("--- keyboard bindings %s (slot0 | slot1, p = priority) ---", label);
    for (const ActionInfo& a : kActions)
        Log("  %2d %-34s %-14s | %s", a.id, a.name, Slot(a.id, 0).c_str(), Slot(a.id, 1).c_str());
}

// ---------------------------------------------------------------------------
// Applying rules
// ---------------------------------------------------------------------------
static void ApplyKeyRule(const Rule& r, const std::set<int>& explicitSlots) {
    using namespace game;
    const int type = r.slot == 0 ? CONTROLLER_KEYBOARD1 : CONTROLLER_KEYBOARD2;
    const std::string before = Slot(r.action, r.slot);

    if (r.clearDuplicates && r.key != KEY_NULL) {
        for (int a = 0; a < NUM_ACTIONS; ++a) {
            if (a == r.action) continue;
            unsigned k = 0, p = 0;
            if (!ReadSlot(a, r.slot, k, p) || k != r.key) continue;
            if (explicitSlots.count(a * 4 + r.slot)) {
                Log("  NOTE line %d: %s slot%d also has %s but is set explicitly by the INI, not cleared", r.line, ActionName(a), r.slot, KeyToName(k).c_str());
                continue;
            }
            const bool ok = GameClear(a, type);
            Log("  cleared duplicate %s on %s slot%d%s", KeyToName(k).c_str(), ActionName(a), r.slot, ok ? "" : " (FAILED)");
        }
    }

    // Clear first so the game's priority bookkeeping (count of used slots) is exact.
    bool ok = GameClear(r.action, type);
    if (ok && r.key != KEY_NULL) ok = GameSetKey(r.action, r.key, type);

    unsigned k = 0, p = 0;
    const bool read = ReadSlot(r.action, r.slot, k, p);
    const bool valid = ok && read && k == r.key && (r.key == KEY_NULL ? p == 0 : (p >= 1 && p <= 4));
    if (!valid) {
        Log("FAILED  line %d [%s] %s: expected %s, slot is now %s (game call %s) - slot may be unbound, see log",
            r.line, r.section.c_str(), r.text.c_str(), KeyToName(r.key).c_str(), Slot(r.action, r.slot).c_str(), ok ? "ok" : "faulted");
        return;
    }
    Log("APPLIED line %d [%s] %s: %s slot%d %s -> %s", r.line, r.section.c_str(), r.text.c_str(),
        ActionName(r.action), r.slot, before.c_str(), Slot(r.action, r.slot).c_str());
}

// The step the game itself performs after the value changed (a gamma or audio call, ...). Returns false if it faulted.
static bool RunAfter(const SettingInfo& si, double stored) {
    using namespace game;
    switch (si.after) {
        case After::None:            return true;
        case After::Gamma:           return CallSetGamma(static_cast<float>(stored) / 512.0f);
        case After::DrawDistance:    return WriteFloatAt(ADDR_LodScale, static_cast<float>(stored));
        case After::FxQuality:       return CallThis1(ADDR_FxSystem, ADDR_SetFxQuality, static_cast<int>(stored));
        case After::MusicVolume:     return CallThis1(ADDR_AudioEngine, ADDR_SetMusicVolume, static_cast<int>(stored));
        case After::SfxVolume:       return CallThis1(ADDR_AudioEngine, ADDR_SetSfxVolume, static_cast<int>(stored));
        case After::BassEnhance:     return CallThis1(ADDR_AudioEngine, ADDR_SetBassEnhance, static_cast<int>(stored));
        case After::RadioAutoRetune: return CallThis1(ADDR_AudioEngine, ADDR_SetRadioAutoRetune, static_cast<int>(stored));
    }
    return true;
}

static void ApplySettingRule(const Rule& r) {
    const SettingInfo& si = kSettings[r.setting];
    double before = 0;
    if (si.addr && !ReadStored(si, before)) { Log("FAILED  line %d [%s] %s: cannot read the current value", r.line, r.section.c_str(), r.text.c_str()); return; }
    const double stored = ToStored(si, r.value);
    if (si.addr && !WriteStored(si, stored)) { Log("FAILED  line %d [%s] %s: cannot write the value", r.line, r.section.c_str(), r.text.c_str()); return; }
    const bool ran = RunAfter(si, stored);
    double after = stored;
    if (si.addr) ReadStored(si, after);
    if (si.addr)
        Log("APPLIED line %d [%s] %s: %s %g -> %g (stored %g -> %g)%s", r.line, r.section.c_str(), r.text.c_str(), si.name,
            FromStored(si, before), FromStored(si, after), before, after, ran ? "" : "  (the game function call FAILED)");
    else
        Log("APPLIED line %d [%s] %s: %s set to %g%s", r.line, r.section.c_str(), r.text.c_str(), si.name, r.value,
            ran ? "" : "  (the game function call FAILED)");
}

// Raw bytes at a game address; the write lifts page protection first so code and read-only data can be patched too.
static bool MemAccess(uintptr_t addr, unsigned char* buf, int n, bool write) {
    __try {
        if (!write) { memcpy(buf, reinterpret_cast<const void*>(addr), n); return true; }
        DWORD old;
        if (!VirtualProtect(reinterpret_cast<void*>(addr), n, PAGE_EXECUTE_READWRITE, &old)) return false;
        memcpy(reinterpret_cast<void*>(addr), buf, n);
        VirtualProtect(reinterpret_cast<void*>(addr), n, old, &old);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(addr), n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void ApplyMemoryRule(const Rule& r) {
    const int n = MemSize(r.memType);
    unsigned char before[8] = {}, want[8] = {}, after[8] = {};
    if (!MemAccess(r.memAddr, before, n, false)) { Log("FAILED  line %d [Memory] %s: cannot read 0x%08lX", r.line, r.text.c_str(), static_cast<unsigned long>(r.memAddr)); return; }
    EncodeMem(r.memType, r.value, want);
    if (!MemAccess(r.memAddr, want, n, true)) { Log("FAILED  line %d [Memory] %s: cannot write 0x%08lX", r.line, r.text.c_str(), static_cast<unsigned long>(r.memAddr)); return; }
    MemAccess(r.memAddr, after, n, false);
    Log("APPLIED line %d [Memory] %s: 0x%08lX %g -> %g", r.line, r.text.c_str(), static_cast<unsigned long>(r.memAddr),
        DecodeMem(r.memType, before), DecodeMem(r.memType, after));
}

static unsigned ScopeOf(const Rule& r) {
    if (r.kind == Rule::Key) return SCOPE_KEYS;
    return r.kind == Rule::Memory ? r.when : kSettings[r.setting].scope;
}

static void ApplyRules(const char* reason, unsigned scopes) {
    const bool keys = (scopes & SCOPE_KEYS) != 0;
    LoadIniOnce();
    Log("== defaults moment: %s ==", reason);
    if (!g_iniUsable) { Log("SKIPPED: no usable INI"); return; }

    unsigned scY = 0, scA = 0;
    const unsigned layout = CurrentLayout(scY, scA);
    Log("keyboard layout: %s (scancode of 'Y' = 0x%02X, of 'A' = 0x%02X)", LayoutName(layout), scY, scA);

    std::set<int> explicitSlots;   // slots set by an INI rule in this run: never cleared as a "duplicate"
    for (const Rule& r : g_ini.rules)
        if (r.kind == Rule::Key && LayoutMatches(r.layouts, layout)) explicitSlots.insert(r.action * 4 + r.slot);

    if (keys) DumpBindings("BEFORE (stock)");
    int applied = 0, considered = 0;
    std::set<int> appliedSlots;
    for (const Rule& r : g_ini.rules) {
        if (!(ScopeOf(r) & scopes)) continue;
        ++considered;
        if (!LayoutMatches(r.layouts, layout)) {
            Log("SKIPPED line %d [%s] %s: layout condition not met (current layout: %s)", r.line, r.section.c_str(),
                r.text.c_str(), LayoutName(layout));
            continue;
        }
        if (r.kind == Rule::Key) { ApplyKeyRule(r, explicitSlots); appliedSlots.insert(r.action * 4 + r.slot); }
        else if (r.kind == Rule::Memory) ApplyMemoryRule(r);
        else ApplySettingRule(r);
        ++applied;
    }
    if (considered == 0) Log("no rules for this moment");
    if (keys && applied) DumpBindings("AFTER");

    // Informational only, on the FINAL state (a swap passes through a transient overlap).
    // Some overlaps are stock (e.g. Space is also the vehicle handbrake).
    for (int id : appliedSlots) {
        const int a = id / 4, slot = id % 4;
        unsigned k = 0, p = 0;
        if (!ReadSlot(a, slot, k, p) || k == KEY_NULL) continue;
        for (int b = 0; b < NUM_ACTIONS; ++b) {
            unsigned k2 = 0, p2 = 0;
            if (b != a && ReadSlot(b, slot, k2, p2) && k2 == k)
                Log("NOTE: %s is bound to both %s and %s (slot%d); use ClearDuplicates if unwanted", KeyToName(k).c_str(), ActionName(a), ActionName(b), slot);
        }
    }
}

static void ApplyGuarded(const char* reason, unsigned scopes) {
    __try { ApplyRules(reason, scopes); }
    __except (EXCEPTION_EXECUTE_HANDLER) { Log("EXCEPTION 0x%08lX while applying (%s): stopped, game continues unmodified from here", GetExceptionCode(), reason); }
}

// ---------------------------------------------------------------------------
// Hooks (E8 call sites, chained to the original target)
// ---------------------------------------------------------------------------
static void PatchRel(uintptr_t site, int32_t rel) {
    uint8_t* p = reinterpret_cast<uint8_t*>(site) + 1;
    DWORD old;
    if (VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &old)) {
        memcpy(p, &rel, 4);
        VirtualProtect(p, 4, old, &old);
        FlushInstructionCache(GetCurrentProcess(), p, 4);
    }
}

// Current call target if `site` holds an E8 call, else 0. The target may already have been redirected by
// another plugin (SilentPatch, widescreen fixes, ... hook the same sites); we then chain to whatever is there.
static uintptr_t ReadCallTarget(uintptr_t site) {
    __try {
        if (*reinterpret_cast<uint8_t*>(site) != 0xE8) return 0;
        int32_t rel; memcpy(&rel, reinterpret_cast<void*>(site + 1), 4);
        return site + 5 + rel;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

struct Site {
    uintptr_t addr;
    int32_t   origRel = 0, hookRel = 0;
    uintptr_t orig = 0;       // call target before our hook (what we chain to)
    bool      hooked = false;
};

static bool HookSite(Site& s, uintptr_t expectedTarget, void* hook, const char* what) {
    uintptr_t target = ReadCallTarget(s.addr);
    if (!target) { Log("hook %s at 0x%X: not an E8 call, skipped", what, static_cast<unsigned>(s.addr)); return false; }
    if (expectedTarget && target != expectedTarget)
        Log("hook %s at 0x%X: already redirected by another plugin (0x%X instead of 0x%X), chaining to it", what,
            static_cast<unsigned>(s.addr), static_cast<unsigned>(target), static_cast<unsigned>(expectedTarget));
    memcpy(&s.origRel, reinterpret_cast<void*>(s.addr + 1), 4);
    s.hookRel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hook) - (s.addr + 5));
    PatchRel(s.addr, s.hookRel);
    s.hooked = true;
    s.orig = target;
    Log("hook %s at 0x%X -> original 0x%X", what, static_cast<unsigned>(s.addr), static_cast<unsigned>(target));
    return true;
}

static void UnhookSite(Site& s) {
    if (!s.hooked) return;
    int32_t cur; memcpy(&cur, reinterpret_cast<void*>(s.addr + 1), 4);
    if (cur == s.hookRel) PatchRel(s.addr, s.origRel);   // else another plugin re-patched it: leave it (we are then a no-op)
    s.hooked = false;
}

// 1) Startup check. Runs once, on whichever fires first, then all three are unhooked:
//    menu draw before / during a session (so the Controls menu already shows the result)
//    and the game process tick (in case the menu was skipped).
using ProcessFn = void(__cdecl*)();
using MenuFn    = void(__thiscall*)(void*);
static Site      g_processSite{ game::ADDR_GameProcessCall };
static Site      g_menuSites[] = { { game::ADDR_MenuDrawCalls[0] }, { game::ADDR_MenuDrawCalls[1] } };
static bool      g_finished = false;
static int       g_ticks = 0;
constexpr int    MAX_TICKS_WAITING_FOR_CONTROLS = 1200;

static bool ControlsReady() {
    unsigned k1 = 0, k2 = 0, p = 0;
    return ReadSlot(game::GO_FORWARD, 0, k1, p) && ReadSlot(game::CONVERSATION_NO, 0, k2, p) &&
           k1 != 0 && k1 != KEY_NULL && k2 != 0 && k2 != KEY_NULL;
}

static bool StartupCheck() {   // returns true when finished
    if (!ControlsReady()) {
        if (++g_ticks < MAX_TICKS_WAITING_FOR_CONTROLS) return false;
        Log("startup: controls never became ready, giving up (nothing applied)");
        return true;
    }
    if (SettingsFileExists()) {
        Log("startup: gta_sa.set exists -> user settings, nothing applied.");
    } else {
        ApplyGuarded("startup, gta_sa.set does not exist", SCOPE_ALL);
    }
    return true;
}

static void StartupTick() {
    if (g_finished) return;
    bool done = false;
    __try { done = StartupCheck(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("EXCEPTION in startup check, giving up"); done = true; }
    if (!done) return;
    g_finished = true;
    UnhookSite(g_processSite);
    for (Site& s : g_menuSites) UnhookSite(s);
}

static void __cdecl ProcessHook() { reinterpret_cast<ProcessFn>(g_processSite.orig)(); StartupTick(); }
template <int N> static void __fastcall MenuHook(void* self, void*) { reinterpret_cast<MenuFn>(g_menuSites[N].orig)(self); StartupTick(); }

// 2) ReinitControls: resets all controls to defaults. Stays hooked all session.
//    During startup it runs before gta_sa.set is loaded; we do NOT apply then
//    (deferred to the startup check, which looks at whether the file exists), so a
//    loaded file can never be affected.
using ReinitFn = void(__thiscall*)(void*);
static Site     g_reinitSites[2] = { { game::ADDR_ReinitCalls[0] }, { game::ADDR_ReinitCalls[1] } };

template <int N> static void __fastcall ReinitHook(void* self, void*) {
    reinterpret_cast<ReinitFn>(g_reinitSites[N].orig)(self);
    if (!g_finished) { Log("ReinitControls during startup: deferred to the startup check."); return; }
    ApplyGuarded("controls reset to defaults (ReinitControls)", SCOPE_KEYS);
}

// 3) SetDefaultPreferences(page): page 36 (controller) resets the mouse settings.
using SetDefFn = char(__thiscall*)(void*, int);
static Site     g_setDefSite{ game::ADDR_SetDefaultCall };

static char __fastcall SetDefHook(void* self, void*, int page) {
    char r = reinterpret_cast<SetDefFn>(g_setDefSite.orig)(self, page);
    if (g_finished) {
        switch (static_cast<char>(page)) {
            case game::MENUPAGE_CONTROLLER_PC:   ApplyGuarded("mouse/gamepad settings reset to defaults (page 36)", SCOPE_MOUSE); break;
            case game::MENUPAGE_AUDIO_SETTINGS:  ApplyGuarded("audio settings reset to defaults (page 3)", SCOPE_AUDIO); break;
            case game::MENUPAGE_DISPLAY_SETTINGS:
            case game::MENUPAGE_DISPLAY_ADVANCED: ApplyGuarded("display settings reset to defaults (page 4/27)", SCOPE_DISPLAY); break;
            default: break;
        }
    }
    return r;
}

static bool Install() {
    OpenLog();
    Log("DefaultSettings.SA %s starting.", kVersion);

    uint32_t probe = 0;
    __try { probe = *reinterpret_cast<uint32_t*>(game::ADDR_VersionProbe); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (probe != 0x53EC8B55 && probe != 0x16197BE9) {
        Log("not GTA SA v1.0 US (probe dword at 0x401000 = 0x%08X): doing nothing.", probe);
        return false;
    }
    Log("GTA SA v1.0 US detected (%s exe).", probe == 0x53EC8B55 ? "compact" : "Hoodlum");

    HookSite(g_processSite, 0, reinterpret_cast<void*>(&ProcessHook), "game tick");
    HookSite(g_menuSites[0], game::ADDR_MenuDraw, reinterpret_cast<void*>(&MenuHook<0>), "menu draw");
    HookSite(g_menuSites[1], game::ADDR_MenuDraw, reinterpret_cast<void*>(&MenuHook<1>), "menu draw");
    if (!g_processSite.hooked && !g_menuSites[0].hooked && !g_menuSites[1].hooked) {
        Log("no startup hook could be installed: doing nothing.");
        return false;
    }

    int reinit = 0;
    reinit += HookSite(g_reinitSites[0], game::ADDR_ReinitControls, reinterpret_cast<void*>(&ReinitHook<0>), "ReinitControls");
    reinit += HookSite(g_reinitSites[1], game::ADDR_ReinitControls, reinterpret_cast<void*>(&ReinitHook<1>), "ReinitControls");
    if (!reinit) Log("WARNING: no ReinitControls call site could be hooked, 'restore defaults' will not re-apply key rules.");

    if (!HookSite(g_setDefSite, game::ADDR_SetDefaultPrefs, reinterpret_cast<void*>(&SetDefHook), "SetDefaultPreferences (restore defaults)"))
        Log("WARNING: restore-defaults call site could not be hooked, mouse rules apply at startup only.");
    return true;
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = mod;
        DisableThreadLibraryCalls(mod);
        __try { Install(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("EXCEPTION during install: doing nothing further."); }
    }
    return TRUE;
}
