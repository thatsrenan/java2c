public class Main {
    static int classifica(int idade) {
        if (idade >= 18) {
            return 2;
        } else if (idade >= 12) {
            return 1;
        } else {
            return 0;
        }
    }

    public static void main(String[] args) {
        int x = 0;
        while (x < 10) {
            if (x == 5) {
                break;
            }
            x++;
        }
        for (int i = 0; i < 10; i++) {
            if (i % 2 == 0) {
                continue;
            }
            System.out.println(i);
        }
        for (;;) {
            break;
        }
        do {
            --x;
        } while (x > 0);
    }
}
