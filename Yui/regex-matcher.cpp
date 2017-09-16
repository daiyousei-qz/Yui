#include "regex-matcher.h"
#include "regex-automaton.h"

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
		RegexMatchOpt SerachInternal(string_view view, bool allow_substr) const override
		{

		}

	private:
		NfaAutomaton nfa_;
	};

	// Matcher Factory
	//

	RegexMatcher::Ptr CreateDfaMatcher(DfaAutomaton atm)
	{
		return make_unique<DfaRegexMatcher>(std::move(atm));
	}
}