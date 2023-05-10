// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_SEE_H
#define BITCOIN_SCRIPT_SEE_H

#include <script/interpreter.h>

/** A data type to abstract out the condition stack during script execution.
 *
 * Conceptually it acts like a vector of booleans, one for each level of nested
 * IF/THEN/ELSE, indicating whether we're in the active or inactive branch of
 * each.
 *
 * The elements on the stack cannot be observed individually; we only need to
 * expose whether the stack is empty and whether or not any false values are
 * present at all. To implement OP_ELSE, a toggle_top modifier is added, which
 * flips the last value without returning it.
 *
 * This uses an optimized implementation that does not materialize the
 * actual stack. Instead, it just stores the size of the would-be stack,
 * and the position of the first false value in it.
 */
class ConditionStack {
private:
    //! A constant for m_first_false_pos to indicate there are no falses.
    static constexpr uint32_t NO_FALSE = std::numeric_limits<uint32_t>::max();

    //! The size of the implied stack.
    uint32_t m_stack_size = 0;
    //! The position of the first false value on the implied stack, or NO_FALSE if all true.
    uint32_t m_first_false_pos = NO_FALSE;

public:
    size_t size() const { return m_stack_size; }
    bool at(size_t idx) const { return m_first_false_pos > idx; }
    bool empty() const { return m_stack_size == 0; }
    bool all_true() const { return m_first_false_pos == NO_FALSE; }
    void push_back(bool f)
    {
        if (m_first_false_pos == NO_FALSE && !f) {
            // The stack consists of all true values, and a false is added.
            // The first false value will appear at the current size.
            m_first_false_pos = m_stack_size;
        }
        ++m_stack_size;
    }
    void pop_back()
    {
        assert(m_stack_size > 0);
        --m_stack_size;
        if (m_first_false_pos == m_stack_size) {
            // When popping off the first false value, everything becomes true.
            m_first_false_pos = NO_FALSE;
        }
    }
    void toggle_top()
    {
        assert(m_stack_size > 0);
        if (m_first_false_pos == NO_FALSE) {
            // The current stack is all true values; the first false will be the top.
            m_first_false_pos = m_stack_size - 1;
        } else if (m_first_false_pos == m_stack_size - 1) {
            // The top is the first false value; toggling it will make everything true.
            m_first_false_pos = NO_FALSE;
        } else {
            // There is a false value, but not on top. No action is needed as toggling
            // anything but the first false value is unobservable.
        }
    }
};

struct ScriptExecutionEnvironment {
    CScript script;
    CScript::const_iterator pend;
    CScript::const_iterator pbegincodehash;
    opcodetype opcode;
    std::vector<uint8_t> vchPushValue;
    ConditionStack vfExec;
    std::vector<std::vector<uint8_t>> altstack;
    int nOpCount;
    bool fRequireMinimal;
    std::vector<std::vector<unsigned char> >& stack;
    unsigned int flags;
    const BaseSignatureChecker& checker;
    SigVersion sigversion;
    ScriptError* serror;
    std::map<std::vector<unsigned char>,std::vector<unsigned char>> pretend_valid_map;
    std::set<std::vector<unsigned char>> pretend_valid_pubkeys;
    ScriptExecutionEnvironment(std::vector<std::vector<unsigned char> >& stack_in, const CScript& script_in, unsigned int flags_in, const BaseSignatureChecker& checker_in);

    uint32_t opcode_pos;
    ScriptExecutionData execdata;

    bool allow_disabled_opcodes;
};

bool StepScript(ScriptExecutionEnvironment& env, CScript::const_iterator& pc, CScript* local_script = nullptr);

// made public to assist instance.cpp
bool VerifyTaprootCommitment(const std::vector<unsigned char>& control, const std::vector<unsigned char>& program, const CScript& script, uint256* tapleaf_hash);

#endif // BITCOIN_SCRIPT_SEE_H
