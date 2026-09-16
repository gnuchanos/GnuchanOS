/*
 * complete_type.c — Tip çıkarımı ve üye çözümleme (§6).
 */
#include "complete_type.h"
#include <stdio.h>

/* Çözümlenmiş bir ifadenin tipi: ya native modül, ya bir (nominal) tip adı. */
typedef struct {
    int  is_module;
    char name[GCLC_NAME];
} EType;

/* "Raylib.Camera3D" -> "Camera3D": modul ile nitelenmis tip adini sadeleştir.

   GCL'de Raylib/Raygui tipleri modul uyesidir; kullanici bunlari `Raylib.`
   onekiyle de yazabilir (`Raylib.Vector2 v;`). Bildirilen tip adi onekle
   kaydedilirse (`v : Raylib.Vector2`) alan cozumlemesi `gcl_native_struct()`
   aramasini tutturamaz ve `v.x` sessizce bos doner. Bu yuzden cozumlemeden
   once modul oneki SOYULUR; hem `Vector2 v;` hem `Raylib.Vector2 v;` calisir. */
static const char *strip_module_prefix(const char *name) {
    if (!name) return "";
    const char *dot = strchr(name, '.');
    if (!dot || !dot[1]) return name;
    char mod[64];
    size_t n = (size_t)(dot - name);
    if (n == 0 || n >= sizeof(mod)) return name;
    memcpy(mod, name, n);
    mod[n] = '\0';
    return gcl_native_is_module(mod) ? dot + 1 : name;
}

static void etype_set(EType *e, int is_module, const char *name) {
    e->is_module = is_module;
    const char *n = name ? name : "";
    if (!is_module) n = strip_module_prefix(n);
    snprintf(e->name, sizeof(e->name), "%s", n);
}

/* Zincirin ilk token'ının tipini çöz. */
static int resolve_primary(const GclScope *s, const char *tok, EType *out) {
    if (!tok || !tok[0]) return 0;

    /* 1) Native modül adı: Raylib, Stdio, ... */
    if (gcl_native_is_module(tok)) { etype_set(out, 1, tok); return 1; }

    /* 2) Değişken → bildirilen tipi. */
    const GclSym *sy = gcl_scope_find(s, tok);
    if (sy && sy->type[0]) { etype_set(out, 0, sy->type); return 1; }

    /* 3) Statik erişim: tip adı (kullanıcı struct/typedef veya native struct). */
    if (gcl_scope_is_type(s, tok) || gcl_native_is_struct(tok)) {
        etype_set(out, 0, tok);
        return 1;
    }
    return 0;
}

/* cur tipinin "mem" üyesinin tipini out'a yaz. */
static int member_type(const GclScope *s, const EType *cur, const char *mem,
                       EType *out) {
    if (!mem || !mem[0]) return 0;

    if (cur->is_module) {
        const GclNativeMember *nm = gcl_native_find(cur->name, mem);
        if (!nm) return 0;
        if (!nm->ret[0] || strcmp(nm->ret, "void") == 0) return 0; /* zincir durur */
        etype_set(out, 0, nm->ret);
        return 1;
    }

    /* Kullanıcı struct/typedef alanı — NATIVE STRUCT'TAN ÖNCE.
       Kullanıcının kendi tanımı aynı adlı yerleşik yapıyı GÖLGELER (yerel
       değişkenin globali gölgelemesi gibi). Eskiden önce native struct
       aranıyordu: kullanıcının `enum Color { RED, GREEN }` tanımı raylib'in
       paketli `Color` alanları (r,g,b,a) tarafından EZİLİYORDU ve `Color.`
       listede RED/GREEN yerine r/g/b/a gösteriyordu. */
    const GclTypeDef *td = gcl_scope_find_type(s, cur->name);
    if (td) {
        for (int i = 0; i < td->member_count; i++) {
            if (strcmp(td->members[i].name, mem) == 0) {
                if (!td->members[i].type[0]) return 0;
                etype_set(out, 0, td->members[i].type);
                return 1;
            }
        }
        return 0;   /* kullanıcı tipi kazandı: native alanlara DÜŞÜLMEZ */
    }

    /* Native struct alanı (Vector2.x → float). */
    const GclNativeStruct *st = gcl_native_struct(cur->name);
    if (st) {
        for (int i = 0; i < st->field_count; i++) {
            if (strcmp(st->fields[i].name, mem) == 0) {
                if (!st->fields[i].type[0]) return 0;
                etype_set(out, 0, st->fields[i].type);
                return 1;
            }
        }
    }
    return 0;
}

/* Native üye → GclItemKind. */
static GclItemKind native_kind(const GclNativeMember *m) {
    if (m->flags & NF_TYPE)  return CIK_TYPE;
    if (m->flags & NF_CONST) return CIK_CONST;
    if (m->flags & NF_ENUM)  return CIK_ENUM_VAL;
    return CIK_FUNC;
}

static void push_native(GclCompletionResult *out, const GclNativeMember *m,
                        const char *file) {
    GclItemKind k = native_kind(m);
    char sig[256];
    sig[0] = '\0';
    if (k == CIK_FUNC)
        snprintf(sig, sizeof(sig), "%s %s(%s)",
                 (m->ret[0] ? m->ret : "void"), m->name,
                 (m->params[0] ? m->params : "void"));
    gcl_complete_result_push(out, m->name, NULL, k,
                             (m->ret[0] ? m->ret : NULL),
                             (m->params[0] ? m->params : NULL),
                             (sig[0] ? sig : NULL),
                             (m->doc && m->doc[0] ? m->doc : NULL), file);
}

/* Bir tipin (modül / native struct / kullanıcı tipi) üyelerini yaz. */
static int emit_members(const GclScope *s, const EType *t, const char *file,
                        GclCompletionResult *out) {
    if (t->is_module) {
        const GclNativeModule *mod = gcl_native_module(t->name);
        if (!mod) return 0;
        for (int i = 0; i < mod->member_count; i++)
            push_native(out, &mod->members[i], file);
        return 1;
    }

    /* Kullanıcı tipi ÖNCE: kendi tanımı aynı adlı yerleşik yapıyı gölgeler
       (bkz. member_type içindeki aynı kural). `enum Color { RED, GREEN }`
       yazan kullanıcı `Color.` ile RED/GREEN görmeli, raylib'in r/g/b/a
       alanlarını değil. */
    const GclTypeDef *td = gcl_scope_find_type(s, t->name);
    if (td) {
        for (int i = 0; i < td->member_count; i++) {
            const GclMember *m = &td->members[i];
            gcl_complete_result_push(out, m->name, NULL,
                                     (m->kind == CIK_ENUM_VAL ? CIK_ENUM_VAL : CIK_FIELD),
                                     (m->type[0] ? m->type : NULL),
                                     NULL, NULL, NULL, file);
        }
        return 1;
    }

    const GclNativeStruct *st = gcl_native_struct(t->name);
    if (st) {
        for (int i = 0; i < st->field_count; i++) {
            gcl_complete_result_push(out, st->fields[i].name, NULL, CIK_FIELD,
                                     (st->fields[i].type[0] ? st->fields[i].type : NULL),
                                     NULL, NULL, NULL, file);
        }
        return 1;
    }
    return 0;
}

int gcl_type_members_of(const GclScope *s, const char *type_name,
                        const char *file, GclCompletionResult *out) {
    if (!type_name || !type_name[0]) return 0;
    EType t;
    if (gcl_native_is_module(type_name)) etype_set(&t, 1, type_name);
    else                                 etype_set(&t, 0, type_name);
    return emit_members(s, &t, file, out);
}

int gcl_type_of_name(const GclScope *s, const char *name,
                     char *out_type, size_t out_cap) {
    if (!name || !out_type || out_cap == 0) return 0;
    out_type[0] = '\0';
    if (gcl_native_is_module(name) || gcl_native_is_struct(name)) {
        snprintf(out_type, out_cap, "%s", name);
        return 1;
    }
    const GclSym *sy = gcl_scope_find(s, name);
    if (sy && sy->type[0]) {
        snprintf(out_type, out_cap, "%s", strip_module_prefix(sy->type));
        return 1;
    }
    if (gcl_scope_is_type(s, name)) { snprintf(out_type, out_cap, "%s", name); return 1; }
    return 0;
}

/* Zincir elemanı, çağrı parantezi OLMADAN kullanılan bir FONKSİYON mu?
   GCL'de fonksiyon adı kendi başına bir değer DEĞİLDİR; yalnızca `fn()` ile
   dönüş değerine zincirlenir. `ctx->chain_is_call` bu bilgiyi taşıyordu ama
   motor onu hiç okumuyordu: `makePoint.` (parantezsiz) dönüş tipinin üyelerini
   öneriyordu. Native modül üyeleri bu denetimin dışındadır (kapsam sembolü
   değiller); modül fonksiyonlarının zinciri `ret` tipiyle zaten kurulur. */
static int chain_uses_uncalled_fn(const GclScope *s, const GclContext *ctx, int i) {
    if (ctx->chain_is_call[i]) return 0;
    const GclSym *sy = gcl_scope_find(s, ctx->chain[i]);
    return sy != NULL && sy->is_call;
}

int gcl_type_resolve_members(const GclScope *scope, const GclContext *ctx,
                             const char *file, GclCompletionResult *out) {
    if (!ctx || ctx->chain_len <= 0) return 0;

    for (int i = 0; i < ctx->chain_len; i++)
        if (chain_uses_uncalled_fn(scope, ctx, i)) return 0;

    EType cur;
    if (!resolve_primary(scope, ctx->chain[0], &cur)) return 0;

    for (int i = 1; i < ctx->chain_len; i++) {
        EType nxt;
        if (!member_type(scope, &cur, ctx->chain[i], &nxt)) return 0;
        cur = nxt;
    }
    return emit_members(scope, &cur, file, out);
}
