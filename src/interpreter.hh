#include "runtime.hh"

namespace Epilog {
	namespace Interpreter {
		struct FunctorClause {
			// A structure entailing a block of instructions containing the definition for each clause with a certain functor.
			std::vector<Instruction::instructionReference> startAddresses;
			Instruction::instructionReference endAddress;
			
			FunctorClause(Instruction::instructionReference startAddress, Instruction::instructionReference endAddress) : endAddress(endAddress) {
				startAddresses.push_back(startAddress);
			}
		};
		
		class Context {
			public:
			std::unordered_map<std::string, FunctorClause> functorClauses;
			Instruction::instructionReference insertionAddress = 0;
		};
	}
	
	// These functions are made visible to external classes so that dynamic instruction generation is possible.
	namespace AST {
		void executeInstructions(BoundsCheckedVector<Instruction>::size_type startAddress, std::unordered_map<std::string, HeapReference>* allocations);
	}
	
	Instruction::instructionReference pushInstruction(Interpreter::Context& context, Instruction* instruction);
}
