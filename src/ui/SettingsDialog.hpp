#pragma once
#include <QDialog>
class QComboBox;
class QCheckBox;
class QLineEdit;
class QLabel;

namespace virgin::storage { class SettingsStore; }

namespace virgin::ui {
class SettingsDialog final : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(virgin::storage::SettingsStore* settings, QWidget* parent = nullptr);
private:
    virgin::storage::SettingsStore* settings_ = nullptr;
    QComboBox* historyCombo_ = nullptr;
    QComboBox* cookieCombo_ = nullptr;
    QComboBox* searchCombo_ = nullptr;
    QCheckBox* strictBox_ = nullptr;
    QCheckBox* httpsFirstBox_ = nullptr;
    QCheckBox* adblockBox_ = nullptr;
    QCheckBox* webrtcBox_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QLabel* searchCustomLabel_ = nullptr;
};
} // namespace virgin::ui
