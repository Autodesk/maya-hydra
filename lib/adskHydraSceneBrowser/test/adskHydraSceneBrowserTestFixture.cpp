// Copyright 2023 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
#include <regex>
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
                { primPath.GetNameToken(), prim.dataSource }, compareDataSourceValues);
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

bool AdskHydraSceneBrowserTestFixture::MatchesFallbackTextOutput(const std::string& text) {
    // Regex for matching the fallback text output used for types that don't provide a custom one.
    // Identifies a literal <', followed by a valid C++ type name (possibly templated), then a literal ',
    // then a space, an @ symbol and another space, a hexadecimal 32 to 64 bit address (case-insensitive,
    // potentially prefixed with 0x), and finally a literal >. Example matches :
    // <'ArResolverContext' @ 0x251ffa80> // Linux
    // <'ArResolverContext' @ 000001D3A4296670> // Windows
    // <'vector<SdfPath, allocator<SdfPath> >' @ 0x261b8c20> // Linux
    // <'vector<SdfPath,allocator<SdfPath> >' @ 000001D49F3390B0> // Windows

    // The space in the second [] group is intentional and must be matched,
    // see the above templated examples
    std::regex fallbackTextOutputRegex("<'[a-zA-Z_][ a-zA-Z_0-9<>,&*()]*' @ (0x)?[0-9a-fA-F]{8,16}>");
    return std::regex_match(text, fallbackTextOutputRegex);
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

    if (!MatchesFallbackTextOutput(expectedValue)) {
        // Happy path : the concrete type of the VtValue supports text output.
        // (this is an assumption and not a truly reliable check; see the not-so-happy path
        // for more details)
        EXPECT_EQ(actualValue, expectedValue);
    } else {
        // Not-so-happy path : the concrete type of the VtValue does not support text output.
        //
        // If a type does not provide custom text output, this will cause Vt_StreamOutGeneric to be
        // called by Vt_StreamOutImpl (see streamOut.h/.cpp). This function outputs the name of the
        // concrete type followed by the address of the held object. Example outputs :
        // <'ArResolverContext' @ 0x251ffa80>
        // <'ArResolverContext' @ 000001D3A4296670>
        //
        // Since it is possible that some data sources return a copy of their underlying object when
        // calling GetValue(), the object held by the VtValue passed in as the parameter to this
        // function may differ from the one held by the VtValue used by the scene browser. In such
        // cases, the printed addresses won't match and the test will fail.
        //
        // This workaround instead compares the values only up to their type name in these cases.
        // The regex check could technically prevent fully comparing values if their custom text
        // output perfectly matches the regex, but this seems very unlikely.
        std::string valueTypeName = value.GetTypeName();
        size_t      indexOfTypeName = expectedValue.find(valueTypeName);
        size_t      substringComparisonLength = indexOfTypeName + valueTypeName.size();
        EXPECT_EQ(
            actualValue.substr(0, substringComparisonLength),
            expectedValue.substr(0, substringComparisonLength));
    }
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
    EXPECT_EQ(static_cast<size_t>(dataSourceItemModel->rowCount()), vtArray.size());

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
