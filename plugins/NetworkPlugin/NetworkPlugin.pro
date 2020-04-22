QT += gui widgets network

TEMPLATE = lib
CONFIG += plugin

CONFIG += c++11

# Include the-libs build tools
include(/usr/share/the-libs/pri/gentranslations.pri)

#For some reason they got rid of QMake support, so we'll have to hack this in for now
#qtHaveModule(ModemManagerQt) {
#    QT += ModemManagerQt
#} else {
#    INCLUDEPATH += /usr/include/KF5/ModemManagerQt/
#    LIBS += -lKF5ModemManagerQt
#}

qtHaveModule(NetworkManagerQt) {
    QT += NetworkManagerQt
} else {
    INCLUDEPATH += /usr/include/KF5/NetworkManagerQt/
    LIBS += -lKF5NetworkManagerQt
}

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    common.cpp \
    models/deviceconnectionlistmodel.cpp \
    models/wirelessaccesspointsmodel.cpp \
    models/wirelessconnectionlistmodel.cpp \
    models/wirelessnetworklistdelegate.cpp \
    plugin.cpp \
    statusCenter/devicePanes/abstractdevicepane.cpp \
    statusCenter/devicePanes/devicepane.cpp \
    statusCenter/devicePanes/wifidevicepane.cpp \
    statusCenter/networkstatuscenterleftpane.cpp \
    statusCenter/networkstatuscenterpane.cpp \
    statusCenter/popovers/wirelessnetworkselectionpopover.cpp

HEADERS += \
    common.h \
    models/deviceconnectionlistmodel.h \
    models/wirelessaccesspointsmodel.h \
    models/wirelessconnectionlistmodel.h \
    models/wirelessnetworklistdelegate.h \
    plugin.h \
    statusCenter/devicePanes/abstractdevicepane.h \
    statusCenter/devicePanes/devicepane.h \
    statusCenter/devicePanes/wifidevicepane.h \
    statusCenter/networkstatuscenterleftpane.h \
    statusCenter/networkstatuscenterpane.h \
    statusCenter/popovers/wirelessnetworkselectionpopover.h

DISTFILES += \
    Plugin.json \
    defaults.conf

unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += libnm

    translations.files = translations/*.qm
    translations.path = /usr/share/thedesk/LocalePlugin/translations

    defaults.files = defaults.conf
    defaults.path = /etc/theSuite/theDesk/LocalePlugin/

    INSTALLS += translations defaults
}

include(../plugins.pri)

FORMS += \
    statusCenter/devicePanes/devicepane.ui \
    statusCenter/devicePanes/wifidevicepane.ui \
    statusCenter/networkstatuscenterleftpane.ui \
    statusCenter/networkstatuscenterpane.ui \
    statusCenter/popovers/wirelessnetworkselectionpopover.ui