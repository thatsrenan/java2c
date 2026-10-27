public class Main {
    static int dobro(int x) {
        return x * 2;
    }

    static double media(int a, double b, float c) {
        return (a + b + c) / 3.0;
    }

    public static void main(String[] args) {
        int r = dobro(dobro(2)) * 2 + dobro(3);
        double m = media(r, 2.5, 1.5f);
        boolean ok = dobro(1) == 2 && !(m < 0);
    }
}
