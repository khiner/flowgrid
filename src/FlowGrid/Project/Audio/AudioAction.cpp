#include "AudioAction.h"

namespace Action {
std::variant<Faust::File::Open, bool> Faust::File::Open::Merge(const Faust::File::Open &other) const {
    if (file_path == other.file_path) return other;
    return false;
}
} // namespace Action
