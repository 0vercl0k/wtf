// Axel '0vercl0k' Souchet - November 27 2022
#include "human.h"

[[nodiscard]] chrono::seconds
SecondsSince(const chrono::system_clock::time_point &Since) {
  const auto &Now = chrono::system_clock::now();
  return chrono::duration_cast<chrono::seconds>(Now - Since);
}

//
// Utility that is used to print microseconds for human.
//

[[nodiscard]] SecondsHuman_t SecondsToHuman(const chrono::seconds &Seconds) {
  const char *Unit = "s";
  double SecondNumber = double(Seconds.count());
  const double M = 60;
  const double H = M * 60;
  const double D = H * 24;
  if (SecondNumber >= D) {
    Unit = "d";
    SecondNumber /= D;
  } else if (SecondNumber >= H) {
    Unit = "hr";
    SecondNumber /= H;
  } else if (SecondNumber >= M) {
    Unit = "min";
    SecondNumber /= M;
  }

  return {SecondNumber, Unit};
}

//
// Utility that is used to print bytes for human.
//

[[nodiscard]] BytesHuman_t BytesToHuman(const uint64_t Bytes_) {
  const char *Unit = "b";
  double Bytes = double(Bytes_);
  const uint64_t K = 1'024;
  const uint64_t M = K * K;
  const uint64_t G = M * K;
  if (Bytes >= G) {
    Unit = "gb";
    Bytes /= G;
  } else if (Bytes >= M) {
    Unit = "mb";
    Bytes /= M;
  } else if (Bytes >= K) {
    Unit = "kb";
    Bytes /= K;
  }

  return {Bytes, Unit};
}

[[nodiscard]] NumberHuman_t NumberToHuman(const uint64_t N_) {
  const char *Unit = "";
  double N = double(N_);
  const uint64_t K = 1'000;
  const uint64_t M = K * K;
  if (N > M) {
    Unit = "m";
    N /= M;
  } else if (N > K) {
    Unit = "k";
    N /= K;
  }

  return {N, Unit};
}
