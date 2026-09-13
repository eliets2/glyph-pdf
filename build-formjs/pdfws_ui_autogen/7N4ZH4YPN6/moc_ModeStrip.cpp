/****************************************************************************
** Meta object code from reading C++ file 'ModeStrip.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/shell/ModeStrip.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ModeStrip.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp9ModeStripE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::ModeStrip::qt_create_metaobjectdata<qt_meta_tag_ZN2gp9ModeStripE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::ModeStrip",
        "modeChanged",
        "",
        "id",
        "themeToggleRequested",
        "aiToggleRequested",
        "taskSelected",
        "setAutosaveTime",
        "time",
        "setSignatureStatus",
        "signedCount",
        "totalCount",
        "onSignatureValidationDone"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'modeChanged'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'themeToggleRequested'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'aiToggleRequested'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'taskSelected'
        QtMocHelpers::SignalData<void(const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Slot 'setAutosaveTime'
        QtMocHelpers::SlotData<void(const QDateTime &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QDateTime, 8 },
        }}),
        // Slot 'setSignatureStatus'
        QtMocHelpers::SlotData<void(int, int)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 }, { QMetaType::Int, 11 },
        }}),
        // Slot 'onSignatureValidationDone'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ModeStrip, qt_meta_tag_ZN2gp9ModeStripE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::ModeStrip::staticMetaObject = { {
    QMetaObject::SuperData::link<QFrame::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp9ModeStripE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp9ModeStripE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp9ModeStripE_t>.metaTypes,
    nullptr
} };

void gp::ModeStrip::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ModeStrip *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->modeChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->themeToggleRequested(); break;
        case 2: _t->aiToggleRequested(); break;
        case 3: _t->taskSelected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->setAutosaveTime((*reinterpret_cast<std::add_pointer_t<QDateTime>>(_a[1]))); break;
        case 5: _t->setSignatureStatus((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 6: _t->onSignatureValidationDone(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ModeStrip::*)(const QString & )>(_a, &ModeStrip::modeChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ModeStrip::*)()>(_a, &ModeStrip::themeToggleRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ModeStrip::*)()>(_a, &ModeStrip::aiToggleRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ModeStrip::*)(const QString & )>(_a, &ModeStrip::taskSelected, 3))
            return;
    }
}

const QMetaObject *gp::ModeStrip::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::ModeStrip::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp9ModeStripE_t>.strings))
        return static_cast<void*>(this);
    return QFrame::qt_metacast(_clname);
}

int gp::ModeStrip::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QFrame::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void gp::ModeStrip::modeChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void gp::ModeStrip::themeToggleRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void gp::ModeStrip::aiToggleRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void gp::ModeStrip::taskSelected(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}
QT_WARNING_POP
