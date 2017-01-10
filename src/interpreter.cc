#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include "parser.hh"

#define DEBUG true

namespace AST {
	struct TermNode {
		Term* term;
		std::shared_ptr<TermNode> parent;
		Epilog::HeapReference reg;
		std::string name;
		std::string symbol;
		std::vector<std::shared_ptr<TermNode>> children;
		
		TermNode(Term* term, std::shared_ptr<TermNode> parent) : term(term), parent(parent) { }
	};
	
	void topologicalSort(std::vector<std::shared_ptr<TermNode>>& allocations, std::shared_ptr<TermNode> current) {
		for (auto& child : current->children) {
			topologicalSort(allocations, child);
		}
		if (current->parent != nullptr && (current->parent->parent == nullptr || dynamic_cast<CompoundTerm*>(current->term))) {
			allocations.push_back(current);
		}
	}
	
	std::pair<std::unordered_set<std::string>, std::unordered_map<std::string, Epilog::HeapReference>> findVariablePermanence(CompoundTerm* head, pegmatite::ASTList<CompoundTerm>* goals, bool forcePermanence) {
		std::unordered_map<std::string, int> appearances;
		std::queue<Term*> terms;
		if (head != nullptr) {
			terms.push(head);
		}
		pegmatite::ASTList<CompoundTerm> emptyBody;
		if (goals == nullptr) {
			goals = &emptyBody;
		}
		for (std::unique_ptr<CompoundTerm>& goal : *goals) {
			std::unordered_set<std::string> variables;
			terms.push(goal.get());
			while (!terms.empty()) {
				Term* term = terms.front(); terms.pop();
				if (CompoundTerm* compoundTerm = dynamic_cast<CompoundTerm*>(term)) {
					for (auto& parameter : compoundTerm->parameterList->parameters) {
						terms.push(parameter.get());
					}
				} else if (Variable* variable = dynamic_cast<Variable*>(term)) {
					variables.insert(variable->toString());
				}
			}
			for (auto& symbol : variables) {
				if (appearances.find(symbol) == appearances.end()) {
					appearances[symbol] = 0;
				}
				++ appearances[symbol];
			}
		}
		
		std::unordered_set<std::string> temporaries;
		std::unordered_map<std::string, Epilog::HeapReference> permanents;
		Epilog::HeapReference::heapIndex index = 0;
		for (auto& appearance : appearances) {
			if (appearance.second > 1 || forcePermanence) {
				permanents[appearance.first] = Epilog::HeapReference(Epilog::StorageArea::environment, index ++);
			} else {
				temporaries.insert(appearance.first);
			}
		}
		return std::make_pair(temporaries, permanents);
	}
	
	std::tuple<std::shared_ptr<TermNode>, std::vector<std::shared_ptr<TermNode>>, std::unordered_map<std::string, Epilog::HeapReference>> buildAllocationTree(std::pair<std::unordered_set<std::string>, std::unordered_map<std::string, Epilog::HeapReference>> permanence, CompoundTerm* head) {
		std::unordered_set<std::string> temporaries = permanence.first;
		std::unordered_map<std::string, Epilog::HeapReference> permanents = permanence.second;
		std::vector<std::shared_ptr<TermNode>> registers;
		std::queue<std::shared_ptr<TermNode>> terms;
		std::unordered_map<std::string, Epilog::HeapReference> allocations;
		std::unordered_map<std::string, std::shared_ptr<TermNode>> variableNodes;
		std::shared_ptr<TermNode> root(new TermNode(head, nullptr));
		terms.push(root);
		Epilog::HeapReference::heapIndex nextRegister = 0;
		// Breadth-first search through the tree
		while (!terms.empty()) {
			std::shared_ptr<TermNode> node(terms.front()); terms.pop();
			std::shared_ptr<TermNode> parent(node->parent);
			std::shared_ptr<TermNode> baseNode(node);
			Term* term = node->term;
			Epilog::HeapReference reg(Epilog::StorageArea::reg, nextRegister);
			bool allocateNewNode = true;
			bool assignedNextRegister = true;
			if (CompoundTerm* compoundTerm = dynamic_cast<CompoundTerm*>(term)) {
				node->name = compoundTerm->name;
				node->symbol = node->name + "/" + std::to_string(compoundTerm->parameterList->parameters.size());
				for (auto& parameter : compoundTerm->parameterList->parameters) {
					terms.push(std::shared_ptr<TermNode>(new TermNode(parameter.get(), node)));
				}
			} else if (Variable* variable = dynamic_cast<Variable*>(term)) {
				node->name = variable->toString();
				node->symbol = node->name; 
				auto previous = allocations.find(node->symbol);
				if (previous != allocations.end()) {
					// If this variable symbol has been seen before, use the register already allocated to it, using the node already in use for that variable.
					reg = allocations[node->symbol];
					baseNode = variableNodes[node->symbol];
					allocateNewNode = false;
					assignedNextRegister = false;
				} else {
					if (parent != nullptr && parent->parent != nullptr) {
						// If this is a new variable, and is not an argument variable, use a new node for that variable.
						// If the variable is not a permanent one, we need to allocate a new temporary register for it, otherwise we use the permanent register.
						bool isPermanentVariable = permanents.find(node->symbol) != allocations.end();
						reg = !isPermanentVariable ? reg : permanents[node->symbol];
						allocations[node->symbol] = reg;
						variableNodes[node->symbol] = node;
						assignedNextRegister = !isPermanentVariable;
					} else {
						// If this is a new variable and is an argument, also push a new non-argument variable, so that it has an associated temporary register.
						terms.push(std::shared_ptr<TermNode>(new TermNode(node->term, node)));
					}
				}
			} else {
				throw Epilog::RuntimeException("Found a term of an unknown type in the query.", __FILENAME__, __func__, __LINE__);
			}
			node->reg = reg;
			if (parent != nullptr) {
				parent->children.push_back(allocateNewNode ? node : baseNode);
			}
			// If the node was assigned a new register, and wasn't a pre-existing variable
			if (assignedNextRegister && parent != nullptr) {
				registers.push_back(node);
				++ nextRegister;
			}
		}
		return std::make_tuple(root, registers, allocations);
	}
	
	void pushInstruction(Interpreter::Context& context, Epilog::Instruction* instruction) {
		if (instruction != nullptr) {
			Epilog::Runtime::instructions.insert(Epilog::Runtime::instructions.begin() + (context.insertionAddress ++), std::unique_ptr<Epilog::Instruction>(instruction));
		}
	}
	
	void printHeap() {
		std::cerr << "Stack (" << Epilog::Runtime::heap.size() << "):" << std::endl;
		Epilog::Runtime::heap.print();
		std::cerr << "Registers (" << Epilog::Runtime::registers.size() << "):" << std::endl;
		Epilog::Runtime::registers.print();
	}
	
	typedef typename std::function<Epilog::Instruction*(std::shared_ptr<TermNode>, std::unordered_map<std::string, Epilog::HeapReference>&)> instructionGenerator;
	
	std::pair<Epilog::BoundsCheckedVector<Epilog::Instruction>::size_type, std::unordered_map<std::string, Epilog::HeapReference>> generateInstructionsForClause(Interpreter::Context& context, bool dependentAllocations, std::pair<std::unordered_set<std::string>, std::unordered_map<std::string, Epilog::HeapReference>> permanence, std::unordered_set<std::string>& encounters, CompoundTerm* head, instructionGenerator unseenArgumentVariable, instructionGenerator unseenRegisterVariable, instructionGenerator seenArgumentVariable, instructionGenerator seenRegisterVariable, instructionGenerator compoundTerm, instructionGenerator conclusion) {
		
		auto tuple = buildAllocationTree(permanence, head);
		std::shared_ptr<TermNode> root(std::get<0>(tuple));
		std::vector<std::shared_ptr<TermNode>> registers(std::get<1>(tuple));
		std::unordered_map<std::string, Epilog::HeapReference> allocations = std::get<2>(tuple);
		
		if (DEBUG) {
			std::cerr << "Temporary register allocation for clause " << head->toString() << ":" << std::endl;
			for (std::vector<std::shared_ptr<TermNode>>::size_type i = 0; i < registers.size(); ++ i) {
				std::shared_ptr<TermNode> node(registers[i]);
				std::cerr << "\t" << (node->parent != nullptr && node->parent->parent != nullptr ? "T" : "A") << i << "(" << node->name << ")" << std::endl;
			}
		}
		
		Epilog::BoundsCheckedVector<Epilog::Instruction>::size_type startAddress = Epilog::Runtime::instructions.size();
		
		// Use the runtime instructions to build this structure on the heap
		while (Epilog::Runtime::registers.size() < registers.size()) {
			Epilog::Runtime::registers.push_back(nullptr);
		}
		std::deque<std::pair<std::shared_ptr<TermNode>, bool>> terms;
		std::stack<std::pair<std::shared_ptr<TermNode>, bool>> reverse;
		if (dependentAllocations) {
			std::vector<std::shared_ptr<TermNode>> orderedAllocations;
			topologicalSort(orderedAllocations, root);
			for (std::shared_ptr<TermNode> allocation : orderedAllocations) {
				terms.push_back(std::make_pair(allocation, false));
			}
		} else {
			terms.push_back(std::make_pair(root, false));
		}
		while (!terms.empty()) {
			auto pair = terms.front(); terms.pop_front();
			std::shared_ptr<TermNode> node(pair.first);
			std::shared_ptr<TermNode> parent(node->parent);
			bool treatAsVariable = pair.second;
			if (treatAsVariable || dynamic_cast<Variable*>(node->term)) {
				if ((!dependentAllocations && treatAsVariable) || encounters.find(node->symbol) == encounters.end()) {
					if (parent->parent == nullptr) {
						pushInstruction(context, unseenArgumentVariable(node, allocations));
					} else {
						pushInstruction(context, unseenRegisterVariable(node, allocations));
					}
					encounters.insert(node->symbol);
				} else {
					if (parent->parent == nullptr) {
						pushInstruction(context, seenArgumentVariable(node, allocations));
					} else {
						pushInstruction(context, seenRegisterVariable(node, allocations));
					}
				}
			} else if (dynamic_cast<CompoundTerm*>(node->term)) {
				if (parent != nullptr) {
					pushInstruction(context, compoundTerm(node, allocations));
					encounters.insert(node->symbol);
				}
				for (std::shared_ptr<TermNode> child : node->children) {
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
		pushInstruction(context, conclusion(root, allocations));
		
		return std::make_pair(startAddress, allocations);
	}
	
	void Clauses::interpret(Interpreter::Context& context) {
		for (auto& clause : clauses) {
			clause->interpret(context);
		}
	}
	
	std::pair<Epilog::BoundsCheckedVector<Epilog::Instruction>::size_type, std::unordered_map<std::string, Epilog::HeapReference>> generateHeadInstructionsForClause(Interpreter::Context& context, std::pair<std::unordered_set<std::string>, std::unordered_map<std::string, Epilog::HeapReference>> permanence, std::unordered_set<std::string>& encounters, CompoundTerm* head, bool proceedAtEnd) {
		auto unseenArgumentVariable = [] (std::shared_ptr<TermNode> node, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::CopyArgumentToRegisterInstruction(allocations[node->symbol], node->reg); };
		auto unseenRegisterVariable = [] (std::shared_ptr<TermNode> node, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::UnifyVariableInstruction(node->reg); };
		auto seenArgumentVariable = [] (std::shared_ptr<TermNode> node, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::UnifyRegisterAndArgumentInstruction(allocations[node->symbol], node->reg); };
		auto seenRegisterVariable = [] (std::shared_ptr<TermNode> node, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::UnifyValueInstruction(node->reg); };
		auto compoundTerm = [] (std::shared_ptr<TermNode> node, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::UnifyCompoundTermInstruction(Epilog::HeapFunctor(node->name, node->children.size()), node->reg); };
		instructionGenerator conclusion;
		if (proceedAtEnd) {
			conclusion = [] (std::shared_ptr<TermNode> root, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::ProceedInstruction(); };
		} else {
			conclusion = [] (std::shared_ptr<TermNode> root, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return nullptr; };
		}
		
		return generateInstructionsForClause(context, false, permanence, encounters, head, unseenArgumentVariable, unseenRegisterVariable, seenArgumentVariable, seenRegisterVariable, compoundTerm, conclusion);
	}
	
	std::pair<Epilog::BoundsCheckedVector<Epilog::Instruction>::size_type, std::unordered_map<std::string, Epilog::HeapReference>> generateBodyInstructionsForClause(Interpreter::Context& context, std::pair<std::unordered_set<std::string>, std::unordered_map<std::string, Epilog::HeapReference>> permanence, std::unordered_set<std::string>& encounters, CompoundTerm* head) {
		auto unseenArgumentVariable = [] (std::shared_ptr<TermNode> node, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::PushVariableToAllInstruction(allocations[node->symbol], node->reg); };
		auto unseenRegisterVariable = [] (std::shared_ptr<TermNode> node, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::PushVariableInstruction(node->reg); };
		auto seenArgumentVariable = [] (std::shared_ptr<TermNode> node, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::CopyRegisterToArgumentInstruction(allocations[node->symbol], node->reg); };
		auto seenRegisterVariable = [] (std::shared_ptr<TermNode> node, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::PushValueInstruction(node->reg); };
		auto compoundTerm = [] (std::shared_ptr<TermNode> node, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::PushCompoundTermInstruction(Epilog::HeapFunctor(node->name, node->children.size()), node->reg); };
		auto conclusion = [] (std::shared_ptr<TermNode> root, std::unordered_map<std::string, Epilog::HeapReference>& allocations) -> Epilog::Instruction* { return new Epilog::CallInstruction(Epilog::HeapFunctor(root->name, root->children.size())); };
		
		return generateInstructionsForClause(context, true, permanence, encounters, head, unseenArgumentVariable, unseenRegisterVariable, seenArgumentVariable, seenRegisterVariable, compoundTerm, conclusion);
	}
	
	std::pair<Epilog::BoundsCheckedVector<Epilog::Instruction>::size_type, std::unordered_map<std::string, Epilog::HeapReference>> generateInstructionsForRule(Interpreter::Context& context, CompoundTerm* head, pegmatite::ASTList<CompoundTerm>* goals) {
		auto permanence = findVariablePermanence(head, goals, head == nullptr);
		auto startAddress = context.insertionAddress = Epilog::Runtime::instructions.size();
		
		if (DEBUG) {
			std::cerr << "Permanent register allocation: " << std::endl;
			for (auto& pair : permanence.second) {
				std::string symbol = pair.first;
				Epilog::HeapReference& ref = pair.second;
				std::cerr << "\t" << "P" << ref.index << "(" << symbol << ")" << std::endl;
			}
		}
		
		if (head != nullptr) {
			std::string symbol = head->name + "/" + std::to_string(head->parameterList->parameters.size());
			auto previous = context.functorClauses.find(symbol);
			if (previous == context.functorClauses.end()) {
				Epilog::Runtime::labels[symbol] = context.insertionAddress;
				context.functorClauses.emplace(symbol, Interpreter::FunctorClause(startAddress, startAddress));
			} else {
				auto& functorClause = previous->second;
				if (functorClause.startAddresses.size() == 1) {
					// A single clause with this functor has been defined.
					context.insertionAddress = functorClause.endAddress + 1;
					Epilog::Runtime::instructions.insert(Epilog::Runtime::instructions.begin() + functorClause.startAddresses.front(), std::unique_ptr<Epilog::Instruction>(new Epilog::TryInitialClauseInstruction(context.insertionAddress)));
				} else {
					// Functors with this clause have already been defined.
					context.insertionAddress = functorClause.endAddress;
					Epilog::Runtime::instructions[functorClause.startAddresses.back()] = std::unique_ptr<Epilog::Instruction>(new Epilog::TryIntermediateClauseInstruction(context.insertionAddress));
				}
				// Change the insertion position
				startAddress = context.insertionAddress;
				functorClause.startAddresses.push_back(startAddress);
				pushInstruction(context, new Epilog::TryFinalClauseInstruction());
			}
		}
		if (goals != nullptr) {
			pushInstruction(context, new Epilog::AllocateInstruction(permanence.second.size()));
		}
		std::unordered_set<std::string> encounters;
		if (head != nullptr) {
			generateHeadInstructionsForClause(context, permanence, encounters, head, goals == nullptr);
		}
		if (goals != nullptr) {
			for (auto& goal : *goals) {
				generateBodyInstructionsForClause(context, permanence, encounters, goal.get());
			}
			pushInstruction(context, new Epilog::DeallocateInstruction());
		}
		
		if (head != nullptr) {
			std::string symbol = head->name + "/" + std::to_string(head->parameterList->parameters.size());
			context.functorClauses.find(symbol)->second.endAddress = context.insertionAddress;
			// Offset labels and start addresses of any clauses whose instructions were displaced by inserting this new clause
			if (context.insertionAddress != Epilog::Runtime::instructions.size()) {
				auto offset = context.insertionAddress - startAddress;
				for (auto& label : Epilog::Runtime::labels) {
					if (label.second > startAddress) {
						label.second += offset;
					}
				}
				for (auto& functorClause : context.functorClauses) {
					if (functorClause.first == symbol) {
						continue;
					}
					for (auto& clauseStartAddress : functorClause.second.startAddresses) {
						if (clauseStartAddress > startAddress) {
							clauseStartAddress += offset;
						}
					}
				}
			}
		}
		
		if (DEBUG) {
			std::cerr << "Instructions:" << std::endl;
			for (auto i = startAddress; i < context.insertionAddress; ++ i) {
				std::unique_ptr<Epilog::Instruction>& instruction = Epilog::Runtime::instructions[i]; 
				std::cerr << "\t" << instruction->toString() << std::endl;
			}
			std::cerr << std::endl;
		}
		
		return std::make_pair(startAddress, permanence.second);
	}
	
	void executeInstructions(Epilog::BoundsCheckedVector<Epilog::Instruction>::size_type startAddress, std::unordered_map<std::string, Epilog::HeapReference>& allocations) {
		// Execute the instructions
		if (DEBUG) {
			std::cerr << "Execute:" << std::endl;
		}
		Epilog::Runtime::nextInstruction = startAddress;
		Epilog::Runtime::nextGoal = Epilog::Runtime::instructions.size();
		while (Epilog::Runtime::nextInstruction < Epilog::Runtime::instructions.size()) {
			std::unique_ptr<Epilog::Instruction>& instruction = Epilog::Runtime::instructions[Epilog::Runtime::nextInstruction];
			if (DEBUG) {
				std::cerr << "\t" << instruction->toString() << std::endl;
			}
			if (Epilog::Runtime::nextInstruction == Epilog::Runtime::instructions.size() - 1) {
				// The last instruction is always a deallocate.
				// We want to print the bindings before they are removed from the stack.
				std::cerr << "Bindings:" << std::endl;
				for (auto& allocation : allocations) {
					std::cerr << "\t" << allocation.first << " = " << allocation.second.get()->trace() << std::endl;
				}
			}
			try {
				instruction->execute();
			} catch (const Epilog::UnificationError& error) {
				if (DEBUG) {
					error.print();
				}
				if (Epilog::Runtime::topChoicePoint != -1UL) {
					// Backtrack to the previous choice point
					Epilog::Runtime::nextInstruction = Epilog::Runtime::currentChoicePoint()->nextClause;
				} else {
					throw;
				}
			}
		}
	}
	
	void Fact::interpret(Interpreter::Context& context) {
		std::cerr << "Register fact: " << head->toString() << std::endl;
		generateInstructionsForRule(context, head.get(), nullptr);
	}
	
	void Rule::interpret(Interpreter::Context& context) {
		std::cerr << "Register rule: " << head->toString() << " :- " << body->toString() << std::endl;
		generateInstructionsForRule(context, head.get(), &body->goals);
	}
	
	void Query::interpret(Interpreter::Context& context) {
		std::cerr << "Register query: " << body->toString() << std::endl;
		auto pair = generateInstructionsForRule(context, nullptr, &body->goals);
		auto startAddress = pair.first;
		auto allocations = pair.second;
		executeInstructions(startAddress, allocations);
	}
}
