# MiniRemote

A lightweight LAN remote desktop tool. Control another Windows PC from any browser on the same network — no installation, no account, no configuration.

## Features

- **Screen streaming** at 5fps with adjustable quality (10–100) and resolution
- **Mouse control** — move, left/right/middle click, scroll wheel
- **Keyboard input** — full key mapping including modifiers and function keys
- **Smart bandwidth** — skips frames when the screen hasn't changed
- **Resolution scaling** — downscale to reduce network usage (640x480 to 4K)
- **Custom resolution** — enter any width/height manually
- **Fullscreen mode** — immersive control
- **Admin elevation** — auto-requests UAC to control elevated windows (Task Manager, etc.)

## Requirements

- Windows 10 or later (64-bit)
- CMake 3.14+
- A C++17 compiler (MinGW/Clang or MSVC)

Dependencies (downloaded automatically by CMake):
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) — HTTP server (header-only)
- [stb_image_write](https://github.com/nothings/stb) — JPEG encoding (header-only)

## Build

### Quick build (MinGW)

```bat
build.bat
```

Output: `build\MiniRemote.exe`

### Manual build

```bat
mkdir build && cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j %NUMBER_OF_PROCESSORS%
```

### MSVC (Visual Studio)

```bat
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

## Usage

1. Run `MiniRemote.exe` on the PC you want to control
2. A console window shows the address, e.g. `http://192.168.1.100:8080`
3. Open that URL in any browser on the same network
4. Move your mouse over the canvas and use the controls at the bottom

### Command-line options

```
MiniRemote.exe --port=9090
```

| Flag | Default | Description |
|------|---------|-------------|
| `--port=N` | 8080 | HTTP server port |

Press `Ctrl+C` in the console to stop.

## Architecture

```
miniControl/
├── src/
│   ├── main.cpp          # HTTP server, screen capture, input simulation
│   └── html_content.h    # (generated) embedded HTML
├── web/
│   └── index.html        # Browser UI
├── include/
│   ├── httplib.h         # cpp-httplib
│   └── stb_image_write.h # JPEG encoder
├── cmake/
│   └── embed_html.cmake  # Embeds HTML into C++ header
├── CMakeLists.txt
└── build.bat
```

- **Screen capture**: Win32 GDI (`BitBlt` + `GetDIBits`)
- **JPEG encoding**: stb_image_write
- **Input simulation**: Win32 `SendInput` API
- **HTTP server**: cpp-httplib (header-only, no Boost/ASIO required)
- **Communication**: HTTP polling at ~5fps (screenshot) + POST (input)

## Notes

- The program auto-requests admin elevation on launch. This is required to send input to elevated windows like Task Manager. If you decline the UAC prompt, the program continues but cannot control admin-level windows.
- DPI scaling is handled automatically — `SetProcessDPIAware()` ensures coordinates are correct at any Windows scaling level.
- All communication is plain HTTP. This tool is intended for trusted LAN environments only.

## License

MIT
