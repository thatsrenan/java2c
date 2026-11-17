=== IR (TAC) ===
global x: double = 0.5

func usa(x: int) -> void
    temps: t0: int
    t0 = x + 1
    x = t0
    print x

func main() -> void
    temps: t0: boolean, t1: double, t2: int, t3: boolean, t4: int, t5: boolean, t6: double
    i = 0
Lfor_cond0:
    t0 = i < 2
    ifFalse t0 goto Lfor_end2
    y = i
    t1 = y + 0.5
    y = t1
Lfor_cont1:
    t2 = i + 1
    i = t2
    goto Lfor_cond0
Lfor_end2:
    t3 = x > 0
    ifFalse t3 goto Lelse3
    y = 3
    t4 = y + 1
    y = t4
    goto Lendif4
Lelse3:
    t5 = x < 0
    ifFalse t5 goto Lendif5
    t6 = x - 1
    x = t6
Lendif5:
Lendif4:
    param 1
    call usa, 1

