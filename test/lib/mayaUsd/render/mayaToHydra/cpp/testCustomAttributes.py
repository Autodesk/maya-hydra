# Copyright 2025 Autodesk
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
import maya.cmds as cmds
import maya.api.OpenMaya as om
import fixturesUtils
import mtohUtils
import mayaUtils


# Resolve a dependency node by name.
def _get_dep_node(name):
    selection = om.MSelectionList()
    selection.add(name)
    return selection.getDependNode(0)


# Check whether a plug exists on a node.
def _plug_exists(node_fn, name):
    try:
        node_fn.findPlug(name, False)
        return True
    except Exception:
        return False


# Create the enum test attribute on the node.
def _create_enum_attr(node_fn):
    if _plug_exists(node_fn, "extEnum"):
        return
    enum_attr = om.MFnEnumAttribute()
    enum_obj = enum_attr.create("extEnum", "extEnum", 0)
    enum_attr.addField("zero", 0)
    enum_attr.addField("one", 1)
    enum_attr.addField("two", 2)
    enum_attr.keyable = True
    enum_attr.storable = True
    node_fn.addAttribute(enum_obj)


# Create a typed attribute for test data.
def _create_typed_attr(node_fn, name, data_type, allow_fail=False):
    if _plug_exists(node_fn, name):
        return True
    try:
        typed_attr = om.MFnTypedAttribute()
        typed_obj = typed_attr.create(name, name, data_type)
        typed_attr.keyable = True
        typed_attr.storable = True
        node_fn.addAttribute(typed_obj)
        return True
    except Exception:
        if allow_fail:
            return False
        raise


# Create a numeric attribute for test data.
def _create_numeric_attr(node_fn, name, numeric_type, default_value=None):
    if _plug_exists(node_fn, name):
        return
    numeric_attr = om.MFnNumericAttribute()
    if numeric_type == om.MFnNumericData.kAddr:
        if default_value is None:
            attr_obj = numeric_attr.createAddr(name, name)
        else:
            attr_obj = numeric_attr.createAddr(name, name, default_value)
    elif numeric_type == om.MFnNumericData.k4Double:
        child_attr = om.MFnNumericAttribute()
        child1 = child_attr.create(
            name + "X", name + "X", om.MFnNumericData.kDouble, 0.0)
        child2 = child_attr.create(
            name + "Y", name + "Y", om.MFnNumericData.kDouble, 0.0)
        child3 = child_attr.create(
            name + "Z", name + "Z", om.MFnNumericData.kDouble, 0.0)
        child4 = child_attr.create(
            name + "W", name + "W", om.MFnNumericData.kDouble, 0.0)
        attr_obj = numeric_attr.create(name, name, child1, child2, child3, child4)
    else:
        if default_value is None:
            # Use integer-like defaults for boolean/integer types,
            # and floating defaults for float/double types.
            if numeric_type in (
                om.MFnNumericData.kBoolean,
                om.MFnNumericData.kByte,
                om.MFnNumericData.kShort,
                om.MFnNumericData.kInt,
                om.MFnNumericData.kInt64,
            ):
                default_val = 0
            else:
                default_val = 0.0
        else:
            default_val = default_value
        attr_obj = numeric_attr.create(name, name, numeric_type, default_val)
    numeric_attr.keyable = True
    numeric_attr.storable = True
    node_fn.addAttribute(attr_obj)


# Create a unit attribute (angle/distance/time).
def _create_unit_attr(node_fn, name, unit_type):
    if _plug_exists(node_fn, name):
        return
    unit_attr = om.MFnUnitAttribute()
    attr_obj = unit_attr.create(name, name, unit_type, 0.0)
    unit_attr.keyable = True
    unit_attr.storable = True
    node_fn.addAttribute(attr_obj)


# Create a matrix attribute (MFnMatrixAttribute).
def _create_matrix_attr(node_fn, name):
    if _plug_exists(node_fn, name):
        return
    matrix_attr = om.MFnMatrixAttribute()
    attr_obj = matrix_attr.create(name, name, om.MFnMatrixAttribute.kDouble)
    matrix_attr.keyable = True
    matrix_attr.storable = True
    node_fn.addAttribute(attr_obj)


# Create all custom attributes needed for the tests.
def _create_custom_attributes(shape_name):
    node_obj = _get_dep_node(shape_name)
    node_fn = om.MFnDependencyNode(node_obj)

    _create_enum_attr(node_fn)

    _create_typed_attr(node_fn, "extString", om.MFnData.kString)
    _create_typed_attr(node_fn, "extStringArray", om.MFnData.kStringArray)
    _create_typed_attr(node_fn, "extIntArray", om.MFnData.kIntArray)
    _create_typed_attr(node_fn, "extFloatArray", om.MFnData.kFloatArray)
    _create_typed_attr(node_fn, "extDoubleArray", om.MFnData.kDoubleArray)
    _create_typed_attr(node_fn, "extVectorArray", om.MFnData.kVectorArray)
    _create_typed_attr(node_fn, "extPointArray", om.MFnData.kPointArray)
    _create_typed_attr(node_fn, "extMatrix", om.MFnData.kMatrix)
    _create_typed_attr(node_fn, "extMatrixArray", om.MFnData.kMatrixArray)

    typed_numeric_ok = _create_typed_attr(
        node_fn, "extNumeric2Float", om.MFnData.kNumeric, allow_fail=True)

    _create_numeric_attr(node_fn, "extBool", om.MFnNumericData.kBoolean)
    _create_numeric_attr(node_fn, "extByte", om.MFnNumericData.kByte)
    _create_numeric_attr(node_fn, "extShort", om.MFnNumericData.kShort)
    _create_numeric_attr(node_fn, "extInt", om.MFnNumericData.kInt)
    _create_numeric_attr(node_fn, "extFloat", om.MFnNumericData.kFloat)
    _create_numeric_attr(node_fn, "extDouble", om.MFnNumericData.kDouble)
    _create_numeric_attr(node_fn, "extInt64", om.MFnNumericData.kInt64)
    _create_numeric_attr(node_fn, "extAddr", om.MFnNumericData.kAddr, 0x1234)

    _create_numeric_attr(node_fn, "extShort2", om.MFnNumericData.k2Short)
    _create_numeric_attr(node_fn, "extShort3", om.MFnNumericData.k3Short)
    _create_numeric_attr(node_fn, "extInt2", om.MFnNumericData.k2Int)
    _create_numeric_attr(node_fn, "extInt3", om.MFnNumericData.k3Int)
    _create_numeric_attr(node_fn, "extFloat2", om.MFnNumericData.k2Float)
    _create_numeric_attr(node_fn, "extFloat3", om.MFnNumericData.k3Float)
    _create_numeric_attr(node_fn, "extDouble2", om.MFnNumericData.k2Double)
    _create_numeric_attr(node_fn, "extDouble3", om.MFnNumericData.k3Double)
    _create_numeric_attr(node_fn, "extDouble4", om.MFnNumericData.k4Double)

    _create_unit_attr(node_fn, "extAngle", om.MFnUnitAttribute.kAngle)
    _create_unit_attr(node_fn, "extDistance", om.MFnUnitAttribute.kDistance)
    _create_unit_attr(node_fn, "extTime", om.MFnUnitAttribute.kTime)

    _create_matrix_attr(node_fn, "extMatrixAttr")

    return typed_numeric_ok

# Author custom attribute values and verify Maya readback.
def _set_custom_attribute_values(shape_name):
    node_obj = _get_dep_node(shape_name)
    node_fn = om.MFnDependencyNode(node_obj)

    # Shortcut to find a plug on the node.
    def _plug(name):
        return node_fn.findPlug(name, False)

    # Exact equality assertion with label.
    def _assert_equal(actual, expected, label):
        if actual != expected:
            raise RuntimeError(
                "Set {} failed: {} != {}".format(label, actual, expected))

    # Floating-point equality with tolerance.
    def _assert_almost_equal(actual, expected, label, tol=1e-6):
        if abs(actual - expected) > tol:
            raise RuntimeError(
                "Set {} failed: {} != {} (tol={})".format(label, actual, expected, tol))

    # Sequence equality with tolerance per element.
    def _assert_seq_almost_equal(actual, expected, label, tol=1e-6):
        if len(actual) != len(expected):
            raise RuntimeError(
                "Set {} failed: len {} != {} ({!r} vs {!r})".format(
                    label, len(actual), len(expected), actual, expected))
        for idx, (act, exp) in enumerate(zip(actual, expected)):
            if abs(act - exp) > tol:
                raise RuntimeError(
                    "Set {}[{}] failed: {} != {} (tol={})".format(
                        label, idx, act, exp, tol))

    # Nested sequence equality with tolerance per element.
    def _assert_tuple_seq_almost_equal(actual, expected, label, tol=1e-6):
        if len(actual) != len(expected):
            raise RuntimeError(
                "Set {} failed: len {} != {} ({!r} vs {!r})".format(
                    label, len(actual), len(expected), actual, expected))
        for idx, (act, exp) in enumerate(zip(actual, expected)):
            if len(act) != len(exp):
                raise RuntimeError(
                    "Set {}[{}] failed: len {} != {} ({!r} vs {!r})".format(
                        label, idx, len(act), len(exp), act, exp))
            for jdx, (act_val, exp_val) in enumerate(zip(act, exp)):
                if abs(act_val - exp_val) > tol:
                    raise RuntimeError(
                        "Set {}[{}][{}] failed: {} != {} (tol={})".format(
                            label, idx, jdx, act_val, exp_val, tol))

    # Normalize cmds.getAttr output into a flat list.
    def _flatten_cmds_array(value):
        if isinstance(value, (list, tuple)) and len(value) == 1 and isinstance(value[0], (list, tuple)):
            return list(value[0])
        if isinstance(value, (list, tuple)):
            return list(value)
        return [value]

    # Read an attribute array value via cmds.getAttr.
    def _get_cmds_array(attr):
        data = cmds.getAttr(attr)
        flattened = _flatten_cmds_array(data)
        if flattened and isinstance(flattened[0], (list, tuple)):
            return [tuple(item) for item in flattened]
        return flattened

    # Read a matrix array value via API with cmds fallback.
    def _get_matrix_array_values(plug):
        try:
            data_obj = plug.asMObject()
            if not data_obj.isNull():
                mat_array = om.MFnMatrixArrayData(data_obj).array()
            else:
                mat_array = []
        except RuntimeError:
            mat_array = []
        if mat_array:
            return [_matrix_to_list(item) for item in mat_array]
        data = cmds.getAttr(plug.name())
        flattened = _flatten_cmds_array(data)
        if flattened and isinstance(flattened[0], (list, tuple)):
            return [list(item) for item in flattened]
        return [list(flattened)]

    # Convert an MMatrix or nested list into a flat list of 16 values.
    def _matrix_to_list(m):
        try:
            length = len(m)
        except Exception:
            raise RuntimeError("Set matrix failed: unsupported type {}".format(type(m)))

        if length == 16:
            flat = list(m)
            if flat and isinstance(flat[0], (list, tuple)):
                return [flat[i][j] for i in range(4) for j in range(4)]
            return [float(x) for x in flat]

        if length == 4 and isinstance(m[0], (list, tuple)):
            return [m[i][j] for i in range(4) for j in range(4)]

        try:
            return [m[i][j] for i in range(4) for j in range(4)]
        except Exception:
            raise RuntimeError("Set matrix failed: unsupported shape {}".format(type(m)))

    # -----------------
    # Enum + string
    # -----------------
    _plug("extEnum").setInt(2)
    _assert_equal(_plug("extEnum").asShort(), 2, "extEnum")
    _plug("extString").setString("hello")
    _assert_equal(_plug("extString").asString(), "hello", "extString")

    # -----------------
    # Typed data attrs (created via MFnData.*)
    # Set them via OpenMaya MFn*Data (most correct + version-stable)
    # -----------------
    str_arr_obj = om.MFnStringArrayData().create(["alpha", "beta"])
    _plug("extStringArray").setMObject(str_arr_obj)
    str_back = om.MFnStringArrayData(_plug("extStringArray").asMObject()).array()
    _assert_equal([str_back[i] for i in range(len(str_back))], ["alpha", "beta"], "extStringArray")

    int_arr_obj = om.MFnIntArrayData().create([1, 2, 3])
    _plug("extIntArray").setMObject(int_arr_obj)
    int_back = om.MFnIntArrayData(_plug("extIntArray").asMObject()).array()
    _assert_equal([int_back[i] for i in range(len(int_back))], [1, 2, 3], "extIntArray")

    cmds.setAttr(
        "{}.extFloatArray".format(shape_name),
        [1.25, 2.5],
        type="floatArray")
    _assert_seq_almost_equal(
        _get_cmds_array("{}.extFloatArray".format(shape_name)),
        [1.25, 2.5],
        "extFloatArray")

    cmds.setAttr(
        "{}.extDoubleArray".format(shape_name),
        [3.5, 4.25],
        type="doubleArray")
    _assert_seq_almost_equal(
        _get_cmds_array("{}.extDoubleArray".format(shape_name)),
        [3.5, 4.25],
        "extDoubleArray")

    cmds.setAttr(
        "{}.extVectorArray".format(shape_name),
        2,
        (1.0, 2.0, 3.0),
        (4.0, 5.0, 6.0),
        type="vectorArray")
    _assert_tuple_seq_almost_equal(
        _get_cmds_array("{}.extVectorArray".format(shape_name)),
        [(1.0, 2.0, 3.0), (4.0, 5.0, 6.0)],
        "extVectorArray")

    cmds.setAttr(
        "{}.extPointArray".format(shape_name),
        2,
        (1.0, 2.0, 3.0, 1.0),
        (4.0, 5.0, 6.0, 1.0),
        type="pointArray")
    _assert_tuple_seq_almost_equal(
        _get_cmds_array("{}.extPointArray".format(shape_name)),
        [(1.0, 2.0, 3.0, 1.0), (4.0, 5.0, 6.0, 1.0)],
        "extPointArray")

    m = om.MMatrix([
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 2.0, 0.0, 0.0],
        [0.0, 0.0, 3.0, 0.0],
        [4.0, 5.0, 6.0, 1.0],
    ])
    mat_obj = om.MFnMatrixData().create(m)
    _plug("extMatrix").setMObject(mat_obj)
    m_back = om.MFnMatrixData(_plug("extMatrix").asMObject()).matrix()
    _assert_seq_almost_equal(_matrix_to_list(m_back), _matrix_to_list(m), "extMatrix")

    m1 = om.MMatrix([
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ])
    m2 = om.MMatrix([
        [2.0, 0.0, 0.0, 0.0],
        [0.0, 2.0, 0.0, 0.0],
        [0.0, 0.0, 2.0, 0.0],
        [7.0, 8.0, 9.0, 1.0],
    ])
    matrix1 = _matrix_to_list(m1)
    matrix2 = _matrix_to_list(m2)
    mat_array = om.MMatrixArray([m1, m2])
    mat_array_obj = om.MFnMatrixArrayData().create(mat_array)
    _plug("extMatrixArray").setMObject(mat_array_obj)
    mat_back = _get_matrix_array_values(_plug("extMatrixArray"))
    if len(mat_back) < 2:
        raise RuntimeError(
            "Set extMatrixArray failed: expected 2 matrices, got {}".format(mat_back))
    _assert_seq_almost_equal(mat_back[0], matrix1, "extMatrixArray[0]")
    _assert_seq_almost_equal(mat_back[1], matrix2, "extMatrixArray[1]")

    # -----------------
    # Numeric attrs (created via MFnNumericData.*)
    # Use MPlug numeric setters (including Int64 + Addr)
    # -----------------
    _plug("extBool").setBool(True)
    _assert_equal(_plug("extBool").asBool(), True, "extBool")
    _plug("extByte").setInt(7)
    _assert_equal(_plug("extByte").asInt(), 7, "extByte")
    _plug("extShort").setInt(12)
    _assert_equal(_plug("extShort").asInt(), 12, "extShort")
    _plug("extInt").setInt(42)
    _assert_equal(_plug("extInt").asInt(), 42, "extInt")
    _plug("extFloat").setFloat(1.75)
    _assert_almost_equal(_plug("extFloat").asFloat(), 1.75, "extFloat")
    _plug("extDouble").setDouble(2.25)
    _assert_almost_equal(_plug("extDouble").asDouble(), 2.25, "extDouble")

    try:
        _plug("extInt64").setInt64(9876543210)
    except AttributeError:
        cmds.setAttr("{}.extInt64".format(shape_name), 9876543210)
    _assert_equal(
        cmds.getAttr("{}.extInt64".format(shape_name)),
        9876543210,
        "extInt64")
    try:
        addr_val = _plug("extAddr").asMDataHandle().asAddr()
    except AttributeError:
        addr_val = cmds.getAttr("{}.extAddr".format(shape_name))
    _assert_equal(addr_val, 0x1234, "extAddr")

    # -----------------
    # 2/3/4 component numerics (created via MFnNumericData.k2*/k3*/k4Double)
    # -----------------
    cmds.setAttr(f"{shape_name}.extShort2", 3, 4)
    cmds.setAttr(f"{shape_name}.extShort3", 5, 6, 7)
    _assert_seq_almost_equal(_get_cmds_array(f"{shape_name}.extShort2"), [3, 4], "extShort2")
    _assert_seq_almost_equal(_get_cmds_array(f"{shape_name}.extShort3"), [5, 6, 7], "extShort3")

    cmds.setAttr(f"{shape_name}.extInt2", 8, 9)
    cmds.setAttr(f"{shape_name}.extInt3", 10, 11, 12)
    _assert_seq_almost_equal(_get_cmds_array(f"{shape_name}.extInt2"), [8, 9], "extInt2")
    _assert_seq_almost_equal(_get_cmds_array(f"{shape_name}.extInt3"), [10, 11, 12], "extInt3")

    cmds.setAttr(f"{shape_name}.extFloat2", 1.1, 2.2)
    cmds.setAttr(f"{shape_name}.extFloat3", 3.3, 4.4, 5.5)
    _assert_seq_almost_equal(_get_cmds_array(f"{shape_name}.extFloat2"), [1.1, 2.2], "extFloat2")
    _assert_seq_almost_equal(_get_cmds_array(f"{shape_name}.extFloat3"), [3.3, 4.4, 5.5], "extFloat3")

    cmds.setAttr(f"{shape_name}.extDouble2", 6.6, 7.7)
    cmds.setAttr(f"{shape_name}.extDouble3", 8.8, 9.9, 10.1)
    cmds.setAttr(f"{shape_name}.extDouble4", 11.1, 12.2, 13.3, 14.4)
    _assert_seq_almost_equal(_get_cmds_array(f"{shape_name}.extDouble2"), [6.6, 7.7], "extDouble2")
    _assert_seq_almost_equal(_get_cmds_array(f"{shape_name}.extDouble3"), [8.8, 9.9, 10.1], "extDouble3")
    _assert_seq_almost_equal(_get_cmds_array(f"{shape_name}.extDouble4"), [11.1, 12.2, 13.3, 14.4], "extDouble4")

    # -----------------
    # Unit attrs (created via MFnUnitAttribute.*)
    # -----------------
    _plug("extAngle").setMAngle(om.MAngle(0.75, om.MAngle.kRadians))
    _plug("extDistance").setMDistance(om.MDistance(2.5, om.MDistance.kCentimeters))
    _plug("extTime").setMTime(om.MTime(1.25, om.MTime.kSeconds))
    angle_val = _plug("extAngle").asMAngle()
    _assert_almost_equal(angle_val.asRadians(), 0.75, "extAngle")
    dist_val = _plug("extDistance").asMDistance()
    _assert_almost_equal(dist_val.asCentimeters(), 2.5, "extDistance")
    time_val = _plug("extTime").asMTime()
    _assert_almost_equal(time_val.asUnits(om.MTime.kSeconds), 1.25, "extTime")

    # -----------------
    # Matrix attribute (created via your _create_matrix_attr; likely numeric/typed "matrix")
    # If it's an actual MFnMatrixAttribute (not typed kMatrix), cmds.setAttr(type="matrix") is correct.
    # -----------------
    matrix_attr_values = (
        9.0, 0.0, 0.0, 0.0,
        0.0, 8.0, 0.0, 0.0,
        0.0, 0.0, 7.0, 0.0,
        2.0, 3.0, 4.0, 1.0,
    )
    cmds.setAttr(
        "{}.extMatrixAttr".format(shape_name),
        *matrix_attr_values,
        type="matrix")
    _assert_seq_almost_equal(
        _get_cmds_array("{}.extMatrixAttr".format(shape_name)),
        list(matrix_attr_values),
        "extMatrixAttr")

class TestCustomAttributes(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _requiredPlugins = ['mtoa']

    # Ensure a clean, unmodified Maya scene for each test.
    def setUp(self):
        mayaUtils.openNewScene()
        modified = cmds.file(query=True, modified=True)
        self.assertFalse(
            modified,
            'Internal test framework error: scene left as modified by mayaUtils.openNewScene()')
        cmds.file(modified=False)

    # Create scene geometry and custom attributes for tests.
    def setupScene(self):
        cmds.polyCube()
        shape_name = "pCubeShape1"
        self._has_typed_numeric = _create_custom_attributes(shape_name)
        _set_custom_attribute_values(shape_name)
        self.setHdStormRenderer()
        cmds.refresh()

    # Run the C++ test that validates Arnold defaults.
    def test_defaultArnoldCustomAttributes(self):
        self.setupScene()
        self.runCppTest("CustomAttributes.defaultArnoldCustomAttributes")

    # Run the C++ tests that validate extension attribute types.
    def test_extensionAttributeTypes(self):
        self.setupScene()
        self.runCppTest("CustomAttributes.extensionAttributeTypes")
        if self._has_typed_numeric:
            self.runCppTest("CustomAttributes.extensionAttributeTypedNumeric")

    # Run the C++ test that validates enum label primvars.
    def test_extensionAttributeEnumLabels(self):
        self.setupScene()
        self.runCppTest("CustomAttributes.extensionAttributeEnumLabels")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
