# AST MODÜLÜ ANALİZ RAPORU

## 1. Modül Yapısı

```
src/ast/
├── ast.h                  → AstKind enum (36 tip), AstNode union, TypeSpec, BinaryOp, UnaryOp
├── ast_alloc.h            → ast_alloc_node(), ast_free() bildirimleri
├── ast_alloc.c            → Node alloc (memset zero) + deep-free (tüm AstKind'ler explicit)
├── ast_util.h             → İsim tabloları, dynamic array, TypeSpec yardımcıları
├── ast_util.c             → ast_kind_name(), binop_name(), unop_name(), ast_dyn_append(),
│                            type_spec_alloc/append/free
├── ast_check.h            → Güvenlik wrapper'ları + validasyon makroları (AST_CHECK_ENABLED)
├── ast_check.c            → ast_check_malloc/calloc/realloc/strdup/free, node validasyonu
├── ast_dump.h             → ast_dump(), ast_dump_indent() bildirimleri
└── ast_dump.c             → AST ağacını girintili yazdırma
```

## 2. Node Tipleri (36 AstKind)

| Kategori | Sayı | AstKind'lar |
|----------|------|-------------|
| Program/Yapı | 2 | `AST_PROGRAM`, `AST_BLOCK` |
| İfadeler | 5 | `AST_IDENTIFIER`, `AST_INT/FLOAT/STRING/CHAR_LITERAL` |
| Bildirimler | 7 | `VAR_DECL`, `FUNC_DECL`, `EXTERN_DECL`, `TYPEDEF`, `STRUCT_DECL`, `ENUM_DECL`, `UNION_DECL` |
| Kontrol akışı | 10 | `IF`, `WHILE`, `FOR`, `DO_WHILE`, `SWITCH`, `CASE`, `DEFAULT`, `RETURN`, `BREAK`, `CONTINUE` |
| Atlama | 2 | `GOTO`, `LABEL` |
| İfade tipleri | 8 | `EXPR_STMT`, `BINARY`, `UNARY`, `TERNARY`, `CALL`, `CAST`, `MEMBER`, `MEMBER_PTR`, `INDEX` |
| Başlatma | 2 | `INIT_LIST`, `COMPOUND_LITERAL` |
| gcLang direktif | 3 | `HASH_LIB`, `HASH_INCLUDE`, `HASH_EXTERN` |
| Boş | 1 | `EMPTY` |

## 3. Operatör Tipleri

### BinaryOp (25 adet)
- Aritmetik: `ADD`, `SUB`, `MUL`, `DIV`, `MOD`
- Karşılaştırma: `EQ`, `NE`, `LT`, `GT`, `LE`, `GE`
- Mantıksal: `LOGICAL_AND`, `LOGICAL_OR`
- Bitsel: `BIT_AND`, `BIT_OR`, `BIT_XOR`, `LSHIFT`, `RSHIFT`
- Atama: `ASSIGN`, `ADD_ASSIGN`, `SUB_ASSIGN`, `MUL_ASSIGN`, `DIV_ASSIGN`, `MOD_ASSIGN`,
  `AND_ASSIGN`, `OR_ASSIGN`, `XOR_ASSIGN`, `LSHIFT_ASSIGN`, `RSHIFT_ASSIGN`
- Virgül: `COMMA`

### UnaryOp (11 adet)
- `PLUS`, `MINUS`, `NOT`, `BIT_NOT`
- `PRE_INC`, `PRE_DEC`, `POST_INC`, `POST_DEC`
- `ADDR`, `DEREF`, `SIZEOF`

## 4. TypeSpec (22 adet)

| Kategori | TypeSpecKind'lar |
|----------|-----------------|
| Temel tipler | `VOID`, `CHAR`, `SHORT`, `INT`, `LONG`, `FLOAT`, `DOUBLE`, `SIGNED`, `UNSIGNED`, `BOOL`, `COMPLEX` |
| Kullanıcı tipleri | `STRUCT`, `UNION`, `ENUM`, `TYPEDEF_NAME` |
| Niteleyiciler | `CONST`, `VOLATILE`, `RESTRICT` |
| Depolama sınıfı | `INLINE`, `EXTERN`, `STATIC`, `REGISTER`, `AUTO` |

## 5. Güvenlik Önlemleri

### Bellek Yönetimi (ast_check.c)
| Fonksiyon | Davranış |
|-----------|----------|
| `ast_check_malloc(size)` | OOM → NULL + stderr log |
| `ast_check_calloc(n, size)` | OOM → NULL + stderr log |
| `ast_check_realloc(ptr, size)` | OOM → NULL + stderr log |
| `ast_check_strdup(str)` | strdup(NULL) → NULL + log; OOM → NULL |
| `ast_check_free(ptr)` | NULL-safe (sessizce atlanır) |

### Node Güvenliği
- `ast_alloc_node()` → `memset(&n->as, 0, sizeof(n->as))` ile union sıfırlanır
- `ast_free()` → 36 AstKind'in **tüm** union field'ları explicit free'lenir
- `ast_free(NULL)` → return (çökmez)
- Linked chain'ler (param, struct member, enum) while döngüsü ile free

### Validasyon Katmanı (opsiyonel, AST_CHECK_ENABLED ile aktif)
```
AST_CHECK_ALLOC_IN()     → line < 0 veya col < 0 kontrolü
AST_CHECK_ALLOC_OUT(n)   → NULL döndü mü kontrolü
AST_CHECK_FREE_IN(n)     → node validasyonu (line/col)
AST_CHECK_NODE(n)        → node NULL + line/col < 0
```

Aktifleştirme: `gcc -DAST_CHECK_ENABLED` ile debug build.

### Raw free() Kontrolü
`src/ast/*.c` içinde wrapper dışında **sıfır** raw free() var.

## 6. Hata Yönetimi

| Seviye | Mekanizma |
|--------|-----------|
| Alloc hatası | `alloc_fail()` → stderr "[ast_alloc] out of memory" |
| OOM log | `ast_check_malloc/calloc/realloc` otomatik stderr |
| strdup(NULL) | `ast_check_strdup` → stderr + NULL dönüş |
| Node validasyon | `ast_check_node()` → stderr "[AST_CHECK]" |
| Parser hataları | (parse modülünde, AST'e dahil değil) |

## 7. AST Modülünün Kapsadığı Dil Özellikleri (NOTES Doğrulama)

| Özellik | AstNode | NOTES |
|---------|---------|-------|
| Program | `AST_PROGRAM` | 00_HOW |
| @lib/@include/@extern | `AST_HASH_LIB/INCLUDE/EXTERN` | 00, 10 |
| Değişken bildirimi | `AST_VAR_DECL` | 03, 04 |
| Fonksiyon | `AST_FUNC_DECL` + `AST_BLOCK` | 05 |
| struct/union | `AST_STRUCT_DECL` / `AST_UNION_DECL` | 03 |
| enum | `AST_ENUM_DECL` | 03 |
| typedef | `AST_TYPEDEF` | 03 |
| if/else | `AST_IF` | 06 |
| while/for/do-while | `AST_WHILE/FOR/DO_WHILE` | 06 |
| switch/case/default | `AST_SWITCH/CASE/DEFAULT` | 06 |
| return/break/continue | `AST_RETURN/BREAK/CONTINUE` | 06 |
| goto/label | `AST_GOTO/LABEL` | 06 |
| Tüm binary operatorler | 25 `BINOP_*` | 07 |
| Tüm unary operatorler | 11 `UNOP_*` | 07 |
| & (adres) / * (deref) | `UNOP_ADDR/DEREF` | 08 |
| -> / . | `AST_MEMBER_PTR/MEMBER` | 08 |
| [] | `AST_INDEX` | 07 |
| (cast) | `AST_CAST` | 07 |
| ? : (ternary) | `AST_TERNARY` | 07 |
| func call | `AST_CALL` | 05 |
| { init list } | `AST_INIT_LIST` | 04 |
| compound literal | `AST_COMPOUND_LITERAL` | 03 |
| sizeof | `UNOP_SIZEOF` | 07 |
| C tip sözcükleri | 22 `TSPEC_*` | 03 |

## 8. Test Durumu

| Test | Parse Hata | Durum |
|------|-----------|-------|
| `hello.gcsf` | 0 | ✅ |
| `extern_regular.gcsf` | 0 | ✅ |
| `bad_types.gcsf` | 5 | 🔶 (geçersiz literal) |
| `bad_string.gcsf` | 7 | 🔶 (geçersiz string) |
| `test_errors.gcsf` | 30 | 🔶 (bilinçli hatalar) |

## 9. Özet

AST modülü **tamam ve güvenli**. 36 node tipi, 25 binary operatör, 11 unary operatör, 22 tip sözcüğü. Tüm bellek yönetimi `ast_check_*` wrapper'ları ile korunuyor. NULL-safe, OOM-safe. Debug validasyonu `-DAST_CHECK_ENABLED` ile açılabilir. Parse modülü (`src/parse/`) ayrı bir modül, AST buna bağımlı değil.
