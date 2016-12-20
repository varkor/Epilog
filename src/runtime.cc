#include <iostream>
#include <vector>
#include <string>
#include "runtime.hh"

namespace Epilog {
	// The global stack used to contain term structures used when unifying.
	static std::vector<HeapContainer> stack;
	
	void putStructure(HeapFunctor& functor, HeapContainer& reg) {
		HeapContainer value = HeapTuple(HeapTuple::structure, stack.size() + 1);
		stack.push_back(value);
		stack.push_back(functor);
		reg = value;
	}
	
	void setVariable(HeapContainer& reg) {
		HeapContainer value = HeapTuple(HeapTuple::reference, stack.size());
		stack.push_back(value);
		reg = value;
	}
	
	void setValue(HeapContainer& reg) {
		stack.push_back(reg);
	}
	
	HeapContainer::heapIndex dereference(HeapContainer::heapIndex index) {
		if (HeapTuple* value = dynamic_cast<HeapTuple*>(&stack[index])) { // STORE
			HeapTuple::Type type = value->type;
			HeapContainer::heapIndex reference = value->reference;
			if (type == HeapTuple::Type::reference && reference != index) {
				return dereference(reference);
			} else { 
				return reference;
			}
		} else {
			throw RuntimeException("Tried to dereference a non-structure address on the stack as a structure.");
		}
	}
	
	void getStructure(HeapFunctor& functor, HeapContainer& reg);
	
	void unify_variable(HeapContainer& reg);
	
	void unify(HeapContainer::heapIndex a, HeapContainer::heapIndex b);
}
