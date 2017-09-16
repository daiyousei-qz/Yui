#include "regex-factory.h"
#include "regex-automaton.h"
#include "regex-debug.h"
#include "regex-matcher.h"

using namespace yui;

class TestRegexFactory : public RegexFactoryBase
{
protected:
	RegexExpr* Construct() override
	{
		return Concat({
			Plus(
				Alter({ Char('a'), Char('b') })
			),

			String("233"),
		});
	}
};

ManagedRegex::Ptr GenerateTestRegex()
{
	return TestRegexFactory{}.Generate();
}

int main()
{
	printf("==== Regex Construction ===========================\n");
	auto regex = GenerateTestRegex();
	regex->Expr()->Print(0);
	printf("\n\n");

	printf("==== NFA Construction ===========================\n");
	auto nfa_builder = NfaBuilder{};
	auto branch = nfa_builder.NewBranch(true);
	regex->Expr()->ConnectNfa(nfa_builder, branch);

	auto nfa_e = nfa_builder.Build(branch.begin);
	PrintNfa(nfa_e);
	printf("\n\n");

	printf("==== Epsilon Elimination ===========================\n");
	auto nfa = EliminateEpsilon(nfa_e);
	PrintNfa(nfa);

	printf("==== DFA Construction ===========================\n");
	auto dfa = GenerateDfa(nfa_e);
	PrintDfa(dfa);

	printf("\n\n");

	printf("==== DFA Matcher Test ===========================\n");
	auto dfa_matcher = CreateDfaMatcher(dfa);
	auto r1 = dfa_matcher->Match("aaa233");
	auto r2 = dfa_matcher->Match("aaa2334");
	auto r3 = dfa_matcher->Match("ababa233");
	auto r4 = dfa_matcher->Match("ggababa233");
	auto r5 = dfa_matcher->Search("ggababa233");
	auto r6 = dfa_matcher->SearchAll("a233a;iogjb233iia6bb233");

	system("pause");
}