#include <iostream>
#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h" // TODO metal
#include "draw.h"
#include "context.h"
#include "windows/faust_editor.h"
#include "windows/show_window.h"
#include "imgui_internal.h"
#include "windows/controls.h"

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
    auto window_flags = (SDL_WindowFlags) (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    DrawContext draw_context;
    draw_context.window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL3 example",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        window_flags);
    draw_context.gl_context = SDL_GL_CreateContext(draw_context.window);

    return draw_context;
}

void load_fonts() {
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);
}

void setup(DrawContext &dc) {
    SDL_GL_MakeCurrent(dc.window, dc.gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    // Disable ImGui's .ini file saving. We handle this manually.
    io.IniFilename = nullptr;
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
}

void teardown(DrawContext &dc) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(dc.gl_context);
    SDL_DestroyWindow(dc.window);
    SDL_Quit();
}

void render(DrawContext &dc, const Color &clear_color) {
    ImGui::Render();
    glViewport(0, 0, (int) dc.io.DisplaySize.x, (int) dc.io.DisplaySize.y);
    glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(dc.window);
}

FaustEditor faust_editor{};
Controls controls{};

// Usually this state management happens in `show_window`, but the demo window doesn't expose
// all its window state handling like we do with internal windows.
// Thus, only the demo window's visibility state is part of the undo stack
// (whereas with internal windows, other things like the collapsed state are considered undoable events).
void draw_demo_window(const std::string &window_name) {
    const auto &w = s.ui.windows.at(window_name);
    auto &mutable_w = ui_s.ui.windows[window_name];
    if (mutable_w.visible != w.visible) q.enqueue(toggle_window{window_name});
    if (w.visible) ImGui::ShowDemoWindow(&mutable_w.visible);
}

void draw_metrics_window(const std::string &window_name) {
    const auto &w = s.ui.windows.at(window_name);
    auto &mutable_w = ui_s.ui.windows[window_name];
    if (mutable_w.visible != w.visible) q.enqueue(toggle_window{window_name});
    if (w.visible) ImGui::ShowMetricsWindow(&mutable_w.visible);
}

bool open = true;

// TODO see https://github.com/ocornut/imgui/issues/2109#issuecomment-426204357
//  for how to programmatically set up a default layout

void draw_frame() {
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
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(3);

    auto dockspace_id = ImGui::GetID("DockSpace");
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace); // Add empty node
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        auto dock_main_id = dockspace_id; // This variable will track the document node, however we are not using it here as we aren't docking anything into it.
        auto dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.35f, nullptr, &dock_main_id);
        auto dock_id_bottom_left = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, 0.5f, nullptr, &dock_id_left);
        auto dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.5f, nullptr, &dock_main_id);

        ImGui::DockBuilderDockWindow(WindowNames::controls.c_str(), dock_id_left);
        ImGui::DockBuilderDockWindow(WindowNames::imgui_metrics.c_str(), dock_id_bottom_left);
        ImGui::DockBuilderDockWindow(WindowNames::faust_editor.c_str(), dock_main_id);
        ImGui::DockBuilderDockWindow(WindowNames::imgui_demo.c_str(), dock_id_bottom);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::DockSpace(dockspace_id);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Options")) {
            bool opt_placeholder;
            ImGui::MenuItem("Placeholder", nullptr, &opt_placeholder);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    draw_demo_window(WindowNames::imgui_demo);
    draw_metrics_window(WindowNames::imgui_metrics);
    draw_window(WindowNames::faust_editor, faust_editor);
    draw_window(WindowNames::controls, controls);

    ImGui::End();
}

int draw() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    auto dc = create_draw_context();
    setup(dc);

    if (!c.ini_settings.empty()) {
        ImGui::LoadIniSettingsFromMemory(c.ini_settings.c_str(), c.ini_settings.size());
    }

    // Main loop
    while (s.ui.running) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                    event.window.windowID == SDL_GetWindowID(dc.window))) {
                q.enqueue(close_application{});
            }
        }

        auto &io = ImGui::GetIO();
        if (c.has_new_ini_settings) {
            ImGui::LoadIniSettingsFromMemory(c.ini_settings.c_str(), c.ini_settings.size());
            c.has_new_ini_settings = false;
        }

        new_frame();
        draw_frame();
        render(dc, s.ui.colors.clear);

        static bool initial_save = true;
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
    }

    faust_editor.destroy();
    teardown(dc);

    return 0;
}
