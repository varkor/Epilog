% A program with multiples rules sharing the same functor.
p(X, a).
p(b, X).
p(X, Y) :- p(X, a), p(b, Y).

?- p(A, c).
