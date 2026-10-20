// expect: 50
// The example from the project plan: everything folds away at -O1.
int main() {
    int x = 10;
    int y = 20;
    int result = x + y * 2;
    return result;
}
