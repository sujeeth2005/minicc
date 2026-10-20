// expect: 15
// Shadowing: the inner x is a different variable from the outer x.
int main() {
    int x = 5;
    int y = 0;
    {
        int x = 10;
        y = x;
    }
    return x + y;
}
