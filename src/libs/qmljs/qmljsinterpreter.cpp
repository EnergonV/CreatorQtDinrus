// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "parser/qmljsast_p.h"
#include "qmljsconstants.h"
#include "qmljscontext.h"
#include "qmljsevaluate.h"
#include "qmljsinterpreter.h"
#include "qmljsmodelmanagerinterface.h"
#include "qmljsscopeastpath.h"
#include "qmljsscopebuilder.h"
#include "qmljsscopechain.h"
#include "qmljstypedescriptionreader.h"
#include "qmljsvalueowner.h"

#include <utils/qtcassert.h>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>

using namespace LanguageUtils;
using namespace QmlJS;
using namespace QmlJS::AST;

/*!
    \class QmlJS::Value
    \brief The Value class is an abstract base class for the result of a
    JS expression.
    \sa Evaluate ValueOwner ValueVisitor

    A Value represents a category of JavaScript values, such as number
    (NumberValue), string (StringValue) or functions with a
    specific signature (FunctionValue). It can also represent internal
    categories such as "a QML component instantiation defined in a file"
    (ASTObjectValue), "a QML component defined in C++"
    (CppComponentValue) or "no specific information is available"
    (UnknownValue).

    The Value class itself provides accept() for admitting
    \l{ValueVisitor}s and a do-nothing getSourceLocation().

    Value instances should be cast to a derived type either through the
    asXXX() helper functions such as asNumberValue() or via the
    value_cast() template function.

    Values are the result of many operations in the QmlJS code model:
    \list
    \li \l{Evaluate}
    \li Context::lookupType() and Context::lookupReference()
    \li ScopeChain::lookup()
    \li ObjectValue::lookupMember()
    \endlist
*/

namespace {

class LookupMember: public MemberProcessor
{
    QString m_name;
    const Value *m_value;

    bool process(const QString &name, const Value *value)
    {
        if (m_value)
            return false;

        if (name == m_name) {
            m_value = value;
            return false;
        }

        return true;
    }

public:
    LookupMember(const QString &name)
        : m_name(name), m_value(nullptr) {}

    const Value *value() const { return m_value; }

    bool processProperty(const QString &name, const Value *value, const PropertyInfo &) override
    {
        return process(name, value);
    }

    bool processEnumerator(const QString &name, const Value *value) override
    {
        return process(name, value);
    }

    bool processSignal(const QString &name, const Value *value) override
    {
        return process(name, value);
    }

    bool processSlot(const QString &name, const Value *value) override
    {
        return process(name, value);
    }

    bool processGeneratedSlot(const QString &name, const Value *value) override
    {
        return process(name, value);
    }
};

} // end of anonymous namespace

namespace QmlJS {

MetaFunction::MetaFunction(const FakeMetaMethod &method, ValueOwner *valueOwner)
    : FunctionValue(valueOwner), m_method(method)
{
}

int MetaFunction::namedArgumentCount() const
{
    return m_method.parameterNames().size();
}

QString MetaFunction::argumentName(int index) const
{
    if (index < m_method.parameterNames().size())
        return m_method.parameterNames().at(index);

    return FunctionValue::argumentName(index);
}

bool MetaFunction::isVariadic() const
{
    return false;
}
const MetaFunction *MetaFunction::asMetaFunction() const
{
    return this;
}
const FakeMetaMethod &MetaFunction::fakeMetaMethod() const
{
    return m_method;
}

FakeMetaObjectWithOrigin::FakeMetaObjectWithOrigin(FakeMetaObject::ConstPtr fakeMetaObject, const QString &originId)
    : fakeMetaObject(fakeMetaObject)
    , originId(originId)
{ }

bool FakeMetaObjectWithOrigin::operator ==(const FakeMetaObjectWithOrigin &o) const
{
    return fakeMetaObject == o.fakeMetaObject;
}

size_t qHash(const FakeMetaObjectWithOrigin &fmoo)
{
    return qHash(fmoo.fakeMetaObject);
}

PropertyInfo::PropertyInfo(uint flags)
    : flags(flags)
{ }

QString PropertyInfo::toString() const
{
    QStringList list;
    if (isReadable())
        list.append("Readable");

    if (isWriteable())
        list.append("Writeable");

    if (isList())
        list.append("ListType");

    if (canBePointer())
        list.append("Pointer");

    if (canBeValue())
        list.append("Value");

    return list.join('|');
}

static QList<CustomImportsProvider *> g_customImportProviders;

CustomImportsProvider::CustomImportsProvider(QObject *parent)
    : QObject(parent)
{
    g_customImportProviders.append(this);
}

CustomImportsProvider::~CustomImportsProvider()
{
    g_customImportProviders.removeOne(this);
}

const QList<CustomImportsProvider *> CustomImportsProvider::allProviders()
{
    return g_customImportProviders;
}

} // namespace QmlJS

CppComponentValue::CppComponentValue(FakeMetaObject::ConstPtr metaObject, const QString &className,
                                     const QString &packageName, const ComponentVersion &componentVersion,
                                     const ComponentVersion &importVersion, int metaObjectRevision,
                                     ValueOwner *valueOwner, const QString &originId)
    : ObjectValue(valueOwner, originId),
      m_metaObject(metaObject),
      m_moduleName(packageName),
      m_componentVersion(componentVersion),
      m_importVersion(importVersion),
      m_metaObjectRevision(metaObjectRevision)
{
    setClassName(className);
    int nEnums = metaObject->enumeratorCount();
    for (int i = 0; i < nEnums; ++i) {
        FakeMetaEnum fEnum = metaObject->enumerator(i);
        m_enums[fEnum.name()] = new QmlEnumValue(this, i);
    }
}

CppComponentValue::~CppComponentValue()
{
    delete m_metaSignatures.loadRelaxed();
    delete m_signalScopes.loadRelaxed();
}

static QString generatedSlotName(const QString &base)
{
    QString slotName = QLatin1String("on");
    int firstChar=0;
    while (firstChar < base.size()) {
        QChar c = base.at(firstChar);
        slotName += c.toUpper();
        ++firstChar;
        if (c != QLatin1Char('_'))
            break;
    }
    slotName += base.mid(firstChar);
    return slotName;
}

const CppComponentValue *CppComponentValue::asCppComponentValue() const
{
    return this;
}

void CppComponentValue::processMembers(MemberProcessor *processor) const
{
    // process the meta enums
    for (int index = m_metaObject->enumeratorOffset(); index < m_metaObject->enumeratorCount(); ++index) {
        FakeMetaEnum e = m_metaObject->enumerator(index);

        for (int i = 0; i < e.keyCount(); ++i) {
            processor->processEnumerator(e.key(i), valueOwner()->numberValue());
        }
    }

    // all explicitly defined signal names
    QSet<QString> explicitSignals;

    // make MetaFunction instances lazily when first needed
    QList<const Value *> *signatures = m_metaSignatures.loadRelaxed();
    if (!signatures) {
        signatures = new QList<const Value *>;
        signatures->reserve(m_metaObject->methodCount());
        for (int index = 0; index < m_metaObject->methodCount(); ++index)
            signatures->append(new MetaFunction(m_metaObject->method(index), valueOwner()));
        if (!m_metaSignatures.testAndSetOrdered(nullptr, signatures)) {
            delete signatures;
            signatures = m_metaSignatures.loadRelaxed();
        }
    }

    // process the meta methods
    for (int index = 0; index < m_metaObject->methodCount(); ++index) {
        const FakeMetaMethod method = m_metaObject->method(index);
        if (m_metaObjectRevision < method.revision())
            continue;

        const QString &methodName = m_metaObject->method(index).methodName();
        const Value *signature = signatures->at(index);

        if (method.methodType() == FakeMetaMethod::Slot && method.access() == FakeMetaMethod::Public) {
            processor->processSlot(methodName, signature);

        } else if (method.methodType() == FakeMetaMethod::Signal && method.access() != FakeMetaMethod::Private) {
            // process the signal
            processor->processSignal(methodName, signature);
            explicitSignals.insert(methodName);

            // process the generated slot
            const QString &slotName = generatedSlotName(methodName);
            processor->processGeneratedSlot(slotName, signature);
        }
    }

    // process the meta properties
    for (int index = 0; index < m_metaObject->propertyCount(); ++index) {
        const FakeMetaProperty prop = m_metaObject->property(index);
        if (m_metaObjectRevision < prop.revision())
            continue;

        const QString propertyName = prop.name();
        uint propertyFlags = PropertyInfo::Readable;
        if (isWritable(propertyName))
            propertyFlags |= PropertyInfo::Writeable;
        if (isListProperty(propertyName))
            propertyFlags |= PropertyInfo::ListType;
        if (isPointer(propertyName))
            propertyFlags |= PropertyInfo::PointerType;
        else
            propertyFlags |= PropertyInfo::ValueType;
        processor->processProperty(propertyName, valueForCppName(prop.typeName()),
                                   PropertyInfo(propertyFlags));

        // every property always has a onXyzChanged slot, even if the NOTIFY
        // signal has a different name
        QString signalName = propertyName;
        signalName += QLatin1String("Changed");
        if (!explicitSignals.contains(signalName)) {
            // process the generated slot
            const QString &slotName = generatedSlotName(signalName);
            processor->processGeneratedSlot(slotName, valueOwner()->unknownValue());
        }
    }

    // look into attached types
    const QString &attachedTypeName = m_metaObject->attachedTypeName();
    if (!attachedTypeName.isEmpty()) {
        const CppComponentValue *attachedType = valueOwner()->cppQmlTypes().objectByCppName(attachedTypeName);
        if (attachedType && attachedType != this) // ### only weak protection against infinite loops
            attachedType->processMembers(processor);
    }

    // look at extension types
    const QString &extensionTypeName = m_metaObject->extensionTypeName();
    if (!extensionTypeName.isEmpty()) {
        const CppComponentValue *extensionType = valueOwner()->cppQmlTypes().objectByCppName(extensionTypeName);
        if (extensionType && extensionType != this) // ### only weak protection against infinite loops
            extensionType->processMembers(processor);
    }

    ObjectValue::processMembers(processor);
}

const Value *CppComponentValue::valueForCppName(const QString &typeName) const
{
    const CppQmlTypes &cppTypes = valueOwner()->cppQmlTypes();

    // check in the same package/version first
    const CppComponentValue *objectValue = cppTypes.objectByQualifiedName(
                m_moduleName, typeName, m_importVersion);
    if (objectValue)
        return objectValue;

    // fallback to plain cpp name
    objectValue = cppTypes.objectByCppName(typeName);
    if (objectValue)
        return objectValue;

    // try qml builtin type names
    if (const Value *v = valueOwner()->defaultValueForBuiltinType(typeName)) {
        if (!v->asUndefinedValue())
            return v;
    }

    // map other C++ types
    if (typeName == QLatin1String("QByteArray")
            || typeName == QLatin1String("QString")) {
        return valueOwner()->stringValue();
    } else if (typeName == QLatin1String("QUrl")) {
        return valueOwner()->urlValue();
    } else if (typeName == QLatin1String("long")) {
        return valueOwner()->intValue();
    }  else if (typeName == QLatin1String("float")
                || typeName == QLatin1String("qreal")) {
        return valueOwner()->realValue();
    } else if (typeName == QLatin1String("QFont")) {
        return valueOwner()->qmlFontObject();
    } else if (typeName == QLatin1String("QPalette")) {
        return valueOwner()->qmlPaletteObject();
    } else if (typeName == QLatin1String("QPoint")
            || typeName == QLatin1String("QPointF")
            || typeName == QLatin1String("QVector2D")) {
        return valueOwner()->qmlPointObject();
    } else if (typeName == QLatin1String("QSize")
            || typeName == QLatin1String("QSizeF")) {
        return valueOwner()->qmlSizeObject();
    } else if (typeName == QLatin1String("QRect")
            || typeName == QLatin1String("QRectF")) {
        return valueOwner()->qmlRectObject();
    } else if (typeName == QLatin1String("QVector3D")) {
        return valueOwner()->qmlVector3DObject();
    } else if (typeName == QLatin1String("QColor")) {
        return valueOwner()->colorValue();
    } else if (typeName == QLatin1String("QDeclarativeAnchorLine")) {
        return valueOwner()->anchorLineValue();
    }

    // might be an enum
    const CppComponentValue *base = this;
    const QStringList components = typeName.split(QLatin1String("::"));
    if (components.size() == 2)
        base = valueOwner()->cppQmlTypes().objectByCppName(components.first());
    if (base) {
        if (const QmlEnumValue *value = base->getEnumValue(components.last()))
            return value;
    }

    // may still be a cpp based value
    return valueOwner()->unknownValue();
}

const CppComponentValue *CppComponentValue::prototype() const
{
    Q_ASSERT(!_prototype || value_cast<CppComponentValue>(_prototype));
    return static_cast<const CppComponentValue *>(_prototype);
}

/*!
  Returns a list started by this object and followed by all its prototypes.

  Use this function rather than calling prototype() in a loop, as it avoids
  cycles.
*/
QList<const CppComponentValue *> CppComponentValue::prototypes() const
{
    QList<const CppComponentValue *> protos;
    for (const CppComponentValue *it = this; it; it = it->prototype()) {
        if (protos.contains(it))
            break;
        protos += it;
    }
    return protos;
}

FakeMetaObject::ConstPtr CppComponentValue::metaObject() const
{
    return m_metaObject;
}

QString CppComponentValue::moduleName() const
{ return m_moduleName; }

ComponentVersion CppComponentValue::componentVersion() const
{ return m_componentVersion; }

ComponentVersion CppComponentValue::importVersion() const
{ return m_importVersion; }

QString CppComponentValue::defaultPropertyName() const
{ return m_metaObject->defaultPropertyName(); }

QString CppComponentValue::propertyType(const QString &propertyName) const
{
    foreach (const CppComponentValue *it, prototypes()) {
        FakeMetaObject::ConstPtr iter = it->m_metaObject;
        int propIdx = iter->propertyIndex(propertyName);
        if (propIdx != -1)
            return iter->property(propIdx).typeName();
    }
    return QString();
}

bool CppComponentValue::isListProperty(const QString &propertyName) const
{
    foreach (const CppComponentValue *it, prototypes()) {
        FakeMetaObject::ConstPtr iter = it->m_metaObject;
        int propIdx = iter->propertyIndex(propertyName);
        if (propIdx != -1)
            return iter->property(propIdx).isList();
    }
    return false;
}

FakeMetaEnum CppComponentValue::getEnum(const QString &typeName, const CppComponentValue **foundInScope) const
{
    foreach (const CppComponentValue *it, prototypes()) {
        FakeMetaObject::ConstPtr iter = it->m_metaObject;
        const int index = iter->enumeratorIndex(typeName);
        if (index != -1) {
            if (foundInScope)
                *foundInScope = it;
            return iter->enumerator(index);
        }
    }
    if (foundInScope)
        *foundInScope = nullptr;
    return FakeMetaEnum();
}

const QmlEnumValue *CppComponentValue::getEnumValue(const QString &typeName, const CppComponentValue **foundInScope) const
{
    foreach (const CppComponentValue *it, prototypes()) {
        if (const QmlEnumValue *e = it->m_enums.value(typeName)) {
            if (foundInScope)
                *foundInScope = it;
            return e;
        }
    }
    if (foundInScope)
        *foundInScope = nullptr;
    return nullptr;
}

const ObjectValue *CppComponentValue::signalScope(const QString &signalName) const
{
    QHash<QString, const ObjectValue *> *scopes = m_signalScopes.loadRelaxed();
    if (!scopes) {
        scopes = new QHash<QString, const ObjectValue *>;
        // usually not all methods are signals
        scopes->reserve(m_metaObject->methodCount() / 2);
        for (int index = 0; index < m_metaObject->methodCount(); ++index) {
            const FakeMetaMethod &method = m_metaObject->method(index);
            if (method.methodType() != FakeMetaMethod::Signal || method.access() == FakeMetaMethod::Private)
                continue;

            const QStringList &parameterNames = method.parameterNames();
            const QStringList &parameterTypes = method.parameterTypes();
            QTC_ASSERT(parameterNames.size() == parameterTypes.size(), continue);

            ObjectValue *scope = valueOwner()->newObject(/*prototype=*/nullptr);
            for (int i = 0; i < parameterNames.size(); ++i) {
                const QString &name = parameterNames.at(i);
                const QString &type = parameterTypes.at(i);
                if (name.isEmpty())
                    continue;
                scope->setMember(name, valueForCppName(type));
            }
            scopes->insert(generatedSlotName(method.methodName()), scope);
        }
        if (!m_signalScopes.testAndSetOrdered(nullptr, scopes)) {
            delete scopes;
            scopes = m_signalScopes.loadRelaxed();
        }
    }

    return scopes->value(signalName);
}

bool CppComponentValue::isWritable(const QString &propertyName) const
{
    foreach (const CppComponentValue *it, prototypes()) {
        FakeMetaObject::ConstPtr iter = it->m_metaObject;
        int propIdx = iter->propertyIndex(propertyName);
        if (propIdx != -1)
            return iter->property(propIdx).isWritable();
    }
    return false;
}

bool CppComponentValue::isPointer(const QString &propertyName) const
{
    foreach (const CppComponentValue *it, prototypes()) {
        FakeMetaObject::ConstPtr iter = it->m_metaObject;
        int propIdx = iter->propertyIndex(propertyName);
        if (propIdx != -1)
            return iter->property(propIdx).isPointer();
    }
    return false;
}

bool CppComponentValue::hasLocalProperty(const QString &typeName) const
{
    int idx = m_metaObject->propertyIndex(typeName);
    if (idx == -1)
        return false;
    return true;
}

bool CppComponentValue::hasProperty(const QString &propertyName) const
{
    foreach (const CppComponentValue *it, prototypes()) {
        FakeMetaObject::ConstPtr iter = it->m_metaObject;
        int propIdx = iter->propertyIndex(propertyName);
        if (propIdx != -1)
            return true;
    }
    return false;
}

bool CppComponentValue::isDerivedFrom(FakeMetaObject::ConstPtr base) const
{
    foreach (const CppComponentValue *it, prototypes()) {
        FakeMetaObject::ConstPtr iter = it->m_metaObject;
        if (iter == base)
            return true;
    }
    return false;
}

QmlEnumValue::QmlEnumValue(const CppComponentValue *owner, int enumIndex)
    : m_owner(owner)
    , m_enumIndex(enumIndex)
{
    owner->valueOwner()->registerValue(this);
}

QmlEnumValue::~QmlEnumValue()
{
}

const QmlEnumValue *QmlEnumValue::asQmlEnumValue() const
{
    return this;
}

QString QmlEnumValue::name() const
{
    return m_owner->metaObject()->enumerator(m_enumIndex).name();
}

QStringList QmlEnumValue::keys() const
{
    return m_owner->metaObject()->enumerator(m_enumIndex).keys();
}

const CppComponentValue *QmlEnumValue::owner() const
{
    return m_owner;
}

////////////////////////////////////////////////////////////////////////////////
// ValueVisitor
////////////////////////////////////////////////////////////////////////////////
ValueVisitor::ValueVisitor()
{
}

ValueVisitor::~ValueVisitor()
{
}

void ValueVisitor::visit(const NullValue *)
{
}

void ValueVisitor::visit(const UndefinedValue *)
{
}

void ValueVisitor::visit(const UnknownValue *)
{
}

void ValueVisitor::visit(const NumberValue *)
{
}

void ValueVisitor::visit(const BooleanValue *)
{
}

void ValueVisitor::visit(const StringValue *)
{
}

void ValueVisitor::visit(const ObjectValue *)
{
}

void ValueVisitor::visit(const FunctionValue *)
{
}

void ValueVisitor::visit(const Reference *)
{
}

void ValueVisitor::visit(const ColorValue *)
{
}

void ValueVisitor::visit(const AnchorLineValue *)
{
}

////////////////////////////////////////////////////////////////////////////////
// Value
////////////////////////////////////////////////////////////////////////////////
Value::Value()
{
}

Value::~Value()
{
}

bool Value::getSourceLocation(Utils::FilePath *, int *, int *) const
{
    return false;
}

const NullValue *Value::asNullValue() const
{
    return nullptr;
}

const UndefinedValue *Value::asUndefinedValue() const
{
    return nullptr;
}

const UnknownValue *Value::asUnknownValue() const
{
    return nullptr;
}

const NumberValue *Value::asNumberValue() const
{
    return nullptr;
}

const IntValue *Value::asIntValue() const
{
    return nullptr;
}

const RealValue *Value::asRealValue() const
{
    return nullptr;
}

const BooleanValue *Value::asBooleanValue() const
{
    return nullptr;
}

const StringValue *Value::asStringValue() const
{
    return nullptr;
}

const UrlValue *Value::asUrlValue() const
{
    return nullptr;
}

const ObjectValue *Value::asObjectValue() const
{
    return nullptr;
}

const FunctionValue *Value::asFunctionValue() const
{
    return nullptr;
}

const Reference *Value::asReference() const
{
    return nullptr;
}

const ColorValue *Value::asColorValue() const
{
    return nullptr;
}

const AnchorLineValue *Value::asAnchorLineValue() const
{
    return nullptr;
}

const CppComponentValue *Value::asCppComponentValue() const
{
    return nullptr;
}

const ASTObjectValue *Value::asAstObjectValue() const
{
    return nullptr;
}

const QmlEnumValue *Value::asQmlEnumValue() const
{
    return nullptr;
}

const QmlPrototypeReference *Value::asQmlPrototypeReference() const
{
    return nullptr;
}

const ASTPropertyReference *Value::asAstPropertyReference() const
{
    return nullptr;
}

const ASTVariableReference *Value::asAstVariableReference() const
{
    return nullptr;
}

const Internal::QtObjectPrototypeReference *Value::asQtObjectPrototypeReference() const
{
    return nullptr;
}

const ASTSignal *Value::asAstSignal() const
{
    return nullptr;
}

const ASTFunctionValue *Value::asAstFunctionValue() const
{
    return nullptr;
}

const Function *Value::asFunction() const
{
    return nullptr;
}

const MetaFunction *Value::asMetaFunction() const
{
    return nullptr;
}

const JSImportScope *Value::asJSImportScope() const
{
    return nullptr;
}

const TypeScope *Value::asTypeScope() const
{
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Values
////////////////////////////////////////////////////////////////////////////////
const NullValue *NullValue::asNullValue() const
{
    return this;
}

void NullValue::accept(ValueVisitor *visitor) const
{
    visitor->visit(this);
}

const UndefinedValue *UndefinedValue::asUndefinedValue() const
{
    return this;
}

void UnknownValue::accept(ValueVisitor *visitor) const
{
    visitor->visit(this);
}

const UnknownValue *UnknownValue::asUnknownValue() const
{
    return this;
}

void UndefinedValue::accept(ValueVisitor *visitor) const
{
    visitor->visit(this);
}
const NumberValue *NumberValue::asNumberValue() const
{
    return this;
}

const RealValue *RealValue::asRealValue() const
{
    return this;
}

const IntValue *IntValue::asIntValue() const
{
    return this;
}

void NumberValue::accept(ValueVisitor *visitor) const
{
    visitor->visit(this);
}

const BooleanValue *BooleanValue::asBooleanValue() const
{
    return this;
}

void BooleanValue::accept(ValueVisitor *visitor) const
{
    visitor->visit(this);
}

const StringValue *StringValue::asStringValue() const
{
    return this;
}

const UrlValue *UrlValue::asUrlValue() const
{
    return this;
}

void StringValue::accept(ValueVisitor *visitor) const
{
    visitor->visit(this);
}

Reference::Reference(ValueOwner *valueOwner)
    : m_valueOwner(valueOwner)
{
    m_valueOwner->registerValue(this);
}

Reference::~Reference()
{
}

ValueOwner *Reference::valueOwner() const
{
    return m_valueOwner;
}

const Reference *Reference::asReference() const
{
    return this;
}

void Reference::accept(ValueVisitor *visitor) const
{
    visitor->visit(this);
}

const Value *Reference::value(ReferenceContext *) const
{
    return m_valueOwner->undefinedValue();
}

void ColorValue::accept(ValueVisitor *visitor) const
{
    visitor->visit(this);
}

const ColorValue *ColorValue::asColorValue() const
{
    return this;
}

void AnchorLineValue::accept(ValueVisitor *visitor) const
{
    visitor->visit(this);
}

const AnchorLineValue *AnchorLineValue::asAnchorLineValue() const
{
    return this;
}

MemberProcessor::MemberProcessor()
{
}

MemberProcessor::~MemberProcessor()
{
}

bool MemberProcessor::processProperty(const QString &, const Value *, const PropertyInfo &)
{
    return true;
}

bool MemberProcessor::processEnumerator(const QString &, const Value *)
{
    return true;
}

bool MemberProcessor::processSignal(const QString &, const Value *)
{
    return true;
}

bool MemberProcessor::processSlot(const QString &, const Value *)
{
    return true;
}

bool MemberProcessor::processGeneratedSlot(const QString &, const Value *)
{
    return true;
}

ObjectValue::ObjectValue(ValueOwner *valueOwner, const QString &originId)
    : m_valueOwner(valueOwner), m_originId(originId),
      _prototype(nullptr)
{
    valueOwner->registerValue(this);
}

ObjectValue::~ObjectValue()
{
}

ValueOwner *ObjectValue::valueOwner() const
{
    return m_valueOwner;
}

QString ObjectValue::className() const
{
    return m_className;
}

void ObjectValue::setClassName(const QString &className)
{
    m_className = className;
}

const Value *ObjectValue::prototype() const
{
    return _prototype;
}

const ObjectValue *ObjectValue::prototype(const Context *context) const
{
    const ObjectValue *prototypeObject = value_cast<ObjectValue>(_prototype);
    if (! prototypeObject) {
        if (const Reference *prototypeReference = value_cast<Reference>(_prototype))
            prototypeObject = value_cast<ObjectValue>(context->lookupReference(prototypeReference));
    }
    return prototypeObject;
}

void ObjectValue::setPrototype(const Value *prototype)
{
    _prototype = prototype;
}

void ObjectValue::setMember(const QString &name, const Value *value)
{
    m_members[name].value = value;
}

void ObjectValue::setMember(QStringView name, const Value *value)
{
    m_members[name.toString()].value = value;
}

void ObjectValue::setPropertyInfo(const QString &name, const PropertyInfo &propertyInfo)
{
    m_members[name].propertyInfo = propertyInfo;
}

void ObjectValue::removeMember(const QString &name)
{
    m_members.remove(name);
}

const ObjectValue *ObjectValue::asObjectValue() const
{
    return this;
}

void ObjectValue::accept(ValueVisitor *visitor) const
{
    visitor->visit(this);
}

void ObjectValue::processMembers(MemberProcessor *processor) const
{
    for (auto it = m_members.cbegin(), end = m_members.cend(); it != end; ++it) {
        if (! processor->processProperty(it.key(), it.value().value, it.value().propertyInfo))
            break;
    }
}

const Value *ObjectValue::lookupMember(const QString &name, const Context *context,
                                       const ObjectValue **foundInObject,
                                       bool examinePrototypes) const
{
    if (const Value *m = m_members.value(name).value) {
        if (foundInObject)
            *foundInObject = this;
        return m;
    } else {
        LookupMember slowLookup(name);
        processMembers(&slowLookup);
        if (slowLookup.value()) {
            if (foundInObject)
                *foundInObject = this;
            return slowLookup.value();
        }
    }

    const ObjectValue *prototypeObject = nullptr;

    if (examinePrototypes && context) {
        PrototypeIterator iter(this, context);
        iter.next(); // skip this
        while (iter.hasNext()) {
            prototypeObject = iter.next();
            if (const Value *m = prototypeObject->lookupMember(name, context, foundInObject, false))
                return m;
        }
    }

    if (foundInObject)
        *foundInObject = nullptr;

    return nullptr;
}

PrototypeIterator::PrototypeIterator(const ObjectValue *start, const Context *context)
    : m_current(nullptr)
    , m_next(start)
    , m_context(context)
    , m_error(NoError)
{
    if (start)
        m_prototypes.reserve(10);
}

PrototypeIterator::PrototypeIterator(const ObjectValue *start, const ContextPtr &context)
    : m_current(nullptr)
    , m_next(start)
    , m_context(context.data())
    , m_error(NoError)
{
    if (start)
        m_prototypes.reserve(10);
}

bool PrototypeIterator::hasNext()
{
    if (m_next)
        return true;
    if (!m_current)
        return false;
    const Value *proto = m_current->prototype();
    if (!proto)
        return false;

    m_next = value_cast<ObjectValue>(proto);
    if (! m_next)
        m_next = value_cast<ObjectValue>(m_context->lookupReference(proto));
    if (!m_next) {
        m_error = ReferenceResolutionError;
        return false;
    }
    if (m_prototypes.contains(m_next)) {
        m_error = CycleError;
        m_next = nullptr;
        return false;
    }
    return true;
}

const ObjectValue *PrototypeIterator::next()
{
    if (hasNext()) {
        m_current = m_next;
        m_prototypes += m_next;
        m_next = nullptr;
        return m_current;
    }
    return nullptr;
}

const ObjectValue *PrototypeIterator::peekNext()
{
    if (hasNext())
        return m_next;
    return nullptr;
}

PrototypeIterator::Error PrototypeIterator::error() const
{
    return m_error;
}

QList<const ObjectValue *> PrototypeIterator::all()
{
    while (hasNext())
        next();
    return m_prototypes;
}

FunctionValue::FunctionValue(ValueOwner *valueOwner)
    : ObjectValue(valueOwner)
{
    setClassName(QLatin1String("Function"));
    setMember(QLatin1String("length"), valueOwner->numberValue());
    setPrototype(valueOwner->functionPrototype());
}

FunctionValue::~FunctionValue()
{
}

const Value *FunctionValue::returnValue() const
{
    return valueOwner()->unknownValue();
}

int FunctionValue::namedArgumentCount() const
{
    return 0;
}

const Value *FunctionValue::argument(int) const
{
    return valueOwner()->unknownValue();
}

QString FunctionValue::argumentName(int index) const
{
    return QString::fromLatin1("arg%1").arg(index + 1);
}

int FunctionValue::optionalNamedArgumentCount() const
{
    return 0;
}

bool FunctionValue::isVariadic() const
{
    return true;
}

const FunctionValue *FunctionValue::asFunctionValue() const
{
    return this;
}

void FunctionValue::accept(ValueVisitor *visitor) const
{
    visitor->visit(this);
}

Function::Function(ValueOwner *valueOwner)
    : FunctionValue(valueOwner)
    , m_returnValue(nullptr)
    , m_optionalNamedArgumentCount(0)
    , m_isVariadic(false)
{
}

Function::~Function()
{
}

void Function::addArgument(const Value *argument, const QString &name)
{
    if (!name.isEmpty()) {
        while (m_argumentNames.size() < m_arguments.size())
            m_argumentNames.push_back(QString());
        m_argumentNames.push_back(name);
    }
    m_arguments.push_back(argument);
}

const Value *Function::returnValue() const
{
    return m_returnValue;
}

void Function::setReturnValue(const Value *returnValue)
{
    m_returnValue = returnValue;
}

void Function::setVariadic(bool variadic)
{
    m_isVariadic = variadic;
}

void Function::setOptionalNamedArgumentCount(int count)
{
    m_optionalNamedArgumentCount = count;
}

int Function::namedArgumentCount() const
{
    return m_arguments.size();
}

int Function::optionalNamedArgumentCount() const
{
    return m_optionalNamedArgumentCount;
}

const Value *Function::argument(int index) const
{
    return m_arguments.at(index);
}

QString Function::argumentName(int index) const
{
    if (index < m_argumentNames.size()) {
        const QString name = m_argumentNames.at(index);
        if (!name.isEmpty())
            return m_argumentNames.at(index);
    }
    return FunctionValue::argumentName(index);
}

bool Function::isVariadic() const
{
    return m_isVariadic;
}

const Function *Function::asFunction() const
{
    return this;
}

////////////////////////////////////////////////////////////////////////////////
// typing environment
////////////////////////////////////////////////////////////////////////////////

CppQmlTypesLoader::BuiltinObjects CppQmlTypesLoader::defaultLibraryObjects;
CppQmlTypesLoader::BuiltinObjects CppQmlTypesLoader::defaultQtObjects;

CppQmlTypesLoader::BuiltinObjects CppQmlTypesLoader::loadQmlTypes(const QFileInfoList &qmlTypeFiles, QStringList *errors, QStringList *warnings)
{
    QHash<QString, FakeMetaObject::ConstPtr> newObjects;
    QStringList newDependencies;

    foreach (const QFileInfo &qmlTypeFile, qmlTypeFiles) {
        QString error, warning;
        QFile file(qmlTypeFile.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray contents = file.readAll();
            file.close();


            parseQmlTypeDescriptions(contents, &newObjects, nullptr, &newDependencies, &error, &warning,
                                     qmlTypeFile.absoluteFilePath());
        } else {
            error = file.errorString();
        }
        if (!error.isEmpty()) {
            errors->append(TypeDescriptionReader::tr(
                               "Errors while loading qmltypes from %1:\n%2").arg(
                               qmlTypeFile.absoluteFilePath(), error));
        }
        if (!warning.isEmpty()) {
            warnings->append(TypeDescriptionReader::tr(
                                 "Warnings while loading qmltypes from %1:\n%2").arg(
                                 qmlTypeFile.absoluteFilePath(), warning));
        }
    }

    return newObjects;
}

void CppQmlTypesLoader::parseQmlTypeDescriptions(const QByteArray &contents,
                                                 BuiltinObjects *newObjects,
                                                 QList<ModuleApiInfo> *newModuleApis,
                                                 QStringList *newDependencies,
                                                 QString *errorMessage,
                                                 QString *warningMessage, const QString &fileName)
{
    if (contents.isEmpty())
        return;
    unsigned char c = contents.at(0);
    switch (c) {
    case 0xfe:
    case 0xef:
    case 0xff:
    case 0xee:
    case 0x00:
        qWarning() << fileName << "seems not to be encoded in UTF8 or has a BOM.";
    default: break;
    }

    errorMessage->clear();
    warningMessage->clear();
    TypeDescriptionReader reader(fileName, QString::fromUtf8(contents));
    if (!reader(newObjects, newModuleApis, newDependencies)) {
        if (reader.errorMessage().isEmpty())
            *errorMessage = QLatin1String("unknown error");
        else
            *errorMessage = reader.errorMessage();
    }
    *warningMessage = reader.warningMessage();
}

CppQmlTypes::CppQmlTypes(ValueOwner *valueOwner)
    : m_cppContextProperties(nullptr)
    , m_valueOwner(valueOwner)

{
}

const QLatin1String CppQmlTypes::defaultPackage("<default>");
const QLatin1String CppQmlTypes::cppPackage("<cpp>");

template <typename T>
void CppQmlTypes::load(const QString &originId, const T &fakeMetaObjects, const QString &overridePackage)
{
    QList<CppComponentValue *> newCppTypes;
    foreach (const FakeMetaObject::ConstPtr &fmo, fakeMetaObjects) {
        foreach (const FakeMetaObject::Export &exp, fmo->exports()) {
            QString package = exp.package;
            if (package.isEmpty())
                package = overridePackage;
            m_fakeMetaObjectsByPackage[package].insert(FakeMetaObjectWithOrigin(fmo, originId));

            // make versionless cpp types directly
            // needed for access to property types that are not exported, like QDeclarativeAnchors
            if (exp.package == cppPackage) {
                QTC_ASSERT(exp.version == ComponentVersion(), continue);
                QTC_ASSERT(exp.type == fmo->className(), continue);
                CppComponentValue *cppValue = new CppComponentValue(
                            fmo, fmo->className(), cppPackage, ComponentVersion(), ComponentVersion(),
                            ComponentVersion::MaxVersion, m_valueOwner, originId);
                m_objectsByQualifiedName[qualifiedName(cppPackage, fmo->className(), ComponentVersion())] = cppValue;
                newCppTypes += cppValue;
            }
        }
    }

    // set prototypes of cpp types
    foreach (CppComponentValue *object, newCppTypes) {
        const QString &protoCppName = object->metaObject()->superclassName();
        const CppComponentValue *proto = objectByCppName(protoCppName);
        if (proto)
            object->setPrototype(proto);
    }
}

// explicitly instantiate load for list and hash
template QMLJS_EXPORT void CppQmlTypes::load< QList<FakeMetaObject::ConstPtr> >(const QString &, const QList<FakeMetaObject::ConstPtr> &, const QString &);
template QMLJS_EXPORT void CppQmlTypes::load< QHash<QString, FakeMetaObject::ConstPtr> >(const QString &, const QHash<QString, FakeMetaObject::ConstPtr> &, const QString &);

QList<const CppComponentValue *> CppQmlTypes::createObjectsForImport(const QString &package, ComponentVersion version)
{
    QHash<QString, const CppComponentValue *> exportedObjects;

    QList<const CppComponentValue *> newObjects;

    // make new exported objects
    foreach (const FakeMetaObjectWithOrigin &fmoo, m_fakeMetaObjectsByPackage.value(package)) {
        const FakeMetaObject::ConstPtr &fmo = fmoo.fakeMetaObject;
        // find the highest-version export for each alias
        QHash<QString, FakeMetaObject::Export> bestExports;
        foreach (const FakeMetaObject::Export &exp, fmo->exports()) {
            if (exp.package != package || (version.isValid() && exp.version > version))
                continue;

            if (bestExports.contains(exp.type)) {
                if (exp.version > bestExports.value(exp.type).version)
                    bestExports.insert(exp.type, exp);
            } else {
                bestExports.insert(exp.type, exp);
            }
        }
        if (bestExports.isEmpty())
            continue;

        // if it already exists, skip
        const QString key = qualifiedName(package, fmo->className(), version);
        if (m_objectsByQualifiedName.contains(key))
            continue;

        ComponentVersion cppVersion;
        foreach (const FakeMetaObject::Export &bestExport, bestExports) {
            QString name = bestExport.type;
            bool exported = true;
            if (name.isEmpty()) {
                exported = false;
                name = fmo->className();
            }

            CppComponentValue *newComponent = new CppComponentValue(
                        fmo, name, package, bestExport.version, version,
                        bestExport.metaObjectRevision, m_valueOwner,
                        fmoo.originId);

            // use package.cppname importversion as key
            if (cppVersion <= bestExport.version) {
                cppVersion = bestExport.version;
                m_objectsByQualifiedName.insert(key, newComponent);
            }
            if (exported) {
                if (!exportedObjects.contains(name) // we might have the same type in different versions
                        || (newComponent->componentVersion() > exportedObjects.value(name)->componentVersion()))
                    exportedObjects.insert(name, newComponent);
            }
            newObjects += newComponent;
        }
    }

    // set their prototypes, creating them if necessary
    // this ensures that the prototypes of C++ objects are resolved correctly and with the correct
    // revision, and cannot be hidden by other objects.
    foreach (const CppComponentValue *cobject, newObjects) {
        CppComponentValue *object = const_cast<CppComponentValue *>(cobject);
        while (!object->prototype()) {
            const QString &protoCppName = object->metaObject()->superclassName();
            if (protoCppName.isEmpty())
                break;

            // if the prototype already exists, done
            const QString key = qualifiedName(object->moduleName(), protoCppName, version);
            if (const CppComponentValue *proto = m_objectsByQualifiedName.value(key)) {
                object->setPrototype(proto);
                break;
            }

            // get the fmo via the cpp name
            const CppComponentValue *cppProto = objectByCppName(protoCppName);
            if (!cppProto)
                break;
            FakeMetaObject::ConstPtr protoFmo = cppProto->metaObject();

            // make a new object
            CppComponentValue *proto = new CppComponentValue(
                        protoFmo, protoCppName, object->moduleName(),
                        ComponentVersion(),
                        object->importVersion(), ComponentVersion::MaxVersion, m_valueOwner,
                        cppProto->originId());
            m_objectsByQualifiedName.insert(key, proto);
            object->setPrototype(proto);

            // maybe set prototype of prototype
            object = proto;
        }
    }

    return exportedObjects.values();
}

bool CppQmlTypes::hasModule(const QString &module) const
{
    return m_fakeMetaObjectsByPackage.contains(module);
}

QString CppQmlTypes::qualifiedName(const QString &module, const QString &type, ComponentVersion version)
{
    return QString::fromLatin1("%1/%2 %3").arg(
                module, type,
                version.toString());

}

const CppComponentValue *CppQmlTypes::objectByQualifiedName(const QString &name) const
{
    return m_objectsByQualifiedName.value(name);
}

const CppComponentValue *CppQmlTypes::objectByQualifiedName(const QString &package, const QString &type,
                                                         ComponentVersion version) const
{
    return objectByQualifiedName(qualifiedName(package, type, version));
}

const CppComponentValue *CppQmlTypes::objectByCppName(const QString &cppName) const
{
    return objectByQualifiedName(qualifiedName(cppPackage, cppName, ComponentVersion()));
}

void CppQmlTypes::setCppContextProperties(const ObjectValue *contextProperties)
{
    m_cppContextProperties = contextProperties;
}

const ObjectValue *CppQmlTypes::cppContextProperties() const
{
    return m_cppContextProperties;
}


ConvertToNumber::ConvertToNumber(ValueOwner *valueOwner)
    : m_valueOwner(valueOwner), m_result(nullptr)
{
}

const Value *ConvertToNumber::operator()(const Value *value)
{
    const Value *previousValue = switchResult(nullptr);

    if (value)
        value->accept(this);

    return switchResult(previousValue);
}

const Value *ConvertToNumber::switchResult(const Value *value)
{
    const Value *previousResult = m_result;
    m_result = value;
    return previousResult;
}

void ConvertToNumber::visit(const NullValue *)
{
    m_result = m_valueOwner->numberValue();
}

void ConvertToNumber::visit(const UndefinedValue *)
{
    m_result = m_valueOwner->numberValue();
}

void ConvertToNumber::visit(const NumberValue *value)
{
    m_result = value;
}

void ConvertToNumber::visit(const BooleanValue *)
{
    m_result = m_valueOwner->numberValue();
}

void ConvertToNumber::visit(const StringValue *)
{
    m_result = m_valueOwner->numberValue();
}

void ConvertToNumber::visit(const ObjectValue *object)
{
    if (const FunctionValue *valueOfMember = value_cast<FunctionValue>(
                object->lookupMember(QLatin1String("valueOf"), ContextPtr()))) {
        m_result = value_cast<NumberValue>(valueOfMember->returnValue());
    }
}

void ConvertToNumber::visit(const FunctionValue *object)
{
    if (const FunctionValue *valueOfMember = value_cast<FunctionValue>(
                object->lookupMember(QLatin1String("valueOf"), ContextPtr()))) {
        m_result = value_cast<NumberValue>(valueOfMember->returnValue());
    }
}

ConvertToString::ConvertToString(ValueOwner *valueOwner)
    : m_valueOwner(valueOwner), m_result(nullptr)
{
}

const Value *ConvertToString::operator()(const Value *value)
{
    const Value *previousValue = switchResult(nullptr);

    if (value)
        value->accept(this);

    return switchResult(previousValue);
}

const Value *ConvertToString::switchResult(const Value *value)
{
    const Value *previousResult = m_result;
    m_result = value;
    return previousResult;
}

void ConvertToString::visit(const NullValue *)
{
    m_result = m_valueOwner->stringValue();
}

void ConvertToString::visit(const UndefinedValue *)
{
    m_result = m_valueOwner->stringValue();
}

void ConvertToString::visit(const NumberValue *)
{
    m_result = m_valueOwner->stringValue();
}

void ConvertToString::visit(const BooleanValue *)
{
    m_result = m_valueOwner->stringValue();
}

void ConvertToString::visit(const StringValue *value)
{
    m_result = value;
}

void ConvertToString::visit(const ObjectValue *object)
{
    if (const FunctionValue *toStringMember = value_cast<FunctionValue>(
                object->lookupMember(QLatin1String("toString"), ContextPtr()))) {
        m_result = value_cast<StringValue>(toStringMember->returnValue());
    }
}

void ConvertToString::visit(const FunctionValue *object)
{
    if (const FunctionValue *toStringMember = value_cast<FunctionValue>(
                object->lookupMember(QLatin1String("toString"), ContextPtr()))) {
        m_result = value_cast<StringValue>(toStringMember->returnValue());
    }
}

ConvertToObject::ConvertToObject(ValueOwner *valueOwner)
    : m_valueOwner(valueOwner), m_result(nullptr)
{
}

const Value *ConvertToObject::operator()(const Value *value)
{
    const Value *previousValue = switchResult(nullptr);

    if (value)
        value->accept(this);

    return switchResult(previousValue);
}

const Value *ConvertToObject::switchResult(const Value *value)
{
    const Value *previousResult = m_result;
    m_result = value;
    return previousResult;
}

void ConvertToObject::visit(const NullValue *value)
{
    m_result = value;
}

void ConvertToObject::visit(const UndefinedValue *)
{
    m_result = m_valueOwner->nullValue();
}

void ConvertToObject::visit(const NumberValue *)
{
    m_result = m_valueOwner->numberCtor()->returnValue();
}

void ConvertToObject::visit(const BooleanValue *)
{
    m_result = m_valueOwner->booleanCtor()->returnValue();
}

void ConvertToObject::visit(const StringValue *)
{
    m_result = m_valueOwner->stringCtor()->returnValue();
}

void ConvertToObject::visit(const ObjectValue *object)
{
    m_result = object;
}

void ConvertToObject::visit(const FunctionValue *object)
{
    m_result = object;
}

QString TypeId::operator()(const Value *value)
{
    _result = QLatin1String("unknown");

    if (value)
        value->accept(this);

    return _result;
}

void TypeId::visit(const NullValue *)
{
    _result = QLatin1String("null");
}

void TypeId::visit(const UndefinedValue *)
{
    _result = QLatin1String("undefined");
}

void TypeId::visit(const NumberValue *)
{
    _result = QLatin1String("number");
}

void TypeId::visit(const BooleanValue *)
{
    _result = QLatin1String("boolean");
}

void TypeId::visit(const StringValue *)
{
    _result = QLatin1String("string");
}

void TypeId::visit(const ObjectValue *object)
{
    _result = object->className();

    if (_result.isEmpty())
        _result = QLatin1String("object");
}

void TypeId::visit(const FunctionValue *object)
{
    _result = object->className();

    if (_result.isEmpty())
        _result = QLatin1String("Function");
}

void TypeId::visit(const ColorValue *)
{
    _result = QLatin1String("string");
}

void TypeId::visit(const AnchorLineValue *)
{
    _result = QLatin1String("AnchorLine");
}

ASTObjectValue::ASTObjectValue(UiQualifiedId *typeName,
                               UiObjectInitializer *initializer,
                               const Document *doc,
                               ValueOwner *valueOwner)
    : ObjectValue(valueOwner, doc->importId()),
      m_typeName(typeName), m_initializer(initializer), m_doc(doc), m_defaultPropertyRef(nullptr)
{
    if (m_initializer) {
        for (UiObjectMemberList *it = m_initializer->members; it; it = it->next) {
            UiObjectMember *member = it->member;
            if (UiPublicMember *def = cast<UiPublicMember *>(member)) {
                if (def->type == UiPublicMember::Property && !def->name.isEmpty()) {
                    ASTPropertyReference *ref = new ASTPropertyReference(def, m_doc, valueOwner);
                    m_properties.append(ref);
                    if (def->defaultToken().isValid())
                        m_defaultPropertyRef = ref;
                } else if (def->type == UiPublicMember::Signal && !def->name.isEmpty()) {
                    ASTSignal *ref = new ASTSignal(def, m_doc, valueOwner);
                    m_signals.append(ref);
                }
            }
        }
    }
}

ASTObjectValue::~ASTObjectValue()
{
}

const ASTObjectValue *ASTObjectValue::asAstObjectValue() const
{
    return this;
}

bool ASTObjectValue::getSourceLocation(Utils::FilePath *fileName, int *line, int *column) const
{
    *fileName = m_doc->fileName();
    *line = m_typeName->identifierToken.startLine;
    *column = m_typeName->identifierToken.startColumn;
    return true;
}

void ASTObjectValue::processMembers(MemberProcessor *processor) const
{
    foreach (ASTPropertyReference *ref, m_properties) {
        uint pFlags = PropertyInfo::Readable;
        if (!ref->ast()->isReadonly())
            pFlags |= PropertyInfo::Writeable;
        processor->processProperty(ref->ast()->name.toString(), ref, PropertyInfo(pFlags));
        // ### Should get a different value?
        processor->processGeneratedSlot(ref->onChangedSlotName(), ref);
    }
    foreach (ASTSignal *ref, m_signals) {
        processor->processSignal(ref->ast()->name.toString(), ref);
        // ### Should get a different value?
        processor->processGeneratedSlot(ref->slotName(), ref);
    }

    ObjectValue::processMembers(processor);
}

QString ASTObjectValue::defaultPropertyName() const
{
    if (m_defaultPropertyRef) {
        UiPublicMember *prop = m_defaultPropertyRef->ast();
        if (prop)
            return prop->name.toString();
    }
    return QString();
}

UiObjectInitializer *ASTObjectValue::initializer() const
{
    return m_initializer;
}

UiQualifiedId *ASTObjectValue::typeName() const
{
    return m_typeName;
}

const Document *ASTObjectValue::document() const
{
    return m_doc;
}

ASTVariableReference::ASTVariableReference(PatternElement *ast, const Document *doc, ValueOwner *valueOwner)
    : Reference(valueOwner)
    , m_ast(ast)
    , m_doc(doc)
{
}

ASTVariableReference::~ASTVariableReference()
{
}

const ASTVariableReference *ASTVariableReference::asAstVariableReference() const
{
    return this;
}

const PatternElement *ASTVariableReference::ast() const
{
    return m_ast;
}

const Value *ASTVariableReference::value(ReferenceContext *referenceContext) const
{
    // may be assigned to later
    ExpressionNode *exp = ((m_ast->initializer) ? m_ast->initializer : m_ast->bindingTarget);
    if (!exp)
        return valueOwner()->unknownValue();

    Document::Ptr doc = m_doc->ptr();
    ScopeChain scopeChain(doc, referenceContext->context());
    ScopeBuilder builder(&scopeChain);
    builder.push(ScopeAstPath(doc)(exp->firstSourceLocation().begin()));

    Evaluate evaluator(&scopeChain, referenceContext);
    const Value *res = evaluator(exp);
    return res;
}

bool ASTVariableReference::getSourceLocation(Utils::FilePath *fileName, int *line, int *column) const
{
    *fileName = m_doc->fileName();
    *line = m_ast->identifierToken.startLine;
    *column = m_ast->identifierToken.startColumn;
    return true;
}

namespace {
class UsesArgumentsArray : protected Visitor
{
    bool m_usesArgumentsArray;

public:
    bool operator()(StatementList *ast)
    {
        if (!ast)
            return false;
        m_usesArgumentsArray = false;
        Node::accept(ast, this);
        return m_usesArgumentsArray;
    }

protected:
    bool visit(ArrayMemberExpression *ast) override
    {
        if (IdentifierExpression *idExp = cast<IdentifierExpression *>(ast->base)) {
            if (idExp->name == QLatin1String("arguments"))
                m_usesArgumentsArray = true;
        }
        return true;
    }

    // don't go into nested functions
    bool visit(Program *) override { return false; }
    bool visit(StatementList *) override { return false; }

    void throwRecursionDepthError() override {
        qWarning("Warning: Hit maximum recursion error visiting AST in UsesArgumentsArray");
    }
};
} // anonymous namespace

ASTFunctionValue::ASTFunctionValue(FunctionExpression *ast, const Document *doc, ValueOwner *valueOwner)
    : FunctionValue(valueOwner)
    , m_ast(ast)
    , m_doc(doc)
{
    setPrototype(valueOwner->functionPrototype());

    for (FormalParameterList *it = ast->formals; it; it = it->next)
        m_argumentNames.append(it->element->bindingIdentifier.toString());

    m_isVariadic = UsesArgumentsArray()(ast->body);
}

ASTFunctionValue::~ASTFunctionValue()
{
}

FunctionExpression *ASTFunctionValue::ast() const
{
    return m_ast;
}

int ASTFunctionValue::namedArgumentCount() const
{
    return m_argumentNames.size();
}

QString ASTFunctionValue::argumentName(int index) const
{
    if (index < m_argumentNames.size()) {
        const QString &name = m_argumentNames.at(index);
        if (!name.isEmpty())
            return name;
    }

    return FunctionValue::argumentName(index);
}

bool ASTFunctionValue::isVariadic() const
{
    return m_isVariadic;
}

const ASTFunctionValue *ASTFunctionValue::asAstFunctionValue() const
{
    return this;
}

bool ASTFunctionValue::getSourceLocation(Utils::FilePath *fileName, int *line, int *column) const
{
    *fileName = m_doc->fileName();
    *line = m_ast->identifierToken.startLine;
    *column = m_ast->identifierToken.startColumn;
    return true;
}

QmlPrototypeReference::QmlPrototypeReference(UiQualifiedId *qmlTypeName, const Document *doc,
                                             ValueOwner *valueOwner)
    : Reference(valueOwner),
      m_qmlTypeName(qmlTypeName),
      m_doc(doc)
{
}

QmlPrototypeReference::~QmlPrototypeReference()
{
}

const QmlPrototypeReference *QmlPrototypeReference::asQmlPrototypeReference() const
{
    return this;
}

UiQualifiedId *QmlPrototypeReference::qmlTypeName() const
{
    return m_qmlTypeName;
}

const Document *QmlPrototypeReference::document() const
{
    return m_doc;
}

const Value *QmlPrototypeReference::value(ReferenceContext *referenceContext) const
{
    return referenceContext->context()->lookupType(m_doc, m_qmlTypeName);
}

ASTPropertyReference::ASTPropertyReference(UiPublicMember *ast, const Document *doc, ValueOwner *valueOwner)
    : Reference(valueOwner), m_ast(ast), m_doc(doc)
{
    const QString &propertyName = ast->name.toString();
    m_onChangedSlotName = generatedSlotName(propertyName);
    m_onChangedSlotName += QLatin1String("Changed");
}

ASTPropertyReference::~ASTPropertyReference()
{
}

const ASTPropertyReference *ASTPropertyReference::asAstPropertyReference() const
{
    return this;
}

bool ASTPropertyReference::getSourceLocation(Utils::FilePath *fileName, int *line, int *column) const
{
    *fileName = m_doc->fileName();
    *line = m_ast->identifierToken.startLine;
    *column = m_ast->identifierToken.startColumn;
    return true;
}

const Value *ASTPropertyReference::value(ReferenceContext *referenceContext) const
{
    if (m_ast->statement
            && (m_ast->memberType->name == QLatin1String("variant")
                || m_ast->memberType->name == QLatin1String("var")
                || m_ast->memberType->name == QLatin1String("alias"))) {

        // Adjust the context for the current location - expensive!
        // ### Improve efficiency by caching the 'use chain' constructed in ScopeBuilder.

        Document::Ptr doc = m_doc->ptr();
        ScopeChain scopeChain(doc, referenceContext->context());
        ScopeBuilder builder(&scopeChain);

        int offset = m_ast->statement->firstSourceLocation().begin();
        builder.push(ScopeAstPath(doc)(offset));

        Evaluate evaluator(&scopeChain, referenceContext);
        return evaluator(m_ast->statement);
    }

    const QString memberType = m_ast->memberType->name.toString();

    const Value *builtin = valueOwner()->defaultValueForBuiltinType(memberType);
    if (!builtin->asUndefinedValue())
        return builtin;

    if (m_ast->typeModifier.isEmpty()) {
        const Value *type = referenceContext->context()->lookupType(m_doc, QStringList(memberType));
        if (type)
            return type;
    }

    return referenceContext->context()->valueOwner()->undefinedValue();
}

ASTSignal::ASTSignal(UiPublicMember *ast, const Document *doc, ValueOwner *valueOwner)
    : FunctionValue(valueOwner), m_ast(ast), m_doc(doc)
{
    const QString &signalName = ast->name.toString();
    m_slotName = generatedSlotName(signalName);

    ObjectValue *v = valueOwner->newObject(/*prototype=*/nullptr);
    for (UiParameterList *it = ast->parameters; it; it = it->next) {
        if (!it->name.isEmpty())
            v->setMember(it->name.toString(), valueOwner->defaultValueForBuiltinType(it->type->name.toString()));
    }
    m_bodyScope = v;
}

ASTSignal::~ASTSignal()
{
}

const ASTSignal *ASTSignal::asAstSignal() const
{
    return this;
}

int ASTSignal::namedArgumentCount() const
{
    int count = 0;
    for (UiParameterList *it = m_ast->parameters; it; it = it->next)
        ++count;
    return count;
}

const Value *ASTSignal::argument(int index) const
{
    UiParameterList *param = m_ast->parameters;
    for (int i = 0; param && i < index; ++i)
        param = param->next;
    if (!param || param->type->name.isEmpty())
        return valueOwner()->unknownValue();
    return valueOwner()->defaultValueForBuiltinType(param->type->name.toString());
}

QString ASTSignal::argumentName(int index) const
{
    UiParameterList *param = m_ast->parameters;
    for (int i = 0; param && i < index; ++i)
        param = param->next;
    if (!param || param->name.isEmpty())
        return FunctionValue::argumentName(index);
    return param->name.toString();
}

bool ASTSignal::getSourceLocation(Utils::FilePath *fileName, int *line, int *column) const
{
    *fileName = m_doc->fileName();
    *line = m_ast->identifierToken.startLine;
    *column = m_ast->identifierToken.startColumn;
    return true;
}


ImportInfo::ImportInfo()
    : m_type(ImportType::Invalid)
    , m_ast(nullptr)
{
}

ImportInfo ImportInfo::moduleImport(QString uri, ComponentVersion version,
                                    const QString &as, UiImport *ast)
{
    ImportInfo info;
    info.m_type = ImportType::Library;
    info.m_name = uri;
    info.m_path = uri;
    info.m_path.replace(QLatin1Char('.'), QLatin1Char('/'));
    info.m_version = version;
    info.m_as = as;
    info.m_ast = ast;
    return info;
}

ImportInfo ImportInfo::pathImport(const Utils::FilePath &docPath,
                                  const QString &path,
                                  ComponentVersion version,
                                  const QString &as,
                                  UiImport *ast)
{
    ImportInfo info;
    info.m_name = path;

    Utils::FilePath importFilePath = Utils::FilePath::fromString(path);
    if (!importFilePath.isAbsolutePath())
        importFilePath = docPath.pathAppended(path);
    info.m_path = importFilePath.absoluteFilePath().path();

    if (importFilePath.isFile()) {
        info.m_type = ImportType::File;
    } else if (importFilePath.isDir()) {
        info.m_type = ImportType::Directory;
    } else if (path.startsWith(QLatin1String("qrc:"))) {
        ModelManagerInterface *model = ModelManagerInterface::instance();
        info.m_path = path;
        info.m_type = !model
                ? ImportType::UnknownFile
                : model->filesAtQrcPath(info.path()).isEmpty()
                  ? ImportType::QrcDirectory
                  : ImportType::QrcFile;
    } else {
        Utils::FilePath dir = docPath;
        while (dir.fileName().startsWith("+"))
            dir = dir.parentDir();

        const Utils::FilePath docPathStripped = dir.absolutePath();
        if (docPathStripped != docPath)
            return pathImport(docPathStripped, path, version, as, ast);

        info.m_type = ImportType::UnknownFile;
    }
    info.m_version = version;
    info.m_as = as;
    info.m_ast = ast;
    return info;
}

ImportInfo ImportInfo::invalidImport(UiImport *ast)
{
    ImportInfo info;
    info.m_type = ImportType::Invalid;
    info.m_ast = ast;
    return info;
}

ImportInfo ImportInfo::implicitDirectoryImport(const QString &directory)
{
    ImportInfo info;
    info.m_type = ImportType::ImplicitDirectory;
    info.m_path = directory;
    return info;
}

ImportInfo ImportInfo::qrcDirectoryImport(const QString &directory)
{
    ImportInfo info;
    info.m_type = ImportType::QrcDirectory;
    info.m_path = directory;
    return info;
}

bool ImportInfo::isValid() const
{
    return m_type != ImportType::Invalid;
}

ImportType::Enum ImportInfo::type() const
{
    return m_type;
}

QString ImportInfo::name() const
{
    return m_name;
}

QString ImportInfo::path() const
{
    return m_path;
}

QString ImportInfo::as() const
{
    return m_as;
}

ComponentVersion ImportInfo::version() const
{
    return m_version;
}

UiImport *ImportInfo::ast() const
{
    return m_ast;
}

Import::Import()
    : object(nullptr), valid(false), used(false)
{}

Import::Import(const Import &other)
    : object(other.object), info(other.info), libraryPath(other.libraryPath),
      valid(other.valid), used(false)
{ }

Import &Import::operator=(const Import &other)
{
    object = other.object;
    info = other.info;
    libraryPath = other.libraryPath;
    valid = other.valid;
    used = false;
    return *this;
}

TypeScope::TypeScope(const Imports *imports, ValueOwner *valueOwner)
    : ObjectValue(valueOwner)
    , m_imports(imports)
{
}

const Value *TypeScope::lookupMember(const QString &name, const Context *context,
                                           const ObjectValue **foundInObject, bool) const
{
    if (const ObjectValue *value = m_imports->resolveAliasAndMarkUsed(name)) {
        if (foundInObject)
            *foundInObject = this;
        return value;
    }

    const QList<Import> &imports = m_imports->all();
    for (int pos = imports.size(); --pos >= 0; ) {
        const Import &i = imports.at(pos);
        const ObjectValue *import = i.object;
        const ImportInfo &info = i.info;

        // JS import has no types
        if (info.type() == ImportType::File || info.type() == ImportType::QrcFile)
            continue;

        if (const Value *v = import->lookupMember(name, context, foundInObject)) {
            // FIXME if we have multiple non-aliased imports containing this object we'd have to
            // disambiguate (and inform the user) about this issue
            if (info.as().isEmpty()) {
                i.used = true;
                return v;
            }
        }
    }
    if (foundInObject)
        *foundInObject = nullptr;
    return nullptr;
}

void TypeScope::processMembers(MemberProcessor *processor) const
{
    const QList<Import> &imports = m_imports->all();
    for (int pos = imports.size(); --pos >= 0; ) {
        const Import &i = imports.at(pos);
        const ObjectValue *import = i.object;
        const ImportInfo &info = i.info;

        // JS import has no types
        if (info.type() == ImportType::File || info.type() == ImportType::QrcFile)
            continue;

        if (!info.as().isEmpty())
            processor->processProperty(info.as(), import, PropertyInfo(PropertyInfo::Readable));
        else
            import->processMembers(processor);
    }
}

const TypeScope *TypeScope::asTypeScope() const
{
    return this;
}

JSImportScope::JSImportScope(const Imports *imports, ValueOwner *valueOwner)
    : ObjectValue(valueOwner)
    , m_imports(imports)
{
}

const Value *JSImportScope::lookupMember(const QString &name, const Context *,
                                         const ObjectValue **foundInObject, bool) const
{
    const ObjectValue *value = m_imports->resolveAliasAndMarkUsed(name);
    if (foundInObject)
        *foundInObject = value ? this : nullptr;
    return value;
}

void JSImportScope::processMembers(MemberProcessor *processor) const
{
    const QList<Import> &imports = m_imports->all();
    for (int pos = imports.size(); --pos >= 0; ) {
        const Import &i = imports.at(pos);
        const ObjectValue *import = i.object;
        const ImportInfo &info = i.info;

        if (info.type() == ImportType::File || info.type() == ImportType::QrcFile)
            processor->processProperty(info.as(), import, PropertyInfo(PropertyInfo::Readable));
    }
}

const JSImportScope *JSImportScope::asJSImportScope() const
{
    return this;
}

Imports::Imports(ValueOwner *valueOwner)
    : m_typeScope(new TypeScope(this, valueOwner))
    , m_jsImportScope(new JSImportScope(this, valueOwner))
    , m_importFailed(false)
{}

class MemberCopy : public MemberProcessor
{
public:
    explicit MemberCopy(ObjectValue *value) : m_value(value) {}
    bool processProperty(const QString &name, const Value *value,
                         const PropertyInfo & /*propertyInfo*/) override
    {
        m_value->setMember(name, value);
        return true;
    }
private:
    ObjectValue *m_value = nullptr;
};

void Imports::append(const Import &import)
{
    // when doing lookup, imports with 'as' clause are looked at first
    if (!import.info.as().isEmpty()) {
        const QString alias = import.info.as();
        if (!m_aliased.contains(alias))
            m_aliased.insert(alias, m_typeScope->valueOwner()->newObject(nullptr));
        ObjectValue *obj = m_aliased[alias];
        MemberCopy copyProcessor(obj);
        import.object->processMembers(&copyProcessor);

        m_imports.append(import);
    } else {
        // find first as-import and prepend
        for (int i = 0; i < m_imports.size(); ++i) {
            if (!m_imports.at(i).info.as().isEmpty()) {
                m_imports.insert(i, import);
                return;
            }
        }
        // not found, append
        m_imports.append(import);
    }

    if (!import.valid)
        m_importFailed = true;
}

void Imports::setImportFailed()
{
    m_importFailed = true;
}

ImportInfo Imports::info(const QString &name, const Context *context) const
{
    QString firstId = name;
    int dotIdx = firstId.indexOf(QLatin1Char('.'));
    if (dotIdx != -1)
        firstId = firstId.left(dotIdx);

    for (int pos = m_imports.size(); --pos >= 0; ) {
        const Import &i = m_imports.at(pos);
        const ObjectValue *import = i.object;
        const ImportInfo &info = i.info;

        if (!info.as().isEmpty()) {
            if (info.as() == firstId)
                return info;
            continue;
        }

        if (info.type() == ImportType::File || info.type() == ImportType::QrcFile) {
            if (import->className() == firstId)
                return info;
        } else {
            if (import->lookupMember(firstId, context))
                return info;
        }
    }
    return ImportInfo();
}

QString Imports::nameForImportedObject(const ObjectValue *value, const Context *context) const
{
    for (int pos = m_imports.size(); --pos >= 0; ) {
        const Import &i = m_imports.at(pos);
        const ObjectValue *import = i.object;
        const ImportInfo &info = i.info;

        if (info.type() == ImportType::File || info.type() == ImportType::QrcFile) {
            if (import == value)
                return import->className();
        } else {
            const Value *v = import->lookupMember(value->className(), context);
            if (v == value) {
                QString result = value->className();
                if (!info.as().isEmpty()) {
                    result.prepend(QLatin1Char('.'));
                    result.prepend(info.as());
                }
                return result;
            }
        }
    }
    return QString();
}

bool Imports::importFailed() const
{
    return m_importFailed;
}

const QList<Import> &Imports::all() const
{
    return m_imports;
}

const ObjectValue *Imports::aliased(const QString &name) const
{
    return m_aliased.value(name, nullptr);
}

const TypeScope *Imports::typeScope() const
{
    return m_typeScope;
}

const JSImportScope *Imports::jsImportScope() const
{
    return m_jsImportScope;
}

const ObjectValue *Imports::resolveAliasAndMarkUsed(const QString &name) const
{
    if (const ObjectValue *value = m_aliased.value(name, nullptr)) {
        // mark all respective ImportInfo objects to avoid dropping imports (QmlDesigner) on rewrite
        for (const Import &i : qAsConst(m_imports)) {
            const ImportInfo &info = i.info;
            if (info.as() == name)
                i.used = true; // FIXME: This evilly modifies a 'const' object
        }
        return value;
    }
    return nullptr;
}

#ifdef QT_DEBUG

class MemberDumper: public MemberProcessor
{
public:
    MemberDumper() {}

    bool processProperty(const QString &name, const Value *, const PropertyInfo &pInfo) override
    {
        qCDebug(qmljsLog) << "property: " << name << " flags:" << pInfo.toString();
        return true;
    }

    bool processEnumerator(const QString &name, const Value *) override
    {
        qCDebug(qmljsLog) << "enumerator: " << name;
        return true;
    }

    bool processSignal(const QString &name, const Value *) override
    {
        qCDebug(qmljsLog) << "signal: " << name;
        return true;
    }

    bool processSlot(const QString &name, const Value *) override
    {
        qCDebug(qmljsLog) << "slot: " << name;
        return true;
    }

    bool processGeneratedSlot(const QString &name, const Value *) override
    {
        qCDebug(qmljsLog) << "generated slot: " << name;
        return true;
    }
};

void Imports::dump() const
{
    qCDebug(qmljsLog) << "Imports contents, in search order:";
    for (int pos = m_imports.size(); --pos >= 0; ) {
        const Import &i = m_imports.at(pos);
        const ObjectValue *import = i.object;
        const ImportInfo &info = i.info;

        qCDebug(qmljsLog) << "  " << info.path() << " " << info.version().toString() << " as " << info.as() << " : " << import;
        MemberDumper dumper;
        import->processMembers(&dumper);
    }
}

#endif
