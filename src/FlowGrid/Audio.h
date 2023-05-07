#pragma once

#include "WindowMember.h"

#include "UI/Style.h"
#include "UI/UI.h"

// Starting at `-1` allows for using `IO` types as array indices.
enum IO_ {
    IO_None = -1,
    IO_In,
    IO_Out
};
using IO = IO_;

constexpr IO IO_All[] = {IO_In, IO_Out};
constexpr int IO_Count = 2;

string to_string(IO io, bool shorten = false);

enum Dir_ {
    Dir_None = -1,
    Dir_Left = 0,
    Dir_Right = 1,
    Dir_Up = 2,
    Dir_Down = 3,
    Dir_COUNT
};

enum FaustGraphHoverFlags_ {
    FaustGraphHoverFlags_None = 0,
    FaustGraphHoverFlags_ShowRect = 1 << 0,
    FaustGraphHoverFlags_ShowType = 1 << 1,
    FaustGraphHoverFlags_ShowChannels = 1 << 2,
    FaustGraphHoverFlags_ShowChildChannels = 1 << 3,
};
using FaustGraphHoverFlags = int;

// Graph

enum FlowGridGraphCol_ {
    FlowGridGraphCol_Bg, // ImGuiCol_WindowBg
    FlowGridGraphCol_Text, // ImGuiCol_Text
    FlowGridGraphCol_DecorateStroke, // ImGuiCol_Border
    FlowGridGraphCol_GroupStroke, // ImGuiCol_Border
    FlowGridGraphCol_Line, // ImGuiCol_PlotLines
    FlowGridGraphCol_Link, // ImGuiCol_Button
    FlowGridGraphCol_Inverter, // ImGuiCol_Text
    FlowGridGraphCol_OrientationMark, // ImGuiCol_Text
    // Box fill colors of various types. todo design these colors for Dark/Classic/Light profiles
    FlowGridGraphCol_Normal,
    FlowGridGraphCol_Ui,
    FlowGridGraphCol_Slot,
    FlowGridGraphCol_Number,

    FlowGridGraphCol_COUNT
};
using FlowGridGraphCol = int;

// Params

enum ParamsWidthSizingPolicy_ {
    ParamsWidthSizingPolicy_StretchToFill, // If a table contains only fixed-width items, allow columns to stretch to fill available width.
    ParamsWidthSizingPolicy_StretchFlexibleOnly, // If a table contains only fixed-width items, it won't stretch to fill available width.
    ParamsWidthSizingPolicy_Balanced, // All param types are given flexible-width, weighted by their minimum width. (Looks more balanced, but less expansion room for wide items).
};
using ParamsWidthSizingPolicy = int;

struct Audio : TabsWindow {
    using TabsWindow::TabsWindow;

    struct Faust : UIStateMember {
        using UIStateMember::UIStateMember;

        WindowMember_(
            FaustEditor,
            WindowFlags_MenuBar,

            WindowMember(Metrics);

            Prop_(Metrics, Metrics, "Faust editor metrics");
        );

        WindowMember_(
            FaustGraph,
            Menu({
                Menu("File", {ShowSaveFaustSvgFileDialog{}}),
                Menu("View", {Settings.HoverFlags}),
            }),

            UIMember_(
                Style,

                Prop_(
                    UInt, FoldComplexity,
                    "?Number of boxes within a graph before folding into a sub-graph.\n"
                    "Setting to zero disables folding altogether, for a fully-expanded graph.",
                    3, 0, 20
                );
                Prop_(Bool, ScaleFillHeight, "?Automatically scale to fill the full height of the graph window, keeping the same aspect ratio.");
                Prop(Float, Scale, 1, 0.1, 5);
                Prop(Enum, Direction, {"Left", "Right"}, Dir_Right);
                Prop(Bool, RouteFrame);
                Prop(Bool, SequentialConnectionZigzag); // `false` uses diagonal lines instead of zigzags instead of zigzags
                Prop(Bool, OrientationMark);
                Prop(Float, OrientationMarkRadius, 1.5, 0.5, 3);

                Prop(Bool, DecorateRootNode, true);
                Prop(Vec2Linked, DecorateMargin, {10, 10}, 0, 20);
                Prop(Vec2Linked, DecoratePadding, {10, 10}, 0, 20);
                Prop(Float, DecorateLineWidth, 1, 1, 4);
                Prop(Float, DecorateCornerRadius, 0, 0, 10);

                Prop(Vec2Linked, GroupMargin, {8, 8}, 0, 20);
                Prop(Vec2Linked, GroupPadding, {8, 8}, 0, 20);
                Prop(Float, GroupLineWidth, 2, 1, 4);
                Prop(Float, GroupCornerRadius, 5, 0, 10);

                Prop(Vec2Linked, NodeMargin, {8, 8}, 0, 20);
                Prop(Vec2Linked, NodePadding, {8, 0}, 0, 20, false); // todo padding y not actually used yet, since blocks already have a min-height determined by WireGap.

                Prop(Float, BoxCornerRadius, 4, 0, 10);
                Prop(Float, BinaryHorizontalGapRatio, 0.25, 0, 1);
                Prop(Float, WireWidth, 1, 0.5, 4);
                Prop(Float, WireGap, 16, 10, 20);
                Prop(Vec2, ArrowSize, {3, 2}, 1, 10);
                Prop(Float, InverterRadius, 3, 1, 5);
                Prop(Colors, Colors, FlowGridGraphCol_COUNT, GetColorName);

                const vector<std::reference_wrapper<PrimitiveBase>> LayoutFields{
                    SequentialConnectionZigzag,
                    OrientationMark,
                    OrientationMarkRadius,
                    DecorateRootNode,
                    DecorateMargin.X,
                    DecorateMargin.Y,
                    DecoratePadding.X,
                    DecoratePadding.Y,
                    DecorateLineWidth,
                    DecorateCornerRadius,
                    GroupMargin.X,
                    GroupMargin.Y,
                    GroupPadding.X,
                    GroupPadding.Y,
                    GroupLineWidth,
                    GroupCornerRadius,
                    BoxCornerRadius,
                    BinaryHorizontalGapRatio,
                    WireWidth,
                    WireGap,
                    NodeMargin.X,
                    NodeMargin.Y,
                    NodePadding.X,
                    NodePadding.Y,
                    ArrowSize.X,
                    ArrowSize.Y,
                    InverterRadius,
                };

                void ColorsDark(TransientStore &store) const;
                void ColorsClassic(TransientStore &store) const;
                void ColorsLight(TransientStore &store) const;
                void ColorsFaust(TransientStore &store) const; // Color Faust graphs the same way Faust does when it renders to SVG.

                void LayoutFlowGrid(TransientStore &store) const;
                void LayoutFaust(TransientStore &store) const; // Layout Faust graphs the same way Faust does when it renders to SVG.

                static const char *GetColorName(FlowGridGraphCol idx);
            );

            Member(
                GraphSettings,
                Prop_(
                    Flags, HoverFlags,
                    "?Hovering over a node in the graph will display the selected information",
                    {"ShowRect?Display the hovered node's bounding rectangle",
                     "ShowType?Display the hovered node's box type",
                     "ShowChannels?Display the hovered node's channel points and indices",
                     "ShowChildChannels?Display the channel points and indices for each of the hovered node's children"},
                    FaustGraphHoverFlags_None
                )
            );

            Prop(GraphSettings, Settings);
            Prop(Style, Style);
        );

        WindowMember(FaustParams);
        WindowMember(
            FaustLog,
            Prop(String, Error);
        );

        Prop_(FaustEditor, Editor, "Faust editor");
        Prop_(FaustGraph, Graph, "Faust graph");
        Prop_(FaustParams, Params, "Faust params");
        Prop_(FaustLog, Log, "Faust log");

        Prop(String, Code, R"#(import("stdfaust.lib");
    pitchshifter = vgroup("Pitch Shifter", ef.transpose(
       vslider("window (samples)", 1000, 50, 10000, 1),
       vslider("xfade (samples)", 10, 1, 10000, 1),
       vslider("shift (semitones)", 0, -24, +24, 0.1)
     )
    );
    process = _ : pitchshifter;)#");
        //    Prop(String, Code, R"#(import("stdfaust.lib");
        // s = vslider("Signal[style:radio{'Noise':0;'Sawtooth':1}]",0,0,1,1);
        // process = select2(s,no.noise,os.sawtooth(440));)#");
        //    Prop(String, Code, R"(import("stdfaust.lib");
        // process = ba.beat(240) : pm.djembe(60, 0.3, 0.4, 1) <: dm.freeverb_demo;)");
        //        Prop(String, Code, R"(import("stdfaust.lib");
        // process = _:fi.highpass(2,1000):_;)");
        //        Prop(String, Code, R"(import("stdfaust.lib");
        // ctFreq = hslider("cutoffFrequency",500,50,10000,0.01);
        // q = hslider("q",5,1,30,0.1);
        // gain = hslider("gain",1,0,1,0.01);
        // process = no:noise : fi.resonlp(ctFreq,q,gain);");

        // Based on Faust::UITester.dsp
        //     Prop(String, Code, R"#(import("stdfaust.lib");
        // declare name "UI Tester";
        // declare version "1.0";
        // declare author "O. Guillerminet";
        // declare license "BSD";
        // declare copyright "(c) O. Guillerminet 2012";

        // vbox = vgroup("vbox",
        //     checkbox("check1"),
        //     checkbox("check2"),
        //     nentry("knob0[style:knob]", 60, 0, 127, 0.1)
        // );

        // sliders = hgroup("sliders",
        //     vslider("vslider1", 60, 0, 127, 0.1),
        //     vslider("vslider2", 60, 0, 127, 0.1),
        //     vslider("vslider3", 60, 0, 127, 0.1)
        // );

        // knobs = hgroup("knobs",
        //     vslider("knob1[style:knob]", 60, 0, 127, 0.1),
        //     vslider("knob2[style:knob]", 60, 0, 127, 0.1),
        //     vslider("knob3[style:knob]", 60, 0, 127, 0.1)
        // );

        // smallhbox1 = hgroup("small box 1",
        //     vslider("vslider5 [unit:Hz]", 60, 0, 127, 0.1),
        //     vslider("vslider6 [unit:Hz]", 60, 0, 127, 0.1),
        //     vslider("knob4[style:knob]", 60, 0, 127, 0.1),
        //     nentry("num1 [unit:f]", 60, 0, 127, 0.1),
        //     vbargraph("vbar1", 0, 127)
        // );

        // smallhbox2 = hgroup("small box 2",
        //     vslider("vslider7 [unit:Hz]", 60, 0, 127, 0.1),
        //     vslider("vslider8 [unit:Hz]", 60, 0, 127, 0.1),
        //     vslider("knob5[style:knob]", 60, 0, 127, 0.1),
        //     nentry("num2 [unit:f]", 60, 0, 127, 0.1),
        //     vbargraph("vbar2", 0, 127)
        // );

        // smallhbox3 = hgroup("small box 3",
        //     vslider("vslider9 [unit:Hz]", 60, 0, 127, 0.1),
        //     vslider("vslider10 [unit:m]", 60, 0, 127, 0.1),
        //     vslider("knob6[style:knob]", 60, 0, 127, 0.1),
        //     nentry("num3 [unit:f]", 60, 0, 127, 0.1),
        //     vbargraph("vbar3", 0, 127)
        // );

        // subhbox1 = hgroup("sub box 1",
        //     smallhbox2,
        //     smallhbox3
        // );

        // vmisc = vgroup("vmisc",
        //     vslider("vslider4 [unit:Hz]", 60, 0, 127, 0.1),
        //     button("button"),
        //     hslider("hslider [unit:Hz]", 60, 0, 127, 0.1),
        //     smallhbox1,
        //     subhbox1,
        //     hbargraph("hbar", 0, 127)
        // );

        // hmisc = hgroup("hmisc",
        //     vslider("vslider4 [unit:f]", 60, 0, 127, 0.1),
        //     button("button"),
        //     hslider("hslider", 60, 0, 127, 0.1),
        //     nentry("num [unit:f]", 60, 0, 127, 0.1),
        //     (63.5 : vbargraph("vbar", 0, 127)),
        //     (42.42 : hbargraph("hbar", 0, 127))
        // );

        // //------------------------- Process --------------------------------

        // process = tgroup("grp 1",
        //     vbox,
        //     sliders,
        //     knobs,
        //     vmisc,
        //     hmisc);)#");

    protected:
        void Render() const override;
    };

    // Corresponds to `ma_device`.
    struct Device : UIStateMember {
        using UIStateMember::UIStateMember;

        static const vector<U32> PrioritizedSampleRates;
        static const string GetFormatName(int); // `ma_format` argmument is converted to an `int`.
        static const string GetSampleRateName(U32);

        void Init() const;
        void Update() const; // Update device based on current settings.
        void Uninit() const;

        void Start() const;
        void Stop() const;
        bool IsStarted() const;

        Prop_(Bool, On, "?When the audio device is turned off, the audio graph is destroyed and no audio processing takes place.", true);
        Prop_(Bool, Muted, "?Completely mute audio output device. All audio computation will still be performed, so this setting does not affect CPU load.", true);
        Prop(Float, Volume, 1.0); // Master volume. Corresponds to `ma_device_set_master_volume`.
        Prop(String, InDeviceName);
        Prop(String, OutDeviceName);
        Prop_(Enum, InFormat, "?An asterisk (*) indicates the format is natively supported by the audio device. All non-native formats require conversion.", GetFormatName);
        Prop_(Enum, OutFormat, "?An asterisk (*) indicates the format is natively supported by the audio device. All non-native formats require conversion.", GetFormatName);
        Prop_(UInt, SampleRate, "?An asterisk (*) indicates the sample rate is natively supported by the audio device. All non-native sample rates require resampling.", GetSampleRateName);

    protected:
        void Render() const override;
    };

    // Corresponds to `ma_node_graph`.
    struct Graph : UIStateMember {
        using UIStateMember::UIStateMember;

        // Corresponds to `ma_node_base`.
        // MA tracks nodes with an `ma_node *` type, where `ma_node` is an alias to `void`.
        // This base `Node` can either be specialized or instantiated on its own.
        struct Node : UIStateMember {
            Node(StateMember *parent, string_view path_segment, string_view name_help = "", bool on = true);

            void Set(void *) const; // Set MA node.
            void *Get() const; // Get MA node.

            Count InputBusCount() const;
            Count OutputBusCount() const;
            Count InputChannelCount(Count bus) const;
            Count OutputChannelCount(Count bus) const;

            bool IsSource() const { return OutputBusCount() > 0; }
            bool IsDestination() const { return InputBusCount() > 0; }

            void Init() const; // Add MA node.
            void Update() const; // Update MA node based on current settings (e.g. volume).
            void Uninit() const; // Remove MA node.

            Prop_(Bool, On, "?When a node is off, it is completely removed from the audio graph.", true);
            Prop(Float, Volume, 1.0);

        protected:
            void Render() const override;
            virtual void DoInit() const;
            virtual void DoUninit() const;
            virtual bool NeedsRestart() const { return false; }; // Return `true` if node needs re-initialization due to changed state.

        private:
            inline static std::unordered_map<ID, void *> DataFor; // MA node for owning Node's ID.
        };

        struct InputNode : Node {
            using Node::Node;
            void DoInit() const override;
            void DoUninit() const override;
        };

        struct FaustNode : Node {
            using Node::Node;
            void DoInit() const override;
            bool NeedsRestart() const override;
        };

        struct Nodes : UIStateMember {
            using UIStateMember::UIStateMember;

            // Iterate over all children, converting each element from a `StateMember *` to a `Node *`.
            // Usage: `for (const Node *node : Nodes) ...`
            struct Iterator : vector<StateMember *>::const_iterator {
                Iterator(auto it) : vector<StateMember *>::const_iterator(it) {}
                const Node *operator*() const { return dynamic_cast<const Node *>(vector<StateMember *>::const_iterator::operator*()); }
            };
            Iterator begin() const { return Children.cbegin(); }
            Iterator end() const { return Children.cend(); }

            void Init() const;
            void Update() const;
            void Uninit() const;

            // `ma_data_source_node` whose `ma_data_source` is a `ma_audio_buffer_ref` pointing directly to the input buffer.
            // todo configurable data source
            Prop(InputNode, Input);
            Prop(FaustNode, Faust);
            Prop(Node, Output);

        protected:
            void Render() const override;
        };

        void Init() const;
        void Update() const;
        void Uninit() const;

        Prop(Nodes, Nodes);
        Prop(Matrix<bool>, Connections);

    protected:
        void Render() const override;
        void RenderConnections() const;
    };

    void Update() const;
    bool NeedsRestart() const;

    Prop(Device, Device);
    Prop(Graph, Graph);
    Prop(Faust, Faust);

protected:
    void Render() const override;
    void Init() const;
    void Uninit() const;
};

extern const Audio &audio;
