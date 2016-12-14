#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include "parser.hh"

void usage(const char command[]) {
	std::cerr << "usage: " << command << " <file>" << std::endl;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		usage(argv[0]);
		
		return EXIT_FAILURE;
	} else {
		Parser::EpilogParser<int> parser;
		Interpreter::Context context;
		std::unique_ptr<AST::Clauses> root = 0;
		pegmatite::AsciiFileInput input(open(argv[1], O_RDONLY));
		if (parser.parse(input, parser.grammar.clauses, parser.grammar.ignored, pegmatite::defaultErrorReporter, root)) {
			root->interpret(context);
			return EXIT_SUCCESS;
		} else {
			return EXIT_FAILURE;
		}
	}
}
