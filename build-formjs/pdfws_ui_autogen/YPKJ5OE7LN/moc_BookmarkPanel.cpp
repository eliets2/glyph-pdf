/****************************************************************************
** Meta object code from reading C++ file 'BookmarkPanel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/BookmarkPanel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'BookmarkPanel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13BookmarkPanelE_t {};
} // unnamed namespace

template <> constexpr inline auto BookmarkPanel::qt_create_metaobjectdata<qt_meta_tag_ZN13BookmarkPanelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "BookmarkPanel",
        "navigateToPage",
        "",
        "page",
        "onItemClicked",
        "QModelIndex",
        "proxyIndex",
        "onFilterChanged",
        "text",
        "updateBookmarkCount"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'navigateToPage'
        QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Slot 'onItemClicked'
        QtMocHelpers::SlotData<void(const QModelIndex &)>(4, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Slot 'onFilterChanged'
        QtMocHelpers::SlotData<void(const QString &)>(7, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Slot 'updateBookmarkCount'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<BookmarkPanel, qt_meta_tag_ZN13BookmarkPanelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject BookmarkPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13BookmarkPanelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13BookmarkPanelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13BookmarkPanelE_t>.metaTypes,
    nullptr
} };

void BookmarkPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<BookmarkPanel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->navigateToPage((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->onItemClicked((*reinterpret_cast<std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 2: _t->onFilterChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->updateBookmarkCount(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (BookmarkPanel::*)(int )>(_a, &BookmarkPanel::navigateToPage, 0))
            return;
    }
}

const QMetaObject *BookmarkPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *BookmarkPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13BookmarkPanelE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int BookmarkPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void BookmarkPanel::navigateToPage(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}
QT_WARNING_POP
