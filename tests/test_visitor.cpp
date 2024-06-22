#include "ast.h"
#include "error.h"
#include "parser.h"
#include "visitor.h"
#include <doctest.h>
#include <sstream>

extern FILE *yyin;

static std::shared_ptr<CompUnits> parse(std::string content) {
    FILE *testfile = fopen("test_frontend.sy", "w");
    fprintf(testfile, "%s", content.c_str());
    fclose(testfile);

    yyin = fopen("test_frontend.sy", "r");

    auto root = std::make_shared<CompUnits>();
    yyparse(root);

    fclose(yyin);
    yyin = nullptr;
    remove("test_frontend.sy");

    return root;
}

static constexpr char CONTENT[] = R"(
int a;
float b;
int c[1], d[1][2];
float e[1], f[1][2];
const int g = 1;
const float h = 1.0;
const int i[1] = {1};
const float j[1] = {1.0};
const int k[1][2] = {{1, 2}};
const float l[1][2] = {{1.0, 2.0}};
int main() {
    int a2;
    float b2;
    int c2[1], d2[1][2];
    float e2[1], f2[1][2];
    const int g2 = 1;
    const float h2 = 1.0;
    const int i2[1] = {1};
    const float j2[1] = {1.0};
    const int k2[1][2] = {{1, 2}};
    const float l2[1][2] = {{1.0, 2.0}};
    return 0;
}
)";

static constexpr char EXPECTED[] = R"(data $a = align 4 { z 4, }
data $b = align 4 { z 4, }
data $c = align 4 { z 4, }
data $d = align 4 { z 8, }
data $e = align 4 { z 4, }
data $f = align 4 { z 8, }
data $g = align 4 { w 1, }
data $h = align 4 { s s_1, }
data $i = align 4 { w 1, }
data $j = align 4 { s s_1, }
data $k = align 4 { w 1, w 2, }
data $l = align 4 { s s_1, s s_2, }
export
function w $main() {
@start.1
    %.1 =l alloc4 4
    %.2 =l alloc4 4
    %.3 =l alloc4 4
    %.4 =l alloc4 8
    %.5 =l alloc4 4
    %.6 =l alloc4 8
    %.7 =l alloc4 4
    %.8 =l alloc4 4
    %.9 =l alloc4 4
    %.10 =l alloc4 4
    %.11 =l alloc4 8
    %.13 =l alloc4 8
    jmp @body.2
@body.2
    storew 1, %.7
    stores s_1, %.8
    storew 1, %.9
    stores s_1, %.10
    storew 1, %.11
    %.12 =l add %.11, 4
    storew 2, %.12
    stores s_1, %.13
    %.14 =l add %.13, 4
    stores s_2, %.14
    ret 0
}
)";

TEST_CASE("testing variable declaration") {
    auto root = parse(CONTENT);

    ir::Module module;

    Visitor visitor(module);

    visitor.visit(*root);

    CHECK_FALSE(has_error());

    std::ostringstream ss;

    module.emit(ss);

    CHECK_EQ(ss.str(), EXPECTED);
}

static constexpr char CONTENT2[] = R"(
int func1() {
    return func1();
}

void func2(int a, int b) {
    func2(b, a);
}

void func3(int a, float b, int c[], float d[], int e[][2], float f[][2]) {
    func3(a, b, c, d, e, f);
}
)";

static constexpr char EXPECTED2[] = R"(function w $func1() {
@start.1
    jmp @body.2
@body.2
    %.1 =w call $func1()
    ret %.1
}
function $func2(w %.1, w %.2, ) {
@start.3
    %.3 =l alloc4 4
    storew %.1, %.3
    %.4 =l alloc4 4
    storew %.2, %.4
    jmp @body.4
@body.4
    %.5 =w loadw %.4
    %.6 =w loadw %.3
    call $func2(w %.5, w %.6, )
    ret
}
function $func3(w %.1, s %.2, l %.3, l %.4, l %.5, l %.6, ) {
@start.5
    %.7 =l alloc4 4
    storew %.1, %.7
    %.8 =l alloc4 4
    stores %.2, %.8
    %.9 =l alloc8 8
    storel %.3, %.9
    %.10 =l alloc8 8
    storel %.4, %.10
    %.11 =l alloc8 8
    storel %.5, %.11
    %.12 =l alloc8 8
    storel %.6, %.12
    jmp @body.6
@body.6
    %.13 =w loadw %.7
    %.14 =s loads %.8
    %.15 =l loadl %.9
    %.16 =l loadl %.10
    %.17 =l loadl %.11
    %.18 =l loadl %.12
    call $func3(w %.13, s %.14, l %.15, l %.16, l %.17, l %.18, )
    ret
}
)";

TEST_CASE("testing function params and call") {
    auto root = parse(CONTENT2);

    ir::Module module;

    Visitor visitor(module);

    visitor.visit(*root);

    CHECK_FALSE(has_error());

    std::ostringstream ss;

    module.emit(ss);

    CHECK_EQ(ss.str(), EXPECTED2);
}

static constexpr char CONTENT3[] = R"(
float b;
void case1(int a) {
    if (a) {
        b = 1.0;
    }
}

void case2(int a) {
    if (a) {
        b = 1.0;
    } else {
        b = 2.0;
    }
}

void case3(int a) {
    if (a) {
        b = 1.0;
    } else if (!a) {
        b = 2.0;
    } else {
        b = 3.0;
    }
}

void case4(int a) {
    if (a) {
        if (!a) {
            b = 1.0;
        }
    }
}

void case5(int a) {
    if (a) {
        if (!a) {
            b = 1.0;
        } else {
            b = 2.0;
        }
    }
}

void case6(int a) {
    if (a) {
        if (!a) {
            b = 1.0;
        } else if (a + 1) {
            b = 2.0;
        } else {
            b = 3.0;
        }
    }
}

void case7(int a) {
    if (a) {
        if (!a) {
            b = 1.0;
        } else if (a + 1) {
            b = 2.0;
        } else {
            b = 3.0;
        }
    } else {
        b = 4.0;
    }
}

void case8(int a) {
    if (a) {
        b = 1.0;
    } else {
        if (a + 1) {
            b = 2.0;
        }
    }
}

void case9(int a) {
    if (a) {
        b = 1.0;
    } else {
        if (a + 1) {
            b = 2.0;
        } else {
            b = 3.0;
        }
    }
}
)";

static constexpr char EXPECTED3[] = R"(data $b = align 4 { z 4, }
function $case1(w %.1, ) {
@start.1
    %.2 =l alloc4 4
    storew %.1, %.2
    jmp @body.2
@body.2
    %.3 =w loadw %.2
    jnz %.3, @if_true.3, @if_false.4
@if_true.3
    stores s_1, $b
    jmp @if_false.4
@if_false.4
    ret
}
function $case2(w %.1, ) {
@start.5
    %.2 =l alloc4 4
    storew %.1, %.2
    jmp @body.6
@body.6
    %.3 =w loadw %.2
    jnz %.3, @if_true.7, @if_false.8
@if_true.7
    stores s_1, $b
    jmp @if_join.9
@if_false.8
    stores s_2, $b
    jmp @if_join.9
@if_join.9
    ret
}
function $case3(w %.1, ) {
@start.10
    %.2 =l alloc4 4
    storew %.1, %.2
    jmp @body.11
@body.11
    %.3 =w loadw %.2
    jnz %.3, @if_true.12, @if_false.13
@if_true.12
    stores s_1, $b
    jmp @if_join.17
@if_false.13
    %.4 =w loadw %.2
    %.5 =w ceqw %.4, 0
    jnz %.5, @if_true.14, @if_false.15
@if_true.14
    stores s_2, $b
    jmp @if_join.16
@if_false.15
    stores s_3, $b
    jmp @if_join.16
@if_join.16
    jmp @if_join.17
@if_join.17
    ret
}
function $case4(w %.1, ) {
@start.18
    %.2 =l alloc4 4
    storew %.1, %.2
    jmp @body.19
@body.19
    %.3 =w loadw %.2
    jnz %.3, @if_true.20, @if_false.23
@if_true.20
    %.4 =w loadw %.2
    %.5 =w ceqw %.4, 0
    jnz %.5, @if_true.21, @if_false.22
@if_true.21
    stores s_1, $b
    jmp @if_false.22
@if_false.22
    jmp @if_false.23
@if_false.23
    ret
}
function $case5(w %.1, ) {
@start.24
    %.2 =l alloc4 4
    storew %.1, %.2
    jmp @body.25
@body.25
    %.3 =w loadw %.2
    jnz %.3, @if_true.26, @if_false.30
@if_true.26
    %.4 =w loadw %.2
    %.5 =w ceqw %.4, 0
    jnz %.5, @if_true.27, @if_false.28
@if_true.27
    stores s_1, $b
    jmp @if_join.29
@if_false.28
    stores s_2, $b
    jmp @if_join.29
@if_join.29
    jmp @if_false.30
@if_false.30
    ret
}
function $case6(w %.1, ) {
@start.31
    %.2 =l alloc4 4
    storew %.1, %.2
    jmp @body.32
@body.32
    %.3 =w loadw %.2
    jnz %.3, @if_true.33, @if_false.40
@if_true.33
    %.4 =w loadw %.2
    %.5 =w ceqw %.4, 0
    jnz %.5, @if_true.34, @if_false.35
@if_true.34
    stores s_1, $b
    jmp @if_join.39
@if_false.35
    %.6 =w loadw %.2
    %.7 =w add %.6, 1
    jnz %.7, @if_true.36, @if_false.37
@if_true.36
    stores s_2, $b
    jmp @if_join.38
@if_false.37
    stores s_3, $b
    jmp @if_join.38
@if_join.38
    jmp @if_join.39
@if_join.39
    jmp @if_false.40
@if_false.40
    ret
}
function $case7(w %.1, ) {
@start.41
    %.2 =l alloc4 4
    storew %.1, %.2
    jmp @body.42
@body.42
    %.3 =w loadw %.2
    jnz %.3, @if_true.43, @if_false.50
@if_true.43
    %.4 =w loadw %.2
    %.5 =w ceqw %.4, 0
    jnz %.5, @if_true.44, @if_false.45
@if_true.44
    stores s_1, $b
    jmp @if_join.49
@if_false.45
    %.6 =w loadw %.2
    %.7 =w add %.6, 1
    jnz %.7, @if_true.46, @if_false.47
@if_true.46
    stores s_2, $b
    jmp @if_join.48
@if_false.47
    stores s_3, $b
    jmp @if_join.48
@if_join.48
    jmp @if_join.49
@if_join.49
    jmp @if_join.51
@if_false.50
    stores s_4, $b
    jmp @if_join.51
@if_join.51
    ret
}
function $case8(w %.1, ) {
@start.52
    %.2 =l alloc4 4
    storew %.1, %.2
    jmp @body.53
@body.53
    %.3 =w loadw %.2
    jnz %.3, @if_true.54, @if_false.55
@if_true.54
    stores s_1, $b
    jmp @if_join.58
@if_false.55
    %.4 =w loadw %.2
    %.5 =w add %.4, 1
    jnz %.5, @if_true.56, @if_false.57
@if_true.56
    stores s_2, $b
    jmp @if_false.57
@if_false.57
    jmp @if_join.58
@if_join.58
    ret
}
function $case9(w %.1, ) {
@start.59
    %.2 =l alloc4 4
    storew %.1, %.2
    jmp @body.60
@body.60
    %.3 =w loadw %.2
    jnz %.3, @if_true.61, @if_false.62
@if_true.61
    stores s_1, $b
    jmp @if_join.66
@if_false.62
    %.4 =w loadw %.2
    %.5 =w add %.4, 1
    jnz %.5, @if_true.63, @if_false.64
@if_true.63
    stores s_2, $b
    jmp @if_join.65
@if_false.64
    stores s_3, $b
    jmp @if_join.65
@if_join.65
    jmp @if_join.66
@if_join.66
    ret
}
)";

TEST_CASE("testing if statement") {
    auto root = parse(CONTENT3);

    ir::Module module;

    Visitor visitor(module);

    visitor.visit(*root);

    CHECK_FALSE(has_error());

    std::ostringstream ss;

    module.emit(ss);

    CHECK_EQ(ss.str(), EXPECTED3);
}

static constexpr char CONTENT4[] = R"(
void case1() {
    while (1) {
        break;
    }
}

void case2() {
    while (1) {
        continue;
    }
}

void case3() {
    while (1) {
        return;
    }
}

void case4() {
    while (1) {
        if (1) {
            break;
        }
    }
}

void case5() {
    while (1) {
        if (1) {
            continue;
        }
    }
}

void case6() {
    while (1) {
        if (1) {
            return;
        }
    }
}

void case7() {
    while (1) {
        if (1) {
            break;
        } else {
            continue;
        }
    }
}

void case8() {
    while (1) {
        while(1) {
            break;
        }
    }
}

void case9() {
    while (1) {
        while(1) {
            continue;
        }
    }
}

void case10() {
    while (1) {
        while(1) {
            return;
        }
    }
}
)";

static constexpr char EXPECTED4[] = R"(function $case1() {
@start.1
    jmp @body.2
@body.2
    jmp @while_cond.3
@while_cond.3
    jnz 1, @while_body.4, @while_join.5
@while_body.4
    jmp @while_join.5
@while_join.5
    ret
}
function $case2() {
@start.6
    jmp @body.7
@body.7
    jmp @while_cond.8
@while_cond.8
    jnz 1, @while_body.9, @while_join.10
@while_body.9
    jmp @while_cond.8
@while_join.10
    ret
}
function $case3() {
@start.11
    jmp @body.12
@body.12
    jmp @while_cond.13
@while_cond.13
    jnz 1, @while_body.14, @while_join.15
@while_body.14
    ret
@while_join.15
    ret
}
function $case4() {
@start.16
    jmp @body.17
@body.17
    jmp @while_cond.18
@while_cond.18
    jnz 1, @while_body.19, @while_join.22
@while_body.19
    jnz 1, @if_true.20, @if_false.21
@if_true.20
    jmp @while_join.22
@if_false.21
    jmp @while_cond.18
@while_join.22
    ret
}
function $case5() {
@start.23
    jmp @body.24
@body.24
    jmp @while_cond.25
@while_cond.25
    jnz 1, @while_body.26, @while_join.29
@while_body.26
    jnz 1, @if_true.27, @if_false.28
@if_true.27
    jmp @while_cond.25
@if_false.28
    jmp @while_cond.25
@while_join.29
    ret
}
function $case6() {
@start.30
    jmp @body.31
@body.31
    jmp @while_cond.32
@while_cond.32
    jnz 1, @while_body.33, @while_join.36
@while_body.33
    jnz 1, @if_true.34, @if_false.35
@if_true.34
    ret
@if_false.35
    jmp @while_cond.32
@while_join.36
    ret
}
function $case7() {
@start.37
    jmp @body.38
@body.38
    jmp @while_cond.39
@while_cond.39
    jnz 1, @while_body.40, @while_join.44
@while_body.40
    jnz 1, @if_true.41, @if_false.42
@if_true.41
    jmp @while_join.44
@if_false.42
    jmp @while_cond.39
@if_join.43
    jmp @while_cond.39
@while_join.44
    ret
}
function $case8() {
@start.45
    jmp @body.46
@body.46
    jmp @while_cond.47
@while_cond.47
    jnz 1, @while_body.48, @while_join.52
@while_body.48
    jmp @while_cond.49
@while_cond.49
    jnz 1, @while_body.50, @while_join.51
@while_body.50
    jmp @while_join.51
@while_join.51
    jmp @while_cond.47
@while_join.52
    ret
}
function $case9() {
@start.53
    jmp @body.54
@body.54
    jmp @while_cond.55
@while_cond.55
    jnz 1, @while_body.56, @while_join.60
@while_body.56
    jmp @while_cond.57
@while_cond.57
    jnz 1, @while_body.58, @while_join.59
@while_body.58
    jmp @while_cond.57
@while_join.59
    jmp @while_cond.55
@while_join.60
    ret
}
function $case10() {
@start.61
    jmp @body.62
@body.62
    jmp @while_cond.63
@while_cond.63
    jnz 1, @while_body.64, @while_join.68
@while_body.64
    jmp @while_cond.65
@while_cond.65
    jnz 1, @while_body.66, @while_join.67
@while_body.66
    ret
@while_join.67
    jmp @while_cond.63
@while_join.68
    ret
}
)";

TEST_CASE("testing while statement") {
    auto root = parse(CONTENT4);

    ir::Module module;

    Visitor visitor(module);

    visitor.visit(*root);

    CHECK_FALSE(has_error());

    std::ostringstream ss;

    module.emit(ss);

    CHECK_EQ(ss.str(), EXPECTED4);
}

static constexpr char CONTENT5[] = R"(
void case1() {
    if (1 || 1) {
        return;
    }
}

void case2() {
    if (1 && 1) {
        return;
    }
}

void case3() {
    if (1 && 1 && 1) {
        return;
    }
}

void case4() {
    if (1 || 1 || 1) {
        return;
    }
}

void case5() {
    if (1 || 1 && 1) {
        return;
    }
}

void case6() {
    if (1 && 1 || 1) {
        return;
    }
}

void case7() {
    if (1 && 1 || 1 && 1) {
        return;
    }
}

void case8() {
    if (1 || 1 && 1 || 1) {
        return;
    }
}
)";

static constexpr char EXPECTED5[] = R"(function $case1() {
@start.1
    jmp @body.2
@body.2
    jnz 1, @if_true.4, @logic_right.3
@logic_right.3
    jnz 1, @if_true.4, @if_false.5
@if_true.4
    ret
@if_false.5
    ret
}
function $case2() {
@start.6
    jmp @body.7
@body.7
    jnz 1, @logic_right.8, @if_false.10
@logic_right.8
    jnz 1, @if_true.9, @if_false.10
@if_true.9
    ret
@if_false.10
    ret
}
function $case3() {
@start.11
    jmp @body.12
@body.12
    jnz 1, @logic_right.13, @if_false.16
@logic_right.13
    jnz 1, @logic_right.14, @if_false.16
@logic_right.14
    jnz 1, @if_true.15, @if_false.16
@if_true.15
    ret
@if_false.16
    ret
}
function $case4() {
@start.17
    jmp @body.18
@body.18
    jnz 1, @if_true.21, @logic_right.19
@logic_right.19
    jnz 1, @if_true.21, @logic_right.20
@logic_right.20
    jnz 1, @if_true.21, @if_false.22
@if_true.21
    ret
@if_false.22
    ret
}
function $case5() {
@start.23
    jmp @body.24
@body.24
    jnz 1, @if_true.27, @logic_right.25
@logic_right.25
    jnz 1, @logic_right.26, @if_false.28
@logic_right.26
    jnz 1, @if_true.27, @if_false.28
@if_true.27
    ret
@if_false.28
    ret
}
function $case6() {
@start.29
    jmp @body.30
@body.30
    jnz 1, @logic_right.31, @logic_right.32
@logic_right.31
    jnz 1, @if_true.33, @logic_right.32
@logic_right.32
    jnz 1, @if_true.33, @if_false.34
@if_true.33
    ret
@if_false.34
    ret
}
function $case7() {
@start.35
    jmp @body.36
@body.36
    jnz 1, @logic_right.37, @logic_right.38
@logic_right.37
    jnz 1, @if_true.40, @logic_right.38
@logic_right.38
    jnz 1, @logic_right.39, @if_false.41
@logic_right.39
    jnz 1, @if_true.40, @if_false.41
@if_true.40
    ret
@if_false.41
    ret
}
function $case8() {
@start.42
    jmp @body.43
@body.43
    jnz 1, @if_true.47, @logic_right.44
@logic_right.44
    jnz 1, @logic_right.45, @logic_right.46
@logic_right.45
    jnz 1, @if_true.47, @logic_right.46
@logic_right.46
    jnz 1, @if_true.47, @if_false.48
@if_true.47
    ret
@if_false.48
    ret
}
)";

TEST_CASE("testing short-circuit evaluation") {
    auto root = parse(CONTENT5);

    ir::Module module;

    Visitor visitor(module);

    visitor.visit(*root);

    CHECK_FALSE(has_error());

    std::ostringstream ss;

    module.emit(ss);

    CHECK_EQ(ss.str(), EXPECTED5);
}

static constexpr char CONTENT6[] = R"(
void func(int a, float b) {
    return;
}

int a = 1.0;
float b = 1;

int main() {
    int a = 1.0;
    float b = 1;
    a = b + 1;
    b = a + 1.0;
    func(a, b);
    func(b, a);
    return 0;
}
)";

static constexpr char EXPECTED6[] = R"(data $a = align 4 { w 1, }
data $b = align 4 { s s_1, }
function $func(w %.1, s %.2, ) {
@start.1
    %.3 =l alloc4 4
    storew %.1, %.3
    %.4 =l alloc4 4
    stores %.2, %.4
    jmp @body.2
@body.2
    ret
}
export
function w $main() {
@start.3
    %.1 =l alloc4 4
    %.2 =l alloc4 4
    jmp @body.4
@body.4
    storew 1, %.1
    stores s_1, %.2
    %.3 =s loads %.2
    %.4 =s add %.3, s_1
    %.5 =w stosi %.4
    storew %.5, %.1
    %.6 =w loadw %.1
    %.7 =s swtof %.6
    %.8 =s add %.7, s_1
    stores %.8, %.2
    %.9 =w loadw %.1
    %.10 =s loads %.2
    call $func(w %.9, s %.10, )
    %.11 =s loads %.2
    %.12 =w stosi %.11
    %.13 =w loadw %.1
    %.14 =s swtof %.13
    call $func(w %.12, s %.14, )
    ret 0
}
)";

TEST_CASE("testing implicit cast") {
    auto root = parse(CONTENT6);

    ir::Module module;

    Visitor visitor(module);

    visitor.visit(*root);

    CHECK_FALSE(has_error());

    std::ostringstream ss;

    module.emit(ss);

    CHECK_EQ(ss.str(), EXPECTED6);
}

static constexpr char CONTENT7[] = R"(
void testparam(int a[], float b[], int c[][3], float d[][3]) {
    return;
}

void testinit() {
    int a[4][2] = {};
    int b[4][2] = {1, 2, 3, 4, 5, 6, 7, 8};
    int c[4][2] = {{1, 2}, {3, 4}, 5, 6, 7, 8};
    int d[4][2] = {{1, 2}, {3}, {5}, 7, 8};
    int e[4][2] = {{1, 2}, {3, 4}, {5, 6}, {7, 8}};
}

int main() {
    int a[3][3] = {{1, 2, 3}, {4}, {7, 8, 9}};
    float b[3][3] = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.0, 8.0, 9.0}};
    testparam(a[0], b[0], a, b);
    return 0;
}
)";

static constexpr char EXPECTED7[] =
    R"(function $testparam(l %.1, l %.2, l %.3, l %.4, ) {
@start.1
    %.5 =l alloc8 8
    storel %.1, %.5
    %.6 =l alloc8 8
    storel %.2, %.6
    %.7 =l alloc8 8
    storel %.3, %.7
    %.8 =l alloc8 8
    storel %.4, %.8
    jmp @body.2
@body.2
    ret
}
function $testinit() {
@start.3
    %.1 =l alloc4 32
    %.9 =l alloc4 32
    %.17 =l alloc4 32
    %.25 =l alloc4 32
    %.33 =l alloc4 32
    jmp @body.4
@body.4
    storew 0, %.1
    %.2 =l add %.1, 4
    storew 0, %.2
    %.3 =l add %.1, 8
    storew 0, %.3
    %.4 =l add %.1, 12
    storew 0, %.4
    %.5 =l add %.1, 16
    storew 0, %.5
    %.6 =l add %.1, 20
    storew 0, %.6
    %.7 =l add %.1, 24
    storew 0, %.7
    %.8 =l add %.1, 28
    storew 0, %.8
    storew 1, %.9
    %.10 =l add %.9, 4
    storew 2, %.10
    %.11 =l add %.9, 8
    storew 3, %.11
    %.12 =l add %.9, 12
    storew 4, %.12
    %.13 =l add %.9, 16
    storew 5, %.13
    %.14 =l add %.9, 20
    storew 6, %.14
    %.15 =l add %.9, 24
    storew 7, %.15
    %.16 =l add %.9, 28
    storew 8, %.16
    storew 1, %.17
    %.18 =l add %.17, 4
    storew 2, %.18
    %.19 =l add %.17, 8
    storew 3, %.19
    %.20 =l add %.17, 12
    storew 4, %.20
    %.21 =l add %.17, 16
    storew 5, %.21
    %.22 =l add %.17, 20
    storew 6, %.22
    %.23 =l add %.17, 24
    storew 7, %.23
    %.24 =l add %.17, 28
    storew 8, %.24
    storew 1, %.25
    %.26 =l add %.25, 4
    storew 2, %.26
    %.27 =l add %.25, 8
    storew 3, %.27
    %.28 =l add %.25, 12
    storew 0, %.28
    %.29 =l add %.25, 16
    storew 5, %.29
    %.30 =l add %.25, 20
    storew 0, %.30
    %.31 =l add %.25, 24
    storew 7, %.31
    %.32 =l add %.25, 28
    storew 8, %.32
    storew 1, %.33
    %.34 =l add %.33, 4
    storew 2, %.34
    %.35 =l add %.33, 8
    storew 3, %.35
    %.36 =l add %.33, 12
    storew 4, %.36
    %.37 =l add %.33, 16
    storew 5, %.37
    %.38 =l add %.33, 20
    storew 6, %.38
    %.39 =l add %.33, 24
    storew 7, %.39
    %.40 =l add %.33, 28
    storew 8, %.40
    ret
}
export
function w $main() {
@start.5
    %.1 =l alloc4 36
    %.10 =l alloc4 36
    jmp @body.6
@body.6
    storew 1, %.1
    %.2 =l add %.1, 4
    storew 2, %.2
    %.3 =l add %.1, 8
    storew 3, %.3
    %.4 =l add %.1, 12
    storew 4, %.4
    %.5 =l add %.1, 16
    storew 0, %.5
    %.6 =l add %.1, 20
    storew 0, %.6
    %.7 =l add %.1, 24
    storew 7, %.7
    %.8 =l add %.1, 28
    storew 8, %.8
    %.9 =l add %.1, 32
    storew 9, %.9
    stores s_1, %.10
    %.11 =l add %.10, 4
    stores s_2, %.11
    %.12 =l add %.10, 8
    stores s_3, %.12
    %.13 =l add %.10, 12
    stores s_4, %.13
    %.14 =l add %.10, 16
    stores s_5, %.14
    %.15 =l add %.10, 20
    stores s_6, %.15
    %.16 =l add %.10, 24
    stores s_7, %.16
    %.17 =l add %.10, 28
    stores s_8, %.17
    %.18 =l add %.10, 32
    stores s_9, %.18
    %.19 =l extsw 0
    %.20 =l mul %.19, 12
    %.21 =l add %.1, %.20
    %.22 =l extsw 0
    %.23 =l mul %.22, 12
    %.24 =l add %.10, %.23
    call $testparam(l %.21, l %.24, l %.1, l %.10, )
    ret 0
}
)";

TEST_CASE("testing array") {
    auto root = parse(CONTENT7);

    ir::Module module;

    Visitor visitor(module);

    visitor.visit(*root);

    CHECK_FALSE(has_error());

    std::ostringstream ss;

    module.emit(ss);

    CHECK_EQ(ss.str(), EXPECTED7);
}

static constexpr char CONTENT8[] = R"(
const int a = 1;
const float b = 1.0;
int c[a + 2] = {1 + 0 - 0, 2 * 1 / 1, 3 % 4};
float d[b + 2] = {1.0 + 0.0 - 0.0, 2.0 * 1.0 / 1.0, 3.0};
const int e[2][2][2] = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}};
int f = e[0][0][0] + e[0][0][1] + e[0][1][0] + e[0][1][1] + e[1][0][0] + e[1][0][1] + e[1][1][0] + e[1][1][1];

int main() {
    const int a2 = 1;
    const float b2 = 1.0;
    const int c2[a2 + 2] = {1 + 0 - 0, 2 * 1 / 1, 3 % 4};
    const float d2[b2 + 2] = {1.0 + 0.0 - 0.0, 2.0 * 1.0 / 1.0, 3.0};
    const int e2[2][2][2] = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}};
    const int f2 = e2[0][0][0] + e2[0][0][1] + e2[0][1][0] + e2[0][1][1] + e2[1][0][0] + e2[1][0][1] + e2[1][1][0] + e2[1][1][1];
    return 0;
}
)";

static constexpr char EXPECTED8[] = R"(data $a = align 4 { w 1, }
data $b = align 4 { s s_1, }
data $c = align 4 { w 1, w 2, w 3, }
data $d = align 4 { s s_1, s s_2, s s_3, }
data $e = align 4 { w 1, w 2, w 3, w 4, w 5, w 6, w 7, w 8, }
data $f = align 4 { w 36, }
export
function w $main() {
@start.1
    %.1 =l alloc4 4
    %.2 =l alloc4 4
    %.3 =l alloc4 12
    %.6 =l alloc4 12
    %.9 =l alloc4 32
    %.17 =l alloc4 4
    jmp @body.2
@body.2
    storew 1, %.1
    stores s_1, %.2
    storew 1, %.3
    %.4 =l add %.3, 4
    storew 2, %.4
    %.5 =l add %.3, 8
    storew 3, %.5
    stores s_1, %.6
    %.7 =l add %.6, 4
    stores s_2, %.7
    %.8 =l add %.6, 8
    stores s_3, %.8
    storew 1, %.9
    %.10 =l add %.9, 4
    storew 2, %.10
    %.11 =l add %.9, 8
    storew 3, %.11
    %.12 =l add %.9, 12
    storew 4, %.12
    %.13 =l add %.9, 16
    storew 5, %.13
    %.14 =l add %.9, 20
    storew 6, %.14
    %.15 =l add %.9, 24
    storew 7, %.15
    %.16 =l add %.9, 28
    storew 8, %.16
    storew 36, %.17
    ret 0
}
)";

TEST_CASE("testing constant folding") {
    auto root = parse(CONTENT8);

    ir::Module module;

    Visitor visitor(module);

    visitor.visit(*root);

    CHECK_FALSE(has_error());

    std::ostringstream ss;

    module.emit(ss);

    CHECK_EQ(ss.str(), EXPECTED8);
}