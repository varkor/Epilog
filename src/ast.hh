#include "../lib/Pegmatite/ast.hh"
#include "runtime.hh"
#include "interpreter.hh"
#include <unordered_set>
#include <functional>

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
	class Statement: public pegmatite::ASTContainer {
		public:
		virtual void interpret(Interpreter::Context& content) = 0;
	};
	
	// Abstract superclass for all expressions (statements that evaluate to a value).
	template<typename T>
	class Expression: public pegmatite::ASTContainer {
		public:
		void interpret(Interpreter::Context& content) {
			std::cerr << "Cannot interpret any expressions yet." << std::endl;
		}
		virtual T evaluate() const = 0;
	};
	
	// Number literal.
	template<typename T>
	class Number: public Expression<T> {
		T value;
		
		public:
		T evaluate() const override {
			return value;
		}
		
		bool construct(const pegmatite::InputRange &range, pegmatite::ASTStack &stack, const ErrorReporter&) override {
			pegmatite::constructValue(range, value);
			return true;
		}
	};
	
	// Abstract superclass for binary operators.
	template<typename T, class Operator>
	class BinaryOperator: public Expression<T> {
		ASTPtr<Expression<T>> left, right;
		
		public:
		T evaluate() const override {
			Operator op;
			return op(left->evaluate(), right->evaluate());
		}
	};
}
