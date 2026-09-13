/****************************************************************************
** Meta object code from reading C++ file 'DocumentSession.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/engines/DocumentSession.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DocumentSession.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15DocumentSessionE_t {};
} // unnamed namespace

template <> constexpr inline auto DocumentSession::qt_create_metaobjectdata<qt_meta_tag_ZN15DocumentSessionE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DocumentSession",
        "reloadRequested",
        "",
        "dirtyChanged",
        "dirty",
        "lastAutosaveChanged",
        "time",
        "readOnlyChanged",
        "readOnly",
        "mutationFailed",
        "reason",
        "setReadOnly",
        "beginDocument",
        "path",
        "documentGeneration",
        "mutationRevision",
        "recoverySource",
        "setRecoverySource",
        "autosaveInputPath",
        "clearRecoverySource",
        "lastAutosave",
        "setLastAutosave",
        "findOrphanedAutosaves",
        "recentFiles"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'reloadRequested'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'dirtyChanged'
        QtMocHelpers::SignalData<void(bool)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 4 },
        }}),
        // Signal 'lastAutosaveChanged'
        QtMocHelpers::SignalData<void(const QDateTime &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QDateTime, 6 },
        }}),
        // Signal 'readOnlyChanged'
        QtMocHelpers::SignalData<void(bool)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 },
        }}),
        // Signal 'mutationFailed'
        QtMocHelpers::SignalData<void(const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Slot 'setReadOnly'
        QtMocHelpers::SlotData<void(bool)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 },
        }}),
        // Slot 'beginDocument'
        QtMocHelpers::SlotData<void(const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Slot 'documentGeneration'
        QtMocHelpers::SlotData<qint64() const>(14, 2, QMC::AccessPublic, QMetaType::LongLong),
        // Slot 'mutationRevision'
        QtMocHelpers::SlotData<qint64() const>(15, 2, QMC::AccessPublic, QMetaType::LongLong),
        // Slot 'recoverySource'
        QtMocHelpers::SlotData<QString() const>(16, 2, QMC::AccessPublic, QMetaType::QString),
        // Slot 'setRecoverySource'
        QtMocHelpers::SlotData<void(const QString &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 18 },
        }}),
        // Slot 'clearRecoverySource'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'lastAutosave'
        QtMocHelpers::SlotData<QDateTime() const>(20, 2, QMC::AccessPublic, QMetaType::QDateTime),
        // Slot 'setLastAutosave'
        QtMocHelpers::SlotData<void(const QDateTime &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QDateTime, 6 },
        }}),
        // Slot 'findOrphanedAutosaves'
        QtMocHelpers::SlotData<QStringList(const QStringList &)>(22, 2, QMC::AccessPublic, QMetaType::QStringList, {{
            { QMetaType::QStringList, 23 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DocumentSession, qt_meta_tag_ZN15DocumentSessionE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DocumentSession::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15DocumentSessionE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15DocumentSessionE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15DocumentSessionE_t>.metaTypes,
    nullptr
} };

void DocumentSession::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DocumentSession *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->reloadRequested(); break;
        case 1: _t->dirtyChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 2: _t->lastAutosaveChanged((*reinterpret_cast<std::add_pointer_t<QDateTime>>(_a[1]))); break;
        case 3: _t->readOnlyChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->mutationFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->setReadOnly((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->beginDocument((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: { qint64 _r = _t->documentGeneration();
            if (_a[0]) *reinterpret_cast<qint64*>(_a[0]) = std::move(_r); }  break;
        case 8: { qint64 _r = _t->mutationRevision();
            if (_a[0]) *reinterpret_cast<qint64*>(_a[0]) = std::move(_r); }  break;
        case 9: { QString _r = _t->recoverySource();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 10: _t->setRecoverySource((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->clearRecoverySource(); break;
        case 12: { QDateTime _r = _t->lastAutosave();
            if (_a[0]) *reinterpret_cast<QDateTime*>(_a[0]) = std::move(_r); }  break;
        case 13: _t->setLastAutosave((*reinterpret_cast<std::add_pointer_t<QDateTime>>(_a[1]))); break;
        case 14: { QStringList _r = _t->findOrphanedAutosaves((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QStringList*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DocumentSession::*)()>(_a, &DocumentSession::reloadRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DocumentSession::*)(bool )>(_a, &DocumentSession::dirtyChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DocumentSession::*)(const QDateTime & )>(_a, &DocumentSession::lastAutosaveChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (DocumentSession::*)(bool )>(_a, &DocumentSession::readOnlyChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (DocumentSession::*)(const QString & )>(_a, &DocumentSession::mutationFailed, 4))
            return;
    }
}

const QMetaObject *DocumentSession::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DocumentSession::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15DocumentSessionE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int DocumentSession::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 15;
    }
    return _id;
}

// SIGNAL 0
void DocumentSession::reloadRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void DocumentSession::dirtyChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void DocumentSession::lastAutosaveChanged(const QDateTime & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void DocumentSession::readOnlyChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void DocumentSession::mutationFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
