#include "regex-matcher.h"
#include "regex-automaton.h"
#include <deque>
#include <algorithm>
#include <iterator>

using namespace std;

namespace yui
{
    // Implementation of RegexMatcher
    //
    bool RegexMatcher::Match(std::string_view s) const
    {
        auto result = SerachInternal(s, false);

        return result && result->content.length() == s.length();
    }

    RegexMatchOpt RegexMatcher::Search(std::string_view s) const
    {
        return SerachInternal(s, true);
    }

    RegexMatchVec RegexMatcher::SearchAll(std::string_view s) const
    {
        RegexMatchVec result;
        std::string_view remaining_view = s;

        while (!remaining_view.empty())
        {
            auto match = SerachInternal(remaining_view, true);
            if (match)
            {
                // truncate remaining view to search next
                auto searched_offset = std::distance(remaining_view.data(), match->content.data()) + match->content.length();
                remaining_view.remove_prefix(searched_offset);

                // save the last successful match
                result.push_back(std::move(*match));
            }
            else
            {
                // no more matches
                break;
            }
        }

        return result;
    }

    // DfaRegexMatcher
    //

    RegexMatch CreateRegexMatch(string_view content)
    {
        return RegexMatch{ content, {} };
    }

    class DfaRegexMatcher : public RegexMatcher
    {
    public:
        DfaRegexMatcher(DfaAutomaton atm)
            : dfa_(std::move(atm)) { }

    protected:
        RegexMatchOpt SerachInternal(string_view view, bool allow_substr) const override
        {
            for (auto start_it = view.begin(); start_it != view.end(); ++start_it)
            {
                auto found = false;
                auto last_matched_it = start_it;
                auto state = dfa_.InitialState();

                for (auto it = start_it; it != view.end(); ++it)
                {
                    state = dfa_.Transit(state, *it);
                    if (state != kInvalidDfaState)
                    {
                        // record the current position if it's accepting
                        if (dfa_.IsAccepting(state))
                        {
                            found = true;
                            last_matched_it = it;
                        }
                    }
                    else // no more character wanted
                    {
                        break;
                    }
                }

                if (found)
                {
                    auto start_offset = distance(view.begin(), start_it);
                    auto matched_len = distance(start_it, last_matched_it) + 1;
                    return CreateRegexMatch(view.substr(start_offset, matched_len));
                }
                else if (!allow_substr)
                {
                    break;
                }
            }

            return std::nullopt;
        }

    private:
        DfaAutomaton dfa_;
    };

    // NfaRegexMatcher
    //
    class NfaRegexMatcher : public RegexMatcher
    {
    public:
        NfaRegexMatcher(NfaAutomaton atm)
            : nfa_(std::move(atm)) { }

    protected:
		// things with larger index are prior
		void ExpandRoutes(deque<pair<int, NfaTransition*>>& output, const NfaState* state, int index, int ch) const
		{
			for (auto it = state->exits.rbegin(); it != state->exits.rend(); ++it)
			{
				auto edge = *it;

				switch (edge->type)
				{
				case TransitionType::Entity:
					if (get<CharRange>(edge->data).Contain(ch))
					{
						output.emplace_back(index+1, edge);
					}
					break;

				case TransitionType::Epsilon:
					throw 0; // not suppose to happen
				default:
					// for non-character-consuming edge
					// output.emplace_back(index, edge);
					throw 0; // not implemented
				}
			}
		}

        RegexMatchOpt SerachInternal(string_view view, bool allow_substr) const override
        {
			assert(!view.empty());

			// TODO: add minimum-length optimization
			for (auto index = 0; index < view.length(); ++index)
			{
				deque<pair<int, NfaTransition*>> routes;

				// initialize routes
				ExpandRoutes(routes, nfa_.IntialState(), index, view[index]);

				// TODO: add capturing and reference
				// iterates and backtracks
				while (!routes.empty())
				{
					auto[target_index, last_edge] = routes.back();
					routes.pop_back();

					// TODO: fix non-greedy behavior
					// record possible match
					if (last_edge->target->is_final)
					{
						return CreateRegexMatch(view.substr(index, target_index - index));
					}

					// lookup possible new routes
					ExpandRoutes(routes, last_edge->target, target_index, view[target_index]);
				}

				if (!allow_substr)
				{
					break;
				}
			}

			return nullopt;
		}

    private:
        NfaAutomaton nfa_;
    };

    // Matcher Factory
    //

    RegexMatcher::Ptr CreateDfaMatcher(DfaAutomaton dfa)
    {
        return make_unique<DfaRegexMatcher>(std::move(dfa));
    }

    RegexMatcher::Ptr CreateNfaMatcher(NfaAutomaton nfa)
    {
        // automata to simulate should have no epsilon edges for the sake of performance
        assert(!nfa.HasEpsilon());

        return make_unique<NfaRegexMatcher>(std::move(nfa));
    }
}