#include <SDL.h>
#include <SDL_opengl.h>
#include <Tracy.hpp>
#include "context.h"
#include "draw.h"
#include "stateful_imgui.h"
#include "windows/faust_editor.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h" // TODO metal
#include "ImGuiFileDialog.h"

struct RenderContext {
    SDL_Window *window = nullptr;
    SDL_GLContext gl_context{};
    const char *glsl_version = "#version 150";
    ImGuiIO io;
};

/**md
## UI methods

These are the only public methods.

```cpp
    create_ui();
    tick_ui();
    destroy_ui(render_context);
```

## Render context methods

```cpp
    create_render_context();
    destroy_render_context(render_context);
```

## UI context methods

Superset of render context.

```cpp
    create_ui_context();
```

## Frame methods

```cpp
    prepare_frame();
    draw_frame();
    render_frame(render_context);
```
 */

RenderContext create_render_context() {
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

void destroy_render_context(const RenderContext &rc) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    ImPlot::DestroyContext();

    SDL_GL_DeleteContext(rc.gl_context);
    SDL_DestroyWindow(rc.window);
    SDL_Quit();
}

UiContext create_ui_context(const RenderContext &render_context) {
    SDL_GL_MakeCurrent(render_context.window, render_context.gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    auto *imgui_context = ImGui::CreateContext();
    auto *implot_context = ImPlot::CreateContext();

    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr; // Disable ImGui's .ini file saving. We handle this manually.

    // However, since the default ImGui behavior is to write to disk (to the .ini file) when the ini state is marked dirty,
    // it buffers marking dirty (`g.IO.WantSaveIniSettings = true`) with a `io.IniSavingRate` timer (which is 5s by default).
    // We want this to be a very small value, since we want to create actions for the undo stack as soon after a user action
    // as possible.
    // TODO closing windows or changing their dockspace should be independent actions, but resize events can get rolled into
    //  the next action?
    io.IniSavingRate = 0.1;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(render_context.window, render_context.gl_context);
    ImGui_ImplOpenGL3_Init(render_context.glsl_version);

    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    c.defaultFont = io.Fonts->AddFontFromFileTTF("../res/fonts/AbletonSansMedium.otf", 16.0f);
    c.fixedWidthFont = io.Fonts->AddFontFromFileTTF("../res/fonts/Cousine-Regular.ttf", 15.0f);
//    c.defaultFont = io.Fonts->AddFontFromFileTTF("../res/fonts/Roboto-Medium.ttf", 16.0f);

    return {imgui_context, implot_context};
}

void prepare_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void render_frame(RenderContext &rc) {
    ImGui::Render();
    glViewport(0, 0, (int) rc.io.DisplaySize.x, (int) rc.io.DisplaySize.y);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(rc.window);
}

static const string open_file_dialog_key = "ApplicationFileDialog";

bool first_draw = true;

void draw_frame() {
    ZoneScoped

    // Good initial layout setup example in this issue: https://github.com/ocornut/imgui/issues/3548
    auto dockspace_id = ImGui::DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    if (first_draw) {
        auto faust_editor_id = dockspace_id;
        auto controls_id = ImGui::DockBuilderSplitNode(faust_editor_id, ImGuiDir_Left, 0.38f, nullptr, &faust_editor_id);
        auto state_viewer_id = ImGui::DockBuilderSplitNode(controls_id, ImGuiDir_Down, 0.9f, nullptr, &controls_id);
        auto state_memory_editor_id = ImGui::DockBuilderSplitNode(state_viewer_id, ImGuiDir_Down, 2.0f / 3.0f, nullptr, &state_viewer_id);
        auto state_path_update_frequency_id = ImGui::DockBuilderSplitNode(state_memory_editor_id, ImGuiDir_Down, 0.4f, nullptr, &state_memory_editor_id);
        auto imgui_windows_id = ImGui::DockBuilderSplitNode(faust_editor_id, ImGuiDir_Down, 0.5f, nullptr, &faust_editor_id);
        auto faust_log_window_id = ImGui::DockBuilderSplitNode(faust_editor_id, ImGuiDir_Down, 0.2f, nullptr, &faust_editor_id);

        dock_window(s.audio.settings, controls_id);
        dock_window(s.audio.faust.editor, faust_editor_id);
        dock_window(s.audio.faust.log, faust_log_window_id);

        dock_window(s.state.viewer, state_viewer_id);
        dock_window(s.state.memory_editor, state_memory_editor_id);
        dock_window(s.state.path_update_frequency, state_path_update_frequency_id);

        dock_window(s.style, imgui_windows_id);
        dock_window(s.demo, imgui_windows_id);
        dock_window(s.metrics, imgui_windows_id);
        first_draw = false;
    }

    static bool is_save_file_dialog = false; // open/save toggle, since the same file dialog is used for both
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open default project", "Cmd+Shift+O")) c.open_default_project();
            if (ImGui::MenuItem("Open project", "Cmd+o")) {
                is_save_file_dialog = false;
                ImGuiFileDialog::Instance()->OpenDialog(open_file_dialog_key, "Choose file", ".flo", ".");
            }
            if (ImGui::MenuItem("Save default project", "Cmd+Shift+S")) c.save_default_project();
            // TODO 'Save' menu item, saving to current project file, only enabled if a project file is opened and there are changes
            if (ImGui::MenuItem("Save project as...", "Cmd+s")) {
                is_save_file_dialog = true;
                ImGuiFileDialog::Instance()->OpenDialog(open_file_dialog_key, "Choose file", ".flo", ".", "my_flowgrid_project", 1, nullptr, ImGuiFileDialogFlags_ConfirmOverwrite);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Cmd+z", false, c.can_undo())) { q(undo{}); }
            if (ImGui::MenuItem("Redo", "Cmd+Shift+Z", false, c.can_redo())) { q(redo{}); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Windows")) {
            if (ImGui::BeginMenu("State")) {
                StatefulImGui::WindowToggleMenuItem(ui_s.state.viewer);
                StatefulImGui::WindowToggleMenuItem(ui_s.state.memory_editor);
                StatefulImGui::WindowToggleMenuItem(ui_s.state.path_update_frequency);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Audio")) {
                StatefulImGui::WindowToggleMenuItem(ui_s.audio.settings);
                if (ImGui::BeginMenu("Faust")) {
                    StatefulImGui::WindowToggleMenuItem(ui_s.audio.faust.editor);
                    StatefulImGui::WindowToggleMenuItem(ui_s.audio.faust.log);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            StatefulImGui::WindowToggleMenuItem(ui_s.style);
            if (ImGui::BeginMenu("ImGui/ImPlot")) {
                StatefulImGui::WindowToggleMenuItem(ui_s.demo);
                StatefulImGui::WindowToggleMenuItem(ui_s.metrics);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();

        // TODO need to get custom vecs with math going
        const ImVec2 min_dialog_size = {ImGui::GetMainViewport()->Size.x / 2.0f, ImGui::GetMainViewport()->Size.y / 2.0f};
        if (ImGuiFileDialog::Instance()->Display(open_file_dialog_key, ImGuiWindowFlags_NoCollapse, min_dialog_size)) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                // TODO provide an option to save with undo state.
                //   This file format would be a json list of diffs.
                //   The file would generally be larger, and the load time would be slower,
                //   but it would provide the option to save/load _exactly_ as if you'd never quit at all,
                //   with full undo/redo history/position/etc.!
                const auto &file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                if (is_save_file_dialog) {
                    if (!c.save_project(file_path)) {
                        // TODO console error
                    }
                } else {
                    c.open_project(file_path);
                }
            }

            ImGuiFileDialog::Instance()->Close();
        }
    }

    StatefulImGui::DrawWindow(ui_s.demo, ImGuiWindowFlags_MenuBar);
    StatefulImGui::DrawWindow(ui_s.metrics);
    ui_s.audio.draw();
    ui_s.state.draw();
    StatefulImGui::DrawWindow(ui_s.style);
}

bool shortcut(ImGuiKeyModFlags mod, ImGuiKey key) {
    return mod == ImGui::GetMergedModFlags() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(key));
}

bool closed_this_frame = false;

RenderContext render_context;

UiContext create_ui() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) throw std::runtime_error(SDL_GetError());

    render_context = create_render_context();
    return create_ui_context(render_context);
}

// Main UI tick function
void tick_ui() {
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
                event.window.windowID == SDL_GetWindowID(render_context.window))) {
            q(close_application{});
            closed_this_frame = true;
        }
    }

    // If the user close the application via the window-close button _this frame_ (or by other means since the previous
    // check above), bail immediately.
    // (Like all other actions, the `close_application` action is enqueued and handled in the main event loop thread.
    // However, we don't want to render another frame after enqueueing a close action, since resources can be deallocated
    // at any point thereafter.)
    if (closed_this_frame || !s.processes.ui.running) {
        FrameMark;
        return;
    }

    if (shortcut(ImGuiKeyModFlags_Super, ImGuiKey_Z)) c.can_undo() && q(undo{});
    else if (shortcut(ImGuiKeyModFlags_Super | ImGuiKeyModFlags_Shift, ImGuiKey_Z)) c.can_redo() && q(redo{});
    else if (shortcut(ImGuiKeyModFlags_Super | ImGuiKeyModFlags_Shift, ImGuiKey_O)) c.open_default_project();
    else if (shortcut(ImGuiKeyModFlags_Super | ImGuiKeyModFlags_Shift, ImGuiKey_S)) c.save_default_project();

    prepare_frame();
    draw_frame();
    render_frame(render_context);

    auto &io = ImGui::GetIO();
    if (io.WantSaveIniSettings) {
        q(set_imgui_settings{ImGuiSettings(c.ui->imgui_context)});
        io.WantSaveIniSettings = false;
    }

    FrameMark
}

void destroy_ui() {
    destroy_faust_editor();
    destroy_render_context(render_context);
}
