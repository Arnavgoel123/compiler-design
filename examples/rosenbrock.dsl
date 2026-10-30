// Rosenbrock function split into steps; gradient w.r.t. both inputs.
input x, y;
a = 1 - x;
b = y - x^2;
f = a^2 + 100 * b^2;
