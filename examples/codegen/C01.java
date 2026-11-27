public class Expressoes {
    static int calc(int a, int b) {
        int x = a + b * 2;
        int y = (a - b) / 2;
        return x - y;
    }

    public static void main(String[] args) {
        int v = calc(7, 3);
        System.out.println(v);
        System.out.println(1 + 2 * 3 - 4 / 2);
        System.out.println((double) 5 / 2);
        System.out.println((int) 3.9);
    }
}
