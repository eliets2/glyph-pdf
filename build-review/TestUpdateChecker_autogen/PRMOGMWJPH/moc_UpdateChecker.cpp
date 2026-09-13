/****************************************************************************
** Meta object code from reading C++ file 'UpdateChecker.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/UpdateChecker.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'UpdateChecker.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN2gp13UpdateCheckerE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::UpdateChecker::qt_create_metaobjectdata<qt_meta_tag_ZN2gp13UpdateCheckerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::UpdateChecker",
        "checkStarted",
        "",
        "updateAvailable",
        "UpdateInfo",
        "info",
        "noUpdateAvailable",
        "checkFailed",
        "reason",
        "downloadStarted",
        "downloadProgressChanged",
        "percent",
        "downloadReady",
        "msiPath",
        "downloadFailed",
        "updateLaunched",
        "checkForUpdates",
        "downloadUpdate",
        "applyUpdate",
        "onManifestReply",
        "onDownloadProgress",
        "received",
        "total",
        "onDownloadFinished"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'checkStarted'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'updateAvailable'
        QtMocHelpers::SignalData<void(const UpdateInfo &)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 5 },
        }}),
        // Signal 'noUpdateAvailable'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'checkFailed'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Signal 'downloadStarted'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'downloadProgressChanged'
        QtMocHelpers::SignalData<void(int)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 11 },
        }}),
        // Signal 'downloadReady'
        QtMocHelpers::SignalData<void(const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Signal 'downloadFailed'
        QtMocHelpers::SignalData<void(const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Signal 'updateLaunched'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'checkForUpdates'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'downloadUpdate'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'applyUpdate'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onManifestReply'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onDownloadProgress'
        QtMocHelpers::SlotData<void(qint64, qint64)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::LongLong, 21 }, { QMetaType::LongLong, 22 },
        }}),
        // Slot 'onDownloadFinished'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<UpdateChecker, qt_meta_tag_ZN2gp13UpdateCheckerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::UpdateChecker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp13UpdateCheckerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp13UpdateCheckerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp13UpdateCheckerE_t>.metaTypes,
    nullptr
} };

void gp::UpdateChecker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<UpdateChecker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->checkStarted(); break;
        case 1: _t->updateAvailable((*reinterpret_cast<std::add_pointer_t<UpdateInfo>>(_a[1]))); break;
        case 2: _t->noUpdateAvailable(); break;
        case 3: _t->checkFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->downloadStarted(); break;
        case 5: _t->downloadProgressChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->downloadReady((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->downloadFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->updateLaunched(); break;
        case 9: _t->checkForUpdates(); break;
        case 10: _t->downloadUpdate(); break;
        case 11: _t->applyUpdate(); break;
        case 12: _t->onManifestReply(); break;
        case 13: _t->onDownloadProgress((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[2]))); break;
        case 14: _t->onDownloadFinished(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (UpdateChecker::*)()>(_a, &UpdateChecker::checkStarted, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (UpdateChecker::*)(const UpdateInfo & )>(_a, &UpdateChecker::updateAvailable, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (UpdateChecker::*)()>(_a, &UpdateChecker::noUpdateAvailable, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (UpdateChecker::*)(const QString & )>(_a, &UpdateChecker::checkFailed, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (UpdateChecker::*)()>(_a, &UpdateChecker::downloadStarted, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (UpdateChecker::*)(int )>(_a, &UpdateChecker::downloadProgressChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (UpdateChecker::*)(const QString & )>(_a, &UpdateChecker::downloadReady, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (UpdateChecker::*)(const QString & )>(_a, &UpdateChecker::downloadFailed, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (UpdateChecker::*)()>(_a, &UpdateChecker::updateLaunched, 8))
            return;
    }
}

const QMetaObject *gp::UpdateChecker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::UpdateChecker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp13UpdateCheckerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int gp::UpdateChecker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 15;
    }
    return _id;
}

// SIGNAL 0
void gp::UpdateChecker::checkStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void gp::UpdateChecker::updateAvailable(const UpdateInfo & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void gp::UpdateChecker::noUpdateAvailable()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void gp::UpdateChecker::checkFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void gp::UpdateChecker::downloadStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void gp::UpdateChecker::downloadProgressChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void gp::UpdateChecker::downloadReady(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void gp::UpdateChecker::downloadFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void gp::UpdateChecker::updateLaunched()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}
QT_WARNING_POP
