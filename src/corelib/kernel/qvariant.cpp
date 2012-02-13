/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qvariant.h"
#include "qbitarray.h"
#include "qbytearray.h"
#include "qdatastream.h"
#include "qdebug.h"
#include "qmap.h"
#include "qdatetime.h"
#include "qeasingcurve.h"
#include "qlist.h"
#include "qstring.h"
#include "qstringlist.h"
#include "qurl.h"
#include "qlocale.h"
#include "quuid.h"
#ifndef QT_BOOTSTRAPPED
#include "qabstractitemmodel.h"
#endif
#include "private/qvariant_p.h"
#include "qmetatype_p.h"

#ifndef QT_NO_GEOM_VARIANT
#include "qsize.h"
#include "qpoint.h"
#include "qrect.h"
#include "qline.h"
#endif

#include <float.h>

QT_BEGIN_NAMESPACE

#ifndef DBL_DIG
#  define DBL_DIG 10
#endif
#ifndef FLT_DIG
#  define FLT_DIG 6
#endif

namespace {
class HandlersManager
{
    static const QVariant::Handler *Handlers[QModulesPrivate::ModulesCount];
public:
    const QVariant::Handler *operator[] (const int typeId) const
    {
        return Handlers[QModulesPrivate::moduleForType(typeId)];
    }

    void registerHandler(const QModulesPrivate::Names name, const QVariant::Handler *handler)
    {
        Handlers[name] = handler;
    }

    inline void unregisterHandler(const QModulesPrivate::Names name);
};
}  // namespace

namespace {
template<typename T>
struct TypeDefinition {
    static const bool IsAvailable = true;
};

// Ignore these types, as incomplete
#ifdef QT_BOOTSTRAPPED
template<> struct TypeDefinition<QEasingCurve> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QModelIndex> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_GEOM_VARIANT
template<> struct TypeDefinition<QRect> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QRectF> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QSize> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QSizeF> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QLine> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QLineF> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QPoint> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QPointF> { static const bool IsAvailable = false; };
#endif

struct CoreTypesFilter {
    template<typename T>
    struct Acceptor {
        static const bool IsAccepted = QTypeModuleInfo<T>::IsCore && TypeDefinition<T>::IsAvailable;
    };
};
} // annonymous used to hide TypeDefinition

namespace { // annonymous used to hide QVariant handlers

static void construct(QVariant::Private *x, const void *copy)
{
    QVariantConstructor<CoreTypesFilter> constructor(x, copy);
    QMetaTypeSwitcher::switcher<void>(constructor, x->type, 0);
}

static void clear(QVariant::Private *d)
{
    QVariantDestructor<CoreTypesFilter> cleaner(d);
    QMetaTypeSwitcher::switcher<void>(cleaner, d->type, 0);
}

static bool isNull(const QVariant::Private *d)
{
    QVariantIsNull<CoreTypesFilter> isNull(d);
    return QMetaTypeSwitcher::switcher<bool>(isNull, d->type, 0);
}

/*!
  \internal

  Compares \a a to \a b. The caller guarantees that \a a and \a b
  are of the same type.
 */
static bool compare(const QVariant::Private *a, const QVariant::Private *b)
{
    QVariantComparator<CoreTypesFilter> comparator(a, b);
    return QMetaTypeSwitcher::switcher<bool>(comparator, a->type, 0);
}

/*!
  \internal
 */
static qlonglong qMetaTypeNumber(const QVariant::Private *d)
{
    switch (d->type) {
    case QMetaType::Int:
        return d->data.i;
    case QMetaType::LongLong:
        return d->data.ll;
    case QMetaType::Char:
        return qlonglong(d->data.c);
    case QMetaType::Short:
        return qlonglong(d->data.s);
    case QMetaType::Long:
        return qlonglong(d->data.l);
    case QMetaType::Float:
        return qRound64(d->data.f);
    case QVariant::Double:
        return qRound64(d->data.d);
    }
    Q_ASSERT(false);
    return 0;
}

static qulonglong qMetaTypeUNumber(const QVariant::Private *d)
{
    switch (d->type) {
    case QVariant::UInt:
        return d->data.u;
    case QVariant::ULongLong:
        return d->data.ull;
    case QMetaType::UChar:
        return d->data.uc;
    case QMetaType::UShort:
        return d->data.us;
    case QMetaType::ULong:
        return d->data.ul;
    }
    Q_ASSERT(false);
    return 0;
}

static qlonglong qConvertToNumber(const QVariant::Private *d, bool *ok)
{
    *ok = true;

    switch (uint(d->type)) {
    case QVariant::String:
        return v_cast<QString>(d)->toLongLong(ok);
    case QVariant::Char:
        return v_cast<QChar>(d)->unicode();
    case QVariant::ByteArray:
        return v_cast<QByteArray>(d)->toLongLong(ok);
    case QVariant::Bool:
        return qlonglong(d->data.b);
    case QVariant::Double:
    case QVariant::Int:
    case QMetaType::Char:
    case QMetaType::Short:
    case QMetaType::Long:
    case QMetaType::Float:
    case QMetaType::LongLong:
        return qMetaTypeNumber(d);
    case QVariant::ULongLong:
    case QVariant::UInt:
    case QMetaType::UChar:
    case QMetaType::UShort:
    case QMetaType::ULong:
        return qlonglong(qMetaTypeUNumber(d));
    }

    *ok = false;
    return Q_INT64_C(0);
}

static qulonglong qConvertToUnsignedNumber(const QVariant::Private *d, bool *ok)
{
    *ok = true;

    switch (uint(d->type)) {
    case QVariant::String:
        return v_cast<QString>(d)->toULongLong(ok);
    case QVariant::Char:
        return v_cast<QChar>(d)->unicode();
    case QVariant::ByteArray:
        return v_cast<QByteArray>(d)->toULongLong(ok);
    case QVariant::Bool:
        return qulonglong(d->data.b);
    case QVariant::Double:
    case QVariant::Int:
    case QMetaType::Char:
    case QMetaType::Short:
    case QMetaType::Long:
    case QMetaType::Float:
    case QMetaType::LongLong:
        return qulonglong(qMetaTypeNumber(d));
    case QVariant::ULongLong:
    case QVariant::UInt:
    case QMetaType::UChar:
    case QMetaType::UShort:
    case QMetaType::ULong:
        return qMetaTypeUNumber(d);
    }

    *ok = false;
    return Q_UINT64_C(0);
}

template<typename TInput, typename LiteralWrapper>
inline bool qt_convertToBool(const QVariant::Private *const d)
{
    TInput str = v_cast<TInput>(d)->toLower();
    return !(str == LiteralWrapper("0") || str == LiteralWrapper("false") || str.isEmpty());
}

/*!
 \internal

 Converts \a d to type \a t, which is placed in \a result.
 */
static bool convert(const QVariant::Private *d, int t, void *result, bool *ok)
{
    Q_ASSERT(d->type != uint(t));
    Q_ASSERT(result);

    bool dummy;
    if (!ok)
        ok = &dummy;

    switch (uint(t)) {
    case QVariant::Url:
        switch (d->type) {
        case QVariant::String:
            *static_cast<QUrl *>(result) = QUrl(*v_cast<QString>(d));
            break;
        default:
            return false;
        }
        break;
    case QVariant::String: {
        QString *str = static_cast<QString *>(result);
        switch (d->type) {
        case QVariant::Char:
            *str = QString(*v_cast<QChar>(d));
            break;
        case QMetaType::Char:
        case QMetaType::UChar:
            *str = QChar::fromAscii(d->data.c);
            break;
        case QMetaType::Short:
        case QMetaType::Long:
        case QVariant::Int:
        case QVariant::LongLong:
            *str = QString::number(qMetaTypeNumber(d));
            break;
        case QVariant::UInt:
        case QVariant::ULongLong:
        case QMetaType::UShort:
        case QMetaType::ULong:
            *str = QString::number(qMetaTypeUNumber(d));
            break;
        case QMetaType::Float:
            *str = QString::number(d->data.f, 'g', FLT_DIG);
            break;
        case QVariant::Double:
            *str = QString::number(d->data.d, 'g', DBL_DIG);
            break;
#if !defined(QT_NO_DATESTRING)
        case QVariant::Date:
            *str = v_cast<QDate>(d)->toString(Qt::ISODate);
            break;
        case QVariant::Time:
            *str = v_cast<QTime>(d)->toString(Qt::ISODate);
            break;
        case QVariant::DateTime:
            *str = v_cast<QDateTime>(d)->toString(Qt::ISODate);
            break;
#endif
        case QVariant::Bool:
            *str = QLatin1String(d->data.b ? "true" : "false");
            break;
        case QVariant::ByteArray:
            *str = QString::fromAscii(v_cast<QByteArray>(d)->constData());
            break;
        case QVariant::StringList:
            if (v_cast<QStringList>(d)->count() == 1)
                *str = v_cast<QStringList>(d)->at(0);
            break;
        case QVariant::Url:
            *str = v_cast<QUrl>(d)->toString();
            break;
        case QVariant::Uuid:
            *str = v_cast<QUuid>(d)->toString();
            break;
        default:
            return false;
        }
        break;
    }
    case QVariant::Char: {
        QChar *c = static_cast<QChar *>(result);
        switch (d->type) {
        case QVariant::Int:
        case QVariant::LongLong:
        case QMetaType::Char:
        case QMetaType::Short:
        case QMetaType::Long:
        case QMetaType::Float:
            *c = QChar(ushort(qMetaTypeNumber(d)));
            break;
        case QVariant::UInt:
        case QVariant::ULongLong:
        case QMetaType::UChar:
        case QMetaType::UShort:
        case QMetaType::ULong:
            *c = QChar(ushort(qMetaTypeUNumber(d)));
            break;
        default:
            return false;
        }
        break;
    }
#ifndef QT_NO_GEOM_VARIANT
    case QVariant::Size: {
        QSize *s = static_cast<QSize *>(result);
        switch (d->type) {
        case QVariant::SizeF:
            *s = v_cast<QSizeF>(d)->toSize();
            break;
        default:
            return false;
        }
        break;
    }

    case QVariant::SizeF: {
        QSizeF *s = static_cast<QSizeF *>(result);
        switch (d->type) {
        case QVariant::Size:
            *s = QSizeF(*(v_cast<QSize>(d)));
            break;
        default:
            return false;
        }
        break;
    }

    case QVariant::Line: {
        QLine *s = static_cast<QLine *>(result);
        switch (d->type) {
        case QVariant::LineF:
            *s = v_cast<QLineF>(d)->toLine();
            break;
        default:
            return false;
        }
        break;
    }

    case QVariant::LineF: {
        QLineF *s = static_cast<QLineF *>(result);
        switch (d->type) {
        case QVariant::Line:
            *s = QLineF(*(v_cast<QLine>(d)));
            break;
        default:
            return false;
        }
        break;
    }
#endif
    case QVariant::StringList:
        if (d->type == QVariant::List) {
            QStringList *slst = static_cast<QStringList *>(result);
            const QVariantList *list = v_cast<QVariantList >(d);
            for (int i = 0; i < list->size(); ++i)
                slst->append(list->at(i).toString());
        } else if (d->type == QVariant::String) {
            QStringList *slst = static_cast<QStringList *>(result);
            *slst = QStringList(*v_cast<QString>(d));
        } else {
            return false;
        }
        break;
    case QVariant::Date: {
        QDate *dt = static_cast<QDate *>(result);
        if (d->type == QVariant::DateTime)
            *dt = v_cast<QDateTime>(d)->date();
#ifndef QT_NO_DATESTRING
        else if (d->type == QVariant::String)
            *dt = QDate::fromString(*v_cast<QString>(d), Qt::ISODate);
#endif
        else
            return false;

        return dt->isValid();
    }
    case QVariant::Time: {
        QTime *t = static_cast<QTime *>(result);
        switch (d->type) {
        case QVariant::DateTime:
            *t = v_cast<QDateTime>(d)->time();
            break;
#ifndef QT_NO_DATESTRING
        case QVariant::String:
            *t = QTime::fromString(*v_cast<QString>(d), Qt::ISODate);
            break;
#endif
        default:
            return false;
        }
        return t->isValid();
    }
    case QVariant::DateTime: {
        QDateTime *dt = static_cast<QDateTime *>(result);
        switch (d->type) {
#ifndef QT_NO_DATESTRING
        case QVariant::String:
            *dt = QDateTime::fromString(*v_cast<QString>(d), Qt::ISODate);
            break;
#endif
        case QVariant::Date:
            *dt = QDateTime(*v_cast<QDate>(d));
            break;
        default:
            return false;
        }
        return dt->isValid();
    }
    case QVariant::ByteArray: {
        QByteArray *ba = static_cast<QByteArray *>(result);
        switch (d->type) {
        case QVariant::String:
            *ba = v_cast<QString>(d)->toAscii();
            break;
        case QVariant::Double:
            *ba = QByteArray::number(d->data.d, 'g', DBL_DIG);
            break;
        case QMetaType::Float:
            *ba = QByteArray::number(d->data.f, 'g', FLT_DIG);
            break;
        case QMetaType::Char:
        case QMetaType::UChar:
            *ba = QByteArray(1, d->data.c);
            break;
        case QVariant::Int:
        case QVariant::LongLong:
        case QMetaType::Short:
        case QMetaType::Long:
            *ba = QByteArray::number(qMetaTypeNumber(d));
            break;
        case QVariant::UInt:
        case QVariant::ULongLong:
        case QMetaType::UShort:
        case QMetaType::ULong:
            *ba = QByteArray::number(qMetaTypeUNumber(d));
            break;
        case QVariant::Bool:
            *ba = QByteArray(d->data.b ? "true" : "false");
            break;
        default:
            return false;
        }
    }
    break;
    case QMetaType::Short:
        *static_cast<short *>(result) = short(qConvertToNumber(d, ok));
        return *ok;
    case QMetaType::Long:
        *static_cast<long *>(result) = long(qConvertToNumber(d, ok));
        return *ok;
    case QMetaType::UShort:
        *static_cast<ushort *>(result) = ushort(qConvertToUnsignedNumber(d, ok));
        return *ok;
    case QMetaType::ULong:
        *static_cast<ulong *>(result) = ulong(qConvertToUnsignedNumber(d, ok));
        return *ok;
    case QVariant::Int:
        *static_cast<int *>(result) = int(qConvertToNumber(d, ok));
        return *ok;
    case QVariant::UInt:
        *static_cast<uint *>(result) = uint(qConvertToUnsignedNumber(d, ok));
        return *ok;
    case QVariant::LongLong:
        *static_cast<qlonglong *>(result) = qConvertToNumber(d, ok);
        return *ok;
    case QVariant::ULongLong: {
        *static_cast<qulonglong *>(result) = qConvertToUnsignedNumber(d, ok);
        return *ok;
    }
    case QMetaType::UChar: {
        *static_cast<uchar *>(result) = qConvertToUnsignedNumber(d, ok);
        return *ok;
    }
    case QVariant::Bool: {
        bool *b = static_cast<bool *>(result);
        switch(d->type) {
        case QVariant::ByteArray:
            *b = qt_convertToBool<QByteArray, QByteArray>(d);
            break;
        case QVariant::String:
            *b = qt_convertToBool<QString, QLatin1String>(d);
            break;
        case QVariant::Char:
            *b = !v_cast<QChar>(d)->isNull();
            break;
        case QVariant::Double:
        case QVariant::Int:
        case QVariant::LongLong:
        case QMetaType::Char:
        case QMetaType::Short:
        case QMetaType::Long:
        case QMetaType::Float:
            *b = qMetaTypeNumber(d) != Q_INT64_C(0);
            break;
        case QVariant::UInt:
        case QVariant::ULongLong:
        case QMetaType::UChar:
        case QMetaType::UShort:
        case QMetaType::ULong:
            *b = qMetaTypeUNumber(d) != Q_UINT64_C(0);
            break;
        default:
            *b = false;
            return false;
        }
        break;
    }
    case QVariant::Double: {
        double *f = static_cast<double *>(result);
        switch (d->type) {
        case QVariant::String:
            *f = v_cast<QString>(d)->toDouble(ok);
            break;
        case QVariant::ByteArray:
            *f = v_cast<QByteArray>(d)->toDouble(ok);
            break;
        case QVariant::Bool:
            *f = double(d->data.b);
            break;
        case QMetaType::Float:
            *f = double(d->data.f);
            break;
        case QVariant::LongLong:
        case QVariant::Int:
        case QMetaType::Char:
        case QMetaType::Short:
        case QMetaType::Long:
            *f = double(qMetaTypeNumber(d));
            break;
        case QVariant::UInt:
        case QVariant::ULongLong:
        case QMetaType::UChar:
        case QMetaType::UShort:
        case QMetaType::ULong:
            *f = double(qMetaTypeUNumber(d));
            break;
        default:
            *f = 0.0;
            return false;
        }
        break;
    }
    case QMetaType::Float: {
        float *f = static_cast<float *>(result);
        switch (d->type) {
        case QVariant::String:
            *f = v_cast<QString>(d)->toFloat(ok);
            break;
        case QVariant::ByteArray:
            *f = v_cast<QByteArray>(d)->toFloat(ok);
            break;
        case QVariant::Bool:
            *f = float(d->data.b);
            break;
        case QVariant::Double:
            *f = float(d->data.d);
            break;
        case QVariant::LongLong:
        case QVariant::Int:
        case QMetaType::Char:
        case QMetaType::Short:
        case QMetaType::Long:
            *f = float(qMetaTypeNumber(d));
            break;
        case QVariant::UInt:
        case QVariant::ULongLong:
        case QMetaType::UChar:
        case QMetaType::UShort:
        case QMetaType::ULong:
            *f = float(qMetaTypeUNumber(d));
            break;
        default:
            *f = 0.0f;
            return false;
        }
        break;
    }
    case QVariant::List:
        if (d->type == QVariant::StringList) {
            QVariantList *lst = static_cast<QVariantList *>(result);
            const QStringList *slist = v_cast<QStringList>(d);
            for (int i = 0; i < slist->size(); ++i)
                lst->append(QVariant(slist->at(i)));
        } else if (qstrcmp(QMetaType::typeName(d->type), "QList<QVariant>") == 0) {
            *static_cast<QVariantList *>(result) =
                *static_cast<QList<QVariant> *>(d->data.shared->ptr);
        } else {
            return false;
        }
        break;
    case QVariant::Map:
        if (qstrcmp(QMetaType::typeName(d->type), "QMap<QString, QVariant>") == 0) {
            *static_cast<QVariantMap *>(result) =
                *static_cast<QMap<QString, QVariant> *>(d->data.shared->ptr);
        } else {
            return false;
        }
        break;
    case QVariant::Hash:
        if (qstrcmp(QMetaType::typeName(d->type), "QHash<QString, QVariant>") == 0) {
            *static_cast<QVariantHash *>(result) =
                *static_cast<QHash<QString, QVariant> *>(d->data.shared->ptr);
        } else {
            return false;
        }
        break;
#ifndef QT_NO_GEOM_VARIANT
    case QVariant::Rect:
        if (d->type == QVariant::RectF)
            *static_cast<QRect *>(result) = (v_cast<QRectF>(d))->toRect();
        else
            return false;
        break;
    case QVariant::RectF:
        if (d->type == QVariant::Rect)
            *static_cast<QRectF *>(result) = *v_cast<QRect>(d);
        else
            return false;
        break;
    case QVariant::PointF:
        if (d->type == QVariant::Point)
            *static_cast<QPointF *>(result) = *v_cast<QPoint>(d);
        else
            return false;
        break;
    case QVariant::Point:
        if (d->type == QVariant::PointF)
            *static_cast<QPoint *>(result) = (v_cast<QPointF>(d))->toPoint();
        else
            return false;
        break;
    case QMetaType::Char:
    {
        *static_cast<qint8 *>(result) = qint8(qConvertToNumber(d, ok));
        return *ok;
    }
#endif
    case QVariant::Uuid:
        switch (d->type) {
        case QVariant::String:
            *static_cast<QUuid *>(result) = QUuid(*v_cast<QString>(d));
            break;
        default:
            return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

#if !defined(QT_NO_DEBUG_STREAM)
static void streamDebug(QDebug dbg, const QVariant &v)
{
    QVariant::Private *d = const_cast<QVariant::Private *>(&v.data_ptr());
    QVariantDebugStream<CoreTypesFilter> stream(dbg, d);
    QMetaTypeSwitcher::switcher<void>(stream, d->type, 0);
}
#endif

const QVariant::Handler qt_kernel_variant_handler = {
    construct,
    clear,
    isNull,
#ifndef QT_NO_DATASTREAM
    0,
    0,
#endif
    compare,
    convert,
    0,
#if !defined(QT_NO_DEBUG_STREAM)
    streamDebug
#else
    0
#endif
};

static void dummyConstruct(QVariant::Private *, const void *) { Q_ASSERT_X(false, "QVariant", "Trying to construct an unknown type"); }
static void dummyClear(QVariant::Private *) { Q_ASSERT_X(false, "QVariant", "Trying to clear an unknown type"); }
static bool dummyIsNull(const QVariant::Private *d) { Q_ASSERT_X(false, "QVariant::isNull", "Trying to call isNull on an unknown type"); return d->is_null; }
static bool dummyCompare(const QVariant::Private *, const QVariant::Private *) { Q_ASSERT_X(false, "QVariant", "Trying to compare an unknown types"); return false; }
static bool dummyConvert(const QVariant::Private *, int, void *, bool *) { Q_ASSERT_X(false, "QVariant", "Trying to convert an unknown type"); return false; }
#if !defined(QT_NO_DEBUG_STREAM)
static void dummyStreamDebug(QDebug, const QVariant &) { Q_ASSERT_X(false, "QVariant", "Trying to convert an unknown type"); }
#endif
const QVariant::Handler qt_dummy_variant_handler = {
    dummyConstruct,
    dummyClear,
    dummyIsNull,
#ifndef QT_NO_DATASTREAM
    0,
    0,
#endif
    dummyCompare,
    dummyConvert,
    0,
#if !defined(QT_NO_DEBUG_STREAM)
    dummyStreamDebug
#else
    0
#endif
};

static void customConstruct(QVariant::Private *d, const void *copy)
{
    const QMetaType type(d->type);
    const uint size = type.sizeOf();
    if (!size) {
        d->type = QVariant::Invalid;
        return;
    }

    // this logic should match with QVariantIntegrator::CanUseInternalSpace
    if (size <= sizeof(QVariant::Private::Data)
            && (type.flags() & QMetaType::MovableType)) {
        type.construct(&d->data.ptr, copy);
        d->is_shared = false;
    } else {
        void *ptr = type.create(copy);
        d->is_shared = true;
        d->data.shared = new QVariant::PrivateShared(ptr);
    }
}

static void customClear(QVariant::Private *d)
{
    if (!d->is_shared) {
        QMetaType::destruct(d->type, &d->data.ptr);
    } else {
        QMetaType::destroy(d->type, d->data.shared->ptr);
        delete d->data.shared;
    }
}

static bool customIsNull(const QVariant::Private *d)
{
    return d->is_null;
}

static bool customCompare(const QVariant::Private *a, const QVariant::Private *b)
{
    const char *const typeName = QMetaType::typeName(a->type);
    if (Q_UNLIKELY(!typeName) && Q_LIKELY(!QMetaType::isRegistered(a->type)))
        qFatal("QVariant::compare: type %d unknown to QVariant.", a->type);

    const void *a_ptr = a->is_shared ? a->data.shared->ptr : &(a->data.ptr);
    const void *b_ptr = b->is_shared ? b->data.shared->ptr : &(b->data.ptr);

    uint typeNameLen = qstrlen(typeName);
    if (typeNameLen > 0 && typeName[typeNameLen - 1] == '*')
        return *static_cast<void *const *>(a_ptr) == *static_cast<void *const *>(b_ptr);

    if (a->is_null && b->is_null)
        return true;

    return !memcmp(a_ptr, b_ptr, QMetaType::sizeOf(a->type));
}

static bool customConvert(const QVariant::Private *, int, void *, bool *ok)
{
    if (ok)
        *ok = false;
    return false;
}

#if !defined(QT_NO_DEBUG_STREAM)
static void customStreamDebug(QDebug, const QVariant &) {}
#endif

const QVariant::Handler qt_custom_variant_handler = {
    customConstruct,
    customClear,
    customIsNull,
#ifndef QT_NO_DATASTREAM
    0,
    0,
#endif
    customCompare,
    customConvert,
    0,
#if !defined(QT_NO_DEBUG_STREAM)
    customStreamDebug
#else
    0
#endif
};

} // annonymous used to hide QVariant handlers

static HandlersManager handlerManager;
Q_STATIC_ASSERT_X(!QModulesPrivate::Core, "Initialization assumes that ModulesNames::Core is 0");
const QVariant::Handler *HandlersManager::Handlers[QModulesPrivate::ModulesCount]
                                        = { &qt_kernel_variant_handler, &qt_dummy_variant_handler,
                                            &qt_dummy_variant_handler, &qt_custom_variant_handler };

Q_CORE_EXPORT const QVariant::Handler *qcoreVariantHandler()
{
    return &qt_kernel_variant_handler;
}

inline void HandlersManager::unregisterHandler(const QModulesPrivate::Names name)
{
    Handlers[name] = &qt_dummy_variant_handler;
}

Q_CORE_EXPORT void QVariantPrivate::registerHandler(const int /* Modules::Names */name, const QVariant::Handler *handler)
{
    handlerManager.registerHandler(static_cast<QModulesPrivate::Names>(name), handler);
}

Q_CORE_EXPORT void QVariantPrivate::unregisterHandler(const int /* Modules::Names */ name)
{
    handlerManager.unregisterHandler(static_cast<QModulesPrivate::Names>(name));
}

/*!
    \class QVariant
    \brief The QVariant class acts like a union for the most common Qt data types.

    \ingroup objectmodel
    \ingroup shared


    Because C++ forbids unions from including types that have
    non-default constructors or destructors, most interesting Qt
    classes cannot be used in unions. Without QVariant, this would be
    a problem for QObject::property() and for database work, etc.

    A QVariant object holds a single value of a single type() at a
    time. (Some type()s are multi-valued, for example a string list.)
    You can find out what type, T, the variant holds, convert it to a
    different type using convert(), get its value using one of the
    toT() functions (e.g., toSize()) and check whether the type can
    be converted to a particular type using canConvert().

    The methods named toT() (e.g., toInt(), toString()) are const. If
    you ask for the stored type, they return a copy of the stored
    object. If you ask for a type that can be generated from the
    stored type, toT() copies and converts and leaves the object
    itself unchanged. If you ask for a type that cannot be generated
    from the stored type, the result depends on the type; see the
    function documentation for details.

    Here is some example code to demonstrate the use of QVariant:

    \snippet doc/src/snippets/code/src_corelib_kernel_qvariant.cpp 0

    You can even store QList<QVariant> and QMap<QString, QVariant>
    values in a variant, so you can easily construct arbitrarily
    complex data structures of arbitrary types. This is very powerful
    and versatile, but may prove less memory and speed efficient than
    storing specific types in standard data structures.

    QVariant also supports the notion of null values, where you can
    have a defined type with no value set. However, note that QVariant
    types can only be cast when they have had a value set.

    \snippet doc/src/snippets/code/src_corelib_kernel_qvariant.cpp 1

    QVariant can be extended to support other types than those
    mentioned in the \l Type enum. See the \l QMetaType documentation
    for details.

    \section1 A Note on GUI Types

    Because QVariant is part of the QtCore library, it cannot provide
    conversion functions to data types defined in QtGui, such as
    QColor, QImage, and QPixmap. In other words, there is no \c
    toColor() function. Instead, you can use the QVariant::value() or
    the qvariant_cast() template function. For example:

    \snippet doc/src/snippets/code/src_corelib_kernel_qvariant.cpp 2

    The inverse conversion (e.g., from QColor to QVariant) is
    automatic for all data types supported by QVariant, including
    GUI-related types:

    \snippet doc/src/snippets/code/src_corelib_kernel_qvariant.cpp 3

    \section1 Using canConvert() and convert() Consecutively

    When using canConvert() and convert() consecutively, it is possible for
    canConvert() to return true, but convert() to return false. This
    is typically because canConvert() only reports the general ability of
    QVariant to convert between types given suitable data; it is still
    possible to supply data which cannot actually be converted.

    For example, canConvert() would return true when called on a variant
    containing a string because, in principle, QVariant is able to convert
    strings of numbers to integers.
    However, if the string contains non-numeric characters, it cannot be
    converted to an integer, and any attempt to convert it will fail.
    Hence, it is important to have both functions return true for a
    successful conversion.

    \sa QMetaType
*/

/*!
    \obsolete Use QMetaType::Type instead
    \enum QVariant::Type

    This enum type defines the types of variable that a QVariant can
    contain.

    \value Invalid  no type
    \value BitArray  a QBitArray
    \value Bitmap  a QBitmap
    \value Bool  a bool
    \value Brush  a QBrush
    \value ByteArray  a QByteArray
    \value Char  a QChar
    \value Color  a QColor
    \value Cursor  a QCursor
    \value Date  a QDate
    \value DateTime  a QDateTime
    \value Double  a double
    \value EasingCurve a QEasingCurve
    \value Uuid a QUuid
    \value ModelIndex a QModelIndex
    \value Font  a QFont
    \value Hash a QVariantHash
    \value Icon  a QIcon
    \value Image  a QImage
    \value Int  an int
    \value KeySequence  a QKeySequence
    \value Line  a QLine
    \value LineF  a QLineF
    \value List  a QVariantList
    \value Locale  a QLocale
    \value LongLong a \l qlonglong
    \value Map  a QVariantMap
    \value Matrix  a QMatrix
    \value Transform  a QTransform
    \value Matrix4x4  a QMatrix4x4
    \value Palette  a QPalette
    \value Pen  a QPen
    \value Pixmap  a QPixmap
    \value Point  a QPoint
    \value PointF  a QPointF
    \value Polygon a QPolygon
    \value PolygonF a QPolygonF
    \value Quaternion  a QQuaternion
    \value Rect  a QRect
    \value RectF  a QRectF
    \value RegExp  a QRegExp
    \value Region  a QRegion
    \value Size  a QSize
    \value SizeF  a QSizeF
    \value SizePolicy  a QSizePolicy
    \value String  a QString
    \value StringList  a QStringList
    \value TextFormat  a QTextFormat
    \value TextLength  a QTextLength
    \value Time  a QTime
    \value UInt  a \l uint
    \value ULongLong a \l qulonglong
    \value Url  a QUrl
    \value Vector2D  a QVector2D
    \value Vector3D  a QVector3D
    \value Vector4D  a QVector4D

    \value UserType Base value for user-defined types.

    \omitvalue CString
    \omitvalue ColorGroup
    \omitvalue IconSet
    \omitvalue LastGuiType
    \omitvalue LastCoreType
    \omitvalue LastType
*/

/*!
    \fn QVariant::QVariant()

    Constructs an invalid variant.
*/


/*!
    \fn QVariant::QVariant(int typeId, const void *copy)

    Constructs variant of type \a typeId, and initializes with
    \a copy if \a copy is not 0.

    Note that you have to pass the address of the variable you want stored.

    Usually, you never have to use this constructor, use QVariant::fromValue()
    instead to construct variants from the pointer types represented by
    \c QMetaType::VoidStar, \c QMetaType::QObjectStar and
    \c QMetaType::QWidgetStar.

    \sa QVariant::fromValue(), Type
*/

/*!
    \fn QVariant::QVariant(Type type)

    Constructs a null variant of type \a type.
*/



/*!
    \fn QVariant::create(int type, const void *copy)

    \internal

    Constructs a variant private of type \a type, and initializes with \a copy if
    \a copy is not 0.
*/

void QVariant::create(int type, const void *copy)
{
    d.type = type;
    handlerManager[type]->construct(&d, copy);
}

/*!
    \fn QVariant::~QVariant()

    Destroys the QVariant and the contained object.

    Note that subclasses that reimplement clear() should reimplement
    the destructor to call clear(). This destructor calls clear(), but
    because it is the destructor, QVariant::clear() is called rather
    than a subclass's clear().
*/

QVariant::~QVariant()
{
    if ((d.is_shared && !d.data.shared->ref.deref()) || (!d.is_shared && d.type > Char))
        handlerManager[d.type]->clear(&d);
}

/*!
  \fn QVariant::QVariant(const QVariant &p)

    Constructs a copy of the variant, \a p, passed as the argument to
    this constructor.
*/

QVariant::QVariant(const QVariant &p)
    : d(p.d)
{
    if (d.is_shared) {
        d.data.shared->ref.ref();
    } else if (p.d.type > Char) {
        handlerManager[d.type]->construct(&d, p.constData());
        d.is_null = p.d.is_null;
    }
}

#ifndef QT_NO_DATASTREAM
/*!
    Reads the variant from the data stream, \a s.
*/
QVariant::QVariant(QDataStream &s)
{
    d.is_null = true;
    s >> *this;
}
#endif //QT_NO_DATASTREAM

/*!
  \fn QVariant::QVariant(const QString &val)

    Constructs a new variant with a string value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QLatin1String &val)

    Constructs a new variant with a string value, \a val.
*/

/*!
  \fn QVariant::QVariant(const char *val)

    Constructs a new variant with a string value of \a val.
    The variant creates a deep copy of \a val into a QString assuming
    UTF-8 encoding on the input \a val.

    Note that \a val is converted to a QString for storing in the
    variant and QVariant::userType() will return QMetaType::QString for
    the variant.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications.
*/

#ifndef QT_NO_CAST_FROM_ASCII
QVariant::QVariant(const char *val)
{
    QString s = QString::fromAscii(val);
    create(String, &s);
}
#endif

/*!
  \fn QVariant::QVariant(const QStringList &val)

    Constructs a new variant with a string list value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QMap<QString, QVariant> &val)

    Constructs a new variant with a map of QVariants, \a val.
*/

/*!
  \fn QVariant::QVariant(const QHash<QString, QVariant> &val)

    Constructs a new variant with a hash of QVariants, \a val.
*/

/*!
  \fn QVariant::QVariant(const QDate &val)

    Constructs a new variant with a date value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QTime &val)

    Constructs a new variant with a time value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QDateTime &val)

    Constructs a new variant with a date/time value, \a val.
*/

/*!
    \since 4.7
  \fn QVariant::QVariant(const QEasingCurve &val)

    Constructs a new variant with an easing curve value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QByteArray &val)

    Constructs a new variant with a bytearray value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QBitArray &val)

    Constructs a new variant with a bitarray value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QPoint &val)

  Constructs a new variant with a point value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QPointF &val)

  Constructs a new variant with a point value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QRectF &val)

  Constructs a new variant with a rect value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QLineF &val)

  Constructs a new variant with a line value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QLine &val)

  Constructs a new variant with a line value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QRect &val)

  Constructs a new variant with a rect value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QSize &val)

  Constructs a new variant with a size value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QSizeF &val)

  Constructs a new variant with a size value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QUrl &val)

  Constructs a new variant with a url value of \a val.
 */

/*!
  \fn QVariant::QVariant(int val)

    Constructs a new variant with an integer value, \a val.
*/

/*!
  \fn QVariant::QVariant(uint val)

    Constructs a new variant with an unsigned integer value, \a val.
*/

/*!
  \fn QVariant::QVariant(qlonglong val)

    Constructs a new variant with a long long integer value, \a val.
*/

/*!
  \fn QVariant::QVariant(qulonglong val)

    Constructs a new variant with an unsigned long long integer value, \a val.
*/


/*!
  \fn QVariant::QVariant(bool val)

    Constructs a new variant with a boolean value, \a val.
*/

/*!
  \fn QVariant::QVariant(double val)

    Constructs a new variant with a floating point value, \a val.
*/

/*!
  \fn QVariant::QVariant(float val)

    Constructs a new variant with a floating point value, \a val.
    \since 4.6
*/

/*!
    \fn QVariant::QVariant(const QList<QVariant> &val)

    Constructs a new variant with a list value, \a val.
*/

/*!
  \fn QVariant::QVariant(QChar c)

  Constructs a new variant with a char value, \a c.
*/

/*!
  \fn QVariant::QVariant(const QLocale &l)

  Constructs a new variant with a locale value, \a l.
*/

/*!
  \fn QVariant::QVariant(const QRegExp &regExp)

  Constructs a new variant with the regexp value \a regExp.
*/

/*! \since 4.2
  \fn QVariant::QVariant(Qt::GlobalColor color)

  Constructs a new variant of type QVariant::Color and initializes
  it with \a color.

  This is a convenience constructor that allows \c{QVariant(Qt::blue);}
  to create a valid QVariant storing a QColor.

  Note: This constructor will assert if the application does not link
  to the Qt GUI library.
 */

QVariant::QVariant(Type type)
{ create(type, 0); }
QVariant::QVariant(int typeId, const void *copy)
{ create(typeId, copy); d.is_null = false; }

/*! \internal
    flags is true if it is a pointer type
 */
QVariant::QVariant(int typeId, const void *copy, uint flags)
{
    if (flags) { //type is a pointer type
        d.type = typeId;
        d.data.ptr = *reinterpret_cast<void *const*>(copy);
    } else {
        create(typeId, copy);
    }
    d.is_null = false;
}

QVariant::QVariant(int val)
{ d.is_null = false; d.type = Int; d.data.i = val; }
QVariant::QVariant(uint val)
{ d.is_null = false; d.type = UInt; d.data.u = val; }
QVariant::QVariant(qlonglong val)
{ d.is_null = false; d.type = LongLong; d.data.ll = val; }
QVariant::QVariant(qulonglong val)
{ d.is_null = false; d.type = ULongLong; d.data.ull = val; }
QVariant::QVariant(bool val)
{ d.is_null = false; d.type = Bool; d.data.b = val; }
QVariant::QVariant(double val)
{ d.is_null = false; d.type = Double; d.data.d = val; }

QVariant::QVariant(const QByteArray &val)
{ d.is_null = false; d.type = ByteArray; v_construct<QByteArray>(&d, val); }
QVariant::QVariant(const QBitArray &val)
{ d.is_null = false; d.type = BitArray; v_construct<QBitArray>(&d, val);  }
QVariant::QVariant(const QString &val)
{ d.is_null = false; d.type = String; v_construct<QString>(&d, val);  }
QVariant::QVariant(QChar val)
{ d.is_null = false; d.type = Char; v_construct<QChar>(&d, val);  }
QVariant::QVariant(const QLatin1String &val)
{ QString str(val); d.is_null = false; d.type = String; v_construct<QString>(&d, str); }
QVariant::QVariant(const QStringList &val)
{ d.is_null = false; d.type = StringList; v_construct<QStringList>(&d, val); }

QVariant::QVariant(const QDate &val)
{ d.is_null = false; d.type = Date; v_construct<QDate>(&d, val); }
QVariant::QVariant(const QTime &val)
{ d.is_null = false; d.type = Time; v_construct<QTime>(&d, val); }
QVariant::QVariant(const QDateTime &val)
{ d.is_null = false; d.type = DateTime; v_construct<QDateTime>(&d, val); }
#ifndef QT_BOOTSTRAPPED
QVariant::QVariant(const QEasingCurve &val)
{ d.is_null = false; d.type = EasingCurve; v_construct<QEasingCurve>(&d, val); }
#endif
QVariant::QVariant(const QList<QVariant> &list)
{ d.is_null = false; d.type = List; v_construct<QVariantList>(&d, list); }
QVariant::QVariant(const QMap<QString, QVariant> &map)
{ d.is_null = false; d.type = Map; v_construct<QVariantMap>(&d, map); }
QVariant::QVariant(const QHash<QString, QVariant> &hash)
{ d.is_null = false; d.type = Hash; v_construct<QVariantHash>(&d, hash); }
#ifndef QT_NO_GEOM_VARIANT
QVariant::QVariant(const QPoint &pt) { d.is_null = false; d.type = Point; v_construct<QPoint>(&d, pt); }
QVariant::QVariant(const QPointF &pt) { d.is_null = false; d.type = PointF; v_construct<QPointF>(&d, pt); }
QVariant::QVariant(const QRectF &r) { d.is_null = false; d.type = RectF; v_construct<QRectF>(&d, r); }
QVariant::QVariant(const QLineF &l) { d.is_null = false; d.type = LineF; v_construct<QLineF>(&d, l); }
QVariant::QVariant(const QLine &l) { d.is_null = false; d.type = Line; v_construct<QLine>(&d, l); }
QVariant::QVariant(const QRect &r) { d.is_null = false; d.type = Rect; v_construct<QRect>(&d, r); }
QVariant::QVariant(const QSize &s) { d.is_null = false; d.type = Size; v_construct<QSize>(&d, s); }
QVariant::QVariant(const QSizeF &s) { d.is_null = false; d.type = SizeF; v_construct<QSizeF>(&d, s); }
#endif
QVariant::QVariant(const QUrl &u) { d.is_null = false; d.type = Url; v_construct<QUrl>(&d, u); }
QVariant::QVariant(const QLocale &l) { d.is_null = false; d.type = Locale; v_construct<QLocale>(&d, l); }
#ifndef QT_NO_REGEXP
QVariant::QVariant(const QRegExp &regExp) { d.is_null = false; d.type = RegExp; v_construct<QRegExp>(&d, regExp); }
#endif
QVariant::QVariant(Qt::GlobalColor color) { create(62, &color); }

/*!
    Returns the storage type of the value stored in the variant.
    Although this function is declared as returning QVariant::Type,
    the return value should be interpreted as QMetaType::Type. In
    particular, QVariant::UserType is returned here only if the value
    is equal or greater than QMetaType::User.

    Note that return values in the ranges QVariant::Char through
    QVariant::RegExp and QVariant::Font through QVariant::Transform
    correspond to the values in the ranges QMetaType::QChar through
    QMetaType::QRegExp and QMetaType::QFont through QMetaType::QQuaternion.

    Pay particular attention when working with char and QChar
    variants.  Note that there is no QVariant constructor specifically
    for type char, but there is one for QChar. For a variant of type
    QChar, this function returns QVariant::Char, which is the same as
    QMetaType::QChar, but for a variant of type \c char, this function
    returns QMetaType::Char, which is \e not the same as
    QVariant::Char.

    Also note that the types \c void*, \c long, \c short, \c unsigned
    \c long, \c unsigned \c short, \c unsigned \c char, \c float, \c
    QObject*, and \c QWidget* are represented in QMetaType::Type but
    not in QVariant::Type, and they can be returned by this function.
    However, they are considered to be user defined types when tested
    against QVariant::Type.

    To test whether an instance of QVariant contains a data type that
    is compatible with the data type you are interested in, use
    canConvert().
*/

QVariant::Type QVariant::type() const
{
    return d.type >= QMetaType::User ? UserType : static_cast<Type>(d.type);
}

/*!
    Returns the storage type of the value stored in the variant. For
    non-user types, this is the same as type().

    \sa type()
*/

int QVariant::userType() const
{
    return d.type;
}

/*!
    Assigns the value of the variant \a variant to this variant.
*/
QVariant& QVariant::operator=(const QVariant &variant)
{
    if (this == &variant)
        return *this;

    clear();
    if (variant.d.is_shared) {
        variant.d.data.shared->ref.ref();
        d = variant.d;
    } else if (variant.d.type > Char) {
        d.type = variant.d.type;
        handlerManager[d.type]->construct(&d, variant.constData());
        d.is_null = variant.d.is_null;
    } else {
        d = variant.d;
    }

    return *this;
}

/*!
    \fn void QVariant::swap(QVariant &other)
    \since 4.8

    Swaps variant \a other with this variant. This operation is very
    fast and never fails.
*/

/*!
    \fn void QVariant::detach()

    \internal
*/

void QVariant::detach()
{
    if (!d.is_shared || d.data.shared->ref.load() == 1)
        return;

    Private dd;
    dd.type = d.type;
    handlerManager[d.type]->construct(&dd, constData());
    if (!d.data.shared->ref.deref())
        handlerManager[d.type]->clear(&d);
    d.data.shared = dd.data.shared;
}

/*!
    \fn bool QVariant::isDetached() const

    \internal
*/

// ### Qt 5: change typeName()(and froends= to return a QString. Suggestion from Harald.
/*!
    Returns the name of the type stored in the variant. The returned
    strings describe the C++ datatype used to store the data: for
    example, "QFont", "QString", or "QVariantList". An Invalid
    variant returns 0.
*/
const char *QVariant::typeName() const
{
    return typeToName(d.type);
}

/*!
    Convert this variant to type Invalid and free up any resources
    used.
*/
void QVariant::clear()
{
    if ((d.is_shared && !d.data.shared->ref.deref()) || (!d.is_shared && d.type > Char))
        handlerManager[d.type]->clear(&d);
    d.type = Invalid;
    d.is_null = true;
    d.is_shared = false;
}

/*!
    Converts the int representation of the storage type, \a typeId, to
    its string representation.

    Returns a null pointer if the type is QVariant::Invalid or doesn't exist.
*/
const char *QVariant::typeToName(int typeId)
{
    if (typeId == Invalid)
        return 0;

    return QMetaType::typeName(typeId);
}


/*!
    Converts the string representation of the storage type given in \a
    name, to its enum representation.

    If the string representation cannot be converted to any enum
    representation, the variant is set to \c Invalid.
*/
QVariant::Type QVariant::nameToType(const char *name)
{
    if (!name || !*name)
        return Invalid;

    int metaType = QMetaType::type(name);
    return metaType <= int(UserType) ? QVariant::Type(metaType) : UserType;
}

#ifndef QT_NO_DATASTREAM
enum { MapFromThreeCount = 36 };
static const ushort mapIdFromQt3ToCurrent[MapFromThreeCount] =
{
    QVariant::Invalid,
    QVariant::Map,
    QVariant::List,
    QVariant::String,
    QVariant::StringList,
    QVariant::Font,
    QVariant::Pixmap,
    QVariant::Brush,
    QVariant::Rect,
    QVariant::Size,
    QVariant::Color,
    QVariant::Palette,
    0, // ColorGroup
    QVariant::Icon,
    QVariant::Point,
    QVariant::Image,
    QVariant::Int,
    QVariant::UInt,
    QVariant::Bool,
    QVariant::Double,
    QVariant::ByteArray,
    QVariant::Polygon,
    QVariant::Region,
    QVariant::Bitmap,
    QVariant::Cursor,
    QVariant::SizePolicy,
    QVariant::Date,
    QVariant::Time,
    QVariant::DateTime,
    QVariant::ByteArray,
    QVariant::BitArray,
    QVariant::KeySequence,
    QVariant::Pen,
    QVariant::LongLong,
    QVariant::ULongLong,
    QVariant::EasingCurve
};

/*!
    Internal function for loading a variant from stream \a s. Use the
    stream operators instead.

    \internal
*/
void QVariant::load(QDataStream &s)
{
    clear();

    quint32 typeId;
    s >> typeId;
    if (s.version() < QDataStream::Qt_4_0) {
        if (typeId >= MapFromThreeCount)
            return;
        typeId = mapIdFromQt3ToCurrent[typeId];
    } else if (s.version() < QDataStream::Qt_5_0) {
        if (typeId == 127 /* QVariant::UserType */) {
            typeId = QMetaType::User;
        } else if (typeId >= 128 && typeId != QVariant::UserType) {
            // In Qt4 id == 128 was FirstExtCoreType. In Qt5 ExtCoreTypes set was merged to CoreTypes
            // by moving all ids down by 97.
            typeId -= 97;
        } else if (typeId == 69 /* QIcon */) {
            // In Qt5 after modularization project this types where moved to a separate module (and ids were downgraded)
            typeId = QMetaType::QIcon;
        } else if (typeId == 75 /* QSizePolicy */) {
            typeId = QMetaType::QSizePolicy;
        } else if (typeId >= 70) {
            // and as a result this types recieved lower ids too
            if (typeId <= 74) { // QImage QPolygon QRegion QBitmap QCursor
                typeId -=1;
            } else if (typeId <= 86) { // QKeySequence QPen QTextLength QTextFormat QMatrix QTransform QMatrix4x4 QVector2D QVector3D QVector4D QQuaternion
                typeId -=2;
            }
        }
    }

    qint8 is_null = false;
    if (s.version() >= QDataStream::Qt_4_2)
        s >> is_null;
    if (typeId == QVariant::UserType) {
        QByteArray name;
        s >> name;
        typeId = QMetaType::type(name);
        if (typeId == QMetaType::UnknownType) {
            s.setStatus(QDataStream::ReadCorruptData);
            return;
        }
    }
    create(static_cast<int>(typeId), 0);
    d.is_null = is_null;

    if (!isValid()) {
        // Since we wrote something, we should read something
        QString x;
        s >> x;
        d.is_null = true;
        return;
    }

    // const cast is safe since we operate on a newly constructed variant
    if (!QMetaType::load(s, d.type, const_cast<void *>(constData()))) {
        s.setStatus(QDataStream::ReadCorruptData);
        qWarning("QVariant::load: unable to load type %d.", d.type);
    }
}

/*!
    Internal function for saving a variant to the stream \a s. Use the
    stream operators instead.

    \internal
*/
void QVariant::save(QDataStream &s) const
{
    quint32 typeId = type();
    if (s.version() < QDataStream::Qt_4_0) {
        int i;
        for (i = MapFromThreeCount - 1; i >= 0; i--) {
            if (mapIdFromQt3ToCurrent[i] == typeId) {
                typeId = i;
                break;
            }
        }
        if (i == -1) {
            s << QVariant();
            return;
        }
    } else if (s.version() < QDataStream::Qt_5_0) {
        if (typeId == QMetaType::User) {
            typeId = 127; // QVariant::UserType had this value in Qt4
        } else if (typeId >= 128 - 97 && typeId <= LastCoreType) {
            // In Qt4 id == 128 was FirstExtCoreType. In Qt5 ExtCoreTypes set was merged to CoreTypes
            // by moving all ids down by 97.
            typeId += 97;
        } else if (typeId == QMetaType::QIcon) {
            // In Qt5 after modularization project this types where moved to a separate module (and ids were downgraded)
            typeId = 69;
        } else if (typeId == QMetaType::QSizePolicy) {
            typeId = 75;
        } else if (typeId >= QMetaType::QImage) {
            // and as a result this types recieved lower ids too
            if (typeId <= QMetaType::QCursor) {
                typeId +=1;
            } else if (typeId <= QMetaType::QQuaternion) {
                typeId +=2;
            }
        }
    }
    s << typeId;
    if (s.version() >= QDataStream::Qt_4_2)
        s << qint8(d.is_null);
    if (d.type >= QVariant::UserType) {
        s << QMetaType::typeName(userType());
    }

    if (!isValid()) {
        s << QString();
        return;
    }

    if (!QMetaType::save(s, d.type, constData())) {
        qWarning("QVariant::save: unable to save type '%s' (type id: %d).\n", QMetaType::typeName(d.type), d.type);
        Q_ASSERT_X(false, "QVariant::save", "Invalid type to save");
    }
}

/*!
    \since 4.4

    Reads a variant \a p from the stream \a s.

    \sa \link datastreamformat.html Format of the QDataStream
    operators \endlink
*/
QDataStream& operator>>(QDataStream &s, QVariant &p)
{
    p.load(s);
    return s;
}

/*!
    Writes a variant \a p to the stream \a s.

    \sa \link datastreamformat.html Format of the QDataStream
    operators \endlink
*/
QDataStream& operator<<(QDataStream &s, const QVariant &p)
{
    p.save(s);
    return s;
}

/*!
    Reads a variant type \a p in enum representation from the stream \a s.
*/
QDataStream& operator>>(QDataStream &s, QVariant::Type &p)
{
    quint32 u;
    s >> u;
    p = (QVariant::Type)u;

    return s;
}

/*!
    Writes a variant type \a p to the stream \a s.
*/
QDataStream& operator<<(QDataStream &s, const QVariant::Type p)
{
    s << static_cast<quint32>(p);

    return s;
}

#endif //QT_NO_DATASTREAM

/*!
    \fn bool QVariant::isValid() const

    Returns true if the storage type of this variant is not
    QVariant::Invalid; otherwise returns false.
*/

template <typename T>
inline T qVariantToHelper(const QVariant::Private &d, const HandlersManager &handlerManager)
{
    const uint targetType = qMetaTypeId<T>();
    if (d.type == targetType)
        return *v_cast<T>(&d);

    T ret;
    handlerManager[d.type]->convert(&d, targetType, &ret, 0);
    return ret;
}

/*!
    \fn QStringList QVariant::toStringList() const

    Returns the variant as a QStringList if the variant has type()
    StringList, \l String, or \l List of a type that can be converted
    to QString; otherwise returns an empty list.

    \sa canConvert(), convert()
*/
QStringList QVariant::toStringList() const
{
    return qVariantToHelper<QStringList>(d, handlerManager);
}

/*!
    Returns the variant as a QString if the variant has type() \l
    String, \l Bool, \l ByteArray, \l Char, \l Date, \l DateTime, \l
    Double, \l Int, \l LongLong, \l StringList, \l Time, \l UInt, or
    \l ULongLong; otherwise returns an empty string.

    \sa canConvert(), convert()
*/
QString QVariant::toString() const
{
    return qVariantToHelper<QString>(d, handlerManager);
}

/*!
    Returns the variant as a QMap<QString, QVariant> if the variant
    has type() \l Map; otherwise returns an empty map.

    \sa canConvert(), convert()
*/
QVariantMap QVariant::toMap() const
{
    return qVariantToHelper<QVariantMap>(d, handlerManager);
}

/*!
    Returns the variant as a QHash<QString, QVariant> if the variant
    has type() \l Hash; otherwise returns an empty map.

    \sa canConvert(), convert()
*/
QVariantHash QVariant::toHash() const
{
    return qVariantToHelper<QVariantHash>(d, handlerManager);
}

/*!
    \fn QDate QVariant::toDate() const

    Returns the variant as a QDate if the variant has type() \l Date,
    \l DateTime, or \l String; otherwise returns an invalid date.

    If the type() is \l String, an invalid date will be returned if the
    string cannot be parsed as a Qt::ISODate format date.

    \sa canConvert(), convert()
*/
QDate QVariant::toDate() const
{
    return qVariantToHelper<QDate>(d, handlerManager);
}

/*!
    \fn QTime QVariant::toTime() const

    Returns the variant as a QTime if the variant has type() \l Time,
    \l DateTime, or \l String; otherwise returns an invalid time.

    If the type() is \l String, an invalid time will be returned if
    the string cannot be parsed as a Qt::ISODate format time.

    \sa canConvert(), convert()
*/
QTime QVariant::toTime() const
{
    return qVariantToHelper<QTime>(d, handlerManager);
}

/*!
    \fn QDateTime QVariant::toDateTime() const

    Returns the variant as a QDateTime if the variant has type() \l
    DateTime, \l Date, or \l String; otherwise returns an invalid
    date/time.

    If the type() is \l String, an invalid date/time will be returned
    if the string cannot be parsed as a Qt::ISODate format date/time.

    \sa canConvert(), convert()
*/
QDateTime QVariant::toDateTime() const
{
    return qVariantToHelper<QDateTime>(d, handlerManager);
}

/*!
    \since 4.7
    \fn QEasingCurve QVariant::toEasingCurve() const

    Returns the variant as a QEasingCurve if the variant has type() \l
    EasingCurve; otherwise returns a default easing curve.

    \sa canConvert(), convert()
*/
#ifndef QT_BOOTSTRAPPED
QEasingCurve QVariant::toEasingCurve() const
{
    return qVariantToHelper<QEasingCurve>(d, handlerManager);
}
#endif

/*!
    \fn QByteArray QVariant::toByteArray() const

    Returns the variant as a QByteArray if the variant has type() \l
    ByteArray or \l String (converted using QString::fromAscii());
    otherwise returns an empty byte array.

    \sa canConvert(), convert()
*/
QByteArray QVariant::toByteArray() const
{
    return qVariantToHelper<QByteArray>(d, handlerManager);
}

#ifndef QT_NO_GEOM_VARIANT
/*!
    \fn QPoint QVariant::toPoint() const

    Returns the variant as a QPoint if the variant has type()
    \l Point or \l PointF; otherwise returns a null QPoint.

    \sa canConvert(), convert()
*/
QPoint QVariant::toPoint() const
{
    return qVariantToHelper<QPoint>(d, handlerManager);
}

/*!
    \fn QRect QVariant::toRect() const

    Returns the variant as a QRect if the variant has type() \l Rect;
    otherwise returns an invalid QRect.

    \sa canConvert(), convert()
*/
QRect QVariant::toRect() const
{
    return qVariantToHelper<QRect>(d, handlerManager);
}

/*!
    \fn QSize QVariant::toSize() const

    Returns the variant as a QSize if the variant has type() \l Size;
    otherwise returns an invalid QSize.

    \sa canConvert(), convert()
*/
QSize QVariant::toSize() const
{
    return qVariantToHelper<QSize>(d, handlerManager);
}

/*!
    \fn QSizeF QVariant::toSizeF() const

    Returns the variant as a QSizeF if the variant has type() \l
    SizeF; otherwise returns an invalid QSizeF.

    \sa canConvert(), convert()
*/
QSizeF QVariant::toSizeF() const
{
    return qVariantToHelper<QSizeF>(d, handlerManager);
}

/*!
    \fn QRectF QVariant::toRectF() const

    Returns the variant as a QRectF if the variant has type() \l Rect
    or \l RectF; otherwise returns an invalid QRectF.

    \sa canConvert(), convert()
*/
QRectF QVariant::toRectF() const
{
    return qVariantToHelper<QRectF>(d, handlerManager);
}

/*!
    \fn QLineF QVariant::toLineF() const

    Returns the variant as a QLineF if the variant has type() \l
    LineF; otherwise returns an invalid QLineF.

    \sa canConvert(), convert()
*/
QLineF QVariant::toLineF() const
{
    return qVariantToHelper<QLineF>(d, handlerManager);
}

/*!
    \fn QLine QVariant::toLine() const

    Returns the variant as a QLine if the variant has type() \l Line;
    otherwise returns an invalid QLine.

    \sa canConvert(), convert()
*/
QLine QVariant::toLine() const
{
    return qVariantToHelper<QLine>(d, handlerManager);
}

/*!
    \fn QPointF QVariant::toPointF() const

    Returns the variant as a QPointF if the variant has type() \l
    Point or \l PointF; otherwise returns a null QPointF.

    \sa canConvert(), convert()
*/
QPointF QVariant::toPointF() const
{
    return qVariantToHelper<QPointF>(d, handlerManager);
}

#endif // QT_NO_GEOM_VARIANT

/*!
    \fn QUrl QVariant::toUrl() const

    Returns the variant as a QUrl if the variant has type()
    \l Url; otherwise returns an invalid QUrl.

    \sa canConvert(), convert()
*/
QUrl QVariant::toUrl() const
{
    return qVariantToHelper<QUrl>(d, handlerManager);
}

/*!
    \fn QLocale QVariant::toLocale() const

    Returns the variant as a QLocale if the variant has type()
    \l Locale; otherwise returns an invalid QLocale.

    \sa canConvert(), convert()
*/
QLocale QVariant::toLocale() const
{
    return qVariantToHelper<QLocale>(d, handlerManager);
}

/*!
    \fn QRegExp QVariant::toRegExp() const
    \since 4.1

    Returns the variant as a QRegExp if the variant has type() \l
    RegExp; otherwise returns an empty QRegExp.

    \sa canConvert(), convert()
*/
#ifndef QT_NO_REGEXP
QRegExp QVariant::toRegExp() const
{
    return qVariantToHelper<QRegExp>(d, handlerManager);
}
#endif

/*!
    \fn QChar QVariant::toChar() const

    Returns the variant as a QChar if the variant has type() \l Char,
    \l Int, or \l UInt; otherwise returns an invalid QChar.

    \sa canConvert(), convert()
*/
QChar QVariant::toChar() const
{
    return qVariantToHelper<QChar>(d, handlerManager);
}

/*!
    Returns the variant as a QBitArray if the variant has type()
    \l BitArray; otherwise returns an empty bit array.

    \sa canConvert(), convert()
*/
QBitArray QVariant::toBitArray() const
{
    return qVariantToHelper<QBitArray>(d, handlerManager);
}

template <typename T>
inline T qNumVariantToHelper(const QVariant::Private &d,
                             const HandlersManager &handlerManager, bool *ok, const T& val)
{
    uint t = qMetaTypeId<T>();
    if (ok)
        *ok = true;
    if (d.type == t)
        return val;

    T ret = 0;
    if (!handlerManager[d.type]->convert(&d, t, &ret, ok) && ok)
        *ok = false;
    return ret;
}

/*!
    Returns the variant as an int if the variant has type() \l Int,
    \l Bool, \l ByteArray, \l Char, \l Double, \l LongLong, \l
    String, \l UInt, or \l ULongLong; otherwise returns 0.

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to an int; otherwise \c{*}\a{ok} is set to false.

    \bold{Warning:} If the value is convertible to a \l LongLong but is too
    large to be represented in an int, the resulting arithmetic overflow will
    not be reflected in \a ok. A simple workaround is to use QString::toInt().
    Fixing this bug has been postponed to Qt 5 in order to avoid breaking existing code.

    \sa canConvert(), convert()
*/
int QVariant::toInt(bool *ok) const
{
    return qNumVariantToHelper<int>(d, handlerManager, ok, d.data.i);
}

/*!
    Returns the variant as an unsigned int if the variant has type()
    \l UInt,  \l Bool, \l ByteArray, \l Char, \l Double, \l Int, \l
    LongLong, \l String, or \l ULongLong; otherwise returns 0.

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to an unsigned int; otherwise \c{*}\a{ok} is set to false.

    \bold{Warning:} If the value is convertible to a \l ULongLong but is too
    large to be represented in an unsigned int, the resulting arithmetic overflow will
    not be reflected in \a ok. A simple workaround is to use QString::toUInt().
    Fixing this bug has been postponed to Qt 5 in order to avoid breaking existing code.

    \sa canConvert(), convert()
*/
uint QVariant::toUInt(bool *ok) const
{
    return qNumVariantToHelper<uint>(d, handlerManager, ok, d.data.u);
}

/*!
    Returns the variant as a long long int if the variant has type()
    \l LongLong, \l Bool, \l ByteArray, \l Char, \l Double, \l Int,
    \l String, \l UInt, or \l ULongLong; otherwise returns 0.

    If \a ok is non-null: \c{*}\c{ok} is set to true if the value could be
    converted to an int; otherwise \c{*}\c{ok} is set to false.

    \sa canConvert(), convert()
*/
qlonglong QVariant::toLongLong(bool *ok) const
{
    return qNumVariantToHelper<qlonglong>(d, handlerManager, ok, d.data.ll);
}

/*!
    Returns the variant as as an unsigned long long int if the
    variant has type() \l ULongLong, \l Bool, \l ByteArray, \l Char,
    \l Double, \l Int, \l LongLong, \l String, or \l UInt; otherwise
    returns 0.

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to an int; otherwise \c{*}\a{ok} is set to false.

    \sa canConvert(), convert()
*/
qulonglong QVariant::toULongLong(bool *ok) const
{
    return qNumVariantToHelper<qulonglong>(d, handlerManager, ok, d.data.ull);
}

/*!
    Returns the variant as a bool if the variant has type() Bool.

    Returns true if the variant has type() \l Bool, \l Char, \l Double,
    \l Int, \l LongLong, \l UInt, or \l ULongLong and the value is
    non-zero, or if the variant has type \l String or \l ByteArray and
    its lower-case content is not empty, "0" or "false"; otherwise
    returns false.

    \sa canConvert(), convert()
*/
bool QVariant::toBool() const
{
    if (d.type == Bool)
        return d.data.b;

    bool res = false;
    handlerManager[d.type]->convert(&d, Bool, &res, 0);

    return res;
}

/*!
    Returns the variant as a double if the variant has type() \l
    Double, \l QMetaType::Float, \l Bool, \l ByteArray, \l Int, \l LongLong, \l String, \l
    UInt, or \l ULongLong; otherwise returns 0.0.

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to a double; otherwise \c{*}\a{ok} is set to false.

    \sa canConvert(), convert()
*/
double QVariant::toDouble(bool *ok) const
{
    return qNumVariantToHelper<double>(d, handlerManager, ok, d.data.d);
}

/*!
    Returns the variant as a float if the variant has type() \l
    Double, \l QMetaType::Float, \l Bool, \l ByteArray, \l Int, \l LongLong, \l String, \l
    UInt, or \l ULongLong; otherwise returns 0.0.

    \since 4.6

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to a double; otherwise \c{*}\a{ok} is set to false.

    \sa canConvert(), convert()
*/
float QVariant::toFloat(bool *ok) const
{
    return qNumVariantToHelper<float>(d, handlerManager, ok, d.data.f);
}

/*!
    Returns the variant as a qreal if the variant has type() \l
    Double, \l QMetaType::Float, \l Bool, \l ByteArray, \l Int, \l LongLong, \l String, \l
    UInt, or \l ULongLong; otherwise returns 0.0.

    \since 4.6

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to a double; otherwise \c{*}\a{ok} is set to false.

    \sa canConvert(), convert()
*/
qreal QVariant::toReal(bool *ok) const
{
    return qNumVariantToHelper<qreal>(d, handlerManager, ok, d.data.real);
}

/*!
    Returns the variant as a QVariantList if the variant has type()
    \l List or \l StringList; otherwise returns an empty list.

    \sa canConvert(), convert()
*/
QVariantList QVariant::toList() const
{
    return qVariantToHelper<QVariantList>(d, handlerManager);
}


static const quint32 qCanConvertMatrix[QVariant::LastCoreType + 1] =
{
/*Invalid*/     0,

/*Bool*/          1 << QVariant::Double     | 1 << QVariant::Int        | 1 << QVariant::UInt
                | 1 << QVariant::LongLong   | 1 << QVariant::ULongLong  | 1 << QVariant::ByteArray
                | 1 << QVariant::String     | 1 << QVariant::Char,

/*Int*/           1 << QVariant::UInt       | 1 << QVariant::String     | 1 << QVariant::Double
                | 1 << QVariant::Bool       | 1 << QVariant::LongLong   | 1 << QVariant::ULongLong
                | 1 << QVariant::Char       | 1 << QVariant::ByteArray,

/*UInt*/          1 << QVariant::Int        | 1 << QVariant::String     | 1 << QVariant::Double
                | 1 << QVariant::Bool       | 1 << QVariant::LongLong   | 1 << QVariant::ULongLong
                | 1 << QVariant::Char       | 1 << QVariant::ByteArray,

/*LLong*/         1 << QVariant::Int        | 1 << QVariant::String     | 1 << QVariant::Double
                | 1 << QVariant::Bool       | 1 << QVariant::UInt       | 1 << QVariant::ULongLong
                | 1 << QVariant::Char       | 1 << QVariant::ByteArray,

/*ULlong*/        1 << QVariant::Int        | 1 << QVariant::String     | 1 << QVariant::Double
                | 1 << QVariant::Bool       | 1 << QVariant::UInt       | 1 << QVariant::LongLong
                | 1 << QVariant::Char       | 1 << QVariant::ByteArray,

/*double*/        1 << QVariant::Int        | 1 << QVariant::String     | 1 << QVariant::ULongLong
                | 1 << QVariant::Bool       | 1 << QVariant::UInt       | 1 << QVariant::LongLong
                | 1 << QVariant::ByteArray,

/*QChar*/         1 << QVariant::Int        | 1 << QVariant::UInt       | 1 << QVariant::LongLong
                | 1 << QVariant::ULongLong,

/*QMap*/          0,

/*QList*/         1 << QVariant::StringList,

/*QString*/       1 << QVariant::StringList | 1 << QVariant::ByteArray  | 1 << QVariant::Int
                | 1 << QVariant::UInt       | 1 << QVariant::Bool       | 1 << QVariant::Double
                | 1 << QVariant::Date       | 1 << QVariant::Time       | 1 << QVariant::DateTime
                | 1 << QVariant::LongLong   | 1 << QVariant::ULongLong  | 1 << QVariant::Char
                | 1 << QVariant::Url        | 1 << QVariant::Uuid,

/*QStringList*/   1 << QVariant::List       | 1 << QVariant::String,

/*QByteArray*/    1 << QVariant::String     | 1 << QVariant::Int        | 1 << QVariant::UInt | 1 << QVariant::Bool
                | 1 << QVariant::Double     | 1 << QVariant::LongLong   | 1 << QVariant::ULongLong,

/*QBitArray*/     0,

/*QDate*/         1 << QVariant::String     | 1 << QVariant::DateTime,

/*QTime*/         1 << QVariant::String     | 1 << QVariant::DateTime,

/*QDateTime*/     1 << QVariant::String     | 1 << QVariant::Date,

/*QUrl*/          1 << QVariant::String,

/*QLocale*/       0,

/*QRect*/         1 << QVariant::RectF,

/*QRectF*/        1 << QVariant::Rect,

/*QSize*/         1 << QVariant::SizeF,

/*QSizeF*/        1 << QVariant::Size,

/*QLine*/         1 << QVariant::LineF,

/*QLineF*/        1 << QVariant::Line,

/*QPoint*/        1 << QVariant::PointF,

/*QPointF*/       1 << QVariant::Point,

/*QRegExp*/       0,

/*QHash*/         0,

/*QEasingCurve*/  0,

/*QUuid*/         1 << QVariant::String
};

/*!
    Returns true if the variant's type can be cast to the requested
    type, \a targetTypeId. Such casting is done automatically when calling the
    toInt(), toBool(), ... methods.

    The following casts are done automatically:

    \table
    \header \o Type \o Automatically Cast To
    \row \o \l Bool \o \l Char, \l Double, \l Int, \l LongLong, \l String, \l UInt, \l ULongLong
    \row \o \l ByteArray \o \l Double, \l Int, \l LongLong, \l String, \l UInt, \l ULongLong
    \row \o \l Char \o \l Bool, \l Int, \l UInt, \l LongLong, \l ULongLong
    \row \o \l Color \o \l String
    \row \o \l Date \o \l DateTime, \l String
    \row \o \l DateTime \o \l Date, \l String, \l Time
    \row \o \l Double \o \l Bool, \l Int, \l LongLong, \l String, \l UInt, \l ULongLong
    \row \o \l Font \o \l String
    \row \o \l Int \o \l Bool, \l Char, \l Double, \l LongLong, \l String, \l UInt, \l ULongLong
    \row \o \l KeySequence \o \l Int, \l String
    \row \o \l List \o \l StringList (if the list's items can be converted to strings)
    \row \o \l LongLong \o \l Bool, \l ByteArray, \l Char, \l Double, \l Int, \l String, \l UInt, \l ULongLong
    \row \o \l Point \o PointF
    \row \o \l Rect \o RectF
    \row \o \l String \o \l Bool, \l ByteArray, \l Char, \l Color, \l Date, \l DateTime, \l Double,
                         \l Font, \l Int, \l KeySequence, \l LongLong, \l StringList, \l Time, \l UInt,
                         \l ULongLong
    \row \o \l StringList \o \l List, \l String (if the list contains exactly one item)
    \row \o \l Time \o \l String
    \row \o \l UInt \o \l Bool, \l Char, \l Double, \l Int, \l LongLong, \l String, \l ULongLong
    \row \o \l ULongLong \o \l Bool, \l Char, \l Double, \l Int, \l LongLong, \l String, \l UInt
    \endtable

    \sa convert()
*/
bool QVariant::canConvert(int targetTypeId) const
{
    // TODO Reimplement this function, currently it works but it is a historical mess.
    const uint currentType = ((d.type == QMetaType::Float) ? QVariant::Double : d.type);
    if (uint(targetTypeId) == uint(QMetaType::Float)) targetTypeId = QVariant::Double;

    if (currentType == uint(targetTypeId))
        return true;

    // FIXME It should be LastCoreType intead of Uuid
    if (currentType > int(QMetaType::QUuid) || targetTypeId > int(QMetaType::QUuid)) {
        switch (uint(targetTypeId)) {
        case QVariant::Int:
            return currentType == QVariant::KeySequence
                   || currentType == QMetaType::ULong
                   || currentType == QMetaType::Long
                   || currentType == QMetaType::UShort
                   || currentType == QMetaType::UChar
                   || currentType == QMetaType::Char
                   || currentType == QMetaType::Short;
        case QVariant::Image:
            return currentType == QVariant::Pixmap || currentType == QVariant::Bitmap;
        case QVariant::Pixmap:
            return currentType == QVariant::Image || currentType == QVariant::Bitmap
                              || currentType == QVariant::Brush;
        case QVariant::Bitmap:
            return currentType == QVariant::Pixmap || currentType == QVariant::Image;
        case QVariant::ByteArray:
            return currentType == QVariant::Color;
        case QVariant::String:
            return currentType == QVariant::KeySequence || currentType == QVariant::Font
                              || currentType == QVariant::Color;
        case QVariant::KeySequence:
            return currentType == QVariant::String || currentType == QVariant::Int;
        case QVariant::Font:
            return currentType == QVariant::String;
        case QVariant::Color:
            return currentType == QVariant::String || currentType == QVariant::ByteArray
                              || currentType == QVariant::Brush;
        case QVariant::Brush:
            return currentType == QVariant::Color || currentType == QVariant::Pixmap;
        case QMetaType::Long:
        case QMetaType::Char:
        case QMetaType::UChar:
        case QMetaType::ULong:
        case QMetaType::Short:
        case QMetaType::UShort:
            return qCanConvertMatrix[QVariant::Int] & (1 << currentType) || currentType == QVariant::Int;
        default:
            return false;
        }
    }

    if (targetTypeId == String && currentType == StringList)
        return v_cast<QStringList>(&d)->count() == 1;
    else
        return qCanConvertMatrix[targetTypeId] & (1 << currentType);
}

/*!
    Casts the variant to the requested type, \a targetTypeId. If the cast cannot be
    done, the variant is cleared. Returns true if the current type of
    the variant was successfully cast; otherwise returns false.

    \warning For historical reasons, converting a null QVariant results
    in a null value of the desired type (e.g., an empty string for
    QString) and a result of false.

    \sa canConvert(), clear()
*/

bool QVariant::convert(int targetTypeId)
{
    if (d.type == uint(targetTypeId))
        return true;

    QVariant oldValue = *this;

    clear();
    if (!oldValue.canConvert(targetTypeId))
        return false;

    create(targetTypeId, 0);
    if (oldValue.isNull())
        return false;

    bool isOk = true;
    if (!handlerManager[d.type]->convert(&oldValue.d, targetTypeId, data(), &isOk))
        isOk = false;
    d.is_null = !isOk;
    return isOk;
}

/*!
  \fn convert(const int type, void *ptr) const
  \internal
  Created for qvariant_cast() usage
*/
bool QVariant::convert(const int type, void *ptr) const
{
    Q_ASSERT(type < int(QMetaType::User));
    return handlerManager[type]->convert(&d, type, ptr, 0);
}


/*!
    \fn bool operator==(const QVariant &v1, const QVariant &v2)

    \relates QVariant

    Returns true if \a v1 and \a v2 are equal; otherwise returns false.

    \warning This function doesn't support custom types registered
    with qRegisterMetaType().
*/
/*!
    \fn bool operator!=(const QVariant &v1, const QVariant &v2)

    \relates QVariant

    Returns false if \a v1 and \a v2 are equal; otherwise returns true.

    \warning This function doesn't support custom types registered
    with qRegisterMetaType().
*/

/*! \fn bool QVariant::operator==(const QVariant &v) const

    Compares this QVariant with \a v and returns true if they are
    equal; otherwise returns false.

    In the case of custom types, their equalness operators are not called.
    Instead the values' addresses are compared.
*/

/*!
    \fn bool QVariant::operator!=(const QVariant &v) const

    Compares this QVariant with \a v and returns true if they are not
    equal; otherwise returns false.

    \warning This function doesn't support custom types registered
    with qRegisterMetaType().
*/

static bool qIsNumericType(uint tp)
{
    return (tp >= QVariant::Bool && tp <= QVariant::Double)
           || (tp >= QMetaType::Long && tp <= QMetaType::Float);
}

static bool qIsFloatingPoint(uint tp)
{
    return tp == QVariant::Double || tp == QMetaType::Float;
}

/*! \internal
 */
bool QVariant::cmp(const QVariant &v) const
{
    QVariant v2 = v;
    if (d.type != v2.d.type) {
        if (qIsNumericType(d.type) && qIsNumericType(v.d.type)) {
            if (qIsFloatingPoint(d.type) || qIsFloatingPoint(v.d.type))
                return qFuzzyCompare(toReal(), v.toReal());
            else
                return toLongLong() == v.toLongLong();
        }
        if (!v2.canConvert(d.type) || !v2.convert(d.type))
            return false;
    }
    return handlerManager[d.type]->compare(&d, &v2.d);
}

/*! \internal
 */

const void *QVariant::constData() const
{
    return d.is_shared ? d.data.shared->ptr : reinterpret_cast<const void *>(&d.data.ptr);
}

/*!
    \fn const void* QVariant::data() const

    \internal
*/

/*! \internal */
void* QVariant::data()
{
    detach();
    return const_cast<void *>(constData());
}


/*!
  Returns true if this is a NULL variant, false otherwise.
*/
bool QVariant::isNull() const
{
    return handlerManager[d.type]->isNull(&d);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QVariant &v)
{
    dbg.nospace() << "QVariant(" << QMetaType::typeName(v.userType()) << ", ";
    handlerManager[v.d.type]->debugStream(dbg, v);
    dbg.nospace() << ')';
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const QVariant::Type p)
{
    dbg.nospace() << "QVariant::" << QMetaType::typeName(p);
    return dbg.space();
}
#endif


/*! \fn void QVariant::setValue(const T &value)

    Stores a copy of \a value. If \c{T} is a type that QVariant
    doesn't support, QMetaType is used to store the value. A compile
    error will occur if QMetaType doesn't handle the type.

    Example:

    \snippet doc/src/snippets/code/src_corelib_kernel_qvariant.cpp 4

    \sa value(), fromValue(), canConvert()
 */

/*! \fn T QVariant::value() const

    Returns the stored value converted to the template type \c{T}.
    Call canConvert() to find out whether a type can be converted.
    If the value cannot be converted, \l{default-constructed value}
    will be returned.

    If the type \c{T} is supported by QVariant, this function behaves
    exactly as toString(), toInt() etc.

    Example:

    \snippet doc/src/snippets/code/src_corelib_kernel_qvariant.cpp 5

    \sa setValue(), fromValue(), canConvert()
*/

/*! \fn bool QVariant::canConvert() const

    Returns true if the variant can be converted to the template type \c{T},
    otherwise false.

    Example:

    \snippet doc/src/snippets/code/src_corelib_kernel_qvariant.cpp 6

    \sa convert()
*/

/*! \fn static QVariant QVariant::fromValue(const T &value)

    Returns a QVariant containing a copy of \a value. Behaves
    exactly like setValue() otherwise.

    Example:

    \snippet doc/src/snippets/code/src_corelib_kernel_qvariant.cpp 7

    \note If you are working with custom types, you should use
    the Q_DECLARE_METATYPE() macro to register your custom type.

    \sa setValue(), value()
*/

/*!
    \fn QVariant qVariantFromValue(const T &value)
    \relates QVariant
    \obsolete

    Returns a variant containing a copy of the given \a value
    with template type \c{T}.

    This function is equivalent to QVariant::fromValue(\a value).

    \note This function was provided as a workaround for MSVC 6
    which did not support member template functions. It is advised
    to use the other form in new code.

    For example, a QObject pointer can be stored in a variant with the
    following code:

    \snippet doc/src/snippets/code/src_corelib_kernel_qvariant.cpp 8

    \sa QVariant::fromValue()
*/

/*! \fn void qVariantSetValue(QVariant &variant, const T &value)
    \relates QVariant
    \obsolete

    Sets the contents of the given \a variant to a copy of the
    \a value with the specified template type \c{T}.

    This function is equivalent to QVariant::setValue(\a value).

    \note This function was provided as a workaround for MSVC 6
    which did not support member template functions. It is advised
    to use the other form in new code.

    \sa QVariant::setValue()
*/

/*!
    \fn T qvariant_cast(const QVariant &value)
    \relates QVariant

    Returns the given \a value converted to the template type \c{T}.

    This function is equivalent to QVariant::value().

    \sa QVariant::value()
*/

/*! \fn T qVariantValue(const QVariant &value)
    \relates QVariant
    \obsolete

    Returns the given \a value converted to the template type \c{T}.

    This function is equivalent to
    \l{QVariant::value()}{QVariant::value}<T>(\a value).

    \note This function was provided as a workaround for MSVC 6
    which did not support member template functions. It is advised
    to use the other form in new code.

    \sa QVariant::value(), qvariant_cast()
*/

/*! \fn bool qVariantCanConvert(const QVariant &value)
    \relates QVariant
    \obsolete

    Returns true if the given \a value can be converted to the
    template type specified; otherwise returns false.

    This function is equivalent to QVariant::canConvert(\a value).

    \note This function was provided as a workaround for MSVC 6
    which did not support member template functions. It is advised
    to use the other form in new code.

    \sa QVariant::canConvert()
*/

/*!
    \typedef QVariantList
    \relates QVariant

    Synonym for QList<QVariant>.
*/

/*!
    \typedef QVariantMap
    \relates QVariant

    Synonym for QMap<QString, QVariant>.
*/

/*!
    \typedef QVariantHash
    \relates QVariant
    \since 4.5

    Synonym for QHash<QString, QVariant>.
*/

/*!
    \typedef QVariant::DataPtr
    \internal
*/

/*!
    \fn DataPtr &QVariant::data_ptr()
    \internal
*/

QT_END_NAMESPACE
