=== IR (TAC) ===
global g: int = 0

func f(a: int, b: int) -> int
    temps: t0: int
    t0 = a + b
    return t0

func k(x: int) -> int
    temps: t0: int
    t0 = x * 2
    return t0

func muda() -> int
    temps: t0: int
    t0 = g + 1
    g = t0
    return g

func main() -> void
    locals: r: int, s: int, t: int
    temps: t0: int, t1: int, t2: int, t3: int, t4: int, t5: int, t6: int, t7: int, t8: int, t9: int, t10: int
    param 2
    t0 = call k, 1
    param 1
    param t0
    t1 = call f, 2
    r = t1
    param r
    t2 = call k, 1
    param 3
    t3 = call k, 1
    param t3
    t4 = call k, 1
    param t2
    param t4
    t5 = call f, 2
    s = t5
    t6 = g
    t7 = call muda, 0
    param t6
    param t7
    t8 = call f, 2
    t = t8
    t9 = r + s
    t10 = t9 + t
    print t10

