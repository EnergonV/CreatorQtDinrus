// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "settings.h"

#include "operation.h"

#include "addcmakeoperation.h"
#include "adddebuggeroperation.h"
#include "adddeviceoperation.h"
#include "addabiflavor.h"
#include "addkeysoperation.h"
#include "addkitoperation.h"
#include "addqtoperation.h"
#include "addtoolchainoperation.h"
#include "addvalueoperation.h"
#include "findkeyoperation.h"
#include "findvalueoperation.h"
#include "getoperation.h"
#include "rmcmakeoperation.h"
#include "rmdebuggeroperation.h"
#include "rmdeviceoperation.h"
#include "rmkeysoperation.h"
#include "rmkitoperation.h"
#include "rmqtoperation.h"
#include "rmtoolchainoperation.h"

#include <app/app_version.h>

#include <iostream>

#include <QCoreApplication>
#include <QStringList>

void printHelp(const Operation *op)
{
    std::cout << Core::Constants::IDE_DISPLAY_NAME << " SDK setup tool." << std::endl;

    std::cout << "Help for operation " << qPrintable(op->name()) << std::endl;
    std::cout << std::endl;
    std::cout << qPrintable(op->argumentsHelpText());
    std::cout << std::endl;
}

const QString tabular(const std::unique_ptr<Operation> &o)
{
    const QString name = o->name();
    return name + QString(16 - name.length(), QChar::Space) + o->helpText();
}

void printHelp(const std::vector<std::unique_ptr<Operation>> &operations)
{
    std::cout << Core::Constants::IDE_DISPLAY_NAME << "SDK setup tool." << std::endl;
    std::cout << "Based on Qt " << qVersion() << std::endl;
    std::cout << "    Usage: " << qPrintable(QCoreApplication::arguments().at(0))
              << " <ARGS> <OPERATION> <OPERATION_ARGS>" << std::endl << std::endl;
    std::cout << "ARGS:" << std::endl;
    std::cout << "    --help|-h                Print this help text" << std::endl;
    std::cout << "    --sdkpath=PATH|-s PATH   Set the path to the SDK files" << std::endl << std::endl;

    std::cout << "Default sdkpath is \""
              << qPrintable(QDir::cleanPath(
                     Utils::FilePath::fromString(QCoreApplication::applicationDirPath())
                         .pathAppended(DATA_PATH)
                         .toUserOutput()))
              << "\"" << std::endl
              << std::endl;

    std::cout << "OPERATION:" << std::endl;
    std::cout << "    One of:" << std::endl;
    for (const std::unique_ptr<Operation> &o : operations)
        std::cout << "        " << qPrintable(tabular(o)) << std::endl;
    std::cout << std::endl;
    std::cout << "OPERATION_ARGS:" << std::endl;
    std::cout << "   use \"--help <OPERATION>\" to get help on the arguments required for an operation." << std::endl;

    std::cout << std::endl;
}

int parseArguments(const QStringList &args, Settings *s,
                   const std::vector<std::unique_ptr<Operation>> &operations)
{
    QStringList opArgs;
    int argCount = args.count();

    for (int i = 1; i < argCount; ++i) {
        const QString current = args[i];
        const QString next = (i + 1 < argCount) ? args[i + 1] : QString();

        if (!s->operation) {
            // help
            if (current == QLatin1String("-h") || current == QLatin1String("--help")) {
                if (!next.isEmpty()) {
                    for (const std::unique_ptr<Operation> &o : operations) {
                        if (o->name() == next) {
                            printHelp(o.get());
                            return 0;
                        }
                    }
                }
                printHelp(operations);
                return 0;
            }

            // sdkpath
            if (current.startsWith(QLatin1String("--sdkpath="))) {
                s->sdkPath = Utils::FilePath::fromString(current.mid(10));
                continue;
            }
            if (current == QLatin1String("-s")) {
                if (next.isNull()) {
                    std::cerr << "Missing argument to '-s'." << std::endl << std::endl;
                    printHelp(operations);
                    return 1;
                }
                s->sdkPath = Utils::FilePath::fromString(next);
                ++i; // skip next;
                continue;
            }

            // operation
            for (const std::unique_ptr<Operation> &o : operations) {
                if (o->name() == current) {
                    s->operation = o.get();
                    break;
                }
            }

            if (s->operation)
                continue;
        } else {
            opArgs << current;
            continue;
        }

        std::cerr << "Unknown parameter given." << std::endl << std::endl;
        printHelp(operations);
        return 1;
    }

    if (!s->operation) {
        std::cerr << "No operation requested." << std::endl << std::endl;
        printHelp(operations);
        return 1;
    }
    if (!s->operation->setArguments(opArgs)) {
        std::cerr << "Argument parsing failed." << std::endl << std::endl;
        printHelp(s->operation);
        s->operation = nullptr;
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    // Since 5.3, Qt by default aborts if the effective user id is different than the
    // real user id. However, in IFW on Mac we use setuid to 'elevate'
    // permissions if needed. This is considered safe because the user has to provide
    // the credentials manually - an attack would require at least access to the
    // user's environment.
    QCoreApplication::setSetuidAllowed(true);

    QCoreApplication a(argc, argv);

    Settings settings;

    std::vector<std::unique_ptr<Operation>> operations;
    operations.emplace_back(std::make_unique<AddKeysOperation>());

    operations.emplace_back(std::make_unique<AddAbiFlavor>());
    operations.emplace_back(std::make_unique<AddCMakeOperation>());
    operations.emplace_back(std::make_unique<AddDebuggerOperation>());
    operations.emplace_back(std::make_unique<AddDeviceOperation>());
    operations.emplace_back(std::make_unique<AddQtOperation>());
    operations.emplace_back(std::make_unique<AddToolChainOperation>());
    operations.emplace_back(std::make_unique<AddValueOperation>());

    operations.emplace_back(std::make_unique<AddKitOperation>());

    operations.emplace_back(std::make_unique<GetOperation>());

    operations.emplace_back(std::make_unique<RmCMakeOperation>());
    operations.emplace_back(std::make_unique<RmKitOperation>());
    operations.emplace_back(std::make_unique<RmDebuggerOperation>());
    operations.emplace_back(std::make_unique<RmDeviceOperation>());
    operations.emplace_back(std::make_unique<RmKeysOperation>());
    operations.emplace_back(std::make_unique<RmQtOperation>());
    operations.emplace_back(std::make_unique<RmToolChainOperation>());

    operations.emplace_back(std::make_unique<FindKeyOperation>());
    operations.emplace_back(std::make_unique<FindValueOperation>());

    int result = parseArguments(a.arguments(), &settings, operations);
    return settings.operation ? settings.operation->execute() : result;
}
