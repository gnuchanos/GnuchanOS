import { useEffect, useMemo, useRef } from "react";

interface Props {
  data: string[];
}

/* ANSI escape code → CSS color map (gcl palette) */
const ANSI_COLORS: Record<string, string> = {
  "38;2;47;3;87": "#2f0357",
  "38;2;71;4;133": "#470485",
  "38;2;95;5;179": "#5f05b3",
  "38;2;111;6;209": "#6f06d1",
  "38;2;160;59;255": "#a03bff",
  "38;2;80;200;80": "#50c850",
  "38;2;220;200;60": "#dcc83c",
  "38;2;80;140;255": "#508cff",
  "38;2;220;60;60": "#dc3c3c",
};

/* Parse ANSI color spans into <span> elements with inline color */
function parseAnsi(text: string): React.ReactNode[] {
  const parts: React.ReactNode[] = [];
  /* eslint-disable-next-line no-control-regex */
  const re = /\u001b\[([0-9;]*)m/g;
  let lastIndex = 0;
  let currentColor = "";
  let key = 0;

  let match: RegExpExecArray | null;
  while ((match = re.exec(text)) !== null) {
    /* text before this escape */
    if (match.index > lastIndex) {
      const segment = text.slice(lastIndex, match.index);
      if (currentColor) {
        parts.push(
          <span key={key++} style={{ color: currentColor }}>
            {segment}
          </span>,
        );
      } else {
        parts.push(segment);
      }
    }

    const code = match[1];
    if (code === "0" || code === "") {
      currentColor = "";
    } else if (ANSI_COLORS[code]) {
      currentColor = ANSI_COLORS[code];
    }

    lastIndex = match.index + match[0].length;
  }

  /* remaining text after last escape */
  if (lastIndex < text.length) {
    const segment = text.slice(lastIndex);
    if (currentColor) {
      parts.push(
        <span key={key++} style={{ color: currentColor }}>
          {segment}
        </span>,
      );
    } else {
      parts.push(segment);
    }
  }

  return parts;
}

export default function OutputPanel({ data }: Props) {
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (ref.current) ref.current.scrollTop = ref.current.scrollHeight;
  }, [data.length]);

  const rendered = useMemo(
    () => data.map((line, i) => ({ key: i, nodes: parseAnsi(line) })),
    [data],
  );

  return (
    <div className="output" ref={ref}>
      {data.length === 0 && (
        <div className="panel-empty">Waiting for output...</div>
      )}
      {rendered.map((r) => (
        <div key={r.key} className="out-line">
          {r.nodes}
        </div>
      ))}
    </div>
  );
}
