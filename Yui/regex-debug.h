#pragma once
#include "regex-automaton.h"
#include <string>

namespace yui
{
    void PrintIdent(size_t ident);

    std::string ToString(EpsilonPriority priority);
    std::string ToString(AnchorType anchor);

    void PrintNfa(const NfaAutomaton &atm);
    void PrintDfa(const DfaAutomaton &atm);
}