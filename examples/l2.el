% A program with facts, a rule and a query.
q(a, b).
r(b, c).
p(X, Y) :- q(X, Z), r(Z, Y).
?- p(U, V).
