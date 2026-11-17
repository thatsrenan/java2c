public class Ordem {
    static int a = 1;

    static int le() {
        return a * 10;
    }

    static int b = le();
    static int c = 5;
    static char letra = 'a' + 2;
    static boolean ativo;
    static double taxa;

    static char proxima(char x) {
        return (char) (x + 1);
    }

    public static void main(String[] args) {
        char d = 66;
        System.out.println(b);
        System.out.println(c);
        System.out.println(proxima(d));
        System.out.println(letra);
        System.out.println(ativo);
        System.out.println(taxa);
    }
}
