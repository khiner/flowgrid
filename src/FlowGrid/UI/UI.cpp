#include "UI.h"

#include "imgui.h"
#include "implot.h"

#include "imgui_impl_opengl3.h" // TODO vulkan
#include "imgui_impl_sdl3.h"
#include "nlohmann/json.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "../ImGuiSettings.h"
#include "../Style.h"

#include "../FileDialog/FileDialog.h"
#include "../Helper/String.h"

#ifdef TRACING_ENABLED
#include <Tracy.hpp>
#endif

using namespace fg;
using namespace ImGui;

struct RenderContext {
    SDL_Window *window = nullptr;
    SDL_GLContext gl_context{};
    const char *glsl_version = "#version 150";
    ImGuiIO io;
};

RenderContext CreateRenderContext() {
#if defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char *glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Enable native IME.
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    auto window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED;

    RenderContext render_context;
    render_context.window = SDL_CreateWindowWithPosition("FlowGrid", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);

    render_context.gl_context = SDL_GL_CreateContext(render_context.window);

    return render_context;
}

void DestroyRenderContext(const RenderContext &rc) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    DestroyContext();
    ImPlot::DestroyContext();

    SDL_GL_DeleteContext(rc.gl_context);
    SDL_DestroyWindow(rc.window);
    SDL_Quit();
}

void UIContext::WidgetGestured() {
    if (IsItemActivated()) IsWidgetGesturing = true;
    if (IsItemDeactivated()) IsWidgetGesturing = false;
}

UIContext CreateUiContext(const RenderContext &RenderContext) {
    SDL_GL_MakeCurrent(RenderContext.window, RenderContext.gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    auto *imgui_context = CreateContext();
    auto *implot_context = ImPlot::CreateContext();

    auto &io = GetIO();
    io.IniFilename = nullptr; // Disable ImGui's .ini file saving. We handle this manually.

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.FontAllowUserScaling = true;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(RenderContext.window, RenderContext.gl_context);
    ImGui_ImplOpenGL3_Init(RenderContext.glsl_version);

    UIContext ui_context = {imgui_context, implot_context};
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    io.FontGlobalScale = style.ImGui.FontScale / FontAtlasScale;
    ui_context.Fonts.Main = io.Fonts->AddFontFromFileTTF("../res/fonts/AbletonSansMedium.otf", 16 * FontAtlasScale);
    ui_context.Fonts.FixedWidth = io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/Cousine-Regular.ttf", 15 * FontAtlasScale);
    io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/ProggyClean.ttf", 14 * FontAtlasScale);
    return ui_context;
}

void PrepareFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    NewFrame();
}

void RenderFrame(RenderContext &rc) {
    Render();

    glViewport(0, 0, (int)rc.io.DisplaySize.x, (int)rc.io.DisplaySize.y);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());
    SDL_GL_SwapWindow(rc.window);
}

using KeyShortcut = std::pair<ImGuiModFlags, ImGuiKey>;

const std::map<string, ImGuiModFlags> ModKeys{
    {"shift", ImGuiModFlags_Shift},
    {"ctrl", ImGuiModFlags_Ctrl},
    {"alt", ImGuiModFlags_Alt},
    {"cmd", ImGuiModFlags_Super},
};

#include <range/v3/core.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

// Handles any number of mods, along with any single non-mod character.
// Example: 'shift+cmd+s'
// **Case-sensitive. `shortcut` must be lowercase.**
static constexpr std::optional<KeyShortcut> ParseShortcut(const string &shortcut) {
    const vector<string> tokens = StringHelper::Split(shortcut, "+");
    if (tokens.empty()) return {};

    const string command = tokens.back();
    if (command.length() != 1) return {};

    const auto key = ImGuiKey(command[0] - 'a' + ImGuiKey_A);
    ImGuiModFlags mod_flags = ImGuiModFlags_None;
    for (const auto &token : ranges::views::reverse(tokens) | ranges::views::drop(1)) {
        mod_flags |= ModKeys.at(token);
    }

    return {{mod_flags, key}};
}

// Transforming `map<ActionID, string>` to `map<KeyShortcut, ActionID>`
// todo Find/implement a `BidirectionalMap` and use it here.
const auto KeyMap = action::ShortcutForId | ranges::views::transform([](const auto &entry) {
                        const auto &[action_id, shortcut] = entry;
                        return std::pair(*ParseShortcut(shortcut), action_id);
                    }) |
    ranges::to<std::map>;

bool IsShortcutPressed(const KeyShortcut &key_shortcut) {
    const auto &[mod, key] = key_shortcut;
    return mod == GetIO().KeyMods && IsKeyPressed(GetKeyIndex(key));
}

RenderContext RenderContext;

UIContext CreateUi() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMEPAD) != 0) throw std::runtime_error(SDL_GetError());

    RenderContext = CreateRenderContext();
    const auto &ui_context = CreateUiContext(RenderContext);
    IGFD::InitializeDemo();

    return ui_context;
}

// Main UI tick function
void TickUi(const Drawable &app) {
    static int PrevFontIndex = 0;
    static float PrevFontScale = 1.0;

    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT ||
            (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(RenderContext.window))) {
            q(Actions::CloseApplication{}, true);
        }
    }

    // Check if new UI settings need to be applied.
    auto &flags = UiContext.ApplyFlags;
    if (flags != UIContext::Flags_None) {
        if (flags & UIContext::Flags_ImGuiSettings) imgui_settings.Apply(UiContext.ImGui);
        if (flags & UIContext::Flags_ImGuiStyle) style.ImGui.Apply(UiContext.ImGui);
        if (flags & UIContext::Flags_ImPlotStyle) style.ImPlot.Apply(UiContext.ImPlot);
        flags = UIContext::Flags_None;
    }

    for (const auto &[shortcut, action_id] : KeyMap) {
        if (IsShortcutPressed(shortcut) && ActionAllowed(action_id)) {
            q(action::Create(action_id));
        }
    }

    PrepareFrame();
    if (PrevFontIndex != style.ImGui.FontIndex) {
        GetIO().FontDefault = GetIO().Fonts->Fonts[style.ImGui.FontIndex];
        PrevFontIndex = style.ImGui.FontIndex;
    }
    if (PrevFontScale != style.ImGui.FontScale) {
        GetIO().FontGlobalScale = style.ImGui.FontScale / FontAtlasScale;
        PrevFontScale = style.ImGui.FontScale;
    }

    app.Draw(); // All the actual application content drawing, along with initial dockspace setup, happens in this main state `Draw()` method.
    RenderFrame(RenderContext);

    auto &io = GetIO();
    if (io.WantSaveIniSettings) {
        // ImGui sometimes sets this flags when settings have not, in fact, changed.
        // E.g. if you click and hold a window-resize, it will set this every frame, even if the cursor is still (no window size change).
        const auto &patch = imgui_settings.CreatePatch(UiContext.ImGui);
        if (!patch.Empty()) q(Actions::ApplyPatch{patch});
        io.WantSaveIniSettings = false;
    }
#ifdef TRACING_ENABLED
    FrameMark;
#endif
}

void DestroyUi() {
    IGFD::CleanupDemo();
    DestroyRenderContext(RenderContext);
}
