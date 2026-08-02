"""Entry point for F4 OAR Conversion Tool (dev / PyInstaller)."""

from __future__ import annotations

import sys
from pathlib import Path


def _ensure_path() -> None:
    here = Path(__file__).resolve().parent
    if str(here) not in sys.path:
        sys.path.insert(0, str(here))


def main() -> None:
    _ensure_path()
    from gui import main as gui_main

    gui_main()


if __name__ == "__main__":
    main()
