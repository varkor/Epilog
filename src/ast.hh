#include <functional>
#include <iostream>
#include <unordered_set>
#include "../lib/Pegmatite/ast.hh"
#include "runtime.hh"
#include "interpreter.hh"

namespace Compiler {
	class Context;
}

namespace llvm {
	class Value;
}

namespace AST {
	using pegmatite::ASTPtr;
	using pegmatite::ASTChild;
	using pegmatite::ASTList;
	using pegmatite::ErrorReporter;
	
	// Abstract superclass for all clauses.
	class Clause: public pegmatite::ASTContainer {
		public:
		virtual void interpret(Interpreter::Context& context) = 0;
	};
	
	// A collection of clauses.
	class Clauses: public pegmatite::ASTContainer {
		pegmatite::ASTList<Clause> clauses;
		public:
		virtual void interpret(Interpreter::Context& context);
	};
	
	class Identifier: public pegmatite::ASTString { };
	
	class VariableIdentifier: public pegmatite::ASTString { };
	
	class Value: public pegmatite::ASTContainer {
		public:
		virtual std::string getName() const = 0;
	};
	
	class Atom: public Value {
		pegmatite::ASTChild<Identifier> name;
		
		public:
		std::string getName() const override {
			return name;
		}
	};
	
	class Variable: public Value {
		pegmatite::ASTChild<VariableIdentifier> name;
		
		public:
		std::string getName() const override {
			return name;
		}
	};
	
	// Number literal.
	class Number: public Value {
		int64_t value;
		
		public:
		std::string getName() const override {
			return std::to_string(value);
		}
		
		bool construct(const pegmatite::InputRange &range, pegmatite::ASTStack &stack, const ErrorReporter&) override {
			pegmatite::constructValue(range, value);
			return true;
		}
	};
	
	class ParameterList: public pegmatite::ASTContainer {
		public:
		pegmatite::ASTList<Value> parameters;
	};
	
	class BaseClause: public Clause {
		pegmatite::ASTChild<Identifier> name;
		// As `fact.` is treated equivalently to `fact().`, both will simply have empty parameter lists.
		pegmatite::ASTPtr<ParameterList> parameterList;
		
		public:
		void interpret(Interpreter::Context& context) override;
	};
	
	class Rule: public Clause {
		pegmatite::ASTChild<Identifier> name;
		// As `fact.` is treated equivalently to `fact().`, both will simply have empty parameter lists.
		pegmatite::ASTPtr<ParameterList> parameterList;
		
		// Required now for the parsing to succeed, though this will be generalised in the future.
		pegmatite::ASTChild<Identifier> conditionName;
		pegmatite::ASTPtr<ParameterList> conditionParameterList;
		
		public:
		void interpret(Interpreter::Context& context) override;
	};
}
