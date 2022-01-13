#include <string_view>
#include <fstream>
#include <sstream>
#include <filesystem>

#include <Windows.h>

#include <Ultralight/Ultralight.h>
#include <AppCore/AppCore.h>
#include <JavaScriptCore/JavaScript.h>

#include "resource.h"

using namespace ultralight;

class DedicatedDesmos : public AppListener,
    public WindowListener,
    public LoadListener,
    public ViewListener {

    static constexpr size_t initial_window_width = 700;
    static constexpr size_t initial_window_height = 400;

    RefPtr<App> m_app;
    RefPtr<Window> m_window;
    RefPtr<Overlay> m_overlay;

public:
    explicit DedicatedDesmos(char* self) {
        m_app = App::Create();
        m_window = Window::Create(
            m_app->main_monitor(),
            initial_window_width,
            initial_window_height,
            false,
            kWindowFlags_Titled | kWindowFlags_Resizable | kWindowFlags_Maximizable
        );

        m_app->set_window(*m_window);

        SetWindowIcon();

        m_overlay = Overlay::Create(
            *m_window,
            initial_window_width,
            initial_window_height,
            0,
            0
        );

        OnResize(m_window->width(), m_window->height());

        auto dir = std::filesystem::weakly_canonical(std::filesystem::path(self)).parent_path();
        std::wstring standalone_path = L"file:///" + std::wstring(dir.c_str()) + L"/desmos/index.html";

        for (wchar_t& c : standalone_path) {
            if (c == L'\\') {
                c = L'/';
            }
        }

        m_overlay->view()->LoadURL(String(standalone_path.data(), standalone_path.size()));

        m_app->set_listener(this);
        m_window->set_listener(this);
        m_overlay->view()->set_load_listener(this);
        m_overlay->view()->set_view_listener(this);
    }

    void Run() {
        m_app->Run();
    }

    void OnUpdate() override {

    }

    void OnChangeTitle(View* caller, const String& title) override {
        m_window->SetTitle((std::string("Dedicated Desmos by Max Goddard - ") + title.utf8().data()).c_str());
    }

    void OnResize(uint32_t width, uint32_t height) override {
        m_overlay->Resize(width, height);
    }

    void OnClose() override {
        m_app->Quit();
    }

    void OnChangeCursor(View* caller, Cursor cursor) override {
        m_window->SetCursor(cursor);
    }

    JSValue JS_GetStoredState(const JSObject& thisObject, const JSArgs& args) {
        if (!args.empty()) {
            if (args[0].IsString()) {
                String8 filename(String(args[0].ToString()).utf8());

                std::ifstream file("saves/" + std::string(filename.data(), filename.size()) + ".json");

                if (file.is_open()) {
                    std::stringstream sstr;
                    sstr << file.rdbuf();
                    std::string data(sstr.str());

                    return { JSString(String(data.data(), data.size())) };
                }
            }
        }

        return JSValueNullTag();
    }

    JSValue JS_GetStoredStateList(const JSObject& thisObject, const JSArgs& args) {
        JSArray stored_states;

        std::filesystem::path dir("saves");

        if (!exists(dir)) {
            return { stored_states };
        }

        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            const auto& path = entry.path();

            if (path.has_extension() && path.extension() == ".json") {
                std::string filename = path.filename().generic_string();
                filename.resize(filename.length() - 5);
                stored_states.push(JSString(filename.c_str()));
            }
        }

        return { stored_states };
    }

    JSValue JS_DeleteStoredState(const JSObject& thisObject, const JSArgs& args) {
        if (!args.empty()) {
            if (args[0].IsString()) {
                String8 filename(String(args[0].ToString()).utf8());

                std::filesystem::remove("saves/" + std::string(filename.data(), filename.size()) + ".json");
            }
        }

        return JSValueUndefinedTag();
    }

    JSValue JS_StoreState(const JSObject& thisObject, const JSArgs& args) {
        if (args.size() >= 2) {
            if (args[0].IsString() && args[1].IsString()) {
                String8 filename(String(args[0].ToString()).utf8());
                String8 data(String(args[1].ToString()).utf8());

                std::filesystem::create_directories("saves");

                std::ofstream file("saves/" + std::string(filename.data(), filename.size()) + ".json");
                file << std::string_view(data.data(), data.size());
                file.close();
            }
        }

        return JSValueUndefinedTag();
    }

    void OnDOMReady(View* caller, uint64_t frame_id, bool is_main_frame, const String& url) override {
        Ref<JSContext> context = caller->LockJSContext();
        SetJSContext(context.get());

        JSObject global = JSGlobalObject();

        global["DD_GET_STORED_STATE"] = BindJSCallbackWithRetval(&DedicatedDesmos::JS_GetStoredState);
        global["DD_GET_STORED_STATE_LIST"] = BindJSCallbackWithRetval(&DedicatedDesmos::JS_GetStoredStateList);
        global["DD_DELETE_STORED_STATE"] = BindJSCallbackWithRetval(&DedicatedDesmos::JS_DeleteStoredState);
        global["DD_STORE_STATE"] = BindJSCallbackWithRetval(&DedicatedDesmos::JS_StoreState);
    }

    void SetWindowIcon() {
        HBITMAP bitmap = (HBITMAP)LoadImageW(nullptr, L"desmos_round.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
        HBITMAP mono_bitmap = CreateBitmap(192, 192, 1, 1, nullptr);

        ICONINFO icon_info;
        icon_info.fIcon = TRUE;
        icon_info.hbmMask = mono_bitmap;
        icon_info.hbmColor = bitmap;

        HICON window_icon = CreateIconIndirect(&icon_info);

        HWND window_hande = reinterpret_cast<HWND>(m_window->native_handle());

        SendMessage(window_hande, WM_SETICON, ICON_SMALL, (LPARAM)window_icon);
        SendMessage(window_hande, WM_SETICON, ICON_BIG, (LPARAM)window_icon);

        DeleteObject(bitmap);
        DeleteObject(mono_bitmap);
    }
};

int main(int argc, char** argv) {
    HWND console;
    AllocConsole();
    console = FindWindowA("ConsoleWindowClass", NULL);
    ShowWindow(console, SW_HIDE);

    DedicatedDesmos dd(argv[0]);
    dd.Run();

    return 0;
}
