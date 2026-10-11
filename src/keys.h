// Key names <-> key codes (RsKeyCodes). Letters/digits/punctuation are plain ASCII (letters uppercase),
// which is how the game stores them.
#pragma once
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

constexpr unsigned KEY_NULL = 1056; // rsNULL = unbound keyboard slot

struct KeyName { const char* name; unsigned code; };

constexpr KeyName kKeyNames[] = {
    {"SPACE", 32},
    {"ESC", 1000}, {"F1", 1001}, {"F2", 1002}, {"F3", 1003}, {"F4", 1004}, {"F5", 1005}, {"F6", 1006},
    {"F7", 1007}, {"F8", 1008}, {"F9", 1009}, {"F10", 1010}, {"F11", 1011}, {"F12", 1012},
    {"INS", 1013}, {"DEL", 1014}, {"HOME", 1015}, {"END", 1016}, {"PGUP", 1017}, {"PGDN", 1018},
    {"UP", 1019}, {"DOWN", 1020}, {"LEFT", 1021}, {"RIGHT", 1022},
    {"DIVIDE", 1023}, {"TIMES", 1024}, {"PLUS", 1025}, {"MINUS", 1026},
    {"PADDEL", 1027}, {"PADEND", 1028}, {"PADDOWN", 1029}, {"PADPGDN", 1030}, {"PADLEFT", 1031},
    {"PAD5", 1032}, {"NUMLOCK", 1033}, {"PADRIGHT", 1034}, {"PADHOME", 1035}, {"PADUP", 1036},
    {"PADPGUP", 1037}, {"PADINS", 1038}, {"PADENTER", 1039},
    {"SCROLL", 1040}, {"PAUSE", 1041}, {"BACKSP", 1042}, {"TAB", 1043}, {"CAPSLK", 1044}, {"ENTER", 1045},
    {"LSHIFT", 1046}, {"RSHIFT", 1047}, {"SHIFT", 1048}, {"LCTRL", 1049}, {"RCTRL", 1050},
    {"LALT", 1051}, {"RALT", 1052}, {"LWIN", 1053}, {"RWIN", 1054}, {"APPS", 1055},
    {"NONE", KEY_NULL},
};

inline std::string ToUpper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// Accepts: a name from the table (case-insensitive), a single printable ASCII
// character (A-Z, 0-9, punctuation), or a number (decimal / 0x hex) in 32..255
// or 1000..1056. Mouse buttons (1..7) are rejected: these are keyboard slots.
inline bool ParseKey(const std::string& text, unsigned& code) {
    if (text.empty()) return false;
    if (text.size() == 1) {
        unsigned char c = static_cast<unsigned char>(text[0]);
        if (c > 32 && c < 127) { code = static_cast<unsigned>(std::toupper(c)); return true; }
        return false;
    }
    const std::string up = ToUpper(text);
    for (const KeyName& k : kKeyNames)
        if (up == k.name) { code = k.code; return true; }
    if (up == "NULL") { code = KEY_NULL; return true; }
    if (std::isdigit(static_cast<unsigned char>(up[0]))) {
        char* end = nullptr;
        unsigned long v = std::strtoul(up.c_str(), &end, 0);
        if (end && *end == '\0' && ((v >= 32 && v <= 255) || (v >= 1000 && v <= KEY_NULL))) {
            code = static_cast<unsigned>(v);
            return true;
        }
    }
    return false;
}

// For the log. Never fails.
inline std::string KeyToName(unsigned code) {
    for (const KeyName& k : kKeyNames)
        if (k.code == code) return k.name;
    if (code > 32 && code < 127) return std::string(1, static_cast<char>(code));
    return "#" + std::to_string(code);
}
