namespace Epilog {
	Instruction::instructionReference pushInstruction(Interpreter::Context& context, Instruction* instruction);
	
	struct StandardLibrary {
		static std::unordered_map<std::string, std::function<void(Interpreter::Context& context, HeapReference::heapIndex& registers)>> functions;
		static std::unordered_map<std::string, std::function<void()>> commands;
	};
}
