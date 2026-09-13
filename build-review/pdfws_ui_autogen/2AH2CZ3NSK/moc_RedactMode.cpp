/****************************************************************************
** Meta object code from reading C++ file 'RedactMode.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/modes/RedactMode.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'RedactMode.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp10RedactModeE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::RedactMode::qt_create_metaobjectdata<qt_meta_tag_ZN2gp10RedactModeE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::RedactMode",
        "statusMessageRequested",
        "",
        "message",
        "exitRequested",
        "onPatternChanged",
        "index",
        "onRegexTextChanged",
        "text",
        "onPreviewMatches",
        "onApplyRedactions",
        "onClearMarks",
        "onScopeChanged",
        "onMarkRegion",
        "onMarkAllOccurrences",
        "onImportWordList"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'statusMessageRequested'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'exitRequested'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onPatternChanged'
        QtMocHelpers::SlotData<void(int)>(5, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 6 },
        }}),
        // Slot 'onRegexTextChanged'
        QtMocHelpers::SlotData<void(const QString &)>(7, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Slot 'onPreviewMatches'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onApplyRedactions'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onClearMarks'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onScopeChanged'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onMarkRegion'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onMarkAllOccurrences'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onImportWordList'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<RedactMode, qt_meta_tag_ZN2gp10RedactModeE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::RedactMode::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp10RedactModeE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp10RedactModeE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp10RedactModeE_t>.metaTypes,
    nullptr
} };

void gp::RedactMode::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<RedactMode *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->statusMessageRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->exitRequested(); break;
        case 2: _t->onPatternChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->onRegexTextChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->onPreviewMatches(); break;
        case 5: _t->onApplyRedactions(); break;
        case 6: _t->onClearMarks(); break;
        case 7: _t->onScopeChanged(); break;
        case 8: _t->onMarkRegion(); break;
        case 9: _t->onMarkAllOccurrences(); break;
        case 10: _t->onImportWordList(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (RedactMode::*)(const QString & )>(_a, &RedactMode::statusMessageRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (RedactMode::*)()>(_a, &RedactMode::exitRequested, 1))
            return;
    }
}

const QMetaObject *gp::RedactMode::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::RedactMode::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp10RedactModeE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int gp::RedactMode::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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

// SIGNAL 0
void gp::RedactMode::statusMessageRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void gp::RedactMode::exitRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
