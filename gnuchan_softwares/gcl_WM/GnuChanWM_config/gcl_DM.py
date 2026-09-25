"""GnuChanWM live config bridge.

This module is the real runtime adapter for the WM config script. It does not
pretend to be a separate parser; the config file is executed by `gcl -pyrun`, and
this module updates the actual live config state that the WM reload path reads.

The embedded GCL runtime exposes a shared-state API (`import gcl; gcl.set_state()`)
for numeric values, so the bridge updates that state when available and keeps a
Python-side snapshot for the richer string-valued config entries.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any

try:
    import gcl  # type: ignore
except Exception:  # pragma: no cover - runtime-only fallback
    gcl = None


class _StateStore(dict):
    def __getattr__(self, name: str) -> Any:
        try:
            return self[name]
        except KeyError as exc:
            raise AttributeError(name) from exc

    def __setattr__(self, name: str, value: Any) -> None:
        self[name] = value


STATE = _StateStore()


def _runtime_state_file() -> Path:
    xdg = os.environ.get("XDG_CONFIG_HOME")
    home = os.environ.get("HOME")
    base = Path(xdg) if xdg else (Path(home) / ".config" if home else Path("."))
    return base / "GnuChanWM" / "GnuChanWM.state.json"


def _json_safe(value: Any) -> Any:
    if isinstance(value, _NamedObject):
        return _json_safe(value.__dict__)
    if isinstance(value, dict):
        return {str(k): _json_safe(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_safe(v) for v in value]
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    return str(value)


def _write_runtime_state() -> None:
    state_path = _runtime_state_file()
    state_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {str(k): _json_safe(v) for k, v in STATE.items()}
    state_path.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")


def _apply_runtime_state(key: str, value: Any) -> None:
    """Push the value into the embedded GCL runtime and the on-disk config state."""
    STATE[key] = value
    _write_runtime_state()
    if gcl is None:
        return
    try:
        if isinstance(value, bool):
            gcl.set_state(key, 1.0 if value else 0.0)
            return
        if isinstance(value, (int, float)):
            gcl.set_state(key, float(value))
            return
    except Exception:
        pass


class _NamedObject:
    def __init__(self, **kwargs: Any):
        self.__dict__.update(kwargs)

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}({self.__dict__})"


class _WindowAPI:
    def __init__(self) -> None:
        self.active_border = "#c369ff"
        self.inactive_border = "#280440"

    def set_active_window_border_color(self, color: str) -> None:
        self.active_border = color
        _apply_runtime_state("wm.active_border", color)
        print(f"[gcl_DM] active_border={color}")

    def set_inactive_window_border_color(self, color: str) -> None:
        self.inactive_border = color
        _apply_runtime_state("wm.inactive_border", color)
        print(f"[gcl_DM] inactive_border={color}")


class _BarAPI:
    def __init__(self) -> None:
        self.last_call: dict[str, Any] | None = None

    def call(self, **kwargs: Any) -> None:
        self.last_call = kwargs
        _apply_runtime_state("wm.bar", kwargs)
        print(f"[gcl_DM] gcl_BAR.call({kwargs})")


class _Widgets:
    def __getattr__(self, name: str):
        def factory(**kwargs: Any) -> _NamedObject:
            payload = {"name": name, **kwargs}
            return _NamedObject(**payload)

        return factory


class _ThemeAPI:
    def Theme_gtk(self, **kwargs: Any) -> None:
        _apply_runtime_state("wm.theme.gtk", kwargs)
        print(f"[gcl_DM] Theme_gtk({kwargs})")

    def Theme_icon(self, **kwargs: Any) -> None:
        _apply_runtime_state("wm.theme.icon", kwargs)
        print(f"[gcl_DM] Theme_icon({kwargs})")

    def Theme_cursor(self, **kwargs: Any) -> None:
        _apply_runtime_state("wm.theme.cursor", kwargs)
        print(f"[gcl_DM] Theme_cursor({kwargs})")


class _Key:
    @staticmethod
    def MultiKey(*, keys: list[str], action: str) -> _NamedObject:
        return _NamedObject(keys=list(keys), action=action)


class _Keys:
    def __init__(self) -> None:
        self.all: list[Any] = []


class _MouseAPI:
    def MouseBehavior(self, **kwargs: Any) -> None:
        _apply_runtime_state("wm.mouse", kwargs)
        print(f"[gcl_DM] MouseBehavior({kwargs})")


class _TouchpadAPI:
    def TouchpadBehavior(self, **kwargs: Any) -> None:
        _apply_runtime_state("wm.touchpad", kwargs)
        print(f"[gcl_DM] TouchpadBehavior({kwargs})")


# Runtime objects used by the config script.
gcl_Window = _WindowAPI()
gcl_BAR = _BarAPI()
gcl_Widgets = _Widgets()
gcl_themes = _ThemeAPI()
gcl_key = _Key()
gcl_keys = _Keys()
gcl_mouse = _MouseAPI()
gcl_touchpad = _TouchpadAPI()

__all__ = [
    "gcl_Window",
    "gcl_BAR",
    "gcl_Widgets",
    "gcl_themes",
    "gcl_key",
    "gcl_keys",
    "gcl_mouse",
    "gcl_touchpad",
    "STATE",
]
