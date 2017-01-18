#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include "parser.hh"
#include "runtime.hh"

using namespace Epilog;

void usage(const char command[]) {
	std::cerr << "usage: " << command << " <file>" << std::endl;
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		usage(argv[0]);
		
		return EXIT_FAILURE;
	} else {
		Parser::EpilogParser parser;
		std::unique_ptr<AST::Clauses> root;
		pegmatite::AsciiFileInput input(open(argv[1], O_RDONLY));
		if (parser.parse(input, parser.grammar.clauses, parser.grammar.ignored, pegmatite::defaultErrorReporter, root)) {
			try {
				Interpreter::Context context;
				root->interpret(context);
				std::cout << "true." << std::endl;
			} catch (const Epilog::UnificationError& error) {
				std::cout << "false." << std::endl;
				return EXIT_FAILURE;
			} catch (const Epilog::Exception& exception) {
				exception.print();
				return EXIT_FAILURE;
			}
			return EXIT_SUCCESS;
		} else {
			return EXIT_FAILURE;
		}
	}
}
