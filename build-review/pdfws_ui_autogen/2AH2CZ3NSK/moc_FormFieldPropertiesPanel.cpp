/****************************************************************************
** Meta object code from reading C++ file 'FormFieldPropertiesPanel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/modes/FormFieldPropertiesPanel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'FormFieldPropertiesPanel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp24FormFieldPropertiesPanelE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::FormFieldPropertiesPanel::qt_create_metaobjectdata<qt_meta_tag_ZN2gp24FormFieldPropertiesPanelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::FormFieldPropertiesPanel",
        "propertiesApplied",
        "",
        "fieldName",
        "geometryCommitted",
        "QRectF",
        "newRect",
        "onApplyClicked",
        "onNameChanged",
        "text",
        "onRegexChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'propertiesApplied'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'geometryCommitted'
        QtMocHelpers::SignalData<void(const QRectF &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Slot 'onApplyClicked'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onNameChanged'
        QtMocHelpers::SlotData<void(const QString &)>(8, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Slot 'onRegexChanged'
        QtMocHelpers::SlotData<void(const QString &)>(10, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FormFieldPropertiesPanel, qt_meta_tag_ZN2gp24FormFieldPropertiesPanelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::FormFieldPropertiesPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp24FormFieldPropertiesPanelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp24FormFieldPropertiesPanelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp24FormFieldPropertiesPanelE_t>.metaTypes,
    nullptr
} };

void gp::FormFieldPropertiesPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FormFieldPropertiesPanel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->propertiesApplied((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->geometryCommitted((*reinterpret_cast<std::add_pointer_t<QRectF>>(_a[1]))); break;
        case 2: _t->onApplyClicked(); break;
        case 3: _t->onNameChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->onRegexChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FormFieldPropertiesPanel::*)(const QString & )>(_a, &FormFieldPropertiesPanel::propertiesApplied, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (FormFieldPropertiesPanel::*)(const QRectF & )>(_a, &FormFieldPropertiesPanel::geometryCommitted, 1))
            return;
    }
}

const QMetaObject *gp::FormFieldPropertiesPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::FormFieldPropertiesPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp24FormFieldPropertiesPanelE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int gp::FormFieldPropertiesPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
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
void gp::FormFieldPropertiesPanel::propertiesApplied(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void gp::FormFieldPropertiesPanel::geometryCommitted(const QRectF & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}
QT_WARNING_POP
