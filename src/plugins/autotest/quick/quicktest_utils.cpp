// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "quicktest_utils.h"
#include "../testframeworkmanager.h"
#include "../testtreeitem.h"

#include <utils/qtcassert.h>

#include <QByteArrayList>

namespace Autotest {
namespace Internal {
namespace QuickTestUtils {

bool isQuickTestMacro(const QByteArray &macro)
{
    static const QByteArrayList valid = {"QUICK_TEST_MAIN", "QUICK_TEST_OPENGL_MAIN",
                                         "QUICK_TEST_MAIN_WITH_SETUP"};
    return valid.contains(macro);
}

QHash<Utils::FilePath, Utils::FilePath> proFilesForQmlFiles(ITestFramework *framework,
                                                            const Utils::FilePaths &files)
{
    QHash<Utils::FilePath, Utils::FilePath> result;
    TestTreeItem *rootNode = framework->rootNode();
    QTC_ASSERT(rootNode, return result);

    if (files.isEmpty())
        return result;

    rootNode->forFirstLevelChildItems([&result, &files](TestTreeItem *child) {
        const Utils::FilePath &file = child->filePath();
        if (!file.isEmpty() && files.contains(file)) {
            const Utils::FilePath &proFile = child->proFile();
            if (!proFile.isEmpty())
                result.insert(file, proFile);
        }
        child->forFirstLevelChildItems([&result, &files](TestTreeItem *grandChild) {
            const Utils::FilePath &file = grandChild->filePath();
            if (!file.isEmpty() && files.contains(file)) {
                const Utils::FilePath &proFile = grandChild->proFile();
                if (!proFile.isEmpty())
                    result.insert(file, proFile);
            }
        });
    });
    return result;
}

} // namespace QuickTestUtils
} // namespace Internal
} // namespace Autotest
