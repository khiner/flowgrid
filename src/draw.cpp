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
#include "file_helpers.h"


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

bool main_window_open = true;
static const std::string open_file_dialog_key = "ApplicationFileDialog";

void draw_frame() {
    ZoneScoped

    static bool is_save_file_dialog = false; // open/save toggle, since the same file dialog is used for both

    // Adapted from `imgui_demo::ShowExampleAppDockSpace`
    // More docking info at https://github.com/ocornut/imgui/issues/2109
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("FlowGrid", &main_window_open, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(3);

    auto dockspace_id = ImGui::GetID("DockSpace");
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace); // Add empty node
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        auto faust_editor_id = dockspace_id;
        auto controls_id = ImGui::DockBuilderSplitNode(faust_editor_id, ImGuiDir_Left, 0.38f, nullptr, &faust_editor_id);
        auto state_viewer_id = ImGui::DockBuilderSplitNode(controls_id, ImGuiDir_Down, 0.9f, nullptr, &controls_id);
        auto state_memory_editor_id = ImGui::DockBuilderSplitNode(state_viewer_id, ImGuiDir_Down, 2.0f / 3.0f, nullptr, &state_viewer_id);
        auto state_path_update_frequency_id = ImGui::DockBuilderSplitNode(state_memory_editor_id, ImGuiDir_Down, 0.4f, nullptr, &state_memory_editor_id);
        auto imgui_windows_id = ImGui::DockBuilderSplitNode(faust_editor_id, ImGuiDir_Down, 0.5f, nullptr, &faust_editor_id);
        auto faust_log_window_id = ImGui::DockBuilderSplitNode(faust_editor_id, ImGuiDir_Down, 0.2f, nullptr, &faust_editor_id);

        const auto &w = s.windows;

        dock_window(w.controls, controls_id);

        dock_window(w.state.viewer, state_viewer_id);
        dock_window(w.state.memory_editor, state_memory_editor_id);
        dock_window(w.state.path_update_frequency, state_path_update_frequency_id);

        dock_window(w.faust.editor, faust_editor_id);
        dock_window(w.faust.log, faust_log_window_id);

        dock_window(w.style_editor, imgui_windows_id);
        dock_window(w.demos, imgui_windows_id);
        dock_window(w.metrics, imgui_windows_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::DockSpace(dockspace_id);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open project", "Cmd+o")) {
                is_save_file_dialog = false;
                ImGuiFileDialog::Instance()->OpenDialog(open_file_dialog_key, "Choose file", ".flo", ".");
            }
            // TODO 'Save' menu item, saving to current project file, only enabled if a project file is opened and there are changes
            if (ImGui::MenuItem("Save project as...")) {
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
            auto &w = ui_s.windows;
            StatefulImGui::WindowToggleMenuItem(w.controls);

            if (ImGui::BeginMenu("State")) {
                StatefulImGui::WindowToggleMenuItem(w.state.viewer);
                StatefulImGui::WindowToggleMenuItem(w.state.memory_editor);
                StatefulImGui::WindowToggleMenuItem(w.state.path_update_frequency);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("ImGui/ImPlot")) {
                StatefulImGui::WindowToggleMenuItem(w.style_editor);
                StatefulImGui::WindowToggleMenuItem(w.demos);
                StatefulImGui::WindowToggleMenuItem(w.metrics);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Faust")) {
                StatefulImGui::WindowToggleMenuItem(w.faust.editor);
                StatefulImGui::WindowToggleMenuItem(w.faust.log);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();

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

                    if (!write_file(file_path, c.state_json.dump())) {
                        // TODO console error
                    }
                } else {
                    c.state_json = json::parse(read_file(file_path));
                    c.reset_from_state_json();
                }
            }

            ImGuiFileDialog::Instance()->Close();
        }
    }

    ui_s.windows.draw();

    ImGui::End();
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
void tick_ui(UiContext &ui_context) {
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

    if (c.has_new_ini_settings) {
        s.imgui_settings.populate_context(ui_context.imgui_context);
        c.has_new_ini_settings = false;
    }
    if (c.has_new_implot_style) {
        ImPlot::BustItemCache();
        c.has_new_implot_style = false;
    }

    // Load style
    ui_context.imgui_context->Style = ui_s.style.imgui;
    ui_context.implot_context->Style = ui_s.style.implot;

    // TODO holding these keys down for super-fast undo/redo is not very stable (specifically for window resize events)
    if (shortcut(ImGuiKeyModFlags_Super, ImGuiKey_Z)) c.can_undo() && q(undo{});
    else if (shortcut(ImGuiKeyModFlags_Super | ImGuiKeyModFlags_Shift, ImGuiKey_Z)) c.can_redo() && q(redo{});

    prepare_frame();
    draw_frame();
    render_frame(render_context);

    static bool initial_save = true;
    auto &io = ImGui::GetIO();
    if (io.WantSaveIniSettings) {
        // TODO we use this (and the one in `main.cpp`) for its side-effects populating in-memory settings on ImGui's context,
        //  but it wastefully allocates and populates a text buffer with a copy of the settings in ImGui's .ini format.
        //  We should probably just update the FlowGrid ImGui fork to completely remove .ini settings handling.
        size_t settings_size = 0;
        ImGui::SaveIniSettingsToMemory(&settings_size);
        if (initial_save) {
            // The first save that ImGui triggers will have the initial loaded state.
            // TODO Once we can guarantee that initial state is loaded from either a saved or default project file,
            //  this should no longer be needed.
            c._state.imgui_settings = ImGuiSettings(ui_context.imgui_context);
            initial_save = false;
        } else {
            q(set_imgui_settings{ImGuiSettings(ui_context.imgui_context)});
        }
        io.WantSaveIniSettings = false;
    }

    FrameMark
}

void destroy_ui(UiContext &) {
    destroy_faust_editor();
    destroy_render_context(render_context);
}
