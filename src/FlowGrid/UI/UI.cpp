#include "SDL.h"
#include "SDL_opengl.h"
#include "imgui_impl_opengl3.h" // TODO vulkan
#include "imgui_impl_sdl.h"
#include "nlohmann/json.hpp"
#include "zep/stringutils.h"
#include <Tracy.hpp>

#include "../App.h"

#include "../FileDialog/FileDialogDemo.h"
#include "Faust/FaustEditor.h"

using namespace fg;
using namespace ImGui;

/**
Public methods:
    CreateUi();
    TickUi();
    DestroyUi();

Internal render context methods:
    CreateRenderContext();
    DestroyRenderContext(RenderContext);

Internal frame methods:
    PrepareFrame();
    RenderFrame(RenderContext);
 */

struct RenderContext {
    SDL_Window *window = nullptr;
    SDL_GLContext gl_context{};
    const char *glsl_version = "#version 150";
    ImGuiIO io;
};

RenderContext CreateRenderContext() {
#if defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    context.glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    auto window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;

    RenderContext draw_context;
    draw_context.window = SDL_CreateWindow("FlowGrid", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    draw_context.gl_context = SDL_GL_CreateContext(draw_context.window);

    return draw_context;
}

void DestroyRenderContext(const RenderContext &rc) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    DestroyContext();
    ImPlot::DestroyContext();

    SDL_GL_DeleteContext(rc.gl_context);
    SDL_DestroyWindow(rc.window);
    SDL_Quit();
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
    ImGui_ImplSDL2_InitForOpenGL(RenderContext.window, RenderContext.gl_context);
    ImGui_ImplOpenGL3_Init(RenderContext.glsl_version);

    UIContext ui_context = {imgui_context, implot_context};
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    const static float atlas_scale = Style::ImGuiStyle::FontAtlasScale;
    io.FontGlobalScale = s.Style.ImGui.FontScale / atlas_scale;
    ui_context.Fonts.Main = io.Fonts->AddFontFromFileTTF("../res/fonts/AbletonSansMedium.otf", 16 * atlas_scale);
    ui_context.Fonts.FixedWidth = io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/Cousine-Regular.ttf", 15 * atlas_scale);
    io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/ProggyClean.ttf", 14 * atlas_scale);
    return ui_context;
}

void PrepareFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    NewFrame();
}

void RenderFrame(RenderContext &rc) {
    Render();
    glViewport(0, 0, (int)rc.io.DisplaySize.x, (int)rc.io.DisplaySize.y);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());
    SDL_GL_SwapWindow(rc.window);
}

using KeyShortcut = pair<ImGuiModFlags, ImGuiKey>;

const map<string, ImGuiModFlags> ModKeys{
    {"shift", ImGuiModFlags_Shift},
    {"ctrl", ImGuiModFlags_Ctrl},
    {"alt", ImGuiModFlags_Alt},
    {"cmd", ImGuiModFlags_Super},
};

// Handles any number of mods, along with any single non-mod character.
// Example: 'shift+cmd+s'
// **Case-sensitive. `shortcut` must be lowercase.**
std::optional<KeyShortcut> ParseShortcut(const string &shortcut) {
    vector<string> tokens;
    Zep::string_split(shortcut, "+", tokens);
    if (tokens.empty()) return {};

    const string command = tokens.back();
    if (command.length() != 1) return {};

    tokens.pop_back();

    auto key = ImGuiKey(command[0] - 'a' + ImGuiKey_A);
    ImGuiModFlags mod_flags = ImGuiModFlags_None;
    while (!tokens.empty()) {
        mod_flags |= ModKeys.at(tokens.back());
        tokens.pop_back();
    }

    return {{mod_flags, key}};
}

// Transforming `map<ActionID, string>` to `map<KeyShortcut, ActionID>`
// todo Find/implement a `BidirectionalMap` and use it here.
const auto KeyMap = action::ShortcutForId | transform([](const auto &entry) {
                        const auto &[action_id, shortcut] = entry;
                        return pair(*ParseShortcut(shortcut), action_id);
                    }) |
    to<map>;

bool IsShortcutPressed(const KeyShortcut &key_shortcut) {
    const auto &[mod, key] = key_shortcut;
    return mod == GetIO().KeyMods && IsKeyPressed(GetKeyIndex(key));
}

RenderContext RenderContext;

UIContext CreateUi() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) throw std::runtime_error(SDL_GetError());

    RenderContext = CreateRenderContext();
    const auto &ui_context = CreateUiContext(RenderContext);
    IGFD::InitializeDemo();

    return ui_context;
}

// Main UI tick function
void TickUi() {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT ||
            (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
             event.window.windowID == SDL_GetWindowID(RenderContext.window))) {
            q(CloseApplication{}, true);
        }
    }

    // Check if new UI settings need to be applied.
    if (UiContext.ApplyFlags != UIContext::Flags_None) {
        s.Apply(UiContext.ApplyFlags);
        UiContext.ApplyFlags = UIContext::Flags_None;
    }

    for (const auto &[shortcut, action_id] : KeyMap) {
        if (IsShortcutPressed(shortcut) && c.ActionAllowed(action_id)) {
            q(action::Create(action_id));
        }
    }

    PrepareFrame();
    s.Draw(); // All the actual application content drawing, along with initial dockspace setup, happens in this main state `Draw()` method.
    RenderFrame(RenderContext);

    auto &io = GetIO();
    if (io.WantSaveIniSettings) {
        // ImGui sometimes sets this flags when settings have not, in fact, changed.
        // E.g. if you click and hold a window-resize, it will set this every frame, even if the cursor is still (no window size change).
        Store new_store = s.ImGuiSettings.Set(UiContext.ImGui);
        const auto &patch = CreatePatch(AppStore, new_store, s.ImGuiSettings.Path);
        if (!patch.empty()) q(ApplyPatch{patch});
        io.WantSaveIniSettings = false;
    }

    FrameMark;
}

void DestroyUi() {
    IGFD::CleanupDemo();
    DestroyFaustEditor();
    DestroyRenderContext(RenderContext);
}

//-----------------------------------------------------------------------------
// [SECTION] Widgets
//-----------------------------------------------------------------------------

using namespace ImGui;

namespace FlowGrid {
void HelpMarker(const char *help) {
    TextDisabled("(?)");
    if (IsItemHovered()) {
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35);
        TextUnformatted(help);
        PopTextWrapPos();
        EndTooltip();
    }
}

bool InvisibleButton(const ImVec2 &size_arg, bool *out_hovered, bool *out_held) {
    auto *window = GetCurrentWindow();
    if (window->SkipItems) return false;

    const auto id = window->GetID("");
    const auto size = CalcItemSize(size_arg, 0.0f, 0.0f);
    const auto &cursor = GetCursorScreenPos();
    const ImRect rect{cursor, cursor + size};
    if (!ItemAdd(rect, id)) return false;

    return ButtonBehavior(rect, id, out_hovered, out_held, ImGuiButtonFlags_None);
}

void MenuItem(const EmptyAction &action) {
    const string menu_label = action::GetMenuLabel(action);
    const string shortcut = action::GetShortcut(action);
    if (ImGui::MenuItem(menu_label.c_str(), shortcut.c_str(), false, c.ActionAllowed(action))) {
        Match(action, [](const auto &a) { q(a); });
    }
}

bool JsonTreeNode(const string &label, JsonTreeNodeFlags flags, const char *id) {
    const bool highlighted = flags & JsonTreeNodeFlags_Highlighted;
    const bool disabled = flags & JsonTreeNodeFlags_Disabled;
    const ImGuiTreeNodeFlags imgui_flags = flags & JsonTreeNodeFlags_DefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

    if (disabled) BeginDisabled();
    if (highlighted) PushStyleColor(ImGuiCol_Text, s.Style.FlowGrid.Colors[FlowGridCol_HighlightText]);
    const bool is_open = id ? TreeNodeEx(id, imgui_flags, "%s", label.c_str()) : TreeNodeEx(label.c_str(), imgui_flags);
    if (highlighted) PopStyleColor();
    if (disabled) EndDisabled();

    return is_open;
}

void JsonTree(const string &label, const json &value, JsonTreeNodeFlags node_flags, const char *id) {
    if (value.is_null()) {
        TextUnformatted(label.empty() ? "(null)" : label.c_str());
    } else if (value.is_object()) {
        if (label.empty() || JsonTreeNode(label, node_flags, id)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                JsonTree(it.key(), *it, node_flags);
            }
            if (!label.empty()) TreePop();
        }
    } else if (value.is_array()) {
        if (label.empty() || JsonTreeNode(label, node_flags, id)) {
            Count i = 0;
            for (const auto &it : value) {
                JsonTree(to_string(i), it, node_flags);
                i++;
            }
            if (!label.empty()) TreePop();
        }
    } else {
        if (label.empty()) TextUnformatted(value.dump().c_str());
        else Text("%s: %s", label.c_str(), value.dump().c_str());
    }
}
} // namespace FlowGrid
