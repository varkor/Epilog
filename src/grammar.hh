#include "../lib/Pegmatite/pegmatite.hh"

namespace Parser {
	using pegmatite::Rule;
	using pegmatite::operator""_E;
	using pegmatite::operator""_S;
	using pegmatite::ExprPtr;
	using pegmatite::BindAST;
	using pegmatite::any;
	using pegmatite::nl;
	using pegmatite::trace;
	
	struct EpilogGrammar {
		
		// Whitespace: spaces, tabs and newline characters.
		Rule whitespace = ' '_E | '\t' | nl('\n');
		
		// Comments: line and block.
		Rule comment =
			("/*"_E >> *(!ExprPtr("*/") >> (nl('\n') | any())) >> "*/") | // Block comment.
			"%"_E >> *(!ExprPtr("\n") >> any()) >> nl('\n'); // Line comment.
		
		Rule ignored = *(comment | whitespace);
		
		// Digits: 0 to 9 inclusive.
		ExprPtr digit = '0'_E - '9';
		
		// Letters: a to z and A to Z.
		ExprPtr lowercase = 'a'_E - 'z';
		
		ExprPtr uppercase = 'A'_E - 'Z';
		
		ExprPtr letter = lowercase | uppercase;
		
		// Characters: digits, letters and underscores.
		ExprPtr character = letter | digit | '_';
		
		// Numbers: sequences of one or more digits.
		Rule number = +digit;
		
		// Value: a number or a bracketed expression.
		Rule value = number | '(' >> expression >> ')';
		
		// Multiplication: number * number.
		Rule multiply = value >> '*' >> value;
		
		// Expression: multiplication or number.
		Rule expression = multiply | value;
		
		// Identifiers: names (for example, for atoms or rules).
		Rule identifier = lowercase >> *character;
		
		// Variable.
		Rule variable = (uppercase | '_') >> *character;
		
		// Parameters: a comma-separated list of values or variables.
		Rule parameter = identifier;
		
		Rule parameters = -("(" >> -(parameter >> *(',' >> parameter)) >> ")");
		
		// Structure: a name optionally followed by an argument list.
		// A set of empty brackets `()` is equivalent to having no brackets at all.
		Rule structure = identifier >> parameters;
		
		// Rule.
		Rule rule = structure >> ":-" >> structure;
		
		// Clause: either a base clause, or a rule.
		Rule baseClause = structure;
		
		Rule clause = (baseClause | rule) >> '.';
		
		// Clauses: a standard Epilog program is made up of a series of clauses.
		Rule clauses = *clause;
		
		// Singleton getter.
		static const EpilogGrammar& get() {
			static EpilogGrammar grammar;
			return grammar;
		}
	
		// Avoid the possibility of accidental copying of the singleton.
		EpilogGrammar(EpilogGrammar const&) = delete;
		void operator=(EpilogGrammar const&) = delete;
		// EpilogGrammar should only be constructed via the getter.
		private:
		EpilogGrammar() {};
	};
}
