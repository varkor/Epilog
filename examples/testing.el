% Parameterless fact
fact.
% Base clauses of differing arity
another().
unary(One).
ternary(one, two, three).
% A compound base clause
top(middle(bottom, bottom), bottom).
% A simple rule
rule(one, Two, 3) :- fact(Two).
