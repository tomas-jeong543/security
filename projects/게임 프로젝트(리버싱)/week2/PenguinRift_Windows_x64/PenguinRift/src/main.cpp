// Penguin Rift - reversing-friendly SDL2 platform action game.
// No C/C++ runtime library is required. SDL2 is loaded dynamically at runtime.

using u8  = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using i32 = int;

extern "C" __declspec(dllimport) void* __stdcall LoadLibraryA(const char* name);
extern "C" __declspec(dllimport) void* __stdcall GetProcAddress(void* module, const char* name);
extern "C" __declspec(dllimport) void  __stdcall ExitProcess(u32 code);
extern "C" __declspec(dllimport) int   __stdcall IsDebuggerPresent();
extern "C" __declspec(dllimport) int   __stdcall MessageBoxA(void*, const char*, const char*, u32);
extern "C" __declspec(dllimport) void* __stdcall CreateFileA(const char*, u32, u32, void*, u32, u32, void*);
extern "C" __declspec(dllimport) int   __stdcall ReadFile(void*, void*, u32, u32*, void*);
extern "C" __declspec(dllimport) int   __stdcall WriteFile(void*, const void*, u32, u32*, void*);
extern "C" __declspec(dllimport) int   __stdcall CloseHandle(void*);

extern "C" int _fltused = 0;

extern "C" void* memset(void* dst, int value, unsigned long long size) {
    u8* p = (u8*)dst;
    for (unsigned long long i = 0; i < size; ++i) p[i] = (u8)value;
    return dst;
}
extern "C" void* memcpy(void* dst, const void* src, unsigned long long size) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    for (unsigned long long i = 0; i < size; ++i) d[i] = s[i];
    return dst;
}

// ------------------------------- SDL2 ABI ---------------------------------
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Rect { i32 x, y, w, h; };
struct SDL_TextInputEvent {
    u32 type;
    u32 timestamp;
    u32 windowID;
    char text[32];
};
union SDL_Event {
    u32 type;
    SDL_TextInputEvent text;
    u8 padding[56];
};

enum : u32 {
    SDL_INIT_VIDEO = 0x00000020u,
    SDL_WINDOW_SHOWN = 0x00000004u,
    SDL_RENDERER_SOFTWARE = 0x00000001u,
    SDL_RENDERER_ACCELERATED = 0x00000002u,
    SDL_RENDERER_PRESENTVSYNC = 0x00000004u,
    SDL_QUIT_EVENT = 0x00000100u,
    SDL_TEXTINPUT_EVENT = 0x00000303u,
    SDL_BLENDMODE_BLEND = 1u
};

enum : i32 {
    SDL_WINDOWPOS_CENTERED = 0x2FFF0000,

    SC_A = 4,
    SC_D = 7,
    SC_R = 21,
    SC_X = 27,
    SC_Z = 29,

    SC_1 = 30,
    SC_2 = 31,
    SC_3 = 32,
    SC_4 = 33,
    SC_5 = 34,

    SC_RETURN = 40,
    SC_ESCAPE = 41,
    SC_BACKSPACE = 42,
    SC_SPACE = 44,

    SC_F5 = 62,
    SC_F6 = 63,
    SC_F7 = 64,
    SC_F10 = 67,

    SC_RIGHT = 79,
    SC_LEFT = 80,
    SC_DOWN = 81,
    SC_UP = 82
};

using PFN_SDL_Init = i32 (*)(u32);
using PFN_SDL_Quit = void (*)();
using PFN_SDL_CreateWindow = SDL_Window* (*)(const char*, i32, i32, i32, i32, u32);
using PFN_SDL_DestroyWindow = void (*)(SDL_Window*);
using PFN_SDL_CreateRenderer = SDL_Renderer* (*)(SDL_Window*, i32, u32);
using PFN_SDL_DestroyRenderer = void (*)(SDL_Renderer*);
using PFN_SDL_PollEvent = i32 (*)(SDL_Event*);
using PFN_SDL_GetKeyboardState = const u8* (*)(i32*);
using PFN_SDL_GetTicks = u32 (*)();
using PFN_SDL_Delay = void (*)(u32);
using PFN_SDL_SetRenderDrawColor = i32 (*)(SDL_Renderer*, u8, u8, u8, u8);
using PFN_SDL_RenderClear = i32 (*)(SDL_Renderer*);
using PFN_SDL_RenderFillRect = i32 (*)(SDL_Renderer*, const SDL_Rect*);
using PFN_SDL_RenderDrawRect = i32 (*)(SDL_Renderer*, const SDL_Rect*);
using PFN_SDL_RenderDrawLine = i32 (*)(SDL_Renderer*, i32, i32, i32, i32);
using PFN_SDL_RenderPresent = void (*)(SDL_Renderer*);
using PFN_SDL_SetWindowTitle = void (*)(SDL_Window*, const char*);
using PFN_SDL_SetRenderDrawBlendMode = i32 (*)(SDL_Renderer*, i32);
using PFN_SDL_StartTextInput = void (*)();
using PFN_SDL_StopTextInput = void (*)();

struct SDLApi {
    PFN_SDL_Init Init;
    PFN_SDL_Quit Quit;
    PFN_SDL_CreateWindow CreateWindow;
    PFN_SDL_DestroyWindow DestroyWindow;
    PFN_SDL_CreateRenderer CreateRenderer;
    PFN_SDL_DestroyRenderer DestroyRenderer;
    PFN_SDL_PollEvent PollEvent;
    PFN_SDL_GetKeyboardState GetKeyboardState;
    PFN_SDL_GetTicks GetTicks;
    PFN_SDL_Delay Delay;
    PFN_SDL_SetRenderDrawColor SetRenderDrawColor;
    PFN_SDL_RenderClear RenderClear;
    PFN_SDL_RenderFillRect RenderFillRect;
    PFN_SDL_RenderDrawRect RenderDrawRect;
    PFN_SDL_RenderDrawLine RenderDrawLine;
    PFN_SDL_RenderPresent RenderPresent;
    PFN_SDL_SetWindowTitle SetWindowTitle;
    PFN_SDL_SetRenderDrawBlendMode SetRenderDrawBlendMode;
    PFN_SDL_StartTextInput StartTextInput;
    PFN_SDL_StopTextInput StopTextInput;
};

static SDLApi g_sdl;
static SDL_Window* g_window;
static SDL_Renderer* g_renderer;

static bool LoadSDL2() {
    void* module = LoadLibraryA("SDL2.dll");
    if (!module) return false;
#define SDL_LOAD(member, name) g_sdl.member = (PFN_SDL_##member)GetProcAddress(module, name); if (!g_sdl.member) return false
    SDL_LOAD(Init, "SDL_Init");
    SDL_LOAD(Quit, "SDL_Quit");
    SDL_LOAD(CreateWindow, "SDL_CreateWindow");
    SDL_LOAD(DestroyWindow, "SDL_DestroyWindow");
    SDL_LOAD(CreateRenderer, "SDL_CreateRenderer");
    SDL_LOAD(DestroyRenderer, "SDL_DestroyRenderer");
    SDL_LOAD(PollEvent, "SDL_PollEvent");
    SDL_LOAD(GetKeyboardState, "SDL_GetKeyboardState");
    SDL_LOAD(GetTicks, "SDL_GetTicks");
    SDL_LOAD(Delay, "SDL_Delay");
    SDL_LOAD(SetRenderDrawColor, "SDL_SetRenderDrawColor");
    SDL_LOAD(RenderClear, "SDL_RenderClear");
    SDL_LOAD(RenderFillRect, "SDL_RenderFillRect");
    SDL_LOAD(RenderDrawRect, "SDL_RenderDrawRect");
    SDL_LOAD(RenderDrawLine, "SDL_RenderDrawLine");
    SDL_LOAD(RenderPresent, "SDL_RenderPresent");
    SDL_LOAD(SetWindowTitle, "SDL_SetWindowTitle");
    SDL_LOAD(SetRenderDrawBlendMode, "SDL_SetRenderDrawBlendMode");
    SDL_LOAD(StartTextInput, "SDL_StartTextInput");
    SDL_LOAD(StopTextInput, "SDL_StopTextInput");
#undef SDL_LOAD
    return true;
}

// -------------------------- Reversing challenge -----------------------------
extern "C" __declspec(dllexport) volatile i32 g_ReversingScore = 0;
extern "C" __declspec(dllexport) volatile i32 g_LevelGate = 4;
extern "C" __declspec(dllexport) volatile u32 g_IceKey = 0x19A4C20Fu;
extern "C" __declspec(dllexport) volatile i32 g_DebugView = 0;
extern "C" __declspec(dllexport) volatile i32 g_DebuggerObserved = 0;

static u32 RotL32(u32 x, u32 n) { return (x << n) | (x >> (32u - n)); }

extern "C" __declspec(dllexport) u32 Challenge_TransformKey(u32 candidate) {
    u32 x = candidate ^ 0xA17C39D2u;
    x = RotL32(x, 7u);
    x += 0x13579BDFu;
    x ^= 0xC0DEC0DEu;
    return RotL32(x, 11u);
}

extern "C" __declspec(dllexport) i32 Challenge_CheckUnlock(u32 candidate) {
    // Patch the comparison, recover the input, or change g_IceKey while debugging.
    return Challenge_TransformKey(candidate) == 0x6A15FDDCu;
}

extern "C" __declspec(dllexport) void Challenge_GrantScore(i32 amount) {
    if (amount > 0 && amount < 1000000) g_ReversingScore += amount;
}

// Strings deliberately retained for static-analysis practice.
static const char* const kReverseMarkers[] = {
    "PATCH_ME_LEVEL_GATE",
    "REVERSE_CHALLENGE_UNLOCKED",
    "F10_DEBUG_HITBOXES",
    "F6_CHECKS_ICE_KEY",
    "SCORE_COMPARE_7777",
    "PENGUIN_RIFT_BUILD_2026"
};

extern "C" __declspec(dllexport) const char* Challenge_GetMarker(i32 index) {
    if (index < 0 || index >= 6) return "INVALID_MARKER_INDEX";
    return kReverseMarkers[index];
}

// ------------------------------- Game data ----------------------------------
static const i32 SCREEN_W = 960;
static const i32 SCREEN_H = 540;
static const i32 FP = 100;
static const i32 MAX_PLATFORMS = 12;
static const i32 MAX_ENEMIES = 12;
static const i32 MAX_SHOTS = 12;
static const i32 MAX_CRYSTALS = 16;
static const i32 MAX_PARTICLES = 64;

struct Platform { i32 x, y, w, h; };
struct Player {
    i32 x, y, vx, vy;
    i32 w, h;
    i32 grounded;
    i32 facing;
    i32 lives;
    i32 invincible;
    i32 attackCooldown;
};
struct Enemy {
    i32 active;
    i32 x, y;
    i32 w, h;
    i32 direction;
    i32 leftBound, rightBound;
    i32 frozen;
};
struct Shot { i32 active, x, y, vx, life; };
struct Crystal { i32 active, x, y, phase; };
struct Particle { i32 active, x, y, vx, vy, life, kind; };

static Platform g_platforms[MAX_PLATFORMS];
static Enemy g_enemies[MAX_ENEMIES];
static Shot g_shots[MAX_SHOTS];
static Crystal g_crystals[MAX_CRYSTALS];
static Particle g_particles[MAX_PARTICLES];
static Player g_player;
static i32 g_platformCount;
static i32 g_enemyCount;
static i32 g_crystalCount;
static i32 g_level = 1;
static i32 g_state = 0;       // 0 playing, 1 game over, 2 completed, 3 ice gate, 4 true ending
static i32 g_levelClearTimer;
static i32 g_frameCounter;
static i32 g_secretTimer;
static i32 g_prevJump;
static i32 g_prevAttack;
static i32 g_prevRestart;
static i32 g_prevConfirm;
static i32 g_prevF6;
static i32 g_prevF10;
static i32 g_inHiddenStage = 0;
static u32 g_rng = 0x73A91E2Du;
static bool g_prevBackdoor;

static void NewGame();

// ---------------------- Login, timer and local records ----------------------
// The DAT file uses a deliberately simple password-based XOR layer and one
// FNV-1a checksum. This prevents casual plaintext editing without turning the
// record format into a second, overly difficult reversing challenge.
static const i32 MAX_LOCAL_RECORDS = 10;
static const i32 MAX_PLAYER_NAME = 12;
static const i32 MAX_PASSWORD_INPUT = 31;
static const char* const RECORD_FILE_NAME = "penguin_rift_records.dat";
static const char* const RECORD_PASSWORD = "security_fact2026";

static const u32 GENERIC_READ_ACCESS = 0x80000000u;
static const u32 GENERIC_WRITE_ACCESS = 0x40000000u;
static const u32 FILE_SHARE_READ_ACCESS = 0x00000001u;
static const u32 CREATE_ALWAYS_MODE = 2u;
static const u32 OPEN_EXISTING_MODE = 3u;
static const u32 FILE_ATTRIBUTE_NORMAL_VALUE = 0x00000080u;

struct LocalRecord {
    char player[MAX_PLAYER_NAME + 1];
    u8 reserved[3];
    u32 timeMs;
    i32 score;
};

struct RecordDatabase {
    u32 magic;
    u32 version;
    u32 count;
    LocalRecord records[MAX_LOCAL_RECORDS];
    u32 checksum;
};

enum : i32 {
    SCREEN_LOGIN = 0,
    SCREEN_GAME = 1,
    SCREEN_PASSWORD = 2,
    SCREEN_RECORDS = 3
};

static LocalRecord g_localRecords[MAX_LOCAL_RECORDS];
static i32 g_localRecordCount = 0;
static i32 g_recordsLoaded = 0;
static i32 g_recordFileInvalid = 0;
static i32 g_screenMode = SCREEN_LOGIN;

static char g_playerName[MAX_PLAYER_NAME + 1];
static i32 g_playerNameLength = 0;
static char g_passwordInput[MAX_PASSWORD_INPUT + 1];
static i32 g_passwordLength = 0;
static i32 g_passwordErrorTimer = 0;

static i32 g_prevEnter = 0;
static i32 g_prevBackspace = 0;
static i32 g_prevF7 = 0;
static u32 g_runElapsedMs = 0;
static u32 g_lastClearTimeMs = 0;
static i32 g_recordSavedThisRun = 0;

static i32 TextLength(const char* s) {
    i32 n = 0;
    while (s[n]) ++n;
    return n;
}

static bool TextEquals(const char* a, const char* b) {
    i32 i = 0;
    while (a[i] && b[i] && a[i] == b[i]) ++i;
    return a[i] == 0 && b[i] == 0;
}

static void CopyText(char* dst, const char* src, i32 capacity) {
    i32 i = 0;
    while (i + 1 < capacity && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = 0;
    while (++i < capacity) dst[i] = 0;
}

static void ClearRecordMemory() {
    g_localRecordCount = 0;
    for (i32 i = 0; i < MAX_LOCAL_RECORDS; ++i) {
        memset(&g_localRecords[i], 0, sizeof(LocalRecord));
    }
}

static u32 DatabaseChecksum(const RecordDatabase& db) {
    const u8* bytes = (const u8*)&db;
    const u32 length = (u32)(sizeof(RecordDatabase) - sizeof(u32));
    u32 hash = 2166136261u;
    for (u32 i = 0; i < length; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash ^ 0x2026FA47u;
}

static void PasswordCrypt(u8* data, u32 length) {
    const i32 passwordLength = TextLength(RECORD_PASSWORD);
    for (u32 i = 0; i < length; ++i) {
        u8 key = (u8)RECORD_PASSWORD[i % (u32)passwordLength];
        key ^= (u8)(0x5Au + (i * 29u));
        data[i] ^= key;
    }
}

static bool LoadLocalRecords() {
    ClearRecordMemory();
    g_recordFileInvalid = 0;

    void* file = CreateFileA(RECORD_FILE_NAME, GENERIC_READ_ACCESS,
        FILE_SHARE_READ_ACCESS, nullptr, OPEN_EXISTING_MODE,
        FILE_ATTRIBUTE_NORMAL_VALUE, nullptr);

    if (file == (void*)(-1LL)) {
        g_recordsLoaded = 1;
        return true;
    }

    RecordDatabase db{};
    u32 bytesRead = 0;
    i32 ok = ReadFile(file, &db, (u32)sizeof(db), &bytesRead, nullptr);
    CloseHandle(file);

    if (!ok || bytesRead != (u32)sizeof(db)) {
        g_recordFileInvalid = 1;
        g_recordsLoaded = 1;
        return false;
    }

    PasswordCrypt((u8*)&db, (u32)sizeof(db));
    if (db.magic != 0x52475052u || db.version != 1u ||
        db.count > (u32)MAX_LOCAL_RECORDS ||
        db.checksum != DatabaseChecksum(db)) {
        g_recordFileInvalid = 1;
        g_recordsLoaded = 1;
        return false;
    }

    g_localRecordCount = (i32)db.count;
    for (i32 i = 0; i < g_localRecordCount; ++i) {
        g_localRecords[i] = db.records[i];
        g_localRecords[i].player[MAX_PLAYER_NAME] = 0;
    }
    g_recordsLoaded = 1;
    return true;
}

static bool SaveLocalRecords() {
    RecordDatabase db{};
    db.magic = 0x52475052u; // "RPGR"
    db.version = 1u;
    db.count = (u32)g_localRecordCount;
    for (i32 i = 0; i < g_localRecordCount; ++i) db.records[i] = g_localRecords[i];
    db.checksum = DatabaseChecksum(db);
    PasswordCrypt((u8*)&db, (u32)sizeof(db));

    void* file = CreateFileA(RECORD_FILE_NAME, GENERIC_WRITE_ACCESS, 0,
        nullptr, CREATE_ALWAYS_MODE, FILE_ATTRIBUTE_NORMAL_VALUE, nullptr);
    if (file == (void*)(-1LL)) return false;

    u32 bytesWritten = 0;
    i32 ok = WriteFile(file, &db, (u32)sizeof(db), &bytesWritten, nullptr);
    CloseHandle(file);
    return ok && bytesWritten == (u32)sizeof(db);
}

static void EnsureRecordsLoaded() {
    if (!g_recordsLoaded) LoadLocalRecords();
}

static void AddLocalRecord(const char* player, u32 timeMs, i32 score) {
    EnsureRecordsLoaded();

    i32 insertAt = g_localRecordCount;
    for (i32 i = 0; i < g_localRecordCount; ++i) {
        if (timeMs < g_localRecords[i].timeMs) { insertAt = i; break; }
    }
    if (g_localRecordCount >= MAX_LOCAL_RECORDS && insertAt >= MAX_LOCAL_RECORDS) return;

    i32 last = g_localRecordCount < MAX_LOCAL_RECORDS
        ? g_localRecordCount : MAX_LOCAL_RECORDS - 1;
    for (i32 i = last; i > insertAt; --i) g_localRecords[i] = g_localRecords[i - 1];

    memset(&g_localRecords[insertAt], 0, sizeof(LocalRecord));
    CopyText(g_localRecords[insertAt].player, player, MAX_PLAYER_NAME + 1);
    g_localRecords[insertAt].timeMs = timeMs;
    g_localRecords[insertAt].score = score;
    if (g_localRecordCount < MAX_LOCAL_RECORDS) ++g_localRecordCount;

    g_recordFileInvalid = 0;
    SaveLocalRecords();
}

static void BeginPasswordEntry() {
    g_screenMode = SCREEN_PASSWORD;
    g_passwordLength = 0;
    g_passwordInput[0] = 0;
    g_passwordErrorTimer = 0;
    g_sdl.StartTextInput();
}

static void FinishPlayerLogin() {
    if (g_playerNameLength <= 0) return;
    g_sdl.StopTextInput();
    g_screenMode = SCREEN_GAME;
    NewGame();
}

static void CheckRecordPassword() {
    if (TextEquals(g_passwordInput, RECORD_PASSWORD)) {
        g_sdl.StopTextInput();
        LoadLocalRecords();
        g_screenMode = SCREEN_RECORDS;
        g_passwordLength = 0;
        g_passwordInput[0] = 0;
    } else {
        g_passwordLength = 0;
        g_passwordInput[0] = 0;
        g_passwordErrorTimer = 120;
    }
}

static void AppendLoginCharacter(char c) {
    if (g_playerNameLength >= MAX_PLAYER_NAME) return;
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    bool allowed = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                   c == '-' || c == ' ';
    if (!allowed) return;
    g_playerName[g_playerNameLength++] = c;
    g_playerName[g_playerNameLength] = 0;
}

static void AppendPasswordCharacter(char c) {
    if (g_passwordLength >= MAX_PASSWORD_INPUT) return;
    if ((u8)c < 32u || (u8)c > 126u) return;
    g_passwordInput[g_passwordLength++] = c;
    g_passwordInput[g_passwordLength] = 0;
}

static void HandleTextInputEvent(const SDL_Event& event) {
    if (event.type != SDL_TEXTINPUT_EVENT) return;
    const char* text = event.text.text;
    for (i32 i = 0; text[i]; ++i) {
        if (g_screenMode == SCREEN_LOGIN) AppendLoginCharacter(text[i]);
        else if (g_screenMode == SCREEN_PASSWORD) AppendPasswordCharacter(text[i]);
    }
}

static void UpdateTextScreen(const u8* keys) {
    i32 enter = keys[SC_RETURN];
    i32 backspace = keys[SC_BACKSPACE];

    if (backspace && !g_prevBackspace) {
        if (g_screenMode == SCREEN_LOGIN && g_playerNameLength > 0) {
            g_playerName[--g_playerNameLength] = 0;
        } else if (g_screenMode == SCREEN_PASSWORD && g_passwordLength > 0) {
            g_passwordInput[--g_passwordLength] = 0;
        }
    }

    if (enter && !g_prevEnter) {
        if (g_screenMode == SCREEN_LOGIN) FinishPlayerLogin();
        else if (g_screenMode == SCREEN_PASSWORD) CheckRecordPassword();
    }

    if (g_passwordErrorTimer > 0) --g_passwordErrorTimer;
    g_prevEnter = enter;
    g_prevBackspace = backspace;
}

static i32 Abs(i32 v) { return v < 0 ? -v : v; }
static i32 Min(i32 a, i32 b) { return a < b ? a : b; }
static i32 Clamp(i32 v, i32 lo, i32 hi) { return v < lo ? lo : (v > hi ? hi : v); }
static u32 NextRandom() {
    u32 x = g_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g_rng = x;
    return x;
}

static bool Intersects(i32 ax, i32 ay, i32 aw, i32 ah,
                       i32 bx, i32 by, i32 bw, i32 bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void SetColor(u8 r, u8 g, u8 b, u8 a = 255) {
    g_sdl.SetRenderDrawColor(g_renderer, r, g, b, a);
}
static void Fill(i32 x, i32 y, i32 w, i32 h) {
    SDL_Rect r{x, y, w, h};
    g_sdl.RenderFillRect(g_renderer, &r);
}
static void Outline(i32 x, i32 y, i32 w, i32 h) {
    SDL_Rect r{x, y, w, h};
    g_sdl.RenderDrawRect(g_renderer, &r);
}

// 3x5 pixel font. Bits are row-major, high-level enough to identify in a decompiler.
static u16 Glyph(char c) {
    switch (c) {
        case 'A': return 0x7BED; case 'B': return 0x6BAE; case 'C': return 0x7927;
        case 'D': return 0x6B6E; case 'E': return 0x79A7; case 'F': return 0x79A4;
        case 'G': return 0x79EB; case 'H': return 0x5BED; case 'I': return 0x7497;
        case 'J': return 0x124E; case 'K': return 0x5BAD; case 'L': return 0x4927;
        case 'M': return 0x5FED; case 'N': return 0x5FED; case 'O': return 0x7B6F;
        case 'P': return 0x7BE4; case 'Q': return 0x7B7B; case 'R': return 0x6BAD;
        case 'S': return 0x79CF; case 'T': return 0x7492; case 'U': return 0x5B6F;
        case 'V': return 0x5B6A; case 'W': return 0x5BFF; case 'X': return 0x5AAD;
        case 'Y': return 0x5A92; case 'Z': return 0x7247;
        case '0': return 0x7B6F; case '1': return 0x2492; case '2': return 0x73E7;
        case '3': return 0x73CF; case '4': return 0x5BC9; case '5': return 0x79CF;
        case '6': return 0x79EF; case '7': return 0x7249; case '8': return 0x7BEF;
        case '9': return 0x7BCF; case '-': return 0x01C0; case ':': return 0x0410;
        case '.': return 0x0002; case '/': return 0x1248; case '!': return 0x2492;
        default: return 0;
    }
}

static void DrawChar(i32 x, i32 y, char c, i32 scale) {
    u16 bits = Glyph(c);
    for (i32 row = 0; row < 5; ++row) {
        for (i32 col = 0; col < 3; ++col) {
            i32 bit = 14 - (row * 3 + col);
            if ((bits >> bit) & 1u) Fill(x + col * scale, y + row * scale, scale, scale);
        }
    }
}
static void DrawText(i32 x, i32 y, const char* s, i32 scale) {
    i32 cursor = x;
    for (i32 i = 0; s[i]; ++i) {
        if (s[i] == ' ') cursor += scale * 2;
        else { DrawChar(cursor, y, s[i], scale); cursor += scale * 4; }
    }
}
static void DrawNumber(i32 x, i32 y, i32 value, i32 scale, i32 minDigits) {
    char buffer[16];
    i32 n = 0;
    if (value < 0) { buffer[n++] = '-'; value = -value; }
    char reverse[12];
    i32 d = 0;
    do { reverse[d++] = (char)('0' + (value % 10)); value /= 10; } while (value && d < 11);
    while (d < minDigits) reverse[d++] = '0';
    while (d > 0) buffer[n++] = reverse[--d];
    buffer[n] = 0;
    DrawText(x, y, buffer, scale);
}

static void DrawTimeMs(i32 x, i32 y, u32 timeMs, i32 scale) {
    u32 minutes = timeMs / 60000u;
    u32 seconds = (timeMs / 1000u) % 60u;
    u32 millis = timeMs % 1000u;
    DrawNumber(x, y, (i32)minutes, scale, 2);
    DrawText(x + 8 * scale, y, ":", scale);
    DrawNumber(x + 12 * scale, y, (i32)seconds, scale, 2);
    DrawText(x + 20 * scale, y, ".", scale);
    DrawNumber(x + 24 * scale, y, (i32)millis, scale, 3);
}

static void AddPlatform(i32 x, i32 y, i32 w, i32 h) {
    if (g_platformCount >= MAX_PLATFORMS) return;
    g_platforms[g_platformCount++] = Platform{x, y, w, h};
}
static void AddEnemy(i32 x, i32 platformTop, i32 leftBound, i32 rightBound, i32 dir) {
    if (g_enemyCount >= MAX_ENEMIES) return;
    Enemy& e = g_enemies[g_enemyCount++];
    e.active = 1; e.x = x; e.y = platformTop - 30; e.w = 28; e.h = 30;
    e.direction = dir; e.leftBound = leftBound; e.rightBound = rightBound; e.frozen = 0;
}
static void AddCrystal(i32 x, i32 y) {
    if (g_crystalCount >= MAX_CRYSTALS) return;
    Crystal& c = g_crystals[g_crystalCount++];
    c.active = 1; c.x = x; c.y = y; c.phase = (i32)(NextRandom() & 63u);
}

static void ClearEntities() {
    for (i32 i = 0; i < MAX_ENEMIES; ++i) g_enemies[i].active = 0;
    for (i32 i = 0; i < MAX_SHOTS; ++i) g_shots[i].active = 0;
    for (i32 i = 0; i < MAX_CRYSTALS; ++i) g_crystals[i].active = 0;
    for (i32 i = 0; i < MAX_PARTICLES; ++i) g_particles[i].active = 0;
    g_platformCount = 0; g_enemyCount = 0; g_crystalCount = 0;
}

static void ResetPlayer(bool resetLives) {
    g_player.x = 72 * FP; g_player.y = 440 * FP;
    g_player.vx = 0; g_player.vy = 0; g_player.w = 30; g_player.h = 40;
    g_player.grounded = 0; g_player.facing = 1; g_player.invincible = 100;
    g_player.attackCooldown = 0;
    if (resetLives) g_player.lives = 3;
}

static void SetupLevel(i32 level) {
    ClearEntities();
    ResetPlayer(false);
    AddPlatform(0, 510, 960, 30);

    i32 pattern = (level - 1) % 3;
    if (pattern == 0) {
        AddPlatform(80, 405, 230, 20); AddPlatform(385, 405, 190, 20); AddPlatform(650, 405, 230, 20);
        AddPlatform(210, 285, 220, 20); AddPlatform(530, 285, 220, 20); AddPlatform(370, 165, 220, 20);
        AddEnemy(130, 405, 90, 275, 1); AddEnemy(430, 405, 395, 540, -1);
        AddEnemy(680, 405, 660, 845, 1); AddEnemy(260, 285, 220, 395, 1);
        AddEnemy(570, 285, 540, 715, -1); AddEnemy(430, 165, 380, 555, 1);
        AddCrystal(180, 370); AddCrystal(480, 370); AddCrystal(760, 370);
        AddCrystal(320, 250); AddCrystal(640, 250); AddCrystal(480, 130);
    } else if (pattern == 1) {
        AddPlatform(50, 430, 180, 20); AddPlatform(290, 430, 180, 20); AddPlatform(530, 430, 180, 20); AddPlatform(770, 430, 140, 20);
        AddPlatform(145, 315, 190, 20); AddPlatform(405, 315, 150, 20); AddPlatform(625, 315, 190, 20);
        AddPlatform(275, 195, 170, 20); AddPlatform(530, 195, 170, 20);
        AddEnemy(90, 430, 60, 195, 1); AddEnemy(330, 430, 300, 435, -1); AddEnemy(570, 430, 540, 675, 1);
        AddEnemy(180, 315, 155, 300, 1); AddEnemy(445, 315, 415, 520, -1); AddEnemy(670, 315, 635, 780, 1);
        AddEnemy(320, 195, 285, 410, 1); AddEnemy(580, 195, 540, 665, -1);
        AddCrystal(140, 395); AddCrystal(380, 395); AddCrystal(620, 395); AddCrystal(825, 395);
        AddCrystal(240, 280); AddCrystal(480, 280); AddCrystal(720, 280); AddCrystal(360, 160); AddCrystal(615, 160);
    } else {
        AddPlatform(90, 420, 300, 20); AddPlatform(570, 420, 300, 20);
        AddPlatform(300, 315, 360, 20); AddPlatform(90, 205, 260, 20); AddPlatform(610, 205, 260, 20);
        AddPlatform(390, 105, 180, 20);
        AddEnemy(140, 420, 100, 355, 1); AddEnemy(270, 420, 100, 355, -1);
        AddEnemy(620, 420, 580, 835, 1); AddEnemy(750, 420, 580, 835, -1);
        AddEnemy(350, 315, 310, 625, 1); AddEnemy(520, 315, 310, 625, -1);
        AddEnemy(140, 205, 100, 315, 1); AddEnemy(670, 205, 620, 835, -1); AddEnemy(445, 105, 400, 535, 1);
        AddCrystal(210, 385); AddCrystal(690, 385); AddCrystal(400, 280); AddCrystal(560, 280);
        AddCrystal(220, 170); AddCrystal(740, 170); AddCrystal(480, 70);
    }
    g_levelClearTimer = 0;
}

static void SetupHiddenStage() {
    ClearEntities();
    ResetPlayer(false);

    // The hidden stage deliberately uses a different layout and a denser enemy set.
    AddPlatform(0, 510, 960, 30);
    AddPlatform(55, 430, 180, 20);
    AddPlatform(290, 390, 160, 20);
    AddPlatform(510, 430, 180, 20);
    AddPlatform(745, 370, 160, 20);
    AddPlatform(110, 285, 190, 20);
    AddPlatform(385, 255, 190, 20);
    AddPlatform(660, 285, 190, 20);
    AddPlatform(270, 145, 180, 20);
    AddPlatform(535, 120, 180, 20);

    AddEnemy(85, 430, 65, 220, 1);
    AddEnemy(320, 390, 300, 435, -1);
    AddEnemy(540, 430, 520, 675, 1);
    AddEnemy(775, 370, 755, 890, -1);
    AddEnemy(145, 285, 120, 285, 1);
    AddEnemy(430, 255, 395, 560, -1);
    AddEnemy(705, 285, 670, 835, 1);
    AddEnemy(305, 145, 280, 435, 1);
    AddEnemy(575, 120, 545, 700, -1);

    AddCrystal(145, 395);
    AddCrystal(370, 355);
    AddCrystal(600, 395);
    AddCrystal(825, 335);
    AddCrystal(205, 250);
    AddCrystal(480, 220);
    AddCrystal(755, 250);
    AddCrystal(360, 110);
    AddCrystal(625, 85);

    g_level = 4;
    g_levelClearTimer = 0;
}

static void EnterHiddenStage() {
    g_inHiddenStage = 1;
    g_state = 0;
    g_secretTimer = 0;
    SetupHiddenStage();
}

static void BackdoorGoToStage(i32 stage) {
    // 스테이지 이동 시 종료 화면과 히든 스테이지 상태를 초기화한다.
    g_state = 0;
    g_secretTimer = 0;
    g_levelClearTimer = 0;

    // 백도어 이동 후 바로 게임 오버가 되지 않도록 목숨을 복구한다.
    g_player.lives = 3;

    if (stage == 4) {
        // 4번은 히든 스테이지다.
        g_inHiddenStage = 1;
        SetupHiddenStage();
    }
    else if (stage >= 1 && stage <= 3) {
        g_inHiddenStage = 0;
        g_level = stage;
        SetupLevel(stage);
    }
}

static void NewGame() {
    g_ReversingScore = 0;
    g_level = 1; g_state = 0; g_secretTimer = 0;
    g_inHiddenStage = 0;
    g_runElapsedMs = 0;
    g_lastClearTimeMs = 0;
    g_recordSavedThisRun = 0;
    g_prevJump = 0;
    g_prevAttack = 0;
    g_prevRestart = 0;
    g_prevConfirm = 0;
    g_prevF6 = 0;
    g_prevF10 = 0;
    g_prevBackdoor = 0;
    ResetPlayer(true);
    SetupLevel(g_level);
}

static void SpawnParticles(i32 x, i32 y, i32 count, i32 kind) {
    for (i32 n = 0; n < count; ++n) {
        for (i32 i = 0; i < MAX_PARTICLES; ++i) {
            if (!g_particles[i].active) {
                Particle& p = g_particles[i];
                p.active = 1; p.x = x * FP; p.y = y * FP;
                p.vx = ((i32)(NextRandom() % 501u) - 250);
                p.vy = -((i32)(NextRandom() % 500u) + 150);
                p.life = 25 + (i32)(NextRandom() % 25u); p.kind = kind;
                break;
            }
        }
    }
}

static void FireShot() {
    if (g_player.attackCooldown > 0) return;
    for (i32 i = 0; i < MAX_SHOTS; ++i) {
        if (!g_shots[i].active) {
            Shot& s = g_shots[i];
            s.active = 1;
            s.x = g_player.x / FP + g_player.w / 2 + g_player.facing * 15;
            s.y = g_player.y / FP + 16;
            s.vx = g_player.facing * 9;
            s.life = 85;
            g_player.attackCooldown = 18;
            return;
        }
    }
}

static void DamagePlayer() {
    if (g_player.invincible > 0 || g_state != 0) return;
    --g_player.lives;
    SpawnParticles(g_player.x / FP + 15, g_player.y / FP + 20, 18, 2);
    if (g_player.lives <= 0) {
        g_state = 1;
        g_player.vx = 0;
        g_player.vy = 0;
    } else {
        ResetPlayer(false);
    }
}

static void MovePlayerHorizontal() {
    g_player.x += g_player.vx;
    i32 px = g_player.x / FP;
    i32 py = g_player.y / FP;
    for (i32 i = 0; i < g_platformCount; ++i) {
        Platform& p = g_platforms[i];
        if (Intersects(px, py, g_player.w, g_player.h, p.x, p.y, p.w, p.h)) {
            if (g_player.vx > 0) g_player.x = (p.x - g_player.w) * FP;
            else if (g_player.vx < 0) g_player.x = (p.x + p.w) * FP;
            g_player.vx = 0;
            px = g_player.x / FP;
        }
    }
}

static void MovePlayerVertical() {
    i32 oldBottom = g_player.y / FP + g_player.h;
    g_player.y += g_player.vy;
    g_player.grounded = 0;
    i32 px = g_player.x / FP;
    i32 py = g_player.y / FP;
    for (i32 i = 0; i < g_platformCount; ++i) {
        Platform& p = g_platforms[i];
        if (!Intersects(px, py, g_player.w, g_player.h, p.x, p.y, p.w, p.h)) continue;
        if (g_player.vy >= 0 && oldBottom <= p.y + 6) {
            g_player.y = (p.y - g_player.h) * FP;
            g_player.vy = 0; g_player.grounded = 1;
        } else if (g_player.vy < 0) {
            g_player.y = (p.y + p.h) * FP;
            g_player.vy = 0;
        }
        py = g_player.y / FP;
    }
}

static void UpdatePlayer(const u8* keys) {
    i32 left = keys[SC_A] || keys[SC_LEFT];
    i32 right = keys[SC_D] || keys[SC_RIGHT];
    i32 jump = keys[SC_SPACE] || keys[SC_UP];
    i32 attack = keys[SC_Z] || keys[SC_X];

    if (left && !right) { g_player.vx -= 70; g_player.facing = -1; }
    else if (right && !left) { g_player.vx += 70; g_player.facing = 1; }
    else g_player.vx = (g_player.vx * 72) / 100;
    g_player.vx = Clamp(g_player.vx, -480, 480);

    if (jump && !g_prevJump && g_player.grounded) {
        g_player.vy = -1280; g_player.grounded = 0;
    }
    if (attack && !g_prevAttack) FireShot();
    g_prevJump = jump; g_prevAttack = attack;

    g_player.vy += 55;
    g_player.vy = Min(g_player.vy, 950);
    MovePlayerHorizontal();
    MovePlayerVertical();

    g_player.x = Clamp(g_player.x, 0, (SCREEN_W - g_player.w) * FP);
    if (g_player.y > SCREEN_H * FP) DamagePlayer();
    if (g_player.invincible > 0) --g_player.invincible;
    if (g_player.attackCooldown > 0) --g_player.attackCooldown;
}

static void UpdateEnemies() {
    i32 px = g_player.x / FP;
    i32 py = g_player.y / FP;
    for (i32 i = 0; i < g_enemyCount; ++i) {
        Enemy& e = g_enemies[i];
        if (!e.active) continue;
        if (e.frozen > 0) {
            --e.frozen;
        } else {
            e.x += e.direction * (1 + (g_level >= 3));
            if (e.x <= e.leftBound) { e.x = e.leftBound; e.direction = 1; }
            if (e.x + e.w >= e.rightBound) { e.x = e.rightBound - e.w; e.direction = -1; }
        }
        if (Intersects(px, py, g_player.w, g_player.h, e.x, e.y, e.w, e.h)) {
            if (e.frozen > 0 && Abs(g_player.vx) > 250) {
                e.active = 0; g_ReversingScore += 350;
                SpawnParticles(e.x + e.w / 2, e.y + e.h / 2, 20, 1);
                g_player.vx = e.direction * -650;
            } else DamagePlayer();
        }
    }
}

static void UpdateShots() {
    for (i32 i = 0; i < MAX_SHOTS; ++i) {
        Shot& s = g_shots[i];
        if (!s.active) continue;
        s.x += s.vx;
        --s.life;
        if (s.life <= 0 || s.x < -20 || s.x > SCREEN_W + 20) { s.active = 0; continue; }
        for (i32 j = 0; j < g_enemyCount; ++j) {
            Enemy& e = g_enemies[j];
            if (!e.active) continue;
            if (Intersects(s.x - 6, s.y - 6, 12, 12, e.x, e.y, e.w, e.h)) {
                s.active = 0;
                if (e.frozen > 0) {
                    e.active = 0; g_ReversingScore += 250;
                    SpawnParticles(e.x + e.w / 2, e.y + e.h / 2, 16, 1);
                } else {
                    e.frozen = 240; g_ReversingScore += 100;
                    SpawnParticles(e.x + e.w / 2, e.y + e.h / 2, 10, 0);
                }
                break;
            }
        }
    }
}

static void UpdateCrystals() {
    i32 px = g_player.x / FP;
    i32 py = g_player.y / FP;
    for (i32 i = 0; i < g_crystalCount; ++i) {
        Crystal& c = g_crystals[i];
        if (!c.active) continue;
        ++c.phase;
        if (Intersects(px, py, g_player.w, g_player.h, c.x - 8, c.y - 10, 16, 20)) {
            c.active = 0; g_ReversingScore += 75;
            SpawnParticles(c.x, c.y, 10, 0);
        }
    }
}

static void UpdateParticles() {
    for (i32 i = 0; i < MAX_PARTICLES; ++i) {
        Particle& p = g_particles[i];
        if (!p.active) continue;
        p.x += p.vx; p.y += p.vy; p.vy += 35;
        if (--p.life <= 0) p.active = 0;
    }
}

static i32 RemainingEnemies() {
    i32 count = 0;
    for (i32 i = 0; i < g_enemyCount; ++i) if (g_enemies[i].active) ++count;
    return count;
}

static void DecodeSecret(char* out) {
    // "THE ICE GATE IS OPEN" XOR 0x5A
    static const u8 encoded[] = {14,18,31,122,19,25,31,122,29,27,14,31,122,19,9,122,21,10,31,20,0};
    for (i32 i = 0; i < 20; ++i) out[i] = (char)(encoded[i] ^ 0x5A);
    out[20] = 0;
}

static void TriggerOneSecondTrueEnding() {
    g_runElapsedMs = 1000u;
    g_lastClearTimeMs = 1000u;
    g_state = 4;
    g_inHiddenStage = 0;
    g_player.vx = 0;
    g_player.vy = 0;
    if (!g_recordSavedThisRun) {
        AddLocalRecord(g_playerName, 1000u, g_ReversingScore);
        g_recordSavedThisRun = 1;
    }
}

static void UpdateGame(const u8* keys) {
    i32 restart = keys[SC_R];
    i32 confirm = keys[SC_A];
    i32 f5 = keys[SC_F5];
    i32 f6 = keys[SC_F6];
    i32 f10 = keys[SC_F10];

    bool backdoorPressed = f5 &&
        (keys[SC_1] || keys[SC_2] || keys[SC_3] || keys[SC_4] || keys[SC_5]);

    if (restart && !g_prevRestart) {
        NewGame();
        g_prevRestart = restart;
        g_prevConfirm = confirm;
        g_prevBackdoor = backdoorPressed;
        g_prevF6 = f6;
        g_prevF10 = f10;
        return;
    }

    if (g_state != 4) g_runElapsedMs += 16u;

    // Existing stage test shortcuts are preserved. F5+5 is an explicit
    // one-second true-ending shortcut requested for record-system testing.
    if (backdoorPressed && !g_prevBackdoor) {
        if (keys[SC_5]) {
            TriggerOneSecondTrueEnding();
            g_prevRestart = restart;
            g_prevConfirm = confirm;
            g_prevBackdoor = backdoorPressed;
            g_prevF6 = f6;
            g_prevF10 = f10;
            return;
        } else if (keys[SC_1]) BackdoorGoToStage(1);
        else if (keys[SC_2]) BackdoorGoToStage(2);
        else if (keys[SC_3]) BackdoorGoToStage(3);
        else if (keys[SC_4]) BackdoorGoToStage(4);
    }

    if (f10 && !g_prevF10) g_DebugView = !g_DebugView;

    // F6 only opens the gate screen after the reversing condition is satisfied.
    if (f6 && !g_prevF6 && g_state != 4 && !g_inHiddenStage) {
        if (Challenge_CheckUnlock(g_IceKey) || g_ReversingScore >= 7777) {
            g_state = 3;
            g_secretTimer = 600;
        }
    }

    // A confirms entry only while the ice-gate screen is active.
    if (g_state == 3 && confirm && !g_prevConfirm) EnterHiddenStage();

    g_prevRestart = restart;
    g_prevConfirm = confirm;
    g_prevBackdoor = backdoorPressed;
    g_prevF6 = f6;
    g_prevF10 = f10;

    if (g_state == 0) {
        UpdatePlayer(keys);
        UpdateEnemies();
        UpdateShots();
        UpdateCrystals();
        UpdateParticles();

        if (RemainingEnemies() == 0) {
            ++g_levelClearTimer;
            if (g_levelClearTimer > 100) {
                if (g_inHiddenStage) {
                    if (!g_recordSavedThisRun && g_player.lives > 0) {
                        g_lastClearTimeMs = g_runElapsedMs;
                        AddLocalRecord(g_playerName, g_lastClearTimeMs, g_ReversingScore);
                        g_recordSavedThisRun = 1;
                    }
                    g_state = 4;
                    g_player.vx = 0;
                    g_player.vy = 0;
                } else {
                    ++g_level;
                    if (g_level >= g_LevelGate) g_state = 2;
                    else SetupLevel(g_level);
                }
            }
        }
    } else {
        UpdateParticles();
        if (g_state == 3 && g_secretTimer > 0) --g_secretTimer;
    }
}

// ------------------------------- Rendering ----------------------------------
static void DrawBackground() {
    SetColor(8, 18, 38); g_sdl.RenderClear(g_renderer);
    // Aurora bands and stars.
    SetColor(18, 47, 76); Fill(0, 90, SCREEN_W, 105);
    SetColor(22, 62, 86); Fill(0, 150, SCREEN_W, 82);
    SetColor(27, 75, 94); Fill(0, 210, SCREEN_W, 58);
    SetColor(125, 215, 240);
    for (i32 i = 0; i < 40; ++i) {
        u32 v = (u32)i * 2654435761u;
        i32 x = (i32)(v % 950u); i32 y = 20 + (i32)((v >> 10) % 210u);
        i32 size = (i % 7 == 0) ? 3 : 2;
        Fill(x, y, size, size);
    }
    // distant ice mountains
    SetColor(24, 83, 110);
    for (i32 x = 0; x < SCREEN_W; x += 160) {
        g_sdl.RenderDrawLine(g_renderer, x, 330, x + 80, 235);
        g_sdl.RenderDrawLine(g_renderer, x + 80, 235, x + 160, 330);
    }
    SetColor(15, 42, 65); Fill(0, 330, SCREEN_W, 180);
}

static void DrawPlatforms() {
    for (i32 i = 0; i < g_platformCount; ++i) {
        Platform& p = g_platforms[i];
        SetColor(80, 186, 218); Fill(p.x, p.y, p.w, p.h);
        SetColor(180, 244, 250); Fill(p.x, p.y, p.w, 5);
        SetColor(40, 120, 165); Fill(p.x, p.y + p.h - 5, p.w, 5);
        SetColor(115, 215, 232);
        for (i32 x = p.x + 12; x < p.x + p.w - 5; x += 36) Fill(x, p.y + 8, 15, 3);
    }
}

static void DrawPenguin(i32 x, i32 y, i32 facing, i32 flash) {
    if (flash) SetColor(225, 245, 255); else SetColor(22, 29, 43);
    Fill(x + 5, y + 2, 20, 33); Fill(x + 2, y + 10, 26, 20);
    SetColor(238, 248, 249); Fill(x + 8, y + 12, 14, 22);
    SetColor(250, 167, 45); Fill(x + 7, y + 35, 7, 4); Fill(x + 17, y + 35, 7, 4);
    Fill(x + (facing > 0 ? 24 : 0), y + 10, 6, 5);
    SetColor(77, 198, 232); Fill(x + 3, y + 17, 24, 5); Fill(x + (facing > 0 ? 24 : 1), y + 20, 5, 9);
    SetColor(240, 248, 255); Fill(x + (facing > 0 ? 18 : 9), y + 7, 3, 3);
    SetColor(8, 13, 20); Fill(x + (facing > 0 ? 19 : 10), y + 8, 2, 2);
}

static void DrawEnemy(const Enemy& e) {
    if (e.frozen > 0) {
        SetColor(93, 213, 244, 210); Fill(e.x - 3, e.y - 3, e.w + 6, e.h + 6);
        SetColor(205, 250, 255); Outline(e.x - 3, e.y - 3, e.w + 6, e.h + 6);
    }
    SetColor(176, 46, 72); Fill(e.x + 3, e.y + 5, e.w - 6, e.h - 7);
    SetColor(236, 76, 84); Fill(e.x, e.y + 10, e.w, 12);
    SetColor(247, 235, 214); Fill(e.x + 5, e.y + 8, 6, 7); Fill(e.x + 17, e.y + 8, 6, 7);
    SetColor(16, 20, 28); Fill(e.x + 7, e.y + 10, 2, 3); Fill(e.x + 19, e.y + 10, 2, 3);
    SetColor(247, 166, 52); Fill(e.x + 10, e.y + 25, 8, 4);
}

static void DrawEntities() {
    for (i32 i = 0; i < g_crystalCount; ++i) {
        Crystal& c = g_crystals[i]; if (!c.active) continue;
        i32 bob = ((c.phase / 8) & 1) ? 2 : 0;
        SetColor(106, 238, 255); Fill(c.x - 4, c.y - 10 - bob, 8, 20);
        SetColor(214, 255, 255); Fill(c.x - 1, c.y - 8 - bob, 3, 13);
    }
    for (i32 i = 0; i < g_enemyCount; ++i) if (g_enemies[i].active) DrawEnemy(g_enemies[i]);
    for (i32 i = 0; i < MAX_SHOTS; ++i) {
        Shot& s = g_shots[i]; if (!s.active) continue;
        SetColor(210, 252, 255); Fill(s.x - 6, s.y - 6, 12, 12);
        SetColor(94, 216, 245); Outline(s.x - 6, s.y - 6, 12, 12);
    }
    for (i32 i = 0; i < MAX_PARTICLES; ++i) {
        Particle& p = g_particles[i]; if (!p.active) continue;
        if (p.kind == 1) SetColor(111, 230, 255);
        else if (p.kind == 2) SetColor(247, 91, 111);
        else SetColor(218, 253, 255);
        Fill(p.x / FP, p.y / FP, 4, 4);
    }
    i32 px = g_player.x / FP; i32 py = g_player.y / FP;
    i32 flash = g_player.invincible > 0 && ((g_player.invincible / 5) & 1);
    DrawPenguin(px, py, g_player.facing, flash);
}

static void DrawHUD() {
    SetColor(6, 13, 27, 220); Fill(0, 0, SCREEN_W, 43);
    SetColor(188, 242, 250); DrawText(18, 13, "SCORE", 3); DrawNumber(108, 13, g_ReversingScore, 3, 5);
    DrawText(354, 13, "LEVEL", 3); DrawNumber(447, 13, g_level, 3, 1);
    DrawText(680, 13, "LIVES", 3); DrawNumber(773, 13, g_player.lives, 3, 1);
    SetColor(92, 202, 232); DrawText(835, 13, "ICE", 3); DrawNumber(900, 13, RemainingEnemies(), 3, 1);

    SetColor(6, 13, 27, 205); Fill(0, 43, SCREEN_W, 28);
    SetColor(145, 244, 255); DrawText(18, 51, "TIME", 2); DrawTimeMs(82, 51, g_runElapsedMs, 2);
    DrawText(310, 51, "PLAYER", 2); DrawText(405, 51, g_playerName, 2);
    SetColor(173, 224, 235); DrawText(760, 51, "F7 RECORDS", 2);

    if (g_DebugView) {
        SetColor(255, 84, 116); DrawText(12, 78, "DEBUG", 2);
        i32 px = g_player.x / FP, py = g_player.y / FP;
        Outline(px, py, g_player.w, g_player.h);
        for (i32 i = 0; i < g_enemyCount; ++i) if (g_enemies[i].active)
            Outline(g_enemies[i].x, g_enemies[i].y, g_enemies[i].w, g_enemies[i].h);
        for (i32 i = 0; i < g_platformCount; ++i)
            Outline(g_platforms[i].x, g_platforms[i].y, g_platforms[i].w, g_platforms[i].h);
    }
    if (g_DebuggerObserved) {
        SetColor(255, 80, 80); Fill(SCREEN_W - 7, SCREEN_H - 7, 4, 4);
    }
}

static void DrawOverlay() {
    if (g_state == 0 && g_frameCounter < 420) {
        SetColor(3, 12, 24, 210); Fill(225, 462, 510, 36);
        SetColor(194, 245, 252); DrawText(248, 473, "A D MOVE  SPACE JUMP  Z ICE", 2);
    }
    if (RemainingEnemies() == 0 && g_state == 0) {
        SetColor(4, 16, 32, 220); Fill(335, 230, 290, 68);
        SetColor(154, 241, 255); DrawText(375, 250, "RIFT CLEAR", 4);
    }
    if (g_state == 1) {
        SetColor(2, 8, 18, 225); Fill(0, 0, SCREEN_W, SCREEN_H);
        SetColor(255, 102, 119); DrawText(324, 190, "GAME OVER", 7);
        SetColor(208, 243, 250); DrawText(365, 290, "PRESS R", 5);
    } else if (g_state == 2) {
        SetColor(2, 12, 24, 225); Fill(0, 0, SCREEN_W, SCREEN_H);
        SetColor(126, 238, 255); DrawText(335, 175, "YOU WIN", 8);
        SetColor(173, 224, 235); DrawText(353, 320, "PRESS R", 4);
    } else if (g_state == 3) {
        char message[24]; DecodeSecret(message);
        SetColor(1, 7, 16, 235); Fill(0, 0, SCREEN_W, SCREEN_H);
        SetColor(255, 215, 86); DrawText(194, 160, "REVERSE CHALLENGE", 6);
        SetColor(145, 244, 255); DrawText(245, 270, message, 4);
        SetColor(205, 235, 241); DrawText(354, 350, "PRESS A", 4);
    } else if (g_state == 4) {
        SetColor(1, 7, 16, 240); Fill(0, 0, SCREEN_W, SCREEN_H);
        SetColor(255, 215, 86); DrawText(260, 135, "TRUE ENDING", 7);
        SetColor(145, 244, 255); DrawText(234, 235, "THE RIFT IS SEALED", 4);
        SetColor(255, 215, 86); DrawText(318, 295, "CLEAR TIME", 4);
        DrawTimeMs(430, 335, g_lastClearTimeMs, 4);
        SetColor(205, 235, 241); DrawText(275, 410, "F7 RECORDS  R RESTART", 3);
    }
}

static void DrawCursor(i32 x, i32 y, i32 height) {
    if ((g_frameCounter / 30) & 1) Fill(x, y, 3, height);
}

static void DrawLoginScreen() {
    SetColor(1, 7, 16, 225); Fill(120, 75, 720, 390);
    SetColor(255, 215, 86); DrawText(255, 110, "PLAYER LOGIN", 6);
    SetColor(145, 244, 255); DrawText(265, 205, "ENTER YOUR NAME", 4);
    SetColor(15, 42, 65); Fill(245, 265, 470, 58);
    SetColor(205, 250, 255);
    if (g_playerNameLength > 0) DrawText(275, 282, g_playerName, 4);
    else DrawText(340, 282, "TYPE NAME", 4);
    DrawCursor(275 + g_playerNameLength * 16, 278, 28);
    SetColor(205, 235, 241); DrawText(305, 380, "PRESS ENTER TO START", 3);
}

static void DrawPasswordMask(i32 x, i32 y, i32 count, i32 scale) {
    for (i32 i = 0; i < count; ++i) DrawText(x + i * 4 * scale, y, ".", scale);
}

static void DrawPasswordScreen() {
    SetColor(1, 7, 16, 232); Fill(120, 75, 720, 390);
    SetColor(255, 215, 86); DrawText(225, 110, "RECORD PASSWORD", 6);
    SetColor(145, 244, 255); DrawText(250, 205, "ENTER PASSWORD", 4);
    SetColor(15, 42, 65); Fill(245, 265, 470, 58);
    SetColor(205, 250, 255); DrawPasswordMask(275, 282, g_passwordLength, 4);
    DrawCursor(275 + g_passwordLength * 16, 278, 28);
    if (g_passwordErrorTimer > 0) {
        SetColor(255, 102, 119); DrawText(320, 345, "WRONG PASSWORD", 4);
    } else {
        SetColor(205, 235, 241); DrawText(270, 370, "ENTER OPEN  F7 CANCEL", 3);
    }
}

static void DrawRecordsScreen() {
    SetColor(1, 7, 16, 232); Fill(70, 45, 820, 450);
    SetColor(255, 215, 86); DrawText(280, 70, "LOCAL RECORDS", 5);
    SetColor(145, 244, 255);
    DrawText(150, 125, "PLAYER", 3);
    DrawText(430, 125, "TIME", 3);
    DrawText(670, 125, "SCORE", 3);

    if (g_recordFileInvalid) {
        SetColor(255, 102, 119); DrawText(275, 255, "RECORD FILE INVALID", 4);
    } else if (g_localRecordCount == 0) {
        SetColor(173, 224, 235); DrawText(355, 255, "NO RECORDS", 4);
    } else {
        for (i32 i = 0; i < g_localRecordCount; ++i) {
            i32 y = 165 + i * 27;
            SetColor(205, 250, 255);
            DrawNumber(105, y, i + 1, 2, 2);
            DrawText(150, y, g_localRecords[i].player, 2);
            DrawTimeMs(430, y, g_localRecords[i].timeMs, 2);
            DrawNumber(670, y, g_localRecords[i].score, 2, 5);
        }
    }
    SetColor(205, 235, 241); DrawText(365, 460, "F7 CLOSE", 3);
}

static void RenderGame() {
    DrawBackground();
    if (g_screenMode == SCREEN_LOGIN) {
        DrawLoginScreen();
    } else if (g_screenMode == SCREEN_PASSWORD) {
        DrawPasswordScreen();
    } else if (g_screenMode == SCREEN_RECORDS) {
        DrawRecordsScreen();
    } else {
        DrawPlatforms(); DrawEntities(); DrawHUD(); DrawOverlay();
    }
    g_sdl.RenderPresent(g_renderer);
}

// Entry point kept explicit to avoid the Microsoft CRT and make the import table small.
extern "C" void mainCRTStartup() {
    (void)Challenge_GetMarker(0);
    if (!LoadSDL2()) {
        MessageBoxA(nullptr,
            "SDL2.dll was not found. Run RUN_GAME.bat once, or place the 64-bit SDL2.dll beside PenguinRift.exe.",
            "Penguin Rift - missing SDL2.dll", 0x10u);
        ExitProcess(2);
    }
    if (g_sdl.Init(SDL_INIT_VIDEO) != 0) {
        MessageBoxA(nullptr, "SDL2 video initialization failed.", "Penguin Rift", 0x10u);
        ExitProcess(3);
    }

    g_window = g_sdl.CreateWindow("Penguin Rift - SDL2 Reversing Lab",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!g_window) { g_sdl.Quit(); ExitProcess(4); }
    g_renderer = g_sdl.CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) g_renderer = g_sdl.CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    if (!g_renderer) { g_sdl.DestroyWindow(g_window); g_sdl.Quit(); ExitProcess(5); }

    g_sdl.SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    g_DebuggerObserved = IsDebuggerPresent();
    ClearRecordMemory();
    g_screenMode = SCREEN_LOGIN;
    g_playerName[0] = 0;
    g_passwordInput[0] = 0;
    g_sdl.StartTextInput();

    bool running = true;
    u32 previous = g_sdl.GetTicks();
    u32 accumulator = 0;
    while (running) {
        SDL_Event event;
        while (g_sdl.PollEvent(&event)) {
            if (event.type == SDL_QUIT_EVENT) running = false;
            else HandleTextInputEvent(event);
        }
        i32 keyCount = 0;
        const u8* keys = g_sdl.GetKeyboardState(&keyCount);
        if (keys && keyCount > SC_ESCAPE && keys[SC_ESCAPE]) running = false;

        u32 now = g_sdl.GetTicks();
        u32 elapsed = now - previous; previous = now;
        if (elapsed > 100u) elapsed = 100u;
        accumulator += elapsed;
        while (accumulator >= 16u) {
            ++g_frameCounter;
            if (keys && keyCount > SC_UP) {
                i32 f7 = keys[SC_F7];
                if (g_screenMode == SCREEN_GAME) {
                    if (f7 && !g_prevF7) BeginPasswordEntry();
                    else UpdateGame(keys);
                } else if (g_screenMode == SCREEN_RECORDS) {
                    if (f7 && !g_prevF7) g_screenMode = SCREEN_GAME;
                } else if (g_screenMode == SCREEN_PASSWORD) {
                    if (f7 && !g_prevF7) {
                        g_sdl.StopTextInput();
                        g_screenMode = SCREEN_GAME;
                    } else UpdateTextScreen(keys);
                } else {
                    UpdateTextScreen(keys);
                }
                g_prevF7 = f7;
            }
            accumulator -= 16u;
        }
        RenderGame();
        g_sdl.Delay(1);
    }

    g_sdl.DestroyRenderer(g_renderer);
    g_sdl.DestroyWindow(g_window);
    g_sdl.Quit();
    ExitProcess(0);
}
