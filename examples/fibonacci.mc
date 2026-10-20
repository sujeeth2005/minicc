// expect: 55
// Iterative Fibonacci: fib(10).
int main() {
    int a = 0;
    int b = 1;
    int i = 0;
    while (i < 10) {
        int next = a + b;
        a = b;
        b = next;
        i = i + 1;
    }
    return a;
}
