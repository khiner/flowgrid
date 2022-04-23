#include <SDL.h>
#include <SDL_opengl.h>
#include <Tracy.hpp>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h" // TODO metal
#include "implot.h"
#include "draw.h"
#include "context.h"
#include "stateful_imgui.h"
#include "window/windows.h"

struct DrawContext {
    SDL_Window *window = nullptr;
    SDL_GLContext gl_context{};
    const char *glsl_version = "#version 150";
    ImGuiIO io;
};

void new_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

DrawContext create_draw_context() {
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

    DrawContext draw_context;
    draw_context.window = SDL_CreateWindow("FlowGrid", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    draw_context.gl_context = SDL_GL_CreateContext(draw_context.window);

    return draw_context;
}

void load_fonts() {
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    auto &io = ImGui::GetIO();
    c.defaultFont = io.Fonts->AddFontFromFileTTF("../res/fonts/AbletonSansMedium.otf", 16.0f);
    c.fixedWidthFont = io.Fonts->AddFontFromFileTTF("../res/fonts/Cousine-Regular.ttf", 15.0f);
//    c.defaultFont = io.Fonts->AddFontFromFileTTF("../res/fonts/Roboto-Medium.ttf", 16.0f);
}

bool shortcut(ImGuiKeyModFlags mod, ImGuiKey key) {
    return mod == ImGui::GetMergedModFlags() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(key));
}

ImGuiContext *setup(DrawContext &dc) {
    SDL_GL_MakeCurrent(dc.window, dc.gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    auto *imgui_context = ImGui::CreateContext();
    ImPlot::CreateContext();

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

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(dc.window, dc.gl_context);
    ImGui_ImplOpenGL3_Init(dc.glsl_version);

    load_fonts();

    return imgui_context;
}

void teardown(DrawContext &dc) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    ImPlot::DestroyContext();

    SDL_GL_DeleteContext(dc.gl_context);
    SDL_DestroyWindow(dc.window);
    SDL_Quit();
}

void render(DrawContext &dc) {
    ImGui::Render();
    glViewport(0, 0, (int) dc.io.DisplaySize.x, (int) dc.io.DisplaySize.y);
//    glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(dc.window);
}

Controls controls{};
StyleEditor style_editor{};
StateViewer state_viewer{};
FaustEditor faust_editor{};
FaustLog faust_log{};
ImGuiWindows::Demo imgui_demo{};
ImGuiWindows::Metrics imgui_metrics{};
ImGuiWindows::ImPlotWindows::Demo implot_demo{};

bool open = true;

// TODO see https://github.com/ocornut/imgui/issues/2109#issuecomment-426204357
//  for how to programmatically set up a default layout

void draw_frame() {
    ZoneScoped

    // Adapted from `imgui_demo::ShowExampleAppDockSpace`
    // More docking info at https://github.com/ocornut/imgui/issues/2109
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("FlowGrid", &open, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(3);

    const auto &w = s.ui.windows;
    auto dockspace_id = ImGui::GetID("DockSpace");
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace); // Add empty node
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        auto faust_editor_id = dockspace_id;
        auto controls_id = ImGui::DockBuilderSplitNode(faust_editor_id, ImGuiDir_Left, 0.35f, nullptr, &faust_editor_id);
        auto state_viewer_id = ImGui::DockBuilderSplitNode(controls_id, ImGuiDir_Down, 0.9f, nullptr, &controls_id);
        auto imgui_windows_id = ImGui::DockBuilderSplitNode(faust_editor_id, ImGuiDir_Down, 0.5f, nullptr, &faust_editor_id);
        auto faust_log_window_id = ImGui::DockBuilderSplitNode(faust_editor_id, ImGuiDir_Down, 0.2f, nullptr, &faust_editor_id);

        // TODO create a single parent "ImGui Windows" window with imgui child windows,
        //  where the tabs can't be dragged out of the window, and nothing else can be dragged in.
        //  Checkboxes that currently control imgui window visibility become menu bar checkboxes.
        //  ImGui windows can, however, be docked _within_ the imgui parent window, and the parent window
        //  itself can be dragged/docked/closed etc.
        //  See `ImGuiWindowClass`.
        dock_window(w.controls.name, controls_id);
        dock_window(w.state_viewer.name, state_viewer_id);

        dock_window(w.faust.editor.name, faust_editor_id);
        dock_window(w.faust.log.name, faust_log_window_id);

        dock_window(w.style_editor.name, imgui_windows_id);
        dock_window(w.imgui.metrics.name, imgui_windows_id);
        dock_window(w.imgui.demo.name, imgui_windows_id);
        dock_window(w.imgui.implot.demo.name, imgui_windows_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::DockSpace(dockspace_id);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Undo", "Cmd+z", false, c.can_undo())) { q.enqueue(undo{}); }
            if (ImGui::MenuItem("Redo", "Cmd+Shift+Z", false, c.can_redo())) { q.enqueue(redo{}); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Windows")) {
            for (const auto &window: s.ui.windows.all_const) {
                StatefulImGui::WindowToggleMenuItem(window.get().name);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    draw_window(w.style_editor.name, style_editor, ImGuiWindowFlags_None);
    draw_window(w.imgui.metrics.name, imgui_metrics, ImGuiWindowFlags_None, false);
    draw_window(w.imgui.demo.name, imgui_demo, ImGuiWindowFlags_None, false);
    draw_window(w.imgui.implot.demo.name, implot_demo, ImGuiWindowFlags_None, false);

    draw_window(w.faust.editor.name, faust_editor, ImGuiWindowFlags_MenuBar);
    draw_window(w.faust.log.name, faust_log, ImGuiWindowFlags_None);
    draw_window(w.controls.name, controls);
    draw_window(w.state_viewer.name, state_viewer, ImGuiWindowFlags_MenuBar);

    ImGui::End();
}

bool closed_this_frame = false;

int draw() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    auto dc = create_draw_context();
    auto *imgui_context = setup(dc);

    if (!c.ini_settings.empty()) {
        ImGui::LoadIniSettingsFromMemory(c.ini_settings.c_str(), c.ini_settings.size());
    }

    // Main loop
    while (s.ui.running) {
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
                    event.window.windowID == SDL_GetWindowID(dc.window))) {
                q.enqueue(close_application{});
                closed_this_frame = true;
            }
        }

        // If the user close the application via the window-close button _this frame_ (or by other means since the previous
        // check above), bail immediately.
        // (Like all other actions, the `close_application` action is enqueued and handled in the main event loop thread.
        // However, we don't want to render another frame after enqueueing a close action, since resources can be deallocated
        // at any point thereafter.)
        if (closed_this_frame || !s.ui.running) {
            FrameMark;
            break;
        }

        if (c.has_new_ini_settings) {
            ImGui::LoadIniSettingsFromMemory(c.ini_settings.c_str(), c.ini_settings.size());
            c.has_new_ini_settings = false;
        }

        imgui_context->Style = ui_s.ui.style; // Load style

        // TODO holding these keys down for super-fast undo/redo is not very stable (lost events?)
        if (shortcut(ImGuiKeyModFlags_Super, ImGuiKey_Z)) c.can_undo() && q.enqueue(undo{});
        else if (shortcut(ImGuiKeyModFlags_Super | ImGuiKeyModFlags_Shift, ImGuiKey_Z)) c.can_redo() && q.enqueue(redo{});

        new_frame();
        draw_frame();
        render(dc);

        static bool initial_save = true;
        auto &io = ImGui::GetIO();
        if (io.WantSaveIniSettings) {
            size_t settings_size = 0;
            const char *settings = ImGui::SaveIniSettingsToMemory(&settings_size);
            if (initial_save) {
                // The first save that ImGui triggers will have the initial loaded state.
                // TODO Once we can guarantee that initial state is loaded from either a saved or default project file,
                //  this should no longer be needed.
                c.ini_settings = c.prev_ini_settings = settings;
                initial_save = false;
            } else {
                q.enqueue(set_ini_settings{settings});
            }
            io.WantSaveIniSettings = false;
        }

        FrameMark
    }

    faust_editor.destroy();
    teardown(dc);

    return 0;
}
