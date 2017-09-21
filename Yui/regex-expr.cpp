#include "regex-expr.h"
#include "regex-automaton.h"
#include "regex-debug.h"

namespace yui
{
    // Implementations for RegexExpr::BuildNfa
    //

    static NfaBranch CreateEvaluatedBranch(NfaBuilder& builder, RegexExpr& expr)
    {
        auto result = builder.NewBranch();

        expr.ConnectNfa(builder, result);
        return result;
    }

    void EntityExpr::ConnectNfa(NfaBuilder& builder, NfaBranch which)
    {
        builder.NewEntityTransition(which, Range());
    }

    // TODO: add reversed concatenation expression
    void ConcatenationExpr::ConnectNfa(NfaBuilder& builder, NfaBranch which)
    {
        // path looks like:
        //
        // which.begin - ... - ... - ... - which.end

        // 1. create an isolated branch for this expression
        NfaState* begin = builder.NewState();
        NfaState* end = begin;
        for (auto child : Children())
        {
            auto new_end = builder.NewState();
            child->ConnectNfa(builder, { end, new_end });

            end = new_end;
        }

        // 2. connect branch created above to the one given
        //    with normal priority
        builder.NewEpsilonTransition({which.begin, begin}, EpsilonPriority::Normal);
        builder.NewEpsilonTransition({end, which.end}, EpsilonPriority::Normal);
    }

    void AlternationExpr::ConnectNfa(NfaBuilder& builder, NfaBranch which)
    {
        // path looks like:
        //
        //                ...
        //              /     \
        // which.begin -  ...  - which.end
        //              \     /
        //                ...

        for (auto child : Children())
        {
            auto nfa = CreateEvaluatedBranch(builder, *child);

            builder.NewEpsilonTransition({which.begin, nfa.begin}, EpsilonPriority::Normal);
            builder.NewEpsilonTransition({nfa.end, which.end}, EpsilonPriority::Normal);
        }
    }

	// TODO: optimize for [1, ...]
    void RepetitionExpr::ConnectNfa(NfaBuilder& builder, NfaBranch which)
    {
		Repetition rep = Count();

        // evaluate child expression of repetition
        NfaBranch child_branch = builder.NewBranch();
        Child()->ConnectNfa(builder, child_branch);

        std::vector<NfaState*> nodes;
        nodes.push_back(child_branch.begin);
        nodes.push_back(child_branch.end);

        // repeat child for particular times by cloning the branch
		// [m, inf] repetition requires m branches
		// [m, n] requires n branches
		// NOTE one branch is pre-installed
		size_t ins_count = rep.GoInfinity() ? rep.Min() : rep.Max();
        for (auto i = 1u; i < ins_count; ++i)
        {
			NfaState *new_begin = nodes.back();
			NfaState *new_end = builder.NewState();
			builder.CloneBranch({ new_begin, new_end }, child_branch);

			nodes.push_back(new_end);
        }

        // calculate epsilon priority of edges for leaving or restarting
        // Greedy closures tend to stay at internal state, while Reluctant closures behave oppositely
        EpsilonPriority leaving_tendency, staying_tendency;
        if (Strategy() == ClosureStrategy::Greedy)
        {
            leaving_tendency = EpsilonPriority::Low;
            staying_tendency = EpsilonPriority::High;
        }
        else // closure.strategy == ClosureStrategy::Reluctant
        {
            leaving_tendency = EpsilonPriority::High;
            staying_tendency = EpsilonPriority::Low;
        }

        if (rep.GoInfinity())
        {
			NfaState* last_begin = nodes[nodes.size() - 2];
			NfaState* last_end = nodes.back();

			// remove leaving transition if at least one repetition is needed
			// doing this saves a branch
			if (rep.Min() == 0)
			{
				builder.NewEpsilonTransition({ last_begin, last_end }, leaving_tendency);
			}

			builder.NewEpsilonTransition({ last_end, last_begin }, staying_tendency);
        }
        else
        {
			for (size_t i = rep.Min(); i < rep.Max(); ++i)
			{
				builder.NewEpsilonTransition({ nodes[i], nodes.back() }, leaving_tendency);
			}
        }

        builder.NewEpsilonTransition({ which.begin, nodes.front() }, EpsilonPriority::Normal);
        builder.NewEpsilonTransition({ nodes.back(), which.end }, leaving_tendency);
    }
    
    void AnchorExpr::ConnectNfa(NfaBuilder& builder, NfaBranch which)
    {
        builder.NewAnchorTransition(which, type_);
    }

    void CaptureExpr::ConnectNfa(NfaBuilder& builder, NfaBranch which)
    {
        auto inner_branch = builder.NewBranch(false);
        expr_->ConnectNfa(builder, inner_branch);

        builder.NewBeginCaptureTransition(NfaBranch{ which.begin, inner_branch.begin }, id_);
        builder.NewEndCaptureTransition(NfaBranch{ inner_branch.end, which.end });
    }

    void ReferenceExpr::ConnectNfa(NfaBuilder& builder, NfaBranch which)
    {
        builder.NewReferenceTransition(which, id_);
    }

    // DEBUG
    //

    void EntityExpr::Print(size_t ident)
    {
        PrintIdent(ident);
        printf("EntityExpr{ %c-%c }\n", range_.Min(), range_.Max());
    }

    void ConcatenationExpr::Print(size_t ident)
    {
        PrintIdent(ident);
        printf("ConcatenationExpr\n");
        for (auto child : seq_)
        {
            child->Print(ident + 2);
        }
    }

    void AlternationExpr::Print(size_t ident)
    {
        PrintIdent(ident);
        printf("AlternationExpr\n");
        for (auto child : any_)
        {
            child->Print(ident + 2);
        }
    }

    void RepetitionExpr::Print(size_t ident)
    {
        PrintIdent(ident);
        auto strategy = strategy_ == ClosureStrategy::Greedy ? "GREEDY" : "RELECTANT";
        printf("RepetitionExpr{ %d-%d %s}\n", rep_.Min(), rep_.Max(), strategy);

        child_->Print(ident + 2);
    }

    void AnchorExpr::Print(size_t ident)
    {
        PrintIdent(ident);
        printf("AnchorExpr{%s}\n", ToString(type_).c_str());
    }

    void CaptureExpr::Print(size_t ident)
    {
        PrintIdent(ident);
        printf("CaptureExpr{%d}\n", id_);

        expr_->Print(ident + 2);
    }

    void ReferenceExpr::Print(size_t ident)
    {
        PrintIdent(ident);
        printf("ReferenceExpr{%d}\n", id_);
    }
}