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

        if (resultado > 20) {
            System.out.println("Resultado maior que 20");
        } else {
            System.out.println("Resultado menor ou igual a 20");
        }

        for (int i = 0; i < 5; i++) {
            incrementar();
        }

        do {
            contador--;
        } while (contador > 2);

        System.out.println(contador);
    }
}
