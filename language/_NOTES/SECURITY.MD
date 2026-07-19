# GCL v0.1 — UB / Memory Leak / Guvenlik Analizi

## UB (Undefined Behavior)

| # | Dosya | Sorun |
|---|-------|-------|
| 1 | memory.c:flist_alloc | Pointer statik buffer disina tasabilir, kontrol yok |
| 2 | ast.c:node_alloc | calloc NULL donerse crash. Hicbir caller kontrol etmiyor |
| 3 | parser.c:parse_decl | n->ival = val->ival binary node icin yanlis deger |
| 4 | memory.c:pool_free | signed modulo, implementation-defined |

## Memory Leak

| # | Dosya | Nesne |
|---|-------|-------|
| 5 | semantic.c | Sym (sem_add_sym) hic free edilmiyor |
| 6 | semantic.c | Scope (sem_enter_scope) free yok |
| 7 | parser.c | callee node_ident alinip kullanilmiyor |

## Guvenlik Aciklari

| # | Dosya | Aciklik |
|---|-------|---------|
| 8 | main.c:read_file | fread donusu kontrolsuz, uninitialized memory |
| 9 | memory.c:area | sz=0 → memset wrap-around, pool silinir |
| 10 | memory.c | mem_shutdown pool/freelist temizlemez |

## Fixler

| # | Pri | Durum |
|---|-----|-------|
| 1 | A | FIXLENDI — bound check eklendi |
| 2 | A | FIXLENDI — NULL kontrol eklendi |
| 8 | B | FIXLENDI — fread kontrol eklendi |
| 9 | B | FIXLENDI — area(0) korumasi eklendi |
| 3 | C | KABUL — codegen'de duzelir |
| 4 | C | KABUL — dusuk risk |
| 5 | D | BEKLIYOR |
| 6 | D | BEKLIYOR |
| 7 | D | BEKLIYOR |
| 10 | D | BEKLIYOR |
