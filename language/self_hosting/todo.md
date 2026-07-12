# GCL-SH (Gnuchan Self-Hosting) Python Clone Dil Tasarımı

Bu dil **Python'un birebir aynısıdır**. Tek fark:
- `{}` girintileme (indentation) yerine blokları belirtir
- `;` ifadeleri sonlandırır (newline anlamını kaybeder)
- `()` Python'dakiyle aynıdır (çağrı, gruplama, demet)

Python'da **olmayan** hiçbir şey bu dilde **olmaz**.

---

## ÖNEMLİ: `:` YOK, `{}` BLOK SINIRIDIR

Python'da `:` + indent blok belirtir. GCL'de indent yerine `{}` kullanıldığı için `:` **kullanılmaz**.

```
// DOĞRU — ) sonra direkt {
def main() {
    println("hello");
}

// DOĞRU
if (x > 0) {
    println("pozitif");
} else {
    println("negatif");
}

// DOĞRU
while (i < 10) {
    i = i + 1;
}

// DOĞRU
for x in range(10) {
    println(x);
}
```

---

## 1. TEMEL VERİ TİPLERİ

| Tip | Örnek | Açıklama |
|-----|-------|----------|
| `None` | `x = None` | Yokluk değeri |
| `bool` | `True`, `False` | Mantıksal |
| `int` | `42`, `-1`, `0xFF`, `0b1010`, `0o77` | Tam sayı (keyfi hassasiyet) |
| `float` | `3.14`, `1e10`, `inf`, `nan` | Ondalıklı sayı |
| `complex` | `1+2j` | Karmaşık sayı |
| `str` | (aşağıya bak) | Karakter dizisi — UNICODE, immutable |
| `bytes` | `b"data"` | Byte dizisi (0-255) |
| `list` | `[1, 2, 3]` | Değiştirilebilir dizi |
| `tuple` | `(1, 2)` | Değiştirilemez dizi |
| `dict` | `{"a": 1}` | Anahtar-değer haritası |
| `set` | `{1, 2, 3}` | Kümeler (sırasız, benzersiz) |

---

## 2. DEĞİŞKENLER ve KAPSAM

| Özellik | Örnek | Açıklama |
|---------|-------|----------|
| Dinamik tipleme | `x = 42; x = "hello"` | Tip bildirimi yok |
| Atama ile oluşturma | `x = 42` | `let`/`var` YOK — direkt yazılır |
| Destructuring | `a, b = 1, 2` | Çoklu atama |
| Augmented assignment | `x += 5; x *= 2;` | += -= *= /= //= %= **= vb |
| Walrus operator | `if (n := len(x)) > 0` | `:=` (Python 3.8+) |
| `del` | `del x; del arr[0]` | Silme |
| `global` | `global x` | Global değişken bildirimi |
| `nonlocal` | `nonlocal x` | Kapsayan scope'a erişim |

## 3. OPERATÖRLER

| Kategori | Operatörler |
|----------|------------|
| Parantez | `() {} []` |
| Aritmetik | `+ - * / // % **` |
| Karşılaştırma | `== != < > <= >=` |
| Kimlik | `is`, `is not` |
| Üyelik | `in`, `not in` |
| Bitsel | `\| & ^ ~ << >>` |
| Mantıksal | `and or not` |
| Atama | `= += -= *= /= //= %= **= \|= &= ^= <<= >>=` |
| Walrus | `:=` |

## 4. KONTROL AKIŞI

```
if (x > 0) {
    println("pozitif");
} elif (x < 0) {
    println("negatif");
} else {
    println("sıfır");
}

while (i < 10) {
    println(i);
    i = i + 1;
}

for x in range(10) {
    println(x);
}

for (i = 0; i < 10; i = i + 1) {
    println(i);
}
```

## 5. FONKSİYONLAR

```
def merhaba(isim) {
    return "Merhaba, " + isim + "!";
}

def topla(a, b) {
    return a + b;
}

def ussu_al(taban, us = 2) {
    return taban ** us;
}

def fonk(*args, **kwargs) {
    println(args);
    println(kwargs);
}
```

## 6. SINIFLAR

```
class Kisi {
    def __init__(self, isim, yas) {
        self.isim = isim;
        self.yas = yas;
    }
    
    def selamla(self) {
        return "Merhaba, ben " + self.isim;
    }
}

class Ogrenci(Kisi) {
    def __init__(self, isim, yas, okul) {
        super().__init__(isim, yas);
        self.okul = okul;
    }
}
```

## 7. MODÜLLER

```
import math;
import numpy as np;
from math import sqrt;
from math import sqrt as karekok;

if (__name__ == "__main__") {
    main();
}
```

## 8. DATA STRUCTURES

| Yapı | Örnek | Özellikler |
|------|-------|-----------|
| list | `[1, 2, 3]` | Mutable, sıralı |
| tuple | `(1, 2, 3)` | Immutable, sıralı, hashable |
| dict | `{"a": 1}` | Mutable, hash table |
| set | `{1, 2, 3}` | Mutable, benzersiz |

## 9. COMPREHENSION

```
squares = [x*x for x in range(10) if x > 0];
evens = {x for x in lst if x % 2 == 0};
square_map = {x: x*x for x in range(5)};
gen = (x*x for x in range(10));
```

---

## UYGULAMA DURUMU

### Mevcut (Çalışıyor)
- [x] **error.ml** — Rust stili hata raporlama (context, ^ marker, renkli)
- [x] Debug logging (phase tracking, timestamps)
- [x] Error chain (caused by, notes)
- [x] **main.ml** — CLI argüman yönetimi (-p, -t, -c, -o, -d)
- [x] Lexer dump mode, AST dump mode, Type check mode
- [x] Compile and generate C code
- [x] **lexer.mll** — Token'lar, string'ler, yorumlar, sayılar
- [x] **parser.mly** — if, while, for, def, class, struct, enum, expr'ler
- [x] **typechecker.ml** — HM type inference, pozisyonlu hatalar
- [x] **codegen.ml** — C kod üretimi

### Sıradaki (todo.md'deki her şey implement edilecek)
- [ ] `:` kaldırıldı — bloklar direkt `)` sonrası `{}` ile
- [ ] `class` keyword'ü (lexer + parser + ast + typechecker + codegen)
- [ ] `for x in y:` (Python-style iteration) — DONE (parser'da var)
- [ ] `try` / `except` / `finally`
- [ ] `raise`
- [ ] `with`
- [ ] `yield`
- [ ] `lambda`
- [ ] `match` / `case`
- [ ] ternary (`x if cond else y`)
- [ ] `global`, `nonlocal`, `del`
- [ ] comprehension'lar (`[x for x in ...]`)
- [ ] slicing (`x[a:b:c]`)
- [ ] set literals (`{1, 2, 3}` — dict'ten ayırt et)
- [ ] decorator'lar (`@decorator`)
- [ ] type hints (`def f(x: int) -> str:`)
- [ ] import'lar (`import`, `from ... import`)

### Self-Hosting (native GCL compiler in GCL)
- [ ] Lexer'ı GCL'de yaz
- [ ] Parser'ı GCL'de yaz
- [ ] AST'yi GCL'de tanımla
- [ ] Type checker'ı GCL'de yaz
- [ ] Codegen'i GCL'de yaz
