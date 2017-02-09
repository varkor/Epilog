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
% A demonstration of unification on lists and basic numeric operations.
length([], 0).
length([H | T], N) :- length(T, M), is(N, +(M, 1)).
?- length([1, 2, 3, [4 | [5 | [6]]]], N), write('The length of the list is: '), writeln(N).
