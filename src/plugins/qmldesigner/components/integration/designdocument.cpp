// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "designdocument.h"
#include "designdocumentview.h"
#include "documentmanager.h"
#include "qmldesignerconstants.h"
#include "qmlvisualnode.h"

#include <auxiliarydataproperties.h>
#include <metainfo.h>
#include <nodeinstanceview.h>
#include <nodelistproperty.h>
#include <rewritingexception.h>
#include <variantproperty.h>
#include <viewmanager.h>
#include <qmldesignerplugin.h>
#include <qmlobjectnode.h>

#include <projectexplorer/projecttree.h>
#include <projectexplorer/project.h>
#include <projectexplorer/target.h>
#include <projectexplorer/session.h>
#include <projectexplorer/kit.h>
#include <qtsupport/qtkitinformation.h>
#include <qtsupport/qtsupportconstants.h>
#include <qtsupport/qtversionmanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/idocument.h>
#include <coreplugin/editormanager/editormanager.h>
#include <utils/algorithm.h>
#include <timelineactions.h>
#include <svgpasteaction.h>

#include <qmljs/qmljsmodelmanagerinterface.h>

#include <utils/qtcassert.h>

#include <QFileInfo>
#include <QUrl>
#include <QDebug>

#include <QApplication>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRandomGenerator>
#include <QClipboard>

using namespace ProjectExplorer;

enum {
    debug = false
};

namespace QmlDesigner {

/**
  \class QmlDesigner::DesignDocument

  DesignDocument acts as a facade to a model representing a qml document,
  and the different views/widgets accessing it.
  */
DesignDocument::DesignDocument(ProjectStorage<Sqlite::Database> &projectStorage)
    : m_documentModel(Model::create("QtQuick.Item", 1, 0))
    , m_subComponentManager(new SubComponentManager(m_documentModel.get(), this))
    , m_rewriterView(new RewriterView(RewriterView::Amend, m_documentModel.get()))
    , m_documentLoaded(false)
    , m_currentTarget(nullptr)
    , m_projectStorage(projectStorage)
{
}

DesignDocument::~DesignDocument() = default;

Model *DesignDocument::currentModel() const
{
    if (m_inFileComponentModel)
        return m_inFileComponentModel.get();

    return m_documentModel.get();
}

Model *DesignDocument::documentModel() const
{
    return m_documentModel.get();
}

QWidget *DesignDocument::centralWidget() const
{
    return qobject_cast<QWidget*>(parent());
}

const ViewManager &DesignDocument::viewManager() const
{
    return QmlDesignerPlugin::instance()->viewManager();
}

ViewManager &DesignDocument::viewManager()
{
    return QmlDesignerPlugin::instance()->viewManager();
}

static ComponentTextModifier *createComponentTextModifier(TextModifier *originalModifier,
                                                          RewriterView *rewriterView,
                                                          const QString &componentText,
                                                          const ModelNode &componentNode)
{
    bool explicitComponent = componentText.contains(QLatin1String("Component"));

    ModelNode rootModelNode = rewriterView->rootModelNode();

    int componentStartOffset;
    int componentEndOffset;

    int rootStartOffset = rewriterView->nodeOffset(rootModelNode);

    if (explicitComponent) { //the component is explciit we have to find the first definition inside
        componentStartOffset = rewriterView->firstDefinitionInsideOffset(componentNode);
        componentEndOffset = componentStartOffset + rewriterView->firstDefinitionInsideLength(componentNode);
    } else { //the component is implicit
        componentStartOffset = rewriterView->nodeOffset(componentNode);
        componentEndOffset = componentStartOffset + rewriterView->nodeLength(componentNode);
    }

    return new ComponentTextModifier(originalModifier, componentStartOffset, componentEndOffset, rootStartOffset);
}

bool DesignDocument::loadInFileComponent(const ModelNode &componentNode)
{
    QString componentText = rewriterView()->extractText({componentNode}).value(componentNode);

    if (componentText.isEmpty())
        return false;

    if (!componentNode.isRootNode()) {
        //change to subcomponent model
        changeToInFileComponentModel(createComponentTextModifier(m_documentTextModifier.data(), rewriterView(), componentText, componentNode));
    }

    return true;
}

AbstractView *DesignDocument::view() const
{
    return viewManager().nodeInstanceView();
}

std::unique_ptr<Model> DesignDocument::createInFileComponentModel()
{
    auto model = Model::create("QtQuick.Item", 1, 0);
    model->setFileUrl(m_documentModel->fileUrl());
    model->setMetaInfo(m_documentModel->metaInfo());

    return model;
}

bool DesignDocument::pasteSVG()
{
    SVGPasteAction svgPasteAction;

    if (!svgPasteAction.containsSVG(QApplication::clipboard()->text()))
        return false;

    rewriterView()->executeInTransaction("DesignDocument::paste1", [&]() {
        ModelNode targetNode;

        // If nodes are currently selected make the first node in selection the target
        if (!view()->selectedModelNodes().isEmpty())
            targetNode = view()->firstSelectedModelNode();

        // If target is still invalid make the root node the target
        if (!targetNode.isValid())
            targetNode = view()->rootModelNode();

        // Check if document has studio components import, if not create it
        QmlDesigner::Import import = QmlDesigner::Import::createLibraryImport("QtQuick.Studio.Components", "1.0");
        if (!currentModel()->hasImport(import, true, true)) {
            QmlDesigner::Import studioComponentsImport = QmlDesigner::Import::createLibraryImport("QtQuick.Studio.Components", "1.0");
            try {
                currentModel()->changeImports({studioComponentsImport}, {});
            } catch (const QmlDesigner::Exception &) {
                QTC_ASSERT(false, return);
            }
        }

        svgPasteAction.createQmlObjectNode(targetNode);
    });

    return true;
}

bool DesignDocument::inFileComponentModelActive() const
{
    return bool(m_inFileComponentModel);
}

QList<DocumentMessage> DesignDocument::qmlParseWarnings() const
{
    return m_rewriterView->warnings();
}

bool DesignDocument::hasQmlParseWarnings() const
{
    return !m_rewriterView->warnings().isEmpty();
}

QList<DocumentMessage> DesignDocument::qmlParseErrors() const
{
    return m_rewriterView->errors();
}

bool DesignDocument::hasQmlParseErrors() const
{
    return !m_rewriterView->errors().isEmpty();
}

QString DesignDocument::displayName() const
{
    return fileName().toString();
}

QString DesignDocument::simplfiedDisplayName() const
{
    if (rootModelNode().id().isEmpty())
        return rootModelNode().id();
    else
        return rootModelNode().simplifiedTypeName();
}

void DesignDocument::updateFileName(const Utils::FilePath & /*oldFileName*/, const Utils::FilePath &newFileName)
{
    if (m_documentModel)
        m_documentModel->setFileUrl(QUrl::fromLocalFile(newFileName.toString()));

    if (m_inFileComponentModel)
        m_inFileComponentModel->setFileUrl(QUrl::fromLocalFile(newFileName.toString()));

    emit displayNameChanged(displayName());
}

Utils::FilePath DesignDocument::fileName() const
{
    if (editor())
        return editor()->document()->filePath();

    return Utils::FilePath();
}

ProjectExplorer::Target *DesignDocument::currentTarget() const
{
    return m_currentTarget;
}

bool DesignDocument::isDocumentLoaded() const
{
    return m_documentLoaded;
}

void DesignDocument::resetToDocumentModel()
{
    const QPlainTextEdit *edit = plainTextEdit();
    if (edit)
        edit->document()->clearUndoRedoStacks();

    m_inFileComponentModel.reset();
}

void DesignDocument::loadDocument(QPlainTextEdit *edit)
{
    Q_CHECK_PTR(edit);

    connect(edit, &QPlainTextEdit::undoAvailable, this, &DesignDocument::undoAvailable);
    connect(edit, &QPlainTextEdit::redoAvailable, this, &DesignDocument::redoAvailable);
    connect(edit, &QPlainTextEdit::modificationChanged, this, &DesignDocument::dirtyStateChanged);

    m_documentTextModifier.reset(new BaseTextEditModifier(qobject_cast<TextEditor::TextEditorWidget *>(plainTextEdit())));

    connect(m_documentTextModifier.data(), &TextModifier::textChanged, this, &DesignDocument::updateQrcFiles);

    m_documentModel->setTextModifier(m_documentTextModifier.data());

    m_inFileComponentTextModifier.reset();

    updateFileName(Utils::FilePath(), fileName());

    updateQrcFiles();

    m_documentLoaded = true;
}

void DesignDocument::changeToDocumentModel()
{
    viewManager().detachRewriterView();
    viewManager().detachViewsExceptRewriterAndComponetView();

    const QPlainTextEdit *edit = plainTextEdit();
    if (edit)
        edit->document()->clearUndoRedoStacks();

    m_inFileComponentModel.reset();
    m_inFileComponentTextModifier.reset();

    viewManager().attachRewriterView();
    viewManager().attachViewsExceptRewriterAndComponetView();
}

bool DesignDocument::isQtForMCUsProject() const
{
    if (m_currentTarget)
        return m_currentTarget->additionalData("CustomQtForMCUs").toBool();

    return false;
}

Utils::FilePath DesignDocument::projectFolder() const
{
    ProjectExplorer::Project *currentProject = ProjectExplorer::SessionManager::projectForFile(fileName());

    if (currentProject)
        return currentProject->projectDirectory();
    return {};
}

bool DesignDocument::hasProject() const
{
    return !DocumentManager::currentProjectDirPath().isEmpty();
}

void DesignDocument::changeToInFileComponentModel(ComponentTextModifier *textModifer)
{
    m_inFileComponentTextModifier.reset(textModifer);
    viewManager().detachRewriterView();
    viewManager().detachViewsExceptRewriterAndComponetView();

    const QPlainTextEdit *edit = plainTextEdit();
    if (edit)
        edit->document()->clearUndoRedoStacks();

    m_inFileComponentModel = createInFileComponentModel();
    m_inFileComponentModel->setTextModifier(m_inFileComponentTextModifier.data());

    viewManager().attachRewriterView();
    viewManager().attachViewsExceptRewriterAndComponetView();
}

void DesignDocument::updateQrcFiles()
{
    ProjectExplorer::Project *currentProject = ProjectExplorer::SessionManager::projectForFile(fileName());

    if (currentProject) {
        const auto srcFiles = currentProject->files(ProjectExplorer::Project::SourceFiles);
        for (const Utils::FilePath &fileName : srcFiles) {
            if (fileName.endsWith(".qrc"))
                QmlJS::ModelManagerInterface::instance()->updateQrcFile(fileName);
        }
    }
}

void DesignDocument::changeToSubComponent(const ModelNode &componentNode)
{
    if (QmlDesignerPlugin::instance()->currentDesignDocument() != this)
        return;

    if (m_inFileComponentModel)
        changeToDocumentModel();

    bool subComponentLoaded = loadInFileComponent(componentNode);

    if (subComponentLoaded)
        attachRewriterToModel();

    QmlDesignerPlugin::instance()->viewManager().pushInFileComponentOnCrumbleBar(componentNode);
    QmlDesignerPlugin::instance()->viewManager().setComponentNode(componentNode);
}

void DesignDocument::changeToMaster()
{
    if (QmlDesignerPlugin::instance()->currentDesignDocument() != this)
        return;

    if (m_inFileComponentModel)
        changeToDocumentModel();

    QmlDesignerPlugin::instance()->viewManager().pushFileOnCrumbleBar(fileName());
    QmlDesignerPlugin::instance()->viewManager().setComponentNode(rootModelNode());
}

void DesignDocument::attachRewriterToModel()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    Q_ASSERT(m_documentModel);

    viewManager().attachRewriterView();

    Q_ASSERT(m_documentModel);
    QApplication::restoreOverrideCursor();
}

bool DesignDocument::isUndoAvailable() const
{
    if (plainTextEdit())
        return plainTextEdit()->document()->isUndoAvailable();

    return false;
}

bool DesignDocument::isRedoAvailable() const
{
    if (plainTextEdit())
        return plainTextEdit()->document()->isRedoAvailable();

    return false;
}

void DesignDocument::close()
{
    m_documentLoaded = false;
    emit designDocumentClosed();
}

void DesignDocument::updateSubcomponentManager()
{
    Q_ASSERT(m_subComponentManager);
    m_subComponentManager->update(QUrl::fromLocalFile(fileName().toString()),
                                  currentModel()->imports() + currentModel()->possibleImports());
}

void DesignDocument::addSubcomponentManagerImport(const Import &import)
{
    m_subComponentManager->addAndParseImport(import);
}

void DesignDocument::deleteSelected()
{
    if (!currentModel())
        return;

    QStringList lockedNodes;
    for (const ModelNode &modelNode : view()->selectedModelNodes()) {
        for (const ModelNode &node : modelNode.allSubModelNodesAndThisNode()) {
            if (node.isValid() && !node.isRootNode() && node.locked() && !lockedNodes.contains(node.id()))
                lockedNodes.push_back(node.id());
        }
    }

    if (!lockedNodes.empty()) {
        Utils::sort(lockedNodes);
        QString detailedText = QString("<b>" + tr("Locked items:") + "</b><br>");

        for (const auto &id : qAsConst(lockedNodes))
            detailedText.append("- " + id + "<br>");

        detailedText.chop(QString("<br>").size());

        QMessageBox msgBox;
        msgBox.setTextFormat(Qt::RichText);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setWindowTitle(tr("Delete/Cut Item"));
        msgBox.setText(QString(tr("Deleting or cutting this item will modify locked items.") + "<br><br>%1")
                               .arg(detailedText));
        msgBox.setInformativeText(tr("Do you want to continue by removing the item (Delete) or removing it and copying it to the clipboard (Cut)?"));
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Ok);

        if (msgBox.exec() == QMessageBox::Cancel)
            return;
    }

    rewriterView()->executeInTransaction("DesignDocument::deleteSelected", [this]() {
        const QList<ModelNode> toDelete = view()->selectedModelNodes();
        for (ModelNode node : toDelete) {
            if (node.isValid() && !node.isRootNode() && QmlObjectNode::isValidQmlObjectNode(node))
                QmlObjectNode(node).destroy();
        }
    });
}

void DesignDocument::copySelected()
{
    DesignDocumentView view;

    currentModel()->attachView(&view);

    DesignDocumentView::copyModelNodes(view.selectedModelNodes());
}

void DesignDocument::cutSelected()
{
    copySelected();
    deleteSelected();
}

static void scatterItem(const ModelNode &pastedNode, const ModelNode &targetNode, int offset = -2000)
{
    if (targetNode.metaInfo().isValid() && targetNode.metaInfo().isLayoutable())
        return;

    if (!(pastedNode.hasVariantProperty("x") && pastedNode.hasVariantProperty("y")))
        return;

    bool scatter = false;
    for (const ModelNode &childNode : targetNode.directSubModelNodes()) {
        if (childNode.variantProperty("x").value() == pastedNode.variantProperty("x").value() &&
            childNode.variantProperty("y").value() == pastedNode.variantProperty("y").value()) {
            scatter = true;
            break;
        }
    }
    if (!scatter)
        return;

    if (offset == -2000) { // scatter in range
        double x = pastedNode.variantProperty("x").value().toDouble();
        double y = pastedNode.variantProperty("y").value().toDouble();

        const double scatterRange = 20.;
        x += QRandomGenerator::global()->generateDouble() * scatterRange - scatterRange / 2;
        y += QRandomGenerator::global()->generateDouble() * scatterRange - scatterRange / 2;

        pastedNode.variantProperty("x").setValue(int(x));
        pastedNode.variantProperty("y").setValue(int(y));
    } else { // offset
        int x = pastedNode.variantProperty("x").value().toInt();
        int y = pastedNode.variantProperty("y").value().toInt();
        x += offset;
        y += offset;
        pastedNode.variantProperty("x").setValue(x);
        pastedNode.variantProperty("y").setValue(y);
    }
}

void DesignDocument::paste()
{
    if (pasteSVG())
        return;

    if (TimelineActions::clipboardContainsKeyframes()) // pasting keyframes is handled in TimelineView
        return;

    auto pasteModel = DesignDocumentView::pasteToModel();

    if (!pasteModel)
        return;

    DesignDocumentView view;
    pasteModel->attachView(&view);
    ModelNode rootNode(view.rootModelNode());
    QList<ModelNode> selectedNodes = rootNode.directSubModelNodes();
    pasteModel->detachView(&view);

    if (rootNode.type() == "empty")
        return;

    if (rootNode.id() == "__multi__selection__") { // pasting multiple objects
        currentModel()->attachView(&view);

        ModelNode targetNode;

        if (!view.selectedModelNodes().isEmpty())
            targetNode = view.firstSelectedModelNode();

        // in case we copy and paste a selection we paste in the parent item
        if ((view.selectedModelNodes().count() == selectedNodes.count()) && targetNode.isValid() && targetNode.hasParentProperty()) {
            targetNode = targetNode.parentProperty().parentModelNode();
        } else if (view.selectedModelNodes().isEmpty()) {
            // if selection is empty and copied nodes are all 3D nodes, paste them under the active scene
            bool all3DNodes = std::find_if(selectedNodes.cbegin(), selectedNodes.cend(),
                                           [](const ModelNode &node) { return !node.isSubclassOf("QtQuick3D.Node"); })
                              == selectedNodes.cend();
            if (all3DNodes) {
                auto data = rootModelNode().auxiliaryData(active3dSceneProperty);
                if (data) {
                    if (int activeSceneId = data->toInt(); activeSceneId != -1) {
                        NodeListProperty sceneNodeProperty = QmlVisualNode::findSceneNodeProperty(
                            rootModelNode().view(), activeSceneId);
                        targetNode = sceneNodeProperty.parentModelNode();
                    }
                }
            }
        }

        if (!targetNode.isValid())
            targetNode = view.rootModelNode();

        for (const ModelNode &node : qAsConst(selectedNodes)) {
            for (const ModelNode &node2 : qAsConst(selectedNodes)) {
                if (node.isAncestorOf(node2))
                    selectedNodes.removeAll(node2);
            }
        }

        rewriterView()->executeInTransaction("DesignDocument::paste1", [&view, selectedNodes, targetNode]() {
            QList<ModelNode> pastedNodeList;

            const double scatterRange = 20.;
            int offset = QRandomGenerator::global()->generateDouble() * scatterRange - scatterRange / 2;

            const auto defaultPropertyName = targetNode.metaInfo().defaultPropertyName();
            auto parentProperty = targetNode.nodeListProperty(defaultPropertyName);
            for (const ModelNode &node : qAsConst(selectedNodes)) {
                ModelNode pastedNode(view.insertModel(node));
                pastedNodeList.append(pastedNode);
                scatterItem(pastedNode, targetNode, offset);
                parentProperty.reparentHere(pastedNode);
            }

            view.setSelectedModelNodes(pastedNodeList);
        });

    } else { // pasting single object
        rewriterView()->executeInTransaction("DesignDocument::paste1", [this, &view, rootNode]() {
            currentModel()->attachView(&view);
            ModelNode pastedNode(view.insertModel(rootNode));
            ModelNode targetNode;

            if (!view.selectedModelNodes().isEmpty()) {
                targetNode = view.firstSelectedModelNode();
            } else {
                // if selection is empty and this is a 3D Node, paste it under the active scene
                if (pastedNode.isSubclassOf("QtQuick3D.Node")) {
                    auto data = rootModelNode().auxiliaryData(active3dSceneProperty);
                    if (data) {
                        if (int activeSceneId = data->toInt(); activeSceneId != -1) {
                            NodeListProperty sceneNodeProperty = QmlVisualNode::findSceneNodeProperty(
                                rootModelNode().view(), activeSceneId);
                            targetNode = sceneNodeProperty.parentModelNode();
                        }
                    }
                }
            }

            if (!targetNode.isValid())
                targetNode = view.rootModelNode();

            if (targetNode.hasParentProperty() &&
                pastedNode.simplifiedTypeName() == targetNode.simplifiedTypeName() &&
                pastedNode.variantProperty("width").value() == targetNode.variantProperty("width").value() &&
                pastedNode.variantProperty("height").value() == targetNode.variantProperty("height").value()) {
                targetNode = targetNode.parentProperty().parentModelNode();
            }

            PropertyName defaultProperty(targetNode.metaInfo().defaultPropertyName());

            scatterItem(pastedNode, targetNode);
            if (targetNode.metaInfo().property(defaultProperty).isListProperty())
                targetNode.nodeListProperty(defaultProperty).reparentHere(pastedNode);
            else
                qWarning() << "Cannot reparent to" << targetNode;

            view.setSelectedModelNodes({pastedNode});
        });
        view.model()->clearMetaInfoCache();
    }
}

void DesignDocument::selectAll()
{
    if (!currentModel())
        return;

    DesignDocumentView view;
    currentModel()->attachView(&view);

    QList<ModelNode> allNodesExceptRootNode(view.allModelNodes());
    allNodesExceptRootNode.removeOne(view.rootModelNode());
    view.setSelectedModelNodes(allNodesExceptRootNode);
}

RewriterView *DesignDocument::rewriterView() const
{
    return m_rewriterView.data();
}

void DesignDocument::setEditor(Core::IEditor *editor)
{
    m_textEditor = editor;
    // if the user closed the file explicit we do not want to do anything with it anymore

    connect(Core::EditorManager::instance(), &Core::EditorManager::aboutToSave,
            this, [this](Core::IDocument *document) {
        if (m_textEditor && m_textEditor->document() == document) {
            if (m_documentModel && m_documentModel->rewriterView()) {
                if (fileName().completeSuffix() == "ui.qml")
                    m_documentModel->rewriterView()->sanitizeModel();
                m_documentModel->rewriterView()->writeAuxiliaryData();
            }
        }
    });

    connect(Core::EditorManager::instance(), &Core::EditorManager::editorAboutToClose,
            this, [this](Core::IEditor *editor) {
        if (m_textEditor.data() == editor)
            m_textEditor.clear();
    });

    connect(editor->document(), &Core::IDocument::filePathChanged, this, &DesignDocument::updateFileName);

    updateActiveTarget();
    updateActiveTarget();
}

Core::IEditor *DesignDocument::editor() const
{
    return m_textEditor.data();
}

TextEditor::BaseTextEditor *DesignDocument::textEditor() const
{
    return qobject_cast<TextEditor::BaseTextEditor*>(editor());
}

QPlainTextEdit *DesignDocument::plainTextEdit() const
{
    if (editor())
        return qobject_cast<QPlainTextEdit*>(editor()->widget());

    return nullptr;
}

ModelNode DesignDocument::rootModelNode() const
{
    return rewriterView()->rootModelNode();
}

void DesignDocument::undo()
{
    if (rewriterView() && !rewriterView()->modificationGroupActive())
        plainTextEdit()->undo();

    viewManager().resetPropertyEditorView();
}

void DesignDocument::redo()
{
    if (rewriterView() && !rewriterView()->modificationGroupActive())
        plainTextEdit()->redo();

    viewManager().resetPropertyEditorView();
}

static Target *getActiveTarget(DesignDocument *designDocument)
{
    Project *currentProject = SessionManager::projectForFile(designDocument->fileName());

    if (!currentProject)
        currentProject = ProjectTree::currentProject();

    if (!currentProject)
        return nullptr;

    QObject::connect(ProjectTree::instance(), &ProjectTree::currentProjectChanged,
                     designDocument, &DesignDocument::updateActiveTarget, Qt::UniqueConnection);

    QObject::connect(currentProject, &Project::activeTargetChanged,
                     designDocument, &DesignDocument::updateActiveTarget, Qt::UniqueConnection);

    Target *target = currentProject->activeTarget();

    if (!target || !target->kit()->isValid())
        return nullptr;

    QObject::connect(target, &Target::kitChanged,
                     designDocument, &DesignDocument::updateActiveTarget, Qt::UniqueConnection);

    return target;
}

void DesignDocument::updateActiveTarget()
{
    m_currentTarget = getActiveTarget(this);
    viewManager().setNodeInstanceViewTarget(m_currentTarget);
}

void DesignDocument::contextHelp(const Core::IContext::HelpCallback &callback) const
{
    if (view())
        view()->contextHelp(callback);
    else
        callback({});
}

} // namespace QmlDesigner
