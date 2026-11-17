=== IR (TAC) ===
func main() -> void
    locals: a: int, b: int, c: boolean, d: int, e: double, f: int, g: boolean
    temps: t0: int, t1: int, t2: int, t3: int, t4: boolean, t5: boolean, t6: boolean, t7: boolean, t8: int, t9: int, t10: int, t11: double, t12: double, t13: double, t14: int, t15: boolean, t16: boolean
    t0 = 2 * 3
    t1 = 1 + t0
    a = t1
    t2 = a - 2
    t3 = t2 - 1
    b = t3
    t6 = ! true
    t5 = t6
    ifFalse t5 goto Land_false2
    t7 = a < b
    t5 = t7
    goto Land_end3
Land_false2:
    t5 = false
Land_end3:
    t4 = t5
    if t4 goto Lor_true0
    t4 = false
    goto Lor_end1
Lor_true0:
    t4 = true
Lor_end1:
    c = t4
    t8 = - a
    t9 = b + 1
    t10 = t8 * t9
    d = t10
    t11 = (double) a
    t12 = t11 / 2
    e = t12
    t13 = - e
    t14 = (int) t13
    f = t14
    t15 = a == b
    t16 = t15 != c
    g = t16

