#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include "runtime.hh"
#include "interpreter.hh"
#include "standardlibrary.hh"

namespace Epilog {
	Runtime* Runtime::currentRuntime = nullptr;
	
	std::unique_ptr<HeapContainer>& HeapReference::get() const {
		switch (area) {
			case StorageArea::heap:
				return Runtime::currentRuntime->heap[index];
			case StorageArea::reg:
				return Runtime::currentRuntime->registers[index];
			case StorageArea::environment:
				return Runtime::currentRuntime->currentEnvironment()->variables[index];
			case StorageArea::undefined:
				throw RuntimeException("Tried to get an undefined reference.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	void HeapReference::assign(std::unique_ptr<HeapContainer> value) const {
		switch (area) {
			case StorageArea::heap:
				Runtime::currentRuntime->heap[index] = std::move(value);
				break;
			case StorageArea::reg:
				Runtime::currentRuntime->registers[index] = std::move(value);
				break;
			case StorageArea::environment:
				Runtime::currentRuntime->currentEnvironment()->variables[index] = std::move(value);
				break;
			case StorageArea::undefined:
				throw RuntimeException("Tried to assign to an undefined reference.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	std::string listToString(HeapContainer* container, bool explicitControlCharacters) {
		if (::Epilog::HeapTuple* tuple = dynamic_cast<::Epilog::HeapTuple*>(container)) {
			HeapFunctor* functor;
			if (tuple->type == HeapTuple::Type::compoundTerm && (functor = dynamic_cast<HeapFunctor*>(Runtime::currentRuntime->heap[tuple->reference].get()))) {
				std::string symbol = functor->toString();
				if (symbol == "./2") {
					return ", " + Runtime::currentRuntime->heap[tuple->reference + 1]->trace(explicitControlCharacters) + listToString(Runtime::currentRuntime->heap[tuple->reference + 2].get(), explicitControlCharacters);
				} else if (symbol == "[]/0") {
					return "";
				}
			}
		}
		return " | " + container->trace(explicitControlCharacters);
	}
	
	std::string HeapTuple::trace(bool explicitControlCharacters) const {
		switch (type) {
			case Type::compoundTerm: {
				if (HeapFunctor* functor = dynamic_cast<HeapFunctor*>(Runtime::currentRuntime->heap[reference].get())) {
					if (functor->name == "." && functor->parameters == 2) {
						// It's a list, so display it as one.
						return "[" + Runtime::currentRuntime->heap[reference + 1]->trace(explicitControlCharacters) + listToString(Runtime::currentRuntime->heap[reference + 2].get(), explicitControlCharacters) + "]";
					} else {
						std::string parameters = "";
						for (int64_t i = 0; i < functor->parameters; ++ i) {
							parameters += (i > 0 ? "," : "") + Runtime::currentRuntime->heap[reference + (i + 1)]->trace(explicitControlCharacters);
						}
						return Runtime::currentRuntime->heap[reference]->trace(explicitControlCharacters) + (functor->parameters > 0 ? "(" + parameters + ")" : "");
					}
				} else {
					throw RuntimeException("Dereferenced a structure that did not point to a functor.", __FILENAME__, __func__, __LINE__);
				}
			}
			case Type::reference: {
				if (Runtime::currentRuntime->heap[reference].get() != this) {
					return Runtime::currentRuntime->heap[reference]->trace(explicitControlCharacters);
				} else {
					return "_";
				}
			}
		}
	}
	
	void PushCompoundTermInstruction::execute() {
		HeapTuple header(HeapTuple::Type::compoundTerm, Runtime::currentRuntime->heap.size() + 1);
		Runtime::currentRuntime->heap.push_back(header.copy());
		Runtime::currentRuntime->heap.push_back(functor.copy());
		registerReference.assign(header.copy());
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void PushVariableInstruction::execute() {
		HeapTuple header(HeapTuple::Type::reference, Runtime::currentRuntime->heap.size());
		Runtime::currentRuntime->heap.push_back(header.copy());
		registerReference.assign(header.copy());
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void PushValueInstruction::execute() {
		Runtime::currentRuntime->heap.push_back(registerReference.getAsCopy());
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void PushNumberInstruction::execute() {
		Runtime::currentRuntime->heap.push_back(number.copy());
		registerReference.assign(number.copy());
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	HeapReference dereference(const HeapReference& reference) {
		if (HeapTuple* value = dynamic_cast<HeapTuple*>(reference.getPointer())) {
			HeapTuple::Type type = value->type;
			HeapReference::heapIndex next = value->reference;
			if (type == HeapTuple::Type::reference && (reference.area != StorageArea::heap || next != reference.index)) {
				HeapReference nextReference(StorageArea::heap, next);
				return dereference(nextReference);
			} else { 
				return reference;
			}
		} else if (dynamic_cast<HeapNumber*>(reference.getPointer())) {
			return reference;
		} else {
			throw RuntimeException("Tried to dereference a non-tuple address on the stack as a tuple.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	void trail(HeapReference& reference) {
		// Only conditional bindings need to be stored.
		// These are bindings that affect variables existing before the creation of the current choice point.
		if (Runtime::currentRuntime->topChoicePoint != -1UL && ((reference.area == StorageArea::heap && reference.index < Runtime::currentRuntime->currentChoicePoint()->heapSize) || reference.area == StorageArea::environment)) {
			Runtime::currentRuntime->trail.push_back(reference);
		}
	}
	
	void bind(HeapReference& referenceA, HeapReference& referenceB) {
		HeapTuple* tupleA = dynamic_cast<HeapTuple*>(referenceA.getPointer());
		HeapTuple* tupleB = dynamic_cast<HeapTuple*>(referenceB.getPointer());
		HeapNumber* numberA = dynamic_cast<HeapNumber*>(referenceA.getPointer());
		HeapNumber* numberB = dynamic_cast<HeapNumber*>(referenceB.getPointer());
		if ((tupleA || numberA) && (tupleB || numberB)) {
			if ((tupleA && tupleA->type == HeapTuple::Type::reference) && (!tupleB || tupleB->type != HeapTuple::Type::reference || referenceA.index <= referenceB.index)) {
				referenceA.assign(referenceB.getAsCopy());
				trail(referenceA);
			} else {
				referenceB.assign(referenceA.getAsCopy());
				trail(referenceB);
			}
		} else {
			throw RuntimeException("Tried to bind a non-tuple structure.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	void unify(HeapReference& a, HeapReference& b) {
		std::stack<HeapReference> pushdownList;
		pushdownList.push(a);
		pushdownList.push(b);
		while (!pushdownList.empty()) {
			HeapReference referenceA = dereference(pushdownList.top()); pushdownList.pop();
			HeapReference referenceB = dereference(pushdownList.top()); pushdownList.pop();
			// Force unification to occur if both values are compound terms placed in registers
			if (referenceA != referenceB) {
				HeapTuple* tupleA = dynamic_cast<HeapTuple*>(referenceA.getPointer());
				HeapTuple* tupleB = dynamic_cast<HeapTuple*>(referenceB.getPointer());
				HeapNumber* numberA = dynamic_cast<HeapNumber*>(referenceA.getPointer());
				HeapNumber* numberB = dynamic_cast<HeapNumber*>(referenceB.getPointer());
				if ((tupleA || numberA) && (tupleB || numberB)) {
					HeapTuple::Type typeA = tupleA ? tupleA->type : HeapTuple::Type::compoundTerm;
					HeapTuple::Type typeB = tupleB ? tupleB->type : HeapTuple::Type::compoundTerm;
					if (typeA == HeapTuple::Type::reference || typeB == HeapTuple::Type::reference) {
						bind(referenceA, referenceB);
					} else {
						if (numberA && numberB) {
							if (numberA->value != numberB->value) {
								throw UnificationError("Tried to unify two unequal numbers.", __FILENAME__, __func__, __LINE__);
							}
						} else if (tupleA && tupleB) {
							HeapReference::heapIndex indexA = tupleA->reference;
							HeapReference::heapIndex indexB = tupleB->reference;
							HeapFunctor* functorA = dynamic_cast<HeapFunctor*>(Runtime::currentRuntime->heap[indexA].get());
							HeapFunctor* functorB = dynamic_cast<HeapFunctor*>(Runtime::currentRuntime->heap[indexB].get());
							if (functorA && functorB) {
								if (functorA->name == functorB->name && functorA->parameters == functorB->parameters) {
									for (int64_t i = 1; i <= functorA->parameters; ++ i) {
										pushdownList.push(HeapReference(StorageArea::heap, indexA + i));
										pushdownList.push(HeapReference(StorageArea::heap, indexB + i));
									}
								} else {
									throw UnificationError("Tried to unify two values that cannot unify.", __FILENAME__, __func__, __LINE__);
								}
							} else {
								throw RuntimeException("Tried to dereference a non-functor address on the stack as a functor.", __FILENAME__, __func__, __LINE__);
							}
						} else {
							throw UnificationError("Tried to unify a number with a compound term.", __FILENAME__, __func__, __LINE__);
						}
					}
				} else {
					throw RuntimeException("Tried to dereference a non-tuple address on the stack as a tuple.", __FILENAME__, __func__, __LINE__);
				}
			}
		}
	}
	
	void UnifyCompoundTermInstruction::execute() {
		HeapReference address = dereference(registerReference);
		if (HeapTuple* value = dynamic_cast<HeapTuple*>(address.getPointer())) {
			HeapTuple::Type type = value->type;
			switch (type) {
				case HeapTuple::Type::reference: {
					HeapReference::heapIndex index = Runtime::currentRuntime->heap.size();
					Runtime::currentRuntime->heap.push_back(std::unique_ptr<HeapTuple>(new HeapTuple(HeapTuple::Type::compoundTerm, index + 1)));
					Runtime::currentRuntime->heap.push_back(functor.copy());
					HeapReference newCompoundTerm(StorageArea::heap, index);
					bind(address, newCompoundTerm);
					Runtime::currentRuntime->mode = Mode::write;
					break;
				}
				case HeapTuple::Type::compoundTerm: {
					HeapReference::heapIndex reference = value->reference;
					if (HeapFunctor* fnc = dynamic_cast<HeapFunctor*>(Runtime::currentRuntime->heap[reference].get())) {
						if (fnc->name == functor.name && fnc->parameters == functor.parameters) {
							Runtime::currentRuntime->unificationIndex = reference + 1;
							Runtime::currentRuntime->mode = Mode::read;
						} else {
							throw UnificationError("Tried to unify two functors that cannot unify.", __FILENAME__, __func__, __LINE__);
						}
					} else {
						throw RuntimeException("Tried to dereference a non-functor address on the stack as a functor.", __FILENAME__, __func__, __LINE__);
					}
					break;
				}					
			}
		} else if (dynamic_cast<HeapNumber*>(address.getPointer())) {
			throw UnificationError("Tried to unify a compound term with a number.", __FILENAME__, __func__, __LINE__);
		} else {
			throw RuntimeException("Tried to dereference a non-tuple address on the stack as a tuple.", __FILENAME__, __func__, __LINE__);
		}
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void UnifyNumberInstruction::execute() {
		HeapReference address = dereference(registerReference);
		if (HeapTuple* value = dynamic_cast<HeapTuple*>(address.getPointer())) {
			HeapTuple::Type type = value->type;
			switch (type) {
				case HeapTuple::Type::reference: {
					HeapReference::heapIndex index = Runtime::currentRuntime->heap.size();
					Runtime::currentRuntime->heap.push_back(number.copy());
					HeapReference newNumber(StorageArea::heap, index);
					bind(address, newNumber);
					Runtime::currentRuntime->mode = Mode::write;
					break;
				}
				case HeapTuple::Type::compoundTerm: {
					throw UnificationError("Tried to unify a number with a compound term.", __FILENAME__, __func__, __LINE__);
				}					
			}
		} else if (HeapNumber* num = dynamic_cast<HeapNumber*>(address.getPointer())) {
			if (num->value == number.value) {
				Runtime::currentRuntime->unificationIndex = address.index + 1;
				Runtime::currentRuntime->mode = Mode::read;
			} else {
				throw UnificationError("Tried to unify two unequal numbers.", __FILENAME__, __func__, __LINE__);
			}
		} else {
			throw RuntimeException("Tried to dereference a non-tuple address on the stack as a tuple.", __FILENAME__, __func__, __LINE__);
		}
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void UnifyVariableInstruction::execute() {
		switch (Runtime::currentRuntime->mode) {
			case Mode::read:
				registerReference.assign(Runtime::currentRuntime->heap[Runtime::currentRuntime->unificationIndex]->copy());
				break;
			case Mode::write:
				HeapTuple header(HeapTuple::Type::reference, Runtime::currentRuntime->heap.size());
				Runtime::currentRuntime->heap.push_back(header.copy());
				registerReference.assign(header.copy());
				break;
		}
		++ Runtime::currentRuntime->unificationIndex;
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void UnifyValueInstruction::execute() {
		switch (Runtime::currentRuntime->mode) {
			case Mode::read: {
				HeapReference unificationReference(StorageArea::heap, Runtime::currentRuntime->unificationIndex);
				unify(registerReference, unificationReference);
				break;
			}
			case Mode::write: {
				Runtime::currentRuntime->heap.push_back(registerReference.getAsCopy());
				break;
			}
		}
		++ Runtime::currentRuntime->unificationIndex;
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void PushVariableToAllInstruction::execute() {
		HeapTuple header(HeapTuple::Type::reference, Runtime::currentRuntime->heap.size());
		Runtime::currentRuntime->heap.push_back(header.copy());
		registerReference.assign(header.copy());
		argumentReference.assign(header.copy());
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void CopyRegisterToArgumentInstruction::execute() {
		argumentReference.assign(registerReference.getAsCopy());
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void CopyArgumentToRegisterInstruction::execute() {
		registerReference.assign(argumentReference.getAsCopy());
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void UnifyRegisterAndArgumentInstruction::execute() {
		unify(registerReference, argumentReference);
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void CallInstruction::execute() {
		Runtime::currentRuntime->modifiers.push(Modifier(modifier, Runtime::currentRuntime->nextInstruction + 1, Runtime::currentRuntime->topEnvironment, Runtime::currentRuntime->topChoicePoint));
		std::string label = functor.toString();
		if (Runtime::currentRuntime->labels.find(label) != Runtime::currentRuntime->labels.end()) {
			Runtime::currentRuntime->nextGoal = Runtime::currentRuntime->nextInstruction + 1;
			Runtime::currentRuntime->currentNumberOfArguments = functor.parameters;
			Runtime::currentRuntime->nextInstruction = Runtime::currentRuntime->labels[label];
		} else {
			throw UnificationError("Tried to jump to an inexistent label.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	void ProceedInstruction::execute() {
		if (!Runtime::currentRuntime->modifiers.empty()) {
			Modifier& modifier(Runtime::currentRuntime->modifiers.top());
			if (modifier.type == Modifier::Type::negate) {
				throw UnificationError("Successfully unified within not.", true, __FILENAME__, __func__, __LINE__);
			}
			if (modifier.type == Modifier::Type::intercept) {
				throw UnificationError("Successfully unified within catch.", true, __FILENAME__, __func__, __LINE__);
			}
			// Otherwise, the modifier is empty, and we may proceed as usual.
		}
		Runtime::currentRuntime->nextInstruction = Runtime::currentRuntime->nextGoal;
	}
	
	void AllocateInstruction::execute() {
		std::unique_ptr<Environment> environment(new Environment(Runtime::currentRuntime->nextGoal));
		environment->previousEnvironment = Runtime::currentRuntime->topEnvironment;
		for (int64_t i = 0; i < variables; ++ i) {
			environment->variables.push_back(nullptr);
		}
		Runtime::currentRuntime->topEnvironment = Runtime::currentRuntime->stateStack.size();
		Runtime::currentRuntime->stateStack.push_back(std::move(environment));
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void DeallocateInstruction::execute() {
		Runtime::currentRuntime->nextInstruction = Runtime::currentRuntime->currentEnvironment()->nextGoal;
		Runtime::currentRuntime->popTopEnvironment();
	}
	
	void unwindTrail(std::vector<HeapReference>::size_type from, std::vector<HeapReference>::size_type to) {
		for (std::vector<HeapReference>::size_type i = from; i < to; ++ i) {
			HeapTuple header(HeapTuple::Type::reference, Runtime::currentRuntime->trail[i].index);
			Runtime::currentRuntime->trail[i].assign(header.copy());
		}
	}
	
	void TryInitialClauseInstruction::execute() {
		if (Runtime::currentRuntime->topEnvironment == -1UL) {
			throw RuntimeException("Tried to try an intial clause with no environment.", __FILENAME__, __func__, __LINE__);
		}
		std::unique_ptr<ChoicePoint> choicePoint(new ChoicePoint(Runtime::currentRuntime->topEnvironment, Runtime::currentRuntime->nextGoal, label, Runtime::currentRuntime->trail.size(), Runtime::currentRuntime->heap.size()));
		choicePoint->previousChoicePoint = Runtime::currentRuntime->topChoicePoint;
		choicePoint->environment = Runtime::currentRuntime->topEnvironment;
		// Initialise the arguments
		for (int64_t i = 0; i < Runtime::currentRuntime->currentNumberOfArguments; ++ i) {
			choicePoint->arguments.push_back(Runtime::currentRuntime->registers[i]->copy());
		}
		Runtime::currentRuntime->topChoicePoint = Runtime::currentRuntime->stateStack.size();
		Runtime::currentRuntime->stateStack.push_back(std::move(choicePoint));
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void TryIntermediateClauseInstruction::execute() {
		ChoicePoint* choicePoint = Runtime::currentRuntime->currentChoicePoint();
		// Set the arguments from frame
		for (HeapReference::heapIndex i = 0; i < choicePoint->arguments.size(); ++ i) {
			Runtime::currentRuntime->registers[i] = choicePoint->arguments[i]->copy();
		}
		// Set other variables
		Runtime::currentRuntime->topEnvironment = choicePoint->environment;
		Runtime::currentRuntime->compressStateStack();
		Runtime::currentRuntime->nextGoal = choicePoint->nextGoal;
		choicePoint->nextClause = label;
		unwindTrail(choicePoint->trailSize, Runtime::currentRuntime->trail.size());
		while (Runtime::currentRuntime->trail.size() > choicePoint->trailSize) {
			Runtime::currentRuntime->trail.pop_back();
		}
		while (Runtime::currentRuntime->heap.size() > choicePoint->heapSize) {
			Runtime::currentRuntime->heap.pop_back();
		}
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void TryFinalClauseInstruction::execute() {
		ChoicePoint* choicePoint = Runtime::currentRuntime->currentChoicePoint();
		// Set the arguments from frame
		for (HeapReference::heapIndex i = 0; i < choicePoint->arguments.size(); ++ i) {
			Runtime::currentRuntime->registers[i] = choicePoint->arguments[i]->copy();
		}
		// Set other variables
		Runtime::currentRuntime->nextGoal = choicePoint->nextGoal;
		unwindTrail(choicePoint->trailSize, Runtime::currentRuntime->trail.size());
		while (Runtime::currentRuntime->trail.size() > choicePoint->trailSize) {
			Runtime::currentRuntime->trail.pop_back();
		}
		while (Runtime::currentRuntime->heap.size() > choicePoint->heapSize) {
			Runtime::currentRuntime->heap.pop_back();
		}
		Runtime::currentRuntime->popTopChoicePoint();
		++ Runtime::currentRuntime->nextInstruction;
	}
	
	void CommandInstruction::execute() {
		if (StandardLibrary::commands.find(function) != StandardLibrary::commands.end()) {
			StandardLibrary::commands[function]();
		} else {
			throw RuntimeException("Tried to execute an unknown command.", __FILENAME__, __func__, __LINE__);
		}
		++ Runtime::currentRuntime->nextInstruction;
	}
}
