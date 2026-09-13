/****************************************************************************
** Meta object code from reading C++ file 'AnnotationLayer.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/AnnotationLayer.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AnnotationLayer.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15AnnotationLayerE_t {};
} // unnamed namespace

template <> constexpr inline auto AnnotationLayer::qt_create_metaobjectdata<qt_meta_tag_ZN15AnnotationLayerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AnnotationLayer",
        "annotationsChanged",
        "",
        "selectionChanged",
        "index",
        "textEditRequested",
        "pageIndex",
        "QPointF",
        "pos",
        "eraseRequested",
        "imageSelected",
        "xobjectName",
        "QRectF",
        "placement",
        "imageMoved",
        "dx",
        "dy",
        "imageResized",
        "newW",
        "newH",
        "measurePreviewChanged",
        "text",
        "measurementFinished"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'annotationsChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'selectionChanged'
        QtMocHelpers::SignalData<void(int)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Signal 'textEditRequested'
        QtMocHelpers::SignalData<void(int, QPointF)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 }, { 0x80000000 | 7, 8 },
        }}),
        // Signal 'eraseRequested'
        QtMocHelpers::SignalData<void(int, QPointF)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 }, { 0x80000000 | 7, 8 },
        }}),
        // Signal 'imageSelected'
        QtMocHelpers::SignalData<void(const QString &, const QRectF &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { 0x80000000 | 12, 13 },
        }}),
        // Signal 'imageMoved'
        QtMocHelpers::SignalData<void(const QString &, double, double)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { QMetaType::Double, 15 }, { QMetaType::Double, 16 },
        }}),
        // Signal 'imageResized'
        QtMocHelpers::SignalData<void(const QString &, double, double)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { QMetaType::Double, 18 }, { QMetaType::Double, 19 },
        }}),
        // Signal 'measurePreviewChanged'
        QtMocHelpers::SignalData<void(const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Signal 'measurementFinished'
        QtMocHelpers::SignalData<void()>(22, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AnnotationLayer, qt_meta_tag_ZN15AnnotationLayerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AnnotationLayer::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AnnotationLayerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AnnotationLayerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15AnnotationLayerE_t>.metaTypes,
    nullptr
} };

void AnnotationLayer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AnnotationLayer *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->annotationsChanged(); break;
        case 1: _t->selectionChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->textEditRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 3: _t->eraseRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 4: _t->imageSelected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QRectF>>(_a[2]))); break;
        case 5: _t->imageMoved((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3]))); break;
        case 6: _t->imageResized((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3]))); break;
        case 7: _t->measurePreviewChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->measurementFinished(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AnnotationLayer::*)()>(_a, &AnnotationLayer::annotationsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationLayer::*)(int )>(_a, &AnnotationLayer::selectionChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationLayer::*)(int , QPointF )>(_a, &AnnotationLayer::textEditRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationLayer::*)(int , QPointF )>(_a, &AnnotationLayer::eraseRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationLayer::*)(const QString & , const QRectF & )>(_a, &AnnotationLayer::imageSelected, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationLayer::*)(const QString & , double , double )>(_a, &AnnotationLayer::imageMoved, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationLayer::*)(const QString & , double , double )>(_a, &AnnotationLayer::imageResized, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationLayer::*)(const QString & )>(_a, &AnnotationLayer::measurePreviewChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationLayer::*)()>(_a, &AnnotationLayer::measurementFinished, 8))
            return;
    }
}

const QMetaObject *AnnotationLayer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AnnotationLayer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AnnotationLayerE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int AnnotationLayer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void AnnotationLayer::annotationsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void AnnotationLayer::selectionChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void AnnotationLayer::textEditRequested(int _t1, QPointF _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void AnnotationLayer::eraseRequested(int _t1, QPointF _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void AnnotationLayer::imageSelected(const QString & _t1, const QRectF & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void AnnotationLayer::imageMoved(const QString & _t1, double _t2, double _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2, _t3);
}

// SIGNAL 6
void AnnotationLayer::imageResized(const QString & _t1, double _t2, double _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2, _t3);
}

// SIGNAL 7
void AnnotationLayer::measurePreviewChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void AnnotationLayer::measurementFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}
QT_WARNING_POP
