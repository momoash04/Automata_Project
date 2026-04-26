import sys
import os
import time
import subprocess
import graphviz
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
                                QPushButton, QTextEdit, QLabel, QScrollArea,
                                QFrame, QGraphicsBlurEffect, QHBoxLayout,
                                QTableWidget, QTableWidgetItem, QHeaderView, QSizePolicy, QAbstractItemView)
from PySide6.QtCore import Qt, QThread, Signal
from PySide6.QtGui import QPixmap, QFont, QColor


# ─────────────────────────────────────────────────────────
#  SLOW-SCROLL EVENT FILTER  (1 row per wheel tick)
# ─────────────────────────────────────────────────────────
from PySide6.QtCore import QObject, QEvent

class OneRowScrollFilter(QObject):
    """Intercepts wheel events on a QTableWidget viewport and scrolls exactly
    one row at a time instead of the default 3–5 rows."""

    def __init__(self, table: QTableWidget):
        super().__init__(table)
        self._table = table

    def eventFilter(self, obj, event):
        if event.type() == QEvent.Wheel:
            sb = self._table.verticalScrollBar()
            sb.setValue(sb.value() + (-1 if event.angleDelta().y() > 0 else 1))
            return True
        return False


# ─────────────────────────────────────────────────────────
#  WORKER
# ─────────────────────────────────────────────────────────
class PDAWorker(QThread):
    finished = Signal(str, list)
    error    = Signal(str)

    def __init__(self, cfg_text):
        super().__init__()
        self.cfg_text = cfg_text

    def run(self):
        try:
            user_input = self.cfg_text.replace("\r", "").strip() + "\n\n"
            exe_path   = os.path.abspath("../P1/pda.exe")

            process = subprocess.run(
                [exe_path], input=user_input, text=True,
                capture_output=True, timeout=3
            )

            transitions = []
            for line in process.stdout.splitlines():
                line = line.strip()
                if not line.startswith("Transition:"):
                    continue
                try:
                    core   = line.replace("Transition:", "").strip()
                    left, right = core.split("->")
                    l_toks = [t.strip() for t in left.strip()[1:-1].split(",", 2)]
                    r_toks = [t.strip() for t in right.strip()[1:-1].split(",", 1)]
                    transitions.append([l_toks[0], l_toks[1], l_toks[2], r_toks[1], r_toks[0]])
                except:
                    continue

            if not transitions:
                raise ValueError("C++ Engine returned no transitions.")

            # ── Graphviz – spacious, readable layout ──────────────
            dot = graphviz.Digraph(format='png', engine='dot')
            dot.attr(
                rankdir='LR',
                bgcolor='transparent',
                ranksep='1.5',        # wide horizontal gaps between rank columns
                nodesep='1.0',        # tall vertical gaps so nodes breathe
                pad='1.2',
                dpi='180',
                splines='polylines',   # straight-ish segments – far less tangling
                overlap='false',
                concentrate='false',  # keep each edge separate, no merging
                mindist='1.0',
            )
            dot.attr('node',
                        shape='circle',
                        color='#18FFFF',
                        fontcolor='white',
                        penwidth='2.8',
                        fontsize='27',          # big enough to read easily
                        fontname='Helvetica-Bold',
                        width='2.0',
                        height='2.0',
                        fixedsize='true',       # uniform node sizes
                        style='filled',
                        fillcolor="#1C2627")    # dark teal fill, not transparent
            dot.attr('edge',
                        color='#18FFFF',        # full cyan like the nodes
                        fontcolor='#18FFFF',    # cyan labels pop against dark bg
                        fontsize='30',          # was 11 – now legible
                        fontname='Helvetica-Bold',
                        penwidth='1.8',
                        arrowsize='2.0',
                        labeldistance='1.0',    # push labels away from arrow tips
                        labelangle='25')

            # ── Merge parallel edges (same src→dst) into one arrow ──
            # This prevents the self-loop pile-up on busy nodes like q_loop
            edge_map: dict[tuple, list] = {}
            for t in transitions:
                key = (t[0], t[4])          # (from_state, to_state)
                part = f"{t[1]}, {t[2]} → {t[3]}"
                edge_map.setdefault(key, []).append(part)

            for (src, dst), labels in edge_map.items():
                combined = "\n".join(labels)
                dot.edge(src, dst, label=combined)

            img_path = f"pda_graph_{int(time.time() * 1000)}"
            dot.render(img_path, cleanup=True)

            self.finished.emit(f"{img_path}.png", transitions)

        except Exception as e:
            self.error.emit(str(e))


# ─────────────────────────────────────────────────────────
#  ZOOMABLE IMAGE WIDGET
# ─────────────────────────────────────────────────────────
class ZoomableImageLabel(QLabel):
    """QLabel with scroll-wheel zoom and click-drag pan."""

    MIN_ZOOM = 0.15
    MAX_ZOOM = 6.0

    def __init__(self):
        super().__init__()
        self._pixmap_src  = None
        self._zoom        = 1.0
        self._scroll_area = None          # set after construction
        self._pan_active  = False
        self._pan_origin  = None          # QPoint where drag started
        self._pan_hval    = 0             # scrollbar values at drag start
        self._pan_vval    = 0
        self.setAlignment(Qt.AlignCenter)
        self.setMinimumSize(200, 200)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setCursor(Qt.OpenHandCursor)

    def set_scroll_area(self, sa: QScrollArea):
        """Store a reference so we can move its scrollbars while panning."""
        self._scroll_area = sa

    def setSourcePixmap(self, pixmap: QPixmap):
        self._pixmap_src = pixmap
        self._zoom       = 0.15
        self._redraw()

    def zoom_in(self):
        self._zoom = min(self._zoom * 1.05, self.MAX_ZOOM)
        self._redraw()

    def zoom_out(self):
        self._zoom = max(self._zoom / 1.05, self.MIN_ZOOM)
        self._redraw()

    def zoom_reset(self):
        self._zoom = 0.15
        self._redraw()

    def _redraw(self):
        if not self._pixmap_src:
            return
        w = int(self._pixmap_src.width()  * self._zoom)
        h = int(self._pixmap_src.height() * self._zoom)
        scaled = self._pixmap_src.scaled(w, h, Qt.KeepAspectRatio, Qt.SmoothTransformation)
        self.setPixmap(scaled)
        self.resize(scaled.size())

    # ── Pan events ────────────────────────────────────────
    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton and self._scroll_area:
            self._pan_active = True
            self._pan_origin = event.globalPosition().toPoint()
            self._pan_hval   = self._scroll_area.horizontalScrollBar().value()
            self._pan_vval   = self._scroll_area.verticalScrollBar().value()
            self.setCursor(Qt.ClosedHandCursor)
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event):
        if self._pan_active and self._scroll_area:
            delta = event.globalPosition().toPoint() - self._pan_origin
            self._scroll_area.horizontalScrollBar().setValue(self._pan_hval - delta.x())
            self._scroll_area.verticalScrollBar().setValue(self._pan_vval   - delta.y())
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.LeftButton:
            self._pan_active = False
            self.setCursor(Qt.OpenHandCursor)
        super().mouseReleaseEvent(event)

    # ── Zoom via scroll wheel ─────────────────────────────
    def wheelEvent(self, event):
        if event.angleDelta().y() > 0:
            self.zoom_in()
        else:
            self.zoom_out()
        event.accept()


# ─────────────────────────────────────────────────────────
#  HELPER: styled section header label
# ─────────────────────────────────────────────────────────
def section_label(text: str) -> QLabel:
    lbl = QLabel(text)
    lbl.setStyleSheet("""
        color: #18FFFF;
        font-size: 11px;
        font-weight: 900;
        letter-spacing: 3px;
        border: none;
        background: transparent;
        padding-bottom: 4px;
    """)
    return lbl


def zoom_button(symbol: str) -> QPushButton:
    btn = QPushButton(symbol)
    btn.setFixedSize(34, 34)
    btn.setStyleSheet("""
        QPushButton {
            background-color: rgba(24, 255, 255, 0.10);
            color: #18FFFF;
            font-weight: 900;
            font-size: 18px;
            border-radius: 7px;
            border: 1px solid rgba(24, 255, 255, 0.25);
        }
        QPushButton:hover {
            background-color: rgba(24, 255, 255, 0.28);
            border-color: #18FFFF;
        }
        QPushButton:pressed {
            background-color: rgba(24, 255, 255, 0.45);
        }
    """)
    return btn


# ─────────────────────────────────────────────────────────
#  MAIN WINDOW
# ─────────────────────────────────────────────────────────
class TranslatioApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Translatio Engine")
        self.resize(1400, 900)
        self.setStyleSheet("background-color: #0B0C10;")

        # ── Outer scroll ──────────────────────────────────
        outer_scroll = QScrollArea()
        outer_scroll.setWidgetResizable(True)
        outer_scroll.setStyleSheet("border: none; background-color: #0B0C10;")
        self.setCentralWidget(outer_scroll)

        self.container = QWidget()
        outer_scroll.setWidget(self.container)

        self.root_layout = QVBoxLayout(self.container)
        self.root_layout.setContentsMargins(48, 40, 48, 48)
        self.root_layout.setSpacing(22)
        self.root_layout.setAlignment(Qt.AlignTop | Qt.AlignHCenter)

        # ── TITLE ─────────────────────────────────────────
        title = QLabel("Translatio Engine")
        title.setFont(QFont("Georgia", 46, QFont.Black))
        title.setStyleSheet("""
            color: white;
            font-size: 46px;
            font-weight: 900;
            letter-spacing: 3px;
            margin-top: 10px;
        """)
        self.root_layout.addWidget(title, alignment=Qt.AlignCenter)

        subtitle = QLabel("Initialize Context-Free Grammar Rules")
        subtitle.setStyleSheet("""
            font-size: 15px;
            color: #18FFFF;
            font-style: italic;
            font-weight: 800;
            letter-spacing: 2px;
        """)
        self.root_layout.addWidget(subtitle, alignment=Qt.AlignCenter)

        # ── INPUT CARD ────────────────────────────────────
        input_card = QFrame()
        input_card.setFixedWidth(640)
        input_card.setStyleSheet("""
            QFrame {
                background-color: rgba(24, 255, 255, 0.04);
                border: 1px solid rgba(24, 255, 255, 0.22);
                border-radius: 16px;
            }
        """)
        ic_layout = QVBoxLayout(input_card)
        ic_layout.setContentsMargins(20, 14, 20, 14)
        ic_layout.setSpacing(6)
        ic_layout.addWidget(section_label("CFG RULES"))

        self.input_box = QTextEdit()
        self.input_box.setPlaceholderText("S = a S b | e\n(Press Enter after each rule, blank line to finish)")
        self.input_box.setFixedHeight(140)
        self.input_box.setStyleSheet("""
            QTextEdit {
                background: transparent;
                border: none;
                color: #18FFFF;
                font-size: 19px;
                font-weight: 800;
                padding: 8px;
                letter-spacing: 0.5px;
            }
        """)
        ic_layout.addWidget(self.input_box)
        self.root_layout.addWidget(input_card, alignment=Qt.AlignCenter)

        # ── COMPILE BUTTON ────────────────────────────────
        self.btn = QPushButton("⚙   COMPILE TO PDA")
        self.btn.setFixedWidth(290)
        self.btn.setStyleSheet("""
            QPushButton {
                background-color: #18FFFF;
                color: #0B0C10;
                font-weight: 900;
                font-size: 15px;
                border-radius: 9px;
                padding: 16px;
                letter-spacing: 1.5px;
            }
            QPushButton:hover  { background-color: #00E5FF; }
            QPushButton:pressed{ background-color: #00BCD4; }
            QPushButton:disabled{ background-color: #1e2025; color: #444; }
        """)
        self.btn.clicked.connect(self.start_compilation)
        self.root_layout.addWidget(self.btn, alignment=Qt.AlignCenter)

        # ── BOTTOM ROW: Graph (left) + Table (right) ──────
        self.bottom_row = QFrame()
        self.bottom_row.hide()
        row_layout = QHBoxLayout(self.bottom_row)
        row_layout.setContentsMargins(0, 0, 0, 0)
        row_layout.setSpacing(18)
        self.root_layout.addWidget(self.bottom_row)

        # ── LEFT: Graph card ──────────────────────────────
        graph_card = QFrame()
        graph_card.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        graph_card.setStyleSheet("""
            QFrame {
                background-color: rgba(255, 255, 255, 0.025);
                border-radius: 16px;
                border: 1px solid rgba(24, 255, 255, 0.15);
            }
        """)
        gc_layout = QVBoxLayout(graph_card)
        gc_layout.setContentsMargins(18, 16, 18, 16)
        gc_layout.setSpacing(10)

        # Header row: label + zoom controls
        gh_row = QHBoxLayout()
        gh_row.addWidget(section_label("PDA  DIAGRAM"))
        gh_row.addStretch()

        self._btn_zoom_out   = zoom_button("−")
        self._btn_zoom_reset = zoom_button("⊙")
        self._btn_zoom_in    = zoom_button("+")

        for b in (self._btn_zoom_out, self._btn_zoom_reset, self._btn_zoom_in):
            gh_row.addWidget(b)
        gc_layout.addLayout(gh_row)

        # Scroll area wrapping the zoomable label
        self._graph_scroll = QScrollArea()
        self._graph_scroll.setWidgetResizable(False)   # we control size manually
        self._graph_scroll.setAlignment(Qt.AlignCenter)
        self._graph_scroll.setStyleSheet("""
            QScrollArea { border: none; background: transparent; }
            QScrollBar:horizontal, QScrollBar:vertical {
                background: rgba(255,255,255,0.05);
                border-radius: 4px; width: 8px; height: 8px;
            }
            QScrollBar::handle:horizontal, QScrollBar::handle:vertical {
                background: rgba(24,255,255,0.35);
                border-radius: 4px;
            }
        """)
        self._graph_scroll.setMinimumHeight(430)

        self.graph_img = ZoomableImageLabel()
        self.graph_img.set_scroll_area(self._graph_scroll)   # enable pan
        self._graph_scroll.setWidget(self.graph_img)
        gc_layout.addWidget(self._graph_scroll)

        hint = QLabel("Scroll to zoom  ·  click & drag to pan")
        hint.setAlignment(Qt.AlignCenter)
        hint.setStyleSheet("""
            color: rgba(255,255,255,0.28);
            font-size: 11px;
            font-weight: 700;
            letter-spacing: 0.5px;
            border: none;
            background: transparent;
        """)
        gc_layout.addWidget(hint)

        # Wire zoom buttons
        self._btn_zoom_in.clicked.connect(self.graph_img.zoom_in)
        self._btn_zoom_out.clicked.connect(self.graph_img.zoom_out)
        self._btn_zoom_reset.clicked.connect(self.graph_img.zoom_reset)

        row_layout.addWidget(graph_card, stretch=3)

        # ── RIGHT: Transition Table card ──────────────────
        table_card = QFrame()
        table_card.setFixedWidth(430)
        table_card.setStyleSheet("""
            QFrame {
                background-color: rgba(255, 255, 255, 0.025);
                border-radius: 16px;
                border: 1px solid rgba(24, 255, 255, 0.15);
            }
        """)
        tc_layout = QVBoxLayout(table_card)
        tc_layout.setContentsMargins(18, 16, 18, 16)
        tc_layout.setSpacing(10)
        tc_layout.addWidget(section_label("TRANSITION TABLE"))

        self.table = QTableWidget()
        self.table.setColumnCount(5)
        self.table.setHorizontalHeaderLabels(["State", "Read", "Pop", "Push", "Next"])
        self.table.verticalHeader().setVisible(False)
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.table.setSelectionMode(QAbstractItemView.NoSelection)
        self.table.setFocusPolicy(Qt.NoFocus)
        self.table.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.table.setStyleSheet("""
            QTableWidget {
                background-color: transparent;
                border: none;
                color: white;
                font-weight: 800;
                font-size: 13px;
                gridline-color: rgba(24, 255, 255, 0.07);
                selection-background-color: rgba(24,255,255,0.18);
            }
            QHeaderView::section {
                background-color: rgba(24, 255, 255, 0.10);
                color: #18FFFF;
                font-weight: 900;
                font-size: 12px;
                letter-spacing: 1.5px;
                border: none;
                padding: 10px 4px;
            }
            QTableWidget::item {
                padding: 7px 4px;
                border-bottom: 1px solid rgba(24, 255, 255, 0.06);
            }
            QScrollBar:vertical {
                background: rgba(255,255,255,0.05);
                border-radius: 4px;
                width: 8px;
            }
            QScrollBar::handle:vertical {
                background: rgba(24,255,255,0.35);
                border-radius: 4px;
            }
        """)
        tc_layout.addWidget(self.table)
        self.table.viewport().installEventFilter(OneRowScrollFilter(self.table))
        row_layout.addWidget(table_card, stretch=2)

        # ── LOADER OVERLAY ────────────────────────────────
        self.overlay = QFrame(self)
        self.overlay.setStyleSheet("background-color: rgba(11,12,16,215);")
        self.overlay.hide()

        ov_layout = QVBoxLayout(self.overlay)
        ov_layout.setAlignment(Qt.AlignCenter)
        ov_layout.setSpacing(10)

        ov_title = QLabel("⚙  COMPILING ARCHITECTURE")
        ov_title.setStyleSheet("""
            color: #18FFFF;
            font-size: 24px;
            font-weight: 900;
            letter-spacing: 3px;
        """)
        ov_layout.addWidget(ov_title, alignment=Qt.AlignCenter)

        ov_sub = QLabel("Converting CFG  →  Pushdown Automaton")
        ov_sub.setStyleSheet("""
            color: rgba(255,255,255,0.45);
            font-size: 14px;
            font-weight: 700;
            letter-spacing: 1px;
        """)
        ov_layout.addWidget(ov_sub, alignment=Qt.AlignCenter)

    # ── Events ────────────────────────────────────────────
    def resizeEvent(self, event):
        self.overlay.resize(event.size())
        super().resizeEvent(event)

    # ── Compilation flow ──────────────────────────────────
    def start_compilation(self):
        blur = QGraphicsBlurEffect()
        blur.setBlurRadius(26)
        self.container.setGraphicsEffect(blur)
        self.overlay.show()
        self.btn.setEnabled(False)

        self.worker = PDAWorker(self.input_box.toPlainText())
        self.worker.finished.connect(self.on_success)
        self.worker.error.connect(self.on_fail)
        self.worker.start()

    def on_success(self, img_path, transitions):
        self.container.setGraphicsEffect(None)
        self.overlay.hide()
        self.btn.setEnabled(True)

        # Graph
        pixmap = QPixmap(img_path)
        self.graph_img.setSourcePixmap(pixmap)

        # Table
        bold = QFont("Helvetica", 12, QFont.Bold)
        self.table.setRowCount(len(transitions))
        for row, data in enumerate(transitions):
            for col, value in enumerate(data):
                item = QTableWidgetItem(value)
                item.setTextAlignment(Qt.AlignCenter)
                item.setFont(bold)
                # Alternate row tinting
                if row % 2 == 0:
                    item.setBackground(QColor(24, 255, 255, 10))
                self.table.setItem(row, col, item)
        self.table.setFixedHeight(min(520, len(transitions) * 40 + 56))

        self.bottom_row.show()

    def on_fail(self, err):
        self.container.setGraphicsEffect(None)
        self.overlay.hide()
        self.btn.setEnabled(True)
        print(f"Engine Error: {err}")


# ─────────────────────────────────────────────────────────
if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = TranslatioApp()
    window.show()
    sys.exit(app.exec())