#pragma once

#include <memory>

#include "Core/Action/ActionMenuItem.h"
#include "Core/Action/Actionable.h"
#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Enum.h"
#include "Core/Primitive/Float.h"
#include "Core/String.h"

#include "TextBufferAction.h"
#include "TextBufferData.h"
#include "TextBufferPalette.h"

struct TextBufferState;

struct TextBuffer : Component, Actionable<Action::TextBuffer::Any> {
    using Line = TextBufferLine;
    using Lines = TextBufferLines;
    using Cursor = LineCharRange;

    TextBuffer(ComponentArgs &&, const fs::path &);
    ~TextBuffer();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    TextBufferData GetBuffer() const;
    std::string GetText() const;
    bool Empty() const;

    void Refresh() override;
    void Render() const override;
    void RenderMenu() const;
    void RenderDebug() const override;

    std::optional<ActionType> ProduceKeyboardAction() const;

    fs::path _LastOpenedFilePath;
    Prop(String, LastOpenedFilePath, _LastOpenedFilePath.c_str());
    Prop_(DebugComponent, Debug, "Editor debug");

    Prop(Bool, ReadOnly, false);
    Prop(Bool, Overwrite, false);
    Prop(Bool, AutoIndent, true);
    Prop(Bool, ShowWhitespaces, true);
    Prop(Bool, ShowLineNumbers, true);
    Prop(Bool, ShowStyleTransitionPoints, false);
    Prop(Bool, ShowChangedCaptureRanges, false);
    Prop(Bool, ShortTabs, true);
    Prop(Float, LineSpacing, 1);
    Prop(Enum, PaletteId, {"Dark", "Light", "Mariana", "RetroBlue"}, int(TextBufferPaletteId::Dark));

private:
    void Commit(TextBufferData) const;

    std::optional<TextBuffer::ActionType> Render(const TextBufferData &, bool is_focused) const;
    std::optional<TextBuffer::ActionType> HandleMouseInputs(const TextBufferData &, ImVec2 char_advance, float text_start_x) const;

    // Returns the range of all edited cursor starts/ends since cursor edits were last cleared.
    // Used for updating the scroll range.
    std::optional<Cursor> GetEditedCursor(TextBufferCursors) const;

    u32 GetColor(PaletteIndex) const;
    void SetFilePath(const fs::path &) const;

    void CreateHoveredNode(u32 byte_index) const;
    void DestroyHoveredNode() const;

    std::unique_ptr<TextBufferState> State; // PIMPL for rendering state.

    ActionProducer<ActionType>::EnqueueFn Q;

    ActionMenuItem<ActionType>
        ShowOpenDialogMenuItem{*this, Q, Action::TextBuffer::ShowOpenDialog{Id}},
        ShowSaveDialogMenuItem{*this, Q, Action::TextBuffer::ShowSaveDialog{Id}};

    const Menu FileMenu{
        "File",
        {
            ShowOpenDialogMenuItem,
            ShowSaveDialogMenuItem,
        }
    };
};
