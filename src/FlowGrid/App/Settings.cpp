#include "Settings.h"

#include "Core/Store/StoreHistory.h"

#include "imgui.h"

using namespace ImGui;

void ApplicationSettings::Render() const {
    int value = int(History.Index);
    if (SliderInt("History index", &value, 0, int(History.Size() - 1))) Action::Project::SetHistoryIndex{value}.q();
    GestureDurationSec.Draw();
}
