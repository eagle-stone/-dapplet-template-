/*
    pybind11/pybind11.h: Main header file of the C++11 python
    binding generator library

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4100) // warning C4100: Unreferenced formal parameter
#  pragma warning(disable: 4127) // warning C4127: Conditional expression is constant
#  pragma warning(disable: 4512) // warning C4512: Assignment operator was implicitly defined as deleted
#  pragma warning(disable: 4800) // warning C4800: 'int': forcing value to bool 'true' or 'false' (performance warning)
#  pragma warning(disable: 4996) // warning C4996: The POSIX name for this item is deprecated. Instead, use the ISO C and C++ conformant name
#  pragma warning(disable: 4702) // warning C4702: unreachable code
#  pragma warning(disable: 4522) // warning C4522: multiple assignment operators specified
#elif defined(__INTEL_COMPILER)
#  pragma warning(push)
#  pragma warning(disable: 68)    // integer conversion resulted in a change of sign
#  pragma warning(disable: 186)   // pointless comparison of unsigned integer with zero
#  pragma warning(disable: 878)   // incompatible exception specifications
#  pragma warning(disable: 1334)  // the "template" keyword used for syntactic disambiguation may only be used within a template
#  pragma warning(disable: 1682)  // implicit conversion of a 64-bit integral type to a smaller integral type (potential portability problem)
#  pragma warning(disable: 1875)  // offsetof applied to non-POD (Plain Old Data) types is nonstandard
#  pragma warning(disable: 2196)  // warning #2196: routine is both "inline" and "noinline"
#elif defined(__GNUG__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#  pragma GCC diagnostic ignored "-Wstrict-aliasing"
#  pragma GCC diagnostic ignored "-Wattributes"
#  if __GNUC__ >= 7
#    pragma GCC diagnostic ignored "-Wnoexcept-type"
#  endif
#endif

#include "attr.h"
#include "options.h"
#include "class_support.h"

NAMESPACE_BEGIN(pybind11)

/// Wraps an arbitrary C++ function/method/lambda function/.. into a callable Python object
class cpp_function : public function {
public:
    cpp_function() { }

    /// Construct a cpp_function from a vanilla function pointer
    template <typename Return, typename... Args, typename... Extra>
    cpp_function(Return (*f)(Args...), const Extra&... extra) {
        initialize(f, f, extra...);
    }

    /// Construct a cpp_function from a lambda function (possibly with internal state)
    template <typename Func, typename... Extra, typename = detail::enable_if_t<
        detail::satisfies_none_of<
            detail::remove_reference_t<Func>,
            std::is_function, std::is_pointer, std::is_member_pointer
        >::value>
    >
    cpp_function(Func &&f, const Extra&... extra) {
        using FuncType = typename detail::remove_class<decltype(&detail::remove_reference_t<Func>::operator())>::type;
        initialize(std::forward<Func>(f),
                   (FuncType *) nullptr, extra...);
    }

    /// Construct a cpp_function from a class method (non-const)
    template <typename Return, typename Class, typename... Arg, typename... Extra>
    cpp_function(Return (Class::*f)(Arg...), const Extra&... extra) {
        initialize([f](Class *c, Arg... args) -> Return { return (c->*f)(args...); },
                   (Return (*) (Class *, Arg...)) nullptr, extra...);
    }

    /// Construct a cpp_function from a class method (const)
    template <typename Return, typename Class, typename... Arg, typename... Extra>
    cpp_function(Return (Class::*f)(Arg...) const, const Extra&... extra) {
        initialize([f](const Class *c, Arg... args) -> Return { return (c->*f)(args...); },
                   (Return (*)(const Class *, Arg ...)) nullptr, extra...);
    }

    /// Return the function name
    object name() const { return attr("__name__"); }

protected:
    /// Space optimization: don't inline this frequently instantiated fragment
    PYBIND11_NOINLINE detail::function_record *make_function_record() {
        return new detail::function_record();
    }

    /// Special internal constructor for functors, lambda functions, etc.
    template <typename Func, typename Return, typename... Args, typename... Extra>
    void initialize(Func &&f, Return (*)(Args...), const Extra&... extra) {

        struct capture { detail::remove_reference_t<Func> f; };

        /* Store the function including any extra state it might have (e.g. a lambda capture object) */
        auto rec = make_function_record();

        /* Store the capture object directly in the function record if there is enough space */
        if (sizeof(capture) <= sizeof(rec->data)) {
            /* Without these pragmas, GCC warns that there might not be
               enough space to use the placement new operator. However, the
               'if' statement above ensures that this is the case. */
#if defined(__GNUG__) && !defined(__clang__) && __GNUC__ >= 6
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wplacement-new"
#endif
            new ((capture *) &rec->data) capture { std::forward<Func>(f) };
#if defined(__GNUG__) && !defined(__clang__) && __GNUC__ >= 6
#  pragma GCC diagnostic pop
#endif
            if (!std::is_trivially_destructible<Func>::value)
                rec->free_data = [](detail::function_record *r) { ((capture *) &r->data)->~capture(); };
        } else {
            rec->data[0] = new capture { std::forward<Func>(f) };
            rec->free_data = [](detail::function_record *r) { delete ((capture *) r->data[0]); };
        }

        /* Type casters for the function arguments and return value */
        using cast_in = detail::argument_loader<Args...>;
        using cast_out = detail::make_caster<
            detail::conditional_t<std::is_void<Return>::value, detail::void_type, Return>
        >;

        static_assert(detail::expected_num_args<Extra...>(sizeof...(Args), cast_in::has_args, cast_in::has_kwargs),
                      "The number of argument annotations does not match the number of function arguments");

        /* Dispatch code which converts function arguments and performs the actual function call */
        rec->impl = [](detail::function_call &call) -> handle {
            cast_in args_converter;

            /* Try to cast the function arguments into the C++ domain */
            if (!args_converter.load_args(call))
                return PYBIND11_TRY_NEXT_OVERLOAD;

            /* Invoke call policy pre-call hook */
            detail::process_attributes<Extra...>::precall(call);

            /* Get a pointer to the capture object */
            auto data = (sizeof(capture) <= sizeof(call.func.data)
                         ? &call.func.data : call.func.data[0]);
            capture *cap = const_cast<capture *>(reinterpret_cast<const capture *>(data));

            /* Override policy for rvalues -- usually to enforce rvp::move on an rvalue */
            const auto policy = detail::return_value_policy_override<Return>::policy(call.func.policy);

            /* Function scope guard -- defaults to the compile-to-nothing `void_type` */
            using Guard = detail::extract_guard_t<Extra...>;

            /* Perform the function call */
            handle result = cast_out::cast(
                std::move(args_converter).template call<Return, Guard>(cap->f), policy, call.parent);

            /* Invoke call policy post-call hook */
            detail::process_attributes<Extra...>::postcall(call, result);

            return result;
        };

        /* Process any user-provided function attributes */
        detail::process_attributes<Extra...>::init(extra..., rec);

        /* Generate a readable signature describing the function's arguments and return value types */
        using detail::descr; using detail::_;
        PYBIND11_DESCR signature = _("(") + cast_in::arg_names() + _(") -> ") + cast_out::name();

        /* Register the function with Python from generic (non-templated) code */
        initialize_generic(rec, signature.text(), signature.types(), sizeof...(Args));

        if (cast_in::has_args) rec->has_args = true;
        if (cast_in::has_kwargs) rec->has_kwargs = true;

        /* Stash some additional information used by an important optimization in 'functional.h' */
        using FunctionType = Return (*)(Args...);
        constexpr bool is_function_ptr =
            std::is_convertible<Func, FunctionType>::value &&
            sizeof(capture) == sizeof(void *);
        if (is_function_ptr) {
            rec->is_stateless = true;
            rec->data[1] = const_cast<void *>(reinterpret_cast<const void *>(&typeid(FunctionType)));
        }
    }

    /// Register a function call with Python (generic non-templated code goes here)
    void initialize_generic(detail::function_record *rec, const char *text,
                            const std::type_info *const *types, size_t args) {

        /* Create copies of all referenced C-style strings */
        rec->name = strdup(rec->name ? rec->name : "");
        if (rec->doc) rec->doc = strdup(rec->doc);
        for (auto &a: rec->args) {
            if (a.name)
                a.name = strdup(a.name);
            if (a.descr)
                a.descr = strdup(a.descr);
            else