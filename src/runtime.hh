#include <iomanip>
#include <unordered_map>

#pragma once

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define polymorphic(class) \
	virtual ~class() = default;\
	class() = default;\
	class(const class&) = default;\
	class& operator=(const class& other) = default;

namespace Epilog {
	class RuntimeException {
		public:
		std::string message;
		std::string file;
		std::string function;
		int line;
		
		void print() const {
			std::cerr << file << " > " << function << "() (L" << line << "): " << message << std::endl;
		}
		
		RuntimeException(std::string message, std::string file, std::string function, int line) : message(message), file(file), function(function), line(line) { }
	};
	
	class UnificationError: public RuntimeException {
		public:
		UnificationError(std::string message, std::string file, std::string function, int line) : RuntimeException(message, file, function, line) { }
	};
	
	enum Mode { read, write };
	
	struct HeapContainer {
		// Ensure HeapContainer is polymorphic, so that we can use dynamic_cast
		polymorphic(HeapContainer);
		
		typedef typename std::vector<std::unique_ptr<HeapContainer>>::size_type heapIndex;
		
		virtual std::unique_ptr<HeapContainer> copy() const = 0;
		
		virtual std::string toString() const {
			return "container";
		}
		
		virtual std::string trace() const = 0;
	};
	
	struct HeapTuple: HeapContainer {
		enum class Type { compoundTerm, reference };
		Type type;
		heapIndex reference;
		
		virtual std::unique_ptr<HeapContainer> copy() const override {
			return std::unique_ptr<HeapTuple>(new HeapTuple(*this));
		}
		
		virtual std::string toString() const override {
			return std::string("(") + (type == Type::compoundTerm ? "compound term" : "reference") + ", " + std::to_string(reference) + ")";
		}
		
		virtual std::string trace() const override;
		
		HeapTuple(Type type, heapIndex reference) : type(type), reference(reference) { }
	};
	
	struct HeapFunctor: HeapContainer {
		std::string name;
		int parameters;
		
		virtual std::unique_ptr<HeapContainer> copy() const override {
			return std::unique_ptr<HeapFunctor>(new HeapFunctor(*this));
		}
		
		virtual std::string toString() const override {
			return name + "/" + std::to_string(parameters);
		}
		
		virtual std::string trace() const override {
			return name;
		}
		
		HeapFunctor(std::string name, int parameters) : name(name), parameters(parameters) { }
	};
	
	void pushCompoundTerm(const HeapFunctor& functor, std::unique_ptr<HeapContainer>& reg);
	
	void pushVariable(std::unique_ptr<HeapContainer>& reg);
	
	void pushValue(std::unique_ptr<HeapContainer>& reg);
	
	void unifyStructure(const HeapFunctor& functor, std::unique_ptr<HeapContainer>& reg);
	
	void unifyVariable(std::unique_ptr<HeapContainer>& reg);
	
	void unifyValue(std::unique_ptr<HeapContainer>& reg);
	
	HeapContainer::heapIndex dereference(std::unique_ptr<HeapContainer>& container, HeapContainer::heapIndex index);
	
	HeapContainer::heapIndex dereference(HeapContainer::heapIndex index);
	
	template <class T>
	class BoundsCheckedVector: public std::vector<std::unique_ptr<T>> {
		public:
		std::unique_ptr<T>& operator[] (const typename std::vector<std::unique_ptr<T>>::size_type index) {
			if (index >= this->size()) {
				throw RuntimeException("Tried to access a vector index out of bounds.", __FILENAME__, __func__, __LINE__);
			}
			return std::vector<std::unique_ptr<T>>::operator[](index);
		}
	};
	
	class StackHeap: public BoundsCheckedVector<HeapContainer> {
		public:
		void print() {
			for (HeapContainer::heapIndex i = 0; i < size(); ++ i) {
				std::cerr << std::setw(2) << i << ": " << ((*this)[i] != nullptr ? (*this)[i]->toString() : "null") << std::endl;
			}
		}
	};
	
	struct Instruction {
		polymorphic(Instruction);
		
		virtual void execute() = 0;
		
		virtual std::string toString() const = 0;
	};
	
	class Runtime {
		public:
		// The global stack used to contain term structures used when unifying.
		static StackHeap stack;
		
		// The registers used to temporarily hold pointers when building queries or rules
		static StackHeap registers;
		
		// The instructions corresponding to the compiled program
		static BoundsCheckedVector<Instruction> instructions;
		
		// Labels with which a particular instruction can be jumped to
		static std::unordered_map<std::string, BoundsCheckedVector<Instruction>::size_type> labels;
		
		static BoundsCheckedVector<Instruction>::size_type nextInstruction;
		
		static Mode mode;
		
		static HeapContainer::heapIndex unificationIndex;
	};
	
	struct PushCompoundTermInstruction: Instruction {
		const HeapFunctor functor;
		int registerIndex;
		
		PushCompoundTermInstruction(const HeapFunctor functor, int registerIndex) : functor(functor), registerIndex(registerIndex) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "put_structure " + functor.name + "/" + std::to_string(functor.parameters) + ", R" + std::to_string(registerIndex);
		}
	};
	
	struct PushVariableInstruction: Instruction {
		int registerIndex;
		
		PushVariableInstruction(int registerIndex) : registerIndex(registerIndex) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "set_variable R" + std::to_string(registerIndex);
		}
	};
	
	struct PushValueInstruction: Instruction {
		int registerIndex;
		
		PushValueInstruction(int registerIndex) : registerIndex(registerIndex) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "set_value R" + std::to_string(registerIndex);
		}
	};
	
	struct UnifyCompoundTermInstruction: Instruction {
		const HeapFunctor functor;
		int registerIndex;
		
		UnifyCompoundTermInstruction(const HeapFunctor functor, int registerIndex) : functor(functor), registerIndex(registerIndex) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "get_structure " + functor.name + "/" + std::to_string(functor.parameters) + ", R" + std::to_string(registerIndex);
		}
	};
	
	struct UnifyVariableInstruction: Instruction {
		int registerIndex;
		
		UnifyVariableInstruction(int registerIndex) : registerIndex(registerIndex) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "unify_variable R" + std::to_string(registerIndex);
		}
	};
	
	struct UnifyValueInstruction: Instruction {
		int registerIndex;
		
		UnifyValueInstruction(int registerIndex) : registerIndex(registerIndex) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "unify_value R" + std::to_string(registerIndex);
		}
	};
	
	struct PushVariableToAllInstruction: Instruction {
		int registerIndex;
		int argumentIndex;
		
		PushVariableToAllInstruction(int registerIndex, int argumentIndex) : registerIndex(registerIndex), argumentIndex(argumentIndex) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "put_variable R" + std::to_string(registerIndex) + ", R" + std::to_string(argumentIndex);
		}
	};
	
	struct CopyRegisterToArgumentInstruction: Instruction {
		int registerIndex;
		int argumentIndex;
		
		CopyRegisterToArgumentInstruction(int registerIndex, int argumentIndex) : registerIndex(registerIndex), argumentIndex(argumentIndex) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "put_value R" + std::to_string(registerIndex) + ", R" + std::to_string(argumentIndex);
		}
	};
	
	struct CopyArgumentToRegisterInstruction: Instruction {
		int registerIndex;
		int argumentIndex;
		
		CopyArgumentToRegisterInstruction(int registerIndex, int argumentIndex) : registerIndex(registerIndex), argumentIndex(argumentIndex) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "get_variable R" + std::to_string(registerIndex) + ", R" + std::to_string(argumentIndex);
		}
	};
	
	struct UnifyRegisterAndArgumentInstruction: Instruction {
		int registerIndex;
		int argumentIndex;
		
		UnifyRegisterAndArgumentInstruction(int registerIndex, int argumentIndex) : registerIndex(registerIndex), argumentIndex(argumentIndex) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "get_value R" + std::to_string(registerIndex) + ", R" + std::to_string(argumentIndex);
		}
	};
	
	struct CallInstruction: Instruction {
		HeapFunctor functor;
		
		CallInstruction(const HeapFunctor functor) : functor(functor) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "call " + functor.name + "/" + std::to_string(functor.parameters);
		}
	};
	
	struct ProceedInstruction: Instruction {
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "proceed";
		}
	};
}
