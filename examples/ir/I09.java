public class Nomes {
    static int total = 1;
    static int t0 = 7;
    static int unsigned = 2;

    static int g(int x) {
        return x + 1;
    }

    static int f(int g, int total) {
        int t1 = g + total;
        return t1;
    }

    public static void main(String[] args) {
        System.out.println(total);
        int total = 5;
        {
            int y = 1;
            System.out.println(y);
        }
        {
            double y = 2.5;
            System.out.println(y);
        }
        int y_1 = 3;
        System.out.println(f(total, y_1) + g(t0) + unsigned);
    }
}
