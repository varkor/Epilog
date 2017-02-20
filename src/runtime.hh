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
	class Exception {
		protected:
		int64_t indentation;
		
		public:
		std::string message;
		std::string file;
		std::string function;
		int64_t line;
		
		void print() const {
			std::cerr << std::string(indentation, '\t') << file << " > " << function << "() (L" << line << "): " << message << std::endl;
		}
		
		Exception(std::string message, std::string file, std::string function, int64_t line, int64_t indentation = 0) : indentation(indentation), message(message), file(file), function(function), line(line) { }
	};
	
	class CompilationException: public Exception {
		public:
		CompilationException(std::string message, std::string file, std::string function, int64_t line) : Exception(message, file, function, line, 1) { }
	};
	
	class RuntimeException: public Exception {
		public:
		bool forceful = false;
		
		RuntimeException(std::string message, std::string file, std::string function, int64_t line) : Exception(message, file, function, line, 2) { }
		
		RuntimeException(std::string message, bool forceful, std::string file, std::string function, int64_t line) : Exception(message, file, function, line, 2), forceful(forceful) { }
	};
	
	class UnificationError: public RuntimeException {
		public:
		UnificationError(std::string message, std::string file, std::string function, int64_t line) : RuntimeException(message, file, function, line) { }
		
		UnificationError(std::string message, bool forceful, std::string file, std::string function, int64_t line) : RuntimeException(message, forceful, file, function, line) { }
	};
	
	enum class Mode { read, write };
	
	struct HeapContainer {
		// Ensure HeapContainer is polymorphic, so that we can use dynamic_cast
		POLYMORPHIC(HeapContainer);
		
		virtual std::unique_ptr<HeapContainer> copy() const = 0;
		
		virtual std::string toString() const {
			return "container";
		}
		
		virtual std::string trace(bool explicitControlCharacters = false) const = 0;
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
		
		virtual std::string trace(bool explicitControlCharacters = false) const override;
		
		HeapTuple(Type type, HeapReference::heapIndex reference) : type(type), reference(reference) { }
	};
	
	struct HeapFunctor: HeapContainer {
		std::string name;
		int64_t parameters;
		
		virtual std::unique_ptr<HeapContainer> copy() const override {
			return std::unique_ptr<HeapFunctor>(new HeapFunctor(*this));
		}
		
		virtual std::string toString() const override {
			return name + "/" + std::to_string(parameters);
		}
		
		virtual std::string trace(bool explicitControlCharacters = false) const override {
			if (!explicitControlCharacters && name.length() > 2 && name[0] == '\'') {
				std::string unquoted = name.substr(1, name.length() - 2);
				std::string::size_type position = 0;
				while ((position = unquoted.find("\\'", position)) != std::string::npos) {
					unquoted.replace(position, 2, "'");
					position += 1;
				}
				return unquoted;
			}
			return name;
		}
		
		HeapFunctor(std::string name, int64_t parameters) : name(name), parameters(parameters) { }
	};
	
	struct HeapNumber: HeapContainer {
		int64_t value;
		
		virtual std::unique_ptr<HeapContainer> copy() const override {
			return std::unique_ptr<HeapNumber>(new HeapNumber(*this));
		}
		
		virtual std::string toString() const override {
			return std::to_string(value);
		}
		
		virtual std::string trace(bool explicitControlCharacters = false) const override {
			return toString();
		}
		
		HeapNumber(int64_t value) : value(value) { }
	};
	
	HeapReference dereference(const HeapReference& reference);
	
	template <class T>
	class BoundsCheckedVector: public std::vector<T> {
		public:
		T& operator[] (const typename std::vector<T>::size_type index) {
			if (index >= this->size()) {
				throw RuntimeException("Tried to access a vector index out of bounds.", __FILENAME__, __func__, __LINE__);
			}
			return std::vector<T>::operator[](index);
		}
	};
	
	template <class T>
	class BoundsCheckedUniqueVector: public BoundsCheckedVector<std::unique_ptr<T>> { };
	
	template <class T>
	class BoundsCheckedSharedVector: public BoundsCheckedVector<std::shared_ptr<T>> { };
	
	class StackHeap: public BoundsCheckedUniqueVector<HeapContainer> {
		public:
		void print() {
			for (HeapReference::heapIndex i = 0; i < size(); ++ i) {
				std::cerr << std::setw(2) << i << ": " << ((*this)[i] != nullptr ? (*this)[i]->toString() : "null") << std::endl;
			}
		}
	};
	
	struct Instruction {
		POLYMORPHIC(Instruction);
		
		typedef typename BoundsCheckedVector<Instruction>::size_type instructionReference;
		
		virtual void execute() = 0;
		
		virtual std::string toString() const = 0;
	};
	
	struct StateReference {
		POLYMORPHIC(StateReference);
		
		typedef std::vector<std::unique_ptr<StateReference>>::size_type stateIndex;
	};
	
	struct Environment: StateReference {
		StateReference::stateIndex previousEnvironment;
		Instruction::instructionReference nextGoal;
		StackHeap variables;
		
		Environment(Instruction::instructionReference nextGoal) : nextGoal(nextGoal) {}
	};
	
	struct ChoicePoint: StateReference {
		StackHeap arguments;
		StateReference::stateIndex environment;
		Instruction::instructionReference nextGoal;
		Instruction::instructionReference nextClause;
		StateReference::stateIndex previousChoicePoint;
		std::vector<HeapReference>::size_type trailSize;
		HeapReference::heapIndex heapSize;
		
		ChoicePoint(StateReference::stateIndex environment, Instruction::instructionReference nextGoal, Instruction::instructionReference nextClause, std::stack<HeapReference>::size_type trailSize, HeapReference::heapIndex heapSize) : environment(environment), nextGoal(nextGoal), nextClause(nextClause), trailSize(trailSize), heapSize(heapSize) { }
	};
	
	struct Modifier {
		enum class Type { none, negate, intercept };
		Type type;
		Instruction::instructionReference nextInstruction;
		StateReference::stateIndex topEnvironment;
		StateReference::stateIndex topChoicePoint;
		
		Modifier(Type type, Instruction::instructionReference nextInstruction, StateReference::stateIndex topEnvironment, StateReference::stateIndex topChoicePoint) : type(type), nextInstruction(nextInstruction), topEnvironment(topEnvironment), topChoicePoint(topChoicePoint) { }
	};
	
	class Runtime {
		public:
		static Runtime* currentRuntime;
		
		// The global stack used to contain term structures used when unifying.
		StackHeap heap;
		
		// The registers used to temporarily hold pointers when building queries or rules
		StackHeap registers;
		
		// The instructions corresponding to the compiled program
		std::shared_ptr<BoundsCheckedSharedVector<Instruction>> instructions;
		
		// The stack used to store variable bindings and choice points
		std::vector<std::unique_ptr<StateReference>> stateStack;
		
		void compressStateStack() {
			// At the moment, the stack currently continues growing as more environments and choice points are added.
			// The stack should instead be overwritten, so that it grows only as much as is necessary.
		}
		
		StateReference::stateIndex topEnvironment = -1UL;
		Environment* currentEnvironment() {
			if (Environment* environment = dynamic_cast<Environment*>(stateStack[topEnvironment].get())) {
				return environment;
			} else {
				throw RuntimeException("Tried to access a choice point as an environment.", __FILENAME__, __func__, __LINE__);
			}
		}
		void popTopEnvironment() {
			topEnvironment = currentEnvironment()->previousEnvironment;
			compressStateStack();
		}
		
		std::vector<std::shared_ptr<StateReference>>::size_type topChoicePoint = -1UL;
		ChoicePoint* currentChoicePoint() {
			if (ChoicePoint* choicePoint = dynamic_cast<ChoicePoint*>(stateStack[topChoicePoint].get())) {
				return choicePoint;
			} else {
				throw RuntimeException("Tried to access an environment as a choice point.", __FILENAME__, __func__, __LINE__);
			}
		}
		void popTopChoicePoint() {
			ChoicePoint* choicePoint = currentChoicePoint();
			topEnvironment = choicePoint->environment;
			topChoicePoint = choicePoint->previousChoicePoint;
			compressStateStack();
		}
		
		int64_t currentNumberOfArguments = 0;
		
		// The stack used to contain the variables to unbind when backtracking
		std::vector<HeapReference> trail;
		
		// Labels with which a particular instruction can be jumped to
		std::unordered_map<std::string, Instruction::instructionReference> labels;
		
		Instruction::instructionReference nextInstruction;
		
		Instruction::instructionReference nextGoal;
		
		Mode mode;
		
		HeapReference::heapIndex unificationIndex;
		
		std::stack<Modifier> modifiers;
		
		Runtime() {
			instructions.reset(new BoundsCheckedSharedVector<Instruction>);
		}
		
		Runtime(Runtime& other) {
			instructions = other.instructions;
			labels = other.labels;
			// Make sure we don't overflow the number of Epilog registers.
			while (registers.size() < other.registers.size()) {
				registers.push_back(nullptr);
			}
		}
	};
	
	struct PushCompoundTermInstruction: Instruction {
		HeapFunctor functor;
		HeapReference registerReference;
		
		PushCompoundTermInstruction(HeapFunctor functor, HeapReference registerReference) : functor(functor), registerReference(registerReference) { }
		
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
	
	struct PushNumberInstruction: Instruction {
		HeapNumber number;
		HeapReference registerReference;
		
		PushNumberInstruction(HeapNumber number, HeapReference registerReference) : number(number), registerReference(registerReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "put_integer " + std::to_string(number.value) + ", " + registerReference.toString();
		}
	};
	
	struct UnifyCompoundTermInstruction: Instruction {
		HeapFunctor functor;
		HeapReference registerReference;
		
		UnifyCompoundTermInstruction(HeapFunctor functor, HeapReference registerReference) : functor(functor), registerReference(registerReference) { }
		
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
	
	struct UnifyNumberInstruction: Instruction {
		HeapNumber number;
		HeapReference registerReference;
		
		UnifyNumberInstruction(HeapNumber number, HeapReference registerReference) : number(number), registerReference(registerReference) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "get_integer " + std::to_string(number.value) + ", " + registerReference.toString();
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
		Modifier::Type modifier = Modifier::Type::none;
		
		CallInstruction(const HeapFunctor functor) : functor(functor) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "call " + std::string(modifier == Modifier::Type::negate ? "\\+" : modifier == Modifier::Type::intercept ? "\\:" : "") + functor.name + "/" + std::to_string(functor.parameters);
		}
	};
	
	struct ProceedInstruction: Instruction {
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "proceed";
		}
	};
	
	struct AllocateInstruction: Instruction {
		int64_t variables;
		
		AllocateInstruction(int64_t variables) : variables(variables) { }
		
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
	
	struct TryInitialClauseInstruction: Instruction {
		Instruction::instructionReference label;
		
		TryInitialClauseInstruction(Instruction::instructionReference label) : label(label) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "try_me_else";
		}
	};
	
	struct TryIntermediateClauseInstruction: Instruction {
		Instruction::instructionReference label;
		
		TryIntermediateClauseInstruction(Instruction::instructionReference label) : label(label) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "retry_me_else";
		}
	};
	
	struct TryFinalClauseInstruction: Instruction {
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "trust_me";
		}
	};
	
	struct CommandInstruction: Instruction {
		std::string function;
		
		CommandInstruction(std::string function) : function(function) { }
		
		virtual void execute() override;
		
		virtual std::string toString() const override {
			return "command " + function;
		}
	};
}
