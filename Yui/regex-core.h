#pragma once
#include <cassert>

namespace yui
{
	// Basic Definition
	//

	enum class EpsilonPriority
	{
		Low,
		Normal,
		High,
	};

	enum class AnchorType
	{
		Circumflex,
		Dollar,
	};

	enum class AssertionType
	{
		PositiveLookAhead,
		NegativeLookAhead,
		PositiveLookBehind,
		NegativeLookBehind
	};

	enum class ClosureStrategy
	{
		Greedy,
		Reluctant,
	};

	// TODO: make max exclusive
	// [min, max]
	class CharRange
	{
	public:
		CharRange(int min, int max)
			: min_(min), max_(max)
		{
			assert(min <= max);
		}

		int Min() const { return min_; }
		int Max() const { return max_; }

		size_t Length() const { return max_ - min_; }

		bool Contain(int ch) const { return ch >= min_ && ch <= max_; }
		bool Contain(CharRange range) const { return Contain(range.min_) && Contain(range.max_); }

	private:
		int min_, max_;
	};

	// [min, max]
	class Repetition
	{
	public:
		// Constructs a Reptition in a specific range
		// NOTES if max is too large, it may be recognized as infinity
		Repetition(size_t min, size_t max)
			: min_(min), max_(max)
		{
			assert(min <= max && max > 0);
		}

		// Constructs a Reptition that goes infinity
		Repetition(size_t min)
			: Repetition(min, kInfinityThreshold+1) { }

		size_t Min() const { return min_; }
		size_t Max() const { return max_; }

		bool GoInfinity() const { return max_ > kInfinityThreshold; }

	public:
		static constexpr size_t kInfinityThreshold = 1000;

	private:
		size_t min_, max_;
	};
}