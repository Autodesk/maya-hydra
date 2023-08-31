
#include "adskHydraSceneBrowserTestFixture.h"

#include "dataSourceTreeWidget.h"
#include "dataSourceValueTreeView.h"
#include "sceneIndexTreeWidget.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>

#include <gtest/gtest.h>

#include <QApplication>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <iostream>
#include <stack>
#include <vector>

template <class ChildType> ChildType* FindFirstChild(QObject* qObject)
{
    for (QObject* child : qObject->children()) {
        ChildType* castChild = qobject_cast<ChildType*>(child);
        if (castChild) {
            return castChild;
        }
    }
    return nullptr;
}

QTreeWidgetItemIterator GetIteratorForTree(QTreeWidget* treeWidget)
{
    // Expand all items so the iterator can traverse them
    treeWidget->expandAll();
    // Immediately process queued events, otherwise some events might linger and lead to a crash
    // trying to access since-deleted items once the Qt event loop resumes and processes the events.
    // (e.g. without this there is a crash involving a setExpanded() call)
    QApplication::processEvents(QEventLoop::ProcessEventsFlag::EventLoopExec);
    return QTreeWidgetItemIterator(treeWidget);
}

pxr::HdSceneIndexBasePtr AdskHydraSceneBrowserTestFixture::sceneIndex = nullptr;

void AdskHydraSceneBrowserTestFixture::SetUp()
{
    ASSERT_NE(sceneIndex, nullptr);

    _sceneBrowserWidget->setWindowTitle("Test Hydra Scene Browser");
    _sceneBrowserWidget->SetSceneIndex("", sceneIndex, true);
    _sceneBrowserWidget->show();

    QSplitter* sceneBrowserSplitter = FindFirstChild<QSplitter>(_sceneBrowserWidget.get());
    ASSERT_NE(sceneBrowserSplitter, nullptr);

    _primHierarchyWidget = FindFirstChild<pxr::HduiSceneIndexTreeWidget>(sceneBrowserSplitter);
    ASSERT_NE(_primHierarchyWidget, nullptr);
    _dataSourceHierarchyWidget
        = FindFirstChild<pxr::HduiDataSourceTreeWidget>(sceneBrowserSplitter);
    ASSERT_NE(_dataSourceHierarchyWidget, nullptr);
    _dataSourceValueView = FindFirstChild<pxr::HduiDataSourceValueTreeView>(sceneBrowserSplitter);
    ASSERT_NE(_dataSourceValueView, nullptr);
}

void AdskHydraSceneBrowserTestFixture::TearDown() { _sceneBrowserWidget->close(); }

void AdskHydraSceneBrowserTestFixture::SetReferenceSceneIndex(
    pxr::HdSceneIndexBasePtr referenceSceneIndex)
{
    sceneIndex = referenceSceneIndex;
}

void AdskHydraSceneBrowserTestFixture::ComparePrimHierarchy(
    bool compareDataSourceHierarchy,
    bool compareDataSourceValues)
{
    // Setup traversal data structures (depth-first search)
    QTreeWidgetItemIterator  itPrimsTreeWidget = GetIteratorForTree(_primHierarchyWidget);
    std::stack<pxr::SdfPath> primPathsStack({ pxr::SdfPath::AbsoluteRootPath() });

    // Traverse hierarchy and compare (depth-first search)
    while (*itPrimsTreeWidget && !primPathsStack.empty()) {
        // Get the objects for the current step
        QTreeWidgetItem* primQtItem = *itPrimsTreeWidget;
        pxr::SdfPath     primPath = primPathsStack.top();

        // Compare prim name
        std::string actualPrimName = primQtItem->text(0).toStdString();
        // SdfPath::GetElementString returns an empty string if the path is the the absolute root
        // (/), as it is not considered to be an element. However, the browser does displays it as
        // "/".
        std::string expectedPrimName
            = primPath.IsAbsoluteRootPath() ? "/" : primPath.GetElementString();
        EXPECT_EQ(actualPrimName, expectedPrimName);

        // Compare prim type
        pxr::HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
        if (primQtItem->columnCount() > 1) {
            std::string actualPrimType = primQtItem->text(1).toStdString();
            std::string expectedPrimType = prim.primType;
            EXPECT_EQ(actualPrimType, expectedPrimType);
        } else {
            // In this case, the Qt prim item only has a column for its name,
            // so we at least make sure the prim type is empty.
            // So far it seems this case only happens for the root path.
            EXPECT_EQ(prim.primType, pxr::TfToken())
                << "Prim had a non-empty type but its Qt item had no column for it.";
        }

        // Compare data source
        if (compareDataSourceHierarchy) {
            _primHierarchyWidget->setCurrentItem(primQtItem);
            CompareDataSourceHierarchy(
                { primPath.GetElementToken(), prim.dataSource }, compareDataSourceValues);
        }

        // Prepare next step (need to pop the stack before pushing the next elements)
        itPrimsTreeWidget++;
        primPathsStack.pop();

        // Push child paths on the stack
        pxr::SdfPathVector childPaths = sceneIndex->GetChildPrimPaths(primPath);
        for (auto itChildPaths = childPaths.rbegin(); itChildPaths != childPaths.rend();
             itChildPaths++) {
            primPathsStack.push(*itChildPaths);
        }
    }
}

void AdskHydraSceneBrowserTestFixture::CompareDataSourceHierarchy(
    DataSourceEntry rootDataSourceEntry,
    bool            compareValues)
{
    // Setup traversal data structures (depth-first search)
    QTreeWidgetItemIterator itDataSourceTreeWidget = GetIteratorForTree(_dataSourceHierarchyWidget);
    std::stack<DataSourceEntry> dataSourceStack({ rootDataSourceEntry });

    // Traverse hierarchy and compare (depth-first search)
    while (*itDataSourceTreeWidget && !dataSourceStack.empty()) {
        // Get the objects for the current step
        QTreeWidgetItem* dataSourceQtItem = *itDataSourceTreeWidget;
        DataSourceEntry  dataSourceEntry = dataSourceStack.top();

        // Compare data source name
        std::string actualDataSourceName = dataSourceQtItem->text(0).toStdString();
        std::string expectedDataSourceName = dataSourceEntry.name;
        EXPECT_EQ(actualDataSourceName, expectedDataSourceName);

        // Compare data source value
        if (compareValues) {
            _dataSourceHierarchyWidget->setCurrentItem(dataSourceQtItem);
            if (auto sampledDataSource
                = pxr::HdSampledDataSource::Cast(dataSourceEntry.dataSource)) {
                CompareDataSourceValue(sampledDataSource);
            }
        }

        // Prepare next step (need to pop the stack before pushing the next elements)
        itDataSourceTreeWidget++;
        dataSourceStack.pop();

        // Push child data sources on the stack
        if (auto containerDataSource
            = pxr::HdContainerDataSource::Cast(dataSourceEntry.dataSource)) {
            pxr::TfTokenVector childNames = containerDataSource->GetNames();
            for (auto itChildNames = childNames.rbegin(); itChildNames != childNames.rend();
                 itChildNames++) {
                pxr::TfToken                dataSourceName = *itChildNames;
                pxr::HdDataSourceBaseHandle dataSource = containerDataSource->Get(dataSourceName);
                if (dataSource) {
                    dataSourceStack.push({ dataSourceName, dataSource });
                }
            }
        } else if (
            auto vectorDataSource = pxr::HdVectorDataSource::Cast(dataSourceEntry.dataSource)) {
            for (size_t iElement = 0; iElement < vectorDataSource->GetNumElements(); iElement++) {
                size_t reversedElementIndex = vectorDataSource->GetNumElements() - 1 - iElement;
                pxr::TfToken dataSourceName = pxr::TfToken(std::to_string(reversedElementIndex));
                pxr::HdDataSourceBaseHandle dataSource
                    = vectorDataSource->GetElement(reversedElementIndex);
                if (dataSource) {
                    dataSourceStack.push({ dataSourceName, dataSource });
                }
            }
        }
    }
}

void AdskHydraSceneBrowserTestFixture::CompareDataSourceValue(
    pxr::HdSampledDataSourceHandle sampledDataSource)
{
    _dataSourceValueView->expandAll();

    pxr::VtValue value = sampledDataSource->GetValue(0.0f);

    // The supported value types can be found in dataSourceValueTreeView.cpp, in the
    // Hdui_GetModelFromValue function.
    if (!value.IsArrayValued()) {
        CompareValueContent(value);
    } else {
        CompareIfArray<int>(value);
        CompareIfArray<float>(value);
        CompareIfArray<double>(value);
        CompareIfArray<pxr::TfToken>(value);
        CompareIfArray<pxr::SdfPath>(value);
        CompareIfArray<pxr::GfVec3f>(value);
        CompareIfArray<pxr::GfVec3d>(value);
        CompareIfArray<pxr::GfMatrix4d>(value);
        CompareIfArray<pxr::GfVec2f>(value);
    }
}

void AdskHydraSceneBrowserTestFixture::CompareValueContent(const pxr::VtValue& value)
{
    QAbstractItemModel* dataSourceItemModel = _dataSourceValueView->model();
    EXPECT_EQ(dataSourceItemModel->rowCount(), 1);

    QModelIndex valueIndex = dataSourceItemModel->index(0, 0);
    QVariant    valueData = dataSourceItemModel->data(valueIndex, Qt::DisplayRole);
    QString     valueText = valueData.toString();
    std::string actualValue = valueText.toStdString();

    std::ostringstream valueStream;
    valueStream << value;
    std::string expectedValue = valueStream.str();

    EXPECT_EQ(actualValue, expectedValue);
}

template <typename ElementType>
void AdskHydraSceneBrowserTestFixture::CompareIfArray(const pxr::VtValue& value)
{
    if (value.IsHolding<pxr::VtArray<ElementType>>()) {
        CompareArrayContents<ElementType>(value.UncheckedGet<pxr::VtArray<ElementType>>());
    }
}

template <typename ElementType>
void AdskHydraSceneBrowserTestFixture::CompareArrayContents(
    const pxr::VtArray<ElementType>& vtArray)
{
    QAbstractItemModel* dataSourceItemModel = _dataSourceValueView->model();
    EXPECT_EQ(dataSourceItemModel->rowCount(), vtArray.size());

    size_t nbRowsToTraverse
        = std::min(static_cast<size_t>(dataSourceItemModel->rowCount()), vtArray.size());
    for (size_t iRow = 0; iRow < nbRowsToTraverse; iRow++) {
        QModelIndex valueIndex = dataSourceItemModel->index(iRow, 0);
        QVariant    valueData = dataSourceItemModel->data(valueIndex, Qt::DisplayRole);
        QString     valueText = valueData.toString();
        std::string actualValue = valueText.toStdString();

        std::ostringstream valueStream;
        valueStream << vtArray.cdata()[iRow];
        std::string expectedValue = valueStream.str();

        EXPECT_EQ(actualValue, expectedValue);
    }
}
