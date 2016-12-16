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
	
	// Abstract superclass for all terms.
	class Term: public pegmatite::ASTContainer {
		public:
		virtual std::string toString() const = 0;
	};
	
	class Identifier: public pegmatite::ASTString { };
	
	class VariableIdentifier: public pegmatite::ASTString { };
	
	class ParameterList: public pegmatite::ASTContainer {
		public:
		pegmatite::ASTList<Term> parameters;
	};
	
	class CompoundTerm: public Term {
		public:
		pegmatite::ASTChild<Identifier> name;
		// As `fact.` is treated equivalently to `fact().`, both will simply have empty parameter lists.
		pegmatite::ASTPtr<ParameterList> parameterList;
		
		std::string toString() const override {
			std::string parameters;
			bool first = true;
			for (auto& parameter : (*parameterList).parameters) {
				parameters += (!first ? "," : (first = false, "")) + parameter->toString();
			}
			return name + "/" + std::to_string((*parameterList).parameters.size()) + "(" + parameters + ")";
		}
	};
	
	class Clause: public pegmatite::ASTContainer {
		protected:
		pegmatite::ASTPtr<CompoundTerm> head;
		
		public:
		virtual void interpret(Interpreter::Context& context) = 0;
	};
	
	// A collection of clauses.
	class Clauses: public pegmatite::ASTContainer {
		pegmatite::ASTList<Clause> clauses;
		public:
		virtual void interpret(Interpreter::Context& context);
	};
	
	class Variable: public Term {
		pegmatite::ASTChild<VariableIdentifier> name;
		
		public:
		std::string toString() const override {
			return name;
		}
	};
	
	// Number literal.
	class Number: public Term {
		int64_t value;
		
		public:
		std::string toString() const override {
			return std::to_string(value);
		}
		
		bool construct(const pegmatite::InputRange &range, pegmatite::ASTStack &stack, const ErrorReporter&) override {
			pegmatite::constructValue(range, value);
			return true;
		}
	};
	
	class Fact: public Clause {
		public:
		void interpret(Interpreter::Context& context) override;
	};
	
	class Rule: public Clause {
		// Required now for the parsing to succeed, though this will be generalised in the future.
		pegmatite::ASTPtr<CompoundTerm> body;
		
		public:
		void interpret(Interpreter::Context& context) override;
	};
}
