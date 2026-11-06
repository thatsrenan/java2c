public class Main {
    static int total;

    public static void zera() {
        total = 0;
    }

    public static void main(String[] args) {
        zera();
        total += 5;
        System.out.println(total);
    }
}
