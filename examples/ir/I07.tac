=== IR (TAC) ===
global x: double = 0.5

func usa(x_1: int) -> void
    temps: t0: int
    t0 = x_1 + 1
    x_1 = t0
    print x_1

func main() -> void
    locals: i: int, y: double, y_1: int
    temps: t0: boolean, t1: double, t2: int, t3: boolean, t4: boolean, t5: double
    i = 0
Lfor_cond0:
    t0 = i < 2
    ifFalse t0 goto Lfor_end2
    y = i
    t1 = y + 0.5
    y = t1
    t2 = i + 1
    i = t2
    goto Lfor_cond0
Lfor_end2:
    t3 = x > 0
    ifFalse t3 goto Lelse3
    y_1 = 3
    y_1 = 4
    goto Lendif4
Lelse3:
    t4 = x < 0
    ifFalse t4 goto Lendif5
    t5 = x - 1
    x = t5
Lendif5:
Lendif4:
    param 1
    call usa, 1

