#include "regex-matcher.h"
#include "regex-automaton.h"
#include <deque>
#include <stack>
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

    private:

		// (target index, passed transition) pairs
		// ones that have largest index are similated first
		using NfaSimulationContext = deque<pair<int, NfaTransition*>>;

		enum class GroupFlag
		{
			Capture,
			Assertion
		};

		struct SimulationContext
		{
			deque<pair<int, NfaTransition*>> routes;
			vector<pair<int, int>> captures;
			stack<pair<int, GroupFlag>> groups;
		};

		// things with larger index are prior
		void ExpandRoutes(NfaSimulationContext& output, const NfaState* state, int index, const string_view view) const
		{
			assert(index >= 0 && index <= view.length());

			for (auto it = state->exits.rbegin(); it != state->exits.rend(); ++it)
			{
				const auto edge = *it;

				switch (edge->type)
				{
					// Entity transition attemps to consume a character in its range
				case TransitionType::Entity:
					if (index < view.length() && get<CharRange>(edge->data).Contain(view[index]))
					{
						output.emplace_back(index+1, edge);
					}
					break;

					// Anchor transition checks the context without consuming any character
				case TransitionType::Anchor:
					if (get<AnchorType>(edge->data) == AnchorType::LineStart)
					{
						if (index == 0 || view[index - 1] == '\n')
						{
							output.emplace_back(index, edge);
						}
					}
					else // then AnchorType::LineBreak
					{
						if (index == view.length() || view[index] == '\n')
						{
							output.emplace_back(index, edge);
						}
					}
					break;

					// Capture and Finish transitions always pass
				case TransitionType::Capture:
				case TransitionType::FinishCapture:
					output.emplace_back(index, edge);
					break;

				case TransitionType::Reference:
				case TransitionType::Assertion:
					throw 0; // not implemented

				case TransitionType::Epsilon:
					throw 0; // not suppose to happen
				}
			}
		}

	protected:

        RegexMatchOpt SerachInternal(string_view view, bool allow_substr) const override
        {
			assert(!view.empty());

			// TODO: add minimum-length optimization
			for (auto index = 0; index < view.length(); ++index)
			{
				bool found = false;
				auto last_matched_index = index;
				NfaSimulationContext routes;
				vector<string_view> captures;
				stack<tuple<int, int, int>> capture_buffer; // (start_pos, thres, id)

				// initialize routes
				ExpandRoutes(routes, nfa_.IntialState(), index, view);

				// TODO: add capturing and reference
				// iterates and backtracks
				while (!routes.empty())
				{
					auto [target_index, last_edge] = routes.back();
					routes.pop_back();

					// never backtrack to discard a match
					if (found && target_index < last_matched_index)
					{
						break;
					}

					// remove capture buffer if no longer valid on backtracking
					while (!capture_buffer.empty() && get<1>(capture_buffer.top()) > routes.size())
					{
						capture_buffer.pop();
					}

					// process special transitions
					if (last_edge->type == TransitionType::Capture)
					{
						auto id = get<unsigned>(last_edge->data);
						capture_buffer.push(make_tuple(target_index, routes.size(), id));
					}
					else if (last_edge->type == TransitionType::FinishCapture)
					{
						// when it managed to get Finish transition
						// then there is a match for capture group
						auto[start_pos, thres, id] = capture_buffer.top();
						// capture_buffer.pop(); TODO: is this required?

						if (captures.size() <= id)
						{
							captures.resize(id + 1);
						}

						captures[id] = view.substr(start_pos, target_index - start_pos);
					}
					else if (last_edge->type == TransitionType::Assertion)
					{
						throw 0; // not implemented
					}

					// record possible match
					if (last_edge->target->is_final)
					{
						found = true;
						last_matched_index = target_index;
					}

					// lookup possible new routes
					ExpandRoutes(routes, last_edge->target, target_index, view);
				}

				if (found)
				{
					auto content = view.substr(index, last_matched_index - index);
					return RegexMatch{ content, captures };
				}
				else if (!allow_substr)
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