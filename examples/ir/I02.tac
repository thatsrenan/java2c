=== IR (TAC) ===
func main() -> void
    i = 0
Lwhile_cond0:
    t0 = i < 10
    ifFalse t0 goto Lwhile_end1
    t1 = i % 2
    t2 = t1 == 0
    ifFalse t2 goto Lendif3
    t3 = i + 1
    i = t3
    goto Lwhile_cond0
Lendif3:
    t4 = i == 7
    ifFalse t4 goto Lendif5
    goto Lwhile_end1
Lendif5:
    print i
    t5 = i + 1
    i = t5
    goto Lwhile_cond0
Lwhile_end1:
    j = 0
Lfor_cond6:
    t6 = j < 3
    ifFalse t6 goto Lfor_end8
    print j
Lfor_cont7:
    t7 = j + 1
    j = t7
    goto Lfor_cond6
Lfor_end8:
Ldo_body9:
    t8 = i - 1
    i = t8
Ldo_cond10:
    t9 = i > 0
    if t9 goto Ldo_body9
Ldo_end11:

