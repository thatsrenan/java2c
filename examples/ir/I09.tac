=== IR (TAC) ===
global total: int = 1
global t0_1: int = 7
global unsigned_1: int = 2

func g(x: int) -> int
    temps: t0: int
    t0 = x + 1
    return t0

func f(g_1: int, total_1: int) -> int
    locals: t1_1: int
    temps: t0: int
    t0 = g_1 + total_1
    t1_1 = t0
    return t1_1

func main() -> void
    locals: total_1: int, y: int, y_1: double, y_1_1: int
    temps: t0: int, t1: int, t2: int, t3: int
    print total
    total_1 = 5
    y = 1
    print 1
    y_1 = 2.5
    print 2.5
    y_1_1 = 3
    param 5
    param 3
    t0 = call f, 2
    param t0_1
    t1 = call g, 1
    t2 = t0 + t1
    t3 = t2 + unsigned_1
    print t3

