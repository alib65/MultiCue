#include <QApplication>
#include <QIcon>

#include "DarkTheme.h"
#include "MainWindow.h"
#include "mc/core/AppInfo.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    mc::app::applyDarkTheme();
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/icon.png")));
    QApplication::setApplicationName(QString::fromUtf8(mc::core::kAppName.data(), static_cast<int>(mc::core::kAppName.size())));
    QApplication::setOrganizationName(
        QString::fromUtf8(mc::core::kAppOrganization.data(), static_cast<int>(mc::core::kAppOrganization.size())));
    QApplication::setApplicationVersion(
        QString::fromUtf8(mc::core::kAppVersion.data(), static_cast<int>(mc::core::kAppVersion.size())));

    mc::app::MainWindow window;
    window.show();
    return QApplication::exec();
}
