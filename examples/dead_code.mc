// expect: 7
// Constant branch + dead stores: the optimizer reduces this to `return 7`.
int main() {
    int unused = 99 * 3;
    int flag = 1;
    int r = 0;
    if (flag) {
        r = 3 + 4;
    } else {
        r = unused;
    }
    return r;
}
