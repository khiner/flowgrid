
#include "Primitive/Bool.h"
#include "Primitive/Enum.h"
#include "Primitive/Flags.h"
#include "Primitive/Float.h"
#include "Primitive/Int.h"
#include "Primitive/String.h"
#include "Primitive/UInt.h"

#include "Container/AdjacencyList.h"
#include "Container/Colors.h"
#include "Container/Navigable.h"
#include "Container/Set.h"
#include "Container/Vec2.h"
#include "Container/Vector.h"

#include "CoreActionHandler.h"
#include "CoreActionProducer.h"
#include "Helper/Hex.h"
#include "Project/ProjectContext.h"
#include "Store/Store.h"
#include "TextEditor/TextBuffer.h"
#include "UI/HelpMarker.h"
#include "UI/InvisibleButton.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot_internal.h"

#include "immer/flex_vector_transient.hpp"
#include "immer/set_transient.hpp"

using std::ranges::find, std::ranges::count_if, std::ranges::to;

/*** Primitives ***/

template<typename T> Primitive<T>::Primitive(ComponentArgs &&args, T value) : Component(std::move(args)), Value(std::move(value)) {
    if (S.Count<T>(Id)) Refresh();
    else _S.Set(Id, Value); // We treat the provided value as a default store value.
}
template<typename T> Primitive<T>::~Primitive() {
    _S.Erase<T>(Id);
}

template<typename T> void Primitive<T>::Refresh() { Value = S.Get<T>(Id); }

template<typename T> json Primitive<T>::ToJson() const { return Value; }
template<typename T> void Primitive<T>::SetJson(json &&j) const { _S.Set(Id, T{std::move(j)}); }

template<typename T> void Primitive<T>::Set(T value) const { _S.Set(Id, std::move(value)); }
template<typename T> void Primitive<T>::Set_(T value) {
    _S.Set(Id, value);
    Value = value;
}

template<typename T> void Primitive<T>::Erase() const { _S.Erase<T>(Id); }

template<typename T> void Primitive<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();
    TreeNode(Name, false, std::format("{}", Value).c_str());
}

template<> void Primitive<u32>::IssueSet(const u32 &value) const { Ctx.CoreQ(Action::Primitive::UInt::Set{Id, value}); };
template<> void Primitive<s32>::IssueSet(const s32 &value) const { Ctx.CoreQ(Action::Primitive::Int::Set{Id, value}); };
template<> void Primitive<float>::IssueSet(const float &value) const { Ctx.CoreQ(Action::Primitive::Float::Set{Id, value}); };
template<> void Primitive<std::string>::IssueSet(const std::string &value) const { Ctx.CoreQ(Action::Primitive::String::Set{Id, value}); };

// Explicit instantiations.
template struct Primitive<bool>;
template struct Primitive<int>;
template struct Primitive<u32>;
template struct Primitive<float>;
template struct Primitive<std::string>;

using namespace ImGui;
void Bool::Toggle_() {
    Set_(!_S.Get<bool>(Id));
    Refresh();
}

void Bool::IssueToggle() const { Ctx.CoreQ(Action::Primitive::Bool::Toggle{Id}); }

void Bool::Render(std::string_view label) const {
    if (bool value = Value; Checkbox(label.data(), &value)) IssueToggle();
    HelpMarker();
}

void Bool::Render() const {
    Render(ImGuiLabel);
}

bool Bool::CheckedDraw() const {
    bool value = Value;
    bool toggled = Checkbox(ImGuiLabel.c_str(), &value);
    if (toggled) IssueToggle();
    HelpMarker();
    return toggled;
}

void Bool::MenuItem() const {
    HelpMarker(false);
    if (const bool value = Value; ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, value)) IssueToggle();
}

Enum::Enum(ComponentArgs &&args, std::vector<std::string> names, int value)
    : Primitive(std::move(args), value), Names(std::move(names)) {}
Enum::Enum(ComponentArgs &&args, std::function<std::string(int)> get_name, int value)
    : Primitive(std::move(args), value), Names({}), GetName(std::move(get_name)) {}

std::string Enum::OptionName(const int option) const { return GetName ? (*GetName)(option) : Names[option]; }

void Enum::Render() const {
    Render(std::views::iota(0, int(Names.size())) | to<std::vector>()); // todo if I stick with this pattern, cache.
}
void Enum::Render(const std::vector<int> &options) const {
    if (options.empty()) return;

    if (const int value = Value; BeginCombo(ImGuiLabel.c_str(), OptionName(value).c_str())) {
        for (int option : options) {
            const bool is_selected = option == value;
            const auto &name = OptionName(option);
            if (Selectable(name.c_str(), is_selected)) IssueSet(option);
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
void Enum::MenuItem() const {
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        const int value = Value;
        for (u32 i = 0; i < Names.size(); i++) {
            const bool is_selected = value == int(i);
            if (ImGui::MenuItem(Names[i].c_str(), nullptr, is_selected)) IssueSet(int(i));
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}

Flags::Flags(ComponentArgs &&args, std::vector<Item> items, int value)
    : Primitive(std::move(args), value), Items(std::move(items)) {}

Flags::Item::Item(const char *name_and_help) {
    const auto meta = HelpInfo::Parse(name_and_help);
    Name = meta.Name;
    Help = meta.Help;
}

void Flags::Render() const {
    if (TreeNodeEx(ImGuiLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        const int value = Value;
        for (u32 i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            bool is_selected = option_mask & value;
            if (Checkbox(item.Name.c_str(), &is_selected)) IssueSet(value ^ option_mask); // Toggle bit
            if (!item.Help.empty()) {
                SameLine();
                flowgrid::HelpMarker(item.Help.c_str());
            }
        }
        TreePop();
    }
    HelpMarker();
}
void Flags::MenuItem() const {
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        const int value = Value;
        for (u32 i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            const bool is_selected = option_mask & value;
            if (!item.Help.empty()) {
                flowgrid::HelpMarker(item.Help);
                SameLine();
            }
            if (ImGui::MenuItem(item.Name.c_str(), nullptr, is_selected)) IssueSet(value ^ option_mask); // Toggle bit
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}

Float::Float(ComponentArgs &&args, float value, float min, float max, const char *fmt, ImGuiSliderFlags flags, float drag_speed)
    : Primitive(std::move(args), value), Min(min), Max(max), DragSpeed(drag_speed), Format(fmt), Flags(flags) {}

void Float::Render() const {
    float value = Value;
    const bool edited = DragSpeed > 0 ?
        DragFloat(ImGuiLabel.c_str(), &value, DragSpeed, Min, Max, Format, Flags) :
        SliderFloat(ImGuiLabel.c_str(), &value, Min, Max, Format, Flags);

    UpdateGesturing();
    if (edited) IssueSet(value);
    HelpMarker();
}

ID GenerateId(ID parent_id, ID child_id) { return ImHashData(&child_id, sizeof(child_id), parent_id); }
ID GenerateId(ID parent_id, std::string_view child_id) { return ImHashStr(child_id.data(), 0, parent_id); }

Int::Int(ComponentArgs &&args, int value, int min, int max)
    : Primitive(std::move(args), value), Min(min), Max(max) {}

void Int::Render() const {
    int value = Value;
    const bool edited = SliderInt(ImGuiLabel.c_str(), &value, Min, Max, "%d", ImGuiSliderFlags_None);
    UpdateGesturing();
    if (edited) IssueSet(value);
    HelpMarker();
}
void Int::Render(const std::vector<int> &options) const {
    if (options.empty()) return;

    if (const int value = Value; BeginCombo(ImGuiLabel.c_str(), std::to_string(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(std::to_string(option).c_str(), is_selected)) IssueSet(option);
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

UInt::UInt(ComponentArgs &&args, u32 value, u32 min, u32 max)
    : Primitive(std::move(args), value), Min(min), Max(max) {}
UInt::UInt(ComponentArgs &&args, std::function<const std::string(u32)> get_name, u32 value)
    : Primitive(std::move(args), value), Min(0), Max(100), GetName(std::move(get_name)) {}

UInt::operator ImColor() const { return Value; }

std::string UInt::ValueName(u32 value) const { return GetName ? (*GetName)(value) : std::to_string(value); }

void UInt::Render() const {
    u32 value = Value;
    const bool edited = SliderScalar(ImGuiLabel.c_str(), ImGuiDataType_S32, &value, &Min, &Max, "%d");
    UpdateGesturing();
    if (edited) IssueSet(value);
    HelpMarker();
}
void UInt::Render(const std::vector<u32> &options) const {
    if (options.empty()) return;

    if (const u32 value = Value; BeginCombo(ImGuiLabel.c_str(), ValueName(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(ValueName(option).c_str(), is_selected)) IssueSet(option);
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

String::String(ComponentArgs &&args, std::string_view value) : Primitive(std::move(args), std::string(value)) {}
String::String(ComponentArgs &&args, fs::path value) : Primitive(std::move(args), std::string(value)) {}

void String::Render() const { TextUnformatted(Value); }

void String::Render(const std::vector<std::string> &options) const {
    if (options.empty()) return;

    if (const std::string value = *this; BeginCombo(ImGuiLabel.c_str(), value.c_str())) {
        for (const auto &option : options) {
            const bool is_selected = option == value;
            if (Selectable(option.c_str(), is_selected)) IssueSet(option);
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

/*** Containers ***/

void ApplyVectorSet(Store &s, const auto &a) {
    s.Set(a.component_id, s.Get<immer::flex_vector<decltype(a.value)>>(a.component_id).set(a.i, a.value));
}
void ApplySetInsert(Store &s, const auto &a) {
    s.Set(a.component_id, s.Get<immer::set<decltype(a.value)>>(a.component_id).insert(a.value));
}
void ApplySetErase(Store &s, const auto &a) {
    s.Set(a.component_id, s.Get<immer::set<decltype(a.value)>>(a.component_id).erase(a.value));
}

CoreActionHandler::CoreActionHandler(Store &store) : _S(store) {}
void CoreActionHandler::Apply(const ActionType &action) const {
    std::visit(
        Match{
            /* Primitives */
            [this](const Action::Primitive::Bool::Toggle &a) { _S.Set(a.component_id, !S.Get<bool>(a.component_id)); },
            [this](const Action::Primitive::Int::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::UInt::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::Float::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::Enum::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::Flags::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::String::Set &a) { _S.Set(a.component_id, a.value); },
            [](const Action::TextBuffer::Any &a) {
                const auto *c = Component::ById.at(a.GetComponentId());
                static_cast<const TextBuffer *>(c)->Apply(a);
            },
            /* Containers */
            [this](const Action::Container::Any &a) {
                const auto *c = Component::ById.at(a.GetComponentId());
                std::visit(
                    Match{
                        [c](const Action::AdjacencyList::ToggleConnection &a) {
                            const auto *al = static_cast<const AdjacencyList *>(c);
                            if (al->IsConnected(a.source, a.destination)) al->Disconnect(a.source, a.destination);
                            else al->Connect(a.source, a.destination);
                        },
                        [this, c](const Action::Vec2::Set &a) {
                            const auto *vec2 = static_cast<const Vec2 *>(c);
                            _S.Set(vec2->X.Id, a.value.first);
                            _S.Set(vec2->Y.Id, a.value.second);
                        },
                        [this, c](const Action::Vec2::SetX &a) { _S.Set(static_cast<const Vec2 *>(c)->X.Id, a.value); },
                        [this, c](const Action::Vec2::SetY &a) { _S.Set(static_cast<const Vec2 *>(c)->Y.Id, a.value); },
                        [this, c](const Action::Vec2::SetAll &a) {
                            const auto *vec2 = static_cast<const Vec2 *>(c);
                            _S.Set(vec2->X.Id, a.value);
                            _S.Set(vec2->Y.Id, a.value);
                        },
                        [this, c](const Action::Vec2::ToggleLinked &) {
                            const auto *vec2 = static_cast<const Vec2Linked *>(c);
                            _S.Set(vec2->Linked.Id, !S.Get<bool>(vec2->Linked.Id));
                            const float x = S.Get<float>(vec2->X.Id);
                            const float y = S.Get<float>(vec2->Y.Id);
                            if (x < y) _S.Set(vec2->Y.Id, x);
                            else if (y < x) _S.Set(vec2->X.Id, y);
                        },
                        [this](const Action::Vector<bool>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<int>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<u32>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<float>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<std::string>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Set<u32>::Insert &a) { ApplySetInsert(_S, a); },
                        [this](const Action::Set<u32>::Erase &a) { ApplySetErase(_S, a); },
                        [this, c](const Action::Navigable<u32>::Clear &) {
                            const auto *nav = static_cast<const Navigable<u32> *>(c);
                            _S.Set<immer::flex_vector<u32>>(nav->Value.Id, {});
                            _S.Set(nav->Cursor.Id, 0);
                        },
                        [this, c](const Action::Navigable<u32>::Push &a) {
                            const auto *nav = static_cast<const Navigable<u32> *>(c);
                            const auto vec = S.Get<immer::flex_vector<u32>>(nav->Value.Id).push_back(a.value);
                            _S.Set<immer::flex_vector<u32>>(nav->Value.Id, vec);
                            _S.Set<u32>(nav->Cursor.Id, vec.size() - 1);
                        },

                        [this, c](const Action::Navigable<u32>::MoveTo &a) {
                            const auto *nav = static_cast<const Navigable<u32> *>(c);
                            auto cursor = u32(std::clamp(int(a.index), 0, int(S.Get<immer::flex_vector<u32>>(nav->Value.Id).size()) - 1));
                            _S.Set(nav->Cursor.Id, std::move(cursor));
                        },
                    },
                    a
                );
            },
        },
        action
    );
}

bool CoreActionHandler::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [](const Action::TextBuffer::Any &a) {
                const auto *c = Component::ById.at(a.GetComponentId());
                static_cast<const TextBuffer *>(c)->CanApply(a);
            },
            [](auto &&) { return true; },
        },
        action
    );
}

template<typename T> Vector<T>::ContainerT Vector<T>::Get() const { return S.Get<ContainerT>(Id); }
template<typename T> void Vector<T>::Erase() const { _S.Erase<ContainerT>(Id); }
template<typename T> void Vector<T>::Clear() const { _S.Clear<ContainerT>(Id); }

template<typename T> void Vector<T>::Set(const std::vector<T> &value) const {
    immer::flex_vector_transient<T> val{};
    for (const auto &v : value) val.push_back(v);
    _S.Set(Id, val.persistent());
}
template<typename T> void Vector<T>::Set(size_t i, T value) const { _S.Set(Id, Get().set(i, std::move(value))); }
template<typename T> void Vector<T>::PushBack(T value) const { _S.Set(Id, Get().push_back(std::move(value))); }
template<typename T> void Vector<T>::PopBack() const {
    const auto v = S.Get<ContainerT>(Id);
    _S.Set(Id, v.take(v.size() - 1));
}

template<typename T> void Vector<T>::Resize(size_t size) const {
    auto val = Get().take(size).transient();
    while (val.size() < size) val.push_back(T{});
    _S.Set(Id, val.persistent());
}
template<typename T> void Vector<T>::Erase(size_t i) const { _S.Set(Id, Get().erase(i)); }

template<typename T> size_t Vector<T>::IndexOf(T value) const {
    auto vec = Get();
    return find(vec, std::move(value)) - vec.begin();
}
template<typename T> bool Vector<T>::Contains(T value) const {
    auto vec = Get();
    return find(vec, std::move(value)) != vec.end();
}
template<typename T> void Vector<T>::SetJson(json &&j) const {
    immer::flex_vector_transient<T> val{};
    for (const auto &v : json::parse(std::string(std::move(j)))) val.push_back(v);
    _S.Set(Id, val.persistent());
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json Vector<T>::ToJson() const { return json(Get()).dump(); }

template<typename T> void Vector<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    auto value = Get();
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name));
        return;
    }

    if (TreeNode(Name, false, nullptr, false, auto_select)) {
        for (u32 i = 0; i < value.size(); i++) {
            FlashUpdateRecencyBackground(std::to_string(i));
            TreeNode(std::to_string(i), false, std::format("{}", value[i]).c_str());
        }
        TreePop();
    }
}

// Explicit instantiations.
template struct Vector<bool>;
template struct Vector<s32>;
template struct Vector<u32>;
template struct Vector<float>;
template struct Vector<std::string>;

template<typename T> Set<T>::ContainerT Set<T>::Get() const { return S.Get<ContainerT>(Id); }
template<typename T> void Set<T>::Erase() const { _S.Erase<ContainerT>(Id); }
template<typename T> void Set<T>::Clear() const { _S.Clear<ContainerT>(Id); }
template<typename T> void Set<T>::Insert(const T &value) const { _S.Set(Id, Get().insert(value)); }
template<typename T> void Set<T>::Erase(const T &value) const { _S.Set(Id, Get().erase(value)); }

template<typename T> void Set<T>::SetJson(json &&j) const {
    immer::set_transient<T> val{};
    for (const auto &v : json::parse(std::string(std::move(j)))) val.insert(v);
    _S.Set(Id, val.persistent());
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json Set<T>::ToJson() const { return json(Get()).dump(); }

template<typename T> void Set<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    auto value = Get();
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name));
        return;
    }

    if (TreeNode(Name, false, nullptr, false, auto_select)) {
        for (u32 v : value) {
            FlashUpdateRecencyBackground(std::to_string(v));
            TextUnformatted(std::to_string(v));
        }
        TreePop();
    }
}

// Explicit instantiations.
template struct Set<u32>;

template<typename T> void Navigable<T>::IssueClear() const { Ctx.CoreQ(typename Action::Navigable<T>::Clear{Id}); }
template<typename T> void Navigable<T>::IssuePush(T value) const { Ctx.CoreQ(typename Action::Navigable<T>::Push{Id, std::move(value)}); }
template<typename T> void Navigable<T>::IssueMoveTo(u32 index) const { Ctx.CoreQ(typename Action::Navigable<T>::MoveTo{Id, index}); }
template<typename T> void Navigable<T>::IssueStepForward() const { Ctx.CoreQ(typename Action::Navigable<T>::MoveTo{Id, u32(Cursor) + 1}); }
template<typename T> void Navigable<T>::IssueStepBackward() const { Ctx.CoreQ(typename Action::Navigable<T>::MoveTo{Id, u32(Cursor) - 1}); }

// Explicit instantiations.
template struct Navigable<u32>;

IdPairs AdjacencyList::Get() const { return S.Get<IdPairs>(Id); }

// Non-recursive DFS handling cycles.
bool AdjacencyList::HasPath(ID from_id, ID to_id) const {
    const auto id_pairs = Get();
    std::unordered_set<ID> visited;
    std::stack<ID> to_visit;
    to_visit.push(from_id);
    while (!to_visit.empty()) {
        ID current = to_visit.top();
        to_visit.pop();
        if (current == to_id) return true;

        if (!visited.contains(current)) {
            visited.insert(current);
            for (const auto &[source_id, destination_id] : id_pairs) {
                if (source_id == current) to_visit.push(destination_id);
            }
        }
    }

    return false;
}

bool AdjacencyList::IsConnected(ID source, ID destination) const { return S.Get<IdPairs>(Id).count({source, destination}) > 0; }
void AdjacencyList::Disconnect(ID source, ID destination) const { _S.Set(Id, S.Get<IdPairs>(Id).erase({source, destination})); }
void AdjacencyList::Add(IdPair &&id_pair) const { _S.Set(Id, S.Get<IdPairs>(Id).insert(std::move(id_pair))); }
void AdjacencyList::Connect(ID source, ID destination) const { Add({source, destination}); }

void AdjacencyList::DisconnectOutput(ID id) const {
    for (const auto &[source_id, destination_id] : Get()) {
        if (source_id == id || destination_id == id) Disconnect(source_id, destination_id);
    }
}

u32 AdjacencyList::SourceCount(ID destination) const {
    return count_if(Get(), [destination](const auto &pair) { return pair.second == destination; });
}
u32 AdjacencyList::DestinationCount(ID source) const {
    return count_if(Get(), [source](const auto &pair) { return pair.first == source; });
}

void AdjacencyList::Erase() const { _S.Erase<IdPairs>(Id); }

void AdjacencyList::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    const auto value = Get();
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name));
        return;
    }

    if (TreeNode(Name, false, nullptr, false, auto_select)) {
        u32 i = 0;
        for (const auto &v : value) {
            FlashUpdateRecencyBackground(SerializeIdPair(v));
            const auto &[source_id, destination_id] = v;
            const bool can_annotate = annotate && ById.contains(source_id) && ById.contains(destination_id);
            const std::string label = can_annotate ?
                std::format("{} -> {}", ById.at(source_id)->Name, ById.at(destination_id)->Name) :
                std::format("#{:08X} -> #{:08X}", source_id, destination_id);
            TreeNode(std::to_string(i++), false, label.c_str(), can_annotate);
        }
        TreePop();
    }
}

void AdjacencyList::SetJson(json &&j) const {
    Erase();
    for (IdPair id_pair : json::parse(std::string(std::move(j)))) Add(std::move(id_pair));
}

// Using a string representation to flatten the JSON without worrying about non-object collection values.
json AdjacencyList::ToJson() const { return json(Get()).dump(); }

Vec2::Vec2(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, const char *fmt)
    : Component(std::move(args)), X({this, "X"}, value.first, min, max, fmt), Y({this, "Y"}, value.second, min, max, fmt) {}

Vec2::operator ImVec2() const { return {float(X), float(Y)}; }

void Vec2::Set(std::pair<float, float> value) const {
    _S.Set(X.Id, value.first);
    _S.Set(Y.Id, value.second);
}

using namespace ImGui;

void Vec2::Render(ImGuiSliderFlags flags) const {
    ImVec2 xy = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&xy, X.Min, X.Max, X.Format, flags);
    Component::UpdateGesturing();
    if (edited) Ctx.CoreQ(Action::Vec2::Set{Id, {xy.x, xy.y}});
    HelpMarker();
}

void Vec2::Render() const { Render(ImGuiSliderFlags_None); }

Vec2Linked::Vec2Linked(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, bool linked, const char *fmt)
    : Vec2(std::move(args), std::move(value), min, max, fmt), Linked({this, "Linked"}, linked) {}

Vec2Linked::Vec2Linked(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, const char *fmt)
    : Vec2Linked(std::move(args), std::move(value), min, max, true, fmt) {}

void Vec2Linked::Render(ImGuiSliderFlags flags) const {
    PushID(ImGuiLabel.c_str());
    bool linked = Linked;
    if (Checkbox(Linked.Name.c_str(), &linked)) Ctx.CoreQ(Action::Vec2::ToggleLinked{Id});
    PopID();

    SameLine();

    ImVec2 xy = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&xy, X.Min, X.Max, X.Format, flags);
    Component::UpdateGesturing();
    if (edited) {
        if (Linked) {
            Ctx.CoreQ(Action::Vec2::SetAll{Id, xy.x != X ? xy.x : xy.y});
        } else {
            Ctx.CoreQ(Action::Vec2::Set{Id, {xy.x, xy.y}});
        }
    }
    HelpMarker();
}

void Vec2Linked::Render() const { Render(ImGuiSliderFlags_None); }

Colors::Colors(ComponentArgs &&args, u32 size, std::function<const char *(int)> get_name, const bool allow_auto)
    : Vector(std::move(args)), GetName(get_name), AllowAuto(allow_auto) {
    immer::flex_vector_transient<u32> val{};
    for (auto v : std::views::iota(0u, u32(size))) val.push_back(std::move(v));
    _S.Set(Id, val.persistent());
}

u32 Colors::Float4ToU32(const ImVec4 &value) { return value == IMPLOT_AUTO_COL ? AutoColor : ImGui::ColorConvertFloat4ToU32(value); }
ImVec4 Colors::U32ToFloat4(u32 value) { return value == AutoColor ? IMPLOT_AUTO_COL : ImGui::ColorConvertU32ToFloat4(value); }

void Colors::Set(const std::vector<ImVec4> &values) const {
    immer::flex_vector_transient<u32> val{};
    for (const auto &v : values) val.push_back(Float4ToU32(v));
    _S.Set(Id, val.persistent());
}
void Colors::Set(const std::unordered_map<size_t, ImVec4> &entries) const {
    auto val = Get().transient();
    for (const auto &entry : entries) val.set(entry.first, Float4ToU32(entry.second));
    _S.Set(Id, val.persistent());
}

void Colors::Render() const {
    static ImGuiTextFilter filter;
    filter.Draw("Filter colors", GetFontSize() * 16);

    static ImGuiColorEditFlags flags = 0;
    if (RadioButton("Opaque", flags == ImGuiColorEditFlags_None)) flags = ImGuiColorEditFlags_None;
    SameLine();
    if (RadioButton("Alpha", flags == ImGuiColorEditFlags_AlphaPreview)) flags = ImGuiColorEditFlags_AlphaPreview;
    SameLine();
    if (RadioButton("Both", flags == ImGuiColorEditFlags_AlphaPreviewHalf)) flags = ImGuiColorEditFlags_AlphaPreviewHalf;
    SameLine();
    flowgrid::HelpMarker("In the color list:\n"
                         "Left-click on color square to open color picker.\n"
                         "Right-click to open edit options menu.");

    BeginChild("##colors", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened);
    PushItemWidth(-160);

    for (u32 i = 0; i < Size(); i++) {
        if (const std::string &color_name = GetName(i); filter.PassFilter(color_name.c_str())) {
            u32 color = (*this)[i];
            const bool is_auto = AllowAuto && color == AutoColor;
            const u32 mapped_value = is_auto ? ColorConvertFloat4ToU32(ImPlot::GetAutoColor(int(i))) : color;

            PushID(i);
            flowgrid::InvisibleButton({GetWindowWidth(), GetFontSize()}, ""); // todo try `Begin/EndGroup` after this works for hover info pane (over label)
            SetItemAllowOverlap();

            // todo use auto for FG colors (link to ImGui colors)
            if (AllowAuto) {
                if (!is_auto) PushStyleVar(ImGuiStyleVar_Alpha, 0.25);
                if (Button("Auto")) Ctx.CoreQ(Action::Vector<u32>::Set{Id, i, is_auto ? mapped_value : AutoColor});
                if (!is_auto) PopStyleVar();
                SameLine();
            }

            auto value = ColorConvertU32ToFloat4(mapped_value);
            if (is_auto) BeginDisabled();
            const bool changed = ImGui::ColorEdit4("", (float *)&value, flags | ImGuiColorEditFlags_AlphaBar | (AllowAuto ? ImGuiColorEditFlags_AlphaPreviewHalf : 0));
            UpdateGesturing();
            if (is_auto) EndDisabled();

            SameLine(0, GetStyle().ItemInnerSpacing.x);
            TextUnformatted(color_name);

            PopID();

            if (changed) Ctx.CoreQ(Action::Vector<u32>::Set{Id, i, ColorConvertFloat4ToU32(value)});
        }
    }
    if (AllowAuto) {
        Separator();
        PushTextWrapPos(0);
        Text("Colors that are set to Auto will be automatically deduced from your ImGui style or the current ImPlot colormap.\n"
             "If you want to style individual plot items, use Push/PopStyleColor around its function.");
        PopTextWrapPos();
    }

    PopItemWidth();
    EndChild();
}

void Colors::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    if (TreeNode(Name)) {
        const auto &value = Get();
        for (u32 i = 0; i < value.size(); i++) {
            TreeNode(annotate ? GetName(i) : std::to_string(i), annotate, U32ToHex(value[i], true).c_str());
        }
        TreePop();
    }
}
