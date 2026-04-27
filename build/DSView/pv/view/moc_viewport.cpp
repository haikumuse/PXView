/****************************************************************************
** Meta object code from reading C++ file 'viewport.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.16)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../DSView/pv/view/viewport.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'viewport.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.16. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_pv__view__Viewport_t {
    QByteArrayData data[11];
    char stringdata0[131];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_pv__view__Viewport_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_pv__view__Viewport_t qt_meta_stringdata_pv__view__Viewport = {
    {
QT_MOC_LITERAL(0, 0, 18), // "pv::view::Viewport"
QT_MOC_LITERAL(1, 19, 15), // "measure_updated"
QT_MOC_LITERAL(2, 35, 0), // ""
QT_MOC_LITERAL(3, 36, 7), // "prgRate"
QT_MOC_LITERAL(4, 44, 8), // "progress"
QT_MOC_LITERAL(5, 53, 16), // "on_trigger_timer"
QT_MOC_LITERAL(6, 70, 13), // "on_drag_timer"
QT_MOC_LITERAL(7, 84, 16), // "show_contextmenu"
QT_MOC_LITERAL(8, 101, 3), // "pos"
QT_MOC_LITERAL(9, 105, 12), // "add_cursor_x"
QT_MOC_LITERAL(10, 118, 12) // "add_cursor_y"

    },
    "pv::view::Viewport\0measure_updated\0\0"
    "prgRate\0progress\0on_trigger_timer\0"
    "on_drag_timer\0show_contextmenu\0pos\0"
    "add_cursor_x\0add_cursor_y"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_pv__view__Viewport[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   49,    2, 0x06 /* Public */,
       3,    1,   50,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       5,    0,   53,    2, 0x08 /* Private */,
       6,    0,   54,    2, 0x08 /* Private */,
       7,    1,   55,    2, 0x08 /* Private */,
       9,    0,   58,    2, 0x08 /* Private */,
      10,    0,   59,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    4,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QPoint,    8,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void pv::view::Viewport::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Viewport *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->measure_updated(); break;
        case 1: _t->prgRate((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->on_trigger_timer(); break;
        case 3: _t->on_drag_timer(); break;
        case 4: _t->show_contextmenu((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 5: _t->add_cursor_x(); break;
        case 6: _t->add_cursor_y(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (Viewport::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&Viewport::measure_updated)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (Viewport::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&Viewport::prgRate)) {
                *result = 1;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject pv::view::Viewport::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_pv__view__Viewport.data,
    qt_meta_data_pv__view__Viewport,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *pv::view::Viewport::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pv::view::Viewport::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_pv__view__Viewport.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "IUiWindow"))
        return static_cast< IUiWindow*>(this);
    return QWidget::qt_metacast(_clname);
}

int pv::view::Viewport::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void pv::view::Viewport::measure_updated()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void pv::view::Viewport::prgRate(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
