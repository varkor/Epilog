#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include "parser.hh"
#include "runtime.hh"

#define DEBUG false

namespace AST {
	struct TermNode {
		Term* term;
		TermNode* parent;
		Epilog::HeapContainer::heapIndex reg;
		std::string name;
		std::string symbol;
		std::vector<TermNode*> children;
		
		TermNode(Term* term, TermNode* parent) : term(term), parent(parent) { }
	};
	
	void topologicalSort(std::vector<TermNode*>& allocations, TermNode* current) {
		for (TermNode* child : current->children) {
			topologicalSort(allocations, child);
		}
		if (current->parent != nullptr && (current->parent->parent == nullptr || dynamic_cast<CompoundTerm*>(current->term))) {
			allocations.push_back(current);
		}
	}
	
	std::tuple<TermNode*, std::vector<TermNode*>, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>> buildAllocationTree(CompoundTerm* rootTerm) {
		std::vector<TermNode*> registers;
		std::queue<TermNode*> terms;
		std::unordered_map<std::string, Epilog::HeapContainer::heapIndex> assignments;
		std::unordered_map<std::string, TermNode*> variableNodes;
		TermNode* root = new TermNode(rootTerm, nullptr);
		terms.push(root);
		Epilog::HeapContainer::heapIndex reg;
		Epilog::HeapContainer::heapIndex nextRegister = 0;
		// Breadth-first search through the tree
		while (!terms.empty()) {
			TermNode* node = terms.front(); terms.pop();
			TermNode* parent = node->parent;
			TermNode* baseNode = node;
			Term* term = node->term;
			reg = nextRegister;
			if (CompoundTerm* compoundTerm = dynamic_cast<CompoundTerm*>(term)) {
				node->name = compoundTerm->name;
				node->symbol = node->name + "/" + std::to_string(compoundTerm->parameterList->parameters.size());
				for (auto& parameter : compoundTerm->parameterList->parameters) {
					terms.push(new TermNode(parameter.get(), node));
				}
			} else if (Variable* variable = dynamic_cast<Variable*>(term)) {
				node->name = variable->toString();
				node->symbol = node->name; 
				auto previous = assignments.find(node->symbol);
				if (previous != assignments.end()) {
					reg = assignments[node->symbol];
					baseNode = variableNodes[node->symbol];
				} else if (parent != nullptr && parent->parent != nullptr) {
					assignments[node->symbol] = nextRegister;
					variableNodes[node->symbol] = node;
				} else {
					terms.push(new TermNode(node->term, node));
				}
			} else {
				throw Epilog::RuntimeException("Found a term of an unknown type in the query.", __FILENAME__, __func__, __LINE__);
			}
			node->reg = reg;
			if (parent != nullptr) {
				parent->children.push_back(reg == nextRegister ? node : baseNode);
			}
			// If the node was assigned a new register, and wasn't a pre-existing variable
			if (reg == nextRegister) {
				if (parent != nullptr) {
					registers.push_back(node);
					++ nextRegister;
				}
			} else {
				delete node;
			}
		}
		return std::make_tuple(root, registers, assignments);
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
	
	typedef typename std::function<Epilog::Instruction*(TermNode*, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>&)> instructionGenerator;
	
	Epilog::BoundsCheckedVector<Epilog::Instruction>::size_type generateInstructionsFromClause(bool dependentAllocations, CompoundTerm* rootTerm, instructionGenerator unseenArgumentVariable, instructionGenerator unseenRegisterVariable, instructionGenerator seenArgumentVariable, instructionGenerator seenRegisterVariable, instructionGenerator compoundTerm, instructionGenerator conclusion) {
		auto tuple = buildAllocationTree(rootTerm);
		TermNode* root = std::get<0>(tuple);
		std::vector<TermNode*> registers = std::get<1>(tuple);
		std::unordered_map<std::string, Epilog::HeapContainer::heapIndex> variableRegisters = std::get<2>(tuple);
		
		if (DEBUG) {
			std::cerr << "Register allocation:" << std::endl;
			for (std::vector<TermNode*>::size_type i = 0; i < registers.size(); ++ i) {
				TermNode* node = registers[i];
				std::cerr << "R" << i << "(" << node->name << ")" << std::endl;
			}
		}
		
		Epilog::BoundsCheckedVector<Epilog::Instruction>::size_type startAddress = Epilog::Runtime::instructions.size();
		
		// Use the runtime instructions to build this structure on the heap
		while (Epilog::Runtime::registers.size() < registers.size()) {
			Epilog::Runtime::registers.push_back(nullptr);
		}
		std::deque<std::pair<TermNode*, bool>> terms;
		std::stack<std::pair<TermNode*, bool>> reverse;
		std::unordered_set<std::string> instructions;
		if (dependentAllocations) {
			std::vector<TermNode*> allocations;
			topologicalSort(allocations, root);
			for (TermNode* allocation : allocations) {
				terms.push_back(std::make_pair(allocation, false));
			}
		} else {
			terms.push_back(std::make_pair(root, false));
		}
		while (!terms.empty()) {
			auto pair = terms.front(); terms.pop_front();
			TermNode* node = pair.first;
			TermNode* parent = node->parent;
			bool treatAsVariable = pair.second;
			if (treatAsVariable || dynamic_cast<Variable*>(node->term)) {
				if ((!dependentAllocations && treatAsVariable) || instructions.find(node->symbol) == instructions.end()) {
					if (parent->parent == nullptr) {
						pushInstruction(unseenArgumentVariable(node, variableRegisters));
					} else {
						pushInstruction(unseenRegisterVariable(node, variableRegisters));
					}
					instructions.insert(node->symbol);
				} else {
					if (parent->parent == nullptr) {
						pushInstruction(seenArgumentVariable(node, variableRegisters));
					} else {
						pushInstruction(seenRegisterVariable(node, variableRegisters));
					}
				}
			} else if (dynamic_cast<CompoundTerm*>(node->term)) {
				if (parent != nullptr) {
					pushInstruction(compoundTerm(node, variableRegisters));
					instructions.insert(node->symbol);
				}
				for (TermNode* child : node->children) {
					if (parent != nullptr && dynamic_cast<CompoundTerm*>(child->term)) {
						if (!dependentAllocations) {
							terms.push_back(std::make_pair(child, false));
						}
						reverse.push(std::make_pair(child, true));
					} else if (!dependentAllocations || parent != nullptr) {
						reverse.push(std::make_pair(child, false));
					}
				}
				while (!reverse.empty()) {
					terms.push_front(reverse.top()); reverse.pop();
				}
			} else {
				throw Epilog::RuntimeException("Found a term of an unknown type in the query.", __FILENAME__, __func__, __LINE__);
			}
		}
		pushInstruction(conclusion(root, variableRegisters));
		
		// Free the memory associated with each node
		for (TermNode* node : registers) {
			delete node;
		}
		
		if (DEBUG) {
			std::cerr << "Instructions:" << std::endl;
			for (auto i = startAddress; i < Epilog::Runtime::instructions.size(); ++ i) {
				std::unique_ptr<Epilog::Instruction>& instruction = Epilog::Runtime::instructions[i]; 
				std::cerr << "\t" << instruction->toString() << std::endl;
			}
		}
		
		return startAddress;
	}
	
	void Clauses::interpret(Interpreter::Context& context) {
		for (auto& clause : clauses) {
			clause->interpret(context);
		}
	}
	
	void Fact::interpret(Interpreter::Context& context) {
		std::cerr << "Register fact: " << head->toString() << std::endl;
		
		auto unseenArgumentVariable = [] (TermNode* node, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::CopyArgumentToRegisterInstruction(variableRegisters[node->symbol], node->reg); };
		auto unseenRegisterVariable = [] (TermNode* node, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::UnifyVariableInstruction(node->reg); };
		auto seenArgumentVariable = [] (TermNode* node, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::UnifyRegisterAndArgumentInstruction(variableRegisters[node->symbol], node->reg); };
		auto seenRegisterVariable = [] (TermNode* node, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::UnifyValueInstruction(node->reg); };
		auto compoundTerm = [] (TermNode* node, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::UnifyCompoundTermInstruction(Epilog::HeapFunctor(node->name, node->children.size()), node->reg); };
		auto conclusion = [] (TermNode* root, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::ProceedInstruction(); };
		
		Epilog::Runtime::labels[head->name + "/" + std::to_string(head->parameterList->parameters.size())] = Epilog::Runtime::instructions.size();
		generateInstructionsFromClause(false, head.get(), unseenArgumentVariable, unseenRegisterVariable, seenArgumentVariable, seenRegisterVariable, compoundTerm, conclusion);
	}
	
	void Rule::interpret(Interpreter::Context& context) {
		std::cerr << "Register rule: " << head->toString() << std::endl;
	}
	
	void Query::interpret(Interpreter::Context& context) {
		std::cerr << "Register query: " << head->toString() << std::endl;
		
		auto unseenArgumentVariable = [] (TermNode* node, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::PushVariableToAllInstruction(variableRegisters[node->symbol], node->reg); };
		auto unseenRegisterVariable = [] (TermNode* node, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::PushVariableInstruction(node->reg); };
		auto seenArgumentVariable = [] (TermNode* node, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::CopyRegisterToArgumentInstruction(variableRegisters[node->symbol], node->reg); };
		auto seenRegisterVariable = [] (TermNode* node, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::PushValueInstruction(node->reg); };
		auto compoundTerm = [] (TermNode* node, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::PushCompoundTermInstruction(Epilog::HeapFunctor(node->name, node->children.size()), node->reg); };
		auto conclusion = [] (TermNode* root, std::unordered_map<std::string, Epilog::HeapContainer::heapIndex>& variableRegisters) -> Epilog::Instruction* { return new Epilog::CallInstruction(Epilog::HeapFunctor(root->name, root->children.size())); };
		
		Epilog::Runtime::nextInstruction = generateInstructionsFromClause(true, head.get(), unseenArgumentVariable, unseenRegisterVariable, seenArgumentVariable, seenRegisterVariable, compoundTerm, conclusion);
		
		// Execute the instructions
		if (DEBUG) {
			std::cerr << "Execute:" << std::endl;
		}
		while (Epilog::Runtime::nextInstruction < Epilog::Runtime::instructions.size()) {
			std::unique_ptr<Epilog::Instruction>& instruction = Epilog::Runtime::instructions[Epilog::Runtime::nextInstruction];
			if (DEBUG) {
				std::cerr << instruction->toString() << std::endl;
			}
			instruction->execute();
		}
	}
}
