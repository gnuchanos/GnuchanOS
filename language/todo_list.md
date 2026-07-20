# GCL Compiler — Roadmap & Todo List

> **Hedef:** `GCL_READ_ONLY.MD` spec'ine sadık, `language/build/*.gcsf` (4 örnek) sorunsuz derleyen, GitHub Actions CI/CD'li bir GCL → C transpiler inşa etmek.

---

## İlerleme Özeti

| Durum | Sayı | Fazlar |
|-------|------|--------|
| ✅ Tamamlandı | 12 | FAZ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 |
| 🟡 Kısmen | 0 | — |
| 🔴 Bekliyor | 0 | — |

---

## FAZ 1—10: ✅ Tamamlandı

Tüm core pipeline (lexer → parser → AST → codegen → CLI), hata sistemi, build sistemi ve 4 örnek dosya compile + run testi başarıyla tamamlandı.

## FAZ 5: Semantic Analysis ✅

- [x] `src/semantic/semantic.h` + `src/semantic/semantic.c` — tam implementasyon
  - [x] Hash-based symbol table (scope chain: global → function → block)
  - [x] Tip çıkarımı (literaller, binary/unary/ternary, cast, call, subscript)
  - [x] Tip kontrolü (assignment, return, function arg count)
  - [x] Undeclared variable hatası (E011)
  - [x] Redeclaration/redefinition hatası (E012)
  - [x] Type mismatch hatası (E013)
  - [x] Warning sistemi (W002: incompatible assignment, missing return, arg mismatch)
  - [x] Builtin fonksiyonlar (printf, malloc, calloc, realloc, free, strlen)
  - [x] `main.c` compile pipeline'ına entegre edildi

## FAZ 11: GitHub Actions CI/CD 🟡

- [x] `.github/workflows/build.yml` — Ubuntu + Windows cross-platform build
- [ ] Push sonrası ilk çalıştırma doğrulaması (GitHub'da test edilecek)
- [ ] README.md build badge

## FAZ 12: Dokümantasyon ✅

- [x] `language/README.md` — kurulum, kullanım, CLI, tipler, sözdizimi, hata sistemi, proje yapısı
- [x] `language/todo_list.md` — roadmap ve bug fix logu güncel

---

## Bug Fixes (Bu Oturum)

| # | Dosya | Sorun | Çözüm |
|---|-------|-------|-------|
| 1 | parser.c | unbounded `strlen` ham buffer'da | `token_dup()` — `tok.length` kullan |
| 2 | codegen.c | escape edilmemiş string literal'lar | `\n`, `\t`, `\\`, `\"` |
| 3 | codegen.c | GCL yorumları C'ye sızıyor | codegen'de strip |
| 4 | lexer.c | `malloc`/`free`/`strlen` keyword | `TOK_IDENTIFIER` ile birleştir |
| 5 | codegen.c | pointer_depth `<=` → `char***` | `<` düzeltmesi |
| 6 | parser.c | cast ifadesi singleton type free | `type_clone()` |
| 7 | codegen.c | multi-dim array ters sıra | bracket'ları reverse et |
| 8 | codegen.c | `'A'` char'ı `char*[]` init'te hata | char → string promotion |
| 9 | codegen.c | param array bracket kaybolması | `emit_array_brackets()` |
| 10 | codegen.c | eksik auto-include'lar | recursive header scanner |
| 11 | parser.c | AST next pointer chain corruption | `ast_list_append` tail tracking |
