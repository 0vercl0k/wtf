// Axel '0vercl0k' Souchet - August 24 2020
// Header to be able to compile https://github.com/google/honggfuzz/blob/2.3.1/
#pragma once
#include "globals.h"
#include "platform.h"
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>

namespace honggfuzz {
#define HF_MIN(x, y) (x <= y ? x : y)
#define HF_MAX(x, y) (x >= y ? x : y)
#define ATOMIC_GET
#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x) / sizeof(*x))
#endif
#define HF_ATTR_UNUSED /* nuthin' */
#define util_Malloc(x) malloc(x)
/* Maximum size of the input file in bytes (1 MiB) */
#define _HF_INPUT_MAX_SIZE (1024ULL * 1024ULL)

static void wmb() {}
static void LOG_F(const char *, ...) { std::abort(); }

struct dynfile_t {
  size_t size;
  uint64_t cov[4];
  size_t idx;
  int fd;
  uint64_t timeExecUSecs;
  struct _dynfile_t *src;
  uint32_t refs;
  uint8_t *data;

  dynfile_t() { memset(this, 0, sizeof(*this)); }
};

struct cmpfeedback_t {
  uint32_t cnt;
  struct {
    uint8_t val[1];
    uint32_t len;
  } valArr[1];
};

struct honggfuzz_t {
  struct {
    time_t lastCovUpdate;
  } timing;
  struct {
    // struct {
    //  uint8_t val[256];
    //  size_t len;
    //} dictionary[1024];
    struct {
      uint8_t val[1];
      size_t len;
    } dictionary[1];
    size_t dictionaryCnt;
    const char *dictionaryFile;
    size_t mutationsMax;
    unsigned mutationsPerRun;
    size_t maxInputSz;
  } mutate;
  struct {
    cmpfeedback_t *cmpFeedbackMap;
    bool cmpFeedback;
  } feedback;
  struct {
    bool only_printable;
  } cfg;

  honggfuzz_t() { memset(this, 0, sizeof(*this)); }
};

struct run_t {
  honggfuzz_t *global = nullptr;
  dynfile_t *dynfile = nullptr;
  unsigned mutationsPerRun = 0;

  std::unique_ptr<uint8_t[]> RandomBuffer;
  size_t RandomBufferSize = 0;
  std::mt19937_64 &Rng_;

  run_t(std::mt19937_64 &Rng) : Rng_(Rng) {}
};

static inline size_t input_getRandomInputAsBuf(run_t *run,
                                               const uint8_t **buf) {
  *buf = run->RandomBuffer.get();
  return run->RandomBufferSize;
}

static uint64_t util_rnd64(run_t *run) { return run->Rng_(); }

static uint64_t util_rndGet(run_t *run, uint64_t min, uint64_t max) {
  if (min > max) {
    LOG_F("min:%" PRIu64 " > max:%" PRIu64, min, max);
  }

  if (max == UINT64_MAX) {
    return util_rnd64(run);
  }

  return ((util_rnd64(run) % (max - min + 1)) + min);
}

static void util_rndBuf(run_t *run, uint8_t *buf, size_t sz) {
  if (sz == 0) {
    return;
  }
  for (size_t i = 0; i < sz; i++) {
    buf[i] = (uint8_t)(util_rnd64(run) >> 40);
  }
}

static inline void input_setSize(run_t *run, size_t sz) {
  run->dynfile->size = sz;
}

/* Generate random printable ASCII */
static uint8_t util_rndPrintable(run_t *run) {
  return uint8_t(util_rndGet(run, 32, 126));
}

/* Turn one byte to a printable ASCII */
static void util_turnToPrintable(uint8_t *buf, size_t sz) {
  for (size_t i = 0; i < sz; i++) {
    buf[i] = buf[i] % 95 + 32;
  }
}

static void util_rndBufPrintable(run_t *run, uint8_t *buf, size_t sz) {
  for (size_t i = 0; i < sz; i++) {
    buf[i] = util_rndPrintable(run);
  }
}

void mangle_mangleContent(run_t *run, int speed_factor);

#ifdef WINDOWS
#pragma warning(disable : 4244)
#pragma warning(disable : 4334)
#endif
}; // namespace honggfuzz