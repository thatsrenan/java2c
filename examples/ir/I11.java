public class Otimizacao {
    static int global;

    static void mudaGlobal() {
        global = 9;
    }

    public static void main(String[] args) {
        int a = 2 + 3 * 4;
        int b = a + 1;
        System.out.println(b);

        a = 8;
        System.out.println(a);

        global = 3;
        mudaGlobal();
        System.out.println(global);

        int cond = 4;
        if (cond == 4) {
            cond = 5;
        }
        System.out.println(cond);
    }
}
