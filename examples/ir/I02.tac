=== IR (TAC) ===
func main() -> void
    temps: t0: boolean, t1: int, t2: boolean, t3: int, t4: boolean, t5: int, t6: boolean, t7: int, t8: int, t9: boolean
    i = 0
Lwhile_cond0:
    t0 = i < 10
    ifFalse t0 goto Lwhile_end1
    t1 = i % 2
    t2 = t1 == 0
    ifFalse t2 goto Lendif2
    t3 = i + 1
    i = t3
    goto Lwhile_cond0
Lendif2:
    t4 = i == 7
    ifFalse t4 goto Lendif3
    goto Lwhile_end1
Lendif3:
    print i
    t5 = i + 1
    i = t5
    goto Lwhile_cond0
Lwhile_end1:
    j = 0
Lfor_cond4:
    t6 = j < 3
    ifFalse t6 goto Lfor_end6
    print j
Lfor_cont5:
    t7 = j + 1
    j = t7
    goto Lfor_cond4
Lfor_end6:
Ldo_body7:
    t8 = i - 1
    i = t8
Ldo_cond8:
    t9 = i > 0
    if t9 goto Ldo_body7
Ldo_end9:

