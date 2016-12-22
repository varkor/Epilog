#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include "runtime.hh"

namespace Epilog {
	StackHeap Runtime::stack;
	StackHeap Runtime::registers;
	BoundsCheckedVector<Instruction> Runtime::instructions;
	Mode Runtime::mode;
	BoundsCheckedVector<Instruction>::size_type Runtime::nextInstruction;
	HeapContainer::heapIndex Runtime::S;
	
	void PushCompoundTermInstruction::execute() {
		HeapTuple header(HeapTuple::Type::compoundTerm, Runtime::stack.size() + 1);
		Runtime::stack.push_back(header.copy());
		Runtime::stack.push_back(functor.copy());
		reg = header.copy();
	}
	
	void PushVariableInstruction::execute() {
		Runtime::stack.push_back(std::unique_ptr<HeapTuple>(new HeapTuple(HeapTuple::Type::reference, Runtime::stack.size())));
		reg = std::unique_ptr<HeapTuple>(new HeapTuple(HeapTuple::Type::reference, Runtime::stack.size()));
	}
	
	void PushValueInstruction::execute() {
		Runtime::stack.push_back(reg->copy());
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
	
	void unify(std::unique_ptr<HeapContainer>& container, HeapContainer::heapIndex b) {
		std::stack<HeapContainer::heapIndex> pushdownList;
		pushdownList.push(dereference(container));
		pushdownList.push(b);
		while (!pushdownList.empty()) {
			HeapContainer::heapIndex d1 = dereference(pushdownList.top()); pushdownList.pop();
			HeapContainer::heapIndex d2 = dereference(pushdownList.top()); pushdownList.pop();
			if (d1 != d2) {
				HeapTuple* value1 = dynamic_cast<HeapTuple*>(Runtime::stack[d1].get());
				HeapTuple* value2 = dynamic_cast<HeapTuple*>((d2 != -1UL ? Runtime::stack[d2] : container).get());
				if (value1 && value2) {
					HeapTuple::Type t1 = value1->type;
					HeapContainer::heapIndex v1 = value1->reference;
					HeapTuple::Type t2 = value2->type;
					HeapContainer::heapIndex v2 = value2->reference;
					if (t1 == HeapTuple::Type::reference || t2 == HeapTuple::Type::reference) {
						bind(Runtime::stack[d1], d2);
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
		HeapContainer::heapIndex address = dereference(reg);
		if (HeapTuple* value = dynamic_cast<HeapTuple*>((address != -1UL ? Runtime::stack[address] : reg).get())) {
			HeapTuple::Type type = value->type;
			switch (type) {
				case HeapTuple::Type::reference: {
					HeapContainer::heapIndex index = Runtime::stack.size();
					Runtime::stack.push_back(std::unique_ptr<HeapTuple>(new HeapTuple(HeapTuple::Type::compoundTerm, index + 1)));
					Runtime::stack.push_back(functor.copy());
					bind((address != -1UL ? Runtime::stack[address] : reg), index);
					Runtime::mode = Mode::write;
					break;
				}
				case HeapTuple::Type::compoundTerm: {
					HeapContainer::heapIndex reference = value->reference;
					if (HeapFunctor* fnc = dynamic_cast<HeapFunctor*>(Runtime::stack[reference].get())) {
						if (fnc->name == functor.name && fnc->parameters == functor.parameters) {
							Runtime::S = reference + 1;
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
	}
	
	void UnifyVariableInstruction::execute() {
		switch (Runtime::mode) {
			case Mode::read:
				reg = Runtime::stack[Runtime::S]->copy();
				break;
			case Mode::write:
				Runtime::stack.push_back(std::unique_ptr<HeapTuple>(new HeapTuple(HeapTuple::Type::reference, Runtime::stack.size())));
				reg = std::unique_ptr<HeapTuple>(new HeapTuple(HeapTuple::Type::reference, Runtime::stack.size()));
				break;
		}
		++ Runtime::S;
	}
	
	void UnifyValueInstruction::execute() {
		switch (Runtime::mode) {
			case Mode::read:
				unify(reg, Runtime::S);
				break;
			case Mode::write:
				Runtime::stack.push_back(reg->copy());
				break;
		}
		++ Runtime::S;
	}
	
}
