#include <iostream>
#include <list>
#include <unordered_set>
#include "Pegmatite/ast.hh"
#include "runtime.hh"
#include "interpreter.hh"

namespace Epilog {
	namespace Parser {
		class EpilogParser;
	}
	
	namespace AST {
		// Abstract superclass for all terms.
		class Term: public pegmatite::ASTContainer {
			public:
			virtual std::string toString() const = 0;
		};
		
		class Identifier: public pegmatite::ASTString {
			public:
			bool construct(const pegmatite::InputRange& range, pegmatite::ASTStack& stack, const pegmatite::ErrorReporter& errorReporter) override;
		};
		
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
				for (auto& parameter : parameterList->parameters) {
					parameters += (!first ? "," : (first = false, "")) + parameter->toString();
				}
				return name + "/" + std::to_string(parameterList->parameters.size()) + (parameterList->parameters.size() > 0 ? "(" + parameters + ")" : "");
			}
		};
		
		class Body: public pegmatite::ASTContainer {
			public:
			pegmatite::ASTList<CompoundTerm> goals;
			
			std::string toString() const {
				std::string compoundTerms;
				bool first = true;
				for (auto& goal : goals) {
					compoundTerms += (!first ? "," : (first = false, "")) + goal->toString();
				}
				return compoundTerms;
			}
		};
		
		class Clause: public pegmatite::ASTContainer {
			public:
			virtual void interpret(Interpreter::Context& context) = 0;
		};
		
		// A collection of clauses.
		class Clauses: public pegmatite::ASTContainer {
			pegmatite::ASTList<Clause> clauses;
			
			public:
			void interpret(Interpreter::Context& context);
		};
		
		class Variable: public Term {
			pegmatite::ASTChild<VariableIdentifier> name;
			
			public:
			std::string toString() const override {
				return name;
			}
		};
		
		// List literal.
		class ElementList: public pegmatite::ASTContainer {
			public:
			pegmatite::ASTList<Term> elements;
		};
		
		class List: public Term {
			public:
			pegmatite::ASTPtr<ElementList> elementList;
			pegmatite::ASTPtr<Term, true> tail;
			
			std::string toString() const override {
				return "<temporary list literal>";
			}
		};
		
		// Number literal.
		class Number: public Term {
			public:
			int64_t value;
			
			std::string toString() const override {
				return std::to_string(value);
			}
			
			bool construct(const pegmatite::InputRange &range, pegmatite::ASTStack &stack, const pegmatite::ErrorReporter&) override {
				pegmatite::constructValue(range, value);
				return true;
			}
		};
		
		class Fact: public Clause {
			pegmatite::ASTPtr<CompoundTerm> head;
			
			public:
			void interpret(Interpreter::Context& context) override;
		};
		
		class Rule: public Clause {
			pegmatite::ASTPtr<CompoundTerm> head;
			pegmatite::ASTPtr<Body> body;
			
			public:
			void interpret(Interpreter::Context& context) override;
		};
		
		class Query: public Clause {
			pegmatite::ASTPtr<Body> body;
			
			public:
			void interpret(Interpreter::Context& context) override;
		};
	}
}
