/****************************************************************************
** Meta object code from reading C++ file 'OCRMode.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/modes/OCRMode.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'OCRMode.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp7OCRModeE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::OCRMode::qt_create_metaobjectdata<qt_meta_tag_ZN2gp7OCRModeE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::OCRMode",
        "ocrRequested",
        "",
        "reviewAccepted",
        "reviewRejected",
        "reOcrRegionRequested",
        "QRectF",
        "regionBbox",
        "reviewStateChanged",
        "gp::OCRMode::ReviewState",
        "state",
        "selectedWordChanged",
        "stableId",
        "onRunOcr",
        "onAcceptResults",
        "onRejectResults",
        "notifyOcrFailed",
        "message",
        "notifyOcrCanceled",
        "notifySaveFinished",
        "saved",
        "canceled",
        "onImagePaneContextMenu",
        "QPoint",
        "pos",
        "onReOcrRegion",
        "onWordLinkActivated",
        "link",
        "ReviewState",
        "Idle",
        "Running",
        "ReviewReady",
        "Saving",
        "RecoverableError"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'ocrRequested'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'reviewAccepted'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'reviewRejected'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'reOcrRegionRequested'
        QtMocHelpers::SignalData<void(QRectF)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'reviewStateChanged'
        QtMocHelpers::SignalData<void(gp::OCRMode::ReviewState)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
        // Signal 'selectedWordChanged'
        QtMocHelpers::SignalData<void(int)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 12 },
        }}),
        // Slot 'onRunOcr'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onAcceptResults'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onRejectResults'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'notifyOcrFailed'
        QtMocHelpers::SlotData<void(const QString &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 17 },
        }}),
        // Slot 'notifyOcrCanceled'
        QtMocHelpers::SlotData<void(const QString &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 17 },
        }}),
        // Slot 'notifySaveFinished'
        QtMocHelpers::SlotData<void(bool, bool, const QString &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 20 }, { QMetaType::Bool, 21 }, { QMetaType::QString, 17 },
        }}),
        // Slot 'onImagePaneContextMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(22, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 23, 24 },
        }}),
        // Slot 'onReOcrRegion'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onWordLinkActivated'
        QtMocHelpers::SlotData<void(const QString &)>(26, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 27 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'ReviewState'
        QtMocHelpers::EnumData<enum ReviewState>(28, 28, QMC::EnumIsScoped).add({
            {   29, ReviewState::Idle },
            {   30, ReviewState::Running },
            {   31, ReviewState::ReviewReady },
            {   32, ReviewState::Saving },
            {   33, ReviewState::RecoverableError },
        }),
    };
    return QtMocHelpers::metaObjectData<OCRMode, qt_meta_tag_ZN2gp7OCRModeE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::OCRMode::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp7OCRModeE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp7OCRModeE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp7OCRModeE_t>.metaTypes,
    nullptr
} };

void gp::OCRMode::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<OCRMode *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->ocrRequested(); break;
        case 1: _t->reviewAccepted(); break;
        case 2: _t->reviewRejected(); break;
        case 3: _t->reOcrRegionRequested((*reinterpret_cast<std::add_pointer_t<QRectF>>(_a[1]))); break;
        case 4: _t->reviewStateChanged((*reinterpret_cast<std::add_pointer_t<gp::OCRMode::ReviewState>>(_a[1]))); break;
        case 5: _t->selectedWordChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->onRunOcr(); break;
        case 7: _t->onAcceptResults(); break;
        case 8: _t->onRejectResults(); break;
        case 9: _t->notifyOcrFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->notifyOcrCanceled((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->notifySaveFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 12: _t->onImagePaneContextMenu((*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 13: _t->onReOcrRegion(); break;
        case 14: _t->onWordLinkActivated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (OCRMode::*)()>(_a, &OCRMode::ocrRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (OCRMode::*)()>(_a, &OCRMode::reviewAccepted, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (OCRMode::*)()>(_a, &OCRMode::reviewRejected, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (OCRMode::*)(QRectF )>(_a, &OCRMode::reOcrRegionRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (OCRMode::*)(gp::OCRMode::ReviewState )>(_a, &OCRMode::reviewStateChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (OCRMode::*)(int )>(_a, &OCRMode::selectedWordChanged, 5))
            return;
    }
}

const QMetaObject *gp::OCRMode::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::OCRMode::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp7OCRModeE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int gp::OCRMode::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
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
void gp::OCRMode::ocrRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void gp::OCRMode::reviewAccepted()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void gp::OCRMode::reviewRejected()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void gp::OCRMode::reOcrRegionRequested(QRectF _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void gp::OCRMode::reviewStateChanged(gp::OCRMode::ReviewState _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void gp::OCRMode::selectedWordChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
