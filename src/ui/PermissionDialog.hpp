#pragma once

#include <QDialog>
#include <QUrl>

#include "privacy/PermissionTypes.hpp"

namespace virgin::ui {

class PermissionDialog final : public QDialog {
    Q_OBJECT
public:
    explicit PermissionDialog(const QUrl& origin,
                              virgin::privacy::PermissionFeature feature,
                              QWidget* parent = nullptr);

    bool rememberChoice() const;
    bool granted() const;

private:
    QUrl origin_;
    virgin::privacy::PermissionFeature feature_;
    bool granted_ = false;
    bool remember_ = false;
};

} // namespace virgin::ui
