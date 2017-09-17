// Defines internal regex model

#pragma once
#include "regex-core.h"
#include "arena.hpp"
#include <cassert>
#include <vector>
#include <memory>

namespace yui
{
    struct NfaBranch;
    class NfaBuilder;

    // TODO: use Visitor pattern
    class RegexExpr
    {
    public:
        RegexExpr() = default;
        virtual ~RegexExpr() = default;

        // TODO: remove these two functions as NfaBuilder
        //       would know it on NFA construction anyway
        // if this expression can be translated into DFA
        virtual bool IsDfaCompatible() = 0;
        // if an expression can be content of an assertion
        virtual bool IsAssertionCompatible() = 0;

        // DEBUG
        virtual void Print(size_t ident) = 0;

        // build a path between of such expression between two states given
        virtual void ConnectNfa(NfaBuilder& builder, NfaBranch which) = 0;
    };

    using RegexExprVec = std::vector<RegexExpr*>;

    // A ManagedRegex instance owns and manages memory resources required
    // to present a regex model, which are a set of RegexExpr instances via RAII
    // NOTE: This class should only be constructed by RegexFactoryBase
    class ManagedRegex
    {
    public:
        using Ptr = std::unique_ptr<ManagedRegex>;

        ManagedRegex(Arena arena, RegexExpr* root)
            : arena_(std::move(arena)), root_(root)
        {
            assert(root != nullptr);
        }

        RegexExpr* Expr() const { return root_; }

    private:
        Arena arena_;
        RegexExpr* root_;
    };

    // Expression Model
    //

    template <bool DfaCompatible, bool AssertionCompatible>
    class RegexExprBase : public RegexExpr
    {
    public:
        RegexExprBase() = default;

        bool IsDfaCompatible() override { return DfaCompatible; }
        bool IsAssertionCompatible() override { return AssertionCompatible; }
    };

    class EntityExpr : public RegexExprBase<true, true>
    {
    public:
        EntityExpr(CharRange rg)
            : range_(rg) { }

        auto Range() const { return range_; }

        void Print(size_t ident) override;
        void ConnectNfa(NfaBuilder& builder, NfaBranch which) override;

    private:
        CharRange range_;
    };

    class ConcatenationExpr : public RegexExprBase<true, true>
    {
    public:
        ConcatenationExpr(const RegexExprVec& seq)
            : seq_(seq) { }

        const auto& Children() { return seq_; }

        void Print(size_t ident) override;
        void ConnectNfa(NfaBuilder& builder, NfaBranch which) override;

    private:
        RegexExprVec seq_;
    };

    class AlternationExpr : public RegexExprBase<true, true>
    {
    public:
        AlternationExpr(const RegexExprVec& any)
            : any_(any) { }

        const auto& Children() { return any_; }

        void Print(size_t ident) override;
        void ConnectNfa(NfaBuilder& builder, NfaBranch which) override;

    private:
        RegexExprVec any_;
    };

    class RepetitionExpr : public RegexExpr
    {
    public:
        RepetitionExpr(RegexExpr* child, Repetition rep, ClosureStrategy strategy)
            : child_(child), rep_(rep), strategy_(strategy) { }

        auto Child() const { return child_; }
        auto Strategy() const { return strategy_; }
        auto Count() const { return rep_; }

        bool IsDfaCompatible() override { return strategy_ == ClosureStrategy::Greedy; }
        bool IsAssertionCompatible() override { return true; }

        void Print(size_t ident) override;
        void ConnectNfa(NfaBuilder& builder, NfaBranch which) override;

    public:
        RegexExpr* child_;
        
        Repetition rep_;
        ClosureStrategy strategy_;
    };

    class AnchorExpr : public RegexExprBase<false, true>
    {
    public:
        AnchorExpr(AnchorType def)
            : type_(def) { }

        auto Type() const { return type_; }

        void Print(size_t ident) override;
        void ConnectNfa(NfaBuilder& builder, NfaBranch which) override;

    private:
        AnchorType type_;
    };

    class CaptureExpr : public RegexExprBase<false, false>
    {
    public:
        CaptureExpr(unsigned id, RegexExpr* expr)
            : id_(id) { }

        auto Id() const { return id_; }

        void Print(size_t ident) override;
        void ConnectNfa(NfaBuilder& builder, NfaBranch which) override;

    private:
        unsigned id_;
        RegexExpr* expr_;
    };

    class ReferenceExpr : public RegexExprBase<false, false>
    {
    public:
        ReferenceExpr(unsigned id)
            : id_(id) { }

        auto Id() const { return id_; }

        void Print(size_t ident) override;
        void ConnectNfa(NfaBuilder& builder, NfaBranch which) override;

    private:
        unsigned id_;
    };
}