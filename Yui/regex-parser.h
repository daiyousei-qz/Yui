// Provides with text-to-regex construction function

#pragma once
#include "regex-expr.h"
#include <string>

namespace yui
{
    // internal machanics should be an explicit automaton
    ManagedRegex::Ptr Parse(const std::string& regex);
}