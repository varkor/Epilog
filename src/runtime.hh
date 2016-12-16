#pragma once

namespace Epilog {
	void registerClause(const std::string& name, struct Clause* clause);
	
	struct Clause {
		const char* name;
	};
	
	struct Fact: Clause { };
	
	struct Rule: Clause { };
}
