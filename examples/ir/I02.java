int i = 0;

while (i < 10) {
    if (i % 2 == 0) {
        i++;
        continue;
    }
    if (i == 7) {
        break;
    }
    System.out.println(i);
    i++;
}

for (int j = 0; j < 3; j++) {
    System.out.println(j);
}

do {
    i--;
} while (i > 0);
