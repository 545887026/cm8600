/****************************************************************************
** Meta object code from reading C++ file 'ChannelWorker.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../src/core/ChannelWorker.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ChannelWorker.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_cm8600__ChannelWorker_t {
    QByteArrayData data[23];
    char stringdata0[214];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_cm8600__ChannelWorker_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_cm8600__ChannelWorker_t qt_meta_stringdata_cm8600__ChannelWorker = {
    {
QT_MOC_LITERAL(0, 0, 21), // "cm8600::ChannelWorker"
QT_MOC_LITERAL(1, 22, 11), // "linkChanged"
QT_MOC_LITERAL(2, 34, 0), // ""
QT_MOC_LITERAL(3, 35, 9), // "channelId"
QT_MOC_LITERAL(4, 45, 9), // "connected"
QT_MOC_LITERAL(5, 55, 6), // "detail"
QT_MOC_LITERAL(6, 62, 16), // "peerAliveChanged"
QT_MOC_LITERAL(7, 79, 5), // "alive"
QT_MOC_LITERAL(8, 85, 12), // "queryHandled"
QT_MOC_LITERAL(9, 98, 7), // "barcode"
QT_MOC_LITERAL(10, 106, 2), // "ok"
QT_MOC_LITERAL(11, 109, 12), // "ChargeParams"
QT_MOC_LITERAL(12, 122, 6), // "params"
QT_MOC_LITERAL(13, 129, 6), // "reason"
QT_MOC_LITERAL(14, 136, 11), // "recordSaved"
QT_MOC_LITERAL(15, 148, 12), // "ResultRecord"
QT_MOC_LITERAL(16, 161, 6), // "record"
QT_MOC_LITERAL(17, 168, 9), // "duplicate"
QT_MOC_LITERAL(18, 178, 5), // "rowId"
QT_MOC_LITERAL(19, 184, 5), // "start"
QT_MOC_LITERAL(20, 190, 4), // "stop"
QT_MOC_LITERAL(21, 195, 6), // "onPoll"
QT_MOC_LITERAL(22, 202, 11) // "onHeartbeat"

    },
    "cm8600::ChannelWorker\0linkChanged\0\0"
    "channelId\0connected\0detail\0peerAliveChanged\0"
    "alive\0queryHandled\0barcode\0ok\0"
    "ChargeParams\0params\0reason\0recordSaved\0"
    "ResultRecord\0record\0duplicate\0rowId\0"
    "start\0stop\0onPoll\0onHeartbeat"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_cm8600__ChannelWorker[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    3,   54,    2, 0x06 /* Public */,
       6,    2,   61,    2, 0x06 /* Public */,
       8,    5,   66,    2, 0x06 /* Public */,
      14,    4,   77,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      19,    0,   86,    2, 0x0a /* Public */,
      20,    0,   87,    2, 0x0a /* Public */,
      21,    0,   88,    2, 0x08 /* Private */,
      22,    0,   89,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Bool, QMetaType::QString,    3,    4,    5,
    QMetaType::Void, QMetaType::Int, QMetaType::Bool,    3,    7,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::Bool, 0x80000000 | 11, QMetaType::QString,    3,    9,   10,   12,   13,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 15, QMetaType::Bool, QMetaType::LongLong,    3,   16,   17,   18,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void cm8600::ChannelWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ChannelWorker *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->linkChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3]))); break;
        case 1: _t->peerAliveChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        case 2: _t->queryHandled((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3])),(*reinterpret_cast< const ChargeParams(*)>(_a[4])),(*reinterpret_cast< const QString(*)>(_a[5]))); break;
        case 3: _t->recordSaved((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const ResultRecord(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3])),(*reinterpret_cast< qint64(*)>(_a[4]))); break;
        case 4: _t->start(); break;
        case 5: _t->stop(); break;
        case 6: _t->onPoll(); break;
        case 7: _t->onHeartbeat(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ChannelWorker::*)(int , bool , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ChannelWorker::linkChanged)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (ChannelWorker::*)(int , bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ChannelWorker::peerAliveChanged)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (ChannelWorker::*)(int , const QString & , bool , const ChargeParams & , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ChannelWorker::queryHandled)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (ChannelWorker::*)(int , const ResultRecord & , bool , qint64 );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ChannelWorker::recordSaved)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject cm8600::ChannelWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_cm8600__ChannelWorker.data,
    qt_meta_data_cm8600__ChannelWorker,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *cm8600::ChannelWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *cm8600::ChannelWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_cm8600__ChannelWorker.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int cm8600::ChannelWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void cm8600::ChannelWorker::linkChanged(int _t1, bool _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void cm8600::ChannelWorker::peerAliveChanged(int _t1, bool _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void cm8600::ChannelWorker::queryHandled(int _t1, const QString & _t2, bool _t3, const ChargeParams & _t4, const QString & _t5)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t5))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void cm8600::ChannelWorker::recordSaved(int _t1, const ResultRecord & _t2, bool _t3, qint64 _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
