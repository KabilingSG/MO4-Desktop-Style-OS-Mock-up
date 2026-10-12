// CSOPESY Desktop OS Emulator  -  Semi-Major Output 2
// Author: Simon Gabriel M. Kabiling (S01)
// Stack: GLFW + OpenGL3 + Dear ImGui (immediate mode)
//
// Entry point: main() in this file (src/main.cpp)
//
// Frame layout (the "compositor"):
//   1. Wallpaper   -> background draw list (always first, fills window)
//   2. Clock       -> fixed top-right overlay
//   3. Taskbar     -> fixed bottom panel (icon buttons + tray + PWR)
//   4. App windows -> File Explorer, Settings, Terminal, Task Manager
//
// Boot flow: BIOS POST -> Splash/Loading -> Desktop -> (PWR) -> Shutdown -> exit

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------
static std::tm LocalNow() {
    std::time_t t = std::time(nullptr);
    std::tm r{};
#ifdef _WIN32
    localtime_s(&r, &t);
#else
    localtime_r(&t, &r);
#endif
    return r;
}

static float Frand(float a, float b) { return a + (b - a) * (std::rand() / (float)RAND_MAX); }

static ImU32 Col(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }

// ----------------------------------------------------------------------------
// OS state
// ----------------------------------------------------------------------------
enum class Stage { Bios, Splash, Desktop, Shutdown };

struct OsState {
    Stage stage = Stage::Bios;
    double stageStart = 0.0;

    // app windows
    bool showFiles = false;
    bool showSettings = false;
    bool showTerminal = false;
    bool showTaskMgr = false;
    bool focusFiles = false, focusSettings = false, focusTerminal = false, focusTaskMgr = false;

    // settings
    int wallpaper = 0;       // 0 = Bliss, 1 = Gradient, 2 = Pattern
    bool clock24h = false;
    float brightness = 1.0f;
    float volume = 0.65f;
    bool muted = false;
    bool netOn = true;
    bool showVolPopup = false;
    bool showNetPopup = false;

    int lastFocused = -1;    // id of the app window that last had focus (0 files, 1 settings, 2 taskmgr, 3 terminal)
};

static OsState g;

static const float kTaskbarH = 52.0f;

// ----------------------------------------------------------------------------
// Boot: BIOS POST screen
// ----------------------------------------------------------------------------
static void DrawBios(double t) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, Col(0, 0, 0));

    // Lines are revealed over time to mimic a POST sequence.
    struct Line { double at; std::string text; ImU32 col; };
    int ram = (int)std::min(64000.0, std::max(0.0, (t - 1.0) * 40000.0));
    char ramBuf[96];
    if (t < 2.6) std::snprintf(ramBuf, sizeof ramBuf, "Checking RAM : %dK", ram);
    else         std::snprintf(ramBuf, sizeof ramBuf, "Checking RAM : 64000K OK");

    std::vector<Line> lines = {
        {0.0, "CSOPESY S01", Col(255, 255, 255)},
        {0.1, "Released: 10/12/2026", Col(255, 255, 255)},
        {0.2, "KABILING LABS (C)2026 KABILING INC.", Col(255, 255, 255)},
        {0.3, "", Col(255, 255, 255)},
        {0.8, "Memory Test:", Col(255, 255, 255)},
        {1.0, ramBuf, Col(255, 255, 255)},
        {2.8, "", Col(255, 255, 255)},
        {3.0, "CPU Type: Pentium III", Col(255, 255, 255)},
        {3.2, "BIOS Version: BCN SIT 1989-1994 Special CSOPESY", Col(255, 255, 255)},
        {3.4, "Main Processor: Pentium III", Col(255, 255, 255)},
        {3.6, "Numeric Processor: Built-in", Col(255, 255, 255)},
        {3.7, "", Col(255, 255, 255)},
        {4.0, "Primary Master:    420 MB WDCAC2420H", Col(255, 255, 255)},
        {4.2, "Primary Slave:     None", Col(255, 255, 255)},
        {4.4, "Secondary Master:  CD-ROM LTN-385A", Col(255, 255, 255)},
        {4.6, "Secondary Slave:   None", Col(255, 255, 255)},
        {4.7, "", Col(255, 255, 255)},
        {5.0, "Fun Fact: The CPU specs is a tribute to Bemani Python 2 Engine, the OS that powers up Dance Dance Revolution SuperNova series.", Col(200, 200, 200)},
        {5.1, "It was essentially a retail Sony PlayStation 2 (SCPH-50000) inside a metal box with a specialized I/O board!", Col(200, 200, 200)},
    };

    ImVec2 p(18, 14);
    float lh = ImGui::GetTextLineHeightWithSpacing() * 1.1f;
    for (auto& l : lines) {
        if (t >= l.at) dl->AddText(p, l.col, l.text.c_str());
        p.y += lh;
    }
    if (t > 5.8) g.stage = Stage::Splash, g.stageStart = ImGui::GetTime();
}

// ----------------------------------------------------------------------------
// Boot: splash / loading screen
// ----------------------------------------------------------------------------
static void DrawSplash(double t) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, Col(0, 0, 0));

    // ASCII logo built from per-letter glyphs (5 rows each)
    static const char* glyphs[7][5] = {
        {"  ____ ", " / ___|", "| |    ", "| |___ ", " \\____|"},  // C
        {" ____  ", "/ ___| ", "\\___ \\ ", " ___) |", "|____/ "},  // S
        {"  ___  ", " / _ \\ ", "| | | |", "| |_| |", " \\___/ "},  // O
        {" ____  ", "|  _ \\ ", "| |_) |", "|  __/ ", "|_|    "},  // P
        {" _____ ", "| ____|", "|  _|  ", "| |___ ", "|_____|"},  // E
        {" ____  ", "/ ___| ", "\\___ \\ ", " ___) |", "|____/ "},  // S
        {"__   __", "\\ \\ / /", " \\ V / ", "  | |  ", "  |_|  "},  // Y
    };
    std::string rows[5];
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 7; c++) rows[r] += glyphs[c][r];

    float lh = ImGui::GetTextLineHeight() * 1.0f;
    float cx = io.DisplaySize.x * 0.5f;
    float y = io.DisplaySize.y * 0.30f;
    for (int r = 0; r < 5; r++) {
        ImVec2 sz = ImGui::CalcTextSize(rows[r].c_str());
        dl->AddText(ImVec2(cx - sz.x * 0.5f, y), Col(40, 170, 230), rows[r].c_str());
        y += lh;
    }
    y += lh * 1.5f;

    const char* info[] = {
        "CSOPESY Operating System Emulator v1.0",
        "Created By: Simon Gabriel M. Kabiling",
        "",
        "In partial fulfillment of the requirements for the course CSOPESY",
        "CSOPESY S01",
    };
    for (const char* s : info) {
        ImVec2 sz = ImGui::CalcTextSize(s);
        dl->AddText(ImVec2(cx - sz.x * 0.5f, y), Col(230, 230, 230), s);
        y += lh * 1.2f;
    }
    y += lh * 2.0f;
    int dots = (int)(t * 3.0) % 4;
    std::string load = "Loading" + std::string(dots, '.');
    ImVec2 sz = ImGui::CalcTextSize("Loading...");
    dl->AddText(ImVec2(cx - sz.x * 0.5f, y), Col(60, 220, 90), load.c_str());

    if (t > 3.5) g.stage = Stage::Desktop, g.stageStart = ImGui::GetTime();
}

// ----------------------------------------------------------------------------
// Desktop: wallpaper (drawn with ImGui draw commands, no textures needed)
// ----------------------------------------------------------------------------
static void DrawWallpaper(double t) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float W = io.DisplaySize.x, H = io.DisplaySize.y;

    if (g.wallpaper == 0) {
        // "Bliss"-style: sky gradient, drifting clouds, rolling green hill
        dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(W, H), Col(36, 110, 220), Col(36, 110, 220),
                                    Col(150, 205, 250), Col(150, 205, 250));
        // clouds (deterministic positions, slow horizontal drift)
        for (int i = 0; i < 9; i++) {
            float seed = i * 97.31f;
            float speed = 6.0f + (i % 4) * 3.0f;
            float x = std::fmod(seed * 13.0f + (float)t * speed, W + 400.0f) - 200.0f;
            float y = H * (0.08f + 0.5f * std::fmod(seed * 0.0173f, 1.0f) * 0.7f);
            float s = 24.0f + std::fmod(seed, 28.0f);
            for (int k = 0; k < 6; k++) {
                float ox = (k - 2.5f) * s * 0.8f;
                float oy = std::sin(k * 1.7f + seed) * s * 0.25f;
                dl->AddCircleFilled(ImVec2(x + ox, y + oy), s * (0.8f + 0.1f * (k % 3)), Col(255, 255, 255, 120), 24);
            }
        }
        // hill
        float base = H * 0.62f;
        for (float x = 0; x < W; x += 3.0f) {
            float u = x / W;
            float hy = base + std::sin(u * 3.1f + 0.6f) * H * 0.07f + (1.0f - u) * H * 0.05f;
            dl->AddRectFilledMultiColor(ImVec2(x, hy), ImVec2(x + 3.0f, H), Col(120, 190, 50), Col(120, 190, 50),
                                        Col(30, 100, 20), Col(30, 100, 20));
        }
    } else if (g.wallpaper == 1) {
        // animated colour gradient
        float s = (float)(0.5 + 0.5 * std::sin(t * 0.3));
        ImU32 a = Col((int)(20 + 40 * s), 30, (int)(90 + 60 * s));
        ImU32 b = Col(10, (int)(80 + 60 * (1 - s)), 120);
        ImU32 c = Col(60, 20, (int)(110 + 60 * s));
        ImU32 d = Col(8, 10, 40);
        dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(W, H), a, b, d, c);
    } else {
        // pattern: dark grid with moving highlights
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), Col(14, 16, 28));
        const float step = 48.0f;
        for (float x = 0; x < W; x += step) dl->AddLine(ImVec2(x, 0), ImVec2(x, H), Col(40, 50, 90, 140));
        for (float y = 0; y < H; y += step) dl->AddLine(ImVec2(0, y), ImVec2(W, y), Col(40, 50, 90, 140));
        for (int i = 0; i < 14; i++) {
            float x = std::fmod(i * 173.0f + (float)t * (20 + i * 3), W);
            float y = std::fmod(i * 91.0f + (float)t * (12 + i * 2), H);
            dl->AddCircleFilled(ImVec2(x, y), 4.0f + (i % 3) * 2.0f, Col(80, 200, 255, 160));
        }
    }

    // brightness overlay
    if (g.brightness < 1.0f)
        dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, Col(0, 0, 0, (int)((1.0f - g.brightness) * 220)));

    // OS watermark (bottom-left above taskbar)
    const char* tag = "CSOPESY OS v1.0  -  System Online";
    dl->AddText(ImVec2(14, H - kTaskbarH - ImGui::GetTextLineHeight() - 10), Col(60, 230, 100), tag);
}

// ----------------------------------------------------------------------------
// Desktop: real-time clock (top-right, recomputed every frame)
// ----------------------------------------------------------------------------
static void DrawClock() {
    ImGuiIO& io = ImGui::GetIO();
    std::tm tmv = LocalNow();
    char date[64], tbuf[32], full[128];
    std::strftime(date, sizeof date, "%A, %b %d, %Y", &tmv);
    std::strftime(tbuf, sizeof tbuf, g.clock24h ? "%H:%M:%S" : "%I:%M:%S %p", &tmv);
    std::snprintf(full, sizeof full, "%s | %s", date, tbuf);

    ImVec2 sz = ImGui::CalcTextSize(full);
    ImVec2 pad(10, 6);
    ImVec2 pos(io.DisplaySize.x - sz.x - pad.x * 2 - 8, 8);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + sz.x + pad.x * 2, pos.y + sz.y + pad.y * 2), Col(10, 12, 20, 210), 6.0f);
    dl->AddRect(pos, ImVec2(pos.x + sz.x + pad.x * 2, pos.y + sz.y + pad.y * 2), Col(255, 255, 255, 50), 6.0f);
    dl->AddText(ImVec2(pos.x + pad.x, pos.y + pad.y), Col(240, 240, 240), full);
}

// ----------------------------------------------------------------------------
// Taskbar
// ----------------------------------------------------------------------------
enum class Icon { Folder, Gear, Terminal, Chart };

static void DrawIconShape(ImDrawList* dl, Icon ic, ImVec2 a, ImVec2 b) {
    ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
    float w = b.x - a.x, h = b.y - a.y;
    switch (ic) {
    case Icon::Folder:
        dl->AddRectFilled(ImVec2(c.x - w * 0.28f, c.y - h * 0.26f), ImVec2(c.x - w * 0.02f, c.y - h * 0.14f), Col(240, 190, 40), 2.0f);
        dl->AddRectFilled(ImVec2(c.x - w * 0.28f, c.y - h * 0.18f), ImVec2(c.x + w * 0.28f, c.y + h * 0.24f), Col(255, 205, 60), 3.0f);
        dl->AddRectFilled(ImVec2(c.x - w * 0.28f, c.y - h * 0.04f), ImVec2(c.x + w * 0.28f, c.y + h * 0.24f), Col(60, 150, 235), 3.0f);
        break;
    case Icon::Gear: {
        float r = w * 0.2f;
        for (int i = 0; i < 8; i++) {
            float an = i * 3.14159265f / 4.0f;
            dl->AddLine(ImVec2(c.x + std::cos(an) * r, c.y + std::sin(an) * r),
                        ImVec2(c.x + std::cos(an) * (r + w * 0.12f), c.y + std::sin(an) * (r + w * 0.12f)), Col(190, 200, 215), 4.0f);
        }
        dl->AddCircleFilled(c, r + 1.0f, Col(190, 200, 215), 20);
        dl->AddCircleFilled(c, r * 0.45f, Col(30, 34, 48), 16);
        break;
    }
    case Icon::Terminal:
        dl->AddRectFilled(ImVec2(c.x - w * 0.3f, c.y - h * 0.22f), ImVec2(c.x + w * 0.3f, c.y + h * 0.22f), Col(15, 15, 20), 3.0f);
        dl->AddRect(ImVec2(c.x - w * 0.3f, c.y - h * 0.22f), ImVec2(c.x + w * 0.3f, c.y + h * 0.22f), Col(120, 130, 150), 3.0f);
        dl->AddText(ImVec2(c.x - w * 0.22f, c.y - ImGui::GetTextLineHeight() * 0.5f), Col(70, 230, 110), ">_");
        break;
    case Icon::Chart: {
        dl->AddRectFilled(ImVec2(c.x - w * 0.3f, c.y - h * 0.24f), ImVec2(c.x + w * 0.3f, c.y + h * 0.24f), Col(20, 60, 110), 3.0f);
        ImVec2 pts[6];
        float ys[6] = {0.15f, -0.05f, 0.1f, -0.2f, 0.0f, -0.12f};
        for (int i = 0; i < 6; i++) pts[i] = ImVec2(c.x - w * 0.26f + i * w * 0.104f, c.y + ys[i] * h);
        dl->AddPolyline(pts, 6, Col(90, 220, 255), 0, 2.0f);
        break;
    }
    }
}

// Draws an icon button; returns true if clicked.
static bool TaskbarIcon(const char* id, Icon ic, bool running, const char* tip) {
    ImVec2 sz(40, 40);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, sz);
    bool hov = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg = hov ? Col(255, 255, 255, 45) : (running ? Col(255, 255, 255, 22) : Col(255, 255, 255, 8));
    dl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y), bg, 8.0f);
    DrawIconShape(dl, ic, p, ImVec2(p.x + sz.x, p.y + sz.y));
    if (running)  // "running application" indicator
        dl->AddRectFilled(ImVec2(p.x + 10, p.y + sz.y - 4), ImVec2(p.x + sz.x - 10, p.y + sz.y - 2), Col(90, 200, 255), 2.0f);
    if (hov) ImGui::SetTooltip("%s", tip);
    return clicked;
}

static bool TrayText(const char* label, ImU32 col) {
    ImVec2 sz = ImGui::CalcTextSize(label);
    ImVec2 box(sz.x + 18, 36);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, box);
    bool hov = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hov) dl->AddRectFilled(p, ImVec2(p.x + box.x, p.y + box.y), Col(255, 255, 255, 30), 6.0f);
    dl->AddText(ImVec2(p.x + 9, p.y + (box.y - sz.y) * 0.5f), col, label);
    return clicked;
}

static void DrawTaskbar() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - kTaskbarH));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, kTaskbarH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Col(14, 16, 26, 245));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##taskbar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);

    // Clicking an icon: closed -> open + focus; open but behind -> bring to front;
    // open and already in front -> close (like a real taskbar).
    auto toggle = [](bool& show, bool& focus, int id) {
        if (show && g.lastFocused == id) show = false;
        else { show = true; focus = true; }
    };

    // Button order matters for the quiz: 1st = File Explorer, 2nd = Settings, 3rd = Task Manager.
    ImGui::SetCursorPosY(6);
    if (TaskbarIcon("files", Icon::Folder, g.showFiles, "File Explorer")) toggle(g.showFiles, g.focusFiles, 0);
    ImGui::SameLine();
    if (TaskbarIcon("settings", Icon::Gear, g.showSettings, "Settings")) toggle(g.showSettings, g.focusSettings, 1);
    ImGui::SameLine();
    if (TaskbarIcon("taskmgr", Icon::Chart, g.showTaskMgr, "Task Manager")) toggle(g.showTaskMgr, g.focusTaskMgr, 2);
    ImGui::SameLine();
    if (TaskbarIcon("terminal", Icon::Terminal, g.showTerminal, "Terminal")) toggle(g.showTerminal, g.focusTerminal, 3);

    // system tray, right aligned: VOL  NET  PWR
    float trayW = 0;
    const char* labels[3] = {"VOL", "NET", "PWR"};
    for (auto* l : labels) trayW += ImGui::CalcTextSize(l).x + 18 + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine(io.DisplaySize.x - trayW - 14);
    ImGui::SetCursorPosY(8);
    if (TrayText(g.muted ? "MUTE" : "VOL", Col(220, 220, 230))) { g.showVolPopup = true; ImGui::OpenPopup("volpop"); }
    ImGui::SameLine();
    ImGui::SetCursorPosY(8);
    if (TrayText("NET", g.netOn ? Col(220, 220, 230) : Col(140, 140, 150))) { g.showNetPopup = true; ImGui::OpenPopup("netpop"); }
    ImGui::SameLine();
    ImGui::SetCursorPosY(8);
    if (TrayText("PWR", Col(255, 70, 70))) {
        g.stage = Stage::Shutdown;
        g.stageStart = ImGui::GetTime();
    }

    if (ImGui::BeginPopup("volpop")) {
        ImGui::Text("Volume");
        ImGui::SliderFloat("##vol", &g.volume, 0.0f, 1.0f, "%.2f");
        ImGui::Checkbox("Mute", &g.muted);
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("netpop")) {
        ImGui::Text("Network: %s", g.netOn ? "CSOPESY-LAN (Connected)" : "Disconnected");
        ImGui::Checkbox("Enabled", &g.netOn);
        ImGui::EndPopup();
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}

// ----------------------------------------------------------------------------
// App: File Explorer (placeholder UI #1)
// ----------------------------------------------------------------------------
static void DrawFileExplorer() {
    if (!g.showFiles) return;
    ImGui::SetNextWindowPos(ImVec2(80, 70), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620, 380), ImGuiCond_FirstUseEver);
    if (g.focusFiles) { ImGui::SetNextWindowFocus(); g.focusFiles = false; }
    bool visible = ImGui::Begin("File Explorer", &g.showFiles);
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) g.lastFocused = 0;
    if (visible) {
        static int selDir = 0;
        static int selFile = -1;
        const char* dirs[] = {"Desktop", "Documents", "Downloads", "Pictures", "Music", "Games"};
        struct F { const char* name; const char* type; const char* size; const char* mod; };
        static const F files[] = {
            {"CSOPESY_Notes.txt", "Text Document", "4 KB", "09/18/2026"},
            {"MO2_Specs.pdf", "PDF File", "812 KB", "09/18/2026"},
            {"scheduler.cpp", "C++ Source", "21 KB", "10/02/2026"},
            {"anito_engine.zip", "Archive", "48.2 MB", "09/30/2026"},
            {"wallpaper_bliss.png", "PNG Image", "1.4 MB", "08/12/2026"},
            {"readme.txt", "Text Document", "1 KB", "10/10/2026"},
        };
        ImGui::BeginChild("tree", ImVec2(150, 0), true);
        ImGui::TextDisabled("This PC");
        ImGui::Separator();
        for (int i = 0; i < 6; i++)
            if (ImGui::Selectable(dirs[i], selDir == i)) selDir = i;
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Text("This PC > %s", dirs[selDir]);
        ImGui::Separator();
        if (ImGui::BeginTable("files", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Modified");
            ImGui::TableHeadersRow();
            for (int i = 0; i < 6; i++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Selectable(files[i].name, selFile == i, ImGuiSelectableFlags_SpanAllColumns)) selFile = i;
                ImGui::TableNextColumn(); ImGui::TextUnformatted(files[i].type);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(files[i].size);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(files[i].mod);
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::TextDisabled("6 items   |   %s", selFile >= 0 ? files[selFile].name : "no selection");
        ImGui::EndGroup();
    }
    ImGui::End();
}

// ----------------------------------------------------------------------------
// App: Settings (placeholder UI #2)
// ----------------------------------------------------------------------------
static void DrawSettings() {
    if (!g.showSettings) return;
    ImGui::SetNextWindowPos(ImVec2(260, 110), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(480, 360), ImGuiCond_FirstUseEver);
    if (g.focusSettings) { ImGui::SetNextWindowFocus(); g.focusSettings = false; }
    bool visible = ImGui::Begin("Settings", &g.showSettings);
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) g.lastFocused = 1;
    if (visible) {
        if (ImGui::BeginTabBar("settingsTabs")) {
            if (ImGui::BeginTabItem("Personalization")) {
                ImGui::Text("Wallpaper");
                ImGui::RadioButton("Bliss (drawn)", &g.wallpaper, 0);
                ImGui::RadioButton("Animated gradient", &g.wallpaper, 1);
                ImGui::RadioButton("Grid pattern", &g.wallpaper, 2);
                ImGui::Separator();
                ImGui::SliderFloat("Brightness", &g.brightness, 0.2f, 1.0f, "%.2f");
                ImGui::Checkbox("24-hour clock", &g.clock24h);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("System")) {
                ImGui::Text("Device name:   CSOPESY-PC");
                ImGui::Text("OS:            CSOPESY OS v1.0");
                ImGui::Text("Processor:     Pentium III (emulated)");
                ImGui::Text("Installed RAM: 64000 KB");
                ImGui::Text("Display:       %.0f x %.0f", ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
                ImGui::Text("Frame rate:    %.1f FPS", ImGui::GetIO().Framerate);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Sound & Network")) {
                ImGui::SliderFloat("Master volume", &g.volume, 0.0f, 1.0f);
                ImGui::Checkbox("Mute", &g.muted);
                ImGui::Checkbox("Network enabled", &g.netOn);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("About")) {
                ImGui::TextWrapped("CSOPESY Operating System Emulator v1.0\nBuilt with GLFW + OpenGL + Dear ImGui.\n"
                                   "Part of Project Anito.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

// ----------------------------------------------------------------------------
// App: Terminal (extra, small interactive demo)
// ----------------------------------------------------------------------------
static void DrawTerminal() {
    if (!g.showTerminal) return;
    ImGui::SetNextWindowPos(ImVec2(420, 150), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 320), ImGuiCond_FirstUseEver);
    if (g.focusTerminal) { ImGui::SetNextWindowFocus(); g.focusTerminal = false; }
    static std::vector<std::string> log = {"CSOPESY Terminal v1.0", "Type 'help' for a list of commands.", ""};
    static char input[128] = "";
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Col(10, 10, 14, 245));
    bool visible = ImGui::Begin("Terminal", &g.showTerminal);
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) g.lastFocused = 3;
    if (visible) {
        float footer = ImGui::GetFrameHeightWithSpacing() + 4;
        ImGui::BeginChild("log", ImVec2(0, -footer), false);
        ImGui::PushStyleColor(ImGuiCol_Text, Col(90, 230, 120));
        for (auto& l : log) ImGui::TextUnformatted(l.c_str());
        ImGui::PopStyleColor();
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::Text("C:\\>");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##cmd", input, sizeof input, ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string cmd = input;
            log.push_back("C:\\> " + cmd);
            if (cmd == "help") {
                log.push_back("  help  clear  ver  date  ps  echo <text>");
            } else if (cmd == "clear") {
                log.clear();
            } else if (cmd == "ver") {
                log.push_back("  CSOPESY OS v1.0");
            } else if (cmd == "date") {
                std::tm t = LocalNow(); char b[64];
                std::strftime(b, sizeof b, "  %Y-%m-%d %H:%M:%S", &t);
                log.push_back(b);
            } else if (cmd == "ps") {
                log.push_back("  PID   NAME");
                log.push_back("  0004  System");
                log.push_back("  0120  compositor.exe");
                log.push_back("  0348  shell.exe");
            } else if (cmd.rfind("echo ", 0) == 0) {
                log.push_back("  " + cmd.substr(5));
            } else if (!cmd.empty()) {
                log.push_back("  '" + cmd + "' is not recognized.");
            }
            input[0] = '\0';
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

// ----------------------------------------------------------------------------
// App: Task Manager (required) - dummy process table + performance graphs
// ----------------------------------------------------------------------------
struct Proc {
    const char* name;
    const char* type;
    float cpuBase;   // %
    float memBase;   // MB
    float cpu;       // current (jittered)
    float mem;
    bool ended;
};

static void DrawTaskManager() {
    static std::vector<Proc> procs = {
        {"System", "Background", 2.0f, 0.1f, 0, 0, false},
        {"csopesy_compositor.exe", "App", 8.0f, 142.5f, 0, 0, false},
        {"imgui_renderer.exe", "App", 6.5f, 96.2f, 0, 0, false},
        {"shell.exe", "App", 1.2f, 38.4f, 0, 0, false},
        {"File Explorer", "App", 0.8f, 54.0f, 0, 0, false},
        {"Settings", "App", 0.2f, 28.7f, 0, 0, false},
        {"Terminal", "App", 0.4f, 21.3f, 0, 0, false},
        {"Task Manager", "App", 1.5f, 33.9f, 0, 0, false},
        {"audio_service.exe", "Background", 0.5f, 17.6f, 0, 0, false},
        {"network_service.exe", "Background", 0.9f, 24.1f, 0, 0, false},
        {"scheduler.exe", "Background", 3.2f, 12.8f, 0, 0, false},
        {"memory_manager.exe", "Background", 1.0f, 64.0f, 0, 0, false},
        {"anito_engine.exe", "App", 11.0f, 210.0f, 0, 0, false},
    };
    static const int kHist = 90;
    static float cpuHist[kHist] = {0}, memHist[kHist] = {0};
    static double lastUpdate = -1.0;
    static int sel = -1;

    double now = ImGui::GetTime();
    if (lastUpdate < 0 || now - lastUpdate > 0.5) {  // refresh dummy numbers twice a second
        lastUpdate = now;
        float totCpu = 0, totMem = 0;
        for (auto& p : procs) {
            p.cpu = p.ended ? 0.0f : std::max(0.0f, p.cpuBase * Frand(0.6f, 1.5f));
            p.mem = p.ended ? 0.0f : p.memBase * Frand(0.97f, 1.04f);
            totCpu += p.cpu; totMem += p.mem;
        }
        std::move(cpuHist + 1, cpuHist + kHist, cpuHist);
        std::move(memHist + 1, memHist + kHist, memHist);
        cpuHist[kHist - 1] = std::min(100.0f, totCpu);
        memHist[kHist - 1] = std::min(100.0f, totMem / 640.0f * 100.0f);
    }

    if (!g.showTaskMgr) return;
    ImGui::SetNextWindowPos(ImVec2(140, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700, 480), ImGuiCond_FirstUseEver);
    if (g.focusTaskMgr) { ImGui::SetNextWindowFocus(); g.focusTaskMgr = false; }

    bool visible = ImGui::Begin("Task Manager", &g.showTaskMgr);
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) g.lastFocused = 2;
    if (visible) {
        float totCpu = 0, totMem = 0;
        for (auto& p : procs) { totCpu += p.cpu; totMem += p.mem; }
        totCpu = std::min(100.0f, totCpu);
        float memPct = std::min(100.0f, totMem / 640.0f * 100.0f);

        if (ImGui::BeginTabBar("tmTabs")) {
            if (ImGui::BeginTabItem("Processes")) {
                ImGui::TextDisabled("Name, type, CPU and memory usage (dummy values)");
                if (ImGui::BeginTable("procTable", 4,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Resizable,
                        ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.5f))) {
                    ImGui::TableSetupScrollFreeze(0, 2);
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.0f);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.4f);
                    ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthStretch, 1.2f);
                    ImGui::TableHeadersRow();
                    // summary row, like Windows' column totals
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextDisabled("Total");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn(); ImGui::TextDisabled("%.0f%%", totCpu);
                    ImGui::TableNextColumn(); ImGui::TextDisabled("%.0f%%", memPct);
                    for (int i = 0; i < (int)procs.size(); i++) {
                        Proc& p = procs[i];
                        if (p.ended) continue;
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(p.name, sel == i, ImGuiSelectableFlags_SpanAllColumns)) sel = i;
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(p.type);
                        ImGui::TableNextColumn();
                        // heat-coloured cell like Windows
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, Col(255, 190, 40, (int)std::min(160.0f, p.cpu * 12.0f)));
                        ImGui::Text("%.1f%%", p.cpu);
                        ImGui::TableNextColumn();
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, Col(255, 190, 40, (int)std::min(160.0f, p.mem * 0.5f)));
                        ImGui::Text("%.1f MB", p.mem);
                    }
                    ImGui::EndTable();
                }
                bool canEnd = sel >= 0 && sel < (int)procs.size() && !procs[sel].ended;
                if (!canEnd) ImGui::BeginDisabled();
                if (ImGui::Button("End task")) { procs[sel].ended = true; sel = -1; }
                if (!canEnd) ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Restore all")) for (auto& p : procs) p.ended = false;
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Performance")) {
                char ov[32];
                std::snprintf(ov, sizeof ov, "CPU  %.0f%%", totCpu);
                ImGui::PlotLines("##cpu", cpuHist, kHist, 0, ov, 0.0f, 100.0f, ImVec2(-1, 110));
                std::snprintf(ov, sizeof ov, "Memory  %.0f%%", memPct);
                ImGui::PlotLines("##mem", memHist, kHist, 0, ov, 0.0f, 100.0f, ImVec2(-1, 110));
                ImGui::Separator();
                ImGui::Text("Processes: %d", (int)std::count_if(procs.begin(), procs.end(), [](const Proc& p) { return !p.ended; }));
                ImGui::Text("Memory in use: %.0f MB / 640 MB", totMem);
                ImGui::Text("Uptime: %d s", (int)ImGui::GetTime());
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

// ----------------------------------------------------------------------------
// Shutdown screen
// ----------------------------------------------------------------------------
static void DrawShutdown(GLFWwindow* win, double t) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, Col(0, 0, 0));
    const char* msg = "Shutting down CSOPESY OS...";
    ImVec2 sz = ImGui::CalcTextSize(msg);
    dl->AddText(ImVec2((io.DisplaySize.x - sz.x) * 0.5f, io.DisplaySize.y * 0.45f), Col(230, 230, 230), msg);
    if (t > 1.2) glfwSetWindowShouldClose(win, GLFW_TRUE);  // the only intended way to exit
}

// ----------------------------------------------------------------------------
// Style
// ----------------------------------------------------------------------------
static void ApplyStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 8.0f;
    s.FrameRounding = 5.0f;
    s.TabRounding = 5.0f;
    s.ChildRounding = 5.0f;
    s.GrabRounding = 5.0f;
    s.PopupRounding = 6.0f;
    s.WindowBorderSize = 1.0f;
    s.FramePadding = ImVec2(8, 5);
    s.ItemSpacing = ImVec2(8, 7);
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.10f, 0.15f, 0.97f);
    c[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.09f, 0.16f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.28f, 0.52f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.18f, 0.40f, 0.70f, 0.55f);
    c[ImGuiCol_Button] = ImVec4(0.16f, 0.36f, 0.64f, 0.8f);
}

// ----------------------------------------------------------------------------
// config.txt  (the only file that may be edited during the quiz)
//   key=value per line, '#' starts a comment. All keys are optional.
//   skip_boot=0|1   wallpaper=0|1|2   clock_24h=0|1   brightness=0.2..1.0
//   window_width=1280   window_height=720   fullscreen=0|1
// ----------------------------------------------------------------------------
struct Config {
    bool skipBoot = false;
    bool fullscreen = false;
    int width = 1280, height = 720;
};

static Config LoadConfig() {
    Config cfg;
    const char* paths[] = {"config.txt", "../config.txt", "../../config.txt", "../../../config.txt"};
    std::ifstream f;
    for (const char* p : paths) {
        f.open(p);
        if (f.is_open()) { std::printf("[config] loaded %s\n", p); break; }
    }
    if (!f.is_open()) { std::printf("[config] config.txt not found, using defaults\n"); return cfg; }

    std::string line;
    while (std::getline(f, line)) {
        size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto trim = [](std::string s) {
            size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
            return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
        };
        std::string key = trim(line.substr(0, eq)), val = trim(line.substr(eq + 1));
        if (val.empty()) continue;
        if (key == "skip_boot") cfg.skipBoot = std::atoi(val.c_str()) != 0;
        else if (key == "fullscreen") cfg.fullscreen = std::atoi(val.c_str()) != 0;
        else if (key == "window_width") cfg.width = std::max(640, std::atoi(val.c_str()));
        else if (key == "window_height") cfg.height = std::max(360, std::atoi(val.c_str()));
        else if (key == "wallpaper") g.wallpaper = std::min(2, std::max(0, std::atoi(val.c_str())));
        else if (key == "clock_24h") g.clock24h = std::atoi(val.c_str()) != 0;
        else if (key == "brightness") g.brightness = std::min(1.0f, std::max(0.2f, (float)std::atof(val.c_str())));
    }
    return cfg;
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main() {
    std::srand((unsigned)std::time(nullptr));
    Config cfg = LoadConfig();
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWmonitor* monitor = cfg.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    GLFWwindow* window = glfwCreateWindow(cfg.width, cfg.height, "CSOPESY Desktop OS Emulator", monitor, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;       // do not persist window layout between runs
    io.FontGlobalScale = 1.25f;
    ApplyStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    g.stageStart = glfwGetTime();
    if (cfg.skipBoot) g.stage = Stage::Desktop;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        double t = ImGui::GetTime() - g.stageStart;
        switch (g.stage) {
        case Stage::Bios:     DrawBios(t); break;
        case Stage::Splash:   DrawSplash(t); break;
        case Stage::Shutdown: DrawShutdown(window, t); break;
        case Stage::Desktop:
            DrawWallpaper(ImGui::GetTime());   // 1. wallpaper (first thing each frame)
            DrawClock();                       // 2. real-time clock
            DrawTaskbar();                     // 3. taskbar
            DrawFileExplorer();                // 4. application windows
            DrawSettings();
            DrawTerminal();
            DrawTaskManager();
            break;
        }

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
