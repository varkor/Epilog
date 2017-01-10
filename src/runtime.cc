#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include "runtime.hh"

namespace Epilog {
	StackHeap Runtime::heap;
	StackHeap Runtime::registers;
	BoundsCheckedVector<Instruction> Runtime::instructions;
	std::vector<std::unique_ptr<StateReference>> Runtime::stateStack;
	StateReference::stateIndex Runtime::topEnvironment = -1UL;
	StateReference::stateIndex Runtime::topChoicePoint = -1UL;
	int Runtime::currentNumberOfArguments = 0;
	std::vector<HeapReference> Runtime::trail;
	std::unordered_map<std::string, Instruction::instructionReference> Runtime::labels;
	Mode Runtime::mode;
	Instruction::instructionReference Runtime::nextInstruction;
	Instruction::instructionReference Runtime::nextGoal;
	HeapReference::heapIndex Runtime::unificationIndex;
	
	std::unique_ptr<HeapContainer>& HeapReference::get() const {
		switch (area) {
			case StorageArea::heap:
				return Runtime::heap[index];
			case StorageArea::reg:
				return Runtime::registers[index];
			case StorageArea::environment:
				return Runtime::currentEnvironment()->variables[index];
			case StorageArea::undefined:
				throw RuntimeException("Tried to get an undefined reference.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	void HeapReference::assign(std::unique_ptr<HeapContainer> value) const {
		switch (area) {
			case StorageArea::heap:
				Runtime::heap[index] = std::move(value);
				break;
			case StorageArea::reg:
				Runtime::registers[index] = std::move(value);
				break;
			case StorageArea::environment:
				Runtime::currentEnvironment()->variables[index] = std::move(value);
				break;
			case StorageArea::undefined:
				throw RuntimeException("Tried to assign to an undefined reference.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	std::string HeapTuple::trace() const {
		switch (type) {
			case Type::compoundTerm: {
				if (HeapFunctor* functor = dynamic_cast<HeapFunctor*>(Runtime::heap[reference].get())) {
					std::string parameters = "";
					for (int i = 0; i < functor->parameters; ++ i) {
						parameters += (i > 0 ? "," : "") + Runtime::heap[reference + (i + 1)]->trace();
					}
					return Runtime::heap[reference]->trace() + (functor->parameters > 0 ? "(" + parameters + ")" : "");
				} else {
					throw RuntimeException("Dereferenced a structure that did not point to a functor.", __FILENAME__, __func__, __LINE__);
				}
			}
			case Type::reference: {
				if (Runtime::heap[reference].get() != this) {
					return Runtime::heap[reference]->trace();
				} else {
					return "_";
				}
			}
		}
	}
	
	void PushCompoundTermInstruction::execute() {
		HeapTuple header(HeapTuple::Type::compoundTerm, Runtime::heap.size() + 1);
		Runtime::heap.push_back(header.copy());
		Runtime::heap.push_back(functor.copy());
		registerReference.assign(header.copy());
		++ Runtime::nextInstruction;
	}
	
	void PushVariableInstruction::execute() {
		HeapTuple header(HeapTuple::Type::reference, Runtime::heap.size());
		Runtime::heap.push_back(header.copy());
		registerReference.assign(header.copy());
		++ Runtime::nextInstruction;
	}
	
	void PushValueInstruction::execute() {
		Runtime::heap.push_back(registerReference.getAsCopy());
		++ Runtime::nextInstruction;
	}
	
	HeapReference& dereference(HeapReference& reference) {
		if (HeapTuple* value = dynamic_cast<HeapTuple*>(reference.getPointer())) {
			HeapTuple::Type type = value->type;
			HeapReference::heapIndex next = value->reference;
			if (type == HeapTuple::Type::reference && (reference.area != StorageArea::heap || next != reference.index)) {
				HeapReference nextReference(StorageArea::heap, next);
				return dereference(nextReference);
			} else { 
				return reference;
			}
		} else {
			throw RuntimeException("Tried to dereference a non-tuple address on the stack as a tuple.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	void trail(HeapReference& reference) {
		// Only conditional bindings need to be stored.
		// These are bindings that affect variables existing before the creation of the current choice point.
		if (Runtime::topChoicePoint != -1UL && ((reference.area == StorageArea::heap && reference.index < Runtime::currentChoicePoint()->heapSize) || reference.area == StorageArea::environment)) {
			Runtime::trail.push_back(reference);
		}
	}
	
	void bind(HeapReference& referenceA, HeapReference& referenceB) {
		HeapTuple* tupleA, * tupleB;
		if ((tupleA = dynamic_cast<HeapTuple*>(referenceA.getPointer())) && (tupleB = dynamic_cast<HeapTuple*>(referenceB.getPointer()))) {
			if (tupleA->type == HeapTuple::Type::reference && (tupleB->type != HeapTuple::Type::reference || referenceA.index <= referenceB.index)) {
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
			HeapReference& d1 = dereference(pushdownList.top()); pushdownList.pop();
			HeapReference& d2 = dereference(pushdownList.top()); pushdownList.pop();
			// Force unification to occur if both values are compound terms placed in registers
			if (d1 != d2) {
				HeapTuple* value1 = dynamic_cast<HeapTuple*>(d1.getPointer());
				HeapTuple* value2 = dynamic_cast<HeapTuple*>(d2.getPointer());
				if (value1 && value2) {
					HeapTuple::Type t1 = value1->type;
					HeapReference::heapIndex v1 = value1->reference;
					HeapTuple::Type t2 = value2->type;
					HeapReference::heapIndex v2 = value2->reference;
					if (t1 == HeapTuple::Type::reference || t2 == HeapTuple::Type::reference) {
						bind(d1, d2);
					} else {
						HeapFunctor* fn1 = dynamic_cast<HeapFunctor*>(Runtime::heap[v1].get());
						HeapFunctor* fn2 = dynamic_cast<HeapFunctor*>(Runtime::heap[v2].get());
						if (fn1 && fn2) {
							if (fn1->name == fn2->name && fn1->parameters == fn2->parameters) {
								for (int i = 1; i <= fn1->parameters; ++ i) {
									pushdownList.push(HeapReference(StorageArea::heap, v1 + i));
									pushdownList.push(HeapReference(StorageArea::heap, v2 + i));
								}
							} else {
								throw UnificationError("Tried to unify two values that cannot unify.", __FILENAME__, __func__, __LINE__);
							}
						} else {
							throw RuntimeException("Tried to dereference a non-functor address on the stack as a functor.", __FILENAME__, __func__, __LINE__);
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
					HeapReference::heapIndex index = Runtime::heap.size();
					Runtime::heap.push_back(std::unique_ptr<HeapTuple>(new HeapTuple(HeapTuple::Type::compoundTerm, index + 1)));
					Runtime::heap.push_back(functor.copy());
					HeapReference newCompoundTerm(StorageArea::heap, index);
					bind(address, newCompoundTerm);
					Runtime::mode = Mode::write;
					break;
				}
				case HeapTuple::Type::compoundTerm: {
					HeapReference::heapIndex reference = value->reference;
					if (HeapFunctor* fnc = dynamic_cast<HeapFunctor*>(Runtime::heap[reference].get())) {
						if (fnc->name == functor.name && fnc->parameters == functor.parameters) {
							Runtime::unificationIndex = reference + 1;
							Runtime::mode = Mode::read;
						} else {
							throw UnificationError("Tried to unify two functors that cannot unify.", __FILENAME__, __func__, __LINE__);
						}
					} else {
						throw RuntimeException("Tried to dereference a non-functor address on the stack as a functor.", __FILENAME__, __func__, __LINE__);
					}
					break;
				}					
			}
		} else {
			throw RuntimeException("Tried to dereference a non-tuple address on the stack as a tuple.", __FILENAME__, __func__, __LINE__);
		}
		++ Runtime::nextInstruction;
	}
	
	void UnifyVariableInstruction::execute() {
		switch (Runtime::mode) {
			case Mode::read:
				registerReference.assign(Runtime::heap[Runtime::unificationIndex]->copy());
				break;
			case Mode::write:
				HeapTuple header(HeapTuple::Type::reference, Runtime::heap.size());
				Runtime::heap.push_back(header.copy());
				registerReference.assign(header.copy());
				break;
		}
		++ Runtime::unificationIndex;
		++ Runtime::nextInstruction;
	}
	
	void UnifyValueInstruction::execute() {
		switch (Runtime::mode) {
			case Mode::read: {
				HeapReference unificationReference(StorageArea::heap, Runtime::unificationIndex);
				unify(registerReference, unificationReference);
				break;
			}
			case Mode::write: {
				Runtime::heap.push_back(registerReference.getAsCopy());
				break;
			}
		}
		++ Runtime::unificationIndex;
		++ Runtime::nextInstruction;
	}
	
	void PushVariableToAllInstruction::execute() {
		HeapTuple header(HeapTuple::Type::reference, Runtime::heap.size());
		Runtime::heap.push_back(header.copy());
		registerReference.assign(header.copy());
		argumentReference.assign(header.copy());
		++ Runtime::nextInstruction;
	}
	
	void CopyRegisterToArgumentInstruction::execute() {
		argumentReference.assign(registerReference.getAsCopy());
		++ Runtime::nextInstruction;
	}
	
	void CopyArgumentToRegisterInstruction::execute() {
		registerReference.assign(argumentReference.getAsCopy());
		++ Runtime::nextInstruction;
	}
	
	void UnifyRegisterAndArgumentInstruction::execute() {
		unify(registerReference, argumentReference);
		++ Runtime::nextInstruction;
	}
	
	void CallInstruction::execute() {
		std::string label = functor.toString();
		if (Runtime::labels.find(label) != Runtime::labels.end()) {
			Runtime::nextGoal = Runtime::nextInstruction + 1;
			Runtime::currentNumberOfArguments = functor.parameters;
			Runtime::nextInstruction = Runtime::labels[label];
		} else {
			throw UnificationError("Tried to jump to an inexistent label.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	void ProceedInstruction::execute() {
		Runtime::nextInstruction = Runtime::nextGoal;
	}
	
	void AllocateInstruction::execute() {
		std::unique_ptr<Environment> environment(new Environment(Runtime::nextGoal));
		environment->previousEnvironment = Runtime::topEnvironment;
		for (int i = 0; i < variables; ++ i) {
			environment->variables.push_back(nullptr);
		}
		Runtime::topEnvironment = Runtime::stateStack.size();
		Runtime::stateStack.push_back(std::move(environment));
		++ Runtime::nextInstruction;
	}
	
	void DeallocateInstruction::execute() {
		Runtime::nextInstruction = Runtime::currentEnvironment()->nextGoal;
		Runtime::popTopEnvironment();
	}
	
	void unwindTrail(std::vector<HeapReference>::size_type from, std::vector<HeapReference>::size_type to) {
		for (std::vector<HeapReference>::size_type i = from; i < to; ++ i) {
			HeapTuple header(HeapTuple::Type::reference, Runtime::trail[i].index);
			Runtime::trail[i].assign(header.copy());
		}
	}
	
	void TryInitialClauseInstruction::execute() {
		if (Runtime::topEnvironment == -1UL) {
			throw RuntimeException("Tried to try an intial clause with no environment.", __FILENAME__, __func__, __LINE__);
		}
		std::unique_ptr<ChoicePoint> choicePoint(new ChoicePoint(Runtime::topEnvironment, Runtime::nextGoal, label, Runtime::trail.size(), Runtime::heap.size()));
		choicePoint->previousChoicePoint = Runtime::topChoicePoint;
		choicePoint->environment = Runtime::topEnvironment;
		// Initialise the arguments
		for (int i = 0; i < Runtime::currentNumberOfArguments; ++ i) {
			choicePoint->arguments.push_back(Runtime::registers[i]->copy());
		}
		Runtime::topChoicePoint = Runtime::stateStack.size();
		Runtime::stateStack.push_back(std::move(choicePoint));
		++ Runtime::nextInstruction;
	}
	
	void TryIntermediateClauseInstruction::execute() {
		ChoicePoint* choicePoint = Runtime::currentChoicePoint();
		// Set the arguments from frame
		for (HeapReference::heapIndex i = 0; i < choicePoint->arguments.size(); ++ i) {
			Runtime::registers[i] = choicePoint->arguments[i]->copy();
		}
		// Set other variables
		Runtime::topEnvironment = choicePoint->environment;
		Runtime::compressStateStack();
		Runtime::nextGoal = choicePoint->nextGoal;
		choicePoint->nextClause = label;
		unwindTrail(choicePoint->trailSize, Runtime::trail.size());
		while (Runtime::trail.size() > choicePoint->trailSize) {
			Runtime::trail.pop_back();
		}
		while (Runtime::heap.size() > choicePoint->heapSize) {
			Runtime::heap.pop_back();
		}
		++ Runtime::nextInstruction;
	}
	
	void TryFinalClauseInstruction::execute() {
		ChoicePoint* choicePoint = Runtime::currentChoicePoint();
		// Set the arguments from frame
		for (HeapReference::heapIndex i = 0; i < choicePoint->arguments.size(); ++ i) {
			Runtime::registers[i] = choicePoint->arguments[i]->copy();
		}
		// Set other variables
		Runtime::nextGoal = choicePoint->nextGoal;
		unwindTrail(choicePoint->trailSize, Runtime::trail.size());
		while (Runtime::trail.size() > choicePoint->trailSize) {
			Runtime::trail.pop_back();
		}
		while (Runtime::heap.size() > choicePoint->heapSize) {
			Runtime::heap.pop_back();
		}
		Runtime::popTopChoicePoint();
		++ Runtime::nextInstruction;
	}
}
