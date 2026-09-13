/****************************************************************************
** Meta object code from reading C++ file 'PdfAValidationPanel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/modes/PdfAValidationPanel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PdfAValidationPanel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2gp19PdfAValidationPanelE_t {};
} // unnamed namespace

template <> constexpr inline auto gp::PdfAValidationPanel::qt_create_metaobjectdata<qt_meta_tag_ZN2gp19PdfAValidationPanelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "gp::PdfAValidationPanel",
        "setDocument",
        "",
        "path",
        "PdfAConformance",
        "level"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'setDocument'
        QtMocHelpers::SlotData<void(const QString &, PdfAConformance)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { 0x80000000 | 4, 5 },
        }}),
        // Slot 'setDocument'
        QtMocHelpers::SlotData<void(const QString &)>(1, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PdfAValidationPanel, qt_meta_tag_ZN2gp19PdfAValidationPanelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject gp::PdfAValidationPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QFrame::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp19PdfAValidationPanelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp19PdfAValidationPanelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2gp19PdfAValidationPanelE_t>.metaTypes,
    nullptr
} };

void gp::PdfAValidationPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PdfAValidationPanel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->setDocument((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<PdfAConformance>>(_a[2]))); break;
        case 1: _t->setDocument((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *gp::PdfAValidationPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gp::PdfAValidationPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2gp19PdfAValidationPanelE_t>.strings))
        return static_cast<void*>(this);
    return QFrame::qt_metacast(_clname);
}

int gp::PdfAValidationPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QFrame::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}
QT_WARNING_POP
