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

		struct SimulationContext
		{
			deque<pair<unsigned, NfaTransition*>> routes; // (target index, passed edge)
			vector<string_view> captures;
		};

		// things with larger index are prior
		void ExpandRoutes(SimulationContext& ctx, const NfaState* state, size_t index, const string_view view) const
		{
			assert(index <= view.length());

			auto&[routes, captures] = ctx;
			for (auto it = state->exits.rbegin(); it != state->exits.rend(); ++it)
			{
				const auto edge = *it;

				switch (edge->type)
				{
					// Entity transition attemps to consume a character in its range
				case TransitionType::Entity:
					if (index < view.length() && get<CharRange>(edge->data).Contain(view[index]))
					{
						routes.emplace_back(index+1, edge);
					}
					break;

					// Anchor transition checks the context without consuming any character
				case TransitionType::Anchor:
					if (get<AnchorType>(edge->data) == AnchorType::LineStart)
					{
						if (index == 0 || view[index - 1] == '\n')
						{
							routes.emplace_back(index, edge);
						}
					}
					else // then AnchorType::LineBreak
					{
						if (index == view.length() || view[index] == '\n')
						{
							routes.emplace_back(index, edge);
						}
					}
					break;

					// the following transitions always pass
				case TransitionType::BeginCapture:
				case TransitionType::EndCapture:
				case TransitionType::BeginAssertion:
				case TransitionType::EndAssertion:
					routes.emplace_back(index, edge);
					break;

					// Reference transitions may consume multiple characters
					// NOTE it cannot refer to empty string
				case TransitionType::Reference:
				{
					auto id = get<unsigned>(edge->data);
					if (captures.size() > id || !captures[id].empty())
					{
						auto expected_str = captures[id];
						auto test_str = view.substr(index, expected_str.length());
						if (expected_str == test_str)
						{
							routes.emplace_back(index + expected_str.length(), edge);
						}
					}
				}
				break;

				case TransitionType::Epsilon:
					throw 0; // not suppose to happen
				}
			}
		}

	protected:

        RegexMatchOpt SerachInternal(string_view view, bool allow_substr) const override
        {
			// TODO: add minimum-length optimization
			// TODO: add Assertion support
			// TODO: discards captured contents when backtracking <- support multiple capture?
			for (size_t index = 0; index < view.length(); ++index)
			{
				bool found = false;
				auto last_matched_depth = 0;
				auto last_matched_index = index;

				SimulationContext ctx;
				auto&[routes, captures] = ctx;

				stack<tuple<int, int, int>> capture_buffer; // (start_pos, thres, id)

				// initialize routes
				ExpandRoutes(ctx, nfa_.IntialState(), index, view);

				// iterate and backtrack for the first match
				while (!routes.empty())
				{
					auto[target_index, last_edge] = routes.back();
					routes.pop_back();

					const auto current_depth = routes.size();

					// never backtrack to discard a match
					if (found && current_depth < last_matched_depth)
					{
						break;
					}

					// remove capture buffer if no longer valid on backtracking
					while (!capture_buffer.empty() 
						&& current_depth < get<1>(capture_buffer.top()))
					{
						capture_buffer.pop();
					}

					// process special transitions
					switch (last_edge->type)
					{
					case TransitionType::BeginCapture:
					{
						auto id = get<unsigned>(last_edge->data);
						capture_buffer.push(make_tuple(target_index, current_depth, id));
					}
						break;
					case TransitionType::EndCapture:
					{
						// when it managed to get EndCapture transition, there must be a match
						// NOTE not to discard the buffer item, 
						//	    it may be used by other EndCapture transitions
						auto[start_pos, thres, id] = capture_buffer.top();

						if (captures.size() <= id)
						{
							captures.resize(id + 1);
						}

						captures[id] = view.substr(start_pos, target_index - start_pos);
					}
						break;

					case TransitionType::BeginAssertion:
					case TransitionType::EndAssertion:
						throw 0;
						break;
					}

					// record possible match
					if (last_edge->target->is_final)
					{
						found = true;
						last_matched_depth = current_depth;
						last_matched_index = target_index;
					}

					// lookup possible new routes
					ExpandRoutes(ctx, last_edge->target, target_index, view);
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
        // automaton for simulation should have no epsilon edge for the sake of performance
        assert(!nfa.HasEpsilon());

        return make_unique<NfaRegexMatcher>(std::move(nfa));
    }
}