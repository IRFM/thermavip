/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 #ifndef VIP_FUNCTION_TRAITS_H
 #define VIP_FUNCTION_TRAITS_H
 
 #include <tuple>
 #include <type_traits>
 #include <utility>
 #include <cstddef>
 
 namespace details
 {
 
     template<size_t N>
     struct ApplyFunctor
     {
         template<typename F, typename Getter, typename... A>
         static inline auto apply(F&& f, Getter&& t, A&&... a)
         {
             return ApplyFunctor<N - 1>::apply(std::forward<F>(f), std::forward<Getter>(t), std::forward<Getter>(t).template get<N - 1>(), std::forward<A>(a)...);
         }
     };
 
     template<>
     struct ApplyFunctor<0>
     {
         template<typename F, typename Getter, typename... A>
         static inline auto apply(F&& f, Getter&&, A&&... a) 
         {
             return std::forward<F>(f)(std::forward<A>(a)...);
         }
     };
 
     template<class TupleRef>
     struct TupleGetter
     {
         TupleRef t;
         template<size_t I>
         auto get() -> decltype(std::get<I>(t))
         {
             return std::get<I>(t);
         }
     };
 }
 
 
 /// @brief C++11 equivalent to std::apply()
 ///
 /// This overload forward tuple arguments to the functor object.
 ///
 template<typename F, typename... Args>
 inline auto vipApply(F&& f, std::tuple<Args...>&& t)
 {
     using tuple_type = std::tuple<Args...>;
     using ref_type = typename std::conditional<std::is_const<decltype(t)>::value, const tuple_type&, tuple_type&>::type;
     using getter = details::TupleGetter<ref_type>;
 
     return details::ApplyFunctor<sizeof...(Args)>::apply(std::forward<F>(f), getter{ t });
 }
 
 /// @brief C++11 equivalent to std::apply()
 ///
 /// This overload forward tuple arguments to the functor object.
 ///
 template<typename F, typename... Args>
 inline auto vipApply(F&& f, const std::tuple<Args...>& t)
 {
     using tuple_type = std::tuple<Args...>;
     using ref_type = const tuple_type&;
     using getter = details::TupleGetter<ref_type>;
 
     return details::ApplyFunctor<sizeof...(Args)>::apply(std::forward<F>(f), getter{ t });
 }
 
 #endif