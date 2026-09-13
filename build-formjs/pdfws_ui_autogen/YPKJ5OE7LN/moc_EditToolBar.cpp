/****************************************************************************
** Meta object code from reading C++ file 'EditToolBar.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/EditToolBar.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'EditToolBar.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11EditToolBarE_t {};
} // unnamed namespace

template <> constexpr inline auto EditToolBar::qt_create_metaobjectdata<qt_meta_tag_ZN11EditToolBarE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "EditToolBar",
        "activeToolChanged",
        "",
        "ToolMode",
        "mode",
        "textFormatChanged",
        "fontFamily",
        "fontSize",
        "QColor",
        "color",
        "bold",
        "italic",
        "alignment"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'activeToolChanged'
        QtMocHelpers::SignalData<void(ToolMode)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'textFormatChanged'
        QtMocHelpers::SignalData<void(const QString &, int, const QColor &, bool, bool, int)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 }, { QMetaType::Int, 7 }, { 0x80000000 | 8, 9 }, { QMetaType::Bool, 10 },
            { QMetaType::Bool, 11 }, { QMetaType::Int, 12 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<EditToolBar, qt_meta_tag_ZN11EditToolBarE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject EditToolBar::staticMetaObject = { {
    QMetaObject::SuperData::link<QToolBar::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11EditToolBarE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11EditToolBarE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11EditToolBarE_t>.metaTypes,
    nullptr
} };

void EditToolBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<EditToolBar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->activeToolChanged((*reinterpret_cast<std::add_pointer_t<ToolMode>>(_a[1]))); break;
        case 1: _t->textFormatChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QColor>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[6]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (EditToolBar::*)(ToolMode )>(_a, &EditToolBar::activeToolChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (EditToolBar::*)(const QString & , int , const QColor & , bool , bool , int )>(_a, &EditToolBar::textFormatChanged, 1))
            return;
    }
}

const QMetaObject *EditToolBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *EditToolBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11EditToolBarE_t>.strings))
        return static_cast<void*>(this);
    return QToolBar::qt_metacast(_clname);
}

int EditToolBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QToolBar::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void EditToolBar::activeToolChanged(ToolMode _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void EditToolBar::textFormatChanged(const QString & _t1, int _t2, const QColor & _t3, bool _t4, bool _t5, int _t6)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3, _t4, _t5, _t6);
}
QT_WARNING_POP
