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
#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/primOriginSchema.h>

#include <gtest/gtest.h>

#include <QApplication>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <iostream>
#include <regex>
#include <set>
#include <stack>
#include <vector>

namespace {

#if PXR_VERSION >= 2603
// Matches HduiDataSourceTreeWidget's name ordering (USD 26.03+).
std::vector<PXR_NS::TfToken>
GetSortedContainerChildNames(const PXR_NS::HdContainerDataSourceHandle& container)
{
    const PXR_NS::TfTokenVector names = container->GetNames();
    const std::set<PXR_NS::TfToken, PXR_NS::TfDictionaryLessThan> sortedNames(
        names.begin(), names.end());
    return std::vector<PXR_NS::TfToken>(sortedNames.begin(), sortedNames.end());
}

void PushSortedContainerChildrenOnStack(
    const PXR_NS::HdContainerDataSourceHandle&           container,
    const PXR_NS::HdDataSourceLocator&                     parentLocator,
    std::stack<DataSourceEntry>&                           dataSourceStack)
{
    const std::vector<PXR_NS::TfToken> sortedChildNames = GetSortedContainerChildNames(container);
    for (auto itChildNames = sortedChildNames.rbegin(); itChildNames != sortedChildNames.rend();
         ++itChildNames) {
        const PXR_NS::TfToken& childName = *itChildNames;
        if (PXR_NS::HdDataSourceBaseHandle childDataSource = container->Get(childName)) {
            dataSourceStack.push(
                { childName, childDataSource, parentLocator.Append(childName) });
        }
    }
}
#endif

std::stack<DataSourceEntry> BuildInitialDataSourceStack(
    const PXR_NS::SdfPath& primPath, const PXR_NS::HdSceneIndexPrim& prim)
{
    std::stack<DataSourceEntry> dataSourceStack;

#if PXR_VERSION >= 2603
    // From USD 26.03, HduiDataSourceTreeWidget::SetPrimDataSource lists sorted
    // container children as top-level items instead of the prim data source
    // container itself (introduced by OpenUSD commit 6be1d6ec75).
    if (PXR_NS::HdContainerDataSourceHandle container
        = PXR_NS::HdContainerDataSource::Cast(prim.dataSource)) {
        PushSortedContainerChildrenOnStack(
            container, PXR_NS::HdDataSourceLocator(), dataSourceStack);
    } else if (prim.dataSource) {
        dataSourceStack.push(
            { primPath.GetNameToken(), prim.dataSource, PXR_NS::HdDataSourceLocator() });
    }
#else
    // USD < 26.03: SetPrimDataSource shows the prim data source container
    // itself as the single top-level item, with text = primPath.GetNameToken().
    dataSourceStack.push(
        { primPath.GetNameToken(), prim.dataSource, PXR_NS::HdDataSourceLocator() });
#endif

    return dataSourceStack;
}

} // namespace

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

int CountTreeItems(QTreeWidget* treeWidget)
{
    int count = 0;
    for (QTreeWidgetItemIterator it(treeWidget); *it; ++it) {
        ++count;
    }
    return count;
}

QTreeWidgetItemIterator GetIteratorForTree(QTreeWidget* treeWidget)
{
    // Hdui_DataSourceTreeWidgetItem builds children lazily (on first expand).
    // A static _expandedSet persists across prim selections: items whose
    // locator is in the set schedule their expansion via QTimer::singleShot(0).
    // processEvents() fires those timers and creates a new generation of child
    // items — but expandAll() has already returned, so those new children are
    // never expanded themselves.  Loop until the item count stabilises so that
    // every generation of lazily-built children is fully expanded before the
    // iterator is created.
    static constexpr int kMaxExpansionIterations = 20;
    int prevCount = -1;
    int currCount = 0;
    int iterations = 0;
    while (currCount != prevCount) {
        EXPECT_LT(iterations, kMaxExpansionIterations)
            << "Data source tree did not stabilise after " << kMaxExpansionIterations
            << " expansion iterations — possible infinite loop in lazy item creation.";
        if (iterations >= kMaxExpansionIterations) {
            break;
        }
        prevCount = currCount;
        treeWidget->expandAll();
        // Process queued events: fires deferred QTimer::singleShot expansions
        // and avoids crashes with since-deleted items (see original comment).
        QApplication::processEvents(QEventLoop::ProcessEventsFlag::EventLoopExec);
        currCount = CountTreeItems(treeWidget);
        ++iterations;
    }
    return QTreeWidgetItemIterator(treeWidget);
}

PXR_NS::HdSceneIndexBasePtr AdskHydraSceneBrowserTestFixture::sceneIndex = nullptr;

void AdskHydraSceneBrowserTestFixture::SetUp()
{
    ASSERT_NE(sceneIndex, nullptr);

    _sceneBrowserWidget->setWindowTitle("Test Hydra Scene Browser");
    _sceneBrowserWidget->SetSceneIndex("", sceneIndex, true);
    _sceneBrowserWidget->show();

    QSplitter* sceneBrowserSplitter = FindFirstChild<QSplitter>(_sceneBrowserWidget.get());
    ASSERT_NE(sceneBrowserSplitter, nullptr);

    _primHierarchyWidget = FindFirstChild<PXR_NS::HduiSceneIndexTreeWidget>(sceneBrowserSplitter);
    ASSERT_NE(_primHierarchyWidget, nullptr);
    _dataSourceHierarchyWidget
        = FindFirstChild<PXR_NS::HduiDataSourceTreeWidget>(sceneBrowserSplitter);
    ASSERT_NE(_dataSourceHierarchyWidget, nullptr);
    _dataSourceValueView = FindFirstChild<PXR_NS::HduiDataSourceValueTreeView>(sceneBrowserSplitter);
    ASSERT_NE(_dataSourceValueView, nullptr);
}

void AdskHydraSceneBrowserTestFixture::TearDown() { _sceneBrowserWidget->close(); }

void AdskHydraSceneBrowserTestFixture::SetReferenceSceneIndex(
    PXR_NS::HdSceneIndexBasePtr referenceSceneIndex)
{
    sceneIndex = referenceSceneIndex;
}

void AdskHydraSceneBrowserTestFixture::ComparePrimHierarchy(
    bool compareDataSourceHierarchy,
    bool compareDataSourceValues)
{
    // Setup traversal data structures (depth-first search)
    QTreeWidgetItemIterator  itPrimsTreeWidget = GetIteratorForTree(_primHierarchyWidget);
    std::stack<PXR_NS::SdfPath> primPathsStack({ PXR_NS::SdfPath::AbsoluteRootPath() });

    // Traverse hierarchy and compare (depth-first search)
    while (*itPrimsTreeWidget && !primPathsStack.empty()) {
        // Get the objects for the current step
        QTreeWidgetItem* primQtItem = *itPrimsTreeWidget;
        PXR_NS::SdfPath     primPath = primPathsStack.top();

        // Compare prim name
        std::string actualPrimName = primQtItem->text(0).toStdString();
        // SdfPath::GetElementString returns an empty string if the path is the the absolute root
        // (/), as it is not considered to be an element. However, the browser does displays it as
        // "/".
        std::string expectedPrimName
            = primPath.IsAbsoluteRootPath() ? "/" : primPath.GetElementString();
        EXPECT_EQ(actualPrimName, expectedPrimName);

        // Compare prim type
        PXR_NS::HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
        if (primQtItem->columnCount() > 1) {
            std::string actualPrimType = primQtItem->text(1).toStdString();
            std::string expectedPrimType = prim.primType;
            EXPECT_EQ(actualPrimType, expectedPrimType);
        } else {
            // In this case, the Qt prim item only has a column for its name,
            // so we at least make sure the prim type is empty.
            // So far it seems this case only happens for the root path.
            EXPECT_EQ(prim.primType, PXR_NS::TfToken())
                << "Prim had a non-empty type but its Qt item had no column for it.";
        }

        // Compare data source
        if (compareDataSourceHierarchy) {
            _primHierarchyWidget->setCurrentItem(primQtItem);
            CompareDataSourceHierarchy(primPath, BuildInitialDataSourceStack(primPath, prim),
                compareDataSourceValues);
        }

        // Prepare next step (need to pop the stack before pushing the next elements)
        itPrimsTreeWidget++;
        primPathsStack.pop();

        // Push child paths on the stack in the same order used by
        // HduiSceneIndexTreeWidget.
        const PXR_NS::SdfPathVector childPathVec = sceneIndex->GetChildPrimPaths(primPath);
#if PXR_VERSION >= 2603
        // USD 26.03+: children are listed in sorted order (see OpenUSD 6be1d6ec75).
        const PXR_NS::SdfPathSet sortedChildPaths(childPathVec.begin(), childPathVec.end());
        for (auto itChildPaths = sortedChildPaths.rbegin(); itChildPaths != sortedChildPaths.rend();
             ++itChildPaths) {
            primPathsStack.push(*itChildPaths);
        }
#else
        // USD < 26.03: children follow GetChildPrimPaths() order; push in
        // reversed order so stack pops in forward (matching) order.
        for (auto itChildPaths = childPathVec.rbegin(); itChildPaths != childPathVec.rend();
             ++itChildPaths) {
            primPathsStack.push(*itChildPaths);
        }
#endif
    }

    // Ensure both sides are fully exhausted — if one has remaining items the
    // traversal would have silently stopped short.
    EXPECT_FALSE(*itPrimsTreeWidget) << "Qt prim tree has more items than expected by the scene index";
    EXPECT_TRUE(primPathsStack.empty()) << "Scene index has more prims than present in the Qt prim tree";
}

void AdskHydraSceneBrowserTestFixture::CompareDataSourceHierarchy(
    const PXR_NS::SdfPath&            primPath,
    std::stack<DataSourceEntry>       initialDataSourceStack,
    bool                              compareValues)
{
    // Setup traversal data structures (depth-first search)
    QTreeWidgetItemIterator itDataSourceTreeWidget = GetIteratorForTree(_dataSourceHierarchyWidget);
    std::stack<DataSourceEntry> dataSourceStack = std::move(initialDataSourceStack);

    // Traverse hierarchy and compare (depth-first search)
    while (*itDataSourceTreeWidget && !dataSourceStack.empty()) {
        // Get the objects for the current step
        QTreeWidgetItem* dataSourceQtItem = *itDataSourceTreeWidget;
        DataSourceEntry  dataSourceEntry = dataSourceStack.top();

        // Compare data source name
        CompareDataSourceName(primPath, dataSourceQtItem, dataSourceEntry);

        // Compare data source value
        if (compareValues) {
            _dataSourceHierarchyWidget->setCurrentItem(dataSourceQtItem);
            if (auto sampledDataSource
                = PXR_NS::HdSampledDataSource::Cast(dataSourceEntry.dataSource)) {
                CompareDataSourceValue(sampledDataSource);
            }
        }

        // Prepare next step (need to pop the stack before pushing the next elements)
        itDataSourceTreeWidget++;
        dataSourceStack.pop();

        // Push child data sources on the stack
        if (auto containerDataSource
            = PXR_NS::HdContainerDataSource::Cast(dataSourceEntry.dataSource)) {
#if PXR_VERSION >= 2603
            // USD 26.03+: _BuildChildren uses sorted order (see 6be1d6ec75).
            PushSortedContainerChildrenOnStack(
                containerDataSource, dataSourceEntry.locator, dataSourceStack);
#else
            // USD < 26.03: _BuildChildren uses GetNames() forward order; push
            // in reversed order so stack pops in forward (matching) order.
            PXR_NS::TfTokenVector childNames = containerDataSource->GetNames();
            for (auto it = childNames.rbegin(); it != childNames.rend(); ++it) {
                if (PXR_NS::HdDataSourceBaseHandle ds = containerDataSource->Get(*it)) {
                    dataSourceStack.push(
                        { *it, ds, dataSourceEntry.locator.Append(*it) });
                }
            }
#endif
        } else if (
            auto vectorDataSource = PXR_NS::HdVectorDataSource::Cast(dataSourceEntry.dataSource)) {
            for (size_t iElement = 0; iElement < vectorDataSource->GetNumElements(); iElement++) {
                size_t reversedElementIndex = vectorDataSource->GetNumElements() - 1 - iElement;
                PXR_NS::TfToken dataSourceName = PXR_NS::TfToken("i" + std::to_string(reversedElementIndex));
                PXR_NS::HdDataSourceBaseHandle dataSource
                    = vectorDataSource->GetElement(reversedElementIndex);
                if (dataSource) {
                    dataSourceStack.push({ dataSourceName, dataSource });
                }
            }
        }
    }

    // Ensure both sides are fully exhausted — if one has remaining items the
    // traversal would have silently stopped short (e.g. when the initial stack
    // is empty on USD 26.03+ and the UI still has stale entries).
    EXPECT_FALSE(*itDataSourceTreeWidget)
        << "Qt data source tree has more items than expected for prim " << primPath.GetText();
    EXPECT_TRUE(dataSourceStack.empty())
        << "Expected more data source items than present in the Qt tree for prim " << primPath.GetText();
}

void AdskHydraSceneBrowserTestFixture::CompareDataSourceName(
    const PXR_NS::SdfPath& primPath,
    const QTreeWidgetItem* dataSourceQtItem,
    const DataSourceEntry& dataSourceEntry)
{
    std::string actualDataSourceName = dataSourceQtItem->text(0).toStdString();
    std::string expectedDataSourceName = dataSourceEntry.name;

#if PXR_VERSION >= 2511
    // Special case for some expected names.
    // See https://github.com/PixarAnimationStudios/OpenUSD/commit/f475246
    if (!dataSourceEntry.locator.IsEmpty()) {
        const PXR_NS::TfToken& lastElement = dataSourceEntry.locator.GetLastElement();
        if (!lastElement.IsEmpty()) {
            expectedDataSourceName = lastElement.GetText();
        } else {
            expectedDataSourceName
                = dataSourceEntry.locator.HasPrefix(PXR_NS::HdMaterialSchema::GetDefaultLocator())
                ? PXR_NS::HdMaterialSchemaTokens->_universalRenderContextToken
                : dataSourceEntry.locator.HasPrefix(
                      PXR_NS::HdMaterialBindingsSchema::GetDefaultLocator())
                ? PXR_NS::HdMaterialBindingsSchemaTokens->_allPurposeToken
                : PXR_NS::TfToken("<empty>");
        }
    }
#endif

    EXPECT_EQ(actualDataSourceName, expectedDataSourceName) << " for prim " << primPath.GetText();
}

void AdskHydraSceneBrowserTestFixture::CompareDataSourceValue(
    PXR_NS::HdSampledDataSourceHandle sampledDataSource)
{
    _dataSourceValueView->expandAll();

    PXR_NS::VtValue value = sampledDataSource->GetValue(0.0f);

    // The supported value types can be found in dataSourceValueTreeView.cpp, in the
    // Hdui_GetModelFromValue function.
    if (!value.IsArrayValued()) {
        CompareValueContent(value);
    } else {
        CompareIfArray<int>(value);
        CompareIfArray<float>(value);
        CompareIfArray<double>(value);
        CompareIfArray<PXR_NS::TfToken>(value);
        CompareIfArray<PXR_NS::SdfPath>(value);
        CompareIfArray<PXR_NS::GfVec3f>(value);
        CompareIfArray<PXR_NS::GfVec3d>(value);
        CompareIfArray<PXR_NS::GfMatrix4d>(value);
        CompareIfArray<PXR_NS::GfVec2f>(value);
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

void AdskHydraSceneBrowserTestFixture::CompareValueContent(const PXR_NS::VtValue& value)
{
    QAbstractItemModel* dataSourceItemModel = _dataSourceValueView->model();
    EXPECT_EQ(dataSourceItemModel->rowCount(), 1);

    QModelIndex valueIndex = dataSourceItemModel->index(0, 0);
    QVariant    valueData = dataSourceItemModel->data(valueIndex, Qt::DisplayRole);
    QString     valueText = valueData.toString();
    std::string actualValue = valueText.toStdString();

    std::ostringstream valueStream;
#if PXR_VERSION < 2408
    valueStream << value;
#else
    if (value.IsHolding<PXR_NS::SdfPathVector>()) {
        // Special case for SdfPathVector.
        // See https://github.com/PixarAnimationStudios/OpenUSD/commit/1d19b1d
        PXR_NS::SdfPathVector paths = value.Get<PXR_NS::SdfPathVector>();
        for (PXR_NS::SdfPath const& path : paths) {
            valueStream << path << "\n";
        }
    } else if (value.IsHolding<PXR_NS::HdPrimOriginSchema::OriginPath>()) {
        // Mirror HduiDataSourceValueTreeView: when OriginPath is a registered
        // Vt type, operator<< outputs "HdPrimOriginSchema::OriginPath(<path>)"
        // and the widget displays that same string. When OriginPath is not yet
        // a registered Vt type, the widget calls .GetPath() directly. Detect
        // at runtime so this works across USD builds regardless of what
        // PXR_VERSION says.
        //
        // We cannot use MatchesFallbackTextOutput here: its regex excludes ':'
        // so the unregistered fallback "<'HdPrimOriginSchema::OriginPath' @
        // 0x...>" would not be detected. Instead check the prefix directly.
        std::ostringstream probeStream;
        probeStream << value;
        const std::string probeOutput = probeStream.str();
        if (probeOutput.rfind("HdPrimOriginSchema::OriginPath(", 0) == 0) {
            // OriginPath is registered: widget uses VtValue streaming.
            valueStream << probeOutput;
        } else {
            // OriginPath is not registered: widget calls .GetPath().
            valueStream << value.UncheckedGet<PXR_NS::HdPrimOriginSchema::OriginPath>().GetPath();
        }
    } else {
        valueStream << value;
    }
#endif
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
void AdskHydraSceneBrowserTestFixture::CompareIfArray(const PXR_NS::VtValue& value)
{
    if (value.IsHolding<PXR_NS::VtArray<ElementType>>()) {
        CompareArrayContents<ElementType>(value.UncheckedGet<PXR_NS::VtArray<ElementType>>());
    }
}

template <typename ElementType>
void AdskHydraSceneBrowserTestFixture::CompareArrayContents(
    const PXR_NS::VtArray<ElementType>& vtArray)
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
