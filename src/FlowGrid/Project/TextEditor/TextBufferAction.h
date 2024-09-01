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

    DefineUnsavedComponentAction(SetCursor, Merge, "", LineChar lc; bool add;);
    DefineUnsavedComponentAction(SetCursorRange, Merge, "", LineCharRange lcr; bool add;);
    DefineUnsavedComponentAction(MoveCursorsLines, Merge, "", int amount; bool select;);
    DefineUnsavedComponentAction(PageCursorsLines, Merge, "", bool up; bool select;);
    DefineUnsavedComponentAction(MoveCursorsChar, Merge, "", bool right; bool select; bool word;);
    DefineUnsavedComponentAction(MoveCursorsTop, Merge, "", bool select;);
    DefineUnsavedComponentAction(MoveCursorsBottom, Merge, "", bool select;);
    DefineUnsavedComponentAction(MoveCursorsStartLine, Merge, "", bool select;);
    DefineUnsavedComponentAction(MoveCursorsEndLine, Merge, "", bool select;);
    DefineUnsavedComponentAction(SelectAll, Merge, "");
    DefineUnsavedComponentAction(SelectNextOccurrence, Merge, "");

    DefineComponentAction(Set, "", std::string value;);

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
        ShowOpenDialog, ShowSaveDialog, Save, Open, Set,
        SetCursor, SetCursorRange, MoveCursorsLines, PageCursorsLines, MoveCursorsChar, MoveCursorsTop, MoveCursorsBottom, MoveCursorsStartLine, MoveCursorsEndLine,
        SelectAll, SelectNextOccurrence, Copy, Cut, Paste, Delete, Backspace, DeleteCurrentLines, ChangeCurrentLinesIndentation,
        MoveCurrentLines, ToggleLineComment, EnterChar>;
);
