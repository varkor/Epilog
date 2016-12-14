#pragma once

namespace Epilog {
	void registerClause(const std::string& name, struct BaseClause* clause);
	
	struct BaseClause {
		const char* name;
	};
}
