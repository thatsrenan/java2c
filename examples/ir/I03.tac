=== IR (TAC) ===
global contador: int = 0

func soma(a: int, b: int) -> int
    temps: t0: int
    t0 = a + b
    return t0

func incrementar() -> void
    temps: t0: int
    t0 = contador + 1
    contador = t0

func main() -> void
    temps: t0: int
    param 10
    param 20
    t0 = call soma, 2
    resultado = t0
    call incrementar, 0
    print resultado
    print contador

