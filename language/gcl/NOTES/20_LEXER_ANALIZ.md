# LEXER MODÜLÜ ANALİZ RAPORU

## 1. Modül Yapısı

```
src/lexer/
├── lexer.h           → Lexer context, public API (lexer_init/next/peek/advance)
├── lexer.c           → Ana lexer döngüsü, lexer_next() dağıtıcı
├── token.h           → TokenType enum (48 tip), Token struct
├── token.c           → token_type_name(), token_init(), token_free()
│
├── lexer_check.h/.c       → Karakter tipi kontrolleri (is_digit, is_alpha, is_space...)
├── lexer_comment.h/.c     → /* */ // --[[ {- { <!-- """ yorumları
├── lexer_identifier.h/.c  → Tanımlayıcı + anahtar kelime token'ları
├── lexer_number.h/.c      → Integer (hex/octal/binary/dec), float, hex-float
├── lexer_operator.h/.c    → Operatörler (++, --, <<=, &&, vs.)
├── lexer_preprocessor.h/.c→ #define, #if, #error, @lib, @include, @extern
├── lexer_skip.h/.c        → Boşluk, newline atlama
└── lexer_string.h/.c      → "string" ve 'char' literal'ları
```

## 2. Token Tipleri (48 adet)

| Kategori | Sayı | TokenType'lar |
|----------|------|---------------|
| C99 keyword | 35 | `KEYWORD_AUTO` - `KEYWORD_WHILE`, `_Bool`, `_Complex` |
| gcLang direktif | 3 | `KEYWORD_AT_LIB/INCLUDE/EXTERN` |
| Built-in sabit | 3 | `KEYWORD_TRUE/FALSE/NULL` |
| Noktalama | 13 | `(){}[];:,.` `->` `...` `#` `##` |
| Operatör | 31 | aritmetik, karşılaştırma, bitsel, atama, ++/-- |
| Literal | 4 | `LITERAL_INTEGER/FLOAT/STRING/CHAR` |
| Tanımlayıcı | 1 | `IDENTIFIER` |
| Preprocessor | 1 | `PREPROCESSOR_LINE` (ham #direktif satırı) |
| Özel | 2 | `TOKEN_EOF`, `TOKEN_ERROR` |

## 3. Güvenlik Önlemleri

### Bellek Yönetimi
- Her Token `value` alanı heap'te saklanır → `token_free()` ile free
- `Lexer` context stack'te, `source` pointer dışarıdan gelir (free caller'da)
- `lexer_next()` dönüş değeri call by value (Token struct kopyalanır)
- `MAX_LOOP_SAFETY = 5000000` — anti-infinite-loop

### NULL / Bound Güvenliği
- `lexer_peek()` → pos `source_len`'i geçerse -1 döner
- `lexer_advance()` → aynı
- Token `value` NULL olabilir (operatörlerde) → token_type_name() ve token_free() NULL-safe

### Hata Yönetimi
- `lexer_error(Lexer *lex, fmt, ...)` → stderr'e formatlı hata + `err_count++`
- Her alt modül kendi validasyonunu yapar, hatalı token'da `lexer_error()` çağırır + `TOKEN_ERROR` döner
- Toplam hata `lex.err_count` ile takip edilir

## 4. Hatalar (lex tarafından yakalanan)

| Hata Türü | Kaynak |
|-----------|--------|
| String kapatılmamış (satır sonu) | lexer_string.c |
| Char kapatılmamış | lexer_string.c |
| Boş char literal `''` | lexer_string.c |
| Bilinmeyen escape `\z` | lexer_string.c |
| Invalid hex literal `0xGHI` | lexer_number.c |
| Invalid binary `0b123` | lexer_number.c |
| Invalid octal `078` | lexer_number.c |
| Float exponent digitsiz `1.0e` | lexer_number.c |
| Hex float exponent digitsiz | lexer_number.c |
| Blok yorum kapatılmamış `/*` | lexer_comment.c |
| Bilinmeyen #direktif `#foo` | lexer_preprocessor.c |
| #define isim digitsiz başlangıç | lexer_preprocessor.c |
| Beklenmeyen karakter `@` | lexer.c |

## 5. Test Durumu

| Test Dosyası | Lex Hata | Durum |
|-------------|----------|-------|
| `hello.gcsf` | 0 | ✅ |
| `extern_regular.gcsf` | 0 | ✅ |
| `bad_types.gcsf` | 0 | 🔶 geçersiz literal → lexer geçerli token üretir (parser hata verir) |
| `bad_string.gcsf` | 4 | ✅ beklenen (string/char hataları) |
| `test_errors.gcsf` | 9 | ✅ beklenen (9 farklı hata) |
| `bad_preprocessor.gcsf` | 1 | ✅ |
| `test_macro_nospace.gcsf` | 3 | ✅ |

## 6. Özet

**Lexer modülü stabil ve güvenli.** Her token tipi için ayrı .c/.h, modüler yapı. Hatalar lexer seviyesinde yakalanıp raporlanıyor, program çökmeden devam ediyor. Bellek yönetimi düzgün (token_free ile). 48 token tipi, tüm C99 operatörleri + gcLang'e özel direktifler destekleniyor.
