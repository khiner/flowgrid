#pragma once

#include "Core/Action/DefineAction.h"
#include "LineChar.h"

Json(LineChar, L, C);
Json(LineCharRange, Start, End);

DefineActionType(
    TextBuffer,
    DefineUnsavedComponentAction(ShowOpenDialog, Merge, "~Open");
    DefineUnsavedComponentAction(ShowSaveDialog, Merge, "~Save as...");
    DefineComponentAction(Open, "", fs::path file_path;);
    DefineUnsavedComponentAction(Save, NoMerge, "", fs::path file_path;);

    DefineUnsavedComponentAction(Undo, NoMerge, "");
    DefineUnsavedComponentAction(Redo, NoMerge, "");

    DefineUnmergableComponentAction(SetCursor, LineChar lc; bool add;);
    DefineUnmergableComponentAction(SetCursorRange, LineCharRange lcr; bool add;);
    DefineUnmergableComponentAction(MoveCursorsLines, int amount; bool select;);
    DefineUnmergableComponentAction(PageCursorsLines, bool up; bool select;);
    DefineUnmergableComponentAction(MoveCursorsChar, bool right; bool select; bool word;);
    DefineUnmergableComponentAction(MoveCursorsTop, bool select;);
    DefineUnmergableComponentAction(MoveCursorsBottom, bool select;);
    DefineUnmergableComponentAction(MoveCursorsStartLine, bool select;);
    DefineUnmergableComponentAction(MoveCursorsEndLine, bool select;);
    DefineUnmergableComponentAction(SelectAll);
    DefineUnmergableComponentAction(SelectNextOccurrence);

    DefineComponentAction(Set, "", std::string value;);
    DefineComponentAction(ToggleOverwrite, "");

    DefineUnsavedComponentAction(Copy, NoMerge, "");
    DefineUnmergableComponentAction(Cut);
    DefineUnmergableComponentAction(Paste);
    DefineUnmergableComponentAction(Delete, bool word;);
    DefineUnmergableComponentAction(Backspace, bool word;);
    DefineUnmergableComponentAction(DeleteCurrentLines);
    DefineUnmergableComponentAction(ChangeCurrentLinesIndentation, bool increase;);
    DefineUnmergableComponentAction(MoveCurrentLines, bool up;);
    DefineUnmergableComponentAction(ToggleLineComment);
    DefineUnmergableComponentAction(EnterChar, unsigned short value;); // Corresponds to `ImWchar`

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

    ComponentActionJson(Set, value);
    ComponentActionJson(ToggleOverwrite);

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
        ShowOpenDialog, ShowSaveDialog, Save, Open, Set, Undo, Redo,
        SetCursor, SetCursorRange, MoveCursorsLines, PageCursorsLines, MoveCursorsChar, MoveCursorsTop, MoveCursorsBottom, MoveCursorsStartLine, MoveCursorsEndLine,
        SelectAll, SelectNextOccurrence, ToggleOverwrite, Copy, Cut, Paste, Delete, Backspace, DeleteCurrentLines, ChangeCurrentLinesIndentation,
        MoveCurrentLines, ToggleLineComment, EnterChar>;
);
