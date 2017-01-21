% Parameterless fact
fact.
% Base clauses of differing arity
another().
unary(One).
ternary(one, two, three).
% A compound fact
top(middle(bottom, bottom), bottom).
% A simple rule
rule(one, Two, a) :- fact(Two).
% A rule involving a list
length([], 0).
length([H | T], N) :- length(T, M), is(N, +(M, 1)).
