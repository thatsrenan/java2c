=== IR (TAC) ===
func main() -> void
    locals: d: double, i: int, c: char, f: float, r: double
    temps: t0: double, t1: double, t2: double, t3: int, t4: int, t5: int, t6: char, t7: float, t8: double, t9: double
    d = 1.5
    t0 = d + 1
    d = t0
    d = 2.0
    t1 = d + 1
    d = t1
    i = 10
    t2 = i + 2.5
    t3 = (int) t2
    i = t3
    t4 = i - 1
    i = t4
    c = 97
    t5 = c + 1
    t6 = (char) t5
    c = t6
    f = 1.0
    t7 = f * 2
    f = t7
    t8 = d * i
    t9 = t8 + f
    r = t9
    print r

