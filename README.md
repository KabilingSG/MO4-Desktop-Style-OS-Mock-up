CSOPESY Semi-Major Output 2 - Desktop-Style OS Mock-up (MO4)
Name   : Simon Gabriel M. Kabiling
Section: S01

ENTRY FILE : src/main.cpp   (the main() function is at the bottom of this file)
STACK      : C++17, GLFW 3.4 + OpenGL 3 + Dear ImGui v1.91.6
             (GLFW and ImGui are downloaded automatically by CMake, internet needed on first build)

---------------------------------------------------------------
HOW TO BUILD AND RUN
---------------------------------------------------------------
Requirements: CMake 3.16+, Ninja (or Make), a C++17 compiler, git, OpenGL.

Windows (MSYS2 UCRT64 terminal):
  pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja git
  cd <project folder>
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
  cmake --build build
  ./build/csopesy_os.exe        <-- run from the project folder so config.txt is found

Linux:
  sudo apt install build-essential cmake git libgl1-mesa-dev xorg-dev
  cmake -S . -B build && cmake --build build && ./build/csopesy_os

VS Code: open the project folder, press F5 (Run/Debug). .vscode/tasks.json builds with CMake and
.vscode/launch.json starts build/csopesy_os.exe from the project folder.

---------------------------------------------------------------
config.txt  (optional, no recompiling needed)
---------------------------------------------------------------
Located in the project folder. Keys: skip_boot, wallpaper (0/1/2), clock_24h, brightness,
window_width, window_height, fullscreen. If the file is missing, defaults are used.

---------------------------------------------------------------
USING THE OS
---------------------------------------------------------------
1. BIOS POST screen, then the CSOPESY splash/loading screen (automatic, about 9 seconds).
2. Desktop: wallpaper (base layer), real-time clock (top-right corner), taskbar (bottom).
3. Taskbar buttons, in order:
     1st  File Explorer   (unique UI screen #1, placeholder files)
     2nd  Settings        (unique UI screen #2, placeholder settings, wallpaper selector)
     3rd  Task Manager    (Processes table with dummy CPU/Memory, Performance graphs)
     4th  Terminal        (extra)
   Click an icon to open it. Clicking an open window's icon brings it to the front; clicking it
   again while it is in front closes it. Windows can also be closed with their X button.
4. Tray (bottom-right): VOL, NET, and PWR.
5. PWR shuts the OS down and closes the application. Use it to exit (not a force exit).

---------------------------------------------------------------
REQUIREMENT MAP
---------------------------------------------------------------
Desktop      : DrawWallpaper(), DrawClock(), PWR button in DrawTaskbar()
Taskbar      : DrawTaskbar()
Task Manager : DrawTaskManager()
Windows      : DrawFileExplorer(), DrawSettings(), DrawTerminal()
Boot/Shutdown: DrawBios(), DrawSplash(), DrawShutdown()
