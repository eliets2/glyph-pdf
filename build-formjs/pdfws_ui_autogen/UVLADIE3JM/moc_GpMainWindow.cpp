/****************************************************************************
** Meta object code from reading C++ file 'GpMainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/GpMainWindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'GpMainWindow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN2gp10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::MainWindow",
        "onScreenSelected",
        "",
        "id",
        "toggleTheme",
        "onToolActivated",
        "showFindReplaceDialog",
        "onTabChanged",
        "tab",
        "onModeChanged",
        "m",
        "toggleAi"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onScreenSelected'
        QtMocHelpers::SlotData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Slot 'toggleTheme'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onToolActivated'
        QtMocHelpers::SlotData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Slot 'showFindReplaceDialog'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onTabChanged'
        QtMocHelpers::SlotData<void(const QString &)>(7, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Slot 'onModeChanged'
        QtMocHelpers::SlotData<void(const QString &)>(9, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Slot 'toggleAi'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN2gp10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp10MainWindowE_t>.metaTypes,
    nullptr
} };

void gp::MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onScreenSelected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->toggleTheme(); break;
        case 2: _t->onToolActivated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->showFindReplaceDialog(); break;
        case 4: _t->onTabChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->onModeChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->toggleAi(); break;
        default: ;
        }
    }
}

const QMetaObject *gp::MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int gp::MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
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
QT_WARNING_POP
