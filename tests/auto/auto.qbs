import qbs

Project {
    name: "QtcAutotests"
    condition: project.withAutotests
    references: [
        "aggregation/aggregation.qbs",
        "algorithm/algorithm.qbs",
        "android/android.qbs",
        "changeset/changeset.qbs",
        "cplusplus/cplusplus.qbs",
        "debugger/debugger.qbs",
        "diff/diff.qbs",
        "environment/environment.qbs",
        "extensionsystem/extensionsystem.qbs",
        "externaltool/externaltool.qbs",
        "filesearch/filesearch.qbs",
        "json/json.qbs",
        "languageserverprotocol/languageserverprotocol.qbs",
        "profilewriter/profilewriter.qbs",
        "qml/qml.qbs",
        "runextensions/runextensions.qbs",
        "sdktool/sdktool.qbs",
        "toolchaincache/toolchaincache.qbs",
        "tracing/tracing.qbs",
        "treeviewfind/treeviewfind.qbs",
        "updateinfo/updateinfo.qbs",
        "utils/utils.qbs",
        "valgrind/valgrind.qbs",
    ].concat(project.additionalAutotests)
}
