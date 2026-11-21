# ViviaMeals Mobil – kostenlose Android-App (Qt Quick), nutzt denselben Kern wie das Desktop-Programm
#
# Bauen: in Qt Creator öffnen, Kit „Android Qt 6.12.x Clang arm64-v8a“ wählen (Anleitung: docs/ViviaMeals-Android-Anleitung.pdf).
# Zum Ausprobieren der Oberfläche geht auch der Desktop-Kit (Fenster in Telefon- oder Tablet-Größe, siehe main.cpp).
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)

TEMPLATE = app
TARGET   = ViviaMeals
QT      += core gui quick quickcontrols2 network svg
CONFIG  += c++17

include(../version.pri)
CONFIG  += vm_mobile   # ohne Registrierungshinweis aus core.pri
include(../core.pri)

# Versionsnummer wie beim Desktop-Programm als erzeugte Kopfdatei
VM_VERSION_H = "$${LITERAL_HASH}pragma once" "$${LITERAL_HASH}define VIVIAMEALS_VERSION \"$$VERSION\""
VM_VERSION_CUR = $$cat($$OUT_PWD/viviameals_version.h, lines)
!equals(VM_VERSION_CUR, $$VM_VERSION_H): write_file($$OUT_PWD/viviameals_version.h, VM_VERSION_H)
INCLUDEPATH += $$OUT_PWD $$PWD/src

SOURCES += \
    src/main.cpp \
    src/mobileapp.cpp \
    src/recipelistmodel.cpp \
    src/recipeimageprovider.cpp \
    src/platform.cpp

HEADERS += \
    src/mobileapp.h \
    src/recipelistmodel.h \
    src/recipeimageprovider.h \
    src/platform.h

RESOURCES += qml.qrc mobiledata.qrc

android {
    # nur 64-Bit-ARM (qmake baut unter Qt 6 eine Prozessorart je Kit)
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
    ANDROID_ABIS               = arm64-v8a
    ANDROID_MIN_SDK_VERSION    = 28
    ANDROID_TARGET_SDK_VERSION = 36
    ANDROID_VERSION_NAME       = $$VERSION
    # Versionscode = Major*10000 + Minor*100 + Patch (1.0.1 → 10001); muss bei jeder Play-Fassung steigen
    VM_VPARTS = $$split(VERSION, .)
    ANDROID_VERSION_CODE = $$num_add($$member(VM_VPARTS, 0)0000, $$member(VM_VPARTS, 1)00, $$member(VM_VPARTS, 2))
    # OpenSSL für HTTPS (Abgleich mit der eigenen Website): Qt Creator lädt android_openssl ins SDK
    VM_SSL = $$(ANDROID_SDK_ROOT)/android_openssl/openssl.pri
    !exists($$VM_SSL): VM_SSL = C:/Android/Sdk/android_openssl/openssl.pri
    exists($$VM_SSL): include($$VM_SSL)
    else: message("android_openssl nicht gefunden – HTTPS-Abgleich braucht OpenSSL (Qt Creator: Einstellungen → SDKs → Android → OpenSSL herunterladen)")

    DISTFILES += android/AndroidManifest.xml
}
