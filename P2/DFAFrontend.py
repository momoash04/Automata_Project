import sys
import math
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QLineEdit, QLabel)
from PySide6.QtCore import Qt, QTimer, QPointF, QRectF
from PySide6.QtGui import QPainter, QPen, QBrush, QColor, QFont, QPainterPath


def get_state_name(state):
    names = {
        0: "Start\n0 Ones",
        1: "0 Ones\nEnds in 0\n(Reject)",
        2: "1 Ones\nEnds in 1\n(Reject)",
        3: "1 Ones\nEnds in 0\n(Reject)",
        4: "2 Ones\nEnds in 1\n(Reject)",
        5: "2 Ones\nEnds in 0\n(Reject)",
        6: "divby3 Ones\nEnds in 1\n(Reject)",
        7: "divby3 Ones\nEnds in 0\n(ACCEPT)",
    }
    return names.get(state, "Unknown")


import ctypes
import os
import subprocess


def load_backend():
    # Try to load the backend DLL
    dll_name = "DFABackend.dll" if os.name == 'nt' else "libDFABackend.so"
    dll_path = os.path.join(os.path.dirname(__file__), dll_name)

    if not os.path.exists(dll_path):
        print(f"Backend library {dll_name} not found. Attempting to compile...")
        cpp_path = os.path.join(os.path.dirname(__file__), "DFABackend.cpp")
        try:
            cmd = ["g++", "-shared", "-o", dll_path, cpp_path, "-fPIC"]
            subprocess.run(cmd, check=True)
            print("Compilation successful.")
        except Exception as e:
            print(
                f"Compilation failed: {e}\nPlease compile manually: g++ -shared -o DFABackend.dll DFABackend.cpp -fPIC")
            return None

    try:
        lib = ctypes.CDLL(dll_path)
        lib.c_checkDFA.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_bool), ctypes.POINTER(ctypes.c_int),
                                   ctypes.POINTER(ctypes.c_bool)]
        lib.c_checkDFA.restype = None
        return lib
    except Exception as e:
        print(f"Failed to load backend library: {e}")
        return None


backend_lib = load_backend()


def check_dfa(input_str):
    if backend_lib is None:
        return {"accepted": False, "final_state": -1, "error": True}

    c_input = input_str.encode('utf-8') if input_str else b""
    accepted = ctypes.c_bool()
    final_state = ctypes.c_int()
    error = ctypes.c_bool()

    backend_lib.c_checkDFA(c_input, ctypes.byref(accepted), ctypes.byref(final_state), ctypes.byref(error))

    return {
        "accepted": accepted.value,
        "final_state": final_state.value,
        "error": error.value
    }


class DFACanvas(QWidget):
    def __init__(self):
        super().__init__()
        self.current_state_index = 0
        self.is_accepted = False
        self.is_error = False
        self.anim_frame = 0
        self.last_transition_from = -1
        self.last_transition_to = -1
        self.last_transition_label = ''
        self.transition_highlight_timer = 0

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.on_timer)
        self.timer.start(100)

    def on_timer(self):
        redraw = False
        if self.is_accepted and not self.is_error and self.current_state_index == 7:
            self.anim_frame = (self.anim_frame + 1) % 10
            redraw = True
        if self.transition_highlight_timer > 0:
            self.transition_highlight_timer -= 1
            if self.transition_highlight_timer == 0:
                self.last_transition_from = -1
                self.last_transition_to = -1
            redraw = True

        if redraw:
            self.update()

    def update_state(self, current_state, is_accepted, is_error, last_from, last_to, last_label):
        self.current_state_index = current_state
        self.is_accepted = is_accepted
        self.is_error = is_error
        self.last_transition_from = last_from
        self.last_transition_to = last_to
        self.last_transition_label = last_label
        if last_from != -1:
            self.transition_highlight_timer = 10
        else:
            self.transition_highlight_timer = 0
        self.anim_frame = 0
        self.update()

    def draw_arrow(self, painter, tip_x, tip_y, angle, is_highlighted):
        L = 14
        phi = 0.5
        p1 = QPointF(tip_x, tip_y)
        p2 = QPointF(tip_x - L * math.cos(angle - phi), tip_y - L * math.sin(angle - phi))
        p3 = QPointF(tip_x - L * math.cos(angle + phi), tip_y - L * math.sin(angle + phi))

        color = QColor(255, 165, 0) if is_highlighted else QColor(160, 160, 160)
        painter.setBrush(QBrush(color))
        painter.setPen(QPen(color, 1))

        path = QPainterPath()
        path.moveTo(p1)
        path.lineTo(p2)
        path.lineTo(p3)
        path.closeSubpath()
        painter.drawPath(path)

    def draw_edge_line(self, painter, centers, _from, _to, label, curve_offset):
        is_highlighted = (self.transition_highlight_timer > 0 and
                          self.last_transition_from == _from and
                          self.last_transition_to == _to and
                          self.last_transition_label == label)

        pen = QPen(QColor(255, 165, 0) if is_highlighted else QColor(160, 160, 160),
                   4 if is_highlighted else 2)
        painter.setPen(pen)

        c_from = centers[_from]
        c_to = centers[_to]

        if _from == _to:
            # Self loop
            rect = QRectF(c_from.x() - 20, c_from.y() - 60, 40, 40)
            painter.setBrush(Qt.NoBrush)
            painter.drawArc(rect, 0, 360 * 16)

            bx = c_from.x()
            by = c_from.y() - 60

            painter.setPen(Qt.NoPen)
            painter.setBrush(QBrush(QColor("#1e1e1e")))
            painter.drawEllipse(int(bx - 12), int(by - 12), 24, 24)

            painter.setPen(QColor(255, 165, 0) if is_highlighted else QColor(255, 255, 255))
            painter.drawText(QRectF(bx - 12, by - 12, 24, 24), Qt.AlignCenter, label)

            tip_x = c_from.x() + 12
            tip_y = c_from.y() - 48
            angle = 1.7
            self.draw_arrow(painter, tip_x, tip_y, angle, is_highlighted)
            return

        x1, y1 = c_from.x(), c_from.y()
        x2, y2 = c_to.x(), c_to.y()

        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        dx, dy = x2 - x1, y2 - y1

        cx = mx - dy * curve_offset / 100
        cy = my + dx * curve_offset / 100

        path = QPainterPath()
        path.moveTo(x1, y1)
        path.quadTo(cx, cy, x2, y2)
        painter.setBrush(Qt.NoBrush)
        painter.drawPath(path)

        bx = 0.25 * x1 + 0.5 * cx + 0.25 * x2
        by = 0.25 * y1 + 0.5 * cy + 0.25 * y2

        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(QColor("#1e1e1e")))
        painter.drawEllipse(int(bx - 12), int(by - 12), 24, 24)

        painter.setPen(QColor(255, 165, 0) if is_highlighted else QColor(255, 255, 255))
        painter.drawText(QRectF(bx - 12, by - 12, 24, 24), Qt.AlignCenter, label)

        angle = math.atan2(y2 - cy, x2 - cx)
        radius = 50
        tip_x = x2 - radius * math.cos(angle)
        tip_y = y2 - radius * math.sin(angle)
        self.draw_arrow(painter, tip_x, tip_y, angle, is_highlighted)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        rect = self.rect()
        w, h = rect.width(), rect.height()

        c_w, c_h = 100, 100
        available_height = h - 20
        start_x = w / 10
        start_y = 60 + (available_height - 320) / 2
        spacing_x = (w - (start_x * 2)) / 3
        spacing_y = 220

        centers = []
        for i in range(8):
            row = i // 4
            col = i % 4
            centers.append(QPointF(start_x + col * spacing_x, start_y + row * spacing_y))

        # Draw edges
        edges = [
            (0, 1, "0", 20), (0, 2, "1", -30),
            (1, 1, "0", 0), (1, 2, "1", 20),
            (2, 3, "0", 20), (2, 4, "1", -20),
            (3, 3, "0", 0), (3, 4, "1", 20),
            (4, 5, "0", 20), (4, 6, "1", -30),
            (5, 5, "0", 0), (5, 6, "1", 20),
            (6, 7, "0", 20), (6, 2, "1", -15),
            (7, 7, "0", 0), (7, 2, "1", 25)
        ]

        font = QFont("Segoe UI", 12, QFont.Bold)
        painter.setFont(font)

        for edge in edges:
            self.draw_edge_line(painter, centers, edge[0], edge[1], edge[2], edge[3])

        font_circle = QFont("Segoe UI", 11, QFont.Bold)
        painter.setFont(font_circle)

        for i in range(8):
            c_x = centers[i].x() - c_w / 2
            c_y = centers[i].y() - c_h / 2

            is_anim = False
            if i == 7:
                state_color = QColor(40, 150, 40)
                if self.current_state_index == 7 and self.is_accepted and not self.is_error:
                    if self.anim_frame % 2 == 0:
                        state_color = QColor(80, 255, 80)
                        is_anim = True
            elif i == self.current_state_index and not self.is_error:
                state_color = QColor(220, 200, 0)
            else:
                state_color = QColor(150, 40, 40)

            text_color = QColor(0, 0, 0) if (
                        i == self.current_state_index and not self.is_error and i != 7) else QColor(255, 255, 255)

            draw_x = c_x - 5 if is_anim else c_x
            draw_y = c_y - 5 if is_anim else c_y
            draw_w = c_w + 10 if is_anim else c_w
            draw_h = c_h + 10 if is_anim else c_h

            painter.setBrush(QBrush(state_color))
            painter.setPen(QPen(QColor(200, 200, 200), 2))
            painter.drawEllipse(int(draw_x), int(draw_y), int(draw_w), int(draw_h))

            painter.setPen(text_color)
            name = get_state_name(i)
            text_rect = QRectF(draw_x + 5, draw_y, draw_w - 10, draw_h)
            painter.drawText(text_rect, Qt.AlignCenter | Qt.TextWordWrap, name)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("DFA Real-Time Validator")
        self.resize(1000, 800)
        self.setStyleSheet("background-color: #1e1e1e; color: #dcdcdc;")

        main_widget = QWidget()
        self.setCentralWidget(main_widget)

        layout = QVBoxLayout(main_widget)
        layout.setContentsMargins(20, 20, 20, 20)

        title = QLabel("DFA Real-Time Validator")
        title.setFont(QFont("Segoe UI", 28, QFont.Bold))
        title.setAlignment(Qt.AlignCenter)
        title.setStyleSheet("color: #64c8ff;")
        layout.addWidget(title)

        input_layout = QHBoxLayout()
        input_layout.setAlignment(Qt.AlignCenter)

        label = QLabel("Input Binary String:")
        label.setFont(QFont("Segoe UI", 18))
        input_layout.addWidget(label)

        self.input_edit = QLineEdit()
        self.input_edit.setFont(QFont("Segoe UI", 18))
        self.input_edit.setStyleSheet(
            "background-color: #2d2d2d; color: #dcdcdc; border: 1px solid #555; padding: 4px;")
        self.input_edit.setFixedWidth(480)
        self.input_edit.setMaxLength(255)
        self.input_edit.textChanged.connect(self.on_text_changed)
        input_layout.addWidget(self.input_edit)

        layout.addLayout(input_layout)

        self.canvas = DFACanvas()
        layout.addWidget(self.canvas, 1)

        self.result_label = QLabel("Result: Waiting for input...")
        self.result_label.setFont(QFont("Segoe UI", 24, QFont.Bold))
        self.result_label.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.result_label)

    def on_text_changed(self, text):
        if not text:
            self.canvas.update_state(0, False, False, -1, -1, '')
            self.result_label.setText("Result: Waiting for input...")
            self.result_label.setStyleSheet("color: #969696;")
            return

        invalid = any(c not in '01' for c in text)
        if invalid:
            self.canvas.update_state(-1, False, True, -1, -1, '')
            self.result_label.setText("Result: ERROR (Invalid Char)")
            self.result_label.setStyleSheet("color: #ff4500;")
            return

        prev_text = text[:-1]
        prev_res = check_dfa(prev_text)
        res = check_dfa(text)

        last_from = prev_res["final_state"]
        last_to = res["final_state"]
        last_label = text[-1]

        self.canvas.update_state(res["final_state"], res["accepted"], False, last_from, last_to, last_label)

        if res["accepted"]:
            self.result_label.setText("Result: ACCEPTED \u2714")
            self.result_label.setStyleSheet("color: #32cd32;")
        else:
            self.result_label.setText("Result: REJECTED \u2718")
            self.result_label.setStyleSheet("color: #ff4500;")


if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = MainWindow()
    window.showMaximized()
    sys.exit(app.exec())
