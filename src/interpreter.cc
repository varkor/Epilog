#include <unordered_map>
#include <queue>
#include <unordered_set>
#include "parser.hh"
#include "runtime.hh"

#define DEBUG false

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
				registers.push_back(node);
				++ nextRegister;
			} else {
				delete node;
			}
			terms.pop();
		}
		return std::make_pair(root, registers);
	}
	
	void pushInstruction(Epilog::Instruction* instruction) {
		Epilog::Runtime::instructions.push_back(std::unique_ptr<Epilog::Instruction>(instruction));
	}
	
	void printHeap() {
		std::cerr << "Stack (" << Epilog::Runtime::stack.size() << "):" << std::endl;
		Epilog::Runtime::stack.print();
		std::cerr << "Registers (" << Epilog::Runtime::registers.size() << "):" << std::endl;
		Epilog::Runtime::registers.print();
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
				pushInstruction(new Epilog::UnifyCompoundTermInstruction(Epilog::HeapFunctor(node->name, node->children.size()), node->reg));
				instructions.insert(node->reg);
				for (TermNode* child : node->children) {
					if (instructions.find(child->reg) == instructions.end()) {
						pushInstruction(new Epilog::UnifyVariableInstruction(child->reg));
						instructions.insert(child->reg);
					} else {
						pushInstruction(new Epilog::UnifyValueInstruction(child->reg));
					}
				}
			}
		}
		
		// Free the memory associated with each node
		for (TermNode* node : registers) {
			delete node;
		}
		
		// Execute the instructions
		for (std::unique_ptr<Epilog::Instruction>& instruction : Epilog::Runtime::instructions) {
			if (DEBUG) {
				std::cerr << instruction->toString() << std::endl;
			}
			instruction->execute();
		}
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
				pushInstruction(new Epilog::PushCompoundTermInstruction(Epilog::HeapFunctor(node->name, node->children.size()), node->reg));
				instructions.insert(node->reg);
				for (TermNode* child : node->children) {
					if (instructions.find(child->reg) == instructions.end()) {
						pushInstruction(new Epilog::PushVariableInstruction(child->reg));
						instructions.insert(child->reg);
					} else {
						pushInstruction(new Epilog::PushValueInstruction(child->reg));
					}
				}
			}
		}
		
		// Free the memory associated with each node
		for (TermNode* node : registers) {
			delete node;
		}
		
		// Execute the instructions
		for (std::unique_ptr<Epilog::Instruction>& instruction : Epilog::Runtime::instructions) {
			if (DEBUG) {
				std::cerr << instruction->toString() << std::endl;
			}
			instruction->execute();
		}
	}
}
