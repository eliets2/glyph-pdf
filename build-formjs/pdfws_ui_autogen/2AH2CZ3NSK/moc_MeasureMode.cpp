/****************************************************************************
** Meta object code from reading C++ file 'MeasureMode.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/modes/MeasureMode.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MeasureMode.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp11MeasureModeE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::MeasureMode::qt_create_metaobjectdata<qt_meta_tag_ZN2gp11MeasureModeE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::MeasureMode",
        "statusMessageRequested",
        "",
        "message",
        "onScalePresetChanged",
        "index",
        "onUnitChanged",
        "onCustomRatioEdited",
        "onToolSelected",
        "tool",
        "onCalibrateFromDrawing",
        "onSnapToggled",
        "on",
        "onAnnotationsChanged",
        "onMeasurePreview",
        "text"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'statusMessageRequested'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Slot 'onScalePresetChanged'
        QtMocHelpers::SlotData<void(int)>(4, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 5 },
        }}),
        // Slot 'onUnitChanged'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCustomRatioEdited'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onToolSelected'
        QtMocHelpers::SlotData<void(int)>(8, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 9 },
        }}),
        // Slot 'onCalibrateFromDrawing'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSnapToggled'
        QtMocHelpers::SlotData<void(bool)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 12 },
        }}),
        // Slot 'onAnnotationsChanged'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onMeasurePreview'
        QtMocHelpers::SlotData<void(const QString &)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MeasureMode, qt_meta_tag_ZN2gp11MeasureModeE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::MeasureMode::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp11MeasureModeE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp11MeasureModeE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp11MeasureModeE_t>.metaTypes,
    nullptr
} };

void gp::MeasureMode::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MeasureMode *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->statusMessageRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->onScalePresetChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->onUnitChanged(); break;
        case 3: _t->onCustomRatioEdited(); break;
        case 4: _t->onToolSelected((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->onCalibrateFromDrawing(); break;
        case 6: _t->onSnapToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 7: _t->onAnnotationsChanged(); break;
        case 8: _t->onMeasurePreview((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (MeasureMode::*)(const QString & )>(_a, &MeasureMode::statusMessageRequested, 0))
            return;
    }
}

const QMetaObject *gp::MeasureMode::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::MeasureMode::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp11MeasureModeE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int gp::MeasureMode::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void gp::MeasureMode::statusMessageRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}
QT_WARNING_POP
