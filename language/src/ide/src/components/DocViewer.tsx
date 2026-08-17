import { useEffect, useMemo, useRef, useState } from "react";
import { FileText, ListTree } from "lucide-react";

interface Props {
  path: string;
}

interface TocEntry {
  id: string;
  level: number;
  text: string;
}

/* .doc ve .gcReference dosyalarini okunabilir, duzenlenemez HTML olarak
 * gosterir. Icerik yerel ve guvenilir oldugu icin dogrudan React ile
 * render edilir (escapeHtml gerekmez).
 *
 * Duzen: solda kalici icerik haritasi (TOC), sagda dokuman. Basliklar
 * scroll-spy ile takip edilir (hangisindesin belli olur). Fonksiyonlar
 * "kart" olarak gruplanir: imza + aciklama + calistirilabilir ornek. */
export default function DocViewer({ path }: Props) {
  const [raw, setRaw] = useState("");
  const [loading, setLoading] = useState(true);
  const [active, setActive] = useState("");
  const bodyRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    let alive = true;
    window.ide
      .readFile(path)
      .then((content) => {
        if (!alive) return;
        setRaw(content);
        setLoading(false);
      })
      .catch(() => {
        if (!alive) return;
        setRaw("# Read error\n\nThe file could not be read.");
        setLoading(false);
      });
    return () => {
      alive = false;
    };
  }, [path]);

  const { toc, body } = useMemo(() => parseDoc(raw), [raw]);

  /* scroll-spy: gorunur basligi takip et */
  useEffect(() => {
    if (loading) return;
    const rootEl = bodyRef.current;
    if (!rootEl) return;
    const heads = Array.from(
      rootEl.querySelectorAll<HTMLElement>("[data-sec]"),
    );
    if (heads.length === 0) return;
    const io = new IntersectionObserver(
      (entries) => {
        for (const en of entries) {
          if (en.isIntersecting) {
            setActive((en.target as HTMLElement).dataset.sec ?? "");
          }
        }
      },
      { root: rootEl, rootMargin: "-10% 0px -80% 0px", threshold: 0 },
    );
    heads.forEach((h) => io.observe(h));
    return () => io.disconnect();
  }, [loading, body]);

  if (loading) return <div className="doc-loading">Loading…</div>;

  return (
    <div className="doc-viewer">
      <div className="doc-viewer-head">
        <span className="doc-viewer-title">
          <FileText size={14} />
          {path.split(/[\\/]/).pop()}
        </span>
        <span className="doc-badge read-only">read-only reference</span>
      </div>
      <div className="doc-viewer-body">
        {toc.length > 0 && (
          <nav className="doc-nav">
            <div className="doc-nav-title">
              <ListTree size={12} /> CONTENTS
            </div>
            {toc.map((t) => (
              <a
                key={t.id}
                href={`#${t.id}`}
                className={`doc-nav-item lvl-${Math.min(t.level, 3)} ${active === t.id ? "active" : ""}`}
                onClick={(e) => {
                  e.preventDefault();
                  document
                    .getElementById(t.id)
                    ?.scrollIntoView({ behavior: "smooth", block: "start" });
                }}
              >
                {t.text}
              </a>
            ))}
          </nav>
        )}
        <div className="doc-body" ref={bodyRef}>
          {body}
        </div>
      </div>
    </div>
  );
}

interface ListGroup {
  text: string;
  desc: string;
  code: string;
}

/* ---- hafif markdown → React (baslik + TOC, kartli liste, tablo, kod) ---- */
function parseDoc(src: string): { toc: TocEntry[]; body: React.ReactNode[] } {
  const lines = src.replace(/\r\n/g, "\n").split("\n");
  const toc: TocEntry[] = [];
  const out: React.ReactNode[] = [];
  let i = 0;
  let key = 0;
  let sec = 0;
  const nextId = () => `sec-${sec++}`;

  while (i < lines.length) {
    const line = lines[i];
    const trim = line.trim();

    /* code fence */
    if (trim.startsWith("```")) {
      const lang = trim.slice(3).trim();
      const buf: string[] = [];
      i++;
      while (i < lines.length && !lines[i].trimStart().startsWith("```")) {
        buf.push(lines[i]);
        i++;
      }
      i++;
      out.push(
        <div className="doc-code" key={key++}>
          {lang && <div className="doc-code-lang">{lang}</div>}
          <pre>
            <code>{buf.join("\n")}</code>
          </pre>
        </div>,
      );
      continue;
    }

    /* hr */
    if (/^\s*(---|\*\*\*|___)\s*$/.test(trim)) {
      i++;
      continue;
    }

    /* header */
    const h = line.match(/^(#{1,6})\s+(.*)$/);
    if (h) {
      const level = h[1].length;
      const text = h[2];
      const id = nextId();
      if (level >= 2) toc.push({ id, level, text: text.replace(/\*\*/g, "") });
      const node = inline(text);
      if (level === 1)
        out.push(
          <h1 className="doc-h1" key={key++} id={id} data-sec={id}>
            {node}
          </h1>,
        );
      else if (level === 2)
        out.push(
          <h2 className="doc-h2" key={key++} id={id} data-sec={id}>
            {node}
          </h2>,
        );
      else if (level === 3)
        out.push(
          <h3 className="doc-h3" key={key++} id={id} data-sec={id}>
            {node}
          </h3>,
        );
      else
        out.push(
          <h4 className="doc-h4" key={key++} id={id} data-sec={id}>
            {node}
          </h4>,
        );
      i++;
      continue;
    }

    /* blockquote */
    if (trim.startsWith(">")) {
      const text = line.replace(/^\s*>\s?/, "");
      out.push(
        <blockquote className="doc-quote" key={key++}>
          {inline(text)}
        </blockquote>,
      );
      i++;
      continue;
    }

    /* table */
    if (
      line.includes("|") &&
      i + 1 < lines.length &&
      /^\s*\|?\s*[-:| ]+\|?\s*$/.test(lines[i + 1])
    ) {
      const head = parseRow(line);
      i += 2;
      const rows: string[][] = [];
      while (i < lines.length && lines[i].includes("|")) {
        rows.push(parseRow(lines[i]));
        i++;
      }
      out.push(
        <table className="doc-table" key={key++}>
          <thead>
            <tr>
              {head.map((c, ci) => (
                <th key={ci}>{inline(c)}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {rows.map((r, ri) => (
              <tr key={ri}>
                {r.map((c, ci) => (
                  <td key={ci}>{inline(c)}</td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>,
      );
      continue;
    }

    /* bullet liste: ardindan gelen girintili aciklama + ornekle kart */
    const li = line.match(/^\s*[-*]\s+(.*)$/);
    if (li) {
      const groups: ListGroup[] = [];
      let text = li[1];
      i++;
      while (i < lines.length) {
        const m = lines[i].match(/^\s*[-*]\s+(.*)$/);
        if (!m) break;
        groups.push({ text, desc: "", code: "" });
        text = m[1];
        i++;
      }
      let desc = "";
      let code = "";
      while (i < lines.length && /^\s+\S/.test(lines[i])) {
        const raw = lines[i].replace(/^\s+/, "");
        if (raw.startsWith("```")) {
          const lang = raw.slice(3).trim();
          const buf: string[] = [];
          i++;
          while (
            i < lines.length &&
            !lines[i].trimStart().startsWith("```")
          ) {
            buf.push(lines[i]);
            i++;
          }
          i++;
          code = buf.join("\n");
        } else if (raw.startsWith('"')) {
          const m = raw.match(/^"(.*)"\s*$/);
          desc += (desc ? " " : "") + (m ? m[1] : raw);
          i++;
        } else if (raw.trim() !== "") {
          desc += (desc ? " " : "") + raw;
          i++;
        } else {
          i++;
        }
      }
      groups.push({ text, desc, code });
      out.push(
        <div className="doc-fn-list" key={key++}>
          {groups.map((g, gi) => (
            <div className="doc-fn" key={gi}>
              <code className="doc-fn-sig">
                {g.text.replace(/\*\*/g, "")}
              </code>
              {g.desc && <div className="doc-fn-desc">{g.desc}</div>}
              {g.code && (
                <pre className="doc-fn-ex">
                  <code>{g.code}</code>
                </pre>
              )}
            </div>
          ))}
        </div>,
      );
      continue;
    }

    /* bos satir */
    if (trim === "") {
      i++;
      continue;
    }

    out.push(
      <p className="doc-p" key={key++}>
        {inline(line)}
      </p>,
    );
    i++;
  }

  return { toc, body: out };
}

function parseRow(line: string): string[] {
  const trimmed = line.trim().replace(/^\|/, "").replace(/\|$/, "");
  return trimmed.split("|").map((c) => c.trim());
}

/* inline markdown: **bold**, *italic*, `code` */
function inline(text: string): React.ReactNode[] {
  const parts: React.ReactNode[] = [];
  const re = /(\*\*[^*]+\*\*|\*[^*]+\*|`[^`]+`)/g;
  let last = 0;
  let m: RegExpExecArray | null;
  let key = 0;
  while ((m = re.exec(text)) !== null) {
    if (m.index > last) parts.push(text.slice(last, m.index));
    const tok = m[0];
    if (tok.startsWith("**"))
      parts.push(<strong key={key++}>{tok.slice(2, -2)}</strong>);
    else if (tok.startsWith("*"))
      parts.push(<em key={key++}>{tok.slice(1, -1)}</em>);
    else
      parts.push(
        <code className="doc-inline" key={key++}>
          {tok.slice(1, -1)}
        </code>,
      );
    last = m.index + tok.length;
  }
  if (last < text.length) parts.push(text.slice(last));
  return parts;
}
