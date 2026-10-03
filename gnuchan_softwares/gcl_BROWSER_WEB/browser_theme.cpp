/*
 * browser_theme.cpp — the violet style sheet.
 */
#include "browser_theme.h"

QString browser_style_sheet()
{
    /* One palette, stated as names at the top so the sheet below reads as
       composition rather than as a wall of hex. */
    const QString background = QStringLiteral("#1b0f26");  // deep violet-black
    const QString surface    = QStringLiteral("#2a1740");  // raised panels
    const QString surface2   = QStringLiteral("#35204f");  // hover
    const QString accent     = QStringLiteral("#b06cff");  // GnuchanOS violet
    const QString text       = QStringLiteral("#efe6ff");  // near-white lilac
    const QString textDim    = QStringLiteral("#b9a6d6");  // secondary text
    const QString border     = QStringLiteral("#4a2f6b");  // hairlines

    return QStringLiteral(R"(
QMainWindow, QWidget { background: %1; color: %6; }

QToolBar {
    background: %2;
    border: 0px;
    border-bottom: 1px solid %7;
    spacing: 4px;
    padding: 4px;
}
QToolBar::separator { background: %7; width: 1px; margin: 4px 6px; }

QToolButton {
    background: transparent;
    color: %6;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 4px 10px;
    font-weight: 600;
}
QToolButton:hover { background: %3; border-color: %4; }
QToolButton:pressed { background: %3; }
QToolButton:disabled { color: %5; }

QLineEdit {
    background: %3;
    color: %6;
    border: 1px solid %7;
    border-radius: 8px;
    padding: 6px 10px;
    selection-background-color: %4;
    selection-color: %1;
}
QLineEdit:focus { border: 1px solid %4; }

QTabWidget::pane { border: 1px solid %7; top: -1px; }
QTabBar { background: %2; qproperty-drawBase: 0; }
QTabBar::tab {
    background: %3;
    color: %5;
    border: 1px solid %7;
    border-bottom: 0px;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
    padding: 6px 12px;
    margin-right: 2px;
    min-width: 90px;
}
QTabBar::tab:hover { background: %3; color: %6; }
QTabBar::tab:selected {
    background: %1;
    color: %6;
    border-color: %4;
    border-bottom: 2px solid %4;
}

QMenuBar { background: %2; color: %6; }
QMenuBar::item:selected { background: %4; color: %1; }
QMenu { background: %2; color: %6; border: 1px solid %7; padding: 4px; }
QMenu::item { padding: 6px 24px; border-radius: 4px; }
QMenu::item:selected { background: %4; color: %1; }

QStatusBar { background: %2; color: %5; border-top: 1px solid %7; }

QScrollBar:vertical { background: %1; width: 12px; margin: 0; }
QScrollBar::handle:vertical { background: %4; border-radius: 6px; min-height: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
QScrollBar:horizontal { background: %1; height: 12px; margin: 0; }
QScrollBar::handle:horizontal { background: %4; border-radius: 6px; min-width: 30px; }
)")
        .arg(background, surface, surface2, accent, textDim, text, border);
}
