import { useLayoutEffect, useRef } from "react";
import type { CompletionItem } from "../completions";

/* Monaco'nun suggest widget'i yerine kendi DOM popup'imiz. Monaco'nun
 * render katmani (codicon/olcum) aradan cikar; satir metinleri dogrudan
 * React DOM'unda cizilir — metin asla bos kalamaz.
 *
 * signature mode: "func(" sonrasi items=[] + signature dolu gelir;
 * popup yalnizca parametre imzasini gosterir (VS Code parameter hints). */

interface Props {
  items: CompletionItem[];
  index: number;
  top: number;
  left: number;
  rowHeight: number;
  signature?: string;
  onMouseDown: (index: number) => void;
}

export default function AutocompletePopup({
  items,
  index,
  top,
  left,
  rowHeight,
  signature,
  onMouseDown,
}: Props) {
  const listRef = useRef<HTMLDivElement | null>(null);
  const activeRef = useRef<HTMLDivElement | null>(null);

  /* klavye ile gezinen secili satiri her zaman gorunur kıl */
  useLayoutEffect(() => {
    const list = listRef.current;
    const active = activeRef.current;
    if (!list || !active) return;
    const lt = active.offsetTop;
    const lb = lt + active.offsetHeight;
    if (lt < list.scrollTop) list.scrollTop = lt;
    else if (lb > list.scrollTop + list.clientHeight)
      list.scrollTop = lb - list.clientHeight;
  }, [index]);

  /* Tum itemlari render et; popup listesi CSS max-height + overflow-y ile
   * kaydirilabilir (scroll). Yalnizca ilk 12'yi kismak scroll'un
   * calisamamasi anlamina geliyordu. */
  return (
    <div
      className="ac-popup"
      style={{ top, left, "--ac-row-height": `${rowHeight}px` } as React.CSSProperties}
      onMouseDown={(e) => e.preventDefault() /* editor imlecini kacirma */}
    >
      {items.length > 0 && (
        <>
          <div className="ac-popup-list" ref={listRef}>
            {items.map((item, i) => (
              <div
                key={`${item.label}-${i}`}
                ref={i === index ? activeRef : undefined}
                className={`ac-row ${i === index ? "active" : ""}`}
                style={{ height: rowHeight }}
                onMouseDown={() => onMouseDown(i)}
                title={item.detail}
              >
                <span className="ac-kind">{kindIcon(item.kind)}</span>
                <span className="ac-label">{item.label}</span>
                <span className="ac-detail">{item.detail}</span>
              </div>
            ))}
          </div>
          <div className="ac-popup-foot">
            {items.length} item{items.length === 1 ? "" : "s"} — ↑↓ navigate, Enter
            insert, Esc close
          </div>
        </>
      )}
      {/* Fonksiyon parametre gostergesi: tamamlama listesinin ALTINDA. Ayri
       * pencere degil — popup'in kendi akisinda durur, boylece ne yazilan
       * satiri ne de listeyi orter. "func(" icinde (")" / yeni satir / Esc
       * olmadigi surece kalici durur. */}
      {signature && (
        <div className="ac-popup-sig">
          <span className="ac-sig-label">params</span>
          <code className="ac-sig-code">{signature}</code>
        </div>
      )}
    </div>
  );
}

function kindIcon(kind: string): string {
  switch (kind) {
    case "fn":
    case "function":
      return "ƒ";
    case "module":
      return "▣";
    case "keyword":
      return "⌗";
    case "const":
      return "c";
    case "class":
    case "type":
      return "◆";
    default:
      return "·";
  }
}
