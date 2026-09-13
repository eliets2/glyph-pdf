/****************************************************************************
** Meta object code from reading C++ file 'FindBar.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/FindBar.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'FindBar.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7FindBarE_t {};
} // unnamed namespace

template <> constexpr inline auto FindBar::qt_create_metaobjectdata<qt_meta_tag_ZN7FindBarE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "FindBar",
        "searchRequested",
        "",
        "text",
        "forward",
        "matchCase",
        "wholeWords",
        "useRegex",
        "scope",
        "replaceRequested",
        "searchText",
        "replaceText",
        "replaceAllRequested",
        "redactAllRequested",
        "closeRequested",
        "updateButtonStates",
        "onNext",
        "onPrev",
        "SearchScope",
        "ScopeDocumentText",
        "ScopeComments",
        "ScopeBookmarks",
        "ScopeAll"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'searchRequested'
        QtMocHelpers::SignalData<void(const QString &, bool, bool, bool, bool, int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::Bool, 4 }, { QMetaType::Bool, 5 }, { QMetaType::Bool, 6 },
            { QMetaType::Bool, 7 }, { QMetaType::Int, 8 },
        }}),
        // Signal 'replaceRequested'
        QtMocHelpers::SignalData<void(const QString &, const QString &, bool, bool, bool)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 }, { QMetaType::QString, 11 }, { QMetaType::Bool, 5 }, { QMetaType::Bool, 6 },
            { QMetaType::Bool, 7 },
        }}),
        // Signal 'replaceAllRequested'
        QtMocHelpers::SignalData<void(const QString &, const QString &, bool, bool, bool)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 }, { QMetaType::QString, 11 }, { QMetaType::Bool, 5 }, { QMetaType::Bool, 6 },
            { QMetaType::Bool, 7 },
        }}),
        // Signal 'redactAllRequested'
        QtMocHelpers::SignalData<void(const QString &, bool, bool)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::Bool, 5 }, { QMetaType::Bool, 6 },
        }}),
        // Signal 'closeRequested'
        QtMocHelpers::SignalData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'updateButtonStates'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onNext'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPrev'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'SearchScope'
        QtMocHelpers::EnumData<enum SearchScope>(18, 18, QMC::EnumFlags{}).add({
            {   19, SearchScope::ScopeDocumentText },
            {   20, SearchScope::ScopeComments },
            {   21, SearchScope::ScopeBookmarks },
            {   22, SearchScope::ScopeAll },
        }),
    };
    return QtMocHelpers::metaObjectData<FindBar, qt_meta_tag_ZN7FindBarE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject FindBar::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7FindBarE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7FindBarE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7FindBarE_t>.metaTypes,
    nullptr
} };

void FindBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FindBar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->searchRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[6]))); break;
        case 1: _t->replaceRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[5]))); break;
        case 2: _t->replaceAllRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[5]))); break;
        case 3: _t->redactAllRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 4: _t->closeRequested(); break;
        case 5: _t->updateButtonStates(); break;
        case 6: _t->onNext(); break;
        case 7: _t->onPrev(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FindBar::*)(const QString & , bool , bool , bool , bool , int )>(_a, &FindBar::searchRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (FindBar::*)(const QString & , const QString & , bool , bool , bool )>(_a, &FindBar::replaceRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (FindBar::*)(const QString & , const QString & , bool , bool , bool )>(_a, &FindBar::replaceAllRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (FindBar::*)(const QString & , bool , bool )>(_a, &FindBar::redactAllRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (FindBar::*)()>(_a, &FindBar::closeRequested, 4))
            return;
    }
}

const QMetaObject *FindBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FindBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7FindBarE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int FindBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void FindBar::searchRequested(const QString & _t1, bool _t2, bool _t3, bool _t4, bool _t5, int _t6)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3, _t4, _t5, _t6);
}

// SIGNAL 1
void FindBar::replaceRequested(const QString & _t1, const QString & _t2, bool _t3, bool _t4, bool _t5)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3, _t4, _t5);
}

// SIGNAL 2
void FindBar::replaceAllRequested(const QString & _t1, const QString & _t2, bool _t3, bool _t4, bool _t5)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3, _t4, _t5);
}

// SIGNAL 3
void FindBar::redactAllRequested(const QString & _t1, bool _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2, _t3);
}

// SIGNAL 4
void FindBar::closeRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}
QT_WARNING_POP
