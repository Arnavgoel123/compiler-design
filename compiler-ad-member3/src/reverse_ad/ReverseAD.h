#pragma once

#include "../../../compiler-ad-member4/src/ir/IR.h"
#include <string>

#include <map>

namespace reverse_ad {

// Reverse-mode Automatic Differentiation (Adjoint Mode).
// Takes the original forward IR evaluation graph and the name of the variable
// to differentiate with respect to. Returns a new ir::IRFunction whose returned
// value computes the exact analytical derivative.
//
// This output is designed to be fed directly into Member 4's Optimizer,
// CodeGenerator, and Validator.
ir::IRFunction reverseDifferentiate(const ir::IRFunction& original, const std::string& withRespectTo);

// Computes the full gradient for all input parameters in a single reverse sweep.
// Returns a map of parameter name -> derivative IRFunction.
std::map<std::string, ir::IRFunction> reverseGradient(const ir::IRFunction& original);

} // namespace reverse_ad

