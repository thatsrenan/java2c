public class Main {
    static int contador = 0;

    static int soma(int a, int b) {
        return a + b;
    }

    static void incrementar() {
        contador++;
    }

    public static void main(String[] args) {
        int resultado = soma(10, 20);
        incrementar();
        System.out.println(resultado);
        System.out.println(contador);
    }
}
