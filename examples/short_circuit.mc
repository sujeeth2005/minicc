// expect: 1
// && and || only evaluate the right-hand side when needed (10 / z would trap).
int main() {
    int z = 0;
    int a = z && (10 / z);
    int b = 1 || (10 / z);
    return a == 0 && b == 1;
}
