#include "PermissionDialog.hpp"
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace virgin::ui {

PermissionDialog::PermissionDialog(const QUrl& origin,
                                   virgin::privacy::PermissionFeature feature,
                                   QWidget* parent)
    : QDialog(parent), origin_(origin), feature_(feature)
{
    setWindowTitle("Site permission");
    setModal(true);
    setFixedSize(380, 180);

    QString featureName;
    if (feature == virgin::privacy::PermissionGeolocation) featureName = "location";
    else if (feature == virgin::privacy::PermissionMediaAudioCapture) featureName = "microphone";
    else if (feature == virgin::privacy::PermissionMediaVideoCapture) featureName = "camera";
    else if (feature == virgin::privacy::PermissionMediaAudioVideoCapture) featureName = "camera & microphone";
    else featureName = "unknown permission";

    auto* layout = new QVBoxLayout(this);
    auto* label = new QLabel(QString("\"%1\" wants to use your %2").arg(origin.host(), featureName), this);
    label->setWordWrap(true);
    layout->addWidget(label);

    auto* note = new QLabel("Only allow this if you recognize and trust the site.", this);
    note->setObjectName("mutedText");
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* cb = new QCheckBox("Remember for this site", this);
    layout->addWidget(cb);
    connect(cb, &QCheckBox::toggled, [this](bool v){ remember_ = v; });

    auto* btnRow = new QHBoxLayout();
    auto* deny = new QPushButton("Deny", this);
    auto* allow = new QPushButton("Allow", this);
    allow->setProperty("accent", true);
    btnRow->addStretch();
    btnRow->addWidget(deny);
    btnRow->addWidget(allow);
    layout->addLayout(btnRow);

    connect(deny, &QPushButton::clicked, [this]{ granted_ = false; accept(); });
    connect(allow, &QPushButton::clicked, [this]{ granted_ = true; accept(); });
}

bool PermissionDialog::rememberChoice() const { return remember_; }
bool PermissionDialog::granted() const { return granted_; }

} // namespace virgin::ui
