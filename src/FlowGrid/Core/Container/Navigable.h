#pragma once

#include <algorithm>
#include <optional>
#include <vector>

using u32 = unsigned int;

template<typename T> struct Navigable {
    inline auto begin() { return Value.begin(); }
    inline auto end() { return Value.end(); }

    inline u32 GetCursor() const { return Cursor; }

    inline bool Empty() const { return Value.empty(); }
    inline bool CanMoveTo(u32 index) const { return index < Value.size(); }
    inline bool CanStepBackward() const { return Cursor > 0; }
    inline bool CanStepForward() const { return Cursor < Value.size() - 1; }

    inline auto Back() const { return Value.back(); }

    // Perfect-forwarding.
    template<typename U> void Push(U &&entry) {
        while (Value.size() > Cursor + 1) Value.pop_back();
        Value.push_back(std::forward<U>(entry));
        MoveTo(Value.size() - 1);
    }
    inline void Pop() {
        if (Cursor == Value.size() - 1) --Cursor;
        Value.pop_back();
    }

    inline void StepForward() { Move(1); }
    inline void StepBackward() { Move(-1); }

    inline void MoveTo(u32 index) { Cursor = std::clamp(int(index), 0, int(Value.size()) - 1); }
    inline void Move(int offset) { Cursor = std::clamp(int(Cursor) + offset, 0, int(Value.size()) - 1); }

    inline auto operator[](u32 index) { return Value[index]; }

    // Dereference operators return a reference to the current element.
    T &operator*() { return Value[Cursor]; }
    const T &operator*() const { return Value[Cursor]; }

    // Checked access - returns an optional const reference to the current element, or std::nullopt if the container is empty.
    inline std::optional<std::reference_wrapper<const T>> TryCurrent() {
        if (Value.empty()) return std::nullopt;
        return std::ref(Value[Cursor]);
    }

private:
    std::vector<T> Value;
    u32 Cursor = 0;
};
