// expect: 3628800
int main() {
    int n = 10;
    int f = 1;
    while (n > 1) {
        f = f * n;
        n = n - 1;
    }
    return f;
}
