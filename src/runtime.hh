#pragma once

namespace Epilog {
	class RuntimeException {
		public:
		std::string message;
		RuntimeException(std::string message) : message(message) { }
	};
	
	struct HeapContainer {
		typedef typename std::vector<HeapContainer>::size_type heapIndex;
		
		// Use a virtual destructor to ensure HeapContainer is polymorphic, and we can use dynamic_cast
		virtual ~HeapContainer() = default;
		
		HeapContainer() = default;
		
		HeapContainer(const HeapContainer&) = default;
		
		HeapContainer& operator=(const HeapContainer& other) = default;
	};
	
	struct HeapFunctor: HeapContainer {
		std::string name;
		int parameters;
		
		HeapFunctor(std::string name, int parameters) : name(name), parameters(parameters) { }
	};
	
	struct HeapTuple: HeapContainer {
		enum Type { structure, reference };
		Type type;
		heapIndex ref;
		HeapTuple(Type type, heapIndex ref) : type(type), ref(ref) { }
	};
}
