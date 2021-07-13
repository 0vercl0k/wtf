//===- FuzzerMutate.h - Internal header for the Fuzzer ----------*- C++ -* ===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// fuzzer::MutationDispatcher
//===----------------------------------------------------------------------===//

#ifndef LLVM_FUZZER_MUTATE_H
#define LLVM_FUZZER_MUTATE_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <limits>
#include <random>

// Platform detection.
#define LIBFUZZER_APPLE 0
#define LIBFUZZER_FUCHSIA 0
#define LIBFUZZER_LINUX 0
#define LIBFUZZER_NETBSD 0
#define LIBFUZZER_FREEBSD 0
#define LIBFUZZER_OPENBSD 0
#define LIBFUZZER_WINDOWS 1

#if defined(_MSC_VER) && !defined(__clang__)
// MSVC compiler is being used.
#define LIBFUZZER_MSVC 1
#else
#define LIBFUZZER_MSVC 0
#endif

#define LIBFUZZER_POSIX                                                        \
  (LIBFUZZER_APPLE || LIBFUZZER_LINUX || LIBFUZZER_NETBSD ||                   \
   LIBFUZZER_FREEBSD || LIBFUZZER_OPENBSD)

#ifdef __x86_64
#else
#  define ATTRIBUTE_TARGET_POPCNT
#endif


#  define ATTRIBUTE_NO_SANITIZE_MEMORY
#  define ALWAYS_INLINE

#if LIBFUZZER_WINDOWS
#define ATTRIBUTE_NO_SANITIZE_ADDRESS
#endif

#if LIBFUZZER_WINDOWS
#define ATTRIBUTE_ALIGNED(X) __declspec(align(X))
#define ATTRIBUTE_INTERFACE __declspec(dllexport)
// This is used for __sancov_lowest_stack which is needed for
// -fsanitize-coverage=stack-depth. That feature is not yet available on
// Windows, so make the symbol static to avoid linking errors.
#define ATTRIBUTES_INTERFACE_TLS_INITIAL_EXEC static
#define ATTRIBUTE_NOINLINE __declspec(noinline)
#endif

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define ATTRIBUTE_NO_SANITIZE_ALL ATTRIBUTE_NO_SANITIZE_ADDRESS
#  elif __has_feature(memory_sanitizer)
#    define ATTRIBUTE_NO_SANITIZE_ALL ATTRIBUTE_NO_SANITIZE_MEMORY
#  else
#    define ATTRIBUTE_NO_SANITIZE_ALL
#  endif
#else
#  define ATTRIBUTE_NO_SANITIZE_ALL
#endif

namespace fuzzer {

    template <class T> T Min(T a, T b) { return a < b ? a : b; }
    template <class T> T Max(T a, T b) { return a > b ? a : b; }

    class Random;
    class Dictionary;
    class DictionaryEntry;
    class MutationDispatcher;
    struct FuzzingOptions;
    class InputCorpus;
    struct InputInfo;
    struct ExternalFunctions;

    // Global interface to functions that may or may not be available.
    extern ExternalFunctions* EF;

    // We are using a custom allocator to give a different symbol name to STL
    // containers in order to avoid ODR violations.
    template<typename T>
    class fuzzer_allocator : public std::allocator<T> {
    public:
        fuzzer_allocator() = default;

        template<class U>
        fuzzer_allocator(const fuzzer_allocator<U>&) {}

        template<class Other>
        struct rebind { typedef fuzzer_allocator<Other> other; };
    };

    template<typename T>
    using Vector = std::vector<T, fuzzer_allocator<T>>;

    template<typename T>
    using Set = std::set<T, std::less<T>, fuzzer_allocator<T>>;

    typedef Vector<uint8_t> Unit;
    typedef Vector<Unit> UnitVector;
    typedef int (*UserCallback)(const uint8_t* Data, size_t Size);

    uint8_t* ExtraCountersBegin();
    uint8_t* ExtraCountersEnd();
    void ClearExtraCounters();

    extern bool RunningUserCallback;

    // A simple POD sized array of bytes.
    template <size_t kMaxSizeT> class FixedWord {
    public:
        static const size_t kMaxSize = kMaxSizeT;
        FixedWord() {}
        FixedWord(const uint8_t* B, uint8_t S) { Set(B, S); }

        void Set(const uint8_t* B, uint8_t S) {
            assert(S <= kMaxSize);
            memcpy(Data, B, S);
            Size = S;
        }

        bool operator==(const FixedWord<kMaxSize>& w) const {
            return Size == w.Size && 0 == memcmp(Data, w.Data, Size);
        }

        static size_t GetMaxSize() { return kMaxSize; }
        const uint8_t* data() const { return Data; }
        uint8_t size() const { return Size; }

    private:
        uint8_t Size = 0;
        uint8_t Data[kMaxSize];
    };

    typedef FixedWord<64> Word;

    class DictionaryEntry {
    public:
        DictionaryEntry() {}
        DictionaryEntry(Word W) : W(W) {}
        DictionaryEntry(Word W, size_t PositionHint) : W(W), PositionHint(PositionHint) {}
        const Word& GetW() const { return W; }
        bool HasPositionHint() const { return PositionHint != std::numeric_limits<size_t>::max(); }
        size_t GetPositionHint() const {
            assert(HasPositionHint());
            return PositionHint;
        }
        void IncUseCount() { UseCount++; }
        void IncSuccessCount() { SuccessCount++; }
        size_t GetUseCount() const { return UseCount; }
        size_t GetSuccessCount() const { return SuccessCount; }
#if 0
        void Print(const char* PrintAfter = "\n") {
            PrintASCII(W.data(), W.size());
            if (HasPositionHint())
                Printf("@%zd", GetPositionHint());
            Printf("%s", PrintAfter);
        }
#endif
    private:
        Word W;
        size_t PositionHint = std::numeric_limits<size_t>::max();
        size_t UseCount = 0;
        size_t SuccessCount = 0;
    };

    class Dictionary {
    public:
        static const size_t kMaxDictSize = 1 << 14;

        bool ContainsWord(const Word& W) const {
            return std::any_of(begin(), end(), [&](const DictionaryEntry& DE) {
                return DE.GetW() == W;
            });
        }
        const DictionaryEntry* begin() const { return &DE[0]; }
        const DictionaryEntry* end() const { return begin() + Size; }
        DictionaryEntry& operator[] (size_t Idx) {
            assert(Idx < Size);
            return DE[Idx];
        }
        void push_back(DictionaryEntry DE) {
            if (Size < kMaxDictSize)
                this->DE[Size++] = DE;
        }
        void clear() { Size = 0; }
        bool empty() const { return Size == 0; }
        size_t size() const { return Size; }

    private:
        DictionaryEntry DE[kMaxDictSize];
        size_t Size = 0;
    };

    // Parses one dictionary entry.
    // If successful, write the enty to Unit and returns true,
    // otherwise returns false.
    bool ParseOneDictionaryEntry(const std::string& Str, Unit* U);
    // Parses the dictionary file, fills Units, returns true iff all lines
    // were parsed successfully.
    bool ParseDictionaryFile(const std::string& Text, Vector<Unit>* Units);


    class Random : public std::minstd_rand {
    public:
        Random(unsigned int seed) : std::minstd_rand(seed) {}
        result_type operator()() { return this->std::minstd_rand::operator()(); }
        size_t Rand() { return this->operator()(); }
        size_t RandBool() { return Rand() % 2; }
        size_t SkewTowardsLast(size_t n) {
            size_t T = this->operator()(n * n);
            size_t Res = size_t(sqrt(T));
            return Res;
        }
        size_t operator()(size_t n) { return n ? Rand() % n : 0; }
        intptr_t operator()(intptr_t From, intptr_t To) {
            assert(From < To);
            intptr_t RangeSize = To - From + 1;
            return operator()(RangeSize) + From;
        }
    };

    struct FuzzingOptions {
        int Verbosity = 1;
        size_t MaxLen = 0;
        size_t LenControl = 1000;
        int UnitTimeoutSec = 300;
        int TimeoutExitCode = 70;
        int OOMExitCode = 71;
        int InterruptExitCode = 72;
        int ErrorExitCode = 77;
        bool IgnoreTimeouts = true;
        bool IgnoreOOMs = true;
        bool IgnoreCrashes = false;
        int MaxTotalTimeSec = 0;
        int RssLimitMb = 0;
        int MallocLimitMb = 0;
        bool DoCrossOver = true;
        int MutateDepth = 5;
        bool ReduceDepth = false;
        bool UseCounters = false;
        bool UseMemmem = true;
        bool UseCmp = false;
        int UseValueProfile = false;
        bool Shrink = false;
        bool ReduceInputs = false;
        int ReloadIntervalSec = 1;
        bool ShuffleAtStartUp = true;
        bool PreferSmall = true;
        size_t MaxNumberOfRuns = -1L;
        int ReportSlowUnits = 10;
        bool OnlyASCII = false;
        std::string OutputCorpus;
        std::string ArtifactPrefix = "./";
        std::string ExactArtifactPath;
        std::string ExitOnSrcPos;
        std::string ExitOnItem;
        std::string FocusFunction;
        std::string DataFlowTrace;
        std::string CollectDataFlow;
        std::string FeaturesDir;
        std::string StopFile;
        bool SaveArtifacts = true;
        bool PrintNEW = true; // Print a status line when new units are found;
        bool PrintNewCovPcs = false;
        int PrintNewCovFuncs = 0;
        bool PrintFinalStats = false;
        bool PrintCorpusStats = false;
        bool PrintCoverage = false;
        bool DumpCoverage = false;
        bool DetectLeaks = true;
        int PurgeAllocatorIntervalSec = 1;
        int  TraceMalloc = 0;
        bool HandleAbrt = false;
        bool HandleBus = false;
        bool HandleFpe = false;
        bool HandleIll = false;
        bool HandleInt = false;
        bool HandleSegv = false;
        bool HandleTerm = false;
        bool HandleXfsz = false;
        bool HandleUsr1 = false;
        bool HandleUsr2 = false;
    };

    class MutationDispatcher {
    public:
        MutationDispatcher(Random& Rand, const FuzzingOptions& Options);
        ~MutationDispatcher() {}
        /// Indicate that we are about to start a new sequence of mutations.
        void StartMutationSequence();
        /// Print the current sequence of mutations.
        void PrintMutationSequence();
        /// Indicate that the current sequence of mutations was successful.
        void RecordSuccessfulMutationSequence();
        /// Mutates data by invoking user-provided mutator.
        size_t Mutate_Custom(uint8_t* Data, size_t Size, size_t MaxSize);
        /// Mutates data by invoking user-provided crossover.
        size_t Mutate_CustomCrossOver(uint8_t* Data, size_t Size, size_t MaxSize);
        /// Mutates data by shuffling bytes.
        size_t Mutate_ShuffleBytes(uint8_t* Data, size_t Size, size_t MaxSize);
        /// Mutates data by erasing bytes.
        size_t Mutate_EraseBytes(uint8_t* Data, size_t Size, size_t MaxSize);
        /// Mutates data by inserting a byte.
        size_t Mutate_InsertByte(uint8_t* Data, size_t Size, size_t MaxSize);
        /// Mutates data by inserting several repeated bytes.
        size_t Mutate_InsertRepeatedBytes(uint8_t* Data, size_t Size, size_t MaxSize);
        /// Mutates data by chanding one byte.
        size_t Mutate_ChangeByte(uint8_t* Data, size_t Size, size_t MaxSize);
        /// Mutates data by chanding one bit.
        size_t Mutate_ChangeBit(uint8_t* Data, size_t Size, size_t MaxSize);
        /// Mutates data by copying/inserting a part of data into a different place.
        size_t Mutate_CopyPart(uint8_t* Data, size_t Size, size_t MaxSize);

        /// Mutates data by adding a word from the manual dictionary.
        size_t Mutate_AddWordFromManualDictionary(uint8_t* Data, size_t Size,
            size_t MaxSize);

        /// Mutates data by adding a word from the TORC.
        size_t Mutate_AddWordFromTORC(uint8_t* Data, size_t Size, size_t MaxSize);

        /// Mutates data by adding a word from the persistent automatic dictionary.
        size_t Mutate_AddWordFromPersistentAutoDictionary(uint8_t* Data, size_t Size,
            size_t MaxSize);

        /// Tries to find an ASCII integer in Data, changes it to another ASCII int.
        size_t Mutate_ChangeASCIIInteger(uint8_t* Data, size_t Size, size_t MaxSize);
        /// Change a 1-, 2-, 4-, or 8-byte integer in interesting ways.
        size_t Mutate_ChangeBinaryInteger(uint8_t* Data, size_t Size, size_t MaxSize);

        /// CrossOver Data with CrossOverWith.
        size_t Mutate_CrossOver(uint8_t* Data, size_t Size, size_t MaxSize);

        /// Applies one of the configured mutations.
        /// Returns the new size of data which could be up to MaxSize.
        size_t Mutate(uint8_t* Data, size_t Size, size_t MaxSize);

        /// Applies one of the configured mutations to the bytes of Data
        /// that have '1' in Mask.
        /// Mask.size() should be >= Size.
        size_t MutateWithMask(uint8_t* Data, size_t Size, size_t MaxSize,
            const Vector<uint8_t>& Mask);

        /// Applies one of the default mutations. Provided as a service
        /// to mutation authors.
        size_t DefaultMutate(uint8_t* Data, size_t Size, size_t MaxSize);

        /// Creates a cross-over of two pieces of Data, returns its size.
        size_t CrossOver(const uint8_t* Data1, size_t Size1, const uint8_t* Data2,
            size_t Size2, uint8_t* Out, size_t MaxOutSize);

        void AddWordToManualDictionary(const Word& W);

        void PrintRecommendedDictionary();

        void SetCrossOverWith(const Unit* U) { CrossOverWith = U; }

        Random& GetRand() { return Rand; }

    private:
        struct Mutator {
            size_t(MutationDispatcher::* Fn)(uint8_t* Data, size_t Size, size_t Max);
            const char* Name;
        };

        size_t AddWordFromDictionary(Dictionary& D, uint8_t* Data, size_t Size,
            size_t MaxSize);
        size_t MutateImpl(uint8_t* Data, size_t Size, size_t MaxSize,
            Vector<Mutator>& Mutators);

        size_t InsertPartOf(const uint8_t* From, size_t FromSize, uint8_t* To,
            size_t ToSize, size_t MaxToSize);
        size_t CopyPartOf(const uint8_t* From, size_t FromSize, uint8_t* To,
            size_t ToSize);
        size_t ApplyDictionaryEntry(uint8_t* Data, size_t Size, size_t MaxSize,
            DictionaryEntry& DE);

        template <class T>
        DictionaryEntry MakeDictionaryEntryFromCMP(T Arg1, T Arg2,
            const uint8_t* Data, size_t Size);
        DictionaryEntry MakeDictionaryEntryFromCMP(const Word& Arg1, const Word& Arg2,
            const uint8_t* Data, size_t Size);
        DictionaryEntry MakeDictionaryEntryFromCMP(const void* Arg1, const void* Arg2,
            const void* Arg1Mutation,
            const void* Arg2Mutation,
            size_t ArgSize,
            const uint8_t* Data, size_t Size);

        Random& Rand;
        const FuzzingOptions Options;

        // Dictionary provided by the user via -dict=DICT_FILE.
        Dictionary ManualDictionary;
        // Temporary dictionary modified by the fuzzer itself,
        // recreated periodically.
        Dictionary TempAutoDictionary;
        // Persistent dictionary modified by the fuzzer, consists of
        // entries that led to successful discoveries in the past mutations.
        Dictionary PersistentAutoDictionary;

        Vector<DictionaryEntry*> CurrentDictionaryEntrySequence;

        static const size_t kCmpDictionaryEntriesDequeSize = 16;
        DictionaryEntry CmpDictionaryEntriesDeque[kCmpDictionaryEntriesDequeSize];
        size_t CmpDictionaryEntriesDequeIdx = 0;

        const Unit* CrossOverWith = nullptr;
        Vector<uint8_t> MutateInPlaceHere;
        Vector<uint8_t> MutateWithMaskTemp;
        // CustomCrossOver needs its own buffer as a custom implementation may call
        // LLVMFuzzerMutate, which in turn may resize MutateInPlaceHere.
        Vector<uint8_t> CustomCrossOverInPlaceHere;

        Vector<Mutator> Mutators;
        Vector<Mutator> DefaultMutators;
        Vector<Mutator> CurrentMutatorSequence;
    };

}  // namespace fuzzer

#endif  // LLVM_FUZZER_MUTATE_H
