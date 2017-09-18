#include "regex-automaton.h"
#include "flat-set.hpp"
#include <functional>
#include <map>
#include <set>

using namespace std;

namespace yui
{
    // Implementation of NfaBuilder
    //

    NfaState* NfaBuilder::NewState(bool is_final)
    {
        NfaState* result = arena_.Construct<NfaState>();
        result->is_final = is_final;

        return result;
    }

    NfaBranch NfaBuilder::NewBranch(bool is_final)
    {
        NfaBranch result;
        result.begin = NewState(false);
        result.end = NewState(is_final);

        return result;
    }

    NfaTransition* NfaBuilder::NewEpsilonTransition(NfaBranch branch, EpsilonPriority priority)
    {
        auto transition = ConstructTransition(branch.begin, branch.end, TransitionType::Epsilon);
        transition->data = priority;

        return transition;
    }
    NfaTransition* NfaBuilder::NewEntityTransition(NfaBranch branch, CharRange value)
    {
        auto transition = ConstructTransition(branch.begin, branch.end, TransitionType::Entity);
        transition->data = value;

        return transition;
    }
    NfaTransition* NfaBuilder::NewAnchorTransition(NfaBranch branch, AnchorType anchor)
    {
        auto transition = ConstructTransition(branch.begin, branch.end, TransitionType::Anchor);
        transition->data = anchor;

        return transition;
    }
    NfaTransition* NfaBuilder::NewCaptureTransition(NfaBranch branch, unsigned id)
    {
        auto transition = ConstructTransition(branch.begin, branch.end, TransitionType::BeginCapture);
        transition->data = id;

        return transition;
    }
    NfaTransition* NfaBuilder::NewReferenceTransition(NfaBranch branch, unsigned id)
    {
        auto transition = ConstructTransition(branch.begin, branch.end, TransitionType::Reference);
        transition->data = id;

        return transition;
    }
    NfaTransition* NfaBuilder::NewAssertionTransition(NfaBranch branch, AssertionType type)
    {
        return ConstructTransition(branch.begin, branch.end, TransitionType::Assertion);
    }
    NfaTransition* NfaBuilder::NewFinishTransition(NfaBranch branch)
    {
        return ConstructTransition(branch.begin, branch.end, TransitionType::EndCapture);
    }

    NfaTransition* NfaBuilder::CloneTransition(NfaBranch branch, const NfaTransition *transition)
    {
        NfaTransition *new_transition = ConstructTransition(branch.begin, branch.end, transition->type);
        new_transition->data = transition->data;

        return new_transition;
    }

    void NfaBuilder::CloneBranch(NfaBranch target, NfaBranch source)
    {
        std::unordered_map<const NfaState*, NfaState*> state_map;
        std::queue<const NfaState*> waitlist;

        state_map.insert_or_assign(source.begin, target.begin);
        state_map.insert_or_assign(source.end, target.end);

        waitlist.push(source.begin);
        while (!waitlist.empty())
        {
            const NfaState *start = waitlist.front();
            waitlist.pop();

            NfaState* mapped_start = state_map[start];
            for (const NfaTransition *edge : start->exits)
            {
                NfaState *mapped_target;

                auto mapped_target_iter = state_map.find(edge->target);
                if (mapped_target_iter == state_map.end())
                {
                    mapped_target = NewState();
                    state_map.insert_or_assign(edge->target, mapped_target);
                    waitlist.push(edge->target);
                }
                else
                {
                    mapped_target = mapped_target_iter->second;
                }

                CloneTransition(NfaBranch{mapped_start, mapped_target}, edge);
            }
        }
    }

    NfaTransition* NfaBuilder::ConstructTransition(NfaState *source, NfaState *target, TransitionType type)
    {
        if (dfa_compatible_)
        {
            switch (type)
            {
            case TransitionType::Anchor:
            case TransitionType::BeginCapture:
            case TransitionType::Reference:
            case TransitionType::Assertion:
            case TransitionType::EndCapture:
                dfa_compatible_ = false;
                break;
            }
        }

        if (type == TransitionType::Epsilon)
        {
            has_epsilon_ = true;
        }

        // create a new transition
        auto result = arena_.Construct<NfaTransition>();
        result->source = source;
        result->target = target;
        result->type = type;

        // add it to source state
        source->exits.push_back(result);

        return result;
    }

    // Implementation of DfaBuilder
    //
    DfaState DfaBuilder::NewState(bool accepting)
    {
        jumptable_.resize(jumptable_.size() + kDfaJumptableWidth, kInvalidDfaState);
        int id = next_state_++;

        if (accepting)
        {
            // TODO: name the magic number 0
            jumptable_[id * kDfaJumptableWidth + kAcceptingIndicatorOffset] = 0;
        }

        return id;
    }

    void DfaBuilder::NewTransition(DfaState src, DfaState target, int ch)
    {
        assert(src < next_state_ && target < next_state_);
        assert(ch > 0 && ch < 128);

        jumptable_[src * kDfaJumptableWidth + ch] = target;
    }

    DfaAutomaton DfaBuilder::Build()
    {
        return DfaAutomaton(0, jumptable_);
    }

    // Algorithms
    //

    // priority of a transition is measured by an integer
    int CalcTransitionPriority(const NfaTransition* edge)
    {
        if (edge->type == TransitionType::Epsilon)
        {
            switch (std::get<EpsilonPriority>(edge->data))
            {
            case EpsilonPriority::High:
                return 0;
            case EpsilonPriority::Normal:
                return 1;
            case EpsilonPriority::Low:
                return 2;
            }
        }
    
        // non-epsilon transtitions have priority of 1
        return 1;
    }

    // returns true if lhs is less prior to rhs
    bool CompareTransitionPriority(const NfaTransition* lhs, const NfaTransition* rhs)
    {
        return CalcTransitionPriority(lhs) < CalcTransitionPriority(rhs);
    }

    // enumerates via brand first search
    void EnumerateNfa(const NfaState* initial, function<void(const NfaState*)> callback)
    {
        std::unordered_set<const NfaState*> visited;
        std::queue<const NfaState*> waitlist;

        waitlist.push(initial);
        visited.insert(initial);
        while (!waitlist.empty())
        {
            const NfaState *source = waitlist.front();
            waitlist.pop();

            callback(source);

            for (const NfaTransition *edge : source->exits)
            {
                if (visited.find(edge->target) == visited.end())
                {
                    waitlist.push(edge->target);
                    visited.insert(edge->target);
                }
            }
        }
    }

    // copies transitions from a state into an output vector and sorts them
    void ExpandTransitions(std::vector<const NfaTransition*>& output, const NfaState* start)
    {
        auto range_begin_iter = output.insert(output.end(), start->exits.begin(), start->exits.end());
        auto range_end_iter = output.end();

        // more prior transitions should come before those less prior ones
        // in this way, they prioritize on matching
        std::sort(range_begin_iter, range_end_iter, CompareTransitionPriority);
    };

    NfaEvaluationResult EvaluateNfa(const NfaAutomaton& atm)
    {
        // a *solid state* is one that has at least one incoming non-epsilon transition
        // a *accepting state* is one that could lead to a match
        // NOTE that only solid states would be evaluated
        
        NfaEvaluationResult result;
        std::queue<const NfaState*> waitlist; // a queue for unprocessed solid states

        // initialize iteration
        const NfaState* initial_state = atm.IntialState();
        result.initial_state = initial_state;
        result.solid_states.insert(initial_state);
        waitlist.push(initial_state);

        // iterate until no solid state can be accessed
        while (!waitlist.empty())
        {
            // fetch a source solid state from waitlist
            const NfaState *source = waitlist.front();
            waitlist.pop();

            std::unordered_set<const NfaTransition*> expanded; // to track expanded epsilon transitions
            std::vector<const NfaTransition*> output_buffer;   // to store results of expansion
            std::vector<const NfaTransition*> input_buffer;    // to store transitions to be expanded

            // the state that is final is accepting
            if (source->is_final)
            {
                result.accepting_states.insert(source);
            }

            // make initial expansion from source state
            ExpandTransitions(output_buffer, source);
            // iterate to expand all epsilon transitions
            bool has_expansion = true;
            while (has_expansion)
            {
                has_expansion = false;
                input_buffer.clear();
                std::swap(input_buffer, output_buffer);

                for (const NfaTransition* edge : input_buffer)
                {
                    if (edge->type == TransitionType::Epsilon)
                    {
                        if (edge->target->is_final)
                        {
                            // the state which can reach the final state with epsilon only is accepting
                            result.accepting_states.insert(source);
                        }

                        // expand the epsilon transition for the first time only
                        if (expanded.find(edge) == expanded.end())
                        {
                            has_expansion = true;
                            expanded.insert(edge);

                            ExpandTransitions(output_buffer, edge->target);
                        }
                    }
                    else
                    {
                        // the edge points to a solid state
                        // queue it if it's not processed yet
                        if (result.solid_states.find(edge->target) == result.solid_states.end())
                        {
                            result.solid_states.insert(edge->target);
                            waitlist.push(edge->target);
                        }

                        // simply copies the non-epsilon transtion
                        output_buffer.push_back(edge);
                    }
                }
            }

			// TODO: ensure this always work
            // eliminate identical transitions(I don't konw if this always works)
            auto new_end_iter = std::unique(output_buffer.begin(), output_buffer.end());
            output_buffer.erase(new_end_iter, output_buffer.end());

            // copy posible transitions from current solid state into result
            for (const NfaTransition *edge : output_buffer)
            {
                result.outbounds.insert({ source, edge });
            }
        }

        return result;
    }

    // generates a Nfa with epsilon eliminated
    NfaAutomaton EliminateEpsilon(const NfaAutomaton &atm)
    {
        NfaEvaluationResult eval = EvaluateNfa(atm);
        NfaBuilder builder;
        std::unordered_map<const NfaState*, NfaState*> state_map; // maps an old solid state to a new one

        // first iteration: clone states
        for (const NfaState* state : eval.solid_states)
        {
            auto is_final = eval.accepting_states.find(state) != eval.accepting_states.end();
            auto mapped_state = builder.NewState(is_final);

            state_map.insert_or_assign(state, mapped_state);
        }

        // second iteration: clone transitions
        for (const NfaState* source : eval.solid_states)
        {
            auto mapped_source = state_map[source];
            auto outgoing_edges = eval.outbounds.equal_range(source);

            // clone transitions one by one
            std::for_each(outgoing_edges.first, outgoing_edges.second,
                [&](auto pair)
            {
                // fetch edge
                const NfaTransition* edge = pair.second;

                // clone transition
                assert(edge->type != TransitionType::Epsilon);
                assert(state_map.find(edge->target) != state_map.end());
                builder.CloneTransition(NfaBranch{ mapped_source, state_map[edge->target] }, edge);
            });
        }

        return builder.Build(state_map[eval.initial_state]);
    }

    // generates a DFA from a NFA
    DfaAutomaton GenerateDfa(const NfaAutomaton &atm)
    {
        assert(atm.DfaCompatible());

        NfaEvaluationResult eval = EvaluateNfa(atm);
        DfaBuilder builder{};

        using NfaStateSet = FlatSet<const NfaState*>;
        std::map<NfaStateSet, DfaState> id_map; // maps a set of NFA states to a DFA state
        std::queue<NfaStateSet> waitlist;

        const auto TestAccepting =
            [&](const NfaState* state)
        {
            return eval.accepting_states.find(state) != eval.accepting_states.end();
        };

        // process initial state
        // initial state cannot be accepting as regex cannot match empty string
        assert(!TestAccepting(eval.initial_state));
        DfaState initial_id = builder.NewState(false);
        NfaStateSet initial_set = NfaStateSet{ eval.initial_state };
        id_map.insert_or_assign(initial_set, initial_id);

        waitlist.push(initial_set);

        while (!waitlist.empty())
        {
            // fetch source set from the queue
            NfaStateSet source_set = std::move(waitlist.front());
            waitlist.pop();

            // lookup source id
            DfaState source_id = id_map[source_set];

            // make a copy of all outgoing transitions
            std::vector<const NfaTransition*> transitions;
            for (const NfaState* state : source_set)
            {
                auto range = eval.outbounds.equal_range(state);
                std::transform(range.first, range.second, std::back_inserter(transitions),
                    [](auto iter) { return iter.second; });
#pragma warning()
            }

            assert(std::all_of(transitions.begin(), transitions.end(),
                [](const NfaTransition *edge) { return edge->type == TransitionType::Entity; }));

            // for each possible character
            for (int ch = 0; ch < 128; ++ch)
            {
                // calculate target dfa state
                NfaStateSet target_set;
                for (const NfaTransition* edge : transitions)
                {
                    if (std::get<CharRange>(edge->data).Contain(ch))
                    {
                        target_set.insert(edge->target);
                    }
                }

                // empty target_set is invalid, so discard it
                if (!target_set.empty())
                {
                    // calculate dfa id for target_set
                    DfaState target_id;
                    auto id_iter = id_map.find(target_set);
                    if (id_iter != id_map.end())
                    {
                        target_id = id_iter->second;
                    }
                    else
                    {
                        // state not found in cache
                        // so create a new one
                        bool accepting = std::any_of(target_set.begin(), target_set.end(), TestAccepting);
                        target_id = builder.NewState(accepting);
                        id_map.insert_or_assign(target_set, target_id);

                        // queue it
                        waitlist.push(std::move(target_set));
                    }

                    // make transition
                    builder.NewTransition(source_id, target_id, ch);
                }
            }
        }

        return builder.Build();
    }
}