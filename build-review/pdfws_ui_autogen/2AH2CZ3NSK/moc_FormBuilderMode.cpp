/****************************************************************************
** Meta object code from reading C++ file 'FormBuilderMode.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/modes/FormBuilderMode.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'FormBuilderMode.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp15FormBuilderModeE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::FormBuilderMode::qt_create_metaobjectdata<qt_meta_tag_ZN2gp15FormBuilderModeE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::FormBuilderMode",
        "onFieldButtonToggled",
        "",
        "checked",
        "onFieldPlacementRequested",
        "pageIndex",
        "QRectF",
        "pdfRect",
        "ToolMode",
        "mode",
        "onAutoDetectClicked",
        "onTabOrderToggled",
        "onPreviewFormClicked",
        "onExitFormClicked",
        "onFieldListSelectionChanged",
        "onDeleteFieldClicked",
        "onTabOrderApplyClicked",
        "onEscapePressed",
        "onFieldGeometryCommitted",
        "newRect"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onFieldButtonToggled'
        QtMocHelpers::SlotData<void(bool)>(1, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 3 },
        }}),
        // Slot 'onFieldPlacementRequested'
        QtMocHelpers::SlotData<void(int, QRectF, ToolMode)>(4, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { 0x80000000 | 6, 7 }, { 0x80000000 | 8, 9 },
        }}),
        // Slot 'onAutoDetectClicked'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTabOrderToggled'
        QtMocHelpers::SlotData<void(bool)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 3 },
        }}),
        // Slot 'onPreviewFormClicked'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onExitFormClicked'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onFieldListSelectionChanged'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onDeleteFieldClicked'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTabOrderApplyClicked'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onEscapePressed'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onFieldGeometryCommitted'
        QtMocHelpers::SlotData<void(const QRectF &)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 6, 19 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FormBuilderMode, qt_meta_tag_ZN2gp15FormBuilderModeE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::FormBuilderMode::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp15FormBuilderModeE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp15FormBuilderModeE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp15FormBuilderModeE_t>.metaTypes,
    nullptr
} };

void gp::FormBuilderMode::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FormBuilderMode *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onFieldButtonToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 1: _t->onFieldPlacementRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QRectF>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<ToolMode>>(_a[3]))); break;
        case 2: _t->onAutoDetectClicked(); break;
        case 3: _t->onTabOrderToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->onPreviewFormClicked(); break;
        case 5: _t->onExitFormClicked(); break;
        case 6: _t->onFieldListSelectionChanged(); break;
        case 7: _t->onDeleteFieldClicked(); break;
        case 8: _t->onTabOrderApplyClicked(); break;
        case 9: _t->onEscapePressed(); break;
        case 10: _t->onFieldGeometryCommitted((*reinterpret_cast<std::add_pointer_t<QRectF>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *gp::FormBuilderMode::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::FormBuilderMode::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp15FormBuilderModeE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int gp::FormBuilderMode::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 11;
    }
    return _id;
}
QT_WARNING_POP
