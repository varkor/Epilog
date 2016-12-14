#include "parser.hh"
#include "runtime.hh"

namespace AST {
	void Clauses::interpret(Interpreter::Context& context) {
		for (auto& clause : clauses) {
			clause->interpret(context);
		}
	}
	
	void BaseClause::interpret(Interpreter::Context& context) {
		Epilog::BaseClause* baseClause = new Epilog::BaseClause();
		std::stringstream parameters;
		bool first = true;
		for (auto& parameter : (*parameterList).parameters) {
			parameters << (!first ? "," : (first = false, "")) << *parameter;
		}
		std::cerr << "Register clause: " << name << "/" + std::to_string((*parameterList).parameters.size()) + "(" << parameters.str() << ")" << std::endl;
		std::string& clauseName = name;
		baseClause->name = strdup(clauseName.c_str());
		Epilog::registerClause(clauseName, baseClause);
	}
}
