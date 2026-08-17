import { useEffect, useMemo, useState } from "react";
import { BookOpen, FileText, FolderTree, RefreshCw } from "lucide-react";
import type { DocsEntry } from "../types";

interface Props {
  root: string;
  onOpen: (path: string) => void;
  refreshKey: number;
}

/* Sol panelin DOCS sekmesi: projedeki Library klasorlerindeki .doc
 * (dil dokumanlari), .gcReference (imzali API) ve wrapper (.py/.lua)
 * dosyalarini UST PROSES (docs:list IPC) ile listeler.
 *
 * KATEGORiLER: main.ts listDocs artik her kayda lang (lua/python/bridge)
 * + group ("Lua"/"Python"/"Bridge") ekler ve kopyalari tekilleye yariyor.
 * Bu panel once dil grubuna, sonra o grubun icinde tur basligina
 * (Rehberler / Imzali API / Yardimcilar) gore bolumler:
 *
 *   LUA
 *     Rehberler
 *       lua.doc
 *     Imzali API
 *       lua.gcReference, lua_raylib.gcReference
 *   PYTHON
 *     Rehberler
 *       py.doc
 *     ...
 *
 * Kopyalar art? gorunmez: ayni (dil, tur, dosya-adi) tek kez listelenir. */
export default function DocsPanel({ root, onOpen, refreshKey }: Props) {
  const [entries, setEntries] = useState<DocsEntry[]>([]);
  const [loading, setLoading] = useState(true);

  const load = async () => {
    if (!root) return;
    setLoading(true);
    try {
      const list = await window.ide.docsList(root);
      setEntries(list);
    } catch {
      setEntries([]);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    load();
  }, [root, refreshKey]);

  /* Gruplar: dil (Lua/Python/Bridge/Other) -> icinde tur (doc/ref/lib).
   * Boylece "referanslar ve doc'lar ayri yerde" kurali saglanir:
   *   LUA
   *     Rehberler ......... lua.doc
   *     Imzali API ........ lua.gcReference, lua_raylib.gcReference
   *     Yardimcilar ....... lua.gcDL helpers
   *   PYTHON
   *     Rehberler ......... py.doc
   *     ... */
  const groups = useMemo(() => {
    /* "other" bilinçli olarak ATLANIR: kök kopyalar artık adından dil
     * tespitiyle doğru gruba düşer (main.ts listDocs); geriye "other"
     * kalırsa bu gerçek bir "sınıflandırılamayan" dosyadır ve DOCS
     * panelinde yeri yoktur — tekrar/karışıklık hissi yaratır. */
    const langOrder: DocsEntry["lang"][] = ["lua", "python", "bridge"];
    const kindOrder: DocsEntry["kind"][] = ["doc", "ref", "lib"];
    const byLang = new Map<DocsEntry["lang"], DocsEntry[]>();
    for (const e of entries) {
      const arr = byLang.get(e.lang);
      if (arr) arr.push(e);
      else byLang.set(e.lang, [e]);
    }
    const out: {
      lang: DocsEntry["lang"];
      group: string;
      sections: { kind: DocsEntry["kind"]; title: string; items: DocsEntry[] }[];
    }[] = [];
    for (const lang of langOrder) {
      const items = byLang.get(lang);
      if (!items || items.length === 0) continue;
      const sections = kindOrder
        .map((kind) => ({
          kind,
          title:
            kind === "doc"
              ? "Rehberler"
              : kind === "ref"
                ? "Imzali API"
                : "Yardimcilar",
          items: items
            .filter((e) => e.kind === kind)
            .sort((a, b) => a.name.localeCompare(b.name)),
        }))
        .filter((s) => s.items.length > 0);
      if (sections.length === 0) continue;
      out.push({
        lang,
        group:
          lang === "lua"
            ? "Lua"
            : lang === "python"
              ? "Python"
              : lang === "bridge"
                ? "Bridge"
                : "Other",
        sections,
      });
    }
    return out;
  }, [entries]);

  /* Tur ikonu (seksiyon basliklarinda kullanilir) */
  const kindIcon = (kind: DocsEntry["kind"]) => {
    if (kind === "doc") return <BookOpen size={13} />;
    if (kind === "ref") return <FolderTree size={13} />;
    return <FileText size={13} />;
  };

  return (
    <div className="explorer">
      <div className="panel-header">
        <span>
          <BookOpen size={14} /> DOCS
        </span>
        <button className="icon-btn" onClick={() => load()} title="Refresh">
          <RefreshCw size={13} />
        </button>
      </div>

      {!root ? (
        <div className="panel-empty">Open a project to see docs.</div>
      ) : loading ? (
        <div className="panel-empty">Loading...</div>
      ) : entries.length === 0 ? (
        <div className="panel-empty">No docs found in Library/.</div>
      ) : (
        <div className="docs-list">
          {groups.map((g) => (
            <div key={g.lang} className="docs-group">
              <div className="docs-group-title">{g.group}</div>
              {g.sections.map((s) => (
                <div key={s.kind} className="docs-section">
                  <div className="docs-section-title">
                    {kindIcon(s.kind)}
                    <span>{s.title}</span>
                  </div>
                  {s.items.map((e) => (
                    <div
                      key={e.path}
                      className="docs-item"
                      onClick={() => onOpen(e.path)}
                      title={e.path}
                    >
                      <span className="docs-icon">{kindIcon(e.kind)}</span>
                      <span className="docs-name">{e.name}</span>
                      <span className={`docs-badge ${e.kind}`}>{e.kind}</span>
                    </div>
                  ))}
                </div>
              ))}
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
