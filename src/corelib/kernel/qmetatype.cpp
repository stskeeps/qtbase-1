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

#include "qmetatype.h"
#include "qmetatype_p.h"
#include "qobjectdefs.h"
#include "qdatetime.h"
#include "qbytearray.h"
#include "qreadwritelock.h"
#include "qstring.h"
#include "qstringlist.h"
#include "qvector.h"
#include "qlocale.h"
#include "qeasingcurve.h"
#include "quuid.h"
#include "qvariant.h"
#include "qmetatypeswitcher_p.h"

#ifdef QT_BOOTSTRAPPED
# ifndef QT_NO_GEOM_VARIANT
#  define QT_NO_GEOM_VARIANT
# endif
#else
#  include "qbitarray.h"
#  include "qurl.h"
#  include "qvariant.h"
#  include "qabstractitemmodel.h"
#endif

#ifndef QT_NO_GEOM_VARIANT
# include "qsize.h"
# include "qpoint.h"
# include "qrect.h"
# include "qline.h"
#endif

QT_BEGIN_NAMESPACE

#define NS(x) QT_PREPEND_NAMESPACE(x)


namespace {
template<typename T>
struct TypeDefinition {
    static const bool IsAvailable = true;
};

struct DefinedTypesFilter {
    template<typename T>
    struct Acceptor {
        static const bool IsAccepted = TypeDefinition<T>::IsAvailable && QTypeModuleInfo<T>::IsCore;
    };
};

// Ignore these types, as incomplete
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
#ifdef QT_BOOTSTRAPPED
template<> struct TypeDefinition<QVariantMap> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QVariantHash> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QVariantList> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QVariant> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QBitArray> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QUrl> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QEasingCurve> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QModelIndex> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_REGEXP
template<> struct TypeDefinition<QRegExp> { static const bool IsAvailable = false; };
#endif
} // namespace

/*!
    \macro Q_DECLARE_OPAQUE_POINTER(Pointer)
    \relates QMetaType

    This macro enables pointers to forward-declared types to be registered with
    QMetaType using either Q_DECLARE_METATYPE() or qRegisterMetaType().

    \sa Q_DECLARE_METATYPE(), qRegisterMetaType()

*/

/*!
    \macro Q_DECLARE_METATYPE(Type)
    \relates QMetaType

    This macro makes the type \a Type known to QMetaType as long as it
    provides a public default constructor, a public copy constructor and
    a public destructor.
    It is needed to use the type \a Type as a custom type in QVariant.

    This macro requires that \a Type is a fully defined type at the point where
    it is used. For pointer types, it also requires that the pointed to type is
    fully defined. Use in conjunction with Q_DECLARE_OPAQUE_POINTER() to
    register pointers to forward declared types.

    Ideally, this macro should be placed below the declaration of
    the class or struct. If that is not possible, it can be put in
    a private header file which has to be included every time that
    type is used in a QVariant.

    Adding a Q_DECLARE_METATYPE() makes the type known to all template
    based functions, including QVariant. Note that if you intend to
    use the type in \e queued signal and slot connections or in
    QObject's property system, you also have to call
    qRegisterMetaType() since the names are resolved at runtime.

    This example shows a typical use case of Q_DECLARE_METATYPE():

    \snippet doc/src/snippets/code/src_corelib_kernel_qmetatype.cpp 0

    If \c MyStruct is in a namespace, the Q_DECLARE_METATYPE() macro
    has to be outside the namespace:

    \snippet doc/src/snippets/code/src_corelib_kernel_qmetatype.cpp 1

    Since \c{MyStruct} is now known to QMetaType, it can be used in QVariant:

    \snippet doc/src/snippets/code/src_corelib_kernel_qmetatype.cpp 2

    \sa qRegisterMetaType()
*/

/*!
    \enum QMetaType::Type

    These are the built-in types supported by QMetaType:

    \value Void \c void
    \value Bool \c bool
    \value Int \c int
    \value UInt \c{unsigned int}
    \value Double \c double
    \value QChar QChar
    \value QString QString
    \value QByteArray QByteArray

    \value VoidStar \c{void *}
    \value Long \c{long}
    \value LongLong LongLong
    \value Short \c{short}
    \value Char \c{char}
    \value ULong \c{unsigned long}
    \value ULongLong ULongLong
    \value UShort \c{unsigned short}
    \value UChar \c{unsigned char}
    \value Float \c float
    \value QObjectStar QObject *
    \value QWidgetStar QWidget *
    \value QVariant QVariant

    \value QCursor QCursor
    \value QDate QDate
    \value QSize QSize
    \value QTime QTime
    \value QVariantList QVariantList
    \value QPolygon QPolygon
    \value QPolygonF QPolygonF
    \value QColor QColor
    \value QSizeF QSizeF
    \value QRectF QRectF
    \value QLine QLine
    \value QTextLength QTextLength
    \value QStringList QStringList
    \value QVariantMap QVariantMap
    \value QVariantHash QVariantHash
    \value QIcon QIcon
    \value QPen QPen
    \value QLineF QLineF
    \value QTextFormat QTextFormat
    \value QRect QRect
    \value QPoint QPoint
    \value QUrl QUrl
    \value QRegExp QRegExp
    \value QDateTime QDateTime
    \value QPointF QPointF
    \value QPalette QPalette
    \value QFont QFont
    \value QBrush QBrush
    \value QRegion QRegion
    \value QBitArray QBitArray
    \value QImage QImage
    \value QKeySequence QKeySequence
    \value QSizePolicy QSizePolicy
    \value QPixmap QPixmap
    \value QLocale QLocale
    \value QBitmap QBitmap
    \value QMatrix QMatrix
    \value QTransform QTransform
    \value QMatrix4x4 QMatrix4x4
    \value QVector2D QVector2D
    \value QVector3D QVector3D
    \value QVector4D QVector4D
    \value QQuaternion QQuaternion
    \value QEasingCurve QEasingCurve

    \value User  Base value for user types
    \value UnknownType This is an invalid type id. It is returned from QMetaType for types that are not registered

    \omitvalue FirstGuiType
    \omitvalue FirstWidgetsType
    \omitvalue LastCoreType
    \omitvalue LastGuiType
    \omitvalue LastWidgetsType
    \omitvalue QReal
    \omitvalue HighestInternalId

    Additional types can be registered using Q_DECLARE_METATYPE().

    \sa type(), typeName()
*/

/*!
    \enum QMetaType::TypeFlags

    The enum describes attributes of a type supported by QMetaType.

    \value NeedsConstruction This type has non-trivial constructors. If the flag is not set instances can be safely initialized with memset to 0.
    \value NeedsDestruction This type has a non-trivial destructor. If the flag is not set calls to the destructor are not necessary before discarding objects.
    \value MovableType An instance of a type having this attribute can be safely moved by memcpy.
*/

/*!
    \class QMetaType
    \brief The QMetaType class manages named types in the meta-object system.

    \ingroup objectmodel
    \threadsafe

    The class is used as a helper to marshall types in QVariant and
    in queued signals and slots connections. It associates a type
    name to a type so that it can be created and destructed
    dynamically at run-time. Declare new types with Q_DECLARE_METATYPE()
    to make them available to QVariant and other template-based functions.
    Call qRegisterMetaType() to make type available to non-template based
    functions, such as the queued signal and slot connections.

    Any class or struct that has a public default
    constructor, a public copy constructor, and a public destructor
    can be registered.

    The following code allocates and destructs an instance of
    \c{MyClass}:

    \snippet doc/src/snippets/code/src_corelib_kernel_qmetatype.cpp 3

    If we want the stream operators \c operator<<() and \c
    operator>>() to work on QVariant objects that store custom types,
    the custom type must provide \c operator<<() and \c operator>>()
    operators.

    \sa Q_DECLARE_METATYPE(), QVariant::setValue(), QVariant::value(), QVariant::fromValue()
*/

#define QT_ADD_STATIC_METATYPE(MetaTypeName, MetaTypeId, RealName) \
    { #RealName, sizeof(#RealName) - 1, MetaTypeId },

#define QT_ADD_STATIC_METATYPE_ALIASES_ITER(MetaTypeName, MetaTypeId, AliasingName, RealNameStr) \
    { RealNameStr, sizeof(RealNameStr) - 1, QMetaType::MetaTypeName },

#define QT_ADD_STATIC_METATYPE_HACKS_ITER(MetaTypeName, TypeId, Name) \
    QT_ADD_STATIC_METATYPE(MetaTypeName, MetaTypeName, Name)

static const struct { const char * typeName; int typeNameLength; int type; } types[] = {
    QT_FOR_EACH_STATIC_TYPE(QT_ADD_STATIC_METATYPE)
    QT_FOR_EACH_STATIC_ALIAS_TYPE(QT_ADD_STATIC_METATYPE_ALIASES_ITER)
    QT_FOR_EACH_STATIC_HACKS_TYPE(QT_ADD_STATIC_METATYPE_HACKS_ITER)
    {0, 0, QMetaType::UnknownType}
};

Q_CORE_EXPORT const QMetaTypeInterface *qMetaTypeGuiHelper = 0;
Q_CORE_EXPORT const QMetaTypeInterface *qMetaTypeWidgetsHelper = 0;

class QCustomTypeInfo : public QMetaTypeInterface
{
public:
    QCustomTypeInfo()
        : alias(-1)
    {
        QMetaTypeInterface empty = QT_METATYPE_INTERFACE_INIT(void);
        *static_cast<QMetaTypeInterface*>(this) = empty;
    }
    QByteArray typeName;
    int alias;
};

namespace
{
union CheckThatItIsPod
{   // This should break if QMetaTypeInterface is not a POD type
    QMetaTypeInterface iface;
};
}

Q_DECLARE_TYPEINFO(QCustomTypeInfo, Q_MOVABLE_TYPE);
Q_GLOBAL_STATIC(QVector<QCustomTypeInfo>, customTypes)
Q_GLOBAL_STATIC(QReadWriteLock, customTypesLock)

#ifndef QT_NO_DATASTREAM
/*! \internal
*/
void QMetaType::registerStreamOperators(const char *typeName, SaveOperator saveOp,
                                        LoadOperator loadOp)
{
    registerStreamOperators(type(typeName), saveOp, loadOp);
}

/*! \internal
*/
void QMetaType::registerStreamOperators(int idx, SaveOperator saveOp,
                                        LoadOperator loadOp)
{
    if (idx < User)
        return; //builtin types should not be registered;
    QVector<QCustomTypeInfo> *ct = customTypes();
    if (!ct)
        return;
    QWriteLocker locker(customTypesLock());
    QCustomTypeInfo &inf = (*ct)[idx - User];
    inf.saveOp = saveOp;
    inf.loadOp = loadOp;
}
#endif // QT_NO_DATASTREAM

/*!
    Returns the type name associated with the given \a type, or 0 if no
    matching type was found. The returned pointer must not be deleted.

    \sa type(), isRegistered(), Type
*/
const char *QMetaType::typeName(int type)
{
    // In theory it can be filled during compilation time, but for some reason template code
    // that is able to do it causes GCC 4.6 to generate additional 3K of executable code. Probably
    // it is not worth of it.
    static const char *namesCache[QMetaType::HighestInternalId + 1];

    const char *result;
    if (type <= QMetaType::HighestInternalId && ((result = namesCache[type])))
        return result;

#define QT_METATYPE_TYPEID_TYPENAME_CONVERTER(MetaTypeName, TypeId, RealName) \
        case QMetaType::MetaTypeName: result = #RealName; break;

    switch (QMetaType::Type(type)) {
    QT_FOR_EACH_STATIC_TYPE(QT_METATYPE_TYPEID_TYPENAME_CONVERTER)

    default: {
        if (Q_UNLIKELY(type < QMetaType::User)) {
            return 0; // It can happen when someone cast int to QVariant::Type, we should not crash...
        } else {
            const QVector<QCustomTypeInfo> * const ct = customTypes();
            QReadLocker locker(customTypesLock());
            return ct && ct->count() > type - QMetaType::User && !ct->at(type - QMetaType::User).typeName.isEmpty()
                    ? ct->at(type - QMetaType::User).typeName.constData()
                    : 0;
        }
    }
    }
#undef QT_METATYPE_TYPEID_TYPENAME_CONVERTER

    Q_ASSERT(type <= QMetaType::HighestInternalId);
    namesCache[type] = result;
    return result;
}

/*! \internal
    Similar to QMetaType::type(), but only looks in the static set of types.
*/
static inline int qMetaTypeStaticType(const char *typeName, int length)
{
    int i = 0;
    while (types[i].typeName && ((length != types[i].typeNameLength)
                                 || strcmp(typeName, types[i].typeName))) {
        ++i;
    }
    return types[i].type;
}

/*! \internal
    Similar to QMetaType::type(), but only looks in the custom set of
    types, and doesn't lock the mutex.
*/
static int qMetaTypeCustomType_unlocked(const char *typeName, int length)
{
    const QVector<QCustomTypeInfo> * const ct = customTypes();
    if (!ct)
        return QMetaType::UnknownType;

    for (int v = 0; v < ct->count(); ++v) {
        const QCustomTypeInfo &customInfo = ct->at(v);
        if ((length == customInfo.typeName.size())
            && !strcmp(typeName, customInfo.typeName.constData())) {
            if (customInfo.alias >= 0)
                return customInfo.alias;
            return v + QMetaType::User;
        }
    }
    return QMetaType::UnknownType;
}

/*! \internal

    This function is needed until existing code outside of qtbase
    has been changed to call the new version of registerType().
 */
int QMetaType::registerType(const char *typeName, Deleter deleter,
                            Creator creator)
{
    return registerType(typeName, deleter, creator, 0, 0, 0, TypeFlags());
}

/*! \internal
    \since 5.0

    Registers a user type for marshalling, with \a typeName, a \a
    deleter, a \a creator, a \a destructor, a \a constructor, and
    a \a size. Returns the type's handle, or -1 if the type could
    not be registered.
 */
int QMetaType::registerType(const char *typeName, Deleter deleter,
                            Creator creator,
                            Destructor destructor,
                            Constructor constructor,
                            int size, TypeFlags flags)
{
    QVector<QCustomTypeInfo> *ct = customTypes();
    if (!ct || !typeName || !deleter || !creator)
        return -1;

#ifdef QT_NO_QOBJECT
    NS(QByteArray) normalizedTypeName = typeName;
#else
    NS(QByteArray) normalizedTypeName = QMetaObject::normalizedType(typeName);
#endif

    int idx = qMetaTypeStaticType(normalizedTypeName.constData(),
                                  normalizedTypeName.size());

    int previousSize = 0;
    int previousFlags = 0;
    if (idx == UnknownType) {
        QWriteLocker locker(customTypesLock());
        idx = qMetaTypeCustomType_unlocked(normalizedTypeName.constData(),
                                           normalizedTypeName.size());
        if (idx == UnknownType) {
            QCustomTypeInfo inf;
            inf.typeName = normalizedTypeName;
            inf.creator = creator;
            inf.deleter = deleter;
#ifndef QT_NO_DATASTREAM
            inf.loadOp = 0;
            inf.saveOp = 0;
#endif
            inf.alias = -1;
            inf.constructor = constructor;
            inf.destructor = destructor;
            inf.size = size;
            inf.flags = flags;
            idx = ct->size() + User;
            ct->append(inf);
            return idx;
        }

        if (idx >= User) {
            previousSize = ct->at(idx - User).size;
            previousFlags = ct->at(idx - User).flags;
        }
    }

    if (idx < User) {
        previousSize = QMetaType::sizeOf(idx);
        previousFlags = QMetaType::typeFlags(idx);
    }

    if (previousSize != size) {
        qFatal("QMetaType::registerType: Binary compatibility break "
            "-- Size mismatch for type '%s' [%i]. Previously registered "
            "size %i, now registering size %i.",
            normalizedTypeName.constData(), idx, previousSize, size);
    }
    if (previousFlags != flags) {
        qFatal("QMetaType::registerType: Binary compatibility break "
            "-- Type flags for type '%s' [%i] don't match. Previously "
            "registered TypeFlags(0x%x), now registering TypeFlags(0x%x).",
            normalizedTypeName.constData(), idx, previousFlags, int(flags));
    }

    return idx;
}

/*! \internal
    \since 4.7

    Registers a user type for marshalling, as an alias of another type (typedef)
*/
int QMetaType::registerTypedef(const char* typeName, int aliasId)
{
    QVector<QCustomTypeInfo> *ct = customTypes();
    if (!ct || !typeName)
        return -1;

#ifdef QT_NO_QOBJECT
    NS(QByteArray) normalizedTypeName = typeName;
#else
    NS(QByteArray) normalizedTypeName = QMetaObject::normalizedType(typeName);
#endif

    int idx = qMetaTypeStaticType(normalizedTypeName.constData(),
                                  normalizedTypeName.size());

    if (idx == UnknownType) {
        QWriteLocker locker(customTypesLock());
        idx = qMetaTypeCustomType_unlocked(normalizedTypeName.constData(),
                                               normalizedTypeName.size());

        if (idx == UnknownType) {
            QCustomTypeInfo inf;
            inf.typeName = normalizedTypeName;
            inf.alias = aliasId;
            inf.creator = 0;
            inf.deleter = 0;
            ct->append(inf);
            return aliasId;
        }
    }

    if (idx != aliasId) {
        qFatal("QMetaType::registerTypedef: Binary compatibility break "
            "-- Type name '%s' previously registered as typedef of '%s' [%i], "
            "now registering as typedef of '%s' [%i].",
            normalizedTypeName.constData(), QMetaType::typeName(idx), idx,
            QMetaType::typeName(aliasId), aliasId);
    }
    return idx;
}

/*!
    Returns true if the datatype with ID \a type is registered;
    otherwise returns false.

    \sa type(), typeName(), Type
*/
bool QMetaType::isRegistered(int type)
{
    // predefined type
    if ((type >= FirstCoreType && type <= LastCoreType)
        || (type >= FirstGuiType && type <= LastGuiType)
        || (type >= FirstWidgetsType && type <= LastWidgetsType)) {
        return true;
    }

    QReadLocker locker(customTypesLock());
    const QVector<QCustomTypeInfo> * const ct = customTypes();
    return ((type >= User) && (ct && ct->count() > type - User) && !ct->at(type - User).typeName.isEmpty());
}

/*!
    Returns a handle to the type called \a typeName, or QMetaType::UnknownType if there is
    no such type.

    \sa isRegistered(), typeName(), Type
*/
int QMetaType::type(const char *typeName)
{
    int length = qstrlen(typeName);
    if (!length)
        return UnknownType;
    int type = qMetaTypeStaticType(typeName, length);
    if (type == UnknownType) {
        QReadLocker locker(customTypesLock());
        type = qMetaTypeCustomType_unlocked(typeName, length);
#ifndef QT_NO_QOBJECT
        if (type == UnknownType) {
            const NS(QByteArray) normalizedTypeName = QMetaObject::normalizedType(typeName);
            type = qMetaTypeStaticType(normalizedTypeName.constData(),
                                       normalizedTypeName.size());
            if (type == UnknownType) {
                type = qMetaTypeCustomType_unlocked(normalizedTypeName.constData(),
                                                    normalizedTypeName.size());
            }
        }
#endif
    }
    return type;
}

#ifndef QT_NO_DATASTREAM
/*!
    Writes the object pointed to by \a data with the ID \a type to
    the given \a stream. Returns true if the object is saved
    successfully; otherwise returns false.

    The type must have been registered with qRegisterMetaType() and
    qRegisterMetaTypeStreamOperators() beforehand.

    Normally, you should not need to call this function directly.
    Instead, use QVariant's \c operator<<(), which relies on save()
    to stream custom types.

    \sa load(), qRegisterMetaTypeStreamOperators()
*/
bool QMetaType::save(QDataStream &stream, int type, const void *data)
{
    if (!data || !isRegistered(type))
        return false;

    switch(type) {
    case QMetaType::UnknownType:
    case QMetaType::Void:
    case QMetaType::VoidStar:
    case QMetaType::QObjectStar:
    case QMetaType::QWidgetStar:
    case QMetaType::QModelIndex:
        return false;
    case QMetaType::Long:
        stream << qlonglong(*static_cast<const long *>(data));
        break;
    case QMetaType::Int:
        stream << *static_cast<const int *>(data);
        break;
    case QMetaType::Short:
        stream << *static_cast<const short *>(data);
        break;
    case QMetaType::Char:
        // force a char to be signed
        stream << *static_cast<const signed char *>(data);
        break;
    case QMetaType::ULong:
        stream << qulonglong(*static_cast<const ulong *>(data));
        break;
    case QMetaType::UInt:
        stream << *static_cast<const uint *>(data);
        break;
    case QMetaType::LongLong:
        stream << *static_cast<const qlonglong *>(data);
        break;
    case QMetaType::ULongLong:
        stream << *static_cast<const qulonglong *>(data);
        break;
    case QMetaType::UShort:
        stream << *static_cast<const ushort *>(data);
        break;
    case QMetaType::UChar:
        stream << *static_cast<const uchar *>(data);
        break;
    case QMetaType::Bool:
        stream << qint8(*static_cast<const bool *>(data));
        break;
    case QMetaType::Float:
        stream << *static_cast<const float *>(data);
        break;
    case QMetaType::Double:
        stream << *static_cast<const double *>(data);
        break;
    case QMetaType::QChar:
        stream << *static_cast<const NS(QChar) *>(data);
        break;
#ifndef QT_BOOTSTRAPPED
    case QMetaType::QVariantMap:
        stream << *static_cast<const NS(QVariantMap)*>(data);
        break;
    case QMetaType::QVariantHash:
        stream << *static_cast<const NS(QVariantHash)*>(data);
        break;
    case QMetaType::QVariantList:
        stream << *static_cast<const NS(QVariantList)*>(data);
        break;
    case QMetaType::QVariant:
        stream << *static_cast<const NS(QVariant)*>(data);
        break;
#endif
    case QMetaType::QByteArray:
        stream << *static_cast<const NS(QByteArray)*>(data);
        break;
    case QMetaType::QString:
        stream << *static_cast<const NS(QString)*>(data);
        break;
    case QMetaType::QStringList:
        stream << *static_cast<const NS(QStringList)*>(data);
        break;
#ifndef QT_BOOTSTRAPPED
    case QMetaType::QBitArray:
        stream << *static_cast<const NS(QBitArray)*>(data);
        break;
#endif
    case QMetaType::QDate:
        stream << *static_cast<const NS(QDate)*>(data);
        break;
    case QMetaType::QTime:
        stream << *static_cast<const NS(QTime)*>(data);
        break;
    case QMetaType::QDateTime:
        stream << *static_cast<const NS(QDateTime)*>(data);
        break;
#ifndef QT_BOOTSTRAPPED
    case QMetaType::QUrl:
        stream << *static_cast<const NS(QUrl)*>(data);
        break;
#endif
    case QMetaType::QLocale:
        stream << *static_cast<const NS(QLocale)*>(data);
        break;
#ifndef QT_NO_GEOM_VARIANT
    case QMetaType::QRect:
        stream << *static_cast<const NS(QRect)*>(data);
        break;
    case QMetaType::QRectF:
        stream << *static_cast<const NS(QRectF)*>(data);
        break;
    case QMetaType::QSize:
        stream << *static_cast<const NS(QSize)*>(data);
        break;
    case QMetaType::QSizeF:
        stream << *static_cast<const NS(QSizeF)*>(data);
        break;
    case QMetaType::QLine:
        stream << *static_cast<const NS(QLine)*>(data);
        break;
    case QMetaType::QLineF:
        stream << *static_cast<const NS(QLineF)*>(data);
        break;
    case QMetaType::QPoint:
        stream << *static_cast<const NS(QPoint)*>(data);
        break;
    case QMetaType::QPointF:
        stream << *static_cast<const NS(QPointF)*>(data);
        break;
#endif
#ifndef QT_NO_REGEXP
    case QMetaType::QRegExp:
        stream << *static_cast<const NS(QRegExp)*>(data);
        break;
#endif
#ifndef QT_BOOTSTRAPPED
    case QMetaType::QEasingCurve:
        stream << *static_cast<const NS(QEasingCurve)*>(data);
        break;
#endif
    case QMetaType::QFont:
    case QMetaType::QPixmap:
    case QMetaType::QBrush:
    case QMetaType::QColor:
    case QMetaType::QPalette:
    case QMetaType::QImage:
    case QMetaType::QPolygon:
    case QMetaType::QPolygonF:
    case QMetaType::QRegion:
    case QMetaType::QBitmap:
    case QMetaType::QCursor:
    case QMetaType::QKeySequence:
    case QMetaType::QPen:
    case QMetaType::QTextLength:
    case QMetaType::QTextFormat:
    case QMetaType::QMatrix:
    case QMetaType::QTransform:
    case QMetaType::QMatrix4x4:
    case QMetaType::QVector2D:
    case QMetaType::QVector3D:
    case QMetaType::QVector4D:
    case QMetaType::QQuaternion:
        if (!qMetaTypeGuiHelper)
            return false;
        qMetaTypeGuiHelper[type - FirstGuiType].saveOp(stream, data);
        break;
    case QMetaType::QIcon:
    case QMetaType::QSizePolicy:
        if (!qMetaTypeWidgetsHelper)
            return false;
        qMetaTypeWidgetsHelper[type - FirstWidgetsType].saveOp(stream, data);
        break;
    case QMetaType::QUuid:
        stream << *static_cast<const NS(QUuid)*>(data);
        break;
    default: {
        const QVector<QCustomTypeInfo> * const ct = customTypes();
        if (!ct)
            return false;

        SaveOperator saveOp = 0;
        {
            QReadLocker locker(customTypesLock());
            saveOp = ct->at(type - User).saveOp;
        }

        if (!saveOp)
            return false;
        saveOp(stream, data);
        break; }
    }

    return true;
}

/*!
    Reads the object of the specified \a type from the given \a
    stream into \a data. Returns true if the object is loaded
    successfully; otherwise returns false.

    The type must have been registered with qRegisterMetaType() and
    qRegisterMetaTypeStreamOperators() beforehand.

    Normally, you should not need to call this function directly.
    Instead, use QVariant's \c operator>>(), which relies on load()
    to stream custom types.

    \sa save(), qRegisterMetaTypeStreamOperators()
*/
bool QMetaType::load(QDataStream &stream, int type, void *data)
{
    if (!data || !isRegistered(type))
        return false;

    switch(type) {
    case QMetaType::UnknownType:
    case QMetaType::Void:
    case QMetaType::VoidStar:
    case QMetaType::QObjectStar:
    case QMetaType::QWidgetStar:
    case QMetaType::QModelIndex:
        return false;
    case QMetaType::Long: {
        qlonglong l;
        stream >> l;
        *static_cast<long *>(data) = long(l);
        break; }
    case QMetaType::Int:
        stream >> *static_cast<int *>(data);
        break;
    case QMetaType::Short:
        stream >> *static_cast<short *>(data);
        break;
    case QMetaType::Char:
        // force a char to be signed
        stream >> *static_cast<signed char *>(data);
        break;
    case QMetaType::ULong: {
        qulonglong ul;
        stream >> ul;
        *static_cast<ulong *>(data) = ulong(ul);
        break; }
    case QMetaType::UInt:
        stream >> *static_cast<uint *>(data);
        break;
    case QMetaType::LongLong:
        stream >> *static_cast<qlonglong *>(data);
        break;
    case QMetaType::ULongLong:
        stream >> *static_cast<qulonglong *>(data);
        break;
    case QMetaType::UShort:
        stream >> *static_cast<ushort *>(data);
        break;
    case QMetaType::UChar:
        stream >> *static_cast<uchar *>(data);
        break;
    case QMetaType::Bool: {
        qint8 b;
        stream >> b;
        *static_cast<bool *>(data) = b;
        break; }
    case QMetaType::Float:
        stream >> *static_cast<float *>(data);
        break;
    case QMetaType::Double:
        stream >> *static_cast<double *>(data);
        break;
    case QMetaType::QChar:
        stream >> *static_cast< NS(QChar)*>(data);
        break;
#ifndef QT_BOOTSTRAPPED
    case QMetaType::QVariantMap:
        stream >> *static_cast< NS(QVariantMap)*>(data);
        break;
    case QMetaType::QVariantHash:
        stream >> *static_cast< NS(QVariantHash)*>(data);
        break;
    case QMetaType::QVariantList:
        stream >> *static_cast< NS(QVariantList)*>(data);
        break;
    case QMetaType::QVariant:
        stream >> *static_cast< NS(QVariant)*>(data);
        break;
#endif
    case QMetaType::QByteArray:
        stream >> *static_cast< NS(QByteArray)*>(data);
        break;
    case QMetaType::QString:
        stream >> *static_cast< NS(QString)*>(data);
        break;
    case QMetaType::QStringList:
        stream >> *static_cast< NS(QStringList)*>(data);
        break;
#ifndef QT_BOOTSTRAPPED
    case QMetaType::QBitArray:
        stream >> *static_cast< NS(QBitArray)*>(data);
        break;
#endif
    case QMetaType::QDate:
        stream >> *static_cast< NS(QDate)*>(data);
        break;
    case QMetaType::QTime:
        stream >> *static_cast< NS(QTime)*>(data);
        break;
    case QMetaType::QDateTime:
        stream >> *static_cast< NS(QDateTime)*>(data);
        break;
#ifndef QT_BOOTSTRAPPED
    case QMetaType::QUrl:
        stream >> *static_cast< NS(QUrl)*>(data);
        break;
#endif
    case QMetaType::QLocale:
        stream >> *static_cast< NS(QLocale)*>(data);
        break;
#ifndef QT_NO_GEOM_VARIANT
    case QMetaType::QRect:
        stream >> *static_cast< NS(QRect)*>(data);
        break;
    case QMetaType::QRectF:
        stream >> *static_cast< NS(QRectF)*>(data);
        break;
    case QMetaType::QSize:
        stream >> *static_cast< NS(QSize)*>(data);
        break;
    case QMetaType::QSizeF:
        stream >> *static_cast< NS(QSizeF)*>(data);
        break;
    case QMetaType::QLine:
        stream >> *static_cast< NS(QLine)*>(data);
        break;
    case QMetaType::QLineF:
        stream >> *static_cast< NS(QLineF)*>(data);
        break;
    case QMetaType::QPoint:
        stream >> *static_cast< NS(QPoint)*>(data);
        break;
    case QMetaType::QPointF:
        stream >> *static_cast< NS(QPointF)*>(data);
        break;
#endif
#ifndef QT_NO_REGEXP
    case QMetaType::QRegExp:
        stream >> *static_cast< NS(QRegExp)*>(data);
        break;
#endif
#ifndef QT_BOOTSTRAPPED
    case QMetaType::QEasingCurve:
        stream >> *static_cast< NS(QEasingCurve)*>(data);
        break;
#endif
    case QMetaType::QFont:
    case QMetaType::QPixmap:
    case QMetaType::QBrush:
    case QMetaType::QColor:
    case QMetaType::QPalette:
    case QMetaType::QImage:
    case QMetaType::QPolygon:
    case QMetaType::QPolygonF:
    case QMetaType::QRegion:
    case QMetaType::QBitmap:
    case QMetaType::QCursor:
    case QMetaType::QKeySequence:
    case QMetaType::QPen:
    case QMetaType::QTextLength:
    case QMetaType::QTextFormat:
    case QMetaType::QMatrix:
    case QMetaType::QTransform:
    case QMetaType::QMatrix4x4:
    case QMetaType::QVector2D:
    case QMetaType::QVector3D:
    case QMetaType::QVector4D:
    case QMetaType::QQuaternion:
        if (!qMetaTypeGuiHelper)
            return false;
        qMetaTypeGuiHelper[type - FirstGuiType].loadOp(stream, data);
        break;
    case QMetaType::QIcon:
    case QMetaType::QSizePolicy:
        if (!qMetaTypeWidgetsHelper)
            return false;
        qMetaTypeWidgetsHelper[type - FirstWidgetsType].loadOp(stream, data);
        break;
    case QMetaType::QUuid:
        stream >> *static_cast< NS(QUuid)*>(data);
        break;
    default: {
        const QVector<QCustomTypeInfo> * const ct = customTypes();
        if (!ct)
            return false;

        LoadOperator loadOp = 0;
        {
            QReadLocker locker(customTypesLock());
            loadOp = ct->at(type - User).loadOp;
        }

        if (!loadOp)
            return false;
        loadOp(stream, data);
        break; }
    }
    return true;
}
#endif // QT_NO_DATASTREAM

/*!
    Returns a copy of \a copy, assuming it is of type \a type. If \a
    copy is zero, creates a default type.

    \sa destroy(), isRegistered(), Type
*/
void *QMetaType::create(int type, const void *copy)
{
    if (copy) {
        switch(type) {
        case QMetaType::VoidStar:
        case QMetaType::QObjectStar:
        case QMetaType::QWidgetStar:
            return new void *(*static_cast<void* const *>(copy));
        case QMetaType::Long:
            return new long(*static_cast<const long*>(copy));
        case QMetaType::Int:
            return new int(*static_cast<const int*>(copy));
        case QMetaType::Short:
            return new short(*static_cast<const short*>(copy));
        case QMetaType::Char:
            return new char(*static_cast<const char*>(copy));
        case QMetaType::ULong:
            return new ulong(*static_cast<const ulong*>(copy));
        case QMetaType::UInt:
            return new uint(*static_cast<const uint*>(copy));
        case QMetaType::LongLong:
            return new qlonglong(*static_cast<const qlonglong*>(copy));
        case QMetaType::ULongLong:
            return new qulonglong(*static_cast<const qulonglong*>(copy));
        case QMetaType::UShort:
            return new ushort(*static_cast<const ushort*>(copy));
        case QMetaType::UChar:
            return new uchar(*static_cast<const uchar*>(copy));
        case QMetaType::Bool:
            return new bool(*static_cast<const bool*>(copy));
        case QMetaType::Float:
            return new float(*static_cast<const float*>(copy));
        case QMetaType::Double:
            return new double(*static_cast<const double*>(copy));
        case QMetaType::QChar:
            return new NS(QChar)(*static_cast<const NS(QChar)*>(copy));
#ifndef QT_BOOTSTRAPPED
        case QMetaType::QVariantMap:
            return new NS(QVariantMap)(*static_cast<const NS(QVariantMap)*>(copy));
        case QMetaType::QVariantHash:
            return new NS(QVariantHash)(*static_cast<const NS(QVariantHash)*>(copy));
        case QMetaType::QVariantList:
            return new NS(QVariantList)(*static_cast<const NS(QVariantList)*>(copy));
        case QMetaType::QVariant:
            return new NS(QVariant)(*static_cast<const NS(QVariant)*>(copy));
#endif
        case QMetaType::QByteArray:
            return new NS(QByteArray)(*static_cast<const NS(QByteArray)*>(copy));
        case QMetaType::QString:
            return new NS(QString)(*static_cast<const NS(QString)*>(copy));
        case QMetaType::QStringList:
            return new NS(QStringList)(*static_cast<const NS(QStringList)*>(copy));
#ifndef QT_BOOTSTRAPPED
        case QMetaType::QBitArray:
            return new NS(QBitArray)(*static_cast<const NS(QBitArray)*>(copy));
#endif
        case QMetaType::QDate:
            return new NS(QDate)(*static_cast<const NS(QDate)*>(copy));
        case QMetaType::QTime:
            return new NS(QTime)(*static_cast<const NS(QTime)*>(copy));
        case QMetaType::QDateTime:
            return new NS(QDateTime)(*static_cast<const NS(QDateTime)*>(copy));
#ifndef QT_BOOTSTRAPPED
        case QMetaType::QUrl:
            return new NS(QUrl)(*static_cast<const NS(QUrl)*>(copy));
#endif
        case QMetaType::QLocale:
            return new NS(QLocale)(*static_cast<const NS(QLocale)*>(copy));
#ifndef QT_NO_GEOM_VARIANT
        case QMetaType::QRect:
            return new NS(QRect)(*static_cast<const NS(QRect)*>(copy));
        case QMetaType::QRectF:
            return new NS(QRectF)(*static_cast<const NS(QRectF)*>(copy));
        case QMetaType::QSize:
            return new NS(QSize)(*static_cast<const NS(QSize)*>(copy));
        case QMetaType::QSizeF:
            return new NS(QSizeF)(*static_cast<const NS(QSizeF)*>(copy));
        case QMetaType::QLine:
            return new NS(QLine)(*static_cast<const NS(QLine)*>(copy));
        case QMetaType::QLineF:
            return new NS(QLineF)(*static_cast<const NS(QLineF)*>(copy));
        case QMetaType::QPoint:
            return new NS(QPoint)(*static_cast<const NS(QPoint)*>(copy));
        case QMetaType::QPointF:
            return new NS(QPointF)(*static_cast<const NS(QPointF)*>(copy));
#endif
#ifndef QT_NO_REGEXP
        case QMetaType::QRegExp:
            return new NS(QRegExp)(*static_cast<const NS(QRegExp)*>(copy));
#endif
#ifndef QT_BOOTSTRAPPED
        case QMetaType::QEasingCurve:
            return new NS(QEasingCurve)(*static_cast<const NS(QEasingCurve)*>(copy));
#endif
        case QMetaType::QUuid:
            return new NS(QUuid)(*static_cast<const NS(QUuid)*>(copy));
#ifndef QT_BOOTSTRAPPED
        case QMetaType::QModelIndex:
            return new NS(QModelIndex)(*static_cast<const NS(QModelIndex)*>(copy));
#endif
        case QMetaType::UnknownType:
        case QMetaType::Void:
            return 0;
        default:
            ;
        }
    } else {
        switch(type) {
        case QMetaType::VoidStar:
        case QMetaType::QObjectStar:
        case QMetaType::QWidgetStar:
            return new void *;
        case QMetaType::Long:
            return new long;
        case QMetaType::Int:
            return new int;
        case QMetaType::Short:
            return new short;
        case QMetaType::Char:
            return new char;
        case QMetaType::ULong:
            return new ulong;
        case QMetaType::UInt:
            return new uint;
        case QMetaType::LongLong:
            return new qlonglong;
        case QMetaType::ULongLong:
            return new qulonglong;
        case QMetaType::UShort:
            return new ushort;
        case QMetaType::UChar:
            return new uchar;
        case QMetaType::Bool:
            return new bool;
        case QMetaType::Float:
            return new float;
        case QMetaType::Double:
            return new double;
        case QMetaType::QChar:
            return new NS(QChar);
#ifndef QT_BOOTSTRAPPED
        case QMetaType::QVariantMap:
            return new NS(QVariantMap);
        case QMetaType::QVariantHash:
            return new NS(QVariantHash);
        case QMetaType::QVariantList:
            return new NS(QVariantList);
        case QMetaType::QVariant:
            return new NS(QVariant);
#endif
        case QMetaType::QByteArray:
            return new NS(QByteArray);
        case QMetaType::QString:
            return new NS(QString);
        case QMetaType::QStringList:
            return new NS(QStringList);
#ifndef QT_BOOTSTRAPPED
        case QMetaType::QBitArray:
            return new NS(QBitArray);
#endif
        case QMetaType::QDate:
            return new NS(QDate);
        case QMetaType::QTime:
            return new NS(QTime);
        case QMetaType::QDateTime:
            return new NS(QDateTime);
#ifndef QT_BOOTSTRAPPED
        case QMetaType::QUrl:
            return new NS(QUrl);
#endif
        case QMetaType::QLocale:
            return new NS(QLocale);
#ifndef QT_NO_GEOM_VARIANT
        case QMetaType::QRect:
            return new NS(QRect);
        case QMetaType::QRectF:
            return new NS(QRectF);
        case QMetaType::QSize:
            return new NS(QSize);
        case QMetaType::QSizeF:
            return new NS(QSizeF);
        case QMetaType::QLine:
            return new NS(QLine);
        case QMetaType::QLineF:
            return new NS(QLineF);
        case QMetaType::QPoint:
            return new NS(QPoint);
        case QMetaType::QPointF:
            return new NS(QPointF);
#endif
#ifndef QT_NO_REGEXP
        case QMetaType::QRegExp:
            return new NS(QRegExp);
#endif
#ifndef QT_BOOTSTRAPPED
        case QMetaType::QEasingCurve:
            return new NS(QEasingCurve);
#endif
        case QMetaType::QUuid:
            return new NS(QUuid);
#ifndef QT_BOOTSTRAPPED
        case QMetaType::QModelIndex:
            return new NS(QModelIndex);
#endif
        case QMetaType::UnknownType:
        case QMetaType::Void:
            return 0;
        default:
            ;
        }
    }

    Creator creator = 0;
    if (type >= FirstGuiType && type <= LastGuiType) {
        if (!qMetaTypeGuiHelper)
            return 0;
        creator = qMetaTypeGuiHelper[type - FirstGuiType].creator;
    } else if (type >= FirstWidgetsType && type <= LastWidgetsType) {
        if (!qMetaTypeWidgetsHelper)
            return 0;
        creator = qMetaTypeWidgetsHelper[type - FirstWidgetsType].creator;
    } else {
        const QVector<QCustomTypeInfo> * const ct = customTypes();
        QReadLocker locker(customTypesLock());
        if (type < User || !ct || ct->count() <= type - User)
            return 0;
        if (ct->at(type - User).typeName.isEmpty())
            return 0;
        creator = ct->at(type - User).creator;
    }

    return creator(copy);
}

namespace {
class TypeDestroyer {
    template<typename T, bool IsAcceptedType = DefinedTypesFilter::Acceptor<T>::IsAccepted>
    struct DestroyerImpl {
        static void Destroy(const int /* type */, void *where) { qMetaTypeDeleteHelper<T>(where); }
    };
    template<typename T>
    struct DestroyerImpl<T, /* IsAcceptedType = */ false> {
        static void Destroy(const int type, void *where)
        {
            if (QTypeModuleInfo<T>::IsGui) {
                if (Q_LIKELY(qMetaTypeGuiHelper))
                    qMetaTypeGuiHelper[type - QMetaType::FirstGuiType].deleter(where);
                return;
            }
            if (QTypeModuleInfo<T>::IsWidget) {
                if (Q_LIKELY(qMetaTypeWidgetsHelper))
                    qMetaTypeWidgetsHelper[type - QMetaType::FirstWidgetsType].deleter(where);
                return;
            }
            // This point can be reached only for known types that definition is not available, for example
            // in bootstrap mode. We have no other choice then ignore it.
        }
    };
public:
    TypeDestroyer(const int type)
        : m_type(type)
    {}

    template<typename T>
    void delegate(const T *where) { DestroyerImpl<T>::Destroy(m_type, const_cast<T*>(where)); }
    void delegate(const void *) {}
    void delegate(const QMetaTypeSwitcher::UnknownType*) {}
    void delegate(const QMetaTypeSwitcher::NotBuiltinType *where) { customTypeDestroyer(m_type, (void*)where); }

private:
    static void customTypeDestroyer(const int type, void *where)
    {
        QMetaType::Destructor deleter;
        const QVector<QCustomTypeInfo> * const ct = customTypes();
        {
            QReadLocker locker(customTypesLock());
            if (Q_UNLIKELY(type < QMetaType::User || !ct || ct->count() <= type - QMetaType::User))
                return;
            deleter = ct->at(type - QMetaType::User).deleter;
        }
        deleter(where);
    }

    const int m_type;
};
} // namespace


/*!
    Destroys the \a data, assuming it is of the \a type given.

    \sa create(), isRegistered(), Type
*/
void QMetaType::destroy(int type, void *data)
{
    TypeDestroyer deleter(type);
    QMetaTypeSwitcher::switcher<void>(deleter, type, data);
}

namespace {
class TypeConstructor {
    template<typename T, bool IsAcceptedType = DefinedTypesFilter::Acceptor<T>::IsAccepted>
    struct ConstructorImpl {
        static void *Construct(const int /*type*/, void *where, const void *copy) { return qMetaTypeConstructHelper<T>(where, copy); }
    };
    template<typename T>
    struct ConstructorImpl<T, /* IsAcceptedType = */ false> {
        static void *Construct(const int type, void *where, const void *copy)
        {
            if (QTypeModuleInfo<T>::IsGui)
                return Q_LIKELY(qMetaTypeGuiHelper) ? qMetaTypeGuiHelper[type - QMetaType::FirstGuiType].constructor(where, copy) : 0;

            if (QTypeModuleInfo<T>::IsWidget)
                return Q_LIKELY(qMetaTypeWidgetsHelper) ? qMetaTypeWidgetsHelper[type - QMetaType::FirstWidgetsType].constructor(where, copy) : 0;

            // This point can be reached only for known types that definition is not available, for example
            // in bootstrap mode. We have no other choice then ignore it.
            return 0;
        }
    };
public:
    TypeConstructor(const int type, void *where)
        : m_type(type)
        , m_where(where)
    {}

    template<typename T>
    void *delegate(const T *copy) { return ConstructorImpl<T>::Construct(m_type, m_where, copy); }
    void *delegate(const void *) { return m_where; }
    void *delegate(const QMetaTypeSwitcher::UnknownType*) { return m_where; }
    void *delegate(const QMetaTypeSwitcher::NotBuiltinType *copy) { return customTypeConstructor(m_type, m_where, copy); }

private:
    static void *customTypeConstructor(const int type, void *where, const void *copy)
    {
        QMetaType::Constructor ctor;
        const QVector<QCustomTypeInfo> * const ct = customTypes();
        {
            QReadLocker locker(customTypesLock());
            if (Q_UNLIKELY(type < QMetaType::User || !ct || ct->count() <= type - QMetaType::User))
                return 0;
            ctor = ct->at(type - QMetaType::User).constructor;
        }
        return ctor(where, copy);
    }

    const int m_type;
    void *m_where;
};
} // namespace

/*!
    \since 5.0

    Constructs a value of the given \a type in the existing memory
    addressed by \a where, that is a copy of \a copy, and returns
    \a where. If \a copy is zero, the value is default constructed.

    This is a low-level function for explicitly managing the memory
    used to store the type. Consider calling create() if you don't
    need this level of control (that is, use "new" rather than
    "placement new").

    You must ensure that \a where points to a location that can store
    a value of type \a type, and that \a where is suitably aligned.
    The type's size can be queried by calling sizeOf().

    The rule of thumb for alignment is that a type is aligned to its
    natural boundary, which is the smallest power of 2 that is bigger
    than the type, unless that alignment is larger than the maximum
    useful alignment for the platform. For practical purposes,
    alignment larger than 2 * sizeof(void*) is only necessary for
    special hardware instructions (e.g., aligned SSE loads and stores
    on x86).

    \sa destruct(), sizeOf()
*/
void *QMetaType::construct(int type, void *where, const void *copy)
{
    if (!where)
        return 0;
    TypeConstructor constructor(type, where);
    return QMetaTypeSwitcher::switcher<void*>(constructor, type, copy);
}


namespace {
class TypeDestructor {
    template<typename T, bool IsAcceptedType = DefinedTypesFilter::Acceptor<T>::IsAccepted>
    struct DestructorImpl {
        static void Destruct(const int /* type */, void *where) { qMetaTypeDestructHelper<T>(where); }
    };
    template<typename T>
    struct DestructorImpl<T, /* IsAcceptedType = */ false> {
        static void Destruct(const int type, void *where)
        {
            if (QTypeModuleInfo<T>::IsGui) {
                if (Q_LIKELY(qMetaTypeGuiHelper))
                    qMetaTypeGuiHelper[type - QMetaType::FirstGuiType].destructor(where);
                return;
            }
            if (QTypeModuleInfo<T>::IsWidget) {
                if (Q_LIKELY(qMetaTypeWidgetsHelper))
                    qMetaTypeWidgetsHelper[type - QMetaType::FirstWidgetsType].destructor(where);
                return;
            }
            // This point can be reached only for known types that definition is not available, for example
            // in bootstrap mode. We have no other choice then ignore it.
        }
    };
public:
    TypeDestructor(const int type)
        : m_type(type)
    {}

    template<typename T>
    void delegate(const T *where) { DestructorImpl<T>::Destruct(m_type, const_cast<T*>(where)); }
    void delegate(const void *) {}
    void delegate(const QMetaTypeSwitcher::UnknownType*) {}
    void delegate(const QMetaTypeSwitcher::NotBuiltinType *where) { customTypeDestructor(m_type, (void*)where); }

private:
    static void customTypeDestructor(const int type, void *where)
    {
        QMetaType::Destructor dtor;
        const QVector<QCustomTypeInfo> * const ct = customTypes();
        {
            QReadLocker locker(customTypesLock());
            if (Q_UNLIKELY(type < QMetaType::User || !ct || ct->count() <= type - QMetaType::User))
                return;
            dtor = ct->at(type - QMetaType::User).destructor;
        }
        dtor(where);
    }

    const int m_type;
};
} // namespace

/*!
    \since 5.0

    Destructs the value of the given \a type, located at \a where.

    Unlike destroy(), this function only invokes the type's
    destructor, it doesn't invoke the delete operator.

    \sa construct()
*/
void QMetaType::destruct(int type, void *where)
{
    if (!where)
        return;
    TypeDestructor destructor(type);
    QMetaTypeSwitcher::switcher<void>(destructor, type, where);
}


namespace {
class SizeOf {
    template<typename T, bool IsAcceptedType = DefinedTypesFilter::Acceptor<T>::IsAccepted>
    struct SizeOfImpl {
        static int Size(const int) { return QTypeInfo<T>::sizeOf; }
    };
    template<typename T>
    struct SizeOfImpl<T, /* IsAcceptedType = */ false> {
        static int Size(const int type)
        {
            if (QTypeModuleInfo<T>::IsGui)
                return Q_LIKELY(qMetaTypeGuiHelper) ? qMetaTypeGuiHelper[type - QMetaType::FirstGuiType].size : 0;

            if (QTypeModuleInfo<T>::IsWidget)
                return Q_LIKELY(qMetaTypeWidgetsHelper) ? qMetaTypeWidgetsHelper[type - QMetaType::FirstWidgetsType].size : 0;

            // This point can be reached only for known types that definition is not available, for example
            // in bootstrap mode. We have no other choice then ignore it.
            return 0;
        }
    };

public:
    SizeOf(int type)
        : m_type(type)
    {}

    template<typename T>
    int delegate(const T*) { return SizeOfImpl<T>::Size(m_type); }
    int delegate(const QMetaTypeSwitcher::UnknownType*) { return 0; }
    int delegate(const QMetaTypeSwitcher::NotBuiltinType*) { return customTypeSizeOf(m_type); }
private:
    static int customTypeSizeOf(const int type)
    {
        const QVector<QCustomTypeInfo> * const ct = customTypes();
        QReadLocker locker(customTypesLock());
        if (Q_UNLIKELY(type < QMetaType::User || !ct || ct->count() <= type - QMetaType::User))
            return 0;
        return ct->at(type - QMetaType::User).size;
    }

    const int m_type;
};
} // namespace

/*!
    \since 5.0

    Returns the size of the given \a type in bytes (i.e., sizeof(T),
    where T is the actual type identified by the \a type argument).

    This function is typically used together with construct()
    to perform low-level management of the memory used by a type.

    \sa construct()
*/
int QMetaType::sizeOf(int type)
{
    SizeOf sizeOf(type);
    return QMetaTypeSwitcher::switcher<int>(sizeOf, type, 0);
}

namespace {
class Flags
{
    template<typename T, bool IsAcceptedType = DefinedTypesFilter::Acceptor<T>::IsAccepted>
    struct FlagsImpl
    {
        static quint32 Flags(const int type)
        {
            return (!QTypeInfo<T>::isStatic * QMetaType::MovableType)
                    | (QTypeInfo<T>::isComplex * QMetaType::NeedsConstruction)
                    | (QTypeInfo<T>::isComplex * QMetaType::NeedsDestruction)
                    | (type == QMetaType::QObjectStar ? QMetaType::PointerToQObject : 0)
                    | (type == QMetaType::QWidgetStar ? QMetaType::PointerToQObject : 0);
        }
    };
    template<typename T>
    struct FlagsImpl<T, /* IsAcceptedType = */ false>
    {
        static quint32 Flags(const int type)
        {
            if (QTypeModuleInfo<T>::IsGui)
                return Q_LIKELY(qMetaTypeGuiHelper) ? qMetaTypeGuiHelper[type - QMetaType::FirstGuiType].flags : 0;

            if (QTypeModuleInfo<T>::IsWidget)
                return Q_LIKELY(qMetaTypeWidgetsHelper) ? qMetaTypeWidgetsHelper[type - QMetaType::FirstWidgetsType].flags : 0;

            // This point can be reached only for known types that definition is not available, for example
            // in bootstrap mode. We have no other choice then ignore it.
            return 0;
        }
    };
public:
    Flags(const int type)
        : m_type(type)
    {}
    template<typename T>
    quint32 delegate(const T*) { return FlagsImpl<T>::Flags(m_type); }
    quint32 delegate(const void*) { return 0; }
    quint32 delegate(const QMetaTypeSwitcher::UnknownType*) { return 0; }
    quint32 delegate(const QMetaTypeSwitcher::NotBuiltinType*) { return customTypeFlags(m_type); }
private:
    const int m_type;
    static quint32 customTypeFlags(const int type)
    {
        const QVector<QCustomTypeInfo> * const ct = customTypes();
        if (Q_UNLIKELY(!ct || type < QMetaType::User))
            return 0;
        QReadLocker locker(customTypesLock());
        if (Q_UNLIKELY(ct->count() <= type - QMetaType::User))
            return 0;
        return ct->at(type - QMetaType::User).flags;
    }
};
}  // namespace

/*!
    \since 5.0

    Returns flags of the given \a type.

    \sa TypeFlags()
*/
QMetaType::TypeFlags QMetaType::typeFlags(int type)
{
    Flags flags(type);
    return static_cast<QMetaType::TypeFlags>(QMetaTypeSwitcher::switcher<quint32>(flags, type, 0));
}

/*!
    \fn int qRegisterMetaType(const char *typeName)
    \relates QMetaType
    \threadsafe

    Registers the type name \a typeName for the type \c{T}. Returns
    the internal ID used by QMetaType. Any class or struct that has a
    public default constructor, a public copy constructor and a public
    destructor can be registered.

    This function requires that \c{T} is a fully defined type at the point
    where the function is called. For pointer types, it also requires that the
    pointed to type is fully defined. Use Q_DECLARE_OPAQUE_POINTER() to be able
    to register pointers to forward declared types.

    After a type has been registered, you can create and destroy
    objects of that type dynamically at run-time.

    This example registers the class \c{MyClass}:

    \snippet doc/src/snippets/code/src_corelib_kernel_qmetatype.cpp 4

    This function is useful to register typedefs so they can be used
    by QMetaProperty, or in QueuedConnections

    \snippet doc/src/snippets/code/src_corelib_kernel_qmetatype.cpp 9

    \sa qRegisterMetaTypeStreamOperators(), QMetaType::isRegistered(),
        Q_DECLARE_METATYPE()
*/

/*!
    \fn int qRegisterMetaTypeStreamOperators(const char *typeName)
    \relates QMetaType
    \threadsafe

    Registers the stream operators for the type \c{T} called \a
    typeName.

    Afterward, the type can be streamed using QMetaType::load() and
    QMetaType::save(). These functions are used when streaming a
    QVariant.

    \snippet doc/src/snippets/code/src_corelib_kernel_qmetatype.cpp 5

    The stream operators should have the following signatures:

    \snippet doc/src/snippets/code/src_corelib_kernel_qmetatype.cpp 6

    \sa qRegisterMetaType(), QMetaType::isRegistered(), Q_DECLARE_METATYPE()
*/

/*! \typedef QMetaType::Deleter
    \internal
*/
/*! \typedef QMetaType::Creator
    \internal
*/
/*! \typedef QMetaType::SaveOperator
    \internal
*/
/*! \typedef QMetaType::LoadOperator
    \internal
*/
/*! \typedef QMetaType::Destructor
    \internal
*/
/*! \typedef QMetaType::Constructor
    \internal
*/

/*!
    \fn int qRegisterMetaType()
    \relates QMetaType
    \threadsafe
    \since 4.2

    Call this function to register the type \c T. \c T must be declared with
    Q_DECLARE_METATYPE(). Returns the meta type Id.

    Example:

    \snippet doc/src/snippets/code/src_corelib_kernel_qmetatype.cpp 7

    To use the type \c T in QVariant, using Q_DECLARE_METATYPE() is
    sufficient. To use the type \c T in queued signal and slot connections,
    \c{qRegisterMetaType<T>()} must be called before the first connection
    is established.

    Also, to use type \c T with the QObject::property() API,
    \c{qRegisterMetaType<T>()} must be called before it is used, typically
    in the constructor of the class that uses \c T, or in the \c{main()}
    function.

    \sa Q_DECLARE_METATYPE()
 */

/*! \fn int qMetaTypeId()
    \relates QMetaType
    \threadsafe
    \since 4.1

    Returns the meta type id of type \c T at compile time. If the
    type was not declared with Q_DECLARE_METATYPE(), compilation will
    fail.

    Typical usage:

    \snippet doc/src/snippets/code/src_corelib_kernel_qmetatype.cpp 8

    QMetaType::type() returns the same ID as qMetaTypeId(), but does
    a lookup at runtime based on the name of the type.
    QMetaType::type() is a bit slower, but compilation succeeds if a
    type is not registered.

    \sa Q_DECLARE_METATYPE(), QMetaType::type()
*/

namespace {
class TypeInfo {
    template<typename T, bool IsAcceptedType = DefinedTypesFilter::Acceptor<T>::IsAccepted>
    struct TypeInfoImpl
    {
        TypeInfoImpl(const uint /* type */, QMetaTypeInterface &info)
        {
            QMetaTypeInterface tmp = QT_METATYPE_INTERFACE_INIT_NO_DATASTREAM(T);
            info = tmp;
        }
    };

    template<typename T>
    struct TypeInfoImpl<T, /* IsAcceptedType = */ false>
    {
        TypeInfoImpl(const uint type, QMetaTypeInterface &info)
        {
            if (QTypeModuleInfo<T>::IsGui) {
                if (Q_LIKELY(qMetaTypeGuiHelper))
                    info = qMetaTypeGuiHelper[type - QMetaType::FirstGuiType];
                return;
            }
            if (QTypeModuleInfo<T>::IsWidget) {
                if (Q_LIKELY(qMetaTypeWidgetsHelper))
                    info = qMetaTypeWidgetsHelper[type - QMetaType::FirstWidgetsType];
                return;
            }
        }
    };
public:
    QMetaTypeInterface info;
    TypeInfo(const uint type)
        : m_type(type)
    {
        QMetaTypeInterface tmp = QT_METATYPE_INTERFACE_INIT_EMPTY();
        info = tmp;
    }
    template<typename T>
    void delegate(const T*) { TypeInfoImpl<T>(m_type, info); }
    void delegate(const void*) {}
    void delegate(const QMetaTypeSwitcher::UnknownType*) {}
    void delegate(const QMetaTypeSwitcher::NotBuiltinType*) { customTypeInfo(m_type); }
private:
    void customTypeInfo(const uint type)
    {
        const QVector<QCustomTypeInfo> * const ct = customTypes();
        if (Q_UNLIKELY(!ct))
            return;
        QReadLocker locker(customTypesLock());
        if (Q_LIKELY(uint(ct->count()) > type - QMetaType::User))
            info = ct->at(type - QMetaType::User);
    }

    const uint m_type;
};
} // namespace

QMetaType QMetaType::typeInfo(const int type)
{
    TypeInfo typeInfo(type);
    QMetaTypeSwitcher::switcher<void>(typeInfo, type, 0);
    return typeInfo.info.creator || type == Void ? QMetaType(QMetaType::NoExtensionFlags
                                 , static_cast<const QMetaTypeInterface *>(0) // typeInfo::info is a temporary variable, we can't return address of it.
                                 , typeInfo.info.creator
                                 , typeInfo.info.deleter
                                 , typeInfo.info.saveOp
                                 , typeInfo.info.loadOp
                                 , typeInfo.info.constructor
                                 , typeInfo.info.destructor
                                 , typeInfo.info.size
                                 , typeInfo.info.flags
                                 , type)
                : QMetaType(UnknownType);
}

QMetaType::QMetaType(const int typeId)
    : m_typeId(typeId)
{
    if (Q_UNLIKELY(typeId == UnknownType)) {
        // Constructs invalid QMetaType instance.
        m_extensionFlags = 0xffffffff;
        Q_ASSERT(!isValid());
    } else {
        // TODO it can be better.
        *this = QMetaType::typeInfo(typeId);
        if (m_typeId == UnknownType)
            m_extensionFlags = 0xffffffff;
        else if (m_typeId == QMetaType::Void)
            m_extensionFlags = CreateEx | DestroyEx | ConstructEx | DestructEx;
    }
}

QMetaType::QMetaType(const QMetaType &other)
    : m_creator(other.m_creator)
    , m_deleter(other.m_deleter)
    , m_saveOp(other.m_saveOp)
    , m_loadOp(other.m_loadOp)
    , m_constructor(other.m_constructor)
    , m_destructor(other.m_destructor)
    , m_extension(other.m_extension) // space reserved for future use
    , m_size(other.m_size)
    , m_typeFlags(other.m_typeFlags)
    , m_extensionFlags(other.m_extensionFlags)
    , m_typeId(other.m_typeId)
{}

QMetaType &QMetaType::operator =(const QMetaType &other)
{
    m_creator = other.m_creator;
    m_deleter = other.m_deleter;
    m_saveOp = other.m_saveOp;
    m_loadOp = other.m_loadOp;
    m_constructor = other.m_constructor;
    m_destructor = other.m_destructor;
    m_size = other.m_size;
    m_typeFlags = other.m_typeFlags;
    m_extensionFlags = other.m_extensionFlags;
    m_extension = other.m_extension; // space reserved for future use
    m_typeId = other.m_typeId;
    return *this;
}

void QMetaType::ctor(const QMetaTypeInterface *info)
{
    // Special case for Void type, the type is valid but not constructible.
    // In future we may consider to remove this assert and extend this function to initialize
    // differently m_extensionFlags for different types. Currently it is not needed.
    Q_ASSERT(m_typeId == QMetaType::Void);
    Q_UNUSED(info);
    m_extensionFlags = CreateEx | DestroyEx | ConstructEx | DestructEx;
}

void QMetaType::dtor()
{}

void *QMetaType::createExtended(const void *copy) const
{
    Q_UNUSED(copy);
    return 0;
}

void QMetaType::destroyExtended(void *data) const
{
    Q_UNUSED(data);
}

void *QMetaType::constructExtended(void *where, const void *copy) const
{
    Q_UNUSED(where);
    Q_UNUSED(copy);
    return 0;
}

void QMetaType::destructExtended(void *data) const
{
    Q_UNUSED(data);
}

uint QMetaType::sizeExtended() const
{
    return 0;
}

QMetaType::TypeFlags QMetaType::flagsExtended() const
{
    return 0;
}

QT_END_NAMESPACE
