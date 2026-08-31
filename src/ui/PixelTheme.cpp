#include "PixelTheme.hpp"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>

namespace virgin::ui {

void applyPixelTheme(QApplication& app) {
    app.setStyle("Fusion");

    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPointSize(9);
    font.setStyleHint(QFont::SansSerif);
    app.setFont(font);

    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#eeeeee"));
    palette.setColor(QPalette::WindowText, QColor("#202124"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#f5f5f5"));
    palette.setColor(QPalette::Text, QColor("#202124"));
    palette.setColor(QPalette::Button, QColor("#f3f3f3"));
    palette.setColor(QPalette::ButtonText, QColor("#202124"));
    palette.setColor(QPalette::Highlight, QColor("#4d78a8"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, QColor("#7a7a7a"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#a0a0a0"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#a0a0a0"));
    app.setPalette(palette);

    app.setStyleSheet(R"QSS(
* { outline: none; }
QMainWindow, QDialog { background: #eeeeee; color: #202124; }
QWidget { color: #202124; }
QMenuBar { background: #e7e7e7; border-bottom: 1px solid #c7c7c7; spacing: 2px; }
QMenuBar::item { padding: 4px 8px; background: transparent; }
QMenuBar::item:selected, QMenu::item:selected { background: #4d78a8; color: #ffffff; }
QMenu { background: #ffffff; border: 1px solid #9f9f9f; padding: 3px; }
QMenu::item { padding: 5px 24px 5px 8px; }
QMenu::separator { height: 1px; background: #d5d5d5; margin: 4px 6px; }
QToolBar { background: #e6e6e6; border: 0; border-bottom: 1px solid #bcbcbc; padding: 4px 5px; spacing: 2px; }
QToolBar QPushButton {
  background: transparent; border: 1px solid transparent; border-radius: 2px; padding: 4px;
}
QToolBar QPushButton:hover { background: #f7f7f7; border-color: #b5b5b5; }
QToolBar QPushButton:pressed, QToolBar QPushButton:checked { background: #d2d2d2; border-color: #9c9c9c; }
QToolBar QPushButton:disabled { background: transparent; border-color: transparent; }
QPushButton, QToolButton {
  background: #f3f3f3; color: #202124; border: 1px solid #a9a9a9;
  border-radius: 2px; padding: 5px 10px; min-height: 18px;
}
QPushButton:hover, QToolButton:hover { background: #fafafa; border-color: #858585; }
QPushButton:pressed, QToolButton:pressed, QPushButton:checked { background: #dcdcdc; }
QPushButton:disabled, QToolButton:disabled { color: #a0a0a0; border-color: #cecece; }
QPushButton[danger="true"] { color: #8f2020; border-color: #b97878; }
QPushButton[accent="true"] { background: #4d78a8; color: #ffffff; border-color: #315d8b; }
QPushButton[accent="true"]:hover { background: #426d9d; }
QLineEdit, QComboBox, QSpinBox, QListWidget, QTreeView, QTextEdit {
  background: #ffffff; color: #202124; border: 1px solid #a9a9a9;
  border-radius: 2px; padding: 5px; selection-background-color: #4d78a8;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QListWidget:focus { border-color: #4d78a8; }
QLineEdit[invalid="true"] { background: #fff6f6; border-color: #b84a4a; color: #641c1c; }
QTabWidget::pane { background: #ffffff; border: 0; border-top: 1px solid #a9a9a9; }
QTabBar { background: #d8d8d8; }
QTabBar::tab { background: #d8d8d8; border: 1px solid #aaaaaa; border-bottom: 0; padding: 6px 14px; margin: 3px 1px 0 0; min-width: 105px; max-width: 220px; }
QTabBar::tab:selected { background: #ffffff; color: #111111; border-color: #999999; }
QTabBar::tab:hover:!selected { background: #e8e8e8; }
QTabBar::close-button { subcontrol-position: right; }
QGroupBox { border: 1px solid #bcbcbc; border-radius: 2px; margin-top: 12px; padding-top: 12px; font-weight: 600; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; background: #eeeeee; }
QCheckBox { spacing: 7px; }
QProgressBar { background: #d7d7d7; border: 1px solid #a9a9a9; height: 6px; }
QProgressBar::chunk { background: #4d78a8; }
QStatusBar { background: #eeeeee; color: #5f6368; border-top: 1px solid #c7c7c7; }
QLabel#privateBanner { background: #e5e2e8; color: #3f3747; border-bottom: 1px solid #aaa4af; padding: 5px; font-weight: 600; }
QWidget#privacyPanel { background: #ffffff; border: 1px solid #8c8c8c; }
QLabel#panelTitle { color: #151515; font-size: 11pt; font-weight: 600; }
QLabel#protectionGood { color: #2e6337; font-weight: 600; }
QLabel#statBlock { background: #f5f5f5; border: 1px solid #d0d0d0; padding: 8px; }
QLabel#warningBlock { background: #fff2f2; color: #7b1f1f; border: 1px solid #c68484; padding: 8px; }
QLabel#mutedText { color: #666666; }
QToolTip { background: #ffffdc; color: #202124; border: 1px solid #8c8c8c; padding: 3px; }
)QSS");
}

} // namespace virgin::ui
