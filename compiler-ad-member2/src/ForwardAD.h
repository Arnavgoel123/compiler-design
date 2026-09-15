#pragma once

#include "../../compiler-ad-member4/src/ir/IR.h"
#include <string>

namespace forward_ad {

// Builds a new IR function whose return value is the directional derivative
// of the original function with respect to one input variable.
ir::IRFunction forwardDifferentiate(const ir::IRFunction& original,
                                    const std::string& withRespectTo);

} // namespace forward_ad

namespace ad {

// Compatibility wrapper for callers using the original Member 2 class API.
class ForwardAD {
public:
    ir::IRFunction differentiate(const ir::IRFunction& original,
                                 const std::string& withRespectTo) const;
};

} // namespace ad
