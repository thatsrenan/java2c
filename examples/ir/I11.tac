=== IR (TAC) ===
global global: int = 0

func mudaGlobal() -> void
    global = 9

func main() -> void
    locals: a: int, b: int, cond: int
    temps: t0: int, t1: int, t2: int, t3: boolean
    t0 = 12
    t1 = 14
    a = 14
    t2 = 15
    b = 15
    print 15
    a = 8
    print 8
    global = 3
    call mudaGlobal, 0
    print global
    cond = 4
    t3 = true
    ifFalse true goto Lendif0
    cond = 5
Lendif0:
    print cond

