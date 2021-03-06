// Author: Enrico Guiraud, Danilo Piparo CERN  03/2017

/*************************************************************************
 * Copyright (C) 1995-2016, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_RRESULTPTR
#define ROOT_RRESULTPTR

#include "ROOT/TypeTraits.hxx"
#include "ROOT/RDFNodes.hxx"
#include "TError.h" // Warning

#include <memory>
#include <functional>

namespace ROOT {
namespace Internal{
namespace RDF{
   class GraphCreatorHelper;
}
}
}

namespace ROOT {
namespace RDF {
// Fwd decl for MakeResultPtr
template <typename T>
class RResultPtr;

} // ns RDF

namespace Detail {
namespace RDF {
using ROOT::RDF::RResultPtr;
// Fwd decl for RResultPtr
template <typename T>
RResultPtr<T> MakeResultPtr(const std::shared_ptr<T> &r, const std::shared_ptr<RLoopManager> &df,
                            std::shared_ptr<ROOT::Internal::RDF::RActionBase> actionPtr);
} // ns RDF
} // ns Detail
namespace RDF {
namespace RDFInternal = ROOT::Internal::RDF;
namespace RDFDetail = ROOT::Detail::RDF;
namespace TTraits = ROOT::TypeTraits;

/// Smart pointer for the return type of actions
/**
\class ROOT::RDF::RResultPtr
\ingroup dataframe
\brief A wrapper around the result of RDataFrame actions able to trigger calculations lazily.
\tparam T Type of the action result

A smart pointer which allows to access the result of a RDataFrame action. The
methods of the encapsulated object can be accessed via the arrow operator.
Upon invocation of the arrow operator or dereferencing (`operator*`), the
loop on the events and calculations of all scheduled actions are executed
if needed.
It is possible to iterate on the result proxy if the proxied object is a collection.
~~~{.cpp}
for (auto& myItem : myResultProxy) { ... };
~~~
If iteration is not supported by the type of the proxied object, a compilation error is thrown.

*/
template <typename T>
class RResultPtr {
   // private using declarations
   using SPT_t = std::shared_ptr<T>;
   using SPTLM_t = std::shared_ptr<RDFDetail::RLoopManager>;
   using WPTLM_t = std::weak_ptr<RDFDetail::RLoopManager>;

   // friend declarations
   template <typename T1>
   friend RResultPtr<T1>
   RDFDetail::MakeResultPtr(const std::shared_ptr<T1> &, const SPTLM_t &, std::shared_ptr<RDFInternal::RActionBase>);
   template <class T1, class T2>
   friend bool operator==(const RResultPtr<T1> &lhs, const RResultPtr<T2> &rhs);
   template <class T1, class T2>
   friend bool operator!=(const RResultPtr<T1> &lhs, const RResultPtr<T2> &rhs);
   template <class T1>
   friend bool operator==(const RResultPtr<T1> &lhs, std::nullptr_t rhs);
   template <class T1>
   friend bool operator==(std::nullptr_t lhs, const RResultPtr<T1> &rhs);
   template <class T1>
   friend bool operator!=(const RResultPtr<T1> &lhs, std::nullptr_t rhs);
   template <class T1>
   friend bool operator!=(std::nullptr_t lhs, const RResultPtr<T1> &rhs);

   friend class ROOT::Internal::RDF::GraphDrawing::GraphCreatorHelper;

   /// \cond HIDDEN_SYMBOLS
   template <typename V, bool hasBeginEnd = TTraits::HasBeginAndEnd<V>::value>
   struct TIterationHelper {
      using Iterator_t = void;
      void GetBegin(const V &) { static_assert(sizeof(V) == 0, "It does not make sense to ask begin for this class."); }
      void GetEnd(const V &) { static_assert(sizeof(V) == 0, "It does not make sense to ask end for this class."); }
   };

   template <typename V>
   struct TIterationHelper<V, true> {
      using Iterator_t = decltype(std::begin(std::declval<V>()));
      static Iterator_t GetBegin(const V &v) { return std::begin(v); };
      static Iterator_t GetEnd(const V &v) { return std::end(v); };
   };
   /// \endcond

   WPTLM_t fImplWeakPtr; ///< Points to the RLoopManager at the root of the functional graph
   SPT_t fObjPtr;  ///< Shared pointer encapsulating the wrapped result
   /// Owning pointer to the action that will produce this result.
   /// Ownership is shared with other copies of this ResultPtr.
   std::shared_ptr<RDFInternal::RActionBase> fActionPtr;

   /// Triggers the event loop in the RLoopManager instance to which it's associated via the fImplWeakPtr
   void TriggerRun();

   /// Get the pointer to the encapsulated result.
   /// Ownership is not transferred to the caller.
   /// Triggers event loop and execution of all actions booked in the associated RLoopManager.
   T *Get()
   {
      if (!fActionPtr->HasRun())
         TriggerRun();
      return fObjPtr.get();
   }

   RResultPtr(std::shared_ptr<T> objPtr, std::shared_ptr<RDFDetail::RLoopManager> lm,
              std::shared_ptr<RDFInternal::RActionBase> actionPtr)
      : fImplWeakPtr(std::move(lm)), fObjPtr(std::move(objPtr)), fActionPtr(std::move(actionPtr))
   {
   }

public:
   using Value_t = T;                       ///< Convenience alias to simplify access to proxied type
   static constexpr ULong64_t kOnce = 0ull; ///< Convenience definition to express a callback must be executed once

   RResultPtr() = default;
   RResultPtr(const RResultPtr &) = default;
   RResultPtr(RResultPtr &&) = default;
   RResultPtr &operator=(const RResultPtr &) = default;
   RResultPtr &operator=(RResultPtr &&) = default;
   explicit operator bool() const { return bool(fObjPtr); }

   /// Get a const reference to the encapsulated object.
   /// Triggers event loop and execution of all actions booked in the associated RLoopManager.
   const T &GetValue() { return *Get(); }

   /// Get the pointer to the encapsulated object.
   /// Triggers event loop and execution of all actions booked in the associated RLoopManager.
   T *GetPtr() { return Get(); }

   /// Get a pointer to the encapsulated object.
   /// Triggers event loop and execution of all actions booked in the associated RLoopManager.
   T &operator*() { return *Get(); }

   /// Get a pointer to the encapsulated object.
   /// Ownership is not transferred to the caller.
   /// Triggers event loop and execution of all actions booked in the associated RLoopManager.
   T *operator->() { return Get(); }

   /// Return an iterator to the beginning of the contained object if this makes
   /// sense, throw a compilation error otherwise
   typename TIterationHelper<T>::Iterator_t begin()
   {
      if (!fActionPtr->HasRun())
         TriggerRun();
      return TIterationHelper<T>::GetBegin(*fObjPtr);
   }

   /// Return an iterator to the end of the contained object if this makes
   /// sense, throw a compilation error otherwise
   typename TIterationHelper<T>::Iterator_t end()
   {
      if (!fActionPtr->HasRun())
         TriggerRun();
      return TIterationHelper<T>::GetEnd(*fObjPtr);
   }

   // clang-format off
   /// Register a callback that RDataFrame will execute "everyNEvents" on a partial result.
   ///
   /// \param[in] everyNEvents Frequency at which the callback will be called, as a number of events processed
   /// \param[in] callback a callable with signature `void(Value_t&)` where Value_t is the type of the value contained in this RResultPtr
   /// \return this RResultPtr, to allow chaining of OnPartialResultSlot with other calls
   ///
   /// The callback must be a callable (lambda, function, functor class...) that takes a reference to the result type as
   /// argument and returns nothing. RDataFrame will invoke registered callbacks passing partial action results as
   /// arguments to them (e.g. a histogram filled with a part of the selected events, a counter incremented only up to a
   /// certain point, a mean over a subset of the events and so forth).
   ///
   /// Callbacks can be used e.g. to inspect partial results of the analysis while the event loop is running. For
   /// example one can draw an up-to-date version of a result histogram every 100 entries like this:
   /// \code{.cpp}
   /// auto h = tdf.Histo1D("x");
   /// TCanvas c("c","x hist");
   /// h.OnPartialResult(100, [&c](TH1D &h_) { c.cd(); h_.Draw(); c.Update(); });
   /// h->Draw(); // event loop runs here, this `Draw` is executed after the event loop is finished
   /// \endcode
   ///
   /// A value of 0 for everyNEvents indicates the callback must be executed only once, before running the event loop.
   /// A conveniece definition `kOnce` is provided to make this fact more expressive in user code (see snippet below).
   /// Multiple callbacks can be registered with the same RResultPtr (i.e. results of RDataFrame actions) and will
   /// be executed sequentially. Callbacks are executed in the order they were registered.
   /// The type of the value contained in a RResultPtr is also available as RResultPtr<T>::Value_t, e.g.
   /// \code{.cpp}
   /// auto h = tdf.Histo1D("x");
   /// // h.kOnce is 0
   /// // decltype(h)::Value_t is TH1D
   /// \endcode
   ///
   /// When implicit multi-threading is enabled, the callback:
   /// - will never be executed by multiple threads concurrently: it needs not be thread-safe. For example the snippet
   ///   above that draws the partial histogram on a canvas works seamlessly in multi-thread event loops.
   /// - will always be executed "everyNEvents": partial results will "contain" that number of events more from
   ///   one call to the next
   /// - might be executed by a different worker thread at different times: the value of `std::this_thread::get_id()`
   ///   might change between calls
   /// To register a callback that is called by _each_ worker thread (concurrently) every N events one can use
   /// OnPartialResultSlot.
   // clang-format on
   RResultPtr<T> &OnPartialResult(ULong64_t everyNEvents, std::function<void(T &)> callback)
   {
      auto lm = fImplWeakPtr.lock();
      if (!lm)
         throw std::runtime_error("The main RDataFrame is not reachable: did it go out of scope?");
      const auto nSlots = lm->GetNSlots();
      auto actionPtr = fActionPtr;
      auto c = [nSlots, actionPtr, callback](unsigned int slot) {
         if (slot != nSlots - 1)
            return;
         auto partialResult = static_cast<Value_t *>(actionPtr->PartialUpdate(slot));
         callback(*partialResult);
      };
      lm->RegisterCallback(everyNEvents, std::move(c));
      return *this;
   }

   // clang-format off
   /// Register a callback that RDataFrame will execute in each worker thread concurrently on that thread's partial result.
   ///
   /// \param[in] everyNEvents Frequency at which the callback will be called by each thread, as a number of events processed
   /// \param[in] a callable with signature `void(unsigned int, Value_t&)` where Value_t is the type of the value contained in this RResultPtr
   /// \return this RResultPtr, to allow chaining of OnPartialResultSlot with other calls
   ///
   /// See `OnPartialResult` for a generic explanation of the callback mechanism.
   /// Compared to `OnPartialResult`, this method has two major differences:
   /// - all worker threads invoke the callback once every specified number of events. The event count is per-thread,
   ///   and callback invocation might happen concurrently (i.e. the callback must be thread-safe)
   /// - the callable must take an extra `unsigned int` parameter corresponding to a multi-thread "processing slot":
   ///   this is a "helper value" to simplify writing thread-safe callbacks: different worker threads might invoke the
   ///   callback concurrently but always with different `slot` numbers.
   /// - a value of 0 for everyNEvents indicates the callback must be executed once _per slot_.
   ///
   /// For example, the following snippet prints out a thread-safe progress bar of the events processed by RDataFrame
   /// \code
   /// auto c = tdf.Count(); // any action would do, but `Count` is the most lightweight
   /// std::string progress;
   /// std::mutex bar_mutex;
   /// c.OnPartialResultSlot(nEvents / 100, [&progress, &bar_mutex](unsigned int, ULong64_t &) {
   ///    std::lock_guard<std::mutex> lg(bar_mutex);
   ///    progress.push_back('#');
   ///    std::cout << "\r[" << std::left << std::setw(100) << progress << ']' << std::flush;
   /// });
   /// std::cout << "Analysis running..." << std::endl;
   /// *c; // trigger the event loop by accessing an action's result
   /// std::cout << "\nDone!" << std::endl;
   /// \endcode
   // clang-format on
   RResultPtr<T> &OnPartialResultSlot(ULong64_t everyNEvents, std::function<void(unsigned int, T &)> callback)
   {
      auto lm = fImplWeakPtr.lock();
      if (!lm)
         throw std::runtime_error("The main RDataFrame is not reachable: did it go out of scope?");
      auto actionPtr = fActionPtr;
      auto c = [actionPtr, callback](unsigned int slot) {
         auto partialResult = static_cast<Value_t *>(actionPtr->PartialUpdate(slot));
         callback(slot, *partialResult);
      };
      lm->RegisterCallback(everyNEvents, std::move(c));
      return *this;
   }
};

template <typename T>
void RResultPtr<T>::TriggerRun()
{
   auto df = fImplWeakPtr.lock();
   if (!df) {
      throw std::runtime_error("The main RDataFrame is not reachable: did it go out of scope?");
   }
   df->Run();
}

template <class T1, class T2>
bool operator==(const RResultPtr<T1> &lhs, const RResultPtr<T2> &rhs)
{
   return lhs.fObjPtr == rhs.fObjPtr;
}

template <class T1, class T2>
bool operator!=(const RResultPtr<T1> &lhs, const RResultPtr<T2> &rhs)
{
   return lhs.fObjPtr != rhs.fObjPtr;
}

template <class T1>
bool operator==(const RResultPtr<T1> &lhs, std::nullptr_t rhs)
{
   return lhs.fObjPtr == rhs;
}

template <class T1>
bool operator==(std::nullptr_t lhs, const RResultPtr<T1> &rhs)
{
   return lhs == rhs.fObjPtr;
}

template <class T1>
bool operator!=(const RResultPtr<T1> &lhs, std::nullptr_t rhs)
{
   return lhs.fObjPtr != rhs;
}

template <class T1>
bool operator!=(std::nullptr_t lhs, const RResultPtr<T1> &rhs)
{
   return lhs != rhs.fObjPtr;
}

} // end NS RDF


namespace Detail {
namespace RDF {
/// Create a RResultPtr and set its pointer to the corresponding RAction
/// This overload is invoked by non-jitted actions, as they have access to RAction before constructing RResultPtr.
template <typename T>
RResultPtr<T> MakeResultPtr(const std::shared_ptr<T> &r, const std::shared_ptr<RLoopManager> &df,
                            std::shared_ptr<RDFInternal::RActionBase> actionPtr)
{
   auto resPtr = RResultPtr<T>(r, df, std::move(actionPtr));
   return resPtr;
}
} // end NS RDF
} // end NS Detail
} // end NS ROOT

#endif // ROOT_TRESULTPROXY
