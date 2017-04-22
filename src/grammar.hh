#pragma once

#include "Pegmatite/pegmatite.hh"

namespace Epilog {
	namespace Parser {
		using pegmatite::Rule;
		using pegmatite::operator""_E;
		using pegmatite::operator""_S;
		using pegmatite::ExprPtr;
		using pegmatite::BindAST;
		using pegmatite::any;
		using pegmatite::nl;
		
		struct EpilogGrammar {
			// Whitespace: spaces, tabs and newline characters.
			Rule whitespace = ' '_E | '\t' | nl('\n');
			
			// Comments: line and block.
			Rule comment =
			("/*"_E >> *(!ExprPtr("*/") >> (nl('\n') | any())) >> "*/") | // Block comment.
			'%'_E >> *(!ExprPtr('\n') >> any()) >> nl('\n'); // Line comment.
			
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
			Rule number = pegmatite::term(-"-"_E >> +digit);
			
			// Operators: special identifiers for built-ins.
			Rule oper = "=<"_E | '<' | "=>" | '>' | ".+-*/="_S;
			
			// Identifiers: names (for example, for facts or rules).
			Rule simpleIdentifier = pegmatite::term(lowercase >> *character) | oper | "[]";
			
			Rule identifier = simpleIdentifier | ('\'' >> *("\\'" | (!ExprPtr('\'') >> any())) >> '\'');
			
			Rule variableIdentifier = pegmatite::term(uppercase >> *character) | '_';
			
			// Variables.
			Rule variable = variableIdentifier;
			
			// List literals.
			Rule elements = term >> *(',' >> term);
			
			Rule list = '['_E >> elements >> -('|' >> term) >> ']';
			
			// Terms.
			Rule term = number | compoundTerm | variable | list | string;
			
			// Parameters: a comma-separated list of terms.
			Rule parameter = term;
			
			Rule parameters = -('(' >> -(parameter >> *(',' >> parameter)) >> ')');
			
			// Compound term: a name optionally followed by an argument list.
			// A set of empty brackets `()` is equivalent to having no brackets at all.
			Rule compoundTerm = identifier >> parameters;
			
			Rule modifier = "\\+"_E | "\\:";
			
			// Enriched compound term: a compound term that optionally has a modifier, such as \+ (not), which modifies the unification method for that term.
			Rule enrichedCompoundTerm = (-modifier) >> compoundTerm;
			
			// Compound terms: a series of goals.
			Rule compoundTerms = enrichedCompoundTerm >> *(',' >> enrichedCompoundTerm);
			
			// Fact.
			Rule fact = compoundTerm;
			
			// Rule.
			Rule rule = compoundTerm >> ":-" >> compoundTerms;
			
			// Query: a way by which we can invoke unification of rules without an interactive mode.
			Rule query = "?-"_E >> compoundTerms;
			
			// Clause: either a fact, a rule, or a query.
			Rule clause = (query | rule | fact) >> '.';
			
			// Clauses: a standard Epilog program is made up of a series of clauses.
			Rule clauses = *clause;
			
			// Singleton getter.
			static EpilogGrammar& get() {
				static EpilogGrammar grammar;
				return grammar;
			}
			
			// Avoid the possibility of accidental copying of the singleton.
			EpilogGrammar(EpilogGrammar const&) = delete;
			void operator=(EpilogGrammar const&) = delete;
			
			// EpilogGrammar should only be constructed via the getter.
			protected:
			EpilogGrammar() {}
		};
	}
}
