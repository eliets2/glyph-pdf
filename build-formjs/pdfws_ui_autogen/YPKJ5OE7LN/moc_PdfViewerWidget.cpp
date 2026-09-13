/****************************************************************************
** Meta object code from reading C++ file 'PdfViewerWidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/PdfViewerWidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PdfViewerWidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15PdfViewerWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto PdfViewerWidget::qt_create_metaobjectdata<qt_meta_tag_ZN15PdfViewerWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "PdfViewerWidget",
        "pageChanged",
        "",
        "currentPage",
        "totalPages",
        "navigationChanged",
        "canBack",
        "canForward",
        "requestPageRotation",
        "degrees",
        "annotationsChanged",
        "annotationEdited",
        "pendingEmbedAnnotationsRestored",
        "pending",
        "textEditRequested",
        "pageIndex",
        "QPointF",
        "pos",
        "pageOperationFinished",
        "cropRequested",
        "QRectF",
        "cropRect",
        "textSelected",
        "selectedText",
        "fieldPlacementRequested",
        "pdfRect",
        "ToolMode",
        "mode",
        "zoomIn",
        "zoomOut"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'pageChanged'
        QtMocHelpers::SignalData<void(int, int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 4 },
        }}),
        // Signal 'navigationChanged'
        QtMocHelpers::SignalData<void(bool, bool)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 6 }, { QMetaType::Bool, 7 },
        }}),
        // Signal 'requestPageRotation'
        QtMocHelpers::SignalData<void(int)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 9 },
        }}),
        // Signal 'annotationsChanged'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'annotationEdited'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'pendingEmbedAnnotationsRestored'
        QtMocHelpers::SignalData<void(bool)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 13 },
        }}),
        // Signal 'textEditRequested'
        QtMocHelpers::SignalData<void(int, QPointF)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 15 }, { 0x80000000 | 16, 17 },
        }}),
        // Signal 'pageOperationFinished'
        QtMocHelpers::SignalData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'cropRequested'
        QtMocHelpers::SignalData<void(int, QRectF)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 15 }, { 0x80000000 | 20, 21 },
        }}),
        // Signal 'textSelected'
        QtMocHelpers::SignalData<void(const QString &)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 23 },
        }}),
        // Signal 'fieldPlacementRequested'
        QtMocHelpers::SignalData<void(int, QRectF, ToolMode)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 15 }, { 0x80000000 | 20, 25 }, { 0x80000000 | 26, 27 },
        }}),
        // Slot 'zoomIn'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'zoomOut'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PdfViewerWidget, qt_meta_tag_ZN15PdfViewerWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject PdfViewerWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15PdfViewerWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15PdfViewerWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15PdfViewerWidgetE_t>.metaTypes,
    nullptr
} };

void PdfViewerWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PdfViewerWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->pageChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 1: _t->navigationChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 2: _t->requestPageRotation((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->annotationsChanged(); break;
        case 4: _t->annotationEdited(); break;
        case 5: _t->pendingEmbedAnnotationsRestored((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->textEditRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 7: _t->pageOperationFinished(); break;
        case 8: _t->cropRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QRectF>>(_a[2]))); break;
        case 9: _t->textSelected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->fieldPlacementRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QRectF>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<ToolMode>>(_a[3]))); break;
        case 11: _t->zoomIn(); break;
        case 12: _t->zoomOut(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (PdfViewerWidget::*)(int , int )>(_a, &PdfViewerWidget::pageChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (PdfViewerWidget::*)(bool , bool )>(_a, &PdfViewerWidget::navigationChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (PdfViewerWidget::*)(int )>(_a, &PdfViewerWidget::requestPageRotation, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (PdfViewerWidget::*)()>(_a, &PdfViewerWidget::annotationsChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (PdfViewerWidget::*)()>(_a, &PdfViewerWidget::annotationEdited, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (PdfViewerWidget::*)(bool )>(_a, &PdfViewerWidget::pendingEmbedAnnotationsRestored, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (PdfViewerWidget::*)(int , QPointF )>(_a, &PdfViewerWidget::textEditRequested, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (PdfViewerWidget::*)()>(_a, &PdfViewerWidget::pageOperationFinished, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (PdfViewerWidget::*)(int , QRectF )>(_a, &PdfViewerWidget::cropRequested, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (PdfViewerWidget::*)(const QString & )>(_a, &PdfViewerWidget::textSelected, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (PdfViewerWidget::*)(int , QRectF , ToolMode )>(_a, &PdfViewerWidget::fieldPlacementRequested, 10))
            return;
    }
}

const QMetaObject *PdfViewerWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PdfViewerWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15PdfViewerWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int PdfViewerWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void PdfViewerWidget::pageChanged(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void PdfViewerWidget::navigationChanged(bool _t1, bool _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void PdfViewerWidget::requestPageRotation(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void PdfViewerWidget::annotationsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void PdfViewerWidget::annotationEdited()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void PdfViewerWidget::pendingEmbedAnnotationsRestored(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void PdfViewerWidget::textEditRequested(int _t1, QPointF _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void PdfViewerWidget::pageOperationFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void PdfViewerWidget::cropRequested(int _t1, QRectF _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1, _t2);
}

// SIGNAL 9
void PdfViewerWidget::textSelected(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void PdfViewerWidget::fieldPlacementRequested(int _t1, QRectF _t2, ToolMode _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
