#pragma once
#include "regex-automaton.h"
#include <string>

namespace yui
{
    void PrintIdent(size_t ident)
    {
        for (size_t i = 0; i < ident; ++i)
            putchar(' ');
    }

    void PrintNfa(const NfaAutomaton &atm);
    void PrintDfa(const DfaAutomaton &atm);

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
        return anchor == AnchorType::Dollar ? "$" : "^";
    }
}