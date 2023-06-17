#include "AudioAction.h"

namespace Action {
std::variant<FaustFile::Open, bool> FaustFile::Open::Merge(const FaustFile::Open &other) const {
    if (file_path == other.file_path) return other;
    return false;
}
} // namespace Action
