#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include "runtime.hh"

namespace Epilog {
	StackHeap Runtime::heap;
	StackHeap Runtime::registers;
	BoundsCheckedVector<Instruction> Runtime::instructions;
	std::stack<std::unique_ptr<Environment>> Runtime::environments;
	std::unordered_map<std::string, BoundsCheckedVector<Instruction>::size_type> Runtime::labels;
	Mode Runtime::mode;
	BoundsCheckedVector<Instruction>::size_type Runtime::nextInstruction;
	BoundsCheckedVector<Instruction>::size_type Runtime::nextGoal;
	HeapReference::heapIndex Runtime::unificationIndex;
	
	std::unique_ptr<HeapContainer>& HeapReference::get() const {
		switch (area) {
			case StorageArea::heap:
				return Runtime::heap[index];
			case StorageArea::reg:
				return Runtime::registers[index];
			case StorageArea::environment:
				return Runtime::environments.top()->variables[index];
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
				Runtime::environments.top()->variables[index] = std::move(value);
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
	
	void bind(HeapReference& bound, HeapReference& to) {
		if (to.area != StorageArea::heap) {
			throw RuntimeException("Tried to bind to a reference not on the heap.", __FILENAME__, __func__, __LINE__);
		}
		bound.assign(std::unique_ptr<HeapTuple>(new HeapTuple(HeapTuple::Type::reference, to.index)));
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
			Runtime::nextInstruction = Runtime::labels[label];
		} else {
			throw UnificationError("Tried to jump to an inexistent label.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	void ProceedInstruction::execute() {
		Runtime::nextInstruction = Runtime::nextGoal;
	}
	
	void AllocateInstruction::execute() {
		std::unique_ptr<Environment> frame(new Environment(Runtime::nextGoal));
		for (int i = 0; i < variables; ++ i) {
			frame->variables.push_back(nullptr);
		}
		Runtime::environments.push(std::move(frame));
		++ Runtime::nextInstruction;
	}
	
	void DeallocateInstruction::execute() {
		std::unique_ptr<Environment>& frame = Runtime::environments.top();
		Runtime::nextInstruction = frame->nextGoal;
		Runtime::environments.pop();
	}
}
