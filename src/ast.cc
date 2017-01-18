#include "parser.hh"
#include "Pegmatite/parser.cc"

namespace Epilog {
	namespace AST {
		bool Identifier::construct(const pegmatite::InputRange& range, pegmatite::ASTStack& stack, const pegmatite::ErrorReporter& errorReporter) {
			if (pegmatite::ASTString::construct(range, stack, errorReporter)) {
				if (length() > 2 && (*this)[0] == '\'') {
					std::string unquoted = substr(1, length() - 2);
					pegmatite::StringInput input(unquoted);
					Parser::EpilogParser parser;
					pegmatite::ParserDelegate* delegate = reinterpret_cast<pegmatite::ParserDelegate*>(&parser);
					pegmatite::Context context(input, Parser::EpilogGrammar::get().whitespace, *delegate);
					if (context.parse_non_term(Parser::EpilogGrammar::get().simpleIdentifier)) {
						this->std::string::operator=(unquoted);
					}
					context.clear_cache();
				}
				return true;
			}
			return false;
		}
	}
}
