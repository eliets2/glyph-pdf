/****************************************************************************
** Meta object code from reading C++ file 'WelcomeWidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/WelcomeWidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'WelcomeWidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13WelcomeWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto WelcomeWidget::qt_create_metaobjectdata<qt_meta_tag_ZN13WelcomeWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "WelcomeWidget",
        "openFileRequested",
        "",
        "mergeFilesRequested",
        "convertRequested",
        "protectRequested",
        "importOfficeRequested",
        "imagesToPdfRequested",
        "recentFileRequested",
        "filePath",
        "removeRecentFileRequested"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'openFileRequested'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'mergeFilesRequested'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'convertRequested'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'protectRequested'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'importOfficeRequested'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'imagesToPdfRequested'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'recentFileRequested'
        QtMocHelpers::SignalData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Signal 'removeRecentFileRequested'
        QtMocHelpers::SignalData<void(const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<WelcomeWidget, qt_meta_tag_ZN13WelcomeWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject WelcomeWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13WelcomeWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13WelcomeWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13WelcomeWidgetE_t>.metaTypes,
    nullptr
} };

void WelcomeWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<WelcomeWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->openFileRequested(); break;
        case 1: _t->mergeFilesRequested(); break;
        case 2: _t->convertRequested(); break;
        case 3: _t->protectRequested(); break;
        case 4: _t->importOfficeRequested(); break;
        case 5: _t->imagesToPdfRequested(); break;
        case 6: _t->recentFileRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->removeRecentFileRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (WelcomeWidget::*)()>(_a, &WelcomeWidget::openFileRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (WelcomeWidget::*)()>(_a, &WelcomeWidget::mergeFilesRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (WelcomeWidget::*)()>(_a, &WelcomeWidget::convertRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (WelcomeWidget::*)()>(_a, &WelcomeWidget::protectRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (WelcomeWidget::*)()>(_a, &WelcomeWidget::importOfficeRequested, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (WelcomeWidget::*)()>(_a, &WelcomeWidget::imagesToPdfRequested, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (WelcomeWidget::*)(const QString & )>(_a, &WelcomeWidget::recentFileRequested, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (WelcomeWidget::*)(const QString & )>(_a, &WelcomeWidget::removeRecentFileRequested, 7))
            return;
    }
}

const QMetaObject *WelcomeWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *WelcomeWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13WelcomeWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int WelcomeWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void WelcomeWidget::openFileRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void WelcomeWidget::mergeFilesRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void WelcomeWidget::convertRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void WelcomeWidget::protectRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void WelcomeWidget::importOfficeRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void WelcomeWidget::imagesToPdfRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void WelcomeWidget::recentFileRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void WelcomeWidget::removeRecentFileRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}
QT_WARNING_POP
