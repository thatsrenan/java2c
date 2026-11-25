=== IR (TAC) ===
global global: int = 0

func mudaGlobal() -> void
    global = 9

func main() -> void
    locals: a: int, b: int, cond: int
    a = 14
    b = 15
    print 15
    a = 8
    print 8
    global = 3
    call mudaGlobal, 0
    print global
    cond = 4
    cond = 5
    print 5

