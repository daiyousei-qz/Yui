#include "regex-debug.h"
#include "regex-automaton.h"
#include <functional>

using namespace std;

namespace yui
{
    void PrintIdent(size_t ident)
    {
        for (size_t i = 0; i < ident; ++i)
            putchar(' ');
    }

    std::string ToString(EpsilonPriority priority)
    {
        switch (priority)
        {
        case EpsilonPriority::Low:
            return "Low";
        case EpsilonPriority::Normal:
            return "Normal";
        case EpsilonPriority::High:
            return "High";
        }
    }

    std::string ToString(AnchorType anchor)
    {
        return anchor == AnchorType::LineBreak ? "$" : "^";
    }

    void PrintNfa(const NfaAutomaton& atm)
    {
        int next_id = 0;
        std::unordered_map<const NfaState*, int> id_map;
        id_map.insert_or_assign(atm.IntialState(), next_id++);

        EnumerateNfa(atm.IntialState(),
            [&](const NfaState *source)
        {
            int source_id = id_map[source];

            // print title
            printf("NfaState %d", source_id);
            if (source->is_checkpoint)
            {
                printf("[checkpoint]");
            }
            if (source->is_final)
            {
                printf("(final)");
            }

            printf(":\n");

            // foreach outgoing edges
            for (const NfaTransition *edge : source->exits)
            {
                printf("  ");

                // print type of the edge
                switch (edge->type)
                {
                case TransitionType::Epsilon:
                {
                    auto priority = ToString(std::get<EpsilonPriority>(edge->data));

                    printf("Epsilon(%s)", priority.c_str());
                }
                break;
                case TransitionType::Entity:
                {
                    auto rg = std::get<CharRange>(edge->data);
                    printf("Codepoint(%c, %c)", rg.Min(), rg.Max());
                }
                break;
                case TransitionType::Anchor:
                    printf("Anchor(%s)", ToString(std::get<AnchorType>(edge->data)).c_str());
                    break;
                case TransitionType::BeginCapture:
                    printf("Capture(%d)", std::get<unsigned>(edge->data));
                    break;
                case TransitionType::Reference:
                    printf("Reference(%d)", std::get<unsigned>(edge->data));
                    break;
                case TransitionType::Assertion:
                    printf("Assertion");
                    {
                        switch (std::get<AssertionType>(edge->data))
                        {
                        case AssertionType::PositiveLookAhead:
                            printf("(PositiveLookAhead)");
                            break;
                        case AssertionType::NegativeLookAhead:
                            printf("(NegativeLookAhead)");
                            break;
                        case AssertionType::PositiveLookBehind:
                            printf("(PositiveLookBehid)");
                            break;
                        case AssertionType::NegativeLookBehind:
                            printf("(NegativeLookBehind)");
                            break;
                        default:
                            break;
                        }
                        printf("\n");
                    }
                    break;
                case TransitionType::EndCapture:
                    printf("(finish)");
                    break;
                }

                // print target id, generate one if needed
                int target_id;
                auto target_iter = id_map.find(edge->target);
                if (target_iter == id_map.end())
                {
                    target_id = next_id++;
                    id_map.insert_or_assign(edge->target, target_id);
                }
                else
                {
                    target_id = target_iter->second;
                }

                printf("  => NfaState %d\n", target_id);

            }
        });
    }

    void PrintDfa(const DfaAutomaton& atm)
    {
        for (DfaState s = 0; s < atm.StateCount(); ++s)
        {
            auto accepting_flag = atm.IsAccepting(s) ? "(final)" : "";
            printf("DfaState %d%s:\n", s, accepting_flag);
            
            for (int ch = 0; ch < 128; ++ch)
            {
                auto target = atm.Transit(s, ch);
                if (target != kInvalidDfaState)
                {
                    printf("  char of %c --> DfaState %d\n", ch, target);
                }
            }
        }
    }
}