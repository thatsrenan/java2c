static int g = 1;
int x = g;
static void f() {
    g++;
}
x += 2;
f();
