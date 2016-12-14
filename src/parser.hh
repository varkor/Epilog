#include "grammar.hh"
#include "ast.hh"

namespace Parser {
	template<typename T>
	class EpilogParser: public pegmatite::ASTParserDelegate {
		BindAST<AST::Clauses> clauses = EpilogGrammar::get().clauses;
		BindAST<AST::Identifier> identifier = EpilogGrammar::get().identifier;
		BindAST<AST::ParameterList> parameterList = EpilogGrammar::get().parameters;
		BindAST<AST::BaseClause> baseClause = EpilogGrammar::get().baseClause;
		BindAST<AST::Number> number = EpilogGrammar::get().number;
		BindAST<AST::BinaryOperator<std::multiplies<int>>> multiply = EpilogGrammar::get().multiply;
		public:
		const EpilogGrammar& grammar = EpilogGrammar::get();
	};
}
