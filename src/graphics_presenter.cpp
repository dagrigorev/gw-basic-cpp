#include "gwbasic/graphics_presenter.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <cwctype>
#include <mutex>
#include <optional>
#include <queue>
#include <chrono>
#include <cctype>
#include <thread>
#include <string>
#include <vector>

#if defined(__linux__)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif

namespace gwbasic::graphics {
namespace {
[[nodiscard]] auto indexed_color_to_rgb(std::uint8_t index) -> std::array<std::uint8_t, 3> {
    static constexpr std::array<std::array<std::uint8_t, 3>, 16> ega = {{
        {{0x00,0x00,0x00}}, {{0x00,0x00,0xAA}}, {{0x00,0xAA,0x00}}, {{0x00,0xAA,0xAA}},
        {{0xAA,0x00,0x00}}, {{0xAA,0x00,0xAA}}, {{0xAA,0x55,0x00}}, {{0xAA,0xAA,0xAA}},
        {{0x55,0x55,0x55}}, {{0x55,0x55,0xFF}}, {{0x55,0xFF,0x55}}, {{0x55,0xFF,0xFF}},
        {{0xFF,0x55,0x55}}, {{0xFF,0x55,0xFF}}, {{0xFF,0xFF,0x55}}, {{0xFF,0xFF,0xFF}}
    }};
    if (index < ega.size()) return ega[index];
    return {static_cast<std::uint8_t>(((index >> 5) & 7) * 255 / 7), static_cast<std::uint8_t>(((index >> 2) & 7) * 255 / 7), static_cast<std::uint8_t>((index & 3) * 255 / 3)};
}

class NullPresenter final : public Presenter {
public:
    void present(const std::vector<std::uint8_t>&, int, int, const std::array<std::uint8_t, 256>&) override {}
    void wait_until_closed() override {}
    [[nodiscard]] bool is_open() const override { return false; }
};

#if defined(__linux__)
using GLXContext = void*; using GLXDrawable = unsigned long; using GLenum = unsigned int; using GLbitfield = unsigned int; using GLint = int; using GLsizei = int; using GLfloat = float; using GLdouble = double;
constexpr int GLX_RGBA = 4, GLX_DOUBLEBUFFER = 5, GLX_DEPTH_SIZE = 12; constexpr GLenum GL_COLOR_BUFFER_BIT=0x00004000, GL_RGB=0x1907, GL_UNSIGNED_BYTE=0x1401, GL_PROJECTION=0x1701, GL_MODELVIEW=0x1700;
using FnChooseVisual=XVisualInfo* (*)(Display*, int, int*); using FnCreateContext=GLXContext (*)(Display*, XVisualInfo*, GLXContext, int); using FnMakeCurrent=int (*)(Display*, GLXDrawable, GLXContext); using FnSwapBuffers=void (*)(Display*, GLXDrawable); using FnDestroyContext=void (*)(Display*, GLXContext); using FnClearColor=void (*)(GLfloat,GLfloat,GLfloat,GLfloat); using FnClear=void (*)(GLbitfield); using FnViewport=void (*)(GLint,GLint,GLsizei,GLsizei); using FnMatrixMode=void (*)(GLenum); using FnLoadIdentity=void (*)(); using FnOrtho=void (*)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble); using FnRasterPos2i=void (*)(GLint,GLint); using FnPixelZoom=void (*)(GLfloat,GLfloat); using FnDrawPixels=void (*)(GLsizei,GLsizei,GLenum,GLenum,const void*); using FnFlush=void (*)();
class LinuxOpenGLPresenter final : public Presenter {
public:
    LinuxOpenGLPresenter() {
        if (std::getenv("DISPLAY") == nullptr) return;
        display_ = XOpenDisplay(nullptr); if (!display_) return;
        gl_lib_ = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL); if (!gl_lib_) { cleanup(); return; }
        glXChooseVisual_=load<FnChooseVisual>("glXChooseVisual"); glXCreateContext_=load<FnCreateContext>("glXCreateContext"); glXMakeCurrent_=load<FnMakeCurrent>("glXMakeCurrent"); glXSwapBuffers_=load<FnSwapBuffers>("glXSwapBuffers"); glXDestroyContext_=load<FnDestroyContext>("glXDestroyContext"); glClearColor_=load<FnClearColor>("glClearColor"); glClear_=load<FnClear>("glClear"); glViewport_=load<FnViewport>("glViewport"); glMatrixMode_=load<FnMatrixMode>("glMatrixMode"); glLoadIdentity_=load<FnLoadIdentity>("glLoadIdentity"); glOrtho_=load<FnOrtho>("glOrtho"); glRasterPos2i_=load<FnRasterPos2i>("glRasterPos2i"); glPixelZoom_=load<FnPixelZoom>("glPixelZoom"); glDrawPixels_=load<FnDrawPixels>("glDrawPixels"); glFlush_=load<FnFlush>("glFlush");
        if (!glXChooseVisual_||!glXCreateContext_||!glXMakeCurrent_||!glXSwapBuffers_||!glXDestroyContext_||!glClearColor_||!glClear_||!glViewport_||!glMatrixMode_||!glLoadIdentity_||!glOrtho_||!glRasterPos2i_||!glPixelZoom_||!glDrawPixels_||!glFlush_) { cleanup(); return; }
        int attrs[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24, None};
        XVisualInfo* visual = glXChooseVisual_(display_, DefaultScreen(display_), attrs); if (!visual) { cleanup(); return; }
        Colormap cmap = XCreateColormap(display_, RootWindow(display_, DefaultScreen(display_)), visual->visual, AllocNone);
        XSetWindowAttributes swa{}; swa.colormap=cmap; swa.event_mask=ExposureMask|StructureNotifyMask|KeyPressMask;
        window_ = XCreateWindow(display_, RootWindow(display_, DefaultScreen(display_)), 0,0,640,400,0, visual->depth, InputOutput, visual->visual, CWColormap|CWEventMask, &swa);
        wm_delete_ = XInternAtom(display_, "WM_DELETE_WINDOW", False); XSetWMProtocols(display_, window_, &wm_delete_, 1); XStoreName(display_, window_, "GW-BASIC Graphics"); XMapWindow(display_, window_);
        context_ = glXCreateContext_(display_, visual, nullptr, True); XFree(visual); if (!context_) { cleanup(); return; }
        glXMakeCurrent_(display_, window_, context_); available_ = true;
    }
    ~LinuxOpenGLPresenter() override { cleanup(); }
    void present(const std::vector<std::uint8_t>& pixels, int width, int height, const std::array<std::uint8_t, 256>& palette_map) override {
        if (!available_ || width <= 0 || height <= 0) return;
        process_events();
        if (!available_) return;
        if (width != canvas_width_ || height != canvas_height_) { canvas_width_=width; canvas_height_=height; rgb_.resize(static_cast<std::size_t>(width*height*3)); XResizeWindow(display_, window_, width*2u, height*2u); }
        for (int y=0; y<height; ++y) for (int x=0; x<width; ++x) { auto rgb=indexed_color_to_rgb(palette_map[pixels[static_cast<std::size_t>(y*width+x)]]); auto dst=static_cast<std::size_t>((height-1-y)*width*3 + x*3); rgb_[dst]=rgb[0]; rgb_[dst+1]=rgb[1]; rgb_[dst+2]=rgb[2]; }
        has_frame_ = true;
        const auto now = std::chrono::steady_clock::now();
        if (now - last_present_ < frame_interval_) return;
        render_frame();
        last_present_ = now;
    }
    void pump_events() override {
        process_events();
        if (available_ && has_frame_) {
            const auto now = std::chrono::steady_clock::now();
            if (now - last_present_ >= frame_interval_) {
                render_frame();
                last_present_ = now;
            }
        }
    }
    [[nodiscard]] auto poll_key() -> std::optional<std::string> override {
        process_events();
        std::lock_guard<std::mutex> lock(key_mutex_);
        if (key_queue_.empty()) return std::nullopt;
        auto value = key_queue_.front();
        key_queue_.pop();
        return value;
    }
    void wait_until_closed() override {
        while (available_) {
            process_events(true);
            if (available_ && has_frame_) render_frame();
        }
    }
    [[nodiscard]] bool is_open() const override { return available_; }
private:
    template<class T> auto load(const char* n) -> T { return reinterpret_cast<T>(dlsym(gl_lib_, n)); }
    void process_events(bool blocking=false) {
        if (!display_) return;
        if (blocking) {
            XEvent e{};
            XNextEvent(display_, &e);
            handle_event(e);
        }
        while (display_ && XPending(display_)>0) { XEvent e{}; XNextEvent(display_, &e); handle_event(e); if (!available_) return; }
    }
    void handle_event(const XEvent& e) {
        if (e.type==ClientMessage && static_cast<Atom>(e.xclient.data.l[0])==wm_delete_) { available_=false; return; }
        if (e.type==DestroyNotify) { available_=false; return; }
        if (e.type==KeyPress) {
            char buffer[16]{};
            KeySym keysym{};
            const int count = XLookupString(const_cast<XKeyEvent*>(&e.xkey), buffer, static_cast<int>(sizeof(buffer)), &keysym, nullptr);
            if (count > 0) {
                std::string value(buffer, buffer + count);
                if (value.size() == 1) {
                    unsigned char ch = static_cast<unsigned char>(value[0]);
                    if (ch >= 'a' && ch <= 'z') value[0] = static_cast<char>(std::toupper(ch));
                }
                std::lock_guard<std::mutex> lock(key_mutex_);
                key_queue_.push(std::move(value));
            }
        }
    }
    void render_frame() {
        if (!available_ || !has_frame_ || canvas_width_ <= 0 || canvas_height_ <= 0) return;
        XWindowAttributes a{}; XGetWindowAttributes(display_, window_, &a); int ww=std::max(1,a.width), wh=std::max(1,a.height);
        glViewport_(0,0,ww,wh); glMatrixMode_(GL_PROJECTION); glLoadIdentity_(); glOrtho_(0.0, static_cast<double>(canvas_width_), 0.0, static_cast<double>(canvas_height_), -1.0, 1.0); glMatrixMode_(GL_MODELVIEW); glLoadIdentity_(); glClearColor_(0,0,0,1); glClear_(GL_COLOR_BUFFER_BIT); glRasterPos2i_(0,canvas_height_); glPixelZoom_(static_cast<GLfloat>(ww)/canvas_width_, -static_cast<GLfloat>(wh)/canvas_height_); glDrawPixels_(canvas_width_,canvas_height_,GL_RGB,GL_UNSIGNED_BYTE,rgb_.data()); glFlush_(); glXSwapBuffers_(display_, window_);
    }
    void cleanup() { if (display_ && context_ && glXMakeCurrent_ && glXDestroyContext_) { glXMakeCurrent_(display_,0,nullptr); glXDestroyContext_(display_,context_); } if (display_ && window_) XDestroyWindow(display_,window_); if (display_) XCloseDisplay(display_); if (gl_lib_) dlclose(gl_lib_); display_=nullptr; window_=0; context_=nullptr; gl_lib_=nullptr; available_=false; }
    Display* display_{nullptr}; Window window_{}; Atom wm_delete_{}; GLXContext context_{nullptr}; void* gl_lib_{nullptr}; bool available_{false}; int canvas_width_{0}, canvas_height_{0}; std::vector<std::uint8_t> rgb_; bool has_frame_{false}; std::queue<std::string> key_queue_; std::mutex key_mutex_; std::chrono::steady_clock::time_point last_present_{}; const std::chrono::milliseconds frame_interval_{16};
    FnChooseVisual glXChooseVisual_{nullptr}; FnCreateContext glXCreateContext_{nullptr}; FnMakeCurrent glXMakeCurrent_{nullptr}; FnSwapBuffers glXSwapBuffers_{nullptr}; FnDestroyContext glXDestroyContext_{nullptr}; FnClearColor glClearColor_{nullptr}; FnClear glClear_{nullptr}; FnViewport glViewport_{nullptr}; FnMatrixMode glMatrixMode_{nullptr}; FnLoadIdentity glLoadIdentity_{nullptr}; FnOrtho glOrtho_{nullptr}; FnRasterPos2i glRasterPos2i_{nullptr}; FnPixelZoom glPixelZoom_{nullptr}; FnDrawPixels glDrawPixels_{nullptr}; FnFlush glFlush_{nullptr};
};
#elif defined(_WIN32)
struct DxVertex {
    float position[3];
    float uv[2];
};

class WindowsDirectXPresenter final : public Presenter {
public:
    WindowsDirectXPresenter() { initialize(); }
    ~WindowsDirectXPresenter() override { cleanup(); }

    void present(const std::vector<std::uint8_t>& pixels, int width, int height, const std::array<std::uint8_t, 256>& palette_map) override {
        if (!available_ || width <= 0 || height <= 0) return;
        process_events();
        if (!available_) return;
        if (!ensure_canvas_resources(width, height)) return;
        update_texture(pixels, width, height, palette_map);
        has_frame_ = true;
        const auto now = std::chrono::steady_clock::now();
        if (now - last_present_ >= frame_interval_) {
            render();
            last_present_ = now;
        }
        InvalidateRect(window_, nullptr, FALSE);
    }
    [[nodiscard]] auto poll_key() -> std::optional<std::string> override {
        process_events();
        std::lock_guard<std::mutex> lock(key_mutex_);
        if (key_queue_.empty()) return std::nullopt;
        auto value = key_queue_.front();
        key_queue_.pop();
        return value;
    }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto* self = reinterpret_cast<WindowsDirectXPresenter*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self != nullptr) {
            switch (message) {
            case WM_CLOSE:
                self->available_ = false;
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                self->available_ = false;
                PostQuitMessage(0);
                return 0;
            case WM_SIZE:
                self->on_resize(LOWORD(lparam), HIWORD(lparam));
                return 0;
            case WM_KEYDOWN:
                if (wparam == VK_ESCAPE) {
                    std::lock_guard<std::mutex> lock(self->key_mutex_);
                    self->key_queue_.push(std::string(1, static_cast<char>(27)));
                    return 0;
                }
                break;
            case WM_CHAR: {
                wchar_t wch = static_cast<wchar_t>(wparam);
                if (wch >= L'a' && wch <= L'z') wch = static_cast<wchar_t>(wch - L'a' + L'A');
                if (wch > 0 && wch < 128) {
                    std::lock_guard<std::mutex> lock(self->key_mutex_);
                    self->key_queue_.push(std::string(1, static_cast<char>(wch)));
                    return 0;
                }
                break;
            }
            case WM_PAINT: {
                PAINTSTRUCT ps{};
                BeginPaint(hwnd, &ps);
                if (self->has_frame_) {
                    self->render();
                }
                EndPaint(hwnd, &ps);
                return 0;
            }
            default:
                break;
            }
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    void initialize() {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        constexpr wchar_t kClassName[] = L"GWBasicDirectXPresenterWindow";
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &WindowsDirectXPresenter::window_proc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.lpszClassName = kClassName;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassExW(&wc);

        RECT rect{0, 0, 960, 600};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        window_ = CreateWindowExW(0, kClassName, L"GW-BASIC Graphics", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  rect.right - rect.left, rect.bottom - rect.top,
                                  nullptr, nullptr, instance, this);
        if (window_ == nullptr) return;

        DXGI_SWAP_CHAIN_DESC swap_desc{};
        swap_desc.BufferCount = 2;
        swap_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_desc.OutputWindow = window_;
        swap_desc.SampleDesc.Count = 1;
        swap_desc.Windowed = TRUE;
        swap_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT flags = 0;
#ifndef NDEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL obtained{};
        static constexpr D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };
        const HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            levels,
            static_cast<UINT>(std::size(levels)),
            D3D11_SDK_VERSION,
            &swap_desc,
            &swap_chain_,
            &device_,
            &obtained,
            &context_);
        if (FAILED(hr) || device_ == nullptr || context_ == nullptr || swap_chain_ == nullptr) {
            cleanup();
            return;
        }

        if (!create_render_target()) {
            cleanup();
            return;
        }
        if (!create_pipeline()) {
            cleanup();
            return;
        }

        ShowWindow(window_, SW_SHOW);
        UpdateWindow(window_);
        available_ = true;
    }

    [[nodiscard]] auto create_render_target() -> bool {
        ID3D11Texture2D* back_buffer = nullptr;
        const HRESULT hr = swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
        if (FAILED(hr) || back_buffer == nullptr) return false;
        const HRESULT rtv_hr = device_->CreateRenderTargetView(back_buffer, nullptr, &render_target_view_);
        back_buffer->Release();
        return SUCCEEDED(rtv_hr) && render_target_view_ != nullptr;
    }

    [[nodiscard]] auto create_pipeline() -> bool {
        static constexpr char vs_source[] = R"(
            struct VSInput {
                float3 pos : POSITION;
                float2 uv  : TEXCOORD0;
            };
            struct PSInput {
                float4 pos : SV_POSITION;
                float2 uv  : TEXCOORD0;
            };
            PSInput main(VSInput input) {
                PSInput output;
                output.pos = float4(input.pos, 1.0f);
                output.uv = input.uv;
                return output;
            }
        )";
        static constexpr char ps_source[] = R"(
            Texture2D frameTex : register(t0);
            SamplerState frameSampler : register(s0);
            float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
                return frameTex.Sample(frameSampler, uv);
            }
        )";

        ID3DBlob* vs_blob = nullptr;
        ID3DBlob* ps_blob = nullptr;
        ID3DBlob* error_blob = nullptr;
        HRESULT hr = D3DCompile(vs_source, sizeof(vs_source) - 1, nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vs_blob, &error_blob);
        if (FAILED(hr)) { if (error_blob) error_blob->Release(); return false; }
        if (error_blob) { error_blob->Release(); error_blob = nullptr; }
        hr = D3DCompile(ps_source, sizeof(ps_source) - 1, nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &ps_blob, &error_blob);
        if (FAILED(hr)) { if (vs_blob) vs_blob->Release(); if (error_blob) error_blob->Release(); return false; }
        if (error_blob) { error_blob->Release(); error_blob = nullptr; }

        hr = device_->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vertex_shader_);
        if (FAILED(hr)) { vs_blob->Release(); ps_blob->Release(); return false; }
        hr = device_->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &pixel_shader_);
        if (FAILED(hr)) { vs_blob->Release(); ps_blob->Release(); return false; }

        const D3D11_INPUT_ELEMENT_DESC layout_desc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(DxVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(DxVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        hr = device_->CreateInputLayout(layout_desc, static_cast<UINT>(std::size(layout_desc)), vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &input_layout_);
        vs_blob->Release();
        ps_blob->Release();
        if (FAILED(hr)) return false;

        const DxVertex vertices[] = {
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}},
            {{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}},
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            {{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}},
            {{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        };
        D3D11_BUFFER_DESC vb_desc{};
        vb_desc.ByteWidth = sizeof(vertices);
        vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
        vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vb_data{};
        vb_data.pSysMem = vertices;
        hr = device_->CreateBuffer(&vb_desc, &vb_data, &vertex_buffer_);
        if (FAILED(hr)) return false;

        D3D11_SAMPLER_DESC sampler_desc{};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device_->CreateSamplerState(&sampler_desc, &sampler_state_);
        if (FAILED(hr) || sampler_state_ == nullptr) return false;

        D3D11_RASTERIZER_DESC raster_desc{};
        raster_desc.FillMode = D3D11_FILL_SOLID;
        raster_desc.CullMode = D3D11_CULL_NONE;
        raster_desc.FrontCounterClockwise = FALSE;
        raster_desc.DepthClipEnable = TRUE;
        hr = device_->CreateRasterizerState(&raster_desc, &rasterizer_state_);
        return SUCCEEDED(hr) && rasterizer_state_ != nullptr;
    }

    [[nodiscard]] auto ensure_canvas_resources(int width, int height) -> bool {
        if (canvas_width_ == width && canvas_height_ == height && frame_texture_ != nullptr && shader_resource_view_ != nullptr) {
            return true;
        }
        if (shader_resource_view_ != nullptr) { shader_resource_view_->Release(); shader_resource_view_ = nullptr; }
        if (frame_texture_ != nullptr) { frame_texture_->Release(); frame_texture_ = nullptr; }
        frame_rgba_.assign(static_cast<std::size_t>(width * height * 4), 0);

        D3D11_TEXTURE2D_DESC texture_desc{};
        texture_desc.Width = static_cast<UINT>(width);
        texture_desc.Height = static_cast<UINT>(height);
        texture_desc.MipLevels = 1;
        texture_desc.ArraySize = 1;
        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Usage = D3D11_USAGE_DYNAMIC;
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device_->CreateTexture2D(&texture_desc, nullptr, &frame_texture_)) || frame_texture_ == nullptr) return false;
        if (FAILED(device_->CreateShaderResourceView(frame_texture_, nullptr, &shader_resource_view_)) || shader_resource_view_ == nullptr) return false;
        canvas_width_ = width;
        canvas_height_ = height;
        return true;
    }

    void update_texture(const std::vector<std::uint8_t>& pixels, int width, int height, const std::array<std::uint8_t, 256>& palette_map) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const auto rgb = indexed_color_to_rgb(palette_map[pixels[static_cast<std::size_t>(y * width + x)]]);
                const auto dst = static_cast<std::size_t>((y * width + x) * 4);
                frame_rgba_[dst + 0] = rgb[0];
                frame_rgba_[dst + 1] = rgb[1];
                frame_rgba_[dst + 2] = rgb[2];
                frame_rgba_[dst + 3] = 0xFF;
            }
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context_->Map(frame_texture_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
        for (int y = 0; y < height; ++y) {
            auto* dst = static_cast<std::uint8_t*>(mapped.pData) + mapped.RowPitch * static_cast<std::size_t>(y);
            const auto* src = frame_rgba_.data() + static_cast<std::size_t>(y * width * 4);
            std::copy(src, src + static_cast<std::size_t>(width * 4), dst);
        }
        context_->Unmap(frame_texture_, 0);
    }

    void render() {
        if (render_target_view_ == nullptr) return;
        RECT client{};
        GetClientRect(window_, &client);
        const auto client_width = static_cast<float>(std::max<LONG>(1, client.right - client.left));
        const auto client_height = static_cast<float>(std::max<LONG>(1, client.bottom - client.top));
        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = client_width;
        viewport.Height = client_height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &viewport);

        const float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        context_->OMSetRenderTargets(1, &render_target_view_, nullptr);
        context_->ClearRenderTargetView(render_target_view_, clear);
        context_->RSSetState(rasterizer_state_);

        UINT stride = sizeof(DxVertex);
        UINT offset = 0;
        context_->IASetInputLayout(input_layout_);
        context_->IASetVertexBuffers(0, 1, &vertex_buffer_, &stride, &offset);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vertex_shader_, nullptr, 0);
        context_->PSSetShader(pixel_shader_, nullptr, 0);
        context_->PSSetSamplers(0, 1, &sampler_state_);
        context_->PSSetShaderResources(0, 1, &shader_resource_view_);
        context_->Draw(6, 0);

        ID3D11ShaderResourceView* null_srv = nullptr;
        context_->PSSetShaderResources(0, 1, &null_srv);
        swap_chain_->Present(1, 0);
    }

    void process_events() {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                available_ = false;
                return;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void pump_events() override { process_events(); if (available_ && has_frame_) { const auto now = std::chrono::steady_clock::now(); if (now - last_present_ >= frame_interval_) { render(); last_present_ = now; } } }
    void wait_until_closed() override {
        if (!available_) return;
        MSG msg{};
        while (available_) {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) { available_ = false; break; }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (!available_) break;
            if (has_frame_) render();
            std::this_thread::sleep_for(frame_interval_);
        }
        available_ = false;
    }
    [[nodiscard]] bool is_open() const override { return available_; }

    void on_resize(int width, int height) {
        if (swap_chain_ == nullptr || width <= 0 || height <= 0) return;
        if (render_target_view_ != nullptr) {
            render_target_view_->Release();
            render_target_view_ = nullptr;
        }
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        if (FAILED(swap_chain_->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0))) {
            available_ = false;
            return;
        }
        if (!create_render_target()) {
            available_ = false;
            return;
        }
        if (has_frame_) {
            render();
        }
    }

    void cleanup() {
        if (rasterizer_state_ != nullptr) { rasterizer_state_->Release(); rasterizer_state_ = nullptr; }
        if (sampler_state_ != nullptr) { sampler_state_->Release(); sampler_state_ = nullptr; }
        if (vertex_buffer_ != nullptr) { vertex_buffer_->Release(); vertex_buffer_ = nullptr; }
        if (input_layout_ != nullptr) { input_layout_->Release(); input_layout_ = nullptr; }
        if (pixel_shader_ != nullptr) { pixel_shader_->Release(); pixel_shader_ = nullptr; }
        if (vertex_shader_ != nullptr) { vertex_shader_->Release(); vertex_shader_ = nullptr; }
        if (shader_resource_view_ != nullptr) { shader_resource_view_->Release(); shader_resource_view_ = nullptr; }
        if (frame_texture_ != nullptr) { frame_texture_->Release(); frame_texture_ = nullptr; }
        if (render_target_view_ != nullptr) { render_target_view_->Release(); render_target_view_ = nullptr; }
        if (swap_chain_ != nullptr) { swap_chain_->Release(); swap_chain_ = nullptr; }
        if (context_ != nullptr) { context_->Release(); context_ = nullptr; }
        if (device_ != nullptr) { device_->Release(); device_ = nullptr; }
        if (window_ != nullptr) { DestroyWindow(window_); window_ = nullptr; }
        available_ = false;
    }

    HWND window_{nullptr};
    bool available_{false};
    int canvas_width_{0};
    int canvas_height_{0};
    std::vector<std::uint8_t> frame_rgba_;

    IDXGISwapChain* swap_chain_{nullptr};
    ID3D11Device* device_{nullptr};
    ID3D11DeviceContext* context_{nullptr};
    ID3D11RenderTargetView* render_target_view_{nullptr};
    ID3D11Texture2D* frame_texture_{nullptr};
    ID3D11ShaderResourceView* shader_resource_view_{nullptr};
    ID3D11VertexShader* vertex_shader_{nullptr};
    ID3D11PixelShader* pixel_shader_{nullptr};
    ID3D11InputLayout* input_layout_{nullptr};
    ID3D11Buffer* vertex_buffer_{nullptr};
    ID3D11SamplerState* sampler_state_{nullptr};
    ID3D11RasterizerState* rasterizer_state_{nullptr};
    bool has_frame_{false};
    std::queue<std::string> key_queue_;
    std::mutex key_mutex_;
    std::chrono::steady_clock::time_point last_present_{};
    const std::chrono::milliseconds frame_interval_{16};
};
#endif
} // namespace

auto create_default_presenter() -> std::unique_ptr<Presenter> {
#if defined(__linux__)
    return std::make_unique<LinuxOpenGLPresenter>();
#elif defined(_WIN32)
    return std::make_unique<WindowsDirectXPresenter>();
#else
    return std::make_unique<NullPresenter>();
#endif
}
} // namespace gwbasic::graphics
