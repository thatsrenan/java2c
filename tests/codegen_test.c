#include "codegen.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Testes unitários do gerador C: constroem IR diretamente (sem lexer/parser)
 * e conferem o texto C emitido. Mesmo estilo de ir_opt_test.c. */

static int failures;
static int checks;

#define CHECK(condition) do { \
    checks++; \
    if (!(condition)) { \
        fprintf(stderr, "Falha na linha %d: %s\n", __LINE__, #condition); \
        failures++; \
    } \
} while (0)

#define CHECK_STR(haystack, needle) do { \
    const char *_h = (haystack); \
    checks++; \
    if (!_h || !strstr(_h, (needle))) { \
        fprintf(stderr, "Falha na linha %d: texto ausente: %s\n", \
                __LINE__, (needle)); \
        failures++; \
    } \
} while (0)

#define CHECK_NO_STR(haystack, needle) do { \
    const char *_h = (haystack); \
    checks++; \
    if (_h && strstr(_h, (needle))) { \
        fprintf(stderr, "Falha na linha %d: texto inesperado: %s\n", \
                __LINE__, (needle)); \
        failures++; \
    } \
} while (0)

static char *emit_source_mode(const IrProgram *program, int clean) {
    FILE *f = tmpfile();
    if (!f) {
        fprintf(stderr, "Falha ao criar arquivo temporario nos testes.\n");
        exit(1);
    }
    int rc = clean ? codegen_emit_clean(f, program) : codegen_emit(f, program);
    if (rc != 0) {
        fprintf(stderr, "codegen_emit falhou em um teste valido.\n");
        failures++;
        fclose(f);
        return NULL;
    }
    fflush(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (!buf) exit(1);
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "Falha ao ler codigo gerado nos testes.\n");
        failures++;
        buf[0] = '\0';
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static char *emit_source(const IrProgram *program) {
    return emit_source_mode(program, 0);
}

static void test_basics(void) {
    IrProgram *p = ir_program_new();
    ir_program_add_global(p, "g", TYPE_INT, 1, ir_const_int(3, TYPE_INT));

    IrFunc *f = ir_program_add_func(p, "soma", TYPE_INT);
    ir_func_add_param(f, "a", TYPE_INT);
    ir_func_add_param(f, "b", TYPE_INT);
    IrInstr *bin = ir_emit(f, IR_BINOP);
    bin->type = TYPE_INT;
    bin->binop = OP_ADD;
    bin->res = ir_temp(0, TYPE_INT);
    bin->arg1 = ir_var("a", TYPE_INT);
    bin->arg2 = ir_var("b", TYPE_INT);
    f->temp_count = 1;
    ir_emit(f, IR_RETURN)->arg1 = ir_temp(0, TYPE_INT);

    IrFunc *main = ir_program_add_func(p, "main", TYPE_VOID);
    ir_emit(main, IR_PARAM)->arg1 = ir_var("g", TYPE_INT);
    ir_emit(main, IR_PARAM)->arg1 = ir_const_int(4, TYPE_INT);
    IrInstr *call = ir_emit(main, IR_CALL);
    call->type = TYPE_INT;
    call->res = ir_temp(0, TYPE_INT);
    call->arg1 = ir_func_ref("soma");
    call->arg2 = ir_const_int(2, TYPE_INT);
    main->temp_count = 1;
    ir_emit(main, IR_PRINT)->arg1 = ir_temp(0, TYPE_INT);

    char *src = emit_source(p);
    if (src) {
        CHECK_STR(src, "#include <stdbool.h>");
        CHECK_STR(src, "#include <stdint.h>");
        CHECK_STR(src, "int32_t g = 3;");
        CHECK_STR(src, "int32_t soma(int32_t a, int32_t b);");
        CHECK_STR(src, "int32_t soma(int32_t a, int32_t b)\n{");
        CHECK_STR(src, "t0 = a + b;");
        CHECK_STR(src, "return t0;");
        CHECK_STR(src, "t0 = soma(g, 4);");
        CHECK_STR(src, "printf(\"%d\\n\", t0);");
        CHECK_STR(src, "void __j2c_main(void)");
        CHECK_STR(src, "int main(void)\n{\n    __j2c_main();");

        const char *proto = strstr(src, "int32_t soma(int32_t a, int32_t b);");
        const char *def = strstr(src, "int32_t soma(int32_t a, int32_t b)\n{");
        CHECK(proto && def && proto < def);
    }
    free(src);
    ir_program_free(p);
}

static void test_flow_and_prints(void) {
    IrProgram *p = ir_program_new();
    IrFunc *f = ir_program_add_func(p, "fluxo", TYPE_VOID);

    ir_emit(f, IR_LABEL)->res = ir_label("L1");
    IrInstr *branch = ir_emit(f, IR_IF_FALSE);
    branch->arg1 = ir_var("flag", TYPE_BOOLEAN);
    branch->res = ir_label("L1");

    IrInstr *text = ir_emit(f, IR_PRINT);
    text->print_string = strdup("100% \\\"ok\\\"\\n");

    IrInstr *boolean = ir_emit(f, IR_PRINT);
    boolean->arg1 = ir_var("flag", TYPE_BOOLEAN);

    IrInstr *character = ir_emit(f, IR_PRINT);
    character->arg1 = ir_const_int(65, TYPE_CHAR);

    IrInstr *real = ir_emit(f, IR_PRINT);
    real->arg1 = ir_const_real(2.0, TYPE_DOUBLE);

    ir_emit(f, IR_GOTO)->res = ir_label("L1");

    char *src = emit_source(p);
    if (src) {
        CHECK_STR(src, "L1: ;");
        CHECK_STR(src, "if (!flag) goto L1;");
        CHECK_STR(src, "goto L1;");
        CHECK_STR(src, "printf(\"%s\\n\", \"100%");
        CHECK_STR(src, "printf(\"%s\\n\", flag ? \"true\" : \"false\");");
        CHECK_STR(src, "__j2c_print_char(65);");
        CHECK_STR(src, "__j2c_print_real(2.0, 0);");
        CHECK_STR(src, "static void __j2c_print_char(uint16_t c)");
        CHECK_STR(src, "static void __j2c_print_real(double v, int is_float)");
        CHECK_STR(src, "#include <math.h>");
    }
    free(src);

    char *clean = emit_source_mode(p, 1);
    if (clean) {
        CHECK_STR(clean, "L1: ;");
        CHECK_STR(clean, "printf(\"%s\\n\", flag ? \"true\" : \"false\");");
        CHECK_STR(clean, "__j2c_print_char(65);");
        CHECK_STR(clean, "printf(\"%g\\n\", 2.0);");
        CHECK_STR(clean, "static void __j2c_print_char(unsigned short c)");
        CHECK_NO_STR(clean, "__j2c_print_real");
        CHECK_NO_STR(clean, "#include <math.h>");
        CHECK_NO_STR(clean, "#include <stdint.h>");
    }
    free(clean);
    ir_program_free(p);
}

static void test_global_initialization_without_main(void) {
    IrProgram *p = ir_program_new();
    ir_program_add_global(p, "g", TYPE_INT, 1, ir_const_int(0, TYPE_INT));
    IrFunc *init = ir_program_add_func(p, "__init_globals", TYPE_VOID);
    ir_emit(init, IR_ASSIGN)->res = ir_var("g", TYPE_INT);
    init->first->arg1 = ir_const_int(7, TYPE_INT);

    char *src = emit_source(p);
    if (src) {
        CHECK_STR(src, "int32_t g = 0;");
        CHECK_STR(src, "void __j2c_init_globals(void)");
        CHECK_STR(src, "g = 7;");
        CHECK_STR(src, "int main(void)\n{\n    __j2c_init_globals();");
        CHECK(!strstr(src, "__j2c_main"));
    }
    free(src);

    char *clean = emit_source_mode(p, 1);
    if (clean) {
        CHECK_STR(clean, "int g = 0;");
        CHECK_STR(clean, "void __j2c_init_globals(void)");
        CHECK_STR(clean, "g = 7;");
        CHECK_STR(clean, "int main(void)\n{\n    __j2c_init_globals();");
        CHECK_NO_STR(clean, "__j2c_main");
        CHECK_NO_STR(clean, "#include <stdint.h>");
    }
    free(clean);
    ir_program_free(p);
}

static void test_clean_inlines_temporaries(void) {
    IrProgram *p = ir_program_new();
    IrFunc *f = ir_program_add_func(p, "calc", TYPE_VOID);
    ir_func_add_param(f, "a", TYPE_INT);
    ir_func_add_param(f, "b", TYPE_INT);

    IrInstr *i = ir_emit(f, IR_BINOP);
    i->type = TYPE_INT;
    i->binop = OP_ADD;
    i->res = ir_temp(0, TYPE_INT);
    i->arg1 = ir_var("a", TYPE_INT);
    i->arg2 = ir_var("b", TYPE_INT);

    i = ir_emit(f, IR_BINOP);
    i->type = TYPE_INT;
    i->binop = OP_MULT;
    i->res = ir_temp(1, TYPE_INT);
    i->arg1 = ir_temp(0, TYPE_INT);
    i->arg2 = ir_const_int(2, TYPE_INT);

    ir_emit(f, IR_PRINT)->arg1 = ir_temp(1, TYPE_INT);
    f->temp_count = 2;

    char *raw = emit_source(p);
    char *clean = emit_source_mode(p, 1);
    if (raw) {
        CHECK_STR(raw, "t0 = a + b;");
        CHECK_STR(raw, "t1 = t0 * 2;");
        CHECK_STR(raw, "printf(\"%d\\n\", t1);");
    }
    if (clean) {
        CHECK_STR(clean, "printf(\"%d\\n\", ((a + b) * 2));");
        CHECK_NO_STR(clean, "t0 =");
        CHECK_NO_STR(clean, "t1 =");
        CHECK_NO_STR(clean, "(void)t");
        CHECK_NO_STR(clean, "int t0;");
        CHECK_NO_STR(clean, "int32_t");
        CHECK_STR(clean, "#include <stdio.h>");
        CHECK_NO_STR(clean, "#include <stdbool.h>");
    }
    free(raw);
    free(clean);
    ir_program_free(p);
}

static void test_clean_preserves_call_order(void) {
    IrProgram *p = ir_program_new();
    ir_program_add_func(p, "f", TYPE_INT);
    IrFunc *g = ir_program_add_func(p, "g", TYPE_VOID);

    for (int id = 0; id < 2; id++) {
        IrInstr *call = ir_emit(g, IR_CALL);
        call->type = TYPE_INT;
        call->res = ir_temp(id, TYPE_INT);
        call->arg1 = ir_func_ref("f");
        call->arg2 = ir_const_int(0, TYPE_INT);
    }
    IrInstr *sum = ir_emit(g, IR_BINOP);
    sum->type = TYPE_INT;
    sum->binop = OP_ADD;
    sum->res = ir_temp(2, TYPE_INT);
    sum->arg1 = ir_temp(0, TYPE_INT);
    sum->arg2 = ir_temp(1, TYPE_INT);
    ir_emit(g, IR_PRINT)->arg1 = ir_temp(2, TYPE_INT);
    g->temp_count = 3;

    char *clean = emit_source_mode(p, 1);
    if (clean) {
        /* A segunda chamada é imediatamente usada na soma, mas a primeira
         * permanece antes dela: a ordem dos efeitos colaterais é mantida. */
        CHECK_STR(clean, "t0 = f();");
        CHECK_STR(clean, "printf(\"%d\\n\", (t0 + f()));");
        CHECK_NO_STR(clean, "t1 = f();");
        CHECK_NO_STR(clean, "(void)t0;");
    }
    free(clean);
    ir_program_free(p);
}

static void test_clean_inlines_calls_and_declares_locals(void) {
    IrProgram *p = ir_program_new();
    IrFunc *f = ir_program_add_func(p, "f", TYPE_INT);
    (void)f;

    IrFunc *g = ir_program_add_func(p, "g", TYPE_INT);
    ir_func_add_local(g, "v", TYPE_INT);

    IrInstr *call1 = ir_emit(g, IR_CALL);
    call1->type = TYPE_INT;
    call1->res = ir_temp(0, TYPE_INT);
    call1->arg1 = ir_func_ref("f");
    call1->arg2 = ir_const_int(0, TYPE_INT);

    IrInstr *assign = ir_emit(g, IR_ASSIGN);
    assign->res = ir_var("v", TYPE_INT);
    assign->arg1 = ir_temp(0, TYPE_INT);
    ir_emit(g, IR_PRINT)->arg1 = ir_var("v", TYPE_INT);

    IrInstr *call2 = ir_emit(g, IR_CALL);
    call2->type = TYPE_INT;
    call2->res = ir_temp(1, TYPE_INT);
    call2->arg1 = ir_func_ref("f");
    call2->arg2 = ir_const_int(0, TYPE_INT);
    ir_emit(g, IR_RETURN)->arg1 = ir_temp(1, TYPE_INT);
    g->temp_count = 2;

    char *clean = emit_source_mode(p, 1);
    if (clean) {
        CHECK_STR(clean, "int v = f();");
        CHECK_STR(clean, "printf(\"%d\\n\", v);");
        CHECK_STR(clean, "return f();");
        CHECK_NO_STR(clean, "t0 =");
        CHECK_NO_STR(clean, "t1 =");
        CHECK_NO_STR(clean, "int t0;");
    }
    free(clean);
    ir_program_free(p);
}

static void test_clean_direct_main(void) {
    IrProgram *p = ir_program_new();
    IrFunc *m = ir_program_add_func(p, "main", TYPE_VOID);
    ir_func_add_local(m, "b", TYPE_BOOLEAN);

    IrInstr *assign = ir_emit(m, IR_ASSIGN);
    assign->res = ir_var("b", TYPE_BOOLEAN);
    assign->arg1 = ir_const_int(1, TYPE_BOOLEAN);
    ir_emit(m, IR_PRINT)->arg1 = ir_var("b", TYPE_BOOLEAN);
    ir_emit(m, IR_RETURN); /* 'return;' vazio do Java */

    char *clean = emit_source_mode(p, 1);
    if (clean) {
        CHECK_STR(clean, "int main(void)");
        CHECK_STR(clean, "bool b = 1;");
        CHECK_STR(clean, "return 0;");
        CHECK_STR(clean, "#include <stdbool.h>");
        CHECK_NO_STR(clean, "__j2c_main");
        CHECK_NO_STR(clean, "#include <stdint.h>");
    }
    free(clean);
    ir_program_free(p);
}

static void test_clean_wraps_called_main(void) {
    IrProgram *p = ir_program_new();
    ir_program_add_func(p, "main", TYPE_VOID);

    IrFunc *other = ir_program_add_func(p, "chama", TYPE_VOID);
    IrInstr *call = ir_emit(other, IR_CALL);
    call->arg1 = ir_func_ref("main");
    call->arg2 = ir_const_int(0, TYPE_INT);

    char *clean = emit_source_mode(p, 1);
    if (clean) {
        CHECK_STR(clean, "void __j2c_main(void)");
        CHECK_STR(clean, "int main(void)\n{\n    __j2c_main();");
    }
    free(clean);
    ir_program_free(p);
}

static void test_clean_decl_after_label_is_not_initialized(void) {
    IrProgram *p = ir_program_new();
    IrFunc *f = ir_program_add_func(p, "f", TYPE_VOID);
    ir_func_add_local(f, "v", TYPE_INT);

    ir_emit(f, IR_LABEL)->res = ir_label("L1");
    IrInstr *assign = ir_emit(f, IR_ASSIGN);
    assign->res = ir_var("v", TYPE_INT);
    assign->arg1 = ir_const_int(1, TYPE_INT);
    ir_emit(f, IR_PRINT)->arg1 = ir_var("v", TYPE_INT);

    char *clean = emit_source_mode(p, 1);
    if (clean) {
        CHECK_STR(clean, "int v;\n");
        CHECK_STR(clean, "v = 1;");
        CHECK_NO_STR(clean, "int v = 1;");
    }
    free(clean);
    ir_program_free(p);
}

static void test_clean_real_literals_need_math_only_when_used(void) {
    IrProgram *p = ir_program_new();
    IrFunc *f = ir_program_add_func(p, "f", TYPE_VOID);
    ir_emit(f, IR_PRINT)->arg1 = ir_const_real(NAN, TYPE_DOUBLE);

    char *clean = emit_source_mode(p, 1);
    if (clean) {
        CHECK_STR(clean, "printf(\"%g\\n\", NAN);");
        CHECK_STR(clean, "#include <math.h>");
        CHECK_NO_STR(clean, "__j2c_print_real");
    }
    free(clean);
    ir_program_free(p);
}

static void test_malformed_ir_is_rejected(void) {
    IrProgram *p = ir_program_new();
    IrFunc *f = ir_program_add_func(p, "quebrada", TYPE_VOID);
    IrInstr *call = ir_emit(f, IR_CALL);
    call->arg1 = ir_func_ref("f");
    call->arg2 = ir_const_int(2, TYPE_INT); /* sem IR_PARAM */

    FILE *fout = tmpfile();
    if (!fout) exit(1);
    CHECK(codegen_emit(fout, p) != 0);
    CHECK(codegen_emit_clean(fout, p) != 0);
    fclose(fout);
    ir_program_free(p);
}

static void test_reserved_names_are_rejected(void) {
    IrProgram *p = ir_program_new();
    ir_program_add_func(p, "__j2c_print_real", TYPE_VOID);

    FILE *fout = tmpfile();
    if (!fout) exit(1);
    CHECK(codegen_emit(fout, p) != 0);
    fclose(fout);
    ir_program_free(p);
}

int main(void) {
    test_basics();
    test_flow_and_prints();
    test_global_initialization_without_main();
    test_clean_inlines_temporaries();
    test_clean_preserves_call_order();
    test_clean_inlines_calls_and_declares_locals();
    test_clean_direct_main();
    test_clean_wraps_called_main();
    test_clean_decl_after_label_is_not_initialized();
    test_clean_real_literals_need_math_only_when_used();
    test_malformed_ir_is_rejected();
    test_reserved_names_are_rejected();

    if (failures) {
        fprintf(stderr, "%d de %d verificacoes falharam.\n", failures, checks);
        return 1;
    }
    printf("%d verificacoes passaram.\n", checks);
    return 0;
}
