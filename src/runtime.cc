#include <string>
#include <iostream>
#include <unordered_map>
#include "runtime.hh"

namespace Epilog {
	static std::unordered_map<std::string, struct Clause*> clauseTable;
	
	void registerClause(const std::string& name, struct Clause* clause) {
		clauseTable[name] = clause;
	}
}
