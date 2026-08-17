import { useEffect, useRef } from "react";

interface Props {
  data: string[];
}

export default function OutputPanel({ data }: Props) {
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (ref.current) ref.current.scrollTop = ref.current.scrollHeight;
  }, [data.length]);

  return (
    <div className="output" ref={ref}>
      {data.length === 0 && <div className="panel-empty">Waiting for output...</div>}
      {data.map((line, i) => (
        <div key={i} className="out-line">
          {line}
        </div>
      ))}
    </div>
  );
}
