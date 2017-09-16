// This is how a regular expression object is exposed to the end users
// though users may never use use this as they would construct a matcher anyway

#pragma once
#include "regex-expr.h"
#include "arena.hpp"
#include <memory>

namespace yui
{
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
}