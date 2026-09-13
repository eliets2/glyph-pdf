/****************************************************************************
** Meta object code from reading C++ file 'PagesController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/shell/controllers/PagesController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PagesController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp15PagesControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::PagesController::qt_create_metaobjectdata<qt_meta_tag_ZN2gp15PagesControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::PagesController",
        "onPageReordered",
        "",
        "from",
        "to",
        "onCropRequested",
        "pageIndex",
        "QRectF",
        "rect",
        "onPageRotateRequested",
        "degrees"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onPageReordered'
        QtMocHelpers::SlotData<void(int, int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 4 },
        }}),
        // Slot 'onCropRequested'
        QtMocHelpers::SlotData<void(int, QRectF)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 }, { 0x80000000 | 7, 8 },
        }}),
        // Slot 'onPageRotateRequested'
        QtMocHelpers::SlotData<void(int)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PagesController, qt_meta_tag_ZN2gp15PagesControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::PagesController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp15PagesControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp15PagesControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp15PagesControllerE_t>.metaTypes,
    nullptr
} };

void gp::PagesController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PagesController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onPageReordered((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 1: _t->onCropRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QRectF>>(_a[2]))); break;
        case 2: _t->onPageRotateRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *gp::PagesController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::PagesController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp15PagesControllerE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "IToolController"))
        return static_cast< IToolController*>(this);
    return QObject::qt_metacast(_clname);
}

int gp::PagesController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}
QT_WARNING_POP
