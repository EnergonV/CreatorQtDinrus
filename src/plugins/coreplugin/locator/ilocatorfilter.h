// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <coreplugin/core_global.h>

#include <utils/filepath.h>
#include <utils/id.h>
#include <utils/optional.h>

#include <QFutureInterface>
#include <QIcon>
#include <QMetaType>
#include <QVariant>

namespace Core {

class ILocatorFilter;

struct LocatorFilterEntry
{
    struct HighlightInfo {
        enum DataType {
            DisplayName,
            ExtraInfo
        };

        HighlightInfo(int startIndex, int length, DataType type = DataType::DisplayName)
            : starts{startIndex}
            , lengths{length}
            , dataType(type)
        {}

        HighlightInfo(QVector<int> startIndex, QVector<int> length, DataType type = DataType::DisplayName)
            : starts(startIndex)
            , lengths(length)
            , dataType(type)
        {}

        QVector<int> starts;
        QVector<int> lengths;
        DataType dataType;
    };

    LocatorFilterEntry() = default;

    LocatorFilterEntry(ILocatorFilter *fromFilter, const QString &name, const QVariant &data,
                Utils::optional<QIcon> icon = Utils::nullopt)
        : filter(fromFilter)
        , displayName(name)
        , internalData(data)
        , displayIcon(icon)
    {}

    /* backpointer to creating filter */
    ILocatorFilter *filter = nullptr;
    /* displayed string */
    QString displayName;
    /* extra information displayed in light-gray in a second column (optional) */
    QString extraInfo;
    /* additional tooltip */
    QString toolTip;
    /* can be used by the filter to save more information about the entry */
    QVariant internalData;
    /* icon to display along with the entry */
    Utils::optional<QIcon> displayIcon;
    /* file path, if the entry is related to a file, is used e.g. for resolving a file icon */
    Utils::FilePath filePath;
    /* highlighting support */
    HighlightInfo highlightInfo{0, 0};

    static bool compareLexigraphically(const Core::LocatorFilterEntry &lhs,
                                       const Core::LocatorFilterEntry &rhs)
    {
        const int cmp = lhs.displayName.compare(rhs.displayName);
        if (cmp < 0)
            return true;
        if (cmp > 0)
            return false;
        return lhs.extraInfo < rhs.extraInfo;
    }
};

class CORE_EXPORT ILocatorFilter : public QObject
{
    Q_OBJECT

public:
    enum class MatchLevel {
        Best = 0,
        Better,
        Good,
        Normal,
        Count
    };

    enum Priority {Highest = 0, High = 1, Medium = 2, Low = 3};

    ILocatorFilter(QObject *parent = nullptr);
    ~ILocatorFilter() override;

    static const QList<ILocatorFilter *> allLocatorFilters();

    Utils::Id id() const;
    Utils::Id actionId() const;

    QString displayName() const;
    void setDisplayName(const QString &displayString);

    QString description() const;
    void setDescription(const QString &description);

    Priority priority() const;

    QString shortcutString() const;
    void setDefaultShortcutString(const QString &shortcut);
    void setShortcutString(const QString &shortcut);

    virtual void prepareSearch(const QString &entry);

    virtual QList<LocatorFilterEntry> matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) = 0;

    virtual void accept(const LocatorFilterEntry &selection,
                        QString *newText, int *selectionStart, int *selectionLength) const = 0;

    virtual void refresh(QFutureInterface<void> &future) { Q_UNUSED(future) };

    virtual QByteArray saveState() const;
    virtual void restoreState(const QByteArray &state);

    virtual bool openConfigDialog(QWidget *parent, bool &needsRefresh);
    bool isConfigurable() const;

    bool isIncludedByDefault() const;
    void setDefaultIncludedByDefault(bool includedByDefault);
    void setIncludedByDefault(bool includedByDefault);

    bool isHidden() const;

    bool isEnabled() const;

    static Qt::CaseSensitivity caseSensitivity(const QString &str);
    static QRegularExpression createRegExp(const QString &text,
                                           Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive);
    static LocatorFilterEntry::HighlightInfo highlightInfo(const QRegularExpressionMatch &match,
        LocatorFilterEntry::HighlightInfo::DataType dataType = LocatorFilterEntry::HighlightInfo::DisplayName);

    static QString msgConfigureDialogTitle();
    static QString msgPrefixLabel();
    static QString msgPrefixToolTip();
    static QString msgIncludeByDefault();
    static QString msgIncludeByDefaultToolTip();

public slots:
    void setEnabled(bool enabled);

protected:
    void setHidden(bool hidden);
    void setId(Utils::Id id);
    void setPriority(Priority priority);
    void setConfigurable(bool configurable);
    bool openConfigDialog(QWidget *parent, QWidget *additionalWidget);

    virtual void saveState(QJsonObject &object) const;
    virtual void restoreState(const QJsonObject &object);

    static bool isOldSetting(const QByteArray &state);

private:
    Utils::Id m_id;
    QString m_shortcut;
    Priority m_priority = Medium;
    QString m_displayName;
    QString m_description;
    QString m_defaultShortcut;
    bool m_defaultIncludedByDefault = false;
    bool m_includedByDefault = m_defaultIncludedByDefault;
    bool m_hidden = false;
    bool m_enabled = true;
    bool m_isConfigurable = true;
};

} // namespace Core
