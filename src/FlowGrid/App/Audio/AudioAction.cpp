#include "AudioAction.h"

namespace Action {
std::variant<FaustFile::Open, bool> FaustFile::Open::Merge(const FaustFile::Open &other) const {
    if (path == other.path) return other;
    return false;
}
} // namespace Action
