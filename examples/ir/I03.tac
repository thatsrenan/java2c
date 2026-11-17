=== IR (TAC) ===
global contador: int = 0

func soma(a: int, b: int) -> int
    t0 = a + b
    return t0

func incrementar() -> void
    t1 = contador + 1
    contador = t1

func main() -> void
    param 10
    param 20
    t2 = call soma, 2
    resultado = t2
    call incrementar, 0
    print resultado
    print contador

