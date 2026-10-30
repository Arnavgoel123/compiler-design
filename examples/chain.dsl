// Multiple equations: later equations reuse earlier results.
// Every assigned variable (a, b, y) is differentiated w.r.t. every input.
input x, z;
a = x * z;
b = a + log(z);
y = sin(a) * b + x^2;
