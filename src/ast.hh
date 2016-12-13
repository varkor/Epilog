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
	
	// Abstract superclass for all statements.
	class Clause: public pegmatite::ASTContainer {
		public:
		virtual void interpret(Interpreter::Context& context) = 0;
	};
	
	class Clauses: public pegmatite::ASTContainer {
		pegmatite::ASTList<Clause> clauses;
		public:
		virtual void interpret(Interpreter::Context& context);
	};
	
	// Abstract superclass for all expressions (statements that evaluate to a value).
	class Expression: public Clause {
		public:
		void interpret(Interpreter::Context& context) {
			std::cout << evaluate() << std::endl;
		}
		virtual int64_t evaluate() const = 0;
	};
	
	// Number literal.
	class Number: public Expression {
		int64_t value;
		
		public:
		int64_t evaluate() const override {
			return value;
		}
		
		bool construct(const pegmatite::InputRange &range, pegmatite::ASTStack &stack, const ErrorReporter&) override {
			pegmatite::constructValue(range, value);
			return true;
		}
	};
	
	// Abstract superclass for binary operators.
	template<class Operator>
	class BinaryOperator: public Expression {
		ASTPtr<Expression> left, right;
		
		public:
		int64_t evaluate() const override {
			Operator op;
			return op(left->evaluate(), right->evaluate());
		}
	};
}
