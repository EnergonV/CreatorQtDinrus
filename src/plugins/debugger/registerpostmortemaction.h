// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/aspects.h>

namespace Debugger {
namespace Internal {

class RegisterPostMortemAction : public Utils::BoolAspect
{
public:
    RegisterPostMortemAction();
    void readSettings(const QSettings *settings = nullptr) override;
    void writeSettings(QSettings *) const override {}

private:
    void registerNow(bool value);
};

} // namespace Internal
} // namespace Debugger
