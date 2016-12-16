#include "grammar.hh"
#include "ast.hh"

namespace Parser {
	template<typename T>
	class EpilogParser: public pegmatite::ASTParserDelegate {
		BindAST<AST::Clauses> clauses = EpilogGrammar::get().clauses;
		BindAST<AST::Identifier> identifier = EpilogGrammar::get().identifier;
		BindAST<AST::VariableIdentifier> variableIdentifier = EpilogGrammar::get().variableIdentifier;
		BindAST<AST::Atom> atom = EpilogGrammar::get().atom;
		BindAST<AST::Variable> variable = EpilogGrammar::get().variable;
		BindAST<AST::Number> number = EpilogGrammar::get().number;
		BindAST<AST::ParameterList> parameterList = EpilogGrammar::get().parameters;
		BindAST<AST::Fact> fact = EpilogGrammar::get().fact;
		BindAST<AST::Rule> rule = EpilogGrammar::get().rule;
		public:
		const EpilogGrammar& grammar = EpilogGrammar::get();
	};
}
