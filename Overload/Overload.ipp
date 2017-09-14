#pragma once

#include <sharp/Overload/Overload.hpp>
#include <sharp/Traits/Traits.hpp>

#include <utility>
#include <type_traits>
#include <tuple>

namespace sharp {
namespace overload_detail {

    /**
     * An incccesible integral_constant-ish type that is used to distinguish
     * between user code return types and make pretend return types.
     *
     * Since lambdas in user space cannot return this.  This is used as the
     * return type for function pointer calls when constructing a list of
     * possible function calls.  If overload resolution dispatches to
     * something that returns an instance of InaccessibleConstant then it has
     * to be a function pointer.
     */
    template <int Value>
    class InaccessibleConstant : public std::integral_constant<int, Value> {};

    /**
     * Check if the type is an instantiation of InaccessibleConstant
     */
    template <typename T>
    struct IsInstantiationOfInaccessibleConstant
            : public std::integral_constant<bool, false> {};
    template <int Value>
    struct IsInstantiationOfInaccessibleConstant<InaccessibleConstant<Value>>
            : public std::integral_constant<bool, true> {};

    /**
     * The overload detector, all this does is import the list of function
     * calls in each type, if function type is a functor then operator() will
     * be imported, and if the type is a function type the return type will be
     * changed to InaccessibleConstant and a pretend operator() will be
     * created
     */
    template <int current, typename... Funcs>
    class FunctionOverloadDetector;
    /**
     * Specialziation for function object types, dont increment the counter
     * because the counter is used for indexing into a tuple of function
     * pointers and then getting the right function pointer to call and all
     * the function pointers are going to be together with no functor in
     * between or after them in the tail
     */
    template <int current, typename Func, typename... Tail>
    class FunctionOverloadDetector<current, Func, Tail...>
            : public Func,
            public FunctionOverloadDetector<current, Tail...> {
    public:

        /**
         * Declare the current impl to just be using the operator() of the
         * functor, since that will never be able to return an
         * InaccessibleConstant this can be used to distinguish between
         * whether a function is being called or a functor
         */
        using Func::operator();
        using FunctionOverloadDetector<current, Tail...>::operator();
    };
    /**
     * Specialization for function pointer types
     */
    template <int current,
              typename ReturnType, typename... Args,
              typename... Tail>
    class FunctionOverloadDetector<current, ReturnType (*) (Args...), Tail...>
            : public FunctionOverloadDetector<current + 1, Tail...> {
    public:

        /**
         * Declare the current impl, use the return type as a guide for which
         * overload was called
         *
         * This will return an inaccessible constant which will be used to
         * determine whether or not a function pointer is the one overload
         * resolution happens to and also determine which function pointer
         * overload resolution dispatches to
         *
         * And then import all the other impl functions as well
         */
        InaccessibleConstant<current> operator()(Args...) const;
        using FunctionOverloadDetector<current + 1, Tail...>::operator();
    };
    /**
     * Base case just creates a never callable operator()
     */
    template <int current>
    class FunctionOverloadDetector<current> {
    protected:

        /**
         * This is never callable as the user can never supply an Inaccessible
         * argument (or more like should not)
         *
         * Further the class Inaccessible is not a complete type, so it can
         * never be instantiated in any code
         *
         * Also the member is protected so is not visible in user score
         */
        class Inaccessible;
        InaccessibleConstant<current> operator()(Inaccessible) const;
    };

    /**
     * Enable if the return type of overload detector is an instantiation
     * of InaccessibleConstant
     */
    template <typename Detector, int Index, typename... Ts>
    using EnableIfThisFunctionPointerBestOverload = std::enable_if_t<
        std::is_same<
            decltype(std::declval<Detector>()(std::declval<Ts>()...)),
            InaccessibleConstant<Index>>::value>;
    /**
     * Enable if a function pointer type is the type preferred in overload
     * dispatch
     */
    template <typename Detector, typename... Args>
    using EnableIfFunctionPreferred = std::enable_if_t<
        IsInstantiationOfInaccessibleConstant<
            decltype(std::declval<Detector>()(std::declval<Args>()...))>
        ::value>;
    /**
     * Enable if a functor overload is preferred, if the overload dispatch
     * does not return an instantiation of InaccessibleConstant then a functor
     * overload will be preferred
     */
    template <typename Detector, typename... Args>
    using EnableIfFunctorPreferred = std::enable_if_t<
        !IsInstantiationOfInaccessibleConstant<
            decltype(std::declval<Detector>()(std::declval<Args>()...))>
        ::value>;
    /**
     * Enable if the type is not an instantiation of self, this is used to
     * prevent forwarding reference constructors from matching everything
     */
    template <typename Type>
    using EnableIfTuple =  sharp::void_t<
        decltype(std::get<0>(std::declval<Type>()))>;
    template <typename Type, typename Self>
    using EnableIfNotSelf = std::enable_if_t<!std::is_same<
        std::decay_t<Type>, std::decay_t<Self>>::value>;
    /**
     * Check to see if the overload resolution is well defined
     */
    template <typename Detector, typename... Args>
    constexpr const auto overload_well_formed = std::is_same<
        sharp::void_t<decltype(std::declval<Detector>()(
                    std::declval<Args>()...))>,
        void>::value;

    /**
     * A trait that gets a value list corresponding to all the functors in the
     * type list
     */
    /**
     * Base case
     */
    template <typename FunctorVList, typename FPtrVList, int current,
              typename... Funcs>
    struct SplitLists {
        static_assert(current >= 0, "");
        using type = std::pair<FunctorVList, FPtrVList>;
    };
    template <typename FunctorVList, typename FPtrVList,
              int current, typename Head, typename... Funcs>
    struct SplitLists<FunctorVList, FPtrVList, current, Head, Funcs...> {
        static_assert(current >= 0, "");

        // in the default case the head is a functor so concatenate the empty
        // value list with 0
        using NewFunctorVList = Concatenate_t<FunctorVList, ValueList<current>>;
        using NewFPtrVList = FPtrVList;

        using Next = SplitLists<NewFunctorVList, NewFPtrVList, current + 1,
                                Funcs...>;
        using type = typename Next::type;
    };
    /**
     * Specialization for function pointers
     */
    template <typename FunctorVList, typename FPtrVList,
              int current,
              typename ReturnType, typename... Args, typename... Funcs>
    struct SplitLists<FunctorVList, FPtrVList, current,
                      ReturnType (*) (Args...), Funcs...> {
        static_assert(current >= 0, "");

        // in the default case the head is a functor so concatenate the empty
        // value list with 0
        using NewFunctorVList = FunctorVList;
        using NewFPtrVList = Concatenate_t<FPtrVList, ValueList<current>>;

        using Next = SplitLists<NewFunctorVList, NewFPtrVList, current + 1,
                                Funcs...>;
        using type = typename Next::type;
    };

    /**
     * The operator() generator, this just imports operator() for functors and
     * writes a SFINAE enabled operator() for functions
     */
    template <typename Detector, int Index, typename Func>
    class OverloadGenerator : public Func {
    public:
        template <typename F,
                  EnableIfNotSelf<F, OverloadGenerator>* = nullptr>
        explicit OverloadGenerator(F&& f) : Func{std::forward<F>(f)} {}

        using Func::operator();
    };
    template <typename Detector, int Index,
              typename ReturnType, typename... Args>
    class OverloadGenerator<Detector, Index, ReturnType (*) (Args...)> {
    public:
        using FPtr_t = ReturnType (*) (Args...);

        template <typename F,
                  EnableIfNotSelf<F, OverloadGenerator>* = nullptr>
        explicit OverloadGenerator(F&& f) : f_ptr{std::forward<F>(f)} {}

        template <typename... Ts,
                  EnableIfThisFunctionPointerBestOverload<Detector,
                                                          Index,
                                                          Ts...>* = nullptr>
        decltype(auto) operator()(Ts&&... args) const {
            return this->f_ptr(std::forward<Ts>(args)...);
        }

    private:
        FPtr_t f_ptr;
    };

    /**
     * The implementation for function pointers, this contains a forwarding
     * reference variadic operator() that is only enabled if the current
     * function pointer is the best overload
     */
    template <typename Detector, int Index, typename... Funcs>
    class OverloadImpl {};
    template <typename Detector, int Index, typename Func, typename... Funcs>
    class OverloadImpl<Detector, Index, Func, Funcs...>
            : public OverloadGenerator<Detector, Index, Func>,
            public OverloadImpl<Detector, Index + 1, Funcs...> {
    public:

        template <typename F, typename... Fs,
                  EnableIfNotSelf<F, OverloadImpl>* = nullptr>
        explicit OverloadImpl(F&& f, Fs&&... fs)
            : OverloadGenerator<Detector, Index, Func>{std::forward<F>(f)},
              OverloadImpl<Detector, Index + 1, Funcs...>{
                  std::forward<Fs>(fs)...} {}

        using OverloadGenerator<Detector, Index, Func>::operator();
        using OverloadImpl<Detector, Index + 1, Funcs...>::operator();
    };
    /**
     * Base case, do not inherit from anything and do not import anything
     */
    template <typename Detector, int Index, typename Func>
    class OverloadImpl<Detector, Index, Func>
            : public OverloadGenerator<Detector, Index, Func> {
    public:

        template <typename F,
                  EnableIfNotSelf<F, OverloadImpl>* = nullptr>
        explicit OverloadImpl(F&& f)
            : OverloadGenerator<Detector, Index, Func>{std::forward<F>(f)} {}

        using OverloadGenerator<Detector, Index, Func>::operator();
    };

    /**
     * Check that the overload detector is valid and then if there is a
     * possible call with the overload detector, dispatch that to either the
     * collection of functors or to the collection of function pointers
     */
    template <typename Detector,
              typename ValueListFunctions, typename ValueListFunctors,
              typename... Funcs>
    class CheckAndForward;
    template <typename Detector, int... IndicesFunctions,
              int... IndicesFunctors, typename... Funcs>
    class CheckAndForward<Detector,
                          ValueList<IndicesFunctions...>,
                          ValueList<IndicesFunctors...>,
                          Funcs...> {
    public:
        /**
         * Typedefs for the functions and functors that are going to be used
         * to store the function pointers and the functors
         */
        using Functors = OverloadImpl<
            Detector, 0,
            std::tuple_element_t<IndicesFunctors, std::tuple<Funcs...>>...>;
        using Functions = OverloadImpl<
            Detector, 0,
            std::tuple_element_t<IndicesFunctions, std::tuple<Funcs...>>...>;
        using FunctionsStorage = std::tuple<std::decay_t<
            std::tuple_element_t<IndicesFunctions, std::tuple<Funcs...>>>...>;

        /**
         * Only use this constructor when the instance is is a tuple-like
         * thing decomposable using std::get
         */
        template <typename Args, EnableIfTuple<Args>* = nullptr>
        explicit CheckAndForward(Args&& args)
            : functors{std::get<IndicesFunctors>(std::forward<Args>(args))...},
            functions{std::get<IndicesFunctions>(std::forward<Args>(args))...},
            fptrs{std::get<IndicesFunctions>(std::forward<Args>(args))...}
        {}

        /**
         * The templated function call operator, make sure that the Detector
         * resolution is not an error and then forward the arguments to the
         * Overload class
         */
        template <typename... Args,
                  EnableIfFunctionPreferred<Detector, Args...>* = nullptr>
        auto operator()(Args&&... args) const
                -> decltype(std::declval<Functions>()(
                            std::declval<Args>()...)) {
            static_assert(overload_well_formed<const Detector, Args...>, "");
            return this->functions(std::forward<Args>(args)...);
        }

        /**
         * 4 overloads corresponding to const, non-const lvalue and rvalue
         * overloads for the operator() member function
         */
        template <typename... Args,
                  EnableIfFunctorPreferred<const Detector&&, Args...>*
                      = nullptr>
        auto operator()(Args&&... args) const &&
                -> decltype(std::declval<const Functors&&>()(
                            std::declval<Args>()...)) {
            static_assert(overload_well_formed<const Detector&&, Args...>, "");
            return std::move(this->functors)(std::forward<Args>(args)...);
        }
        template <typename... Args,
                  EnableIfFunctorPreferred<const Detector&, Args...>* = nullptr>
        auto operator()(Args&&... args) const &
                -> decltype(std::declval<const Functors&>()(
                            std::declval<Args>()...)) {
            static_assert(overload_well_formed<const Detector&, Args...>, "");
            return this->functors(std::forward<Args>(args)...);
        }
        template <typename... Args,
                  EnableIfFunctorPreferred<Detector&&, Args...>* = nullptr>
        auto operator()(Args&&... args) &&
                -> decltype(std::declval<Functors&&>()(
                            std::declval<Args>()...)) {
            static_assert(overload_well_formed<Detector&&, Args...>, "");
            return std::move(this->functors)(std::forward<Args>(args)...);
        }
        template <typename... Args,
                  EnableIfFunctorPreferred<Detector&, Args...>* = nullptr>
        auto operator()(Args&&... args) &
                -> decltype(std::declval<Functors&>()(
                            std::declval<Args>()...)) {
            static_assert(overload_well_formed<Detector&, Args...>, "");
            return this->functors(std::forward<Args>(args)...);
        }

        /**
         * The instances of OverloadImpl objects
         */
        Functors functors;
        Functions functions;

        /**
         * Store the function pointers for ease
         */
        FunctionsStorage fptrs;
    };

    /**
     * Checks if the type is an instantiation of CheckAndForward
     */
    template <typename T>
    struct IsInstantiationCheckForward
        : public std::integral_constant<bool, false> {};
    template <typename One, typename Two, typename Three, typename... Funcs>
    struct IsInstantiationCheckForward<
        CheckAndForward<One, Two, Three, Funcs...>>
        : public std::integral_constant<bool, true> {};
    /**
     * Enable if the type is an instantiation of CheckAndForward
     */
    template <typename Type>
    using EnableIfCheckForward = std::enable_if_t<IsInstantiationCheckForward<
        Type>::value>;

    /**
     * The implementation of the overload function that actually returns the
     * overload object.  This does some template preprocessing on the type
     * lists and creates some stuff required by the CheckAndForward handle
     * class that will be held by the user
     */
    template <typename... Funcs>
    auto overload_impl(std::tuple<Funcs...>&& funcs) {
        // get the overload detector
        using Detector = overload_detail::FunctionOverloadDetector<
            0, std::decay_t<Funcs>...>;

	    // get the value list of the functions and the functors
        using SplitValueLists = typename overload_detail::SplitLists<
            ValueList<>, ValueList<>, 0, std::decay_t<Funcs>...>::type;
        using FunctorVList = typename SplitValueLists::first_type;
        using FPtrVList = typename SplitValueLists::second_type;

        // return the overloaded object
        return overload_detail::CheckAndForward<Detector,
                                                FPtrVList, FunctorVList,
                                                std::decay_t<Funcs>...>{
            std::move(funcs)};
    }

    /**
     * Decompose a CheckAndForward instance into a tuple of function pointers
     * and a function object, and for the rest just make a tuple only of that
     * element
     */
    template <typename T>
    auto decompose(sharp::preferred_dispatch<0>, T&& instance) {
        return std::forward_as_tuple(std::forward<T>(instance));
    }
    template <typename T, EnableIfCheckForward<std::decay_t<T>>* = nullptr>
    auto decompose(sharp::preferred_dispatch<1>, T&& instance) {

        // get the function pointers and the function object out into tuples
        // and concatenate them
        auto fptrs = instance.fptrs;
        auto&& functor = std::forward<T>(instance).functors;
        auto functors = std::forward_as_tuple(
                std::forward<decltype(functor)>(functor));

        return std::tuple_cat(std::move(fptrs), std::move(functors));
    }

    /**
     * Individually decompose each argument into a tuple, concatenate those
     * and return the concatenated tuple
     */
    template <std::size_t... Indices, typename TupleParams>
    auto decompose_args(std::index_sequence<Indices...>,
                        TupleParams&& params) {
        auto decomposed_args = std::tuple_cat(decompose(
                    sharp::preferred_dispatch<1>{},
                    std::get<Indices>(std::forward<TupleParams>(params)))...);
        return decomposed_args;
    }

} // namespace overload_detail

template <typename... Funcs>
auto overload(Funcs&&... funcs) {

    using namespace overload_detail;

    // decompose any CheckAndForward instances in the arguments into flag
    // arguments that include the functors and the function pointers that
    // compose that instance, this leads to a flag structure of overload
    // resolution and not a very vertical one with lots of inheritance with
    // recursive overload generation
    auto args = decompose_args(
            std::index_sequence_for<Funcs...>{},
            std::forward_as_tuple(std::forward<Funcs>(funcs)...));

    // forward the args to the overload impl function
    return overload_impl(std::move(args));
}

} // namespace sharp