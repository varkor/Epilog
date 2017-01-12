#include "runtime.hh"

namespace Epilog {
	namespace Interpreter {
		struct FunctorClause {
			// A structure entailing a block of instructions containing the definition for each clause with a certain functor.
			std::vector<Epilog::Instruction::instructionReference> startAddresses;
			Epilog::Instruction::instructionReference endAddress;
			
			FunctorClause(Epilog::Instruction::instructionReference startAddress, Epilog::Instruction::instructionReference endAddress) : endAddress(endAddress) {
				startAddresses.push_back(startAddress);
			}
		};
		
		class Context {
			public:
			std::unordered_map<std::string, FunctorClause> functorClauses;
			Epilog::Instruction::instructionReference insertionAddress;
		};
	}
}
