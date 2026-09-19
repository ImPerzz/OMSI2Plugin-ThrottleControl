#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "shared.h"
#include "resource.h"

enum {
    SYS_TIMEGAP = 0,
    VEH_THROTTLE = 0
};

enum OmsiModFlags {
    OMSIMOD_CONTINUITY = 1,
    OMSIMOD_SHIFT = 2,
    OMSIMOD_CTRL = 4,
    OMSIMOD_ALT = 8
};

struct KeyBinding {
    unsigned scan;
    unsigned mods;
    unsigned vk;
    bool valid;
};

static KeyBinding bind_throttle = { 0x48, OMSIMOD_CONTINUITY, 0, false };
static KeyBinding bind_brake = { 0x50, OMSIMOD_CONTINUITY, 0, false };
static KeyBinding bind_amplify = { 0x4E, OMSIMOD_CONTINUITY, 0, false };

static float current_throttle = 0.0f;
static float current_timegap = 0.02f;
static bool amplify_was_down = false;
static DWORD last_amplify_press_ms = 0;

static const float THROTTLE_ATTACK = 1.0f;
static const float THROTTLE_RELEASE = 1.0f;
static const float WRITE_EPS = 0.01f;
static const DWORD AMPLIFY_DOUBLE_TAP_MS = 500;

extern "C" {
    __declspec(dllexport) void __stdcall PluginStart(void* aOwner);
    __declspec(dllexport) void __stdcall PluginFinalize();
    __declspec(dllexport) void __stdcall AccessSystemVariable(unsigned short index, float* value, bool* write);
    __declspec(dllexport) void __stdcall AccessVariable(unsigned short index, float* value, bool* write);
    __declspec(dllexport) void __stdcall AccessTrigger(unsigned short triggerindex, bool* active);
    __declspec(dllexport) void __stdcall AccessStringVariable(unsigned short index, wchar_t* str, bool* write);
}

static void TrimInPlace(char* s)
{
    char* start = s;
    while (*start && isspace(static_cast<unsigned char>(*start)))
        ++start;
    if (start != s)
        memmove(s, start, strlen(start) + 1);

    size_t n = strlen(s);
    while (n > 0 && isspace(static_cast<unsigned char>(s[n - 1])))
        s[--n] = '\0';
}

static bool EqualsIgnoreCase(const char* a, const char* b)
{
    while (*a && *b) {
        if (tolower(static_cast<unsigned char>(*a)) != tolower(static_cast<unsigned char>(*b)))
            return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

static unsigned ScanToVk(unsigned scan)
{
    switch (scan) {
    case 0x47: return VK_NUMPAD7;
    case 0x48: return VK_NUMPAD8;
    case 0x49: return VK_NUMPAD9;
    case 0x4A: return VK_SUBTRACT;
    case 0x4B: return VK_NUMPAD4;
    case 0x4C: return VK_NUMPAD5;
    case 0x4D: return VK_NUMPAD6;
    case 0x4E: return VK_ADD;
    case 0x4F: return VK_NUMPAD1;
    case 0x50: return VK_NUMPAD2;
    case 0x51: return VK_NUMPAD3;
    case 0x52: return VK_NUMPAD0;
    case 0x53: return VK_DECIMAL;
    case 0x37: return VK_MULTIPLY;
    case 0x35: return VK_DIVIDE;
    case 0x9C: return VK_RETURN;
    default: break;
    }

    UINT vk = MapVirtualKeyA(scan & 0xFFu, MAPVK_VSC_TO_VK);
    if (!vk)
        vk = MapVirtualKeyA(scan & 0xFFu, MAPVK_VSC_TO_VK_EX);
    if (!vk && scan > 0x7F)
        vk = MapVirtualKeyA((scan & 0x7Fu) | 0xE000u, MAPVK_VSC_TO_VK_EX);
    return vk;
}

static void FinalizeBinding(KeyBinding& b)
{
    b.vk = ScanToVk(b.scan);
    b.valid = (b.vk != 0);
}

static bool KeyDownVk(unsigned vk)
{
    return vk != 0 && (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0;
}

static bool BindingPressed(const KeyBinding& b)
{
    if (!b.valid || !KeyDownVk(b.vk))
        return false;
    if ((b.mods & OMSIMOD_SHIFT) && !KeyDownVk(VK_SHIFT))
        return false;
    if ((b.mods & OMSIMOD_CTRL) && !KeyDownVk(VK_CONTROL))
        return false;
    if ((b.mods & OMSIMOD_ALT) && !KeyDownVk(VK_MENU))
        return false;
    return true;
}

static void UpdateThrottle()
{
    const bool gas = BindingPressed(bind_throttle);
    const bool brake = BindingPressed(bind_brake);
    const bool amplify = BindingPressed(bind_amplify);

    bool instant_dump = false;
    if (amplify && !amplify_was_down) {
        const DWORD now = GetTickCount();
        if (last_amplify_press_ms != 0 && (now - last_amplify_press_ms) < AMPLIFY_DOUBLE_TAP_MS)
            instant_dump = true;
        last_amplify_press_ms = now;
    }
    amplify_was_down = amplify;

    if (brake || instant_dump) {
        current_throttle = 0.0f;
    }
    else if (gas && amplify) {
        current_throttle = 1.0f; // Num8 + Num+ kickdown
    }
    else if (amplify && current_throttle > 0.0f) {
        // Num+ alone: gradually release held throttle
        current_throttle -= THROTTLE_RELEASE * current_timegap;
        if (current_throttle < 0.0f)
            current_throttle = 0.0f;
    }
    else if (gas) {
        current_throttle += THROTTLE_ATTACK * current_timegap;
        if (current_throttle > 1.0f)
            current_throttle = 1.0f;
    }
}

static bool ResolveKeyboardCfgPath(char* out, size_t out_size)
{
    char base[MAX_PATH]{ 0 };

    if (GetFullPathNameA("Inputs\\keyboard.cfg", MAX_PATH, base, nullptr) &&
        GetFileAttributesA(base) != INVALID_FILE_ATTRIBUTES) {
        strncpy_s(out, out_size, base, _TRUNCATE);
        return true;
    }

    if (!GetModuleFileNameA(nullptr, base, MAX_PATH))
        return false;

    char* slash = strrchr(base, '\\');
    if (!slash)
        return false;
    *(slash + 1) = '\0';

    char candidate[MAX_PATH]{ 0 };
    if (sprintf_s(candidate, "%sInputs\\keyboard.cfg", base) <= 0)
        return false;
    if (GetFileAttributesA(candidate) == INVALID_FILE_ATTRIBUTES)
        return false;

    strncpy_s(out, out_size, candidate, _TRUNCATE);
    return true;
}

static void ApplyNamedBinding(const char* name, unsigned scan, unsigned mods)
{
    KeyBinding* target = nullptr;
    if (EqualsIgnoreCase(name, "throttle"))
        target = &bind_throttle;
    else if (EqualsIgnoreCase(name, "brake"))
        target = &bind_brake;
    else if (EqualsIgnoreCase(name, "throttle_amplify"))
        target = &bind_amplify;
    else
        return;

    target->scan = scan;
    target->mods = mods;
    FinalizeBinding(*target);
}

static void LoadKeyboardBindings()
{
    FinalizeBinding(bind_throttle);
    FinalizeBinding(bind_brake);
    FinalizeBinding(bind_amplify);

    char path[MAX_PATH]{ 0 };
    if (!ResolveKeyboardCfgPath(path, MAX_PATH)) {
        Log(LT_WARN, "keyboard.cfg not found, using default Num8/Num2/Num+.");
        return;
    }

    FILE* file = nullptr;
    if (fopen_s(&file, path, "r") != 0 || !file) {
        Log(LT_WARN, "Failed to open %s.", path);
        return;
    }

    char line[256];
    int state = 0;
    char name[128]{ 0 };
    unsigned scan = 0;

    while (fgets(line, sizeof(line), file)) {
        TrimInPlace(line);
        if (line[0] == '\0' || line[0] == ';')
            continue;

        if (EqualsIgnoreCase(line, "[entry]")) {
            state = 1;
            name[0] = '\0';
            scan = 0;
            continue;
        }

        if (state == 1) {
            strncpy_s(name, line, _TRUNCATE);
            state = 2;
        }
        else if (state == 2) {
            scan = static_cast<unsigned>(strtoul(line, nullptr, 10));
            state = 3;
        }
        else if (state == 3) {
            ApplyNamedBinding(name, scan, static_cast<unsigned>(strtoul(line, nullptr, 10)));
            state = 0;
        }
    }

    fclose(file);
    Log(LT_INFO,
        "Binds: throttle vk=0x%02X; brake vk=0x%02X; amplify vk=0x%02X",
        bind_throttle.vk, bind_brake.vk, bind_amplify.vk);
}

BOOL APIENTRY DllMain(HMODULE instance, DWORD, LPVOID)
{
    g::dll_instance = instance;
    return TRUE;
}

void __stdcall PluginStart(void* /*aOwner*/)
{
    g::debug = strstr(GetCommandLineA(), "-throttlecontrol_debug") != nullptr;
    if (g::debug) {
        FILE* console = nullptr;
        AllocConsole();
        SetConsoleTitleA("ThrottleControl " PROJECT_VERSION " - Debug");
        freopen_s(&console, "CONIN$", "r", stdin);
        freopen_s(&console, "CONOUT$", "w", stdout);
        freopen_s(&console, "CONOUT$", "w", stderr);

        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        GetConsoleMode(handle, &mode);
        SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
    }

    g::version = Offsets::CheckVersion();
    if (!g::version)
        Log(LT_WARN, "Unsupported OMSI 2 version - logfile output disabled.");
    else
        Log(LT_INFO, "Detected OMSI version %s", g::version);

    LoadKeyboardBindings();
    Log(LT_INFO, "Plugin ThrottleControl loaded.");
}

void __stdcall PluginFinalize()
{
    Log(LT_INFO, "Plugin ThrottleControl finalized.");
}

void __stdcall AccessTrigger(unsigned short, bool*)
{
}

void __stdcall AccessSystemVariable(unsigned short index, float* value, bool*)
{
    if (!value || index != SYS_TIMEGAP)
        return;

    float dt = *value;
    if (dt < 0.0001f) dt = 0.0001f;
    if (dt > 0.25f) dt = 0.25f;
    current_timegap = dt;

    UpdateThrottle();
}

void __stdcall AccessVariable(unsigned short index, float* value, bool* write)
{
    if (!value || !write || index != VEH_THROTTLE)
        return;

    if (fabsf(*value - current_throttle) < WRITE_EPS)
        return;

    *value = current_throttle;
    *write = true;
}

void __stdcall AccessStringVariable(unsigned short, wchar_t*, bool*)
{
}

void Log(LogType log_t, const char* message, ...)
{
    char buffer[1024]{ 0 };
    va_list va;
    va_start(va, message);
    vsprintf_s(buffer, 1024, message, va);
    va_end(va);

    if (g::debug) {
        char tag[15]{ 0 };
        switch (log_t) {
        case LT_FATAL:
        case LT_ERROR: mystrcpy(tag, "\x1B[91mERROR\x1B[0m", 15); break;
        case LT_WARN: mystrcpy(tag, "\x1B[93m WARN\x1B[0m", 15); break;
        default: mystrcpy(tag, "\x1B[97m INFO\x1B[0m", 15); break;
        }

        SYSTEMTIME time;
        GetLocalTime(&time);
        printf("[%02d:%02d:%02d %s] %s\n", time.wHour, time.wMinute, time.wSecond, tag, buffer);
    }

    if (g::version) {
        UnicodeString<1024> entry(L"[ThrottleControl] %hs", buffer);
        wchar_t* log = entry.string;

        __asm
        {
            mov     eax, log
            mov     dl, log_t
            call    Offsets::AddLogEntry
        }
    }
}
