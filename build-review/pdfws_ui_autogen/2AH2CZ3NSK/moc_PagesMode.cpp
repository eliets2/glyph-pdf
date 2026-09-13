/****************************************************************************
** Meta object code from reading C++ file 'PagesMode.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/modes/PagesMode.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PagesMode.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp9PagesModeE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::PagesMode::qt_create_metaobjectdata<qt_meta_tag_ZN2gp9PagesModeE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::PagesMode",
        "onPreviewSplit",
        "",
        "onSplit",
        "onApplyReorder",
        "onResetReorder",
        "onThumbnailSizeChanged",
        "onPageCountReady",
        "moveSelectedPagesBy",
        "delta",
        "onApplyPageLabels",
        "fillGridContextMenu",
        "QMenu*",
        "menu",
        "onGridSelectionChanged",
        "onUndoStackIndexChanged",
        "index",
        "onThumbnailReady",
        "pageIndex",
        "QImage",
        "img"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onPreviewSplit'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSplit'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onApplyReorder'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onResetReorder'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onThumbnailSizeChanged'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPageCountReady'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'moveSelectedPagesBy'
        QtMocHelpers::SlotData<void(int)>(8, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 9 },
        }}),
        // Slot 'onApplyPageLabels'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'fillGridContextMenu'
        QtMocHelpers::SlotData<void(QMenu *)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Slot 'onGridSelectionChanged'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onUndoStackIndexChanged'
        QtMocHelpers::SlotData<void(int)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Slot 'onThumbnailReady'
        QtMocHelpers::SlotData<void(int, const QImage &)>(17, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 18 }, { 0x80000000 | 19, 20 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PagesMode, qt_meta_tag_ZN2gp9PagesModeE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::PagesMode::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp9PagesModeE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp9PagesModeE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp9PagesModeE_t>.metaTypes,
    nullptr
} };

void gp::PagesMode::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PagesMode *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onPreviewSplit(); break;
        case 1: _t->onSplit(); break;
        case 2: _t->onApplyReorder(); break;
        case 3: _t->onResetReorder(); break;
        case 4: _t->onThumbnailSizeChanged(); break;
        case 5: _t->onPageCountReady(); break;
        case 6: _t->moveSelectedPagesBy((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->onApplyPageLabels(); break;
        case 8: _t->fillGridContextMenu((*reinterpret_cast<std::add_pointer_t<QMenu*>>(_a[1]))); break;
        case 9: _t->onGridSelectionChanged(); break;
        case 10: _t->onUndoStackIndexChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->onThumbnailReady((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QImage>>(_a[2]))); break;
        default: ;
        }
    }
}

const QMetaObject *gp::PagesMode::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::PagesMode::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp9PagesModeE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int gp::PagesMode::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    return _id;
}
QT_WARNING_POP
