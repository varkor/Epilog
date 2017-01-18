namespace Epilog {
	void pushInstruction(Interpreter::Context& context, Instruction* instruction);
	
	struct StandardLibrary {
		static std::unordered_map<std::string, std::function<void(Interpreter::Context& context)>> functions;
		static std::unordered_map<std::string, std::function<void()>> commands;
	};
}
