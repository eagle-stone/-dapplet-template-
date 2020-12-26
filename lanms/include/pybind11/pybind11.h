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
            else if (a.value)
                a.descr = strdup(a.value.attr("__repr__")().cast<std::string>().c_str());
        }

        /* Generate a proper function signature */
        std::string signature;
        size_t type_depth = 0, char_index = 0, type_index = 0, arg_index = 0;
        while (true) {
            char c = text[char_index++];
            if (c == '\0')
                break;

            if (c == '{') {
                // Write arg name for everything except *args, **kwargs and return type.
                if (type_depth == 0 && text[char_index] != '*' && arg_index < args) {
                    if (!rec->args.empty() && rec->args[arg_index].name) {
                        signature += rec->args[arg_index].name;
                    } else if (arg_index == 0 && rec->is_method) {
                        signature += "self";
                    } else {
                        signature += "arg" + std::to_string(arg_index - (rec->is_method ? 1 : 0));
                    }
                    signature += ": ";
                }
                ++type_depth;
            } else if (c == '}') {
                --type_depth;
                if (type_depth == 0) {
                    if (arg_index < rec->args.size() && rec->args[arg_index].descr) {
                        signature += "=";
                        signature += rec->args[arg_index].descr;
                    }
                    arg_index++;
                }
            } else if (c == '%') {
                const std::type_info *t = types[type_index++];
                if (!t)
                    pybind11_fail("Internal error while parsing type signature (1)");
                if (auto tinfo = detail::get_type_info(*t)) {
#if defined(PYPY_VERSION)
                    signature += handle((PyObject *) tinfo->type)
                                     .attr("__module__")
                                     .cast<std::string>() + ".";
#endif
                    signature += tinfo->type->tp_name;
                } else {
                    std::string tname(t->name());
                    detail::clean_type_id(tname);
                    signature += tname;
                }
            } else {
                signature += c;
            }
        }
        if (type_depth != 0 || types[type_index] != nullptr)
            pybind11_fail("Internal error while parsing type signature (2)");

        #if !defined(PYBIND11_CONSTEXPR_DESCR)
            delete[] types;
            delete[] text;
        #endif

#if PY_MAJOR_VERSION < 3
        if (strcmp(rec->name, "__next__") == 0) {
            std::free(rec->name);
            rec->name = strdup("next");
        } else if (strcmp(rec->name, "__bool__") == 0) {
            std::free(rec->name);
            rec->name = strdup("__nonzero__");
        }
#endif
        rec->signature = strdup(signature.c_str());
        rec->args.shrink_to_fit();
        rec->is_constructor = !strcmp(rec->name, "__init__") || !strcmp(rec->name, "__setstate__");
        rec->nargs = (std::uint16_t) args;

        if (rec->sibling && PYBIND11_INSTANCE_METHOD_CHECK(rec->sibling.ptr()))
            rec->sibling = PYBIND11_INSTANCE_METHOD_GET_FUNCTION(rec->sibling.ptr());

        detail::function_record *chain = nullptr, *chain_start = rec;
        if (rec->sibling) {
            if (PyCFunction_Check(rec->sibling.ptr())) {
                auto rec_capsule = reinterpret_borrow<capsule>(PyCFunction_GET_SELF(rec->sibling.ptr()));
                chain = (detail::function_record *) rec_capsule;
                /* Never append a method to an overload chain of a parent class;
                   instead, hide the parent's overloads in this case */
                if (!chain->scope.is(rec->scope))
                    chain = nullptr;
            }
            // Don't trigger for things like the default __init__, which are wrapper_descriptors that we are intentionally replacing
            else if (!rec->sibling.is_none() && rec->name[0] != '_')
                pybind11_fail("Cannot overload existing non-function object \"" + std::string(rec->name) +
                        "\" with a function of the same name");
        }

        if (!chain) {
            /* No existing overload was found, create a new function object */
            rec->def = new PyMethodDef();
            std::memset(rec->def, 0, sizeof(PyMethodDef));
            rec->def->ml_name = rec->name;
            rec->def->ml_meth = reinterpret_cast<PyCFunction>(*dispatcher);
            rec->def->ml_flags = METH_VARARGS | METH_KEYWORDS;

            capsule rec_capsule(rec, [](void *ptr) {
                destruct((detail::function_record *) ptr);
            });

            object scope_module;
            if (rec->scope) {
                if (hasattr(rec->scope, "__module__")) {
                    scope_module = rec->scope.attr("__module__");
                } else if (hasattr(rec->scope, "__name__")) {
                    scope_module = rec->scope.attr("__name__");
                }
            }

            m_ptr = PyCFunction_NewEx(rec->def, rec_capsule.ptr(), scope_module.ptr());
            if (!m_ptr)
                pybind11_fail("cpp_function::cpp_function(): Could not allocate function object");
        } else {
            /* Append at the end of the overload chain */
            m_ptr = rec->sibling.ptr();
            inc_ref();
            chain_start = chain;
            if (chain->is_method != rec->is_method)
                pybind11_fail("overloading a method with both static and instance methods is not supported; "
                    #if defined(NDEBUG)
                        "compile in debug mode for more details"
                    #else
                        "error while attempting to bind " + std::string(rec->is_method ? "instance" : "static") + " method " +
                        std::string(pybind11::str(rec->scope.attr("__name__"))) + "." + std::string(rec->name) + signature
                    #endif
                );
            while (chain->next)
                chain = chain->next;
            chain->next = rec;
        }

        std::string signatures;
        int index = 0;
        /* Create a nice pydoc rec including all signatures and
           docstrings of the functions in the overload chain */
        if (chain && options::show_function_signatures()) {
            // First a generic signature
            signatures += rec->name;
            signatures += "(*args, **kwargs)\n";
            signatures += "Overloaded function.\n\n";
        }
        // Then specific overload signatures
        bool first_user_def = true;
        for (auto it = chain_start; it != nullptr; it = it->next) {
            if (options::show_function_signatures()) {
                if (index > 0) signatures += "\n";
                if (chain)
                    signatures += std::to_string(++index) + ". ";
                signatures += rec->name;
                signatures += it->signature;
                signatures += "\n";
            }
            if (it->doc && strlen(it->doc) > 0 && options::show_user_defined_docstrings()) {
                // If we're appending another docstring, and aren't printing function signatures, we
                // need to append a newline first:
                if (!options::show_function_signatures()) {
                    if (first_user_def) first_user_def = false;
                    else signatures += "\n";
                }
                if (options::show_function_signatures()) signatures += "\n";
                signatures += it->doc;
                if (options::show_function_signatures()) signatures += "\n";
            }
        }

        /* Install docstring */
        PyCFunctionObject *func = (PyCFunctionObject *) m_ptr;
        if (func->m_ml->ml_doc)
            std::free(const_cast<char *>(func->m_ml->ml_doc));
        func->m_ml->ml_doc = strdup(signatures.c_str());

        if (rec->is_method) {
            m_ptr = PYBIND11_INSTANCE_METHOD_NEW(m_ptr, rec->scope.ptr());
            if (!m_ptr)
                pybind11_fail("cpp_function::cpp_function(): Could not allocate instance method object");
            Py_DECREF(func);
        }
    }

    /// When a cpp_function is GCed, release any memory allocated by pybind11
    static void destruct(detail::function_record *rec) {
        while (rec) {
            detail::function_record *next = rec->next;
            if (rec->free_data)
                rec->free_data(rec);
            std::free((char *) rec->name);
            std::free((char *) rec->doc);
            std::free((char *) rec->signature);
            for (auto &arg: rec->args) {
                std::free(const_cast<char *>(arg.name));
                std::free(const_cast<char *>(arg.descr));
                arg.value.dec_ref();
            }
            if (rec->def) {
                std::free(const_cast<char *>(rec->def->ml_doc));
                delete rec->def;
            }
            delete rec;
            rec = next;
        }
    }

    /// Main dispatch logic for calls to functions bound using pybind11
    static PyObject *dispatcher(PyObject *self, PyObject *args_in, PyObject *kwargs_in) {
        using namespace detail;

        /* Iterator over the list of potentially admissible overloads */
        function_record *overloads = (function_record *) PyCapsule_GetPointer(self, nullptr),
                        *it = overloads;

        /* Need to know how many arguments + keyword arguments there are to pick the right overload */
        const size_t n_args_in = (size_t) PyTuple_GET_SIZE(args_in);

        handle parent = n_args_in > 0 ? PyTuple_GET_ITEM(args_in, 0) : nullptr,
               result = PYBIND11_TRY_NEXT_OVERLOAD;

        try {
            // We do this in two passes: in the first pass, we load arguments with `convert=false`;
            // in the second, we allow conversion (except for arguments with an explicit
            // py::arg().noconvert()).  This lets us prefer calls without conversion, with
            // conversion as a fallback.
            std::vector<function_call> second_pass;

            // However, if there are no overloads, we can just skip the no-convert pass entirely
            const bool overloaded = it != nullptr && it->next != nullptr;

            for (; it != nullptr; it = it->next) {

                /* For each overload:
                   1. Copy all positional arguments we were given, also checking to make sure that
                      named positional arguments weren't *also* specified via kwarg.
                   2. If we weren't given enough, try to make up the omitted ones by checking
                      whether they were provided by a kwarg matching the `py::arg("name")` name.  If
                      so, use it (and remove it from kwargs; if not, see if the function binding
                      provided a default that we can use.
                   3. Ensure that either all keyword arguments were "consumed", or that the function
                      takes a kwargs argument to accept unconsumed kwargs.
                   4. Any positional arguments still left get put into a tuple (for args), and any
                      leftover kwargs get put into a dict.
                   5. Pack everything into a vector; if we have py::args or py::kwargs, they are an
                      extra tuple or dict at the end of the positional arguments.
                   6. Call the function call dispatcher (function_record::impl)

                   If one of these fail, move on to the next overload and keep trying until we get a
                   result other than PYBIND11_TRY_NEXT_OVERLOAD.
                 */

                function_record &func = *it;
                size_t pos_args = func.nargs;    // Number of positional arguments that we need
                if (func.has_args) --pos_args;   // (but don't count py::args
                if (func.has_kwargs) --pos_args; //  or py::kwargs)

                if (!func.has_args && n_args_in > pos_args)
                    continue; // Too many arguments for this overload

                if (n_args_in < pos_args && func.args.size() < pos_args)
                    continue; // Not enough arguments given, and not enough defaults to fill in the blanks

                function_call call(func, parent);

                size_t args_to_copy = std::min(pos_args, n_args_in);
                size_t args_copied = 0;

                // 1. Copy any position arguments given.
                bool bad_arg = false;
                for (; args_copied < args_to_copy; ++args_copied) {
                    argument_record *arg_rec = args_copied < func.args.size() ? &func.args[args_copied] : nullptr;
                    if (kwargs_in && arg_rec && arg_rec->name && PyDict_GetItemString(kwargs_in, arg_rec->name)) {
                        bad_arg = true;
                        break;
                    }

                    handle arg(PyTuple_GET_ITEM(args_in, args_copied));
                    if (arg_rec && !arg_rec->none && arg.is_none()) {
                        bad_arg = true;
                        break;
                    }
                    call.args.push_back(arg);
                    call.args_convert.push_back(arg_rec ? arg_rec->convert : true);
                }
                if (bad_arg)
                    continue; // Maybe it was meant for another overload (issue #688)

                // We'll need to copy this if we steal some kwargs for defaults
                dict kwargs = reinterpret_borrow<dict>(kwargs_in);

                // 2. Check kwargs and, failing that, defaults that may help complete the list
                if (args_copied < pos_args) {
                    bool copied_kwargs = false;

                    for (; args_copied < pos_args; ++args_copied) {
                        const auto &arg = func.args[args_copied];

                        handle value;
                        if (kwargs_in && arg.name)
                            value = PyDict_GetItemString(kwargs.ptr(), arg.name);

                        if (value) {
                            // Consume a kwargs value
                            if (!copied_kwargs) {
                                kwargs = reinterpret_steal<dict>(PyDict_Copy(kwargs.ptr()));
                                copied_kwargs = true;
                            }
                            PyDict_DelItemString(kwargs.ptr(), arg.name);
                        } else if (arg.value) {
                            value = arg.value;
                        }

                        if (value) {
                            call.args.push_back(value);
                            call.args_convert.push_back(arg.convert);
                        }
                        else
                            break;
                    }

                    if (args_copied < pos_args)
                        continue; // Not enough arguments, defaults, or kwargs to fill the positional arguments
                }

                // 3. Check everything was consumed (unless we have a kwargs arg)
                if (kwargs && kwargs.size() > 0 && !func.has_kwargs)
                    continue; // Unconsumed kwargs, but no py::kwargs argument to accept them

                // 4a. If we have a py::args argument, create a new tuple with leftovers
                tuple extra_args;
                if (func.has_args) {
                    if (args_to_copy == 0) {
                        // We didn't copy out any position arguments from the args_in tuple, so we
                        // can reuse it directly without copying:
                        extra_args = reinterpret_borrow<tuple>(args_in);
                    } else if (args_copied >= n_args_in) {
                        extra_args = tuple(0);
                    } else {
                        size_t args_size = n_args_in - args_copied;
                        extra_args = tuple(args_size);
                        for (size_t i = 0; i < args_size; ++i) {
                            handle item = PyTuple_GET_ITEM(args_in, args_copied + i);
                            extra_args[i] = item.inc_ref().ptr();
                        }
                    }
                    call.args.push_back(extra_args);
                    call.args_convert.push_back(false);
                }

                // 4b. If we have a py::kwargs, pass on any remaining kwargs
                if (func.has_kwargs) {
                    if (!kwargs.ptr())
                        kwargs = dict(); // If we didn't get one, send an empty one
                    call.args.push_back(kwargs);
                    call.args_convert.push_back(false);
                }

                // 5. Put everything in a vector.  Not technically step 5, we've been building it
                // in `call.args` all along.
                #if !defined(NDEBUG)
                if (call.args.size() != func.nargs || call.args_convert.size() != func.nargs)
                    pybind11_fail("Internal error: function call dispatcher inserted wrong number of arguments!");
                #endif

                std::vector<bool> second_pass_convert;
                if (overloaded) {
                    // We're in the first no-convert pass, so swap out the conversion flags for a
                    // set of all-false flags.  If the call fails, we'll swap the flags back in for
                    // the conversion-allowed call below.
                    second_pass_convert.resize(func.nargs, false);
                    call.args_convert.swap(second_pass_convert);
                }

                // 6. Call the function.
                try {
                    loader_life_support guard{};
                    result = func.impl(call);
                }