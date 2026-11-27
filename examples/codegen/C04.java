public class Funcoes {
    static int fat(int n) {
        if (n <= 1) { return 1; }
        return n * fat(n - 1);
    }

    static int soma(int a, int b) {
        return a + b;
    }

    static void imprime(int x) {
        System.out.println(x);
    }

    public static void main(String[] args) {
        int f = fat(5);
        imprime(soma(f, soma(1, 2)));
        soma(1, 2);
        System.out.println("fim");
    }
}
