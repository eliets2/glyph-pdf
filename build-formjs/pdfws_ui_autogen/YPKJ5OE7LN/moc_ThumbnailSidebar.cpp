/****************************************************************************
** Meta object code from reading C++ file 'ThumbnailSidebar.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/ThumbnailSidebar.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ThumbnailSidebar.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN16ThumbnailSidebarE_t {};
} // unnamed namespace

template <> constexpr inline auto ThumbnailSidebar::qt_create_metaobjectdata<qt_meta_tag_ZN16ThumbnailSidebarE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ThumbnailSidebar",
        "pageClicked",
        "",
        "page",
        "pageReordered",
        "sourceIndex",
        "targetIndex",
        "zoomChanged",
        "factor",
        "zoomIn",
        "zoomOut",
        "setZoom",
        "z"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'pageClicked'
        QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Signal 'pageReordered'
        QtMocHelpers::SignalData<void(int, int)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::Int, 6 },
        }}),
        // Signal 'zoomChanged'
        QtMocHelpers::SignalData<void(double)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 8 },
        }}),
        // Method 'zoomIn'
        QtMocHelpers::MethodData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'zoomOut'
        QtMocHelpers::MethodData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'setZoom'
        QtMocHelpers::MethodData<void(double)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Double, 12 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ThumbnailSidebar, qt_meta_tag_ZN16ThumbnailSidebarE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ThumbnailSidebar::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16ThumbnailSidebarE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16ThumbnailSidebarE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16ThumbnailSidebarE_t>.metaTypes,
    nullptr
} };

void ThumbnailSidebar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ThumbnailSidebar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->pageClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->pageReordered((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 2: _t->zoomChanged((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 3: _t->zoomIn(); break;
        case 4: _t->zoomOut(); break;
        case 5: _t->setZoom((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ThumbnailSidebar::*)(int )>(_a, &ThumbnailSidebar::pageClicked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ThumbnailSidebar::*)(int , int )>(_a, &ThumbnailSidebar::pageReordered, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ThumbnailSidebar::*)(double )>(_a, &ThumbnailSidebar::zoomChanged, 2))
            return;
    }
}

const QMetaObject *ThumbnailSidebar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ThumbnailSidebar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16ThumbnailSidebarE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ThumbnailSidebar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void ThumbnailSidebar::pageClicked(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ThumbnailSidebar::pageReordered(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void ThumbnailSidebar::zoomChanged(double _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
