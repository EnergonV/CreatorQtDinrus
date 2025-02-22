// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include <utils/filesearch.h>

#include <QTextCodec>
#include <QtTest>

class tst_FileSearch : public QObject
{
    Q_OBJECT
public:
    enum RegExpFlag {
        NoRegExp,
        RegExp
    };

private slots:
    void multipleResults();
    void caseSensitive();
    void caseInSensitive();
    void matchCaseReplacement();
};

namespace {
    const char * const FILENAME = ":/tst_filesearch/testfile.txt";

    void test_helper(const Utils::FileSearchResultList &expectedResults,
                     const QString &term,
                     QTextDocument::FindFlags flags, tst_FileSearch::RegExpFlag regexp = tst_FileSearch::NoRegExp)
    {
        Utils::FileIterator *it = new Utils::FileListIterator(QStringList(QLatin1String(FILENAME)), QList<QTextCodec *>() << QTextCodec::codecForLocale());
        QFutureWatcher<Utils::FileSearchResultList> watcher;
        QSignalSpy ready(&watcher, SIGNAL(resultsReadyAt(int,int)));
        if (regexp == tst_FileSearch::NoRegExp)
            watcher.setFuture(Utils::findInFiles(term, it, flags));
        else
            watcher.setFuture(Utils::findInFilesRegExp(term, it, flags));
        watcher.future().waitForFinished();
        QTest::qWait(100); // process events
        QCOMPARE(ready.count(), 1);
        Utils::FileSearchResultList results = watcher.resultAt(0);
        QCOMPARE(results.count(), expectedResults.count());
        for (int i = 0; i < expectedResults.size(); ++i) {
            QCOMPARE(results.at(i), expectedResults.at(i));
        }
    }
}

void tst_FileSearch::multipleResults()
{
    Utils::FileSearchResultList expectedResults;
    expectedResults << Utils::FileSearchResult(QLatin1String(FILENAME), 2, QLatin1String("search to find multiple find results"), 10, 4, QStringList());
    expectedResults << Utils::FileSearchResult(QLatin1String(FILENAME), 2, QLatin1String("search to find multiple find results"), 24, 4, QStringList());
    expectedResults << Utils::FileSearchResult(QLatin1String(FILENAME), 4, QLatin1String("here you find another result"), 9, 4, QStringList());
    test_helper(expectedResults, QLatin1String("find"), QTextDocument::FindFlags());

    expectedResults.clear();
    expectedResults << Utils::FileSearchResult(QLatin1String(FILENAME), 5, QLatin1String("aaaaaaaa this line has 2 results for four a in a row"), 0, 4, QStringList());
    expectedResults << Utils::FileSearchResult(QLatin1String(FILENAME), 5, QLatin1String("aaaaaaaa this line has 2 results for four a in a row"), 4, 4, QStringList());
    test_helper(expectedResults, QLatin1String("aaaa"), QTextDocument::FindFlags());

    expectedResults.clear();
    expectedResults << Utils::FileSearchResult(QLatin1String(FILENAME), 5, QLatin1String("aaaaaaaa this line has 2 results for four a in a row"), 0, 4, QStringList() << QLatin1String("aaaa"));
    expectedResults << Utils::FileSearchResult(QLatin1String(FILENAME), 5, QLatin1String("aaaaaaaa this line has 2 results for four a in a row"), 4, 4, QStringList() << QLatin1String("aaaa"));
    test_helper(expectedResults, QLatin1String("aaaa"), QTextDocument::FindFlags(), RegExp);
}

void tst_FileSearch::caseSensitive()
{
    Utils::FileSearchResultList expectedResults;
    expectedResults << Utils::FileSearchResult(QLatin1String(FILENAME), 3, QLatin1String("search CaseSensitively for casesensitive"), 7, 13, QStringList());
    test_helper(expectedResults, QLatin1String("CaseSensitive"), QTextDocument::FindCaseSensitively);
}

void tst_FileSearch::caseInSensitive()
{
    Utils::FileSearchResultList expectedResults;
    expectedResults << Utils::FileSearchResult(QLatin1String(FILENAME), 3, QLatin1String("search CaseSensitively for casesensitive"), 7, 13, QStringList());
    expectedResults << Utils::FileSearchResult(QLatin1String(FILENAME), 3, QLatin1String("search CaseSensitively for casesensitive"), 27, 13, QStringList());
    test_helper(expectedResults, QLatin1String("CaseSensitive"), QTextDocument::FindFlags());
}

void tst_FileSearch::matchCaseReplacement()
{
    QCOMPARE(Utils::matchCaseReplacement("", "foobar"), QString("foobar"));          //empty string

    QCOMPARE(Utils::matchCaseReplacement("testpad", "foobar"), QString("foobar"));   //lower case
    QCOMPARE(Utils::matchCaseReplacement("TESTPAD", "foobar"), QString("FOOBAR"));   //upper case
    QCOMPARE(Utils::matchCaseReplacement("Testpad", "foobar"), QString("Foobar"));   //capitalized
    QCOMPARE(Utils::matchCaseReplacement("tESTPAD", "foobar"), QString("fOOBAR"));   //un-capitalized
    QCOMPARE(Utils::matchCaseReplacement("tEsTpAd", "foobar"), QString("foobar"));   //mixed case, use replacement as specified
    QCOMPARE(Utils::matchCaseReplacement("TeStPaD", "foobar"), QString("foobar"));   //mixed case, use replacement as specified

    QCOMPARE(Utils::matchCaseReplacement("testpad", "fooBar"), QString("foobar"));   //lower case
    QCOMPARE(Utils::matchCaseReplacement("TESTPAD", "fooBar"), QString("FOOBAR"));   //upper case
    QCOMPARE(Utils::matchCaseReplacement("Testpad", "fooBar"), QString("Foobar"));   //capitalized
    QCOMPARE(Utils::matchCaseReplacement("tESTPAD", "fooBar"), QString("fOOBAR"));   //un-capitalized
    QCOMPARE(Utils::matchCaseReplacement("tEsTpAd", "fooBar"), QString("fooBar"));   //mixed case, use replacement as specified
    QCOMPARE(Utils::matchCaseReplacement("TeStPaD", "fooBar"), QString("fooBar"));   //mixed case, use replacement as specified

    //with common prefix
    QCOMPARE(Utils::matchCaseReplacement("pReFiXtestpad", "prefixfoobar"), QString("pReFiXfoobar"));   //lower case
    QCOMPARE(Utils::matchCaseReplacement("pReFiXTESTPAD", "prefixfoobar"), QString("pReFiXFOOBAR"));   //upper case
    QCOMPARE(Utils::matchCaseReplacement("pReFiXTestpad", "prefixfoobar"), QString("pReFiXFoobar"));   //capitalized
    QCOMPARE(Utils::matchCaseReplacement("pReFiXtESTPAD", "prefixfoobar"), QString("pReFiXfOOBAR"));   //un-capitalized
    QCOMPARE(Utils::matchCaseReplacement("pReFiXtEsTpAd", "prefixfoobar"), QString("pReFiXfoobar"));   //mixed case, use replacement as specified
    QCOMPARE(Utils::matchCaseReplacement("pReFiXTeStPaD", "prefixfoobar"), QString("pReFiXfoobar"));   //mixed case, use replacement as specified

    //with common suffix
    QCOMPARE(Utils::matchCaseReplacement("testpadSuFfIx", "foobarsuffix"), QString("foobarSuFfIx"));   //lower case
    QCOMPARE(Utils::matchCaseReplacement("TESTPADSuFfIx", "foobarsuffix"), QString("FOOBARSuFfIx"));   //upper case
    QCOMPARE(Utils::matchCaseReplacement("TestpadSuFfIx", "foobarsuffix"), QString("FoobarSuFfIx"));   //capitalized
    QCOMPARE(Utils::matchCaseReplacement("tESTPADSuFfIx", "foobarsuffix"), QString("fOOBARSuFfIx"));   //un-capitalized
    QCOMPARE(Utils::matchCaseReplacement("tEsTpAdSuFfIx", "foobarsuffix"), QString("foobarSuFfIx"));   //mixed case, use replacement as specified
    QCOMPARE(Utils::matchCaseReplacement("TeStPaDSuFfIx", "foobarsuffix"), QString("foobarSuFfIx"));   //mixed case, use replacement as specified

    //with common prefix and suffix
    QCOMPARE(Utils::matchCaseReplacement("pReFiXtestpadSuFfIx", "prefixfoobarsuffix"), QString("pReFiXfoobarSuFfIx"));   //lower case
    QCOMPARE(Utils::matchCaseReplacement("pReFiXTESTPADSuFfIx", "prefixfoobarsuffix"), QString("pReFiXFOOBARSuFfIx"));   //upper case
    QCOMPARE(Utils::matchCaseReplacement("pReFiXTestpadSuFfIx", "prefixfoobarsuffix"), QString("pReFiXFoobarSuFfIx"));   //capitalized
    QCOMPARE(Utils::matchCaseReplacement("pReFiXtESTPADSuFfIx", "prefixfoobarsuffix"), QString("pReFiXfOOBARSuFfIx"));   //un-capitalized
    QCOMPARE(Utils::matchCaseReplacement("pReFiXtEsTpAdSuFfIx", "prefixfoobarsuffix"), QString("pReFiXfoobarSuFfIx"));   //mixed case, use replacement as specified
    QCOMPARE(Utils::matchCaseReplacement("pReFiXTeStPaDSuFfIx", "prefixfoobarsuffix"), QString("pReFiXfoobarSuFfIx"));   //mixed case, use replacement as specified
}

QTEST_GUILESS_MAIN(tst_FileSearch)

#include "tst_filesearch.moc"
