=== IR (TAC) ===
global base: int = 7
global dobro: int

func get() -> int
    return dobro

func __init_globals() -> void
    temps: t0: int
    t0 = base * 2
    dobro = t0

