public class Controle {
    public static void main(String[] args) {
        int total = 0;
        for (int i = 0; i < 10; i++) {
            if (i == 3) { continue; }
            if (i == 7) { break; }
            total = total + i;
        }
        System.out.println(total);

        int x = 5;
        if (x > 10) {
            System.out.println("grande");
        } else if (x > 3) {
            System.out.println("medio");
        } else {
            System.out.println("pequeno");
        }

        int d = 0;
        do { d = d + 2; } while (d < 5);
        System.out.println(d);
    }
}
