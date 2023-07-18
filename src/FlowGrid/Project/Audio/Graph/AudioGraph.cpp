#include "AudioGraph.h"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "miniaudio.h"

#include "Core/Container/AdjacencyListAction.h"
#include "Project/Audio/AudioInputDevice.h"
#include "Project/Audio/AudioOutputDevice.h"
#include "UI/HelpMarker.h"
#include "UI/InvisibleButton.h"
#include "UI/Styling.h"

AudioGraph::MaGraph::MaGraph(u32 in_channels) {
    Init(in_channels);
}
AudioGraph::MaGraph::~MaGraph() {
    Uninit();
}

void AudioGraph::MaGraph::Init(u32 in_channels) {
    ma_node_graph_config config = ma_node_graph_config_init(in_channels);
    Graph = std::make_unique<ma_node_graph>();
    const int result = ma_node_graph_init(&config, nullptr, Graph.get());

    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize node graph: {}", result));
}
void AudioGraph::MaGraph::Uninit() {
    // Graph endpoint node is already uninitialized by `Nodes.Output`.
    Graph.reset();
}

static const AudioGraph *Singleton;

AudioGraph::AudioGraph(ComponentArgs &&args) : Component(std::move(args)), Graph(InputDevice.Channels) {
    const Field::References listened_fields = {
        InputDevice.On,
        OutputDevice.On,
        InputDevice.Channels,
        OutputDevice.Channels,
        InputDevice.SampleRate,
        OutputDevice.SampleRate,
        InputDevice.Format,
        OutputDevice.Format,
        Connections,
    };
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);
    for (auto *node : Nodes) node->RegisterListener(this);

    // Set up default connections.
    // Connections.Connect(Nodes.Input.Id, Nodes.Faust.Id);
    // Connections.Connect(Nodes.Faust.Id, Nodes.Output.Id);
    Connections.Connect(Nodes.GetInput()->Id, Nodes.GetOutput()->Id);
    Singleton = this;
}

AudioGraph::~AudioGraph() {
    Singleton = nullptr;
    for (auto *node : Nodes) node->UnregisterListener(this);
    Field::UnregisterChangeListener(this);
}

void AudioGraph::OnFaustDspChanged(dsp *dsp) {
    Nodes.OnFaustDspChanged(dsp);
    UpdateConnections();
}

void AudioGraph::OnNodeConnectionsChanged(AudioGraphNode *) {
    UpdateConnections();
}

void AudioGraph::OnFieldChanged() {
    if (InputDevice.On.IsChanged() ||
        OutputDevice.On.IsChanged() ||
        InputDevice.Channels.IsChanged() ||
        OutputDevice.Channels.IsChanged() ||
        InputDevice.Format.IsChanged() ||
        OutputDevice.Format.IsChanged() ||
        InputDevice.SampleRate.IsChanged() ||
        OutputDevice.SampleRate.IsChanged()) {
        Nodes.Uninit();
        Graph.Uninit();
        Graph.Init(InputDevice.Channels);
        Nodes.Init();
        UpdateConnections();
        return;
    }
    // if (InputDevice.SampleRate.IsChanged() || OutputDevice.SampleRate.IsChanged()) {
    //     Nodes.OnDeviceSampleRateChanged();
    // }

    if (Connections.IsChanged()) {
        UpdateConnections();
    }
}

u32 AudioGraph::GetDeviceSampleRate() const { return OutputDevice.SampleRate; }
u32 AudioGraph::GetDeviceBufferSize() const { return OutputDevice.Get()->playback.internalPeriodSizeInFrames; }

void AudioGraph::AudioInputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
    if (Singleton && Singleton->Nodes.GetInput()) Singleton->Nodes.GetInput()->SetBufferData(input, frame_count);
    (void)device;
    (void)output;
}
void AudioGraph::AudioOutputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
    if (Singleton) ma_node_graph_read_pcm_frames(Singleton->Get(), output, frame_count, nullptr);
    (void)device;
    (void)input;
}

void AudioGraph::UpdateConnections() {
    for (auto *out_node : Nodes) out_node->DisconnectAll();

    for (auto *out_node : Nodes) {
        if (out_node->OutputBusCount() == 0) continue;

        for (auto *in_node : Nodes) {
            if (in_node->InputBusCount() == 0) continue;

            if (Connections.IsConnected(out_node->Id, in_node->Id)) {
                out_node->ConnectTo(*in_node);
            }
        }
    }

    // Update node active states.
    // Nodes that are turned off (here - disabled) are not removed from the `Connections` object in order to preserve their connections.
    // So we need to check if there is a path to the output node that doesn't go through any disabled nodes.
    std::unordered_set<ID> disabled_node_ids;
    for (const auto *node : Nodes) {
        if (!node->On) disabled_node_ids.insert(node->Id);
    }
    for (auto *node : Nodes) {
        node->SetActive(OutputDevice.On && Connections.HasPath(node->Id, Nodes.GetOutput()->Id, disabled_node_ids));
    }
}

using namespace ImGui;

void AudioGraph::RenderConnections() const {
    // Calculate the maximum I/O label widths.
    ImVec2 max_label_w_no_padding{0, 0}; // I/O vec: in (left), out (top)
    for (const auto *node : Nodes) {
        const float label_w = CalcTextSize(node->Name.c_str()).x;
        if (node->InputBusCount() > 0) max_label_w_no_padding.x = std::max(max_label_w_no_padding.x, label_w);
        if (node->OutputBusCount() > 0) max_label_w_no_padding.y = std::max(max_label_w_no_padding.y, label_w);
    }

    const ImVec2 label_padding = ImVec2{ImGui::GetStyle().ItemInnerSpacing.x, 0} + ImGui::GetStyle().FramePadding;

    const auto &style = Style.Matrix;
    const float max_allowed_label_w = style.MaxLabelSpace * GetTextLineHeight();
    const ImVec2 node_label_w_no_padding = {std::min(max_allowed_label_w, max_label_w_no_padding.x), std::min(max_allowed_label_w, max_label_w_no_padding.y)};
    const ImVec2 node_label_w = node_label_w_no_padding + label_padding.x * 2; // I/O vec
    const float fhws = GetFrameHeightWithSpacing();
    const auto og_cursor_pos = GetCursorScreenPos();
    const ImVec2 grid_top_left = og_cursor_pos + node_label_w + fhws; // Last line-height is for the I/O header labels.

    BeginGroup();

    static const string InputsLabel = "Inputs";
    static const string OutputsLabel = "Outputs";
    // I/O header frames + labels on the left/top, respectively.
    const ImVec2 io_header_w_no_padding = ImVec2{CalcTextSize(InputsLabel).x, CalcTextSize(OutputsLabel).x}; // I/O vec
    const ImVec2 io_header_w = io_header_w_no_padding + label_padding.x * 2; // I/O vec
    ImVec2 io_frame_w = GetContentRegionAvail() - (node_label_w + fhws); // I/O vec
    io_frame_w = ImVec2{std::max(io_frame_w.x, io_header_w.x), std::max(io_frame_w.y, io_header_w.y)};

    SetCursorScreenPos({grid_top_left.x, og_cursor_pos.y});
    RenderFrame(
        GetCursorScreenPos(),
        GetCursorScreenPos() + ImVec2{io_frame_w.x, fhws},
        GetColorU32(ImGuiCol_FrameBg)
    );
    RenderText(GetCursorScreenPos() + ImVec2{(io_frame_w.x - io_header_w.x) / 2, 0} + label_padding, InputsLabel.c_str());

    SetCursorScreenPos({og_cursor_pos.x, grid_top_left.y});
    RenderFrame(
        GetCursorScreenPos(),
        GetCursorScreenPos() + ImVec2{fhws, io_frame_w.y},
        GetColorU32(ImGuiCol_FrameBg)
    );
    ImPlot::AddTextVertical(
        GetWindowDrawList(),
        GetCursorScreenPos() + ImVec2{0, (io_frame_w.y - io_header_w.y) / 2 + io_header_w_no_padding.y} + ImVec2{label_padding.y, label_padding.x},
        GetColorU32(ImGuiCol_Text), OutputsLabel.c_str()
    );

    const float cell_size = style.CellSize * GetTextLineHeight();
    const float cell_gap = style.CellGap;

    // Output channel labels.
    u32 out_count = 0;
    for (const auto *out_node : Nodes) {
        if (out_node->OutputBusCount() == 0) continue;

        SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * out_count, -node_label_w.y});
        const auto label_interaction_flags = fg::InvisibleButton({cell_size, node_label_w.y}, out_node->ImGuiLabel.c_str());

        const string label = out_node->Name;
        const string ellipsified_label = Ellipsify(label, node_label_w_no_padding.y);
        const bool is_active = out_node->IsActive;
        if (!is_active) BeginDisabled();
        ImPlot::AddTextVertical(
            GetWindowDrawList(),
            grid_top_left + ImVec2{(cell_size + cell_gap) * out_count + (cell_size - GetTextLineHeight()) / 2, -label_padding.y},
            GetColorU32(ImGuiCol_Text), ellipsified_label.c_str()
        );
        if (!is_active) EndDisabled();

        const bool text_clipped = ellipsified_label.find("...") != string::npos;
        if (text_clipped && (label_interaction_flags & InteractionFlags_Hovered)) SetTooltip("%s", label.c_str());
        out_count++;
    }

    // Input channel labels and mixer cells.
    u32 in_i = 0;
    for (const auto *in_node : Nodes) {
        if (in_node->InputBusCount() == 0) continue;

        SetCursorScreenPos(grid_top_left + ImVec2{-node_label_w.x, (cell_size + cell_gap) * in_i});
        const auto label_interaction_flags = fg::InvisibleButton({node_label_w.x, cell_size}, in_node->ImGuiLabel.c_str());

        const string label = in_node->Name;
        const string ellipsified_label = Ellipsify(label, node_label_w_no_padding.x);
        SetCursorPos(GetCursorPos() + ImVec2{node_label_w.x - CalcTextSize(ellipsified_label.c_str()).x - label_padding.y, (cell_size - GetTextLineHeight()) / 2}); // Right-align & vertically center label.

        const bool is_active = in_node->IsActive;
        if (!is_active) BeginDisabled();
        TextUnformatted(ellipsified_label.c_str());
        if (!is_active) EndDisabled();

        const bool text_clipped = ellipsified_label.find("...") != string::npos;
        if (text_clipped && (label_interaction_flags & InteractionFlags_Hovered)) SetTooltip("%s", label.c_str());

        u32 out_i = 0;
        for (const auto *out_node : Nodes) {
            if (out_node->OutputBusCount() == 0) continue;

            PushID(in_i * out_count + out_i);
            SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * out_i, (cell_size + cell_gap) * in_i});

            const bool disabled = out_node->Id == in_node->Id;
            if (disabled) BeginDisabled();

            const auto flags = fg::InvisibleButton({cell_size, cell_size}, "Cell");
            if (flags & InteractionFlags_Clicked) {
                Action::AdjacencyList::ToggleConnection{Connections.Path, out_node->Id, in_node->Id}.q();
            }

            const bool is_connected = Connections.IsConnected(out_node->Id, in_node->Id);
            const auto fill_color =
                flags & InteractionFlags_Held ?
                ImGuiCol_ButtonActive :
                (flags & InteractionFlags_Hovered ?
                     ImGuiCol_ButtonHovered :
                     (is_connected ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg)
                );
            RenderFrame(GetItemRectMin(), GetItemRectMax(), GetColorU32(fill_color));

            if (disabled) EndDisabled();

            PopID();
            out_i++;
        }
        in_i++;
    }
    EndGroup();
}

void AudioGraph::Style::Matrix::Render() const {
    CellSize.Draw();
    CellGap.Draw();
    MaxLabelSpace.Draw();
}
