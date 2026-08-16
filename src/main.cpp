#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#undef CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include "httplib.h"
#include "html_content.h"
#include <windows.h>
#include <wingdi.h>
#include <winuser.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <cstring>

// stb_image_write for JPEG encoding
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ============ Global State ============
static std::atomic<int> g_quality{70};
static std::atomic<int> g_target_width{0};
static std::atomic<int> g_target_height{0};
static std::atomic<bool> g_force_refresh{false};
static std::atomic<bool> g_running{true};

static std::mutex g_frame_mutex;
static std::vector<uint8_t> g_cached_frame;
static std::mutex g_config_mutex;

// ============ Key Code Mapping ============
static const std::unordered_map<std::string, int> key_map = {
    {"KeyA", 'A'}, {"KeyB", 'B'}, {"KeyC", 'C'}, {"KeyD", 'D'},
    {"KeyE", 'E'}, {"KeyF", 'F'}, {"KeyG", 'G'}, {"KeyH", 'H'},
    {"KeyI", 'I'}, {"KeyJ", 'J'}, {"KeyK", 'K'}, {"KeyL", 'L'},
    {"KeyM", 'M'}, {"KeyN", 'N'}, {"KeyO", 'O'}, {"KeyP", 'P'},
    {"KeyQ", 'Q'}, {"KeyR", 'R'}, {"KeyS", 'S'}, {"KeyT", 'T'},
    {"KeyU", 'U'}, {"KeyV", 'V'}, {"KeyW", 'W'}, {"KeyX", 'X'},
    {"KeyY", 'Y'}, {"KeyZ", 'Z'},
    {"Digit0", '0'}, {"Digit1", '1'}, {"Digit2", '2'}, {"Digit3", '3'},
    {"Digit4", '4'}, {"Digit5", '5'}, {"Digit6", '6'}, {"Digit7", '7'},
    {"Digit8", '8'}, {"Digit9", '9'},
    {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
    {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
    {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},
    {"ArrowUp", VK_UP}, {"ArrowDown", VK_DOWN},
    {"ArrowLeft", VK_LEFT}, {"ArrowRight", VK_RIGHT},
    {"Space", VK_SPACE}, {"Enter", VK_RETURN}, {"Tab", VK_TAB},
    {"Escape", VK_ESCAPE}, {"Backspace", VK_BACK}, {"Delete", VK_DELETE},
    {"Insert", VK_INSERT}, {"Home", VK_HOME}, {"End", VK_END},
    {"PageUp", VK_PRIOR}, {"PageDown", VK_NEXT},
    {"ControlLeft", VK_CONTROL}, {"ControlRight", VK_CONTROL},
    {"ShiftLeft", VK_SHIFT}, {"ShiftRight", VK_SHIFT},
    {"AltLeft", VK_MENU}, {"AltRight", VK_MENU},
    {"MetaLeft", VK_LWIN}, {"MetaRight", VK_RWIN},
    {"CapsLock", VK_CAPITAL}, {"NumLock", VK_NUMLOCK}, {"ScrollLock", VK_SCROLL},
    {"PrintScreen", VK_SNAPSHOT}, {"Pause", VK_PAUSE},
    {"Context", VK_APPS},
    {"Semicolon", VK_OEM_1}, {"Equal", VK_OEM_PLUS},
    {"Comma", VK_OEM_COMMA}, {"Minus", VK_OEM_MINUS},
    {"Period", VK_OEM_PERIOD}, {"Slash", VK_OEM_2},
    {"Backquote", VK_OEM_3},
    {"BracketLeft", VK_OEM_4}, {"Backslash", VK_OEM_5},
    {"BracketRight", VK_OEM_6}, {"Quote", VK_OEM_7},
    {"Numpad0", VK_NUMPAD0}, {"Numpad1", VK_NUMPAD1}, {"Numpad2", VK_NUMPAD2},
    {"Numpad3", VK_NUMPAD3}, {"Numpad4", VK_NUMPAD4}, {"Numpad5", VK_NUMPAD5},
    {"Numpad6", VK_NUMPAD6}, {"Numpad7", VK_NUMPAD7}, {"Numpad8", VK_NUMPAD8},
    {"Numpad9", VK_NUMPAD9},
    {"NumpadAdd", VK_ADD}, {"NumpadSubtract", VK_SUBTRACT},
    {"NumpadMultiply", VK_MULTIPLY}, {"NumpadDivide", VK_DIVIDE},
    {"NumpadDecimal", VK_DECIMAL}, {"NumpadEnter", VK_RETURN},
};

// ============ Screen Capture ============
static void capture_and_cache() {
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    HDC hdc_screen = GetDC(nullptr);
    HDC hdc_mem = CreateCompatibleDC(hdc_screen);
    HBITMAP hbitmap = CreateCompatibleBitmap(hdc_screen, screen_w, screen_h);
    HGDIOBJ old_bmp = SelectObject(hdc_mem, hbitmap);

    BitBlt(hdc_mem, 0, 0, screen_w, screen_h, hdc_screen, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = screen_w;
    bi.biHeight = -screen_h;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    std::vector<uint8_t> pixels(screen_w * screen_h * 4);
    GetDIBits(hdc_mem, hbitmap, 0, screen_h, pixels.data(),
              reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

    SelectObject(hdc_mem, old_bmp);
    DeleteObject(hbitmap);
    DeleteDC(hdc_mem);
    ReleaseDC(nullptr, hdc_screen);

    int out_w = screen_w;
    int out_h = screen_h;
    int tw = g_target_width.load();
    int th = g_target_height.load();
    if (tw > 0 && th > 0) {
        out_w = tw;
        out_h = th;
    }

    std::vector<uint8_t> rgb(out_w * out_h * 3);
    for (int y = 0; y < out_h; y++) {
        int src_y = y * screen_h / out_h;
        for (int x = 0; x < out_w; x++) {
            int src_x = x * screen_w / out_w;
            int si = (src_y * screen_w + src_x) * 4;
            int di = (y * out_w + x) * 3;
            rgb[di + 0] = pixels[si + 2];
            rgb[di + 1] = pixels[si + 1];
            rgb[di + 2] = pixels[si + 0];
        }
    }

    int q = g_quality.load();
    std::vector<uint8_t> jpeg;
    stbi_write_jpg_to_func(
        [](void* ctx, void* data, int size) {
            auto* v = static_cast<std::vector<uint8_t>*>(ctx);
            v->insert(v->end(), static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
        },
        &jpeg, out_w, out_h, 3, rgb.data(), std::max(1, std::min(100, q))
    );

    if (!jpeg.empty()) {
        std::lock_guard<std::mutex> lock(g_frame_mutex);
        g_cached_frame = std::move(jpeg);
    }
}

// ============ Input Simulation ============
static void simulate_mouse_move(int x, int y) {
    SetCursorPos(x, y);
}

static void simulate_mouse_button(const std::string& button, bool pressed) {
    DWORD flags = 0;
    if (button == "left")
        flags = pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    else if (button == "right")
        flags = pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    else if (button == "middle")
        flags = pressed ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    if (flags) {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = flags;
        SendInput(1, &input, sizeof(INPUT));
    }
}

static void simulate_mouse_scroll(int dx, int dy) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(-dy);
    SendInput(1, &input, sizeof(INPUT));
}

static void simulate_key(const std::string& key, bool pressed) {
    auto it = key_map.find(key);
    int vk = 0;
    if (it != key_map.end()) {
        vk = it->second;
    } else if (key.length() == 1) {
        char c = key[0];
        if (c >= 'a' && c <= 'z') vk = VkKeyScan(c) & 0xFF;
        else if (c >= 'A' && c <= 'Z') vk = VkKeyScan(c + 32) & 0xFF;
        else if (c >= '0' && c <= '9') vk = c;
    }
    if (vk) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = static_cast<WORD>(vk);
        input.ki.dwFlags = pressed ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    }
}

// ============ Simple JSON Helpers ============
static std::string json_get(const std::string& j, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto p = j.find(search);
    if (p == std::string::npos) return "";
    p = j.find(':', p + search.size());
    if (p == std::string::npos) return "";
    p++;
    while (p < j.size() && j[p] == ' ') p++;
    if (p >= j.size()) return "";
    if (j[p] == '"') {
        p++;
        auto e = j.find('"', p);
        return (e != std::string::npos) ? j.substr(p, e - p) : "";
    }
    auto e = j.find_first_of(",}", p);
    return (e != std::string::npos) ? j.substr(p, e - p) : j.substr(p);
}

static int json_int(const std::string& j, const std::string& key, int def = 0) {
    auto v = json_get(j, key);
    if (v.empty()) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}

static bool json_bool(const std::string& j, const std::string& key, bool def = false) {
    auto v = json_get(j, key);
    return v.empty() ? def : (v == "true");
}

// ============ Main ============
// ============ Admin Elevation ============
static bool is_running_as_admin() {
    BOOL is_admin = FALSE;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    PSID admin_group = nullptr;
    if (AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin_group)) {
        CheckTokenMembership(nullptr, admin_group, &is_admin);
        FreeSid(admin_group);
    }
    return is_admin;
}

static void elevate_if_needed(int argc, char* argv[]) {
    if (is_running_as_admin()) return;
    // Re-launch with runas verb
    std::string exe_path;
    char buf[MAX_PATH]{};
    if (GetModuleFileNameA(nullptr, buf, MAX_PATH)) exe_path = buf;
    
    std::string params;
    for (int i = 1; i < argc; i++) {
        if (!params.empty()) params += " ";
        params += "\"";
        params += argv[i];
        params += "\"";
    }
    
    HINSTANCE result = ShellExecuteA(nullptr, "runas", exe_path.c_str(),
                                      params.empty() ? nullptr : params.c_str(),
                                      nullptr, SW_SHOW);
    // If elevation succeeded or was cancelled by user, exit current instance
    if (reinterpret_cast<intptr_t>(result) > 32) {
        ExitProcess(0);
    }
    // If elevation failed (e.g. user cancelled), continue without admin
    std::cout << "[WARNING] Running without admin - some windows (Task Manager, etc.) may not be controllable\n";
}

int main(int argc, char* argv[]) {
    elevate_if_needed(argc, argv);

    // DPI awareness - must be called before any GetSystemMetrics/SetCursorPos
    SetProcessDPIAware();

    uint16_t port = 8080;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.find("--port=") == 0) {
            try { port = static_cast<uint16_t>(std::stoi(arg.substr(7))); } catch (...) {}
        }
    }

    // Get local IP
    char hostname[256]{};
    gethostname(hostname, sizeof(hostname));
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo(hostname, nullptr, &hints, &res);
    std::string local_ip = "127.0.0.1";
    if (res) {
        char ip[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr, ip, sizeof(ip));
        local_ip = ip;
        freeaddrinfo(res);
    }

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    // Capture thread
    std::thread capture_thread([]() {
        using clock = std::chrono::steady_clock;
        const auto interval = std::chrono::milliseconds(200);
        while (g_running.load()) {
            auto t0 = clock::now();
            capture_and_cache();
            auto dt = clock::now() - t0;
            if (dt < interval) std::this_thread::sleep_for(interval - dt);
        }
    });

    httplib::Server svr;

    // Serve HTML
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(HTML_CONTENT, "text/html; charset=utf-8");
    });

    // Screenshot endpoint (polling)
    svr.Get("/api/screenshot", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_frame_mutex);
        if (g_cached_frame.empty()) {
            res.status = 204;
        } else {
            res.set_content(
                std::string(reinterpret_cast<const char*>(g_cached_frame.data()), g_cached_frame.size()),
                "image/jpeg"
            );
        }
    });

    // Mouse/keyboard input
    svr.Post("/api/input", [](const httplib::Request& req, httplib::Response& res) {
        std::string type = json_get(req.body, "type");

        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);

        if (type == "mouse_move") {
            int x = json_int(req.body, "x");
            int y = json_int(req.body, "y");
            int tw = g_target_width.load();
            int th = g_target_height.load();
            if (tw > 0 && th > 0) {
                x = x * screen_w / tw;
                y = y * screen_h / th;
            }
            simulate_mouse_move(x, y);
        } else if (type == "mouse_button") {
            simulate_mouse_button(json_get(req.body, "button"), json_bool(req.body, "pressed"));
        } else if (type == "mouse_scroll") {
            simulate_mouse_scroll(json_int(req.body, "dx"), json_int(req.body, "dy"));
        } else if (type == "key") {
            simulate_key(json_get(req.body, "key"), json_bool(req.body, "pressed"));
        }

        res.set_content("ok", "text/plain");
    });

    // Config
    svr.Get("/api/config", [](const httplib::Request&, httplib::Response& res) {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        std::string json = "{\"quality\":" + std::to_string(g_quality.load()) +
            ",\"screen_width\":" + std::to_string(sw) +
            ",\"screen_height\":" + std::to_string(sh) +
            ",\"target_width\":" + std::to_string(g_target_width.load()) +
            ",\"target_height\":" + std::to_string(g_target_height.load()) + "}";
        res.set_content(json, "application/json");
    });

    svr.Post("/api/config", [](const httplib::Request& req, httplib::Response& res) {
        int q = json_int(req.body, "quality", -1);
        int w = json_int(req.body, "width", 0);
        int h = json_int(req.body, "height", 0);
        if (q > 0) g_quality.store(q);
        if (w > 0 && h > 0) {
            g_target_width.store(w);
            g_target_height.store(h);
        }
        res.set_content("ok", "text/plain");
    });

    // Force refresh
    svr.Post("/api/refresh", [](const httplib::Request&, httplib::Response& res) {
        g_force_refresh.store(true);
        capture_and_cache();
        res.set_content("ok", "text/plain");
    });

    std::cout << "=========================================\n";
    std::cout << "  MiniRemote - LAN Remote Desktop\n";
    std::cout << "  http://" << local_ip << ":" << port << "\n";
    std::cout << "=========================================\n";
    std::cout << "Open the above URL in a browser to control\n";
    std::cout << "Press Ctrl+C to stop\n";

    svr.listen("0.0.0.0", port);

    g_running.store(false);
    capture_thread.join();
    return 0;
}
