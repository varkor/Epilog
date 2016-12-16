#include "parser.hh"
#include "runtime.hh"

namespace AST {
	void Clauses::interpret(Interpreter::Context& context) {
		for (auto& clause : clauses) {
			clause->interpret(context);
		}
	}
	
	void Fact::interpret(Interpreter::Context& context) {
		Epilog::Fact* fact = new Epilog::Fact();
		std::cerr << "Register base clause: " << head->toString() << std::endl;
		std::string& clauseName = head->name;
		fact->name = strdup(clauseName.c_str());
		Epilog::registerClause(clauseName, fact);
	}
	
	void Rule::interpret(Interpreter::Context& context) {
		Epilog::Rule* rule = new Epilog::Rule();
		std::cerr << "Register rule: " << head->toString() << std::endl;
		std::string& clauseName = head->name;
		rule->name = strdup(clauseName.c_str());
		Epilog::registerClause(clauseName, rule);
	}
}
