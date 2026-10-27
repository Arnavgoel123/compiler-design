// One sigmoid neuron with squared-error loss against target 1.
// d(loss)/dw and d(loss)/db are the gradients used in training.
input w, b, x;
z = w * x + b;
s = 1 / (1 + exp(-z));
loss = (s - 1)^2;
