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
		
		// Numbers: sequences of one or more digits.
		Rule number = +digit;
		
		// Value: a number or a bracketed expression.
		Rule value = number | '(' >> expression >> ')';
		
		// Multiplication: number * number.
		Rule multiply = value >> '*' >> value;
		
		// Expression: multiplication or number.
		Rule expression = multiply | value;
		
		// Clause.
		Rule clause = expression >> '.';
		
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
