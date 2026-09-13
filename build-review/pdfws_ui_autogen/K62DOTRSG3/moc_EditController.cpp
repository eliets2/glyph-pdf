/****************************************************************************
** Meta object code from reading C++ file 'EditController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/shell/controllers/EditController.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'EditController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp14EditControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::EditController::qt_create_metaobjectdata<qt_meta_tag_ZN2gp14EditControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::EditController",
        "ocrResultsReady",
        "",
        "QList<MergedOcrWord>",
        "words",
        "ocrReviewReady",
        "gp::OcrReviewSession",
        "session",
        "ocrRunFailed",
        "message",
        "ocrRunAbandoned",
        "ocrSaveFinished",
        "saved",
        "canceled",
        "runOcr",
        "onOcrAcceptRequested",
        "QList<OcrReviewedWord>",
        "reviewedWords",
        "buildPageOcrResult",
        "PageOcrResult",
        "pageIndex",
        "ocrSaveDialogTitle",
        "totalPages",
        "ocrSavedStatus",
        "fileName",
        "onImageSelected",
        "name",
        "QRectF",
        "placement",
        "onImageMoved",
        "dx",
        "dy",
        "onImageResized",
        "newW",
        "newH",
        "onTextEditRequested",
        "QPointF",
        "pos",
        "onTextFormatChanged",
        "fontFamily",
        "fontSize",
        "QColor",
        "color",
        "bold",
        "italic",
        "alignment",
        "onEraseRequested",
        "OcrJobVerdict",
        "Deliver",
        "Stale",
        "Failed"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'ocrResultsReady'
        QtMocHelpers::SignalData<void(const QList<MergedOcrWord> &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'ocrReviewReady'
        QtMocHelpers::SignalData<void(const gp::OcrReviewSession &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'ocrRunFailed'
        QtMocHelpers::SignalData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Signal 'ocrRunAbandoned'
        QtMocHelpers::SignalData<void(const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Signal 'ocrSaveFinished'
        QtMocHelpers::SignalData<void(bool, bool, const QString &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 12 }, { QMetaType::Bool, 13 }, { QMetaType::QString, 9 },
        }}),
        // Slot 'runOcr'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onOcrAcceptRequested'
        QtMocHelpers::SlotData<void(const QList<OcrReviewedWord> &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Slot 'onOcrAcceptRequested'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'buildPageOcrResult'
        QtMocHelpers::SlotData<PageOcrResult(int, const QList<MergedOcrWord> &)>(18, 2, QMC::AccessPublic, 0x80000000 | 19, {{
            { QMetaType::Int, 20 }, { 0x80000000 | 3, 4 },
        }}),
        // Slot 'ocrSaveDialogTitle'
        QtMocHelpers::SlotData<QString(int, int)>(21, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::Int, 22 }, { QMetaType::Int, 20 },
        }}),
        // Slot 'ocrSavedStatus'
        QtMocHelpers::SlotData<QString(int, int, const QString &)>(23, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::Int, 22 }, { QMetaType::Int, 20 }, { QMetaType::QString, 24 },
        }}),
        // Slot 'onImageSelected'
        QtMocHelpers::SlotData<void(const QString &, const QRectF &)>(25, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 26 }, { 0x80000000 | 27, 28 },
        }}),
        // Slot 'onImageMoved'
        QtMocHelpers::SlotData<void(const QString &, double, double)>(29, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 26 }, { QMetaType::Double, 30 }, { QMetaType::Double, 31 },
        }}),
        // Slot 'onImageResized'
        QtMocHelpers::SlotData<void(const QString &, double, double)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 26 }, { QMetaType::Double, 33 }, { QMetaType::Double, 34 },
        }}),
        // Slot 'onTextEditRequested'
        QtMocHelpers::SlotData<void(int, QPointF)>(35, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 20 }, { 0x80000000 | 36, 37 },
        }}),
        // Slot 'onTextFormatChanged'
        QtMocHelpers::SlotData<void(const QString &, int, const QColor &, bool, bool, int)>(38, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 39 }, { QMetaType::Int, 40 }, { 0x80000000 | 41, 42 }, { QMetaType::Bool, 43 },
            { QMetaType::Bool, 44 }, { QMetaType::Int, 45 },
        }}),
        // Slot 'onEraseRequested'
        QtMocHelpers::SlotData<void(int, QPointF)>(46, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 20 }, { 0x80000000 | 36, 37 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'OcrJobVerdict'
        QtMocHelpers::EnumData<enum OcrJobVerdict>(47, 47, QMC::EnumIsScoped).add({
            {   48, OcrJobVerdict::Deliver },
            {   49, OcrJobVerdict::Stale },
            {   50, OcrJobVerdict::Failed },
        }),
    };
    return QtMocHelpers::metaObjectData<EditController, qt_meta_tag_ZN2gp14EditControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::EditController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp14EditControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp14EditControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp14EditControllerE_t>.metaTypes,
    nullptr
} };

void gp::EditController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<EditController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->ocrResultsReady((*reinterpret_cast<std::add_pointer_t<QList<MergedOcrWord>>>(_a[1]))); break;
        case 1: _t->ocrReviewReady((*reinterpret_cast<std::add_pointer_t<gp::OcrReviewSession>>(_a[1]))); break;
        case 2: _t->ocrRunFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->ocrRunAbandoned((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->ocrSaveFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 5: _t->runOcr(); break;
        case 6: _t->onOcrAcceptRequested((*reinterpret_cast<std::add_pointer_t<QList<OcrReviewedWord>>>(_a[1]))); break;
        case 7: _t->onOcrAcceptRequested(); break;
        case 8: { PageOcrResult _r = _t->buildPageOcrResult((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<MergedOcrWord>>>(_a[2])));
            if (_a[0]) *reinterpret_cast<PageOcrResult*>(_a[0]) = std::move(_r); }  break;
        case 9: { QString _r = _t->ocrSaveDialogTitle((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 10: { QString _r = _t->ocrSavedStatus((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 11: _t->onImageSelected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QRectF>>(_a[2]))); break;
        case 12: _t->onImageMoved((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3]))); break;
        case 13: _t->onImageResized((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3]))); break;
        case 14: _t->onTextEditRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 15: _t->onTextFormatChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QColor>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[6]))); break;
        case 16: _t->onEraseRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (EditController::*)(const QList<MergedOcrWord> & )>(_a, &EditController::ocrResultsReady, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (EditController::*)(const gp::OcrReviewSession & )>(_a, &EditController::ocrReviewReady, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (EditController::*)(const QString & )>(_a, &EditController::ocrRunFailed, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (EditController::*)(const QString & )>(_a, &EditController::ocrRunAbandoned, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (EditController::*)(bool , bool , const QString & )>(_a, &EditController::ocrSaveFinished, 4))
            return;
    }
}

const QMetaObject *gp::EditController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::EditController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp14EditControllerE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "IToolController"))
        return static_cast< IToolController*>(this);
    return QObject::qt_metacast(_clname);
}

int gp::EditController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 17;
    }
    return _id;
}

// SIGNAL 0
void gp::EditController::ocrResultsReady(const QList<MergedOcrWord> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void gp::EditController::ocrReviewReady(const gp::OcrReviewSession & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void gp::EditController::ocrRunFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void gp::EditController::ocrRunAbandoned(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void gp::EditController::ocrSaveFinished(bool _t1, bool _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
