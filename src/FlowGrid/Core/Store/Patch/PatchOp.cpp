#include "PatchOp.h"

std::string to_string(PatchOp::Type patch_op_type) {
    switch (patch_op_type) {
        case PatchOp::Type::Add: return "Add";
        case PatchOp::Type::Remove: return "Remove";
        case PatchOp::Type::Replace: return "Replace";
    }
}
