// ViviaMeals Mobil – Plattform-Hilfen (siehe platform.h)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "platform.h"

#include <QClipboard>
#include <QGuiApplication>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

namespace Platform {

bool isAndroid()
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

void setKeepScreenOn(bool on)
{
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([on]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) return;
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        if (!window.isValid()) return;
        const jint flagKeepScreenOn = 128;   // WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        if (on)
            window.callMethod<void>("addFlags", "(I)V", flagKeepScreenOn);
        else
            window.callMethod<void>("clearFlags", "(I)V", flagKeepScreenOn);
    });
#else
    Q_UNUSED(on);
#endif
}

bool shareText(const QString &title, const QString &text)
{
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([title, text]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) return;
        QJniObject intent("android/content/Intent");
        QJniObject action = QJniObject::getStaticObjectField("android/content/Intent", "ACTION_SEND", "Ljava/lang/String;");
        QJniObject extra = QJniObject::getStaticObjectField("android/content/Intent", "EXTRA_TEXT", "Ljava/lang/String;");
        QJniObject subject = QJniObject::getStaticObjectField("android/content/Intent", "EXTRA_SUBJECT", "Ljava/lang/String;");
        intent.callObjectMethod("setAction", "(Ljava/lang/String;)Landroid/content/Intent;", action.object<jstring>());
        intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;", QJniObject::fromString(QStringLiteral("text/plain")).object<jstring>());
        intent.callObjectMethod("putExtra", "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;", extra.object<jstring>(),
                                QJniObject::fromString(text).object<jstring>());
        intent.callObjectMethod("putExtra", "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;", subject.object<jstring>(),
                                QJniObject::fromString(title).object<jstring>());
        QJniObject chooser = QJniObject::callStaticObjectMethod("android/content/Intent", "createChooser",
                                                                "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;",
                                                                intent.object(), QJniObject::fromString(title).object<jstring>());
        if (chooser.isValid()) activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", chooser.object());
    });
    return true;
#else
    Q_UNUSED(title);
    QGuiApplication::clipboard()->setText(text);
    return false;
#endif
}

} // namespace Platform
