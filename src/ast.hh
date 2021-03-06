#include <iostream>
#include <list>
#include <unordered_set>
#include "Pegmatite/ast.hh"
#include "runtime.hh"
#include "interpreter.hh"

#define POLYMORPHIC(class) \
	virtual ~class() = default;\
	class() = default;\
	class(const class&) = default;\
	class& operator=(const class& other) = default;

namespace Epilog {
	namespace Parser {
		class EpilogParser;
	}
	
	namespace AST {
		class Term;
		
		struct TermNode {
			Term* term;
			std::shared_ptr<TermNode> parent;
			HeapReference reg;
			std::string name;
			std::string symbol;
			int64_t value = 0;
			std::vector<std::shared_ptr<TermNode>> children;
			
			TermNode(Term* term, std::shared_ptr<TermNode> parent) : term(term), parent(parent) { }
		};
		
		struct Printable {
			POLYMORPHIC(Printable)
			
			virtual std::string toString() const = 0;
		};
		
		// Abstract superclass for all terms.
		class Term: public pegmatite::ASTContainer, public Printable { };
		
		// Dynamic terms are terms that might resolve to terms of different types (for example: compound terms, or numbers) each time they are evaluated. This is used to enable certain runtime modifications to clauses.
		class DynamicTerm: public Term {
			static int64_t dynamicID;
			
			public:
			std::string name;
			const std::string symbol;
			bool usesRegister;
			
			virtual std::list<Instruction*> instructions(std::shared_ptr<TermNode> node, std::unordered_map<std::string, HeapReference>& allocations, bool dependentAllocations, bool argumentTerm) const = 0;
			
			DynamicTerm(std::string name, bool usesRegister = true) : name(name), symbol(name + ":" + std::to_string(DynamicTerm::dynamicID ++)), usesRegister(usesRegister) {
				
			}
		};
		
		std::string normaliseIdentifierName(std::string);
		
		class Identifier: public pegmatite::ASTString {
			public:
			bool construct(const pegmatite::InputRange& range, pegmatite::ASTStack& stack, const pegmatite::ErrorReporter& errorReporter) override;
		};
		
		class VariableIdentifier: public pegmatite::ASTString {
			public:
			bool construct(const pegmatite::InputRange& range, pegmatite::ASTStack& stack, const pegmatite::ErrorReporter& errorReporter) override;
		};
		
		class ParameterList: public pegmatite::ASTContainer {
			public:
			pegmatite::ASTList<Term> parameters;
		};
		
		class Modifier: public pegmatite::ASTString { };
		
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
		
		class EnrichedCompoundTerm: public pegmatite::ASTContainer, public Printable {
			public:
			pegmatite::ASTPtr<Modifier, true> modifier;
			pegmatite::ASTPtr<CompoundTerm> compoundTerm;
			
			std::string toString() const override {
				return (modifier != nullptr ? *modifier : std::string()) + compoundTerm->toString();
			}
		};
		
		class Body: public pegmatite::ASTContainer, public Printable {
			public:
			pegmatite::ASTList<EnrichedCompoundTerm> goals;
			
			std::string toString() const override {
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
		
		// String literal.
		class StringContent: public pegmatite::ASTString { };
		
		class String: public Term {
			public:
			pegmatite::ASTChild<StringContent> text;
			
			std::string toString() const override {
				return "<temporary string literal>";
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
			public:
			pegmatite::ASTPtr<Body> body;
			void interpret(Interpreter::Context& context) override;
		};
	}
}
