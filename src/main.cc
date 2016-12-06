#include <iostream>

void usage(const char command[]) {
	std::cerr << "usage: " << command << std::endl
		<< "Epilog is currently in development. Its functionality may not be as expected." << std::endl;
}

int main(int argc, char **argv) {
	usage(argv[0]);
}
