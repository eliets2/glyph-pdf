/****************************************************************************
** Meta object code from reading C++ file 'ModeController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/modes/ModeController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ModeController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp14ModeControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::ModeController::qt_create_metaobjectdata<qt_meta_tag_ZN2gp14ModeControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::ModeController",
        "screenChanged",
        "",
        "id",
        "ocrRunRequested",
        "ocrReviewAccepted",
        "ocrReviewRejected",
        "ocrReRunRegionRequested",
        "QRectF",
        "regionBbox",
        "redactStatusMessage",
        "message",
        "redactExitRequested"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'screenChanged'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'ocrRunRequested'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'ocrReviewAccepted'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'ocrReviewRejected'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'ocrReRunRegionRequested'
        QtMocHelpers::SignalData<void(QRectF)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 8, 9 },
        }}),
        // Signal 'redactStatusMessage'
        QtMocHelpers::SignalData<void(const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Signal 'redactExitRequested'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ModeController, qt_meta_tag_ZN2gp14ModeControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::ModeController::staticMetaObject = { {
    QMetaObject::SuperData::link<QStackedWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp14ModeControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp14ModeControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp14ModeControllerE_t>.metaTypes,
    nullptr
} };

void gp::ModeController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ModeController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->screenChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->ocrRunRequested(); break;
        case 2: _t->ocrReviewAccepted(); break;
        case 3: _t->ocrReviewRejected(); break;
        case 4: _t->ocrReRunRegionRequested((*reinterpret_cast<std::add_pointer_t<QRectF>>(_a[1]))); break;
        case 5: _t->redactStatusMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->redactExitRequested(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ModeController::*)(const QString & )>(_a, &ModeController::screenChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ModeController::*)()>(_a, &ModeController::ocrRunRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ModeController::*)()>(_a, &ModeController::ocrReviewAccepted, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ModeController::*)()>(_a, &ModeController::ocrReviewRejected, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (ModeController::*)(QRectF )>(_a, &ModeController::ocrReRunRegionRequested, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (ModeController::*)(const QString & )>(_a, &ModeController::redactStatusMessage, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (ModeController::*)()>(_a, &ModeController::redactExitRequested, 6))
            return;
    }
}

const QMetaObject *gp::ModeController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::ModeController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp14ModeControllerE_t>.strings))
        return static_cast<void*>(this);
    return QStackedWidget::qt_metacast(_clname);
}

int gp::ModeController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QStackedWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void gp::ModeController::screenChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void gp::ModeController::ocrRunRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void gp::ModeController::ocrReviewAccepted()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void gp::ModeController::ocrReviewRejected()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void gp::ModeController::ocrReRunRegionRequested(QRectF _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void gp::ModeController::redactStatusMessage(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void gp::ModeController::redactExitRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}
QT_WARNING_POP
