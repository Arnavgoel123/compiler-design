#ifndef COMPILER_AD_MEMBER2_FORWARD_AD_H
#define COMPILER_AD_MEMBER2_FORWARD_AD_H

#include "compiler-ad-member4/src/ir/IR.h"
#include <string>
#include <unordered_map>

namespace ad {

/**
 * @brief Forward-Mode Automatic Differentiation Engine (Member 2)
 * 
 * Performs Tangent / Dual Propagation over intermediate representation (IR)
 * instructions to generate a combined Primal + Forward Derivative IR function.
 */
class ForwardAD {
public:
    ForwardAD() = default;

    /**
     * @brief Differentiates an IR function in Forward Mode w.r.t. a target variable.
     * 
     * @param original The input primal IR function.
     * @param wrtVariable The variable name w.r.t. which derivatives are computed.
     * @return ir::IRFunction Transformed IR containing primal and tangent instructions.
     */
    ir::IRFunction differentiate(const ir::IRFunction& original, const std::string& wrtVariable);

private:
    std::unordered_map<std::string, std::string> tangentMap; // Maps primal var -> tangent var name

    std::string getTangentName(const std::string& varName);
    std::string resolveOperandTangent(const ir::Operand& op, ir::IRFunction& newFunc, double seedValue);
};

} // namespace ad

#endif // COMPILER_AD_MEMBER2_FORWARD_AD_H
feat: add ForwardAD.h
