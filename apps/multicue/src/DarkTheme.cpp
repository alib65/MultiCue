#include "DarkTheme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace mc::app {

void applyDarkTheme() {
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    const QColor windowColor(45, 45, 48);
    const QColor baseColor(30, 30, 30);
    const QColor textColor(220, 220, 220);
    const QColor highlightColor(60, 130, 200);
    const QColor disabledColor(110, 110, 110);

    palette.setColor(QPalette::Window, windowColor);
    palette.setColor(QPalette::WindowText, textColor);
    palette.setColor(QPalette::Base, baseColor);
    palette.setColor(QPalette::AlternateBase, windowColor);
    palette.setColor(QPalette::ToolTipBase, textColor);
    palette.setColor(QPalette::ToolTipText, textColor);
    palette.setColor(QPalette::Text, textColor);
    palette.setColor(QPalette::Button, windowColor);
    palette.setColor(QPalette::ButtonText, textColor);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, highlightColor);
    palette.setColor(QPalette::Highlight, highlightColor);
    palette.setColor(QPalette::HighlightedText, Qt::black);

    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledColor);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);

    QApplication::setPalette(palette);
}

} // namespace mc::app
