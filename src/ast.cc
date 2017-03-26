#include "parser.hh"
#include "Pegmatite/parser.cc"

namespace Epilog {
	namespace AST {
		uint64_t anonymousIndex = 0;
		
		std::string normaliseIdentifierName(std::string name) {
			std::string& normalised = name;
			if (name.length() > 2 && name[0] == '\'') {
				// Atoms / Functors.
				std::string unquoted = name.substr(1, name.length() - 2);
				pegmatite::StringInput input(unquoted);
				Parser::EpilogParser parser;
				pegmatite::ParserDelegate* delegate = reinterpret_cast<pegmatite::ParserDelegate*>(&parser);
				pegmatite::Context context(input, Parser::EpilogGrammar::get().whitespace, *delegate);
				if (context.parse_non_term(Parser::EpilogGrammar::get().simpleIdentifier)) {
					normalised = unquoted;
				}
				context.clear_cache();
			} else if (name.length() == 1 && name[0] == '_') {
				// Variables.
				std::string unique = "_" + std::to_string(anonymousIndex ++);
				normalised = unique;
			}
			return normalised;
		}
		
		bool Identifier::construct(const pegmatite::InputRange& range, pegmatite::ASTStack& stack, const pegmatite::ErrorReporter& errorReporter) {
			if (pegmatite::ASTString::construct(range, stack, errorReporter)) {
				this->std::string::operator=(normaliseIdentifierName(*this));
				return true;
			}
			return false;
		}
		
		bool VariableIdentifier::construct(const pegmatite::InputRange& range, pegmatite::ASTStack& stack, const pegmatite::ErrorReporter& errorReporter) {
			if (pegmatite::ASTString::construct(range, stack, errorReporter)) {
				this->std::string::operator=(normaliseIdentifierName(*this));
				return true;
			}
			return false;
		}
	}
}
