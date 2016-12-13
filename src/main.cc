#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include "parser.hh"

void usage(const char command[]) {
	std::cerr << "usage: " << command << " <file>" << std::endl;
}

int main(int argc, char *argv[]) {
	Parser::EpilogParser<int> parser;
	std::unique_ptr<AST::Expression<int>> root = 0;
	
	if (argc < 2) {
		usage(argv[0]);
		
		return EXIT_FAILURE;
	} else {
		pegmatite::AsciiFileInput input(open(argv[1], O_RDONLY));
		if (parser.parse(input, parser.grammar.expression, parser.grammar.ignored, pegmatite::defaultErrorReporter, root)) {
			int value = root->evaluate();
			std::cout << value << std::endl;
			
			return EXIT_SUCCESS;
		} else {
			return EXIT_FAILURE;
		}
	}
}
