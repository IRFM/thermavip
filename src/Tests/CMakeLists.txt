# Test tree for the SDK.
#
# These are CHARACTERISATION tests: they capture current behaviour, including
# its flaws, on the surfaces the fix branches will touch. Their purpose is to
# catch a regression, not to validate a specification.
#
# A test failing because of a real, pre-existing defect must not be neutralised.
# Mark it QEXPECT_FAIL with the reason; QTest then reports XPASS and fails the
# test once the defect is fixed, forcing the marker to be removed.

add_subdirectory(Logging)
add_subdirectory(DataType)
add_subdirectory(Core)
add_subdirectory(Gui)
add_subdirectory(Annotation)
add_subdirectory(Python)
