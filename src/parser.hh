#include "grammar.hh"
#include "ast.hh"

namespace Parser {
	template<typename T>
	class EpilogParser: public pegmatite::ASTParserDelegate {
		BindAST<AST::Number<T>> number = EpilogGrammar::get().number;
		BindAST<AST::BinaryOperator<T, std::multiplies<T>>> multiply = EpilogGrammar::get().multiply;
		public:
		const EpilogGrammar& grammar = EpilogGrammar::get();
	};
}
