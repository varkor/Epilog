#include <iomanip>
#include <stack>
#include <unordered_map>

#pragma once

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define POLYMORPHIC(class) \
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
		POLYMORPHIC(HeapContainer);
		
		virtual std::unique_ptr<HeapContainer> copy() const = 0;
		
		virtual std::string toString() const {
			return "container";
		}
		
		virtual std::string trace() const = 0;
	};
	
	enum class StorageArea { heap, reg, environment, undefined };
	
	struct HeapReference: public std::pair<StorageArea, std::vector<std::unique_ptr<HeapContainer>>::size_type> {
		typedef typename std::vector<std::unique_ptr<HeapContainer>>::size_type heapIndex;
		
		StorageArea area;
		heapIndex index;
		
		HeapReference(StorageArea area, heapIndex index) : area(area), index(index) { }
		
		HeapReference() : HeapReference(StorageArea::undefined, 0) { }
		
		bool operator==(const HeapReference& other) {
			return area == other.area && index == other.index;
		}
		
		bool operator!=(const HeapReference& other) {
			return !(*this == other);
		}
		
		std::unique_ptr<HeapContainer>& get() const;
		
		std::unique_ptr<HeapContainer> getAsCopy() const {
			return get()->copy();
		}
		
		HeapContainer* getPointer() const {
			return get().get();
		}
		
		void assign(std::unique_ptr<HeapContainer> value) const;
		
		std::string toString() const {
			std::string string;
			switch (area) {
				case StorageArea::heap:
					string += "H";
					break;
				case StorageArea::reg:
					string += "T";
					break;
				case StorageArea::environment:
					string += "P";
					break;
				case StorageArea::undefined:
					return string + "?";
			}
			return string + std::to_string(index);
		}
	};
	
	struct HeapTuple: HeapContainer {
		enum class Type { compoundTerm, reference };
		Type type;
		HeapReference::heapIndex reference;
		
		virtual std::unique_ptr<HeapContainer> copy() const override {
			return std::unique_ptr<HeapTuple>(new HeapTuple(*this));
		}
		
		virtual std::string toString() const override {
			return std::string("(") + (type == Type::compoundTerm ? "compound term" : "reference") + ", " + std::to_string(reference) + ")";
		}
		
		virtual std::string trace() const override;
		
		HeapTuple(Type type, HeapReference::heapIndex reference) : type(type), reference(reference) { }
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
	
	HeapReference::heapIndex dereference(std::unique_ptr<HeapContainer>& container, HeapReference::heapIndex index);
	
	HeapReference::heapIndex dereference(HeapReference::heapIndex index);
	
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
			for (HeapReference::heapIndex i = 0; i < size(); ++ i) {
				std::cerr << std::setw(2) << i << ": " << ((*this)[i] != nullptr ? (*this)[i]->toString() : "null") << std::endl;
			}
		}
	};
	
	struct Instruction {
		POLYMORPHIC(Instruction);
		
		virtual void execute() = 0;
		
		virtual std::string toString() const = 0;
	};
	
	struct Environment {
		BoundsCheckedVector<Instruction>::size_type nextGoal;
		StackHeap variables;
		
		Environment(BoundsCheckedVector<Instruction>::size_type nextGoal) : nextGoal(nextGoal) {}
	};
	
	class Runtime {
		public:
		// The global stack used to contain term structures used when unifying.
		static StackHeap heap;
		
		// The registers used to temporarily hold pointers when building queries or rules
		static StackHeap registers;
		
		// The instructions corresponding to the compiled program
		static BoundsCheckedVector<Instruction> instructions;
		
		// The environment stack used to store variable bindings
		static std::stack<std::unique_ptr<Environment>> environments;
		
		// Labels with which a particular instruction can be jumped to
		static std::unordered_map<std::string, BoundsCheckedVector<Instruction>::size_type> labels;
		
		static BoundsCheckedVector<Instruction>::size_type nextInstruction;
		
		static BoundsCheckedVector<Instruction>::size_type nextGoal;
		
		static Mode mode;
		
		static HeapReference::heapIndex unificationIndex;
	};
	
	struct PushCompoundTermInstruction: Instruction {
		const HeapFunctor functor;
		HeapReference registerReference;
		
		PushCompoundTermInstruction(const HeapFunctor functor, HeapReference registerReference) : functor(functor), registerReference(registerReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "put_structure " + functor.name + "/" + std::to_string(functor.parameters) + ", " + registerReference.toString();
		}
	};
	
	struct PushVariableInstruction: Instruction {
		HeapReference registerReference;
		
		PushVariableInstruction(HeapReference registerReference) : registerReference(registerReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "set_variable " + registerReference.toString();
		}
	};
	
	struct PushValueInstruction: Instruction {
		HeapReference registerReference;
		
		PushValueInstruction(HeapReference registerReference) : registerReference(registerReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "set_value " + registerReference.toString();
		}
	};
	
	struct UnifyCompoundTermInstruction: Instruction {
		const HeapFunctor functor;
		HeapReference registerReference;
		
		UnifyCompoundTermInstruction(const HeapFunctor functor, HeapReference registerReference) : functor(functor), registerReference(registerReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "get_structure " + functor.name + "/" + std::to_string(functor.parameters) + ", " + registerReference.toString();
		}
	};
	
	struct UnifyVariableInstruction: Instruction {
		HeapReference registerReference;
		
		UnifyVariableInstruction(HeapReference registerReference) : registerReference(registerReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "unify_variable " + registerReference.toString();
		}
	};
	
	struct UnifyValueInstruction: Instruction {
		HeapReference registerReference;
		
		UnifyValueInstruction(HeapReference registerReference) : registerReference(registerReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "unify_value " + registerReference.toString();
		}
	};
	
	struct PushVariableToAllInstruction: Instruction {
		HeapReference registerReference;
		HeapReference argumentReference;
		
		PushVariableToAllInstruction(HeapReference registerReference, HeapReference argumentReference) : registerReference(registerReference), argumentReference(argumentReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "put_variable " + registerReference.toString() + ", " + argumentReference.toString();
		}
	};
	
	struct CopyRegisterToArgumentInstruction: Instruction {
		HeapReference registerReference;
		HeapReference argumentReference;
		
		CopyRegisterToArgumentInstruction(HeapReference registerReference, HeapReference argumentReference) : registerReference(registerReference), argumentReference(argumentReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "put_value " + registerReference.toString() + ", " + argumentReference.toString();
		}
	};
	
	struct CopyArgumentToRegisterInstruction: Instruction {
		HeapReference registerReference;
		HeapReference argumentReference;
		
		CopyArgumentToRegisterInstruction(HeapReference registerReference, HeapReference argumentReference) : registerReference(registerReference), argumentReference(argumentReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "get_variable " + registerReference.toString() + ", " + argumentReference.toString();
		}
	};
	
	struct UnifyRegisterAndArgumentInstruction: Instruction {
		HeapReference registerReference;
		HeapReference argumentReference;
		
		UnifyRegisterAndArgumentInstruction(HeapReference registerReference, HeapReference argumentReference) : registerReference(registerReference), argumentReference(argumentReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "get_value " + registerReference.toString() + ", " + argumentReference.toString();
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
	
	struct AllocateInstruction: Instruction {
		int variables;
		
		AllocateInstruction(int variables) : variables(variables) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "allocate " + std::to_string(variables);
		}
	};
	
	struct DeallocateInstruction: Instruction {
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "deallocate";
		}
	};
}
