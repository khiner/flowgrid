#include "AudioAction.h"

namespace Action {
std::variant<OpenFaustFile, bool> OpenFaustFile::Merge(const OpenFaustFile &other) const {
    if (path == other.path) return other;
    return false;
}
} // namespace Action
