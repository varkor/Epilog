#include <iomanip>

#pragma once

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

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
	
	enum Mode { read, write };
	
	struct HeapContainer {
		typedef typename std::vector<HeapContainer>::size_type heapIndex;
		
		virtual std::unique_ptr<HeapContainer> copy() const = 0;
		
		virtual std::string toString() {
			return "container";
		}
		
		// Use a virtual destructor to ensure HeapContainer is polymorphic, and we can use dynamic_cast
		virtual ~HeapContainer() = default;
		
		HeapContainer() = default;
		
		HeapContainer(const HeapContainer&) = default;
		
		HeapContainer& operator=(const HeapContainer& other) = default;
	};
	
	struct HeapTuple: HeapContainer {
		enum class Type { compoundTerm, reference };
		Type type;
		heapIndex reference;
		
		virtual std::unique_ptr<HeapContainer> copy() const {
			return std::unique_ptr<HeapTuple>(new HeapTuple(*this));
		}
		
		virtual std::string toString() {
			return std::string("(") + (type == Type::compoundTerm ? "compound term" : "reference") + ", " + std::to_string(reference) + ")";
		}
		
		HeapTuple(Type type, heapIndex reference) : type(type), reference(reference) { }
	};
	
	struct HeapFunctor: HeapContainer {
		std::string name;
		int parameters;
		
		virtual std::unique_ptr<HeapContainer> copy() const {
			return std::unique_ptr<HeapFunctor>(new HeapFunctor(*this));
		}
		
		virtual std::string toString() {
			return name + "/" + std::to_string(parameters);
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
	
	class StackHeap: public std::vector<std::unique_ptr<HeapContainer>> {
		public:
		std::unique_ptr<HeapContainer>& operator[] (const HeapContainer::heapIndex index) {
			if (index >= size()) {
				throw RuntimeException("Tried to access a stack index out of bounds.", __FILENAME__, __func__, __LINE__);
			}
			return std::vector<std::unique_ptr<HeapContainer>>::operator[](index);
		}
		
		void print() {
			for (HeapContainer::heapIndex i = 0; i < size(); ++ i) {
				std::cerr << std::setw(2) << i << ": " << (*this)[i]->toString() << std::endl;
			}
		}
	};
	
	class Runtime {
		public:
		// The global stack used to contain term structures used when unifying.
		static StackHeap stack;
		
		// The registers used to temporarily hold pointers when building queries or rules
		static StackHeap registers;
		
		static Mode mode;
		
		static HeapContainer::heapIndex S;
	};
}
