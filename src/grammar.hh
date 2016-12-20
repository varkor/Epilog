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
		
		// Identifiers: names (for example, for facts or rules).
		Rule identifier = pegmatite::term(lowercase >> *character);
		
		Rule variableIdentifier = pegmatite::term((uppercase | '_') >> *character);
		
		// Variable.
		Rule variable = variableIdentifier;
		
		// Term.
		Rule term = compoundTerm | variable | number;
		
		// Parameters: a comma-separated list of terms.
		Rule parameter = term;
		
		Rule parameters = -("(" >> -(parameter >> *(',' >> parameter)) >> ")");
		
		// Compound term: a name optionally followed by an argument list.
		// A set of empty brackets `()` is equivalent to having no brackets at all.
		Rule compoundTerm = identifier >> parameters;
		
		// Rule.
		Rule rule = compoundTerm >> ":-" >> compoundTerm;
		
		// Clause: either a fact, a rule, or a query.
		Rule fact = compoundTerm;
		
		// Query: a way by which we can invoke unification of rules without an interactive mode.
		Rule query = "?-"_E >> compoundTerm;
		
		Rule clause = (query | rule | fact) >> '.';
		
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
