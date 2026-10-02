/****************************************************************************
** Meta object code from reading C++ file 'ChannelPanel.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../src/ui/ChannelPanel.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/QVector>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ChannelPanel.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_cm8600__ChannelPanel_t {
    QByteArrayData data[24];
    char stringdata0[234];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_cm8600__ChannelPanel_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_cm8600__ChannelPanel_t qt_meta_stringdata_cm8600__ChannelPanel = {
    {
QT_MOC_LITERAL(0, 0, 20), // "cm8600::ChannelPanel"
QT_MOC_LITERAL(1, 21, 7), // "setLink"
QT_MOC_LITERAL(2, 29, 0), // ""
QT_MOC_LITERAL(3, 30, 9), // "connected"
QT_MOC_LITERAL(4, 40, 6), // "detail"
QT_MOC_LITERAL(5, 47, 12), // "setPeerAlive"
QT_MOC_LITERAL(6, 60, 5), // "alive"
QT_MOC_LITERAL(7, 66, 14), // "onQueryHandled"
QT_MOC_LITERAL(8, 81, 7), // "barcode"
QT_MOC_LITERAL(9, 89, 2), // "ok"
QT_MOC_LITERAL(10, 92, 12), // "ChargeParams"
QT_MOC_LITERAL(11, 105, 6), // "params"
QT_MOC_LITERAL(12, 112, 6), // "reason"
QT_MOC_LITERAL(13, 119, 13), // "onRecordSaved"
QT_MOC_LITERAL(14, 133, 12), // "ResultRecord"
QT_MOC_LITERAL(15, 146, 6), // "record"
QT_MOC_LITERAL(16, 153, 9), // "duplicate"
QT_MOC_LITERAL(17, 163, 9), // "appendLog"
QT_MOC_LITERAL(18, 173, 4), // "time"
QT_MOC_LITERAL(19, 178, 5), // "level"
QT_MOC_LITERAL(20, 184, 7), // "message"
QT_MOC_LITERAL(21, 192, 11), // "loadHistory"
QT_MOC_LITERAL(22, 204, 21), // "QVector<ResultRecord>"
QT_MOC_LITERAL(23, 226, 7) // "records"

    },
    "cm8600::ChannelPanel\0setLink\0\0connected\0"
    "detail\0setPeerAlive\0alive\0onQueryHandled\0"
    "barcode\0ok\0ChargeParams\0params\0reason\0"
    "onRecordSaved\0ResultRecord\0record\0"
    "duplicate\0appendLog\0time\0level\0message\0"
    "loadHistory\0QVector<ResultRecord>\0"
    "records"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_cm8600__ChannelPanel[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    2,   44,    2, 0x0a /* Public */,
       5,    1,   49,    2, 0x0a /* Public */,
       7,    4,   52,    2, 0x0a /* Public */,
      13,    2,   61,    2, 0x0a /* Public */,
      17,    3,   66,    2, 0x0a /* Public */,
      21,    1,   73,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,    3,    4,
    QMetaType::Void, QMetaType::Bool,    6,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool, 0x80000000 | 10, QMetaType::QString,    8,    9,   11,   12,
    QMetaType::Void, 0x80000000 | 14, QMetaType::Bool,   15,   16,
    QMetaType::Void, QMetaType::QDateTime, QMetaType::Int, QMetaType::QString,   18,   19,   20,
    QMetaType::Void, 0x80000000 | 22,   23,

       0        // eod
};

void cm8600::ChannelPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ChannelPanel *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->setLink((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 1: _t->setPeerAlive((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 2: _t->onQueryHandled((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2])),(*reinterpret_cast< const ChargeParams(*)>(_a[3])),(*reinterpret_cast< const QString(*)>(_a[4]))); break;
        case 3: _t->onRecordSaved((*reinterpret_cast< const ResultRecord(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        case 4: _t->appendLog((*reinterpret_cast< const QDateTime(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3]))); break;
        case 5: _t->loadHistory((*reinterpret_cast< const QVector<ResultRecord>(*)>(_a[1]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject cm8600::ChannelPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_cm8600__ChannelPanel.data,
    qt_meta_data_cm8600__ChannelPanel,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *cm8600::ChannelPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *cm8600::ChannelPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_cm8600__ChannelPanel.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int cm8600::ChannelPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 6;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
