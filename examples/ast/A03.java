public class Main {
    static int f(int n, char c) {
        if (n > 0) {
            return n;
        } else if (n == 0) {
            return 0;
        } else {
            return -n;
        }
    }

    public static void main(String[] args) {
        int i;
        for (i = 0; i < 3; ++i) {
            continue;
        }
        for (;;) {
            break;
        }
        while (i != 0) {
            i -= 1;
        }
        System.out.println('x');
        System.out.println(2.5);
        System.out.println(f(i,
                             'y'));
        return;
    }
}
