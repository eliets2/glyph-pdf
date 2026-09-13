/****************************************************************************
** Meta object code from reading C++ file 'RedactOperation.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/engines/RedactOperation.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'RedactOperation.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp15RedactOperationE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::RedactOperation::qt_create_metaobjectdata<qt_meta_tag_ZN2gp15RedactOperationE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::RedactOperation",
        "stageChanged",
        "",
        "gp::RedactStage",
        "stage",
        "pagesDone",
        "pagesTotal",
        "finished",
        "gp::RedactResult",
        "result"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'stageChanged'
        QtMocHelpers::SignalData<void(gp::RedactStage, int, int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Int, 5 }, { QMetaType::Int, 6 },
        }}),
        // Signal 'finished'
        QtMocHelpers::SignalData<void(const gp::RedactResult &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 8, 9 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<RedactOperation, qt_meta_tag_ZN2gp15RedactOperationE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::RedactOperation::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp15RedactOperationE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp15RedactOperationE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp15RedactOperationE_t>.metaTypes,
    nullptr
} };

void gp::RedactOperation::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<RedactOperation *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->stageChanged((*reinterpret_cast<std::add_pointer_t<gp::RedactStage>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 1: _t->finished((*reinterpret_cast<std::add_pointer_t<gp::RedactResult>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< gp::RedactStage >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< gp::RedactResult >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (RedactOperation::*)(gp::RedactStage , int , int )>(_a, &RedactOperation::stageChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (RedactOperation::*)(const gp::RedactResult & )>(_a, &RedactOperation::finished, 1))
            return;
    }
}

const QMetaObject *gp::RedactOperation::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::RedactOperation::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp15RedactOperationE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int gp::RedactOperation::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void gp::RedactOperation::stageChanged(gp::RedactStage _t1, int _t2, int _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3);
}

// SIGNAL 1
void gp::RedactOperation::finished(const gp::RedactResult & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}
QT_WARNING_POP
