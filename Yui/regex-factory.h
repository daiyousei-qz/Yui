// Provides with class with which a regex model is constructed
// Users may construct a ManagedRegex object manually by inheriting RegexFactoryBase
// or use a builtin text parser that use RegexFactoryBase internally

#pragma once
#include "regex-model.h"
#include "regex-expr.h"

namespace yui
{
    // Inherits this class to create a customized factory
    // for a specific regular expression model
    // You may use ParseRegex instead
    class RegexFactoryBase
    {
    public:
        RegexFactoryBase(); // TODO: provides options

        // Call this function to build a new ManagedRegex instance
        ManagedRegex::Ptr Generate();

    protected:
        // Character Construction
        //
        RegexExpr* Range(CharRange rg);
        RegexExpr* Char(int ch);
        RegexExpr* String(const char* s);

        // Compound Construction
        //
        RegexExpr* Concat(const RegexExprVec& seq);
        RegexExpr* Alter(const RegexExprVec& any);

        // Repetition Construction
        //
        RegexExpr* Repeat(RegexExpr* expr, Repetition rep, ClosureStrategy strategy);

        RegexExpr* Optional(RegexExpr* expr);
        RegexExpr* Star(RegexExpr* expr);
        RegexExpr* Plus(RegexExpr* expr);

        // Fancy Construction
        //
        RegexExpr* Anchor(AnchorType type);
        RegexExpr* Capture(RegexExpr* expr);

        // User-defined Construction Function
        // Override this to control the construction process
        virtual RegexExpr* Construct() = 0;

    private:
        Arena arena_;
    };
}