#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include "runtime.hh"

namespace Epilog {
	StackHeap Runtime::stack;
	StackHeap Runtime::registers;
	BoundsCheckedVector<Instruction> Runtime::instructions;
	std::unordered_map<std::string, BoundsCheckedVector<Instruction>::size_type> Runtime::labels;
	Mode Runtime::mode;
	BoundsCheckedVector<Instruction>::size_type Runtime::nextInstruction;
	HeapContainer::heapIndex Runtime::unificationIndex;
	
	std::string HeapTuple::trace() const {
		switch (type) {
			case Type::compoundTerm: {
				if (HeapFunctor* functor = dynamic_cast<HeapFunctor*>(Runtime::stack[reference].get())) {
					std::string parameters = "";
					for (int i = 0; i < functor->parameters; ++ i) {
						parameters += (i > 0 ? "," : "") + Runtime::stack[reference + (i + 1)]->trace();
					}
					return Runtime::stack[reference]->trace() + (functor->parameters > 0 ? "(" + parameters + ")" : "");
				} else {
					throw RuntimeException("Dereferenced a structure that did not point to a functor.", __FILENAME__, __func__, __LINE__);
				}
			}
			case Type::reference: {
				if (Runtime::stack[reference].get() != this) {
					return Runtime::stack[reference]->trace();
				} else {
					return "_";
				}
			}
		}
	}
	
	void PushCompoundTermInstruction::execute() {
		HeapTuple header(HeapTuple::Type::compoundTerm, Runtime::stack.size() + 1);
		Runtime::stack.push_back(header.copy());
		Runtime::stack.push_back(functor.copy());
		Runtime::registers[registerIndex] = header.copy();
		++ Runtime::nextInstruction;
	}
	
	void PushVariableInstruction::execute() {
		HeapTuple header(HeapTuple::Type::reference, Runtime::stack.size());
		Runtime::stack.push_back(header.copy());
		Runtime::registers[registerIndex] = header.copy();
		++ Runtime::nextInstruction;
	}
	
	void PushValueInstruction::execute() {
		Runtime::stack.push_back(Runtime::registers[registerIndex]->copy());
		++ Runtime::nextInstruction;
	}
	
	HeapContainer::heapIndex dereference(std::unique_ptr<HeapContainer>& container, HeapContainer::heapIndex index = -1UL) {
		if (HeapTuple* value = dynamic_cast<HeapTuple*>(container.get())) {
			HeapTuple::Type type = value->type;
			HeapContainer::heapIndex reference = value->reference;
			if (type == HeapTuple::Type::reference && reference != index) {
				return dereference(reference);
			} else { 
				return index;
			}
		} else {
			throw RuntimeException("Tried to dereference a non-tuple address on the stack as a tuple.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	HeapContainer::heapIndex dereference(HeapContainer::heapIndex index) {
		if (index == -1UL) {
			return index;
		}
		return dereference(Runtime::stack[index], index);
	}
	
	void bind(std::unique_ptr<HeapContainer>& container, HeapContainer::heapIndex reference) {
		container = std::unique_ptr<HeapTuple>(new HeapTuple(HeapTuple::Type::reference, reference));
	}
	
	void unify(std::unique_ptr<HeapContainer>& a, std::unique_ptr<HeapContainer>& b) {
		std::stack<HeapContainer::heapIndex> pushdownList;
		pushdownList.push(dereference(a));
		pushdownList.push(dereference(b));
		while (!pushdownList.empty()) {
			HeapContainer::heapIndex d1 = dereference(pushdownList.top()); pushdownList.pop();
			HeapContainer::heapIndex d2 = dereference(pushdownList.top()); pushdownList.pop();
			// Force unification to occur if both values are compound terms placed in registers
			if (d1 != d2 || (d1 == -1UL && d2 == -1UL)) {
				HeapTuple* value1 = dynamic_cast<HeapTuple*>((d1 != -1UL ? Runtime::stack[d1] : b).get());
				HeapTuple* value2 = dynamic_cast<HeapTuple*>((d2 != -1UL ? Runtime::stack[d2] : a).get());
				if (value1 && value2) {
					HeapTuple::Type t1 = value1->type;
					HeapContainer::heapIndex v1 = value1->reference;
					HeapTuple::Type t2 = value2->type;
					HeapContainer::heapIndex v2 = value2->reference;
					if (t1 == HeapTuple::Type::reference || t2 == HeapTuple::Type::reference) {
						bind(Runtime::stack[v1], v2);
					} else {
						HeapFunctor* fn1 = dynamic_cast<HeapFunctor*>(Runtime::stack[v1].get());
						HeapFunctor* fn2 = dynamic_cast<HeapFunctor*>(Runtime::stack[v2].get());
						if (fn1 && fn2) {
							if (fn1->name == fn2->name && fn1->parameters == fn2->parameters) {
								for (int i = 1; i <= fn1->parameters; ++ i) {
									pushdownList.push(v1 + i);
									pushdownList.push(v2 + i);
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
		HeapContainer::heapIndex address = dereference(Runtime::registers[registerIndex]);
		if (HeapTuple* value = dynamic_cast<HeapTuple*>((address != -1UL ? Runtime::stack[address] : Runtime::registers[registerIndex]).get())) {
			HeapTuple::Type type = value->type;
			switch (type) {
				case HeapTuple::Type::reference: {
					HeapContainer::heapIndex index = Runtime::stack.size();
					Runtime::stack.push_back(std::unique_ptr<HeapTuple>(new HeapTuple(HeapTuple::Type::compoundTerm, index + 1)));
					Runtime::stack.push_back(functor.copy());
					bind((address != -1UL ? Runtime::stack[address] : Runtime::registers[registerIndex]), index);
					Runtime::mode = Mode::write;
					break;
				}
				case HeapTuple::Type::compoundTerm: {
					HeapContainer::heapIndex reference = value->reference;
					if (HeapFunctor* fnc = dynamic_cast<HeapFunctor*>(Runtime::stack[reference].get())) {
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
				Runtime::registers[registerIndex] = Runtime::stack[Runtime::unificationIndex]->copy();
				break;
			case Mode::write:
				HeapTuple header(HeapTuple::Type::reference, Runtime::stack.size());
				Runtime::stack.push_back(header.copy());
				Runtime::registers[registerIndex] = header.copy();
				break;
		}
		++ Runtime::unificationIndex;
		++ Runtime::nextInstruction;
	}
	
	void UnifyValueInstruction::execute() {
		switch (Runtime::mode) {
			case Mode::read:
				unify(Runtime::registers[registerIndex], Runtime::stack[Runtime::unificationIndex]);
				break;
			case Mode::write:
				Runtime::stack.push_back(Runtime::registers[registerIndex]->copy());
				break;
		}
		++ Runtime::unificationIndex;
		++ Runtime::nextInstruction;
	}
	
	void PushVariableToAllInstruction::execute() {
		HeapTuple header(HeapTuple::Type::reference, Runtime::stack.size());
		Runtime::stack.push_back(header.copy());
		Runtime::registers[registerIndex] = header.copy();
		Runtime::registers[argumentIndex] = header.copy();
		++ Runtime::nextInstruction;
	}
	
	void CopyRegisterToArgumentInstruction::execute() {
		Runtime::registers[argumentIndex] = Runtime::registers[registerIndex]->copy();
		++ Runtime::nextInstruction;
	}
	
	void CopyArgumentToRegisterInstruction::execute() {
		Runtime::registers[registerIndex] = Runtime::registers[argumentIndex]->copy();
		++ Runtime::nextInstruction;
	}
	
	void UnifyRegisterAndArgumentInstruction::execute() {
		unify(Runtime::registers[registerIndex], Runtime::registers[argumentIndex]);
		++ Runtime::nextInstruction;
	}
	
	void CallInstruction::execute() {
		std::string label = functor.toString();
		if (Runtime::labels.find(label) != Runtime::labels.end()) {
			Runtime::nextInstruction = Runtime::labels[label];
		} else {
			throw UnificationError("Tried to jump to an inexistent label.", __FILENAME__, __func__, __LINE__);
		}
	}
	
	void ProceedInstruction::execute() {
		// The proceed instruction simply terminates the program by jumping to the end of the set of instructions
		Runtime::nextInstruction = Runtime::instructions.size();
	}
}
