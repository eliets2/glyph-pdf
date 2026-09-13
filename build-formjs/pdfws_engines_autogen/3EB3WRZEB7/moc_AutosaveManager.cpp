/****************************************************************************
** Meta object code from reading C++ file 'AutosaveManager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/engines/AutosaveManager.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AutosaveManager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15AutosaveManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto AutosaveManager::qt_create_metaobjectdata<qt_meta_tag_ZN15AutosaveManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AutosaveManager",
        "autosaveStarted",
        "",
        "autosaveCompleted",
        "time",
        "autosaveFailed",
        "reason",
        "autosaveStale",
        "capturedPath",
        "onTick"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'autosaveStarted'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'autosaveCompleted'
        QtMocHelpers::SignalData<void(const QDateTime &)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QDateTime, 4 },
        }}),
        // Signal 'autosaveFailed'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'autosaveStale'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Slot 'onTick'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AutosaveManager, qt_meta_tag_ZN15AutosaveManagerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AutosaveManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AutosaveManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AutosaveManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15AutosaveManagerE_t>.metaTypes,
    nullptr
} };

void AutosaveManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AutosaveManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->autosaveStarted(); break;
        case 1: _t->autosaveCompleted((*reinterpret_cast<std::add_pointer_t<QDateTime>>(_a[1]))); break;
        case 2: _t->autosaveFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->autosaveStale((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->onTick(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AutosaveManager::*)()>(_a, &AutosaveManager::autosaveStarted, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AutosaveManager::*)(const QDateTime & )>(_a, &AutosaveManager::autosaveCompleted, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AutosaveManager::*)(const QString & )>(_a, &AutosaveManager::autosaveFailed, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (AutosaveManager::*)(const QString & )>(_a, &AutosaveManager::autosaveStale, 3))
            return;
    }
}

const QMetaObject *AutosaveManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AutosaveManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AutosaveManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AutosaveManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void AutosaveManager::autosaveStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void AutosaveManager::autosaveCompleted(const QDateTime & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void AutosaveManager::autosaveFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void AutosaveManager::autosaveStale(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}
QT_WARNING_POP
