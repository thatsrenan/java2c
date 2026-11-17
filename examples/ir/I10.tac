=== IR (TAC) ===
global a: int = 1
global b: int = 0
global c: int = 0
global letra: char = 0
global ativo: boolean = false
global taxa: double = 0.0

func le() -> int
    temps: t0: int
    t0 = a * 10
    return t0

func proxima(x: char) -> char
    temps: t0: int, t1: char
    t0 = x + 1
    t1 = (char) t0
    return t1

func main() -> void
    locals: d: char
    temps: t0: int, t1: char
    t0 = call le, 0
    b = t0
    c = 5
    letra = 99
    d = 66
    print b
    print c
    param d
    t1 = call proxima, 1
    print t1
    print letra
    print ativo
    print taxa

