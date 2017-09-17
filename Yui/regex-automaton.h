// Provides models of NFA and DFA

#pragma once
#include "regex-core.h"
#include "class-utils.hpp"
#include "arena.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <functional>

namespace yui
{
    // Non-deterministic Finite Automaton(NFA)
    //

    struct NfaTransition;
    struct NfaState;

    enum class TransitionType
    {
        Epsilon,    // empty transition
        Entity,     // for char range
        Anchor,     // builtin zero-width assertion
        Capture,    // begin capture
        Reference,  // backreference
        Assertion,  // begin custom zero-width assertion
        FinishCapture,     // end mark for Capture and Assertion group
    };

    struct NfaTransition
    {
        NfaTransition() = default;
        ~NfaTransition() = default;

        const NfaState* source;
        const NfaState* target;
        TransitionType type;

        std::variant<
            std::monostate,         // default empty choice
            EpsilonPriority,        // valid only when type is Epsilon
            AnchorType,             // valid only when type is Anchor
            CharRange,              // valid only when type is Entity
            unsigned,               // valid only when type is Capture or Reference
            AssertionType           // valid only when type is Assertion
            > data;    
    };

    struct NfaState
    {
        bool is_final;                              // indicate whether this state is accepting
        bool is_checkpoint;                         // indicate whether this state should be backtracked
        std::vector<NfaTransition*> exits;          // where outgoing edges stores
    };

    struct NfaBranch
    {
        NfaState* begin;
        NfaState* end;
    };

    // This class should only be constructed via a NfaBuilder
    class NfaAutomaton
    {
    private:
        friend class NfaBuilder;
        NfaAutomaton(Arena arena, NfaState* begin, bool has_epsilon, bool dfa_compatible)
            : arena_(std::move(arena))
            , initial_state_(begin)
            , has_epsilon_(has_epsilon)
            , dfa_compatible_(dfa_compatible) { }
    
    public:
        bool DfaCompatible() const
        {
            return dfa_compatible_;
        }

        bool HasEpsilon() const
        {
            return has_epsilon_;
        }

        const NfaState* IntialState() const
        {
            return initial_state_;
        }

    private:
        Arena arena_;

        bool dfa_compatible_;
        bool has_epsilon_;
        const NfaState* initial_state_;
    };

    class NfaBuilder : Uncopyable, Unmovable
    {
    public:
        NfaBuilder()
        {
            has_epsilon_ = false;
            // assume it compatible with DFA at first
            dfa_compatible_ = true;
        }

        // Creates States
        //

        // allocates a new state
        NfaState* NewState(bool is_final = false);

        // allocates an independent states pair
        NfaBranch NewBranch(bool is_final = false);

        // Creates Transitions
        //

        NfaTransition* NewEpsilonTransition(NfaBranch branch, EpsilonPriority priority);
        NfaTransition* NewEntityTransition(NfaBranch branch, CharRange value);
        NfaTransition* NewAnchorTransition(NfaBranch branch, AnchorType anchor);
        NfaTransition* NewCaptureTransition(NfaBranch branch, unsigned id);
        NfaTransition* NewReferenceTransition(NfaBranch branch, unsigned id);
        NfaTransition* NewAssertionTransition(NfaBranch branch, AssertionType type);
        NfaTransition* NewFinishTransition(NfaBranch branch);

        // construct the same transition between source and target
        NfaTransition* CloneTransition(NfaBranch branch, const NfaTransition *transition);

        // construct the same transition graph between source and target
        // this function may introduce several new NfaState
        void CloneBranch(NfaBranch target, NfaBranch source);

        // Build
        //

        NfaAutomaton Build(NfaState* start)
        {
            return NfaAutomaton{ std::move(arena_), start, has_epsilon_, dfa_compatible_ };
        }

    private:

        // constructs a transition edge from the source state to the target with its type specified
        // NOTE that its data field is left empty
        NfaTransition* ConstructTransition(NfaState *source, NfaState *target, TransitionType type);

    private:
        bool has_epsilon_;
        bool dfa_compatible_;

        Arena arena_;
    };

    // Deterministic Finite Automaton(DFA)
    //

    // A state in a DFA is denoted as an unsigned int
    // NOTES states that are invalid equal maximum of unsigned int by convention

    using DfaState = unsigned int;
    using DfaStateVec = std::vector<DfaState>;

    // TODO: refine constant definition here
    static constexpr auto kInvalidDfaState = std::numeric_limits<DfaState>::max();
    static constexpr auto kDfaJumptableWidth = 129;
    static constexpr auto kAcceptingIndicatorOffset = 128;

    // Jumptable of a DfaAutomaton should be a n*129 table
    // NOTE last column of the table denotes if a state is accepting(when it unequals kInvalidDfaState)
    class DfaAutomaton
    {
    private:
        friend class DfaBuilder;
        DfaAutomaton(DfaState initial, const DfaStateVec& jumptable)
            : initial_(initial)
            , jumptable_(jumptable) 
        {
            assert(jumptable.size() % kDfaJumptableWidth == 0);
        }

    public:
        size_t StateCount() const { return jumptable_.size() / kDfaJumptableWidth; }

        bool IsAccepting(DfaState state) const 
        {
            return state != kInvalidDfaState
                && jumptable_[state * kDfaJumptableWidth + kAcceptingIndicatorOffset] != kInvalidDfaState;
        }

        int InitialState() const 
        {
            return initial_;
        }

        int Transit(DfaState src, int ch) const
        {
            assert(src <= jumptable_.size() / kDfaJumptableWidth);
            assert(ch >= 0 && ch < 128);

            return jumptable_[src * kDfaJumptableWidth + ch];
        }

    private:
        // TODO: replace this with a constant 0?
        DfaState initial_;

        DfaStateVec jumptable_;
    };

    class DfaBuilder : Uncopyable, Unmovable
    {
    public:
        DfaState NewState(bool accepting);
        void NewTransition(DfaState src, DfaState target, int ch);

        DfaAutomaton Build();

    private:
        DfaState next_state_ = 0;
        DfaStateVec jumptable_;
    };

    // On-Automaton Algorithms
    //

    // TODO: rename this and elaborate the structure
    struct NfaEvaluationResult
    {
        const NfaState* initial_state;
        std::unordered_set<const NfaState*> solid_states;
        std::unordered_set<const NfaState*> accepting_states;

        // first non-epsilon outgoing transitions from a solid state
        // NOTE source state of which may not be a solid state
        std::unordered_multimap<const NfaState*, const NfaTransition*> outbounds;
    };

    void EnumerateNfa(const NfaState* initial, std::function<void(const NfaState*)> callback);
    NfaEvaluationResult EvaluateNfa(const NfaAutomaton& atm);

    NfaAutomaton EliminateEpsilon(const NfaAutomaton &atm);
    DfaAutomaton GenerateDfa(const NfaAutomaton &atm);
}