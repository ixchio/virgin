#pragma once

#include <QDialog>
#include <QUrl>
#include <QWebEnginePermission>

namespace virgin::ui {

class PermissionDialog final : public QDialog {
    Q_OBJECT
public:
    explicit PermissionDialog(const QUrl& origin,
                              QWebEnginePermission::PermissionType feature,
                              QWidget* parent = nullptr);

    bool rememberChoice() const;
    bool granted() const;

private:
    QUrl origin_;
    QWebEnginePermission::PermissionType feature_;
    bool granted_ = false;
    bool remember_ = false;
};

} // namespace virgin::ui
