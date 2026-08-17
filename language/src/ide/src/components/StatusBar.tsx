import { Play, Save } from "lucide-react";

interface Props {
  path: string;
  lang: string;
  line: number;
  col: number;
  modified: boolean;
  gclReady: boolean;
}

export default function StatusBar({
  path,
  lang,
  line,
  col,
  modified,
  gclReady,
}: Props) {
  return (
    <div className="status-bar">
      <span className="sb-item">{gclReady ? "GCL ✓" : "gcl not found"}</span>
      <span className="sb-sep">|</span>
      <span className="sb-item">
        {path ? path.split(/[\\/]/).pop() : "no file"}
        {modified ? " ●" : ""}
      </span>
      <span className="sb-sep">|</span>
      <span className="sb-item">{lang}</span>
      <span className="sb-spacer" />
      <span className="sb-item">
        Ln {line}, Col {col}
      </span>
    </div>
  );
}
