public class Logica {
    static int chamadas;

    static boolean toca() {
        chamadas = chamadas + 1;
        return true;
    }

    public static void main(String[] args) {
        chamadas = 0;
        boolean a = false && toca();
        boolean b = true || toca();
        boolean c = (1 < 2) && (3 > 2);
        System.out.println(a);
        System.out.println(b);
        System.out.println(c);
        System.out.println(chamadas);
        System.out.println(!a && b);
    }
}
