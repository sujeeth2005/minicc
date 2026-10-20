// expect: 25
// Counts primes below 100 by trial division.
int main() {
    int count = 0;
    int n = 2;
    while (n < 100) {
        int isPrime = 1;
        int d = 2;
        while (d * d <= n) {
            if (n % d == 0) { isPrime = 0; }
            d = d + 1;
        }
        if (isPrime) { count = count + 1; }
        n = n + 1;
    }
    return count;
}
