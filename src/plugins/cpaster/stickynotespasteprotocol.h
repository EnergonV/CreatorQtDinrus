// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "protocol.h"

namespace CodePaster {

class StickyNotesPasteProtocol : public NetworkProtocol
{
    Q_OBJECT
public:
    unsigned capabilities() const override;

    void fetch(const QString &id) override;
    void paste(const QString &text,
               ContentType ct = Text,
               int expiryDays = 1,
               const QString &username = QString(),
               const QString &comment = QString(),
               const QString &description = QString()) override;
    void list() override;



    QString hostUrl() const { return m_hostUrl; }
    void setHostUrl(const QString &hostUrl);

protected:
    bool checkConfiguration(QString *errorMessage = nullptr) override;

private:
    void fetchFinished();
    void pasteFinished();
    void listFinished();

    QString m_hostUrl;

    QNetworkReply *m_fetchReply = nullptr;
    QNetworkReply *m_pasteReply = nullptr;
    QNetworkReply *m_listReply = nullptr;

    QString m_fetchId;
    int m_postId = -1;
    bool m_hostChecked = false;
};

} // namespace CodePaster
