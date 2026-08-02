"""CustomTkinter GUI for the F4 OAR Conversion Tool.

Workflow: configure one conversion job, Add to Queue (form clears), repeat for
other folders with different destinations / SubMod names, then Run Queue.
"""

from __future__ import annotations

import copy
import datetime as dt
import re
import shutil
import sys
import tkinter as tk
import tkinter.filedialog as fd
import tkinter.messagebox as mb
from pathlib import Path

import customtkinter as ctk

from oar_conversion_tool.ba2_io import Ba2Error
from oar_conversion_tool.engine import (
    JobOptions,
    build_preview,
    discover_roots,
    discover_subgraphs,
    extract_ba2_source,
    run_job,
)
from oar_conversion_tool.esp_io import parse_esp
from oar_conversion_tool.paths import AnimRoot
from oar_conversion_tool.weapon_match import RootWeaponGroup, match_roots_to_weapons

# Fallout-inspired dark theme
BG = "#1a1d1a"
PANEL = "#242824"
ACCENT = "#3d8c40"
ACCENT_HOVER = "#4caf50"
TEXT = "#e8f0e8"
MUTED = "#8a9a8a"
DANGER = "#c45c26"

PLUGIN_SUFFIXES = {".esp", ".esm", ".esl"}

ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("green")


def _tool_dir() -> Path:
    """Directory for writable logs: next to the exe when frozen, else next to gui.py."""
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent


def _strip_ba2_main_suffix(stem: str) -> str:
    """Drop a trailing "Main" archive-role word from a BA2 stem for auto-naming.

    BA2s are conventionally split by content role, e.g. "SomeMod - Main.ba2",
    "SomeMod_Main.ba2", "SomeMod Main.ba2". That trailing role word is meaningful
    for archive bookkeeping but is noise in a suggested Pack/SubMod name, so strip
    only the word "main" itself (requires a preceding separator, so "Mainline" or
    a bare "Main.ba2" are left alone). The separator before it (a dash or
    underscore) is kept, e.g. "SomeMod - Main" -> "SomeMod -", so the suggested
    name still reads like it was deliberately trimmed rather than truncated.
    Leaves other BA2 role suffixes (Textures, Voices, etc.) untouched since those
    weren't asked for.
    """
    stripped = re.sub(r"(?<=[\s_-])main\s*$", "", stem, flags=re.IGNORECASE).rstrip()
    return stripped or stem


def _discover_plugins(folder: Path) -> list[Path]:
    """Find plugin files directly in folder (not recursive). Prefer non-_OAR names."""
    if not folder.is_dir():
        return []
    hits: list[Path] = []
    for p in folder.iterdir():
        if p.is_file() and p.suffix.lower() in PLUGIN_SUFFIXES:
            hits.append(p)
    hits.sort(key=lambda p: (p.stem.lower().endswith("_oar"), p.name.lower()))
    return hits


class EspPickerDialog(ctk.CTkToplevel):
    """Simple multi-ESP chooser when a source folder has more than one plugin."""

    def __init__(self, master, plugins: list[Path]):
        super().__init__(master)
        self.title("Select ESP")
        self.geometry("560x320")
        self.configure(fg_color=BG)
        self.transient(master)
        self.grab_set()
        self.result: Path | None = None

        ctk.CTkLabel(
            self,
            text="Multiple plugins found in the source folder. Pick one:",
            text_color=TEXT,
        ).pack(anchor="w", padx=14, pady=(14, 8))

        self.listbox = tk.Listbox(
            self,
            bg="#1e221e",
            fg=TEXT,
            selectbackground=ACCENT,
            highlightthickness=0,
            font=("Segoe UI", 11),
        )
        self.listbox.pack(fill="both", expand=True, padx=14, pady=4)
        self._plugins = plugins
        for p in plugins:
            self.listbox.insert("end", p.name)
        self.listbox.selection_set(0)

        brow = ctk.CTkFrame(self, fg_color="transparent")
        brow.pack(fill="x", padx=14, pady=12)
        ctk.CTkButton(
            brow, text="Use Selected", command=self._ok,
            fg_color=ACCENT, hover_color=ACCENT_HOVER,
        ).pack(side="left")
        ctk.CTkButton(
            brow, text="Skip (no ESP)", command=self._skip,
            fg_color="#3a3f3a",
        ).pack(side="left", padx=8)

        self.protocol("WM_DELETE_WINDOW", self._skip)
        self.wait_window(self)

    def _ok(self) -> None:
        sel = self.listbox.curselection()
        if sel:
            self.result = self._plugins[sel[0]]
        self.destroy()

    def _skip(self) -> None:
        self.result = None
        self.destroy()


class OARConversionApp(ctk.CTk):
    def __init__(self) -> None:
        super().__init__()
        self.title("F4 OAR Conversion Tool")
        self.geometry("1100x700")
        self.minsize(960, 600)
        self.configure(fg_color=BG)

        # Current form state (one job being edited)
        self.source_dirs: list[Path] = []
        self.subgraph_paths: list[Path] = []
        self.esp_path: Path | None = None
        self.scanned_roots: list[AnimRoot] = []
        self.root_vars: dict[str, ctk.BooleanVar] = {}
        # root_key -> " -> WeaponEdid (0xForm)" suffix, only populated when a Scan finds
        # 2+ distinct weapon forms (see _match_groups/_build_root_labels), so a single-
        # weapon job's checkboxes stay exactly as plain as before.
        self._root_weapon_labels: dict[str, str] = {}

        # BA2 archive used in lieu of a source folder (None when using a plain folder).
        self.ba2_path: Path | None = None
        self.ba2_extract_dir: Path | None = None

        # Queued jobs, each a full JobOptions snapshot
        self.queue: list[JobOptions] = []

        # Session log file (also mirrored in the on-screen textbox)
        self.log_path = self._open_log_file()

        self._build()
        self._sync_esp_controls()
        self._refresh_queue_view()
        self._log(f"Log file: {self.log_path}")
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _on_close(self) -> None:
        """Avoid leaking an unused BA2 extraction if the window is closed mid-form."""
        self._discard_active_ba2()
        self.destroy()

    def _open_log_file(self) -> Path:
        log_dir = _tool_dir() / "logs"
        log_dir.mkdir(parents=True, exist_ok=True)
        stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        path = log_dir / f"F4OARConversionTool_{stamp}.log"
        path.write_text(
            f"F4 OAR Conversion Tool session started {dt.datetime.now().isoformat()}\n",
            encoding="utf-8",
        )
        return path

    def _build(self) -> None:
        pad = {"padx": 8, "pady": 3}

        titlerow = ctk.CTkFrame(self, fg_color="transparent")
        titlerow.pack(fill="x", padx=12, pady=(8, 0))
        ctk.CTkLabel(
            titlerow,
            text="F4 OAR Conversion Tool",
            font=ctk.CTkFont(size=18, weight="bold"),
            text_color=ACCENT,
        ).pack(side="left")
        ctk.CTkLabel(
            titlerow,
            text="  |  Configure one folder \u2192 Add to Queue \u2192 repeat \u2192 Confirm & Run Queue.",
            text_color=MUTED,
            font=ctk.CTkFont(size=11),
        ).pack(side="left")
        ctk.CTkLabel(
            self,
            text=(
                "Preview/log also saved to logs/. BA2 support format re-derived from "
                "BSA Browser by AlexxEG (github.com/AlexxEG/BSA_Browser)."
            ),
            text_color=MUTED,
            font=ctk.CTkFont(size=10),
        ).pack(anchor="w", padx=12, pady=(0, 3))

        # Scrollable form area; log stays pinned at the bottom of the window
        body = ctk.CTkScrollableFrame(self, fg_color=BG)
        body.pack(fill="both", expand=True, padx=6, pady=1)

        # --- Current job inputs ---
        inputs = ctk.CTkFrame(body, fg_color=PANEL, corner_radius=8)
        inputs.pack(fill="x", **pad)
        ctk.CTkLabel(
            inputs,
            text="Current job: inputs",
            font=ctk.CTkFont(size=13, weight="bold"),
        ).pack(anchor="w", padx=10, pady=(6, 1))

        row = ctk.CTkFrame(inputs, fg_color="transparent")
        row.pack(fill="x", padx=10, pady=1)
        ctk.CTkButton(
            row, text="Source Folder…", command=self._set_source,
            fg_color=ACCENT, hover_color=ACCENT_HOVER,
        ).pack(side="left")
        ctk.CTkButton(
            row, text="Subgraph .txt…", command=self._add_subgraph,
            fg_color=ACCENT, hover_color=ACCENT_HOVER,
        ).pack(side="left", padx=6)
        ctk.CTkButton(
            row, text="BA2 Archive…", command=self._set_ba2,
            fg_color=ACCENT, hover_color=ACCENT_HOVER,
        ).pack(side="left", padx=6)
        ctk.CTkButton(
            row, text="ESP…", command=self._choose_esp,
            fg_color=ACCENT, hover_color=ACCENT_HOVER,
        ).pack(side="left")
        ctk.CTkButton(
            row, text="Clear Form", command=self._clear_form,
            fg_color="#3a3f3a", hover_color="#4a504a",
        ).pack(side="left", padx=6)
        self.keep_ba2 = ctk.BooleanVar(value=False)
        ctk.CTkCheckBox(
            row,
            text="Keep extracted BA2 files",
            variable=self.keep_ba2,
            fg_color=ACCENT,
        ).pack(side="left", padx=(10, 0))

        self.inputs_label = ctk.CTkLabel(
            inputs,
            text=(
                "No inputs yet. Pick a Source Folder or a BA2 Archive (ESP is auto-detected "
                "from the same folder either way)."
            ),
            text_color=MUTED,
            justify="left",
            anchor="w",
        )
        self.inputs_label.pack(fill="x", padx=10, pady=(1, 6))

        # --- Destination / names ---
        destf = ctk.CTkFrame(body, fg_color=PANEL, corner_radius=8)
        destf.pack(fill="x", **pad)
        ctk.CTkLabel(
            destf,
            text="Current job: output",
            font=ctk.CTkFont(size=13, weight="bold"),
        ).pack(anchor="w", padx=10, pady=(6, 1))

        drow = ctk.CTkFrame(destf, fg_color="transparent")
        drow.pack(fill="x", padx=10, pady=1)
        self.dest_var = ctk.StringVar(value="")
        ctk.CTkEntry(drow, textvariable=self.dest_var).pack(side="left", fill="x", expand=True)
        ctk.CTkButton(
            drow, text="Browse…", width=90, command=self._browse_dest,
            fg_color=ACCENT, hover_color=ACCENT_HOVER,
        ).pack(side="left", padx=6)
        self.keep_dest = ctk.BooleanVar(value=False)
        ctk.CTkCheckBox(
            drow,
            text="Keep this path between jobs",
            variable=self.keep_dest,
            fg_color=ACCENT,
        ).pack(side="left")

        nrow = ctk.CTkFrame(destf, fg_color="transparent")
        nrow.pack(fill="x", padx=10, pady=(1, 6))
        ctk.CTkLabel(nrow, text="Pack").pack(side="left")
        self.pack_var = ctk.StringVar(value="OAR Conversion")
        ctk.CTkEntry(nrow, textvariable=self.pack_var, width=160).pack(side="left", padx=6)
        ctk.CTkLabel(nrow, text="TR SubMod").pack(side="left")
        self.tr_name_var = ctk.StringVar(value="Tactical Reload")
        ctk.CTkEntry(nrow, textvariable=self.tr_name_var, width=170).pack(side="left", padx=6)
        ctk.CTkLabel(nrow, text="Idle SubMod").pack(side="left")
        self.idle_name_var = ctk.StringVar(value="Idle Empty")
        ctk.CTkEntry(nrow, textvariable=self.idle_name_var, width=150).pack(side="left", padx=6)

        # --- Operations ---
        ops = ctk.CTkFrame(body, fg_color=PANEL, corner_radius=8)
        ops.pack(fill="x", **pad)
        ctk.CTkLabel(
            ops,
            text="Current job: operations",
            font=ctk.CTkFont(size=13, weight="bold"),
        ).pack(anchor="w", padx=10, pady=(6, 1))

        self.do_tr = ctk.BooleanVar(value=True)
        self.do_idle = ctk.BooleanVar(value=False)
        self.do_3rd = ctk.BooleanVar(value=False)
        self.do_esp = ctk.BooleanVar(value=False)
        self.remove_kw = ctk.BooleanVar(value=True)
        self.remove_master = ctk.BooleanVar(value=True)
        self.overwrite = ctk.BooleanVar(value=False)

        orow = ctk.CTkFrame(ops, fg_color="transparent")
        orow.pack(fill="x", padx=10, pady=1)
        ctk.CTkCheckBox(orow, text="Tactical Reload → OAR", variable=self.do_tr, fg_color=ACCENT).pack(side="left")
        ctk.CTkCheckBox(orow, text="Idle Empty", variable=self.do_idle, fg_color=ACCENT).pack(side="left", padx=12)
        ctk.CTkCheckBox(orow, text="Include 3rd person", variable=self.do_3rd, fg_color=ACCENT).pack(side="left")

        self.esp_frame = ctk.CTkFrame(ops, fg_color="transparent")
        self.esp_frame.pack(fill="x", padx=10, pady=1)
        self.esp_check = ctk.CTkCheckBox(
            self.esp_frame,
            text="Patch ESP (strip AnimsReloadReserve / drop TR master if unused)",
            variable=self.do_esp,
            fg_color=ACCENT,
            command=self._sync_esp_controls,
        )
        self.esp_check.pack(side="left")
        self.kw_check = ctk.CTkCheckBox(
            self.esp_frame, text="Strip keyword", variable=self.remove_kw, fg_color=ACCENT
        )
        self.kw_check.pack(side="left", padx=10)
        self.master_check = ctk.CTkCheckBox(
            self.esp_frame, text="Drop TR master if unused", variable=self.remove_master, fg_color=ACCENT
        )
        self.master_check.pack(side="left")

        erow = ctk.CTkFrame(ops, fg_color="transparent")
        erow.pack(fill="x", padx=10, pady=(1, 6))
        ctk.CTkLabel(erow, text="Idle bones").pack(side="left")
        self.bones_var = ctk.StringVar(value="WeaponBolt")
        ctk.CTkEntry(erow, textvariable=self.bones_var, width=140).pack(side="left", padx=6)
        ctk.CTkLabel(erow, text="Manual IsEquipped FormID (single-weapon jobs only)").pack(side="left")
        self.equip_fid_var = ctk.StringVar(value="")
        ctk.CTkEntry(erow, textvariable=self.equip_fid_var, width=80).pack(side="left", padx=4)
        ctk.CTkLabel(erow, text="plugin").pack(side="left")
        self.equip_plugin_var = ctk.StringVar(value="")
        ctk.CTkEntry(erow, textvariable=self.equip_plugin_var, width=120).pack(side="left", padx=4)

        # --- Weapon folders (formerly "anim roots") ---
        rootsf = ctk.CTkFrame(body, fg_color=PANEL, corner_radius=8)
        rootsf.pack(fill="x", **pad)
        rhead = ctk.CTkFrame(rootsf, fg_color="transparent")
        rhead.pack(fill="x", padx=10, pady=(6, 0))
        ctk.CTkLabel(
            rhead,
            text="Weapon folders to convert",
            font=ctk.CTkFont(size=13, weight="bold"),
        ).pack(side="left")
        ctk.CTkButton(
            rhead, text="Rescan", width=80, command=self._scan,
            fg_color=ACCENT, hover_color=ACCENT_HOVER,
        ).pack(side="left", padx=6)
        ctk.CTkButton(
            rhead, text="All", width=50,
            command=lambda: self._set_all_roots(True), fg_color="#3a3f3a",
        ).pack(side="left")
        ctk.CTkButton(
            rhead, text="None", width=50,
            command=lambda: self._set_all_roots(False), fg_color="#3a3f3a",
        ).pack(side="left", padx=4)

        ctk.CTkLabel(
            rootsf,
            text=(
                "Checked = copied into the SubMod. A folder gets its own weapon match "
                "(-> shown inline) and IsEquipped SubMod whenever the ESP has 2+ weapons."
            ),
            text_color=MUTED,
            font=ctk.CTkFont(size=11),
            justify="left",
            anchor="w",
            wraplength=1000,
        ).pack(fill="x", padx=10, pady=(0, 1))

        self.roots_frame = ctk.CTkScrollableFrame(rootsf, height=90, fg_color="#1e221e")
        self.roots_frame.pack(fill="x", padx=10, pady=(1, 6))

        # --- Queue actions ---
        qact = ctk.CTkFrame(body, fg_color=PANEL, corner_radius=8)
        qact.pack(fill="x", **pad)
        brow = ctk.CTkFrame(qact, fg_color="transparent")
        brow.pack(fill="x", padx=10, pady=6)
        ctk.CTkButton(
            brow, text="Preview Current", command=self._preview_current,
            fg_color="#3a3f3a", hover_color="#4a504a", width=130,
        ).pack(side="left")
        ctk.CTkButton(
            brow, text="Add to Queue", command=self._add_to_queue,
            fg_color=ACCENT, hover_color=ACCENT_HOVER, width=120,
        ).pack(side="left", padx=8)
        ctk.CTkCheckBox(
            brow, text="Overwrite existing", variable=self.overwrite, fg_color=DANGER
        ).pack(side="left", padx=6)

        # --- Queue list ---
        queuef = ctk.CTkFrame(body, fg_color=PANEL, corner_radius=8)
        queuef.pack(fill="x", **pad)
        qhead = ctk.CTkFrame(queuef, fg_color="transparent")
        qhead.pack(fill="x", padx=10, pady=(6, 1))
        ctk.CTkLabel(qhead, text="Job queue", font=ctk.CTkFont(size=13, weight="bold")).pack(side="left")
        self.queue_count_label = ctk.CTkLabel(qhead, text="(0)", text_color=MUTED)
        self.queue_count_label.pack(side="left", padx=6)
        ctk.CTkButton(
            qhead, text="Remove Selected", width=120, command=self._remove_selected_queue,
            fg_color="#3a3f3a",
        ).pack(side="right", padx=4)
        ctk.CTkButton(
            qhead, text="Clear Queue", width=100, command=self._clear_queue,
            fg_color="#3a3f3a",
        ).pack(side="right", padx=4)

        self.queue_frame = ctk.CTkScrollableFrame(queuef, height=80, fg_color="#1e221e")
        self.queue_frame.pack(fill="x", padx=10, pady=(1, 3))
        self.queue_select_vars: list[ctk.BooleanVar] = []

        qrun = ctk.CTkFrame(queuef, fg_color="transparent")
        qrun.pack(fill="x", padx=10, pady=(0, 6))
        ctk.CTkButton(
            qrun, text="Preview Queue", command=self._preview_queue,
            fg_color="#3a3f3a", hover_color="#4a504a", width=120,
        ).pack(side="left")
        ctk.CTkButton(
            qrun, text="Confirm & Run Queue", command=self._run_queue,
            fg_color=ACCENT, hover_color=ACCENT_HOVER, width=170,
        ).pack(side="left", padx=8)
        ctk.CTkButton(
            qrun, text="Run Current Only", command=self._run_current,
            fg_color="#3a3f3a", hover_color="#4a504a", width=130,
        ).pack(side="left")

        # Pinned log (outside scrollable body so it always fits)
        log_bar = ctk.CTkFrame(self, fg_color=PANEL, corner_radius=0)
        log_bar.pack(fill="x", side="bottom")
        log_head = ctk.CTkFrame(log_bar, fg_color="transparent")
        log_head.pack(fill="x", padx=10, pady=(4, 0))
        ctk.CTkLabel(log_head, text="Log", font=ctk.CTkFont(size=12, weight="bold")).pack(side="left")
        self.log_path_label = ctk.CTkLabel(log_head, text="", text_color=MUTED, font=ctk.CTkFont(size=10))
        self.log_path_label.pack(side="left", padx=8)
        ctk.CTkButton(
            log_head, text="Open Log Folder", width=120, command=self._open_log_folder,
            fg_color="#3a3f3a",
        ).pack(side="right")

        self.log = ctk.CTkTextbox(log_bar, height=110, fg_color="#121512", text_color=TEXT)
        self.log.pack(fill="x", padx=10, pady=(3, 6))
        self.log_path_label.configure(text=str(self.log_path))
        self._log(
            "Ready. Pick Source Folder (ESP auto-detects), set destination, Rescan if needed, "
            "then Preview / Add to Queue."
        )

    # ----- logging / labels -----

    def _log(self, msg: str) -> None:
        line = msg if msg.endswith("\n") else msg + "\n"
        try:
            self.log.insert("end", line)
            self.log.see("end")
        except Exception:  # noqa: BLE001
            pass
        try:
            with self.log_path.open("a", encoding="utf-8") as fh:
                fh.write(line)
        except OSError:
            pass

    def _open_log_folder(self) -> None:
        folder = self.log_path.parent
        try:
            import os

            os.startfile(str(folder))  # type: ignore[attr-defined]
        except Exception as exc:  # noqa: BLE001
            mb.showinfo("Log folder", f"{folder}\n\n({exc})")

    def _refresh_inputs_label(self) -> None:
        lines = []
        if self.ba2_path:
            lines.append(f"BA2: {self.ba2_path} (extracted -> {self.ba2_extract_dir})")
        else:
            for p in self.source_dirs:
                lines.append(f"Source: {p}")
        for p in self.subgraph_paths:
            lines.append(f"Subgraph: {p}")
        if self.esp_path:
            lines.append(f"ESP: {self.esp_path}")
        self.inputs_label.configure(
            text="\n".join(lines)
            if lines
            else "No inputs yet. Pick a Source Folder or a BA2 Archive (ESP is auto-detected)."
        )

    def _apply_auto_names(self, pack: str) -> None:
        """Set pack + TR/Idle SubMod names from a source/ESP stem.

        `pack` may still carry a trailing separator left over from
        `_strip_ba2_main_suffix` (e.g. "SomeMod -"), which reads fine inside a
        longer SubMod name ("SomeMod - Tactical Reload") but looks like a typo
        as a standalone Pack name ("SomeMod -"). So the Pack field gets that
        trailing punctuation trimmed off, while the SubMod names keep it.
        """
        pack = pack.strip() or "OAR Conversion"
        pack_label = pack.rstrip(" -_") or pack
        self.pack_var.set(pack_label)
        self.tr_name_var.set(f"{pack} Tactical Reload")
        self.idle_name_var.set(f"{pack} Idle Empty")

    # ----- form inputs -----

    def _discard_active_ba2(self) -> None:
        """Drop (and, unless kept, delete) any BA2 extraction from the current form.

        Called before switching to a plain Source Folder or picking a different BA2, so
        an abandoned extraction (never added to the queue, so run_job never cleans it up)
        does not linger under BA2_Extracted/ forever.
        """
        if self.ba2_extract_dir is None:
            return
        old_dir = self.ba2_extract_dir
        if self.keep_ba2.get():
            self._log(f"Keeping previous BA2 extraction (user preference): {old_dir}")
        else:
            try:
                shutil.rmtree(old_dir, ignore_errors=True)
                self._log(f"Discarded unused BA2 extraction: {old_dir}")
            except OSError as exc:
                self._log(f"Warning: could not remove {old_dir}: {exc}")
        self.ba2_path = None
        self.ba2_extract_dir = None

    def _set_source(self) -> None:
        """One primary source folder per job (replaces previous)."""
        p = fd.askdirectory(title="Source mod / meshes folder for this job")
        if not p:
            return
        self._discard_active_ba2()
        folder = Path(p)
        self.source_dirs = [folder]
        self._apply_auto_names(folder.name)
        self._autodetect_esp(folder)
        self._refresh_inputs_label()
        self._scan()

    def _set_ba2(self) -> None:
        """Pick a BA2 archive in lieu of a source folder; extract meshes\\actors from it."""
        p = fd.askopenfilename(
            title="BA2 archive for this job (in lieu of a source folder)",
            filetypes=[("Fallout 4 archives", "*.ba2"), ("All", "*.*")],
        )
        if not p:
            return
        self._discard_active_ba2()
        ba2_path = Path(p)
        extract_root = _tool_dir() / "BA2_Extracted"
        self._log(f"Extracting meshes\\actors from {ba2_path.name}…")
        try:
            extract_dir = extract_ba2_source(ba2_path, extract_root, log=self._log)
        except Ba2Error as exc:
            self._log(f"BA2 extraction failed: {exc}")
            mb.showerror("BA2 extraction failed", str(exc))
            return
        except OSError as exc:
            self._log(f"BA2 extraction failed: {exc}")
            mb.showerror("BA2 extraction failed", str(exc))
            return

        self.ba2_path = ba2_path
        self.ba2_extract_dir = extract_dir
        self.source_dirs = [extract_dir]
        self._apply_auto_names(_strip_ba2_main_suffix(ba2_path.stem))
        # ESPs ship as loose files next to the BA2, not inside it.
        self._autodetect_esp(ba2_path.parent)
        self._refresh_inputs_label()
        self._scan()

    def _autodetect_esp(self, folder: Path) -> None:
        plugins = _discover_plugins(folder)
        if not plugins:
            self._log(f"No ESP/ESM/ESL found directly in {folder}")
            return
        if len(plugins) == 1:
            self._set_esp(plugins[0], from_autodetect=True)
            return
        # Prefer a single non-_OAR plugin if that uniquely identifies the source
        primary = [p for p in plugins if not p.stem.lower().endswith("_oar")]
        if len(primary) == 1:
            self._set_esp(primary[0], from_autodetect=True)
            self._log(
                f"Auto-selected {primary[0].name} "
                f"(ignored {len(plugins) - 1} other plugin(s) in folder)."
            )
            return
        dlg = EspPickerDialog(self, plugins)
        if dlg.result is not None:
            self._set_esp(dlg.result, from_autodetect=True)
        else:
            self._log("ESP selection skipped.")

    def _add_subgraph(self) -> None:
        paths = fd.askopenfilenames(
            title="Subgraph text for this job",
            filetypes=[("Text", "*.txt"), ("All", "*.*")],
        )
        for p in paths:
            path = Path(p)
            if path not in self.subgraph_paths:
                self.subgraph_paths.append(path)
        self._refresh_inputs_label()

    def _choose_esp(self) -> None:
        p = fd.askopenfilename(
            title="ESP/ESM/ESL for this job",
            filetypes=[("Plugins", "*.esp *.esm *.esl"), ("All", "*.*")],
        )
        if not p:
            return
        self._set_esp(Path(p), from_autodetect=False)

    def _set_esp(self, path: Path, *, from_autodetect: bool) -> None:
        self.esp_path = path
        parent = path.parent
        if not self.source_dirs:
            self.source_dirs = [parent]
            self._apply_auto_names(parent.name if from_autodetect else path.stem)
        try:
            info = parse_esp(path)
            if info.weapons:
                w = info.weapons[0]
                self.equip_fid_var.set(w["form_id_hex"])
                self.equip_plugin_var.set(path.name)
            if not from_autodetect:
                # Manual pick: refresh names from ESP stem when still defaults
                cur_pack = self.pack_var.get().strip()
                if cur_pack in ("", "OAR Conversion") or cur_pack == parent.name:
                    self._apply_auto_names(path.stem)
            self._log(
                f"ESP: {path.name}; masters={info.masters}; WEAP count={len(info.weapons)}"
            )
        except Exception as exc:  # noqa: BLE001
            self._log(f"ESP parse warning: {exc}")
        self._refresh_inputs_label()
        self._sync_esp_controls()

    def _clear_form(self, *, keep_queued_ba2: bool = False) -> None:
        """Reset the current-job form so the next folder can be configured.

        keep_queued_ba2=True is used right after Add to Queue: the queued JobOptions
        snapshot already owns the ba2_extract_dir reference (run_job cleans it up when
        that job runs), so the form must forget it here WITHOUT deleting the folder.
        """
        if self.ba2_extract_dir is not None and not keep_queued_ba2:
            self._discard_active_ba2()
        else:
            self.ba2_path = None
            self.ba2_extract_dir = None
        self.source_dirs.clear()
        self.subgraph_paths.clear()
        self.esp_path = None
        self.scanned_roots.clear()
        self.root_vars.clear()
        self._root_weapon_labels.clear()
        if not self.keep_dest.get():
            self.dest_var.set("")
        self.pack_var.set("OAR Conversion")
        self.tr_name_var.set("Tactical Reload")
        self.idle_name_var.set("Idle Empty")
        self.bones_var.set("WeaponBolt")
        self.equip_fid_var.set("")
        self.equip_plugin_var.set("")
        self.do_tr.set(True)
        self.do_idle.set(False)
        self.do_3rd.set(False)
        self.do_esp.set(False)
        self.remove_kw.set(True)
        self.remove_master.set(True)
        self.overwrite.set(False)
        self.keep_ba2.set(False)
        self._render_roots()
        self._refresh_inputs_label()
        self._sync_esp_controls()

    def _browse_dest(self) -> None:
        p = fd.askdirectory(title="Destination folder for this job")
        if p:
            self.dest_var.set(p)

    def _sync_esp_controls(self) -> None:
        has_esp = self.esp_path is not None and self.esp_path.is_file()
        state = "normal" if has_esp else "disabled"
        self.esp_check.configure(state=state)
        self.kw_check.configure(state=state if self.do_esp.get() else "disabled")
        self.master_check.configure(state=state if self.do_esp.get() else "disabled")
        if not has_esp:
            self.do_esp.set(False)

    def _job_options(self) -> JobOptions:
        bones = [b.strip() for b in self.bones_var.get().split(",") if b.strip()]
        selected = []
        for root in self.scanned_roots:
            key = self._root_key(root)
            var = self.root_vars.get(key)
            if var is not None and var.get():
                selected.append(root)
        fid = self.equip_fid_var.get().strip() or None
        plugin = self.equip_plugin_var.get().strip() or None
        dest = Path(self.dest_var.get()) if self.dest_var.get().strip() else None
        return JobOptions(
            source_dirs=list(self.source_dirs),
            subgraph_paths=list(self.subgraph_paths),
            esp_path=self.esp_path,
            destination=dest,
            ba2_path=self.ba2_path,
            ba2_extract_dir=self.ba2_extract_dir,
            keep_ba2_extracted=self.keep_ba2.get(),
            do_tactical_reload=self.do_tr.get(),
            do_idle_empty=self.do_idle.get(),
            include_1st=True,
            include_3rd=self.do_3rd.get(),
            selected_roots=list(selected),
            pack_name=self.pack_var.get().strip() or "OAR Conversion",
            tr_submod_name=self.tr_name_var.get().strip() or "Tactical Reload",
            idle_submod_name=self.idle_name_var.get().strip() or "Idle Empty",
            idle_bones=bones or ["WeaponBolt"],
            equipped_form_id=fid,
            equipped_plugin=plugin,
            patch_esp=self.do_esp.get(),
            remove_tr_keyword=self.remove_kw.get(),
            remove_tr_master=self.remove_master.get(),
            overwrite=self.overwrite.get(),
        )

    @staticmethod
    def _root_key(root: AnimRoot) -> str:
        return f"{root.source_mod}::{root.perspective}::{root.name}"

    def _match_groups(self, opts: JobOptions, roots: list[AnimRoot]) -> list[RootWeaponGroup]:
        """Weapon-match roots for display only (Scan/queue summaries); never raises.

        Mirrors engine.build_preview's own matching so the checkbox list and queue
        summary agree with what a Run will actually produce, without needing to run a
        full preview just to find out.
        """
        if not roots:
            return []
        esp_info = None
        if opts.esp_path and opts.esp_path.is_file():
            try:
                esp_info = parse_esp(opts.esp_path)
            except Exception:  # noqa: BLE001
                esp_info = None
        try:
            subgraphs = discover_subgraphs(opts) if (esp_info or opts.subgraph_paths) else []
        except Exception:  # noqa: BLE001
            subgraphs = []
        try:
            return match_roots_to_weapons(
                roots, esp_info, subgraphs,
                include_1st=opts.include_1st, include_3rd=opts.include_3rd,
            )
        except Exception:  # noqa: BLE001
            return []

    @staticmethod
    def _build_root_labels(groups: list[RootWeaponGroup]) -> dict[str, str]:
        """root_key -> ' -> Weapon(s)' suffix, only for a real multi-weapon split.

        A single matched weapon (or none at all) is the common case already implied by
        the plain root checkbox, so it stays unlabeled to keep that case exactly as
        compact as before Scan ever ran.
        """
        matched = [g for g in groups if g.weapons]
        if len(matched) < 2:
            return {}
        labels: dict[str, str] = {}
        for g in matched:
            if len(g.weapons) == 1:
                w = g.weapons[0]
                suffix = f"  \u2192  {w.edid} ({w.form_id_hex})"
            else:
                suffix = "  \u2192  " + "+".join(w.edid for w in g.weapons)
            for r in g.roots:
                labels[OARConversionApp._root_key(r)] = suffix
        return labels

    def _scan(self) -> None:
        opts = self._job_options()
        if not opts.source_dirs and opts.esp_path:
            opts.source_dirs = [opts.esp_path.parent]
            self.source_dirs = list(opts.source_dirs)
            self._refresh_inputs_label()
        if not opts.source_dirs:
            self._log("Scan skipped: no source folder.")
            return
        self.scanned_roots = discover_roots(opts)
        groups = self._match_groups(opts, self.scanned_roots)
        self._root_weapon_labels = self._build_root_labels(groups)
        self._render_roots()
        n_matched = len([g for g in groups if g.weapons])
        extra = f"; {n_matched} distinct weapon(s) matched -> will split into {n_matched} SubMod(s) per operation" if n_matched > 1 else ""
        self._log(f"Scan found {len(self.scanned_roots)} weapon folder(s) for current job{extra}.")

    def _render_roots(self) -> None:
        for w in self.roots_frame.winfo_children():
            w.destroy()
        self.root_vars.clear()
        if not self.scanned_roots:
            ctk.CTkLabel(
                self.roots_frame,
                text="No weapon folders yet. Set a Source Folder (auto-scans) or click Rescan.",
                text_color=MUTED,
            ).pack(anchor="w", pady=2)
            return
        for root in self.scanned_roots:
            key = self._root_key(root)
            var = ctk.BooleanVar(value=True)
            self.root_vars[key] = var
            label = f"[{root.perspective}] {root.name}" + self._root_weapon_labels.get(key, "")
            ctk.CTkCheckBox(
                self.roots_frame, text=label, variable=var, fg_color=ACCENT
            ).pack(anchor="w", pady=1)

    def _set_all_roots(self, value: bool) -> None:
        for var in self.root_vars.values():
            var.set(value)

    # ----- queue -----

    def _job_summary(self, opts: JobOptions, index: int | None = None) -> str:
        src = opts.source_dirs[0].name if opts.source_dirs else "(no source)"
        dest = str(opts.destination) if opts.destination else "(no dest)"
        n_matched = len([g for g in self._match_groups(opts, opts.selected_roots) if g.weapons])
        submod_tag = f" ({n_matched}x SubMods)" if n_matched > 1 else ""
        ops = []
        if opts.do_tactical_reload:
            ops.append(f"TR:{opts.tr_submod_name}{submod_tag}")
        if opts.do_idle_empty:
            ops.append(f"Idle:{opts.idle_submod_name}{submod_tag}")
        if opts.patch_esp:
            ops.append("ESP")
        roots = ", ".join(r.name for r in opts.selected_roots) or "(no folders)"
        prefix = f"#{index + 1} " if index is not None else ""
        return (
            f"{prefix}{src} | pack={opts.pack_name} | {' '.join(ops) or 'none'} | "
            f"folders=[{roots}] | dest={dest}"
        )

    def _refresh_queue_view(self) -> None:
        for w in self.queue_frame.winfo_children():
            w.destroy()
        self.queue_select_vars = []
        self.queue_count_label.configure(text=f"({len(self.queue)})")
        if not self.queue:
            ctk.CTkLabel(
                self.queue_frame,
                text="Queue is empty. Configure a job above and click Add to Queue.",
                text_color=MUTED,
            ).pack(anchor="w", pady=4)
            return
        for i, opts in enumerate(self.queue):
            var = ctk.BooleanVar(value=False)
            self.queue_select_vars.append(var)
            ctk.CTkCheckBox(
                self.queue_frame,
                text=self._job_summary(opts, i),
                variable=var,
                fg_color=ACCENT,
            ).pack(anchor="w", pady=2)

    def _ensure_roots_for_preview(self, opts: JobOptions) -> JobOptions:
        """If the form has no selection yet, auto-scan so Preview has something to show."""
        if opts.selected_roots or not (opts.do_tactical_reload or opts.do_idle_empty):
            return opts
        if not self.scanned_roots:
            self._scan()
        if self.scanned_roots and not opts.selected_roots:
            # Re-read selection after scan (all checked by default)
            return self._job_options()
        return opts

    def _add_to_queue(self) -> None:
        opts = self._ensure_roots_for_preview(self._job_options())
        preview = build_preview(opts)
        if preview.errors:
            self._log("Cannot queue:\n  " + "\n  ".join(preview.errors))
            mb.showerror("Cannot queue", "\n".join(preview.errors))
            return
        if not preview.plans and not opts.patch_esp:
            msg = "Nothing to do. Scan and select weapon folders, or enable ESP patch."
            self._log(msg)
            mb.showerror("Cannot queue", msg)
            return
        if (opts.do_tactical_reload or opts.do_idle_empty) and not opts.selected_roots:
            msg = "Scan and select at least one weapon folder."
            self._log(msg)
            mb.showerror("Cannot queue", msg)
            return

        snapshot = copy.deepcopy(opts)
        self.queue.append(snapshot)
        self._refresh_queue_view()
        self._log(f"Queued job #{len(self.queue)}: {self._job_summary(snapshot)}")
        # keep_queued_ba2=True: the snapshot above now owns ba2_extract_dir: run_job will
        # delete/keep it when this job actually runs, so the form must not delete it now.
        self._clear_form(keep_queued_ba2=True)
        self._log("Form cleared. Configure the next folder, or Confirm & Run Queue.")

    def _remove_selected_queue(self) -> None:
        keep: list[JobOptions] = []
        for opts, var in zip(self.queue, self.queue_select_vars):
            if not var.get():
                keep.append(opts)
        removed = len(self.queue) - len(keep)
        self.queue = keep
        self._refresh_queue_view()
        if removed:
            self._log(f"Removed {removed} job(s) from queue.")

    def _clear_queue(self) -> None:
        if not self.queue:
            return
        if not mb.askyesno("Clear queue", f"Remove all {len(self.queue)} queued job(s)?"):
            return
        self.queue.clear()
        self._refresh_queue_view()
        self._log("Queue cleared.")

    def _preview_job(self, opts: JobOptions, label: str) -> None:
        preview = build_preview(opts)
        self._log(f"=== {label} ===")
        if preview.errors:
            for e in preview.errors:
                self._log(f"ERROR: {e}")
        for m in preview.messages:
            self._log(m)
        if not preview.plans and not opts.patch_esp and not preview.errors:
            self._log("(No file plans. Set destination, enable an operation, and select weapon folders.)")
        for plan in preview.plans:
            self._log(f"{plan.label}: {len(plan.files)} files -> {plan.submod_dir}")
            for fp in plan.files[:12]:
                self._log(f"  {fp.source.name} -> {fp.dest}")
            if len(plan.files) > 12:
                self._log(f"  ... and {len(plan.files) - 12} more")
        if opts.patch_esp and opts.esp_path:
            out = opts.esp_output or opts.esp_path.with_name(
                opts.esp_path.stem + "_OAR" + opts.esp_path.suffix
            )
            self._log(f"ESP patch would write: {out}")
        self._log(f"(Full preview also saved to {self.log_path})")

    def _preview_current(self) -> None:
        opts = self._ensure_roots_for_preview(self._job_options())
        self._preview_job(opts, "Preview current job")

    def _preview_queue(self) -> None:
        if not self.queue:
            mb.showinfo("Queue empty", "Add at least one job to the queue first.")
            return
        for i, opts in enumerate(self.queue):
            self._preview_job(opts, f"Preview queue #{i + 1}: {self._job_summary(opts)}")

    def _confirm_and_run_jobs(self, jobs: list[JobOptions], title: str) -> None:
        if not jobs:
            mb.showerror("Nothing to run", "No jobs to run.")
            return

        lines: list[str] = []
        any_esp = False
        for i, opts in enumerate(jobs):
            preview = build_preview(opts)
            if preview.errors:
                self._log(f"Job #{i + 1} errors: {preview.errors}")
                mb.showerror(
                    "Cannot run",
                    f"Job #{i + 1} has errors:\n" + "\n".join(preview.errors),
                )
                return
            lines.append(self._job_summary(opts, i))
            for plan in preview.plans:
                lines.append(f"  {plan.label}: {len(plan.files)} file(s) -> {plan.submod_dir}")
            if opts.patch_esp:
                any_esp = True
                lines.append(f"  Patch ESP: {opts.esp_path}")

        if not mb.askokcancel(title, "Run the following?\n\n" + "\n".join(lines)):
            return

        if any_esp and not mb.askyesno(
            "Write patched ESP?",
            "One or more jobs will write a new ESP (original not overwritten in-place).\nContinue?",
        ):
            return

        for i, opts in enumerate(jobs):
            preview = build_preview(opts)
            for plan in preview.plans:
                if plan.submod_dir.exists() and any(plan.submod_dir.iterdir()) and not opts.overwrite:
                    if not mb.askyesno(
                        "Overwrite?",
                        f"Job #{i + 1}: SubMod folder already exists:\n{plan.submod_dir}\n\nOverwrite?",
                    ):
                        return
                    opts.overwrite = True

        self._log(f"=== {title} ({len(jobs)} job(s)) ===")
        failures = 0
        for i, opts in enumerate(jobs):
            self._log(f"--- Running job #{i + 1}/{len(jobs)} ---")
            result = run_job(opts, log=self._log)
            if result.errors:
                failures += 1
                self._log(f"Job #{i + 1} finished with errors.")
            else:
                self._log(f"Job #{i + 1} completed.")

        if failures:
            mb.showerror("Finished with errors", f"{failures} of {len(jobs)} job(s) failed. See log.")
        else:
            self._log("All jobs completed successfully.")

    def _run_current(self) -> None:
        opts = self._ensure_roots_for_preview(self._job_options())
        self._confirm_and_run_jobs([opts], "Confirm & Run Current")
        # run_job may have deleted opts.ba2_extract_dir (same folder the form still
        # references); forget it here so the form does not point at a removed directory.
        if self.ba2_extract_dir is not None and not self.ba2_extract_dir.exists():
            self.ba2_path = None
            self.ba2_extract_dir = None
            self._refresh_inputs_label()

    def _run_queue(self) -> None:
        if not self.queue:
            mb.showinfo("Queue empty", "Add jobs with Add to Queue first.")
            return
        self._confirm_and_run_jobs(list(self.queue), "Confirm & Run Queue")


def main() -> None:
    app = OARConversionApp()
    app.mainloop()


if __name__ == "__main__":
    main()
