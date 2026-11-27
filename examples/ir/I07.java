public class Escopo {
    static double x = 0.5;

    static void usa(int x) {
        x++;
        System.out.println(x);
    }

    public static void main(String[] args) {
        for (int i = 0; i < 2; i++) {
            double y = i;
            y += 0.5;
        }
        if (x > 0) {
            int y = 3;
            y++;
        } else if (x < 0) {
            x--;
        }
        usa(1);
    }
}
