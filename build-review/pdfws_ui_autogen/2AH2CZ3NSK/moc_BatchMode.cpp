/****************************************************************************
** Meta object code from reading C++ file 'BatchMode.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/modes/BatchMode.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'BatchMode.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp9BatchModeE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::BatchMode::qt_create_metaobjectdata<qt_meta_tag_ZN2gp9BatchModeE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::BatchMode",
        "batchFinished",
        "",
        "onAddFiles",
        "onAddFolder",
        "onClearFiles",
        "onRemoveSelected",
        "onRunClicked",
        "onCancelClicked",
        "onBatchProgress",
        "value",
        "onBatchFinished",
        "onExportLog",
        "onOperationChanged",
        "index",
        "onToggleHotFolder",
        "onHotFolderChanged",
        "path"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'batchFinished'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onAddFiles'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAddFolder'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onClearFiles'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onRemoveSelected'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onRunClicked'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCancelClicked'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onBatchProgress'
        QtMocHelpers::SlotData<void(int)>(9, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 10 },
        }}),
        // Slot 'onBatchFinished'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onExportLog'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onOperationChanged'
        QtMocHelpers::SlotData<void(int)>(13, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 14 },
        }}),
        // Slot 'onToggleHotFolder'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onHotFolderChanged'
        QtMocHelpers::SlotData<void(const QString &)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 17 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<BatchMode, qt_meta_tag_ZN2gp9BatchModeE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::BatchMode::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp9BatchModeE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp9BatchModeE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp9BatchModeE_t>.metaTypes,
    nullptr
} };

void gp::BatchMode::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<BatchMode *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->batchFinished(); break;
        case 1: _t->onAddFiles(); break;
        case 2: _t->onAddFolder(); break;
        case 3: _t->onClearFiles(); break;
        case 4: _t->onRemoveSelected(); break;
        case 5: _t->onRunClicked(); break;
        case 6: _t->onCancelClicked(); break;
        case 7: _t->onBatchProgress((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->onBatchFinished(); break;
        case 9: _t->onExportLog(); break;
        case 10: _t->onOperationChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->onToggleHotFolder(); break;
        case 12: _t->onHotFolderChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (BatchMode::*)()>(_a, &BatchMode::batchFinished, 0))
            return;
    }
}

const QMetaObject *gp::BatchMode::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::BatchMode::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp9BatchModeE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int gp::BatchMode::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 13;
    }
    return _id;
}

// SIGNAL 0
void gp::BatchMode::batchFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
