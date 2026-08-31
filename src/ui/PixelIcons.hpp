#pragma once

#include <QColor>
#include <QIcon>

namespace virgin::ui {

enum class PixelIcon {
    Back,
    Forward,
    Reload,
    Stop,
    Home,
    Bookmark,
    BookmarkFilled,
    Shield,
    Menu
};

QIcon pixelIcon(PixelIcon icon, const QColor& color = QColor("#3f4347"));

} // namespace virgin::ui
