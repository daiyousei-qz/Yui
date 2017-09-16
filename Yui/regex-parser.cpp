#include "regex-parser.h"
#include "regex-factory.h"

using namespace std;

namespace yui
{
	class RegexParser : public RegexFactoryBase
	{
	public:
		RegexParser(const string& regex)
			: regex_(regex) { }

	protected:
		RegexExpr* Construct() override
		{
			// TODO: finish parsing
			throw 0;
		}

	private:
		const string regex_;
	};

	ManagedRegex::Ptr Parse(const std::string& regex)
	{
		return RegexParser{ regex }.Generate();
	}


}