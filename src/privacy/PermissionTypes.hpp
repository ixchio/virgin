#pragma once

#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QWebEnginePermission>
#else
#include <QWebEnginePage>
#endif

namespace virgin::privacy {

// Qt 6.8 replaced QWebEnginePage::Feature with QWebEnginePermission::PermissionType.
// Keep Virgin's stored permission model independent of that API transition so the
// AppImage can be built on the older, broadly compatible Qt 6.4 runtime.
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
using PermissionFeature = QWebEnginePermission::PermissionType;
inline constexpr PermissionFeature PermissionGeolocation = QWebEnginePermission::PermissionType::Geolocation;
inline constexpr PermissionFeature PermissionMediaAudioCapture = QWebEnginePermission::PermissionType::MediaAudioCapture;
inline constexpr PermissionFeature PermissionMediaVideoCapture = QWebEnginePermission::PermissionType::MediaVideoCapture;
inline constexpr PermissionFeature PermissionMediaAudioVideoCapture = QWebEnginePermission::PermissionType::MediaAudioVideoCapture;
#else
using PermissionFeature = QWebEnginePage::Feature;
inline constexpr PermissionFeature PermissionGeolocation = QWebEnginePage::Geolocation;
inline constexpr PermissionFeature PermissionMediaAudioCapture = QWebEnginePage::MediaAudioCapture;
inline constexpr PermissionFeature PermissionMediaVideoCapture = QWebEnginePage::MediaVideoCapture;
inline constexpr PermissionFeature PermissionMediaAudioVideoCapture = QWebEnginePage::MediaAudioVideoCapture;
#endif

} // namespace virgin::privacy
