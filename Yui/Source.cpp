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
		auto ref_id = 0u;

		return Plus(
			Alter({
				String("ab"), String("aa"),
			})
		);
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
    PrintNfa(*nfa_e);
    printf("\n\n");

    printf("==== Epsilon Elimination ===========================\n");
    auto nfa = EliminateEpsilon(*nfa_e);
    PrintNfa(*nfa);

	printf("\n\n");

    printf("==== DFA Construction ===========================\n");
	if (nfa_e->DfaCompatible())
	{
		auto dfa = GenerateDfa(*nfa_e);
		PrintDfa(*dfa);
	}
	else
	{
		printf("this automaton is not compatible with DFA\n");
	}

    printf("\n\n");

    printf("==== DFA Matcher Test ===========================\n");
    auto nfa_matcher = CreateNfaMatcher(std::move(nfa));
    auto r1 = nfa_matcher->Match("aaa233;");
    auto r2 = nfa_matcher->Match("aaa2");
    auto r3 = nfa_matcher->Match("ababa233");
    auto r4 = nfa_matcher->Match("ggababa233");
    auto r5 = nfa_matcher->Search("acabbaba233");
	
	// REGEX: ^([$|:])([a-z]|[A-Z])+[0-9]*\1;
	auto r6 = nfa_matcher->SearchAll(":a233:iogjb233iia6\n|bb233$\n$as6$\n$agu8;$");

    system("pause");
}