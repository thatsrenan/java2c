public class Chamadas {
    static int g = 0;

    static int f(int a, int b) {
        return a + b;
    }

    static int k(int x) {
        return x * 2;
    }

    static int muda() {
        g = g + 1;
        return g;
    }

    public static void main(String[] args) {
        int r = f(1, k(2));
        int s = f(k(r), k(k(3)));
        int t = f(g, muda());
        System.out.println(r + s + t);
    }
}
