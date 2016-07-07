#include "messages.h"

// src/compiler/ac_class.c
const char *msgerr_bad_impl        = "Implementation of method (%s) in class (%s) does not match interface";
const char *msgerr_incomplete_impl = "Class (%s) does not fully implement all announced interfaces";

// src/compiler/ac_constants.c
const char *msgerr_unknown_value_type             = "Unknown value type.";
const char *msgerr_invalid_constant_struct_member = "Invalid type for constant struct member";

// src/compiler/ac_control_flow.c
const char *msgerr_fallthrough_outside_switch = "Attempting a fallthrough outside of switch statement";
const char *msgerr_yield_no_expression        = "Yield statement must have an associated expression.";
const char *msgerr_nonconstant_case_label     = "Case branch not constant";
const char *msgerr_duplicate_case_label       = "Duplicate case label";
const char *msgerr_invalid_switch_type        = "Cannot switch over given type";
const char *msgerr_range_not_gen              = "Range-based for-loops only work with generators.";
const char *msgerr_mistmatch_gen_iter         = "Generator type and iterator do not match.";
const char *msgerr_invalid_test_type          = "Cannot test against given type.";

// src/compiler/ac_functions.c
const char *msgerr_function_return_required = "Function must return a value.";

// src/compiler/ac_general.c
const char *msgerr_invalid_conversion_constant     = "Invalid implicit conversion in constant initializer";
const char *msgerr_invalid_static_type             = "Variable %s does not have a valid static type";
const char *msgerr_unsupported_struct_byval        = "Passing struct by value not supported.";
const char *msgerr_unsupported_struct_return_byval = "Returning struct by value not supported. (%s)\n";
const char *msgerr_invalid_constant_type           = "Invalid constant type.";
const char *msgerr_internal_ast_resultantType      = "Internal Error. AST Resultant Type for expression not set.";
const char *msgerr_invalid_expression_type         = "Invalid expression type.";
const char *msgerr_invalid_statement_type          = "Invalid statement type.";
const char *msgerr_invalid_declaration_type        = "Invalid declaration type.";
const char *msgerr_invalid_statement_in_defer      = "Invalid statement in deferment";

// src/compiler/ac_generator.c

// src/compiler/ac_generics.c
const char *msgerr_generic_references_non_exported    = "Exported generic (%s) references non-exported symbol (%s)";
const char *msgerr_generic_type_disagreement          = "Two different concrete types assigned to same generic type (%s)";
const char *msgerr_generic_pointer_disagreement       = "Pointer types do not match in generic";
const char *msgerr_generic_param_count_disagreement   = "Parameter counts do not match in generic function pointer";
const char *msgerr_generic_function_type_disagreement = "Function types do not match in generic function pointer";
const char *msgerr_generic_given_mismatch             = "Generic and given type do not match";
const char *msgerr_generic_inference_impossible       = "Cannot infer generic type (%s)";
const char *msgerr_internal_no_generic_bundle         = "Internal compiler error: could not find generic bundle";
const char *msgerr_generic_unspecified_return         = "Return type of generic function is unspecified";

// src/compiler/ac_expressions.c
const char *msgerr_item_not_in_enum                  = "Item %s not associated with enum %s.";
const char *msgerr_unknown_var_read                  = "Trying to read variable of unknown type (%s)";
const char *msgerr_redeclaration                     = "Redeclaration of variable %s";
const char *msgerr_redefinition                      = "Redefinition of variable %s";
const char *msgerr_invalid_global_type               = "Cannot declare global variable of the given type";
const char *msgerr_member_access_non_struct          = "Attempting to access member of non-struct type (%s).";
const char *msgerr_member_access_non_struct_ptr      = "Attempting to access member of non-struct pointer type (%s).";
const char *msgerr_internal_method_resolution        = "Internal compiler error: unable to resolve method";
const char *msgerr_internal_struct_load_find         = "Internal compiler error. Struct not loaded but found.";
const char *msgerr_member_not_in_struct              = "Struct (%s) has no member (%s).";
const char *msgerr_invalid_type_lookup_type          = "Trying to lookup item of non-enum and non-class type %s.";
const char *msgerr_invalid_struct_lit_assignment     = "Attempting to initialize non-struct pointer type with struct literal";
const char *msgerr_non_int_ptr_cast                  = "Cannot cast non-integer type to pointer.";
const char *msgerr_ptr_cast                          = "Pointers may only be cast to other pointers or integers.";
const char *msgerr_unknown_conversion                = "Unknown type conversion requested.";
const char *msgerr_invalid_indexed_type              = "Only pointer types may be indexed.";
const char *msgerr_deref_any_ptr                     = "Trying to dereference any-pointer.";
const char *msgerr_invalid_indexer_type              = "Arrays can only be indexed by a number.";
const char *msgerr_internal                          = "Internal compiler error";
const char *msgerr_op_invalid_for_ptr                = "Operation '%c' not valid for pointer types.";
const char *msgerr_invalid_ptr_arithmetic            = "Pointer arithmetic is only valid with integer and non-any pointer types.";
const char *msgerr_ptr_arithmetic_deref_any          = "Pointer arithmetic results in dereferencing any pointer.";
const char *msgerr_invalid_binary_op                 = "Invalid binary operation (%c).";
const char *msgerr_undeclared_identifier             = "Undeclared identifier (%s)";
const char *msgerr_addr_invalid_expression           = "Address may not be taken of this expression.";
const char *msgerr_invalid_puts_type                 = "The requested type may not be printed.";
const char *msgerr_invalid_deref_type                = "Only pointers may be dereferenced.";
const char *msgerr_deref_any_no_cast                 = "Any pointers may not be dereferenced without cast.";
const char *msgerr_invalid_countof_type              = "countof operator only valid for arrays.";
const char *msgerr_invalid___inc_type                = "Only pointers in the counted regime may be manipulated using __inc";
const char *msgerr_invalid___dec_type                = "Only pointers in the counted regime may be manipulated using __dec";
const char *msgerr_invalid_unwrap_type               = "Only pointers in the counted regime may be unwrapped.";
const char *msgerr_invalid_negate_type               = "Trying to negate non-numeric type";
const char *msgerr_invalid_unary_op                  = "Invalid unary operator (%c).";
const char *msgerr_mismatch_param_count              = "Function takes %d parameters, but %d provided";
const char *msgerr_mismatch_param_count_variadic     = "Variadic function takes at least %d parameters, but %d provided";
const char *msgerr_global_init_non_constant          = "Attempting to initialize global variable with non-constant value";
const char *msgerr_invalid_assignment                = "Left hand side may not be assigned to.";
const char *msgerr_struct_literal_type_unknown       = "Unable to infer type of struct literal from context.";
const char *msgerr_invalid_struct_lit_assignment_val = "Attempting to assign structure literal to non-struct object";
const char *msgerr_internal_storageBundle            = "Internal compiler error!\nstorageBundle = %p; storageIdent = %p;";

// src/compiler/ac_helpers.c
const char *msgerr_invalid_ptr_conversion               = "Non-pointer type may not be converted to pointer type.";
const char *msgerr_invalid_counted_uncounted_conversion = "Counted pointer type may not be converted to counted pointer type. Use the `unwrap` keyword.";
const char *msgerr_mismatched_ptr_depth                 = "Implicit pointer conversion invalid. Cannot conver pointer of depth %d to depth %d.";
const char *msgerr_interface_not_impl_by_class          = "Class does not implement the requested interface";
const char *msgerr_invalid_implicit_ptr_conversion      = "Implicit pointer conversion invalid; pointer types are incompatible.";
const char *msgerr_invalid_array_conversion             = "Arrays may only be converted to equivalent pointers.";
const char *msgerr_invalid_implicit_conversion          = "Invalid implicit conversion.";
const char *msgerr_invalid_sum_types                    = "The given types may not be summed.";
const char *msgerr_invalid_sub_types                    = "The given types may not be subtracted.";
const char *msgerr_invalid_mul_types                    = "The given types may not be multiplied.";
const char *msgerr_invalid_div_types                    = "The given types may not be divided.";
const char *msgerr_invalid_mod_types                    = "The given types may not have modulo applied.";
const char *msgerr_invalid_OR_types                     = "Bitwise OR does not apply to floating point types";
const char *msgerr_invalid_AND_types                    = "Bitwise AND does not apply to floating point types";
const char *msgerr_invalid_XOR_types                    = "Bitwise XOR does not apply to floating point types";
const char *msgerr_invalid_shift_types                  = "Bitwise shift does not apply to floating point types";
const char *msgerr_invalid_neg_type                     = "The given type may not have negation applied (%d).";
const char *msgerr_invalid_NOT_type                     = "Bitwise NOT does not apply to floating point types";
const char *msgerr_invalid_comparison_types             = "The given types may not be compared.";

// src/compiler/ac_rc.c

// src/compiler/ac_struct.c
const char *msgerr_assign_struct_lit_member = "Attempting to assign struct literal to non-struct member %s";
const char *msgwarn_struct_redeclaration    = "Attempting to redeclare struct %s";

// src/compiler/ast.c
const char *msgerr_class_decl_no_parens            = "Missing parentheses after new declaration.";
const char *msgerr_unexpected_identifier           = "Unexpected identifier: %s";
const char *msgerr_non_interface_type_in_impl_list = "Non-interface type declared in class signature (%s)";
const char *msgerr_bad_composite_type_decl         = "Cannot implement composite interface; implement each interface separately (with a comma) (%s)";
const char *msgerr_destructor_has_params           = "Custom destructors can't accept parameters";
const char *msgerr_void_ptr_decl                   = "Cannot declare void pointer type. Use an any-pointer instead (any *).";
const char *msgerr_invalid_counted_type            = "Only pointer types may be counted.";
const char *msgerr_invalid_weak_type               = "Only pointer types may be declared weak.";
const char *msgerr_invalid_weak_ptr_type           = "Only counted pointers may be declared weak.";
const char *msgerr_composite_from_non_interfaces   = "Attempting to make composite type from non-interface componenents";
const char *msgerr_duplicate_default_case          = "Switch statement has multiple default cases";

// src/compiler/ast_walk.c

// src/compiler/variable_manager.c
const char *msgerr_internal_deferment_callback_missing = "Internal compiler error: deferment callback not set";
const char *msgwarn_unused_variable                    = "Unused variable %s";
const char *msgwarn_assigned_not_used                  = "Variable %s assigned but never used";

// src/core/arguments.c
const char *msgerr_arg_code_missing        = "--code switch specified but no extra code defined.";
const char *msgerr_arg_threads_missing     = "Argument expects operand but non provided (%s)";
const char *msgerr_arg_skip_missing        = "Argument expects operand but non provided (%s)";
const char *msgwarn_invalid_thread_count   = "Invalid thread count";
const char *msgwarn_ignoring_unknown_param = "Ignoring unknown parameter (%s)";

// src/core/main.c
const char *msgerr_no_code_specified = "No valid operands provided.";

// src/core/shipping.c
const char *msgerr_internal_execvp_returned = "Internal compiler error: execvp() should not return!";
const char *msgerr_internal_fork_failed     = "Internal compiler error: failed to fork process";

// src/core/types.c
const char *msgerr_internal_no_enum            = "Internal compiler error: no such enum %s";
const char *msgerr_class_redeclaration         = "Redeclaring class with name: %s";
const char *msgerr_interface_redeclaration     = "Redeclaring interface with name: %s";
const char *msgerr_interface_class_name_clash  = "Interface declaration %s clashes with previously named class.";
const char *msgerr_enum_redeclaration          = "Redeclaring enum with name: %s";
const char *msgerr_internal_enum_lookup_failed = "Internal compiler error could not find enum item %s.%s";
const char *msgerr_internal_enum_decl_failed   = "Internal compiler error declaring enum item %s.%s";
const char *msgerr_unknown_generic             = "Unknown generic type: %s";

// src/core/utils.c
const char *msgerr_unexpected_escaped_string_end = "Unexpected end of escaped string: '%s'";
const char *msgerr_unexpected_escaped_char       = "Unknown escape character in string: '%s'";

// src/environment/imports.c
const char *msgerr_improperly_defined_name  = "%s name not properly defined";
const char *msgerr_improperly_defined_class = "class name not properly defined";
const char *msgerr_unknown_top_level_symbol = "Unknown top-level symbol";
const char *msgerr_unknown_code_file        = "Unknown code file: %s";
const char *msgerr_import_does_not_exist    = "Imported file (%s) does not exist";

// src/grammar/eagle.l
const char *msgerr_proper_formatting_tab = "Proper formatting violation: Tab character as indentation";

// src/grammar/eagle.y
const char *msgerr_proper_formatting_brace_same_line = "Proper formatting violation: Opening brace on same line as declaration";

