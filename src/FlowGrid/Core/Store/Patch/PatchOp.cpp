#include "PatchOp.h"

std::string to_string(PatchOpType patch_op_type) {
    switch (patch_op_type) {
        case PatchOpType::Add: return "Add";
        case PatchOpType::Remove: return "Remove";
        case PatchOpType::Replace: return "Replace";
    }
}
