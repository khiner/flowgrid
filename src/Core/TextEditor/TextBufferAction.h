#pragma once

#include "Core/Action/DefineAction.h"
#include "LineChar.h"

Json(LineChar, L, C);
Json(LineCharRange, Start, End);

DefineActionType(
    TextBuffer,
    DefineComponentAction(ShowOpenDialog, Unsaved, Merge, "~Open");
    DefineComponentAction(ShowSaveDialog, Unsaved, Merge, "~Save as...");
    DefineComponentAction(Open, Saved, SameIdMerge, "", fs::path file_path;);
    DefineComponentAction(Save, Unsaved, NoMerge, "", fs::path file_path;);

    DefineComponentAction(SetCursor, Unsaved, Merge, "", LineChar lc; bool add;);
    DefineComponentAction(SetCursorRange, Unsaved, Merge, "", LineCharRange lcr; bool add;);
    DefineComponentAction(MoveCursorsLines, Unsaved, Merge, "", int amount; bool select;);
    DefineComponentAction(PageCursorsLines, Unsaved, Merge, "", bool up; bool select;);
    DefineComponentAction(MoveCursorsChar, Unsaved, Merge, "", bool right; bool select; bool word;);
    DefineComponentAction(MoveCursorsTop, Unsaved, Merge, "", bool select;);
    DefineComponentAction(MoveCursorsBottom, Unsaved, Merge, "", bool select;);
    DefineComponentAction(MoveCursorsStartLine, Unsaved, Merge, "", bool select;);
    DefineComponentAction(MoveCursorsEndLine, Unsaved, Merge, "", bool select;);
    DefineComponentAction(SelectAll, Unsaved, Merge, "");
    DefineComponentAction(SelectNextOccurrence, Unsaved, Merge, "");

    DefineComponentAction(SetText, Saved, SameIdMerge, "", std::string value;);

    DefineComponentAction(Copy, Unsaved, NoMerge, "");
    DefineComponentAction(Cut, Saved, NoMerge, "");
    DefineComponentAction(Paste, Saved, NoMerge, "");
    DefineComponentAction(Delete, Saved, NoMerge, "", bool word;);
    DefineComponentAction(Backspace, Saved, NoMerge, "", bool word;);
    DefineComponentAction(DeleteCurrentLines, Saved, NoMerge, "");
    DefineComponentAction(ChangeCurrentLinesIndentation, Saved, NoMerge, "", bool increase;);
    DefineComponentAction(MoveCurrentLines, Saved, NoMerge, "", bool up;);
    DefineComponentAction(ToggleLineComment, Saved, NoMerge, "");
    DefineComponentAction(EnterChar, Saved, NoMerge, "", unsigned short value;); // Corresponds to `ImWchar`

    ComponentActionJson(Open, file_path);
    ComponentActionJson(Save, file_path);

    ComponentActionJson(SetCursor, lc, add);
    ComponentActionJson(SetCursorRange, lcr, add);
    ComponentActionJson(MoveCursorsLines, amount, select);
    ComponentActionJson(PageCursorsLines, up, select);
    ComponentActionJson(MoveCursorsChar, right, select, word);
    ComponentActionJson(MoveCursorsTop, select);
    ComponentActionJson(MoveCursorsBottom, select);
    ComponentActionJson(MoveCursorsStartLine, select);
    ComponentActionJson(MoveCursorsEndLine, select);
    ComponentActionJson(SelectAll);
    ComponentActionJson(SelectNextOccurrence);

    ComponentActionJson(SetText, value);

    ComponentActionJson(Copy);
    ComponentActionJson(Cut);
    ComponentActionJson(Paste);
    ComponentActionJson(Delete, word);
    ComponentActionJson(Backspace, word);
    ComponentActionJson(DeleteCurrentLines);
    ComponentActionJson(ChangeCurrentLinesIndentation, increase);
    ComponentActionJson(MoveCurrentLines, up);
    ComponentActionJson(ToggleLineComment);
    ComponentActionJson(EnterChar, value);

    using Any = ActionVariant<
        ShowOpenDialog, ShowSaveDialog, Save, Open, SetText,
        SetCursor, SetCursorRange, MoveCursorsLines, PageCursorsLines, MoveCursorsChar, MoveCursorsTop, MoveCursorsBottom, MoveCursorsStartLine, MoveCursorsEndLine,
        SelectAll, SelectNextOccurrence, Copy, Cut, Paste, Delete, Backspace, DeleteCurrentLines, ChangeCurrentLinesIndentation,
        MoveCurrentLines, ToggleLineComment, EnterChar>;
);
