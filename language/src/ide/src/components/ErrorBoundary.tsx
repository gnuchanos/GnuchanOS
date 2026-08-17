import { Component, type ReactNode } from "react";

interface Props {
  children: ReactNode;
  label?: string;
}

interface State {
  error: Error | null;
}

/* One panel crashing must never blank the whole IDE. */
export default class ErrorBoundary extends Component<Props, State> {
  state: State = { error: null };

  static getDerivedStateFromError(error: Error): State {
    return { error };
  }

  componentDidCatch(error: Error, info: unknown) {
    console.error("[GnuChanIDE] panel error:", error, info);
  }

  render() {
    if (this.state.error) {
      return (
        <div className="panel-empty panel-error" title={this.state.error.stack}>
          <div>⚠ {this.props.label ?? "Panel"} crashed</div>
          <div className="panel-error-msg">{this.state.error.message}</div>
        </div>
      );
    }
    return this.props.children;
  }
}
