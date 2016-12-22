#include <unordered_map>
#include <queue>
#include <unordered_set>
#include "parser.hh"
#include "runtime.hh"

namespace AST {
	struct TermNode {
		Term* term;
		int reg;
		std::string name;
		std::vector<TermNode*> children;
		
		TermNode(Term* term) : term(term) { }
	};
	
	typedef typename std::pair<TermNode*, TermNode*> termPair;
	
	void topologicalSort(std::vector<TermNode*>& allocations, TermNode* current) {
		for (TermNode* child : current->children) {
			topologicalSort(allocations, child);
		}
		allocations.push_back(current);
	}
	
	std::pair<TermNode*, std::vector<TermNode*>> buildAllocationTree(Term* rootTerm) {
		std::vector<TermNode*> registers;
		std::queue<termPair> terms;
		std::unordered_map<std::string, TermNode*> assignments;
		TermNode* root = new TermNode(rootTerm);
		terms.push(termPair(root, nullptr));
		int reg;
		int nextRegister = 0;
		// Breadth-first search through the query
		while (!terms.empty()) {
			termPair pair = terms.front();
			TermNode* node = pair.first;
			TermNode* baseNode = node;
			TermNode* parent = pair.second;
			Term* term = node->term;
			reg = nextRegister;
			if (CompoundTerm* compoundTerm = dynamic_cast<CompoundTerm*>(term)) {
				node->name = compoundTerm->name;
				for (auto& parameter : compoundTerm->parameterList->parameters) {
					terms.push(termPair(new TermNode(parameter.get()), node));
				}
			} else if (Variable* variable = dynamic_cast<Variable*>(term)) {
				node->name = variable->toString();
				auto previous = assignments.find(node->name);
				if (previous == assignments.end()) {
					assignments[node->name] = node;
				} else {
					baseNode = previous->second;
					reg = baseNode->reg;
				}
			} else if (Number* number = dynamic_cast<Number*>(term)) {
				node->name = number->toString();
			} else {
				throw Epilog::RuntimeException("Found a term of an unknown type in the query.", __FILENAME__, __func__, __LINE__);
			}
			node->reg = reg;
			if (parent != nullptr) {
				parent->children.push_back(baseNode);
			}
			// If the node was assigned a new register, and wasn't a pre-existing variable
			if (reg == nextRegister) {
//				std::cerr << (node->reg + 1) << " = " << node->name << std::endl;
				registers.push_back(node);
				++ nextRegister;
			} else {
				delete node;
			}
			terms.pop();
		}
		return std::make_pair(root, registers);
	}
	
	void Clauses::interpret(Interpreter::Context& context) {
		for (auto& clause : clauses) {
			clause->interpret(context);
		}
	}
	
	void Fact::interpret(Interpreter::Context& context) {
		std::cerr << "Register fact: " << head->toString() << std::endl;
		auto pair = buildAllocationTree(head.get());
		std::vector<TermNode*> registers = pair.second;
		
		// Use the runtime instructions to build this structure on the heap
		while (Epilog::Runtime::registers.size() < registers.size()) {
			Epilog::Runtime::registers.push_back(nullptr);
		}
		std::unordered_set<int> instructions;
		for (TermNode* node : registers) {
			if (dynamic_cast<CompoundTerm*>(node->term)) {
				std::cerr << "get_structure " << node->name << ", x" << (node->reg) << std::endl;
				Epilog::unifyStructure(Epilog::HeapFunctor(node->name, node->children.size()), Epilog::Runtime::registers[node->reg]);
				instructions.insert(node->reg);
				for (TermNode* child : node->children) {
					if (instructions.find(child->reg) == instructions.end()) {
						std::cerr << "unify_variable x" << child->reg << std::endl;
						Epilog::unifyVariable(Epilog::Runtime::registers[child->reg]);
						instructions.insert(child->reg);
					} else {
						std::cerr << "unify_value x" << child->reg << std::endl;
						Epilog::unifyValue(Epilog::Runtime::registers[child->reg]);
					}
				}
			}
		}
		
		// Free the memory associated with each node
		for (TermNode* node : registers) {
			delete node;
		}
		// Use the runtime commands now to build this structure on the heap, and set a register value to point to its address
	}
	
	void Rule::interpret(Interpreter::Context& context) {
		std::cerr << "Register rule: " << head->toString() << std::endl;
	}
	
	void Query::interpret(Interpreter::Context& context) {
		std::cerr << "Register query: " << head->toString() << std::endl;
		auto pair = buildAllocationTree(head.get());
		TermNode* root = pair.first;
		std::vector<TermNode*> registers = pair.second;
		// Depth-first search through the query to perform a topological ordering
		std::vector<TermNode*> allocations;
		topologicalSort(allocations, root);
		
		// Use the runtime instructions to build this structure on the heap
		while (Epilog::Runtime::registers.size() < registers.size()) {
			Epilog::Runtime::registers.push_back(nullptr);
		}
		std::unordered_set<int> instructions;
		for (TermNode* node : allocations) {
			if (dynamic_cast<CompoundTerm*>(node->term)) {
				std::cerr << "put_structure " << node->name << ", x" << node->reg << std::endl;
				Epilog::pushCompoundTerm(Epilog::HeapFunctor(node->name, node->children.size()), Epilog::Runtime::registers[node->reg]);
				instructions.insert(node->reg);
				for (TermNode* child : node->children) {
					if (instructions.find(child->reg) == instructions.end()) {
						std::cerr << "set_variable x" << child->reg << std::endl;
						Epilog::pushVariable(Epilog::Runtime::registers[child->reg]);
						instructions.insert(child->reg);
					} else {
						std::cerr << "set_value x" << child->reg << std::endl;
						Epilog::pushValue(Epilog::Runtime::registers[child->reg]);
					}
				}
			}
		}
		
		std::cerr << "Stack (" << Epilog::Runtime::stack.size() << "):" << std::endl;
		Epilog::Runtime::stack.print();
		std::cerr << "Registers (" << Epilog::Runtime::registers.size() << "):" << std::endl;
		Epilog::Runtime::registers.print();
		
		// Free the memory associated with each node
		for (TermNode* node : registers) {
			delete node;
		}
	}
}
