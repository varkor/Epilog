namespace Epilog {
	void pushInstruction(Interpreter::Context& context, Instruction* instruction) {
		if (instruction != nullptr) {
			Runtime::instructions.insert(Runtime::instructions.begin() + (context.insertionAddress ++), std::unique_ptr<Instruction>(instruction));
		}
	}
	
	namespace StandardLibrary {
		std::unordered_map<std::string, std::function<void(Interpreter::Context& context)>> functions = {
			{ "nl/0", [] (Interpreter::Context& context) {
				pushInstruction(context, new CommandInstruction("nl"));
				pushInstruction(context, new ProceedInstruction());
			} },
			{ "write/1", [] (Interpreter::Context& context) {
				pushInstruction(context, new CommandInstruction("print"));
				pushInstruction(context, new ProceedInstruction());
			} }
		};
	}
}
