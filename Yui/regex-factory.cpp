#include "regex-factory.h"

using namespace std;

namespace yui
{
    // Implementation of RegexFactoryBase

    // TODO: provides options
    RegexFactoryBase::RegexFactoryBase() { }

    ManagedRegex::Ptr RegexFactoryBase::Generate()
    {
        return std::make_unique<ManagedRegex>(std::move(arena_), Construct());
    }

    RegexExpr* RegexFactoryBase::Range(CharRange rg)
    {
        return arena_.Construct<EntityExpr>(rg);
    }

    RegexExpr* RegexFactoryBase::Char(int ch)
    {
        assert(ch >= 0 && ch < 128); // TODO: ?
        return Range(CharRange{ ch, ch });
    }

    RegexExpr* RegexFactoryBase::String(const char* s)
    {
        assert(s != nullptr);

        RegexExprVec vec;
        for (auto p = s; *p; ++p)
        {
            vec.push_back(Char(*p));
        }

        return Concat(vec);
    }

	RegexExpr* RegexFactoryBase::Letter()
	{
		return Alter({
			Range({ 'a', 'z' }),
			Range({ 'A', 'Z' }),
		});
	}

	RegexExpr* RegexFactoryBase::Digit()
	{
		return Range({ '0', '9' });
	}

    RegexExpr* RegexFactoryBase::Concat(const RegexExprVec& seq)
    {
        return arena_.Construct<ConcatenationExpr>(seq);
    }

    RegexExpr* RegexFactoryBase::Alter(const RegexExprVec& seq)
    {
        return arena_.Construct<AlternationExpr>(seq);
    }

    RegexExpr* RegexFactoryBase::Repeat(RegexExpr* expr, Repetition rep, ClosureStrategy strategy)
    {
        return arena_.Construct<RepetitionExpr>(expr, rep, strategy);
    }

    RegexExpr* RegexFactoryBase::Optional(RegexExpr* expr)
    {
        return Repeat(expr, Repetition{ 0, 1 }, ClosureStrategy::Greedy);
    }

    RegexExpr* RegexFactoryBase::Star(RegexExpr* expr)
    {
        return Repeat(expr, Repetition{ 0 }, ClosureStrategy::Greedy);
    }

    RegexExpr* RegexFactoryBase::Plus(RegexExpr* expr)
    {
        return Repeat(expr, Repetition{ 1 }, ClosureStrategy::Greedy);
    }

    RegexExpr* RegexFactoryBase::Anchor(AnchorType type)
    {
        return arena_.Construct<AnchorExpr>(type);
    }

	RegexExpr* RegexFactoryBase::Capture(unsigned id, RegexExpr* expr)
	{
		assert(id < 1000);

		return arena_.Construct<CaptureExpr>(id, expr);
	}

	RegexExpr* RegexFactoryBase::Reference(unsigned id)
	{
		return arena_.Construct<ReferenceExpr>(id);
	}
}