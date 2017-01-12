#include "grammar.hh"
#include "ast.hh"

namespace Epilog {
	namespace Parser {
		class EpilogParser: public pegmatite::ASTParserDelegate {
			BindAST<AST::Clauses> clauses = EpilogGrammar::get().clauses;
			BindAST<AST::Identifier> identifier = EpilogGrammar::get().identifier;
			BindAST<AST::VariableIdentifier> variableIdentifier = EpilogGrammar::get().variableIdentifier;
			BindAST<AST::Variable> variable = EpilogGrammar::get().variable;
			BindAST<AST::Number> number = EpilogGrammar::get().number;
			BindAST<AST::CompoundTerm> compoundTerm = EpilogGrammar::get().compoundTerm;
			BindAST<AST::Body> body = EpilogGrammar::get().compoundTerms;
			BindAST<AST::ParameterList> parameterList = EpilogGrammar::get().parameters;
			BindAST<AST::Fact> fact = EpilogGrammar::get().fact;
			BindAST<AST::Rule> rule = EpilogGrammar::get().rule;
			BindAST<AST::Query> query = EpilogGrammar::get().query;
			public:
			const EpilogGrammar& grammar = EpilogGrammar::get();
		};
	}
}
