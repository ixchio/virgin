#include "PixelIcons.hpp"

#include <QPainter>
#include <QPixmap>
#include <QPolygon>

namespace virgin::ui {

QIcon pixelIcon(PixelIcon icon, const QColor& color) {
    QPixmap pixels(18, 18);
    pixels.fill(Qt::transparent);

    QPainter painter(&pixels);
    painter.setRenderHint(QPainter::Antialiasing, false);
    QPen pen(color, 2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (icon) {
    case PixelIcon::Back:
        painter.drawLine(13, 3, 6, 9);
        painter.drawLine(6, 9, 13, 15);
        painter.drawLine(6, 9, 16, 9);
        break;
    case PixelIcon::Forward:
        painter.drawLine(5, 3, 12, 9);
        painter.drawLine(12, 9, 5, 15);
        painter.drawLine(2, 9, 12, 9);
        break;
    case PixelIcon::Reload:
        painter.drawLine(14, 7, 14, 3);
        painter.drawLine(14, 3, 10, 3);
        painter.drawLine(14, 4, 16, 6);
        painter.drawLine(4, 7, 4, 12);
        painter.drawLine(4, 12, 8, 15);
        painter.drawLine(8, 15, 13, 13);
        painter.drawLine(4, 7, 8, 3);
        painter.drawLine(8, 3, 11, 3);
        break;
    case PixelIcon::Stop:
        painter.fillRect(4, 4, 10, 10, color);
        break;
    case PixelIcon::Home:
        painter.drawLine(3, 9, 9, 3);
        painter.drawLine(9, 3, 15, 9);
        painter.drawRect(5, 9, 8, 6);
        painter.drawLine(9, 12, 9, 15);
        break;
    case PixelIcon::Bookmark:
    case PixelIcon::BookmarkFilled: {
        const QPolygon mark({QPoint(5, 3), QPoint(13, 3), QPoint(13, 15),
                             QPoint(9, 12), QPoint(5, 15)});
        if (icon == PixelIcon::BookmarkFilled) painter.setBrush(color);
        painter.drawPolygon(mark);
        break;
    }
    case PixelIcon::Shield: {
        const QPolygon shield({QPoint(9, 2), QPoint(15, 5), QPoint(14, 11),
                               QPoint(9, 16), QPoint(4, 11), QPoint(3, 5)});
        painter.drawPolygon(shield);
        painter.drawLine(9, 5, 9, 12);
        painter.drawPoint(9, 14);
        break;
    }
    case PixelIcon::Menu:
        painter.drawLine(3, 5, 15, 5);
        painter.drawLine(3, 9, 15, 9);
        painter.drawLine(3, 13, 15, 13);
        break;
    }

    return QIcon(pixels);
}

} // namespace virgin::ui
