set(TSTFLAMEGRAPHVIEW_CPP_SOURCES
  testflamegraphmodel.h
  tst_flamegraphview.cpp
)

add_qtc_test(tst_tracing_flamegraphview
  EXCLUDE_FROM_PRECHECK
  DEPENDS Tracing Qt5::QuickWidgets Qt5::Quick Utils
)

if (NOT TARGET tst_tracing_flamegraphview) # qt_add_qml_module has no DEPENDS check
  return()
endif()

qt_add_qml_module(tst_tracing_flamegraphview
  URI "QtCreator.TstTracingFlameGraphView"
  VERSION "1.0"
  NO_PLUGIN
  RESOURCE_PREFIX "/qt/qml"
  QML_FILES
    TestFlameGraphView.qml
  SOURCES
    ${TSTFLAMEGRAPHVIEW_CPP_SOURCES}
)
