/*
 * gcl_terminal.h — "#pragma commandline" (terminal uygulamasi) destegi.
 *
 * Ornek: language/examples/gcl_simple_syntax/11_scanf_commandline_test.gcsf
 *
 *   #pragma commendline
 *   #native <Stdio>
 *   int main() { int t = 0; Stdio.scanf(t); }
 *
 * IDE'nin kendi terminali YOKTUR (GUI uygulamasidir ve cocuk sureci
 * CREATE_NO_WINDOW ile baslatir) — bu yuzden Stdio.scanf girdi bekleyemez.
 * #pragma commandline varsa program OTOMATIK olarak isletim sisteminin
 * terminalinde calistirilir.
 */

#ifndef GCL_TERMINAL_H
#define GCL_TERMINAL_H

/* Kaynakta "#pragma commandline" var mi?
   "commendline" (ornek dosyadaki yazim), "commandline" ve "cmdline"
   varyantlari kabul edilir. #| ... |# yorum bloklari atlanir; boylece
   yalnizca PRAGMA'yi ANLATAN dokumantasyon blogu pragma sayilmaz. */
int gcl_terminal_pragma_present(const char *src);

/* Terminal uygulamasi moduna gir:
     - GCL_TERMINAL=1 isaretlenir (Embed alt surecleri ayri terminal acar),
     - konsol yoksa program YENI bir terminal penceresinde yeniden baslatilir.
   Donus: 1 → yeni terminal acildi ve kapandi, bu surec HEMEN cikmali,
          0 → bu surecte devam edilebilir (konsol hazir ya da acilamadi). */
int gcl_terminal_enter(void);

#endif /* GCL_TERMINAL_H */
