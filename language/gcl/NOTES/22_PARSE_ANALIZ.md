# PARSE MODÜLÜ ANALİZ RAPORU

## 1. Modül Yapısı

```
src/parse/
├── parse.h              → Public API: ast_parse(tokens, count, filename, error_count)
├── parse.c              → parse_program() döngüsü, ast_parse() giriş noktası
├── parse_ctx.h          → ParseCtx struct, peek/advance/match/expect/parse_error helper'lar
├── parse_decl.h/.c      → Tip bildirimleri, typedef, struct/union/enum, var/func decl
├── parse_expr.h/.c      → İfade ayrıştırma (precedence climbing)
└── parse_stmt.h/.c      → Kontrol akışı, blok, if/while/for/switch/return/goto
```

## 2. Toplam Satır Sayısı

| Modül | Satır | Açıklama |
|-------|-------|----------|
| parse.h | 15 | Public API |
| parse.c | 59 | program döngüsü, ast_parse() |
| parse_ctx.h | 72 | context, helpers (inline) |
| parse_decl.h | 14 | bildirim |
| parse_decl.c | 371 | tip chain, struct/union/enum/typedef, var/func decl |
| parse_expr.h | 14 | bildirim |
| parse_expr.c | 327 | öncelik tablosu, primary, postfix, unary, binary |
| parse_stmt.h | 14 | bildirim |
| parse_stmt.c | 302 | blok, if/else, while, for, do-while, switch, return, break/continue, goto |

## 3. Kapsanan Dil Özellikleri (NOTES Doğrulama)

| Özellik | NOTES Referansı | Modül | Destek? |
|---------|----------------|-------|---------|
| @lib/@include/@extern | 10_MODULES.md | parse_decl.c (AST_HASH_LIB/INCLUDE/EXTERN) | ✅ |
| typedef | 03_C_TYPES.md | parse_decl.c | ✅ |
| struct/union | 03_C_TYPES.md | parse_decl.c | ✅ |
| enum | 03_C_TYPES.md | parse_decl.c | ✅ |
| var decl (tüm C tipleri) | 03_C_TYPES.md | parse_decl.c | ✅ |
| func decl/body | 05_FUNCTIONS.md | parse_decl.c + parse_stmt.c | ✅ |
| if/else | 06_CONTROL_FLOW.md | parse_stmt.c | ✅ |
| while | 06_CONTROL_FLOW.md | parse_stmt.c | ✅ |
| for | 06_CONTROL_FLOW.md | parse_stmt.c | ✅ |
| do-while | 06_CONTROL_FLOW.md | parse_stmt.c | ✅ |
| switch/case/default | 06_CONTROL_FLOW.md | parse_stmt.c | ✅ |
| return | 06_CONTROL_FLOW.md | parse_stmt.c | ✅ |
| break/continue | 06_CONTROL_FLOW.md | parse_stmt.c | ✅ |
| goto/label | 06_CONTROL_FLOW.md | parse_stmt.c | ✅ |
| ++/-- (pre/post) | 07_OPERATORS.md | parse_expr.c | ✅ |
| binary ops (25) | 07_OPERATORS.md | parse_expr.c | ✅ |
| unary ops (11) | 07_OPERATORS.md | parse_expr.c | ✅ |
| ternary ?: | 07_OPERATORS.md | parse_expr.c | ✅ |
| cast (type)expr | 07_OPERATORS.md | parse_expr.c | ✅ |
| sizeof | 07_OPERATORS.md | parse_expr.c | ✅ |
| &/* (addr/deref) | 08_POINTERS.md | parse_expr.c | ✅ |
| . / -> | 08_POINTERS.md | parse_expr.c | ✅ |
| [] (index) | 08_POINTERS.md | parse_expr.c | ✅ |
| function call | 05_FUNCTIONS.md | parse_expr.c | ✅ |
| string/char/int/float literal | 09_STRINGS.md | parse_expr.c | ✅ |
| true/false/null | built-in | parse_expr.c | ✅ |
| init list { } | 04_ARRAY_TYPES.md | parse_expr.c | ✅ |

## 4. Güvenlik ve Memory Yönetimi

### Mevcut Koruma
- Her Token heap'te value → token_free() ile free
- Her AstNode ast_check_malloc/strdup ile allocate
- ast_free() tüm union member'ları explicit free eder
- ast_free(NULL) → return (çökmez)
- ParseCtx stack'te, tokens dışarıdan gelir
- parse_error() hata mesajı + error_count artırır

### Legacy Bug'lar (Fix Edildi)
| # | Bug | Dosya | Fix |
|---|-----|-------|-----|
| 1 | FUNC/STRUCT/ENUM chain free'de double-free | ast_alloc.c | init=NULL before free |
| 2 | enum body'de evalue kayboluyor | parse_decl.c | init'e atandı |
| 3 | struct/union body chain ters sıra | parse_decl.c | normal sıraya çevrildi |
| 4 | enum body chain ters sıra | parse_decl.c | normal sıraya çevrildi |
| 5 | Ternary else_expr precedence yanlış | parse_expr.c | min_prec yerine 0 kullanıldı |
| 6 | struct/union forward decl'de error | parse_decl.c | body yoksa doğru işleme |
| 7 | struct body'de bitfield colon token sorunu | parse_decl.c | sadece PUNCT_COLON kontrolü |
| 8 | Çeşitli error path memory leak | parse_decl/expr/stmt | tüm path'lerde type_spec/name free |

## 5. Hata Yönetimi

| Hata | Kaynak | Mesaj |
|------|--------|-------|
| expected identifier | parse_decl.c | "expected identifier" |
| expected name after typedef | parse_decl.c | "expected name after typedef" |
| unexpected token in expression | parse_expr.c | "unexpected token %s in expression" |
| case/default outside switch | parse_stmt.c | "case/default outside switch" |
| expected ( / ) / { / } / ; | parse_ctx.h (expect) | "expected %s, got %s" |

## 6. Test Durumu

| Test Dosyası | Parse Error | Durum |
|-------------|------------|-------|
| var_decls.gcsf | 0 | ✅ |
| functions.gcsf | 0 | ✅ |
| control_flow.gcsf | 0 | ✅ |
| expressions.gcsf | 0 | ✅ |
| struct_enum.gcsf | 0 | ✅ |
| modules.gcsf | 0 | ✅ |
| errors.gcsf | ≥1 | 🔶 bilinçli hatalar |

## 7. Özet

Parse modülü gcLang dilinin tüm temel özelliklerini kapsar:
- C99 tip sistemi (void, char, int, float, double, short, long, signed, unsigned, _Bool)
- typedef, struct, union, enum
- Fonksiyon bildirimi ve gövdesi
- Kontrol akışı (if/else, while, for, do-while, switch, break, continue, goto)
- 25 binary + 11 unary operatör, ternary ?:
- Cast, sizeof, & (adres), * (deref), . (member), -> (member ptr), [] (index)
- Fonksiyon çağrısı
- Tüm literal tipleri (int, float, string, char, true, false, null)
- gcLang modül sistemi (@lib, @include, @extern)

Bellek yönetimi: ast_check_* wrapper, NULL-safe free, double-free korumalı.
