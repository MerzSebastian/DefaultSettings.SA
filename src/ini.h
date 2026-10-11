// INI parser for DefaultSettings.SA.ini. No game access, no Windows headers.
//
//   [Mouse] [Gamepad] [Display] [Audio]
//   NAME=VALUE [OnlyIfLayout=...]   ; NAME, range and stock value: see src/settings.h
//   [Memory]
//   LABEL=ADDRESS TYPE VALUE [When=ALL|KEYS,MOUSE,DISPLAY,AUDIO] [OnlyIfLayout=...]   ; write any game/mod variable
//   [Keys]
//   ACTION[.Slot0|.Slot1]=KEY [OnlyIfLayout=QWERTZ,QWERTY,AZERTY,ANY (list)] [ClearDuplicates[=0|1]]
//
// ';' starts a comment at the start of a line or after whitespace.
// Bad lines are reported through the log callback and skipped.
#pragma once
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include "actions.h"
#include "keys.h"
#include "settings.h"

// Keyboard layouts as bit flags so a rule can list several: OnlyIfLayout=QWERTZ,AZERTY
enum LayoutBit : unsigned { LAYOUT_QWERTZ = 1, LAYOUT_QWERTY = 2, LAYOUT_AZERTY = 4, LAYOUT_ANY = 7 };

// Raw value types of a [Memory] rule.
enum class MemType { U8, I8, U16, I16, U32, I32, Float, Double };

inline int MemSize(MemType t) {
    switch (t) { case MemType::U8: case MemType::I8: return 1; case MemType::U16: case MemType::I16: return 2; case MemType::Double: return 8; default: return 4; }
}

// Little-endian bytes of a value as the game stores it (x86). Returns false if the value does not fit the type.
inline bool EncodeMem(MemType t, double v, unsigned char out[8]) {
    auto put = [&](long long iv, long long lo, long long hi, int n) {
        if (iv < lo || iv > hi) return false;
        for (int i = 0; i < n; ++i) out[i] = static_cast<unsigned char>((iv >> (8 * i)) & 0xFF);
        return true;
    };
    const long long iv = static_cast<long long>(v);
    switch (t) {
        case MemType::U8:  return put(iv, 0, 255, 1);
        case MemType::I8:  return put(iv, -128, 127, 1);
        case MemType::U16: return put(iv, 0, 65535, 2);
        case MemType::I16: return put(iv, -32768, 32767, 2);
        case MemType::U32: return put(iv, 0, 4294967295LL, 4);
        case MemType::I32: return put(iv, -2147483648LL, 2147483647LL, 4);
        case MemType::Float:  { const float f = static_cast<float>(v); std::memcpy(out, &f, 4); return true; }
        case MemType::Double: { std::memcpy(out, &v, 8); return true; }
    }
    return false;
}

inline double DecodeMem(MemType t, const unsigned char* b) {
    unsigned long long u = 0;
    for (int i = 0; i < MemSize(t) && i < 8; ++i) u |= static_cast<unsigned long long>(b[i]) << (8 * i);
    switch (t) {
        case MemType::U8: case MemType::U16: case MemType::U32: return static_cast<double>(u);
        case MemType::I8:  return static_cast<double>(static_cast<signed char>(u));
        case MemType::I16: return static_cast<double>(static_cast<short>(u));
        case MemType::I32: return static_cast<double>(static_cast<int>(u));
        case MemType::Float:  { float f; std::memcpy(&f, b, 4); return f; }
        case MemType::Double: { double d; std::memcpy(&d, b, 8); return d; }
    }
    return 0;
}

inline bool ParseMemType(const std::string& s, MemType& t) {
    const std::string u = ToUpper(s);
    if (u == "U8" || u == "BYTE" || u == "BOOL") t = MemType::U8;
    else if (u == "I8") t = MemType::I8;
    else if (u == "U16" || u == "WORD") t = MemType::U16;
    else if (u == "I16" || u == "SHORT") t = MemType::I16;
    else if (u == "U32" || u == "DWORD") t = MemType::U32;
    else if (u == "I32" || u == "INT") t = MemType::I32;
    else if (u == "FLOAT") t = MemType::Float;
    else if (u == "DOUBLE") t = MemType::Double;
    else return false;
    return true;
}

// Addresses accepted in [Memory]: the game's own image and data (0x400000 .. 0x1000000).
constexpr uintptr_t MEM_ADDR_MIN = 0x400000, MEM_ADDR_MAX = 0x1000000;

struct Rule {
    enum Kind { Key, Setting, Memory } kind = Key;
    int         line = 0;
    std::string section;        // "Keys", "Mouse", "Gamepad", "Display" or "Audio"
    std::string text;           // the rule as written, for the log
    int         action = -1;    // Key
    int         slot = 0;       // Key: 0 or 1
    unsigned    key = 0;        // Key (KEY_NULL = unbind)
    int         setting = -1;   // Setting: index into kSettings
    double      value = 0;      // Setting / Memory: the INI value
    uintptr_t   memAddr = 0;    // Memory: address to write
    MemType     memType = MemType::I32;
    unsigned    when = SCOPE_ALL;   // Memory: Scope bits of the defaults moments that trigger the write
    unsigned    layouts = LAYOUT_ANY;   // LayoutBit mask; the rule applies if the current layout is in it
    bool        clearDuplicates = false;
};

struct IniData {
    std::vector<Rule> rules;   // file order
};

using LogFn = std::function<void(const std::string&)>;

namespace ini_detail {

inline std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
    return s.substr(b, e - b);
}

inline std::string StripComment(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == ';' && (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t')) return s.substr(0, i);
    return s;
}

inline std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t b = 0;
    for (;;) {
        size_t e = s.find(sep, b);
        out.push_back(Trim(s.substr(b, e == std::string::npos ? std::string::npos : e - b)));
        if (e == std::string::npos) break;
        b = e + 1;
    }
    return out;
}

inline bool ParseBool(const std::string& v, bool& out) {
    const std::string u = ToUpper(v);
    if (u == "1" || u == "TRUE" || u == "YES" || u == "ON")  { out = true;  return true; }
    if (u == "0" || u == "FALSE" || u == "NO" || u == "OFF") { out = false; return true; }
    return false;
}

inline int FindAction(const std::string& name) {
    const std::string u = ToUpper(name);
    for (const ActionInfo& a : kActions)
        if (u == a.name || (a.alias && u == a.alias)) return a.id;
    return -1;
}

// Options after the main value: "OnlyIfLayout=QWERTZ", "ClearDuplicates", "ClearDuplicates=1".
inline bool ParseOptions(const std::vector<std::string>& toks, size_t from, Rule& r, std::string& err) {
    for (size_t i = from; i < toks.size(); ++i) {
        const std::string& t = toks[i];
        const size_t eq = t.find('=');
        const std::string k = ToUpper(Trim(t.substr(0, eq)));
        const std::string v = eq == std::string::npos ? "" : Trim(t.substr(eq + 1));
        if (k == "ONLYIFLAYOUT") {
            // one or more of QWERTZ, QWERTY, AZERTY, ANY separated by ',' or '|' (no spaces)
            unsigned mask = 0;
            std::string cur;
            for (size_t c = 0; c <= v.size(); ++c) {
                if (c < v.size() && v[c] != ',' && v[c] != '|') { cur += v[c]; continue; }
                const std::string u = ToUpper(cur);
                if (u == "QWERTZ") mask |= LAYOUT_QWERTZ;
                else if (u == "QWERTY") mask |= LAYOUT_QWERTY;
                else if (u == "AZERTY") mask |= LAYOUT_AZERTY;
                else if (u == "ANY") mask |= LAYOUT_ANY;
                else { err = "bad OnlyIfLayout value '" + v + "' (QWERTZ, QWERTY, AZERTY or ANY, separated by ',')"; return false; }
                cur.clear();
            }
            r.layouts = mask;
        } else if (k == "WHEN") {
            if (r.kind != Rule::Memory) { err = "'When' is only for [Memory] rules"; return false; }
            // ALL, or a list of KEYS, MOUSE, DISPLAY, AUDIO separated by ',' or '|'
            unsigned mask = 0;
            std::string cur;
            for (size_t c = 0; c <= v.size(); ++c) {
                if (c < v.size() && v[c] != ',' && v[c] != '|') { cur += v[c]; continue; }
                const std::string u = ToUpper(cur);
                if (u == "ALL") mask |= SCOPE_ALL;
                else if (u == "KEYS") mask |= SCOPE_KEYS;
                else if (u == "MOUSE") mask |= SCOPE_MOUSE;
                else if (u == "DISPLAY") mask |= SCOPE_DISPLAY;
                else if (u == "AUDIO") mask |= SCOPE_AUDIO;
                else { err = "bad When value '" + v + "' (ALL, KEYS, MOUSE, DISPLAY or AUDIO, separated by ',')"; return false; }
                cur.clear();
            }
            r.when = mask;
        } else if (k == "CLEARDUPLICATES") {
            if (r.kind != Rule::Key) { err = "'ClearDuplicates' is only for [Keys] rules"; return false; }
            if (eq == std::string::npos) r.clearDuplicates = true;
            else if (!ParseBool(v, r.clearDuplicates)) { err = "bad ClearDuplicates value '" + v + "'"; return false; }
        } else {
            err = "unknown option '" + t + "'";
            return false;
        }
    }
    return true;
}

} // namespace ini_detail

inline void ParseIni(const std::string& content, IniData& out, const LogFn& log) {
    using namespace ini_detail;
    std::string section;
    int lineNo = 0;
    size_t pos = 0;
    if (content.compare(0, 3, "\xEF\xBB\xBF") == 0) pos = 3; // UTF-8 BOM

    while (pos <= content.size()) {
        size_t nl = content.find('\n', pos);
        std::string raw = content.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? content.size() + 1 : nl + 1;
        ++lineNo;

        const std::string line = Trim(StripComment(raw));
        if (line.empty()) continue;

        if (line.front() == '[') {
            if (line.back() != ']') { log("line " + std::to_string(lineNo) + ": malformed section header, ignored: " + line); section = "?"; continue; }
            section = Trim(line.substr(1, line.size() - 2));
            continue;
        }

        const size_t eq = line.find('=');
        if (eq == std::string::npos) { log("line " + std::to_string(lineNo) + ": no '=', ignored: " + line); continue; }
        const std::string name = Trim(line.substr(0, eq));
        const std::string rest = Trim(line.substr(eq + 1));
        const std::string secU = ToUpper(section);
        const std::string where = "line " + std::to_string(lineNo) + ": ";

        if (secU != "KEYS" && secU != "MEMORY" && !IsSettingSection(section)) {
            log(where + "line in unknown section [" + section + "], ignored: " + line);
            continue;
        }

        // value + options, whitespace separated
        std::vector<std::string> toks;
        {
            size_t b = 0;
            while (b < rest.size()) {
                while (b < rest.size() && (rest[b] == ' ' || rest[b] == '\t')) ++b;
                size_t e = b;
                while (e < rest.size() && rest[e] != ' ' && rest[e] != '\t') ++e;
                if (e > b) toks.push_back(rest.substr(b, e - b));
                b = e;
            }
        }
        if (toks.empty()) { log(where + "empty value, ignored: " + line); continue; }

        Rule r;
        r.line = lineNo;
        r.section = section;
        r.text = line;
        std::string err;

        if (secU == "MEMORY") {   // LABEL=ADDRESS TYPE VALUE [options]
            r.kind = Rule::Memory;
            if (toks.size() < 3) { log(where + "needs ADDRESS TYPE VALUE, ignored: " + line); continue; }
            char* end = nullptr;
            const unsigned long a = std::strtoul(toks[0].c_str(), &end, 0);
            if (end == toks[0].c_str() || *end != '\0' || a < MEM_ADDR_MIN || a >= MEM_ADDR_MAX) {
                log(where + "address must be a number like 0xC1704C inside the game image (0x400000-0xFFFFFF), ignored: " + line);
                continue;
            }
            r.memAddr = a;
            if (!ParseMemType(toks[1], r.memType)) { log(where + "unknown type '" + toks[1] + "' (u8 i8 u16 i16 u32 i32 float double), ignored: " + line); continue; }
            if (r.memType == MemType::Float || r.memType == MemType::Double) r.value = std::strtod(toks[2].c_str(), &end);
            else r.value = static_cast<double>(std::strtoll(toks[2].c_str(), &end, 0));
            unsigned char tmp[8];
            if (end == toks[2].c_str() || *end != '\0' || !EncodeMem(r.memType, r.value, tmp)) {
                log(where + "value '" + toks[2] + "' is not valid for type " + toks[1] + ", ignored: " + line);
                continue;
            }
            if (!ParseOptions(toks, 3, r, err)) { log(where + err + ", ignored: " + line); continue; }
            out.rules.push_back(r);
            continue;
        }

        if (secU != "KEYS") {   // [Mouse] [Gamepad] [Display] [Audio]: table-driven settings
            r.setting = FindSetting(section, name);
            if (r.setting < 0) { log(where + "unknown [" + section + "] key '" + name + "', ignored"); continue; }
            const SettingInfo& si = kSettings[r.setting];
            r.kind = Rule::Setting;
            if (si.type == VType::Bool) {
                bool b = false;
                if (!ParseBool(toks[0], b)) { log(where + std::string(si.name) + " needs 0 or 1, ignored: " + line); continue; }
                r.value = b ? 1 : 0;
            } else if (!ParseSettingName(si, toks[0], r.value)) {
                char* end = nullptr;
                r.value = std::strtod(toks[0].c_str(), &end);
                if (end == toks[0].c_str() || *end != '\0' || r.value < si.min || r.value > si.max) {
                    log(where + std::string(si.name) + " needs a number from " + std::to_string(si.min) + " to " + std::to_string(si.max) +
                        (si.names ? std::string(" or a name (") + si.names + ")" : std::string()) + ", ignored: " + line);
                    continue;
                }
            }
            if (!ParseOptions(toks, 1, r, err)) { log(where + err + ", ignored: " + line); continue; }
            out.rules.push_back(r);
            continue;
        }

        // Keys: ACTION[.SlotN]
        std::string actName = name;
        const size_t dot = name.find('.');
        if (dot != std::string::npos) {
            const std::string s = ToUpper(name.substr(dot + 1));
            actName = name.substr(0, dot);
            if (s == "SLOT0") r.slot = 0;
            else if (s == "SLOT1") r.slot = 1;
            else { log(where + "bad slot '" + name.substr(dot + 1) + "' (Slot0 or Slot1), ignored: " + line); continue; }
        }
        r.action = FindAction(actName);
        if (r.action < 0) { log(where + "unknown action '" + actName + "', ignored"); continue; }
        if (!ParseKey(toks[0], r.key)) { log(where + "unknown key name '" + toks[0] + "', ignored: " + line); continue; }
        if (!ParseOptions(toks, 1, r, err)) { log(where + err + ", ignored: " + line); continue; }
        out.rules.push_back(r);
    }
}
