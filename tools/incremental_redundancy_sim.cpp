// HARQ vs *adaptive* Incremental Redundancy compared with a rate-1/3
// baseline.
//
// Reference (no HARQ) and Type-I/II HARQ schemes use a rate-1/3
// convolutional mother code.  Adaptive IR uses a rate-1/M (M = 6)
// convolutional mother code with six distinct K=7 generators.  Each
// generator produces one parity *stream* of the rate-matched buffer;
// retransmissions add streams one at a time, so the effective code
// rate progressively drops:
//
//   attempt 0 -> streams {0, 1}  (2 streams, self-decodable rate 1/2)
//   attempt 1 -> stream  {2}     (cumulative rate 1/3, == base rate)
//   attempt 2 -> stream  {3}     (cumulative rate 1/4)
//   attempt 3 -> stream  {4}     (cumulative rate 1/5)
//   attempt 4 -> stream  {5}     (cumulative rate 1/6 == mother)
//
// On the receiver side every attempt's LLRs are placed into an
// N_ir-bit accumulating buffer at the corresponding stream positions
// (other positions stay zero -- "erasure") and decoded with the same
// rate-1/M codec each time.
//
// Pipeline (per information block):
//   info bits (k_info)
//     -> CRC encode  (append r_crc parity bits)
//   k_fec = k_info + r_crc
//     -> Convolutional encode
//          rate 1/3 (3 gens) for base/arq/chase
//          rate 1/M (M gens) for ir
//   N_base = 3 * (k_fec + (K - 1))
//   N_ir   = M * (k_fec + (K - 1))
//
// Equal *bit-budget* setup (instead of equal number of attempts):
//   all HARQ schemes are capped at the same maximum number of
//   channel bits per block (= N_ir = 1680, the full rate-1/6
//   codeword).  This translates to:
//       arq / chase : 2 attempts of N_base = 840  bits each
//       ir          : 5 attempts of 560 / 280...  bits each
//
// Schemes:
//   base    : single transmission of the rate-1/3 codeword. No HARQ.
//             A CRC failure counts as a block error.
//   arq     : Type-I HARQ on rate 1/3.  Retransmit the whole
//             codeword, decode each attempt independently.
//   chase   : Type-II Chase Combining on rate 1/3.  Retransmit the
//             whole codeword, accumulate LLRs across attempts.
//   ir      : Adaptive Type-II IR (see above).  First transmission
//             is *half* the size of a Chase transmission, every
//             subsequent retransmission is even smaller (one stream),
//             so in a good channel IR uses far fewer channel bits to
//             deliver the same block.
//
//             Bits per attempt:
//               arq / chase : k_fec_rate_1/3 codeword = N_base   = 840
//               ir  attempt 0: 2 * (k_fec + 6)                   = 560
//               ir  attempts >=1: 1 * (k_fec + 6)                = 280
//             Cumulative bits after k=1..5 IR attempts:
//                560,  840, 1120, 1400, 1680
//                                (effective rates 1/2 ... 1/6)
//             Cumulative bits after k=1..2 chase attempts:
//                840, 1680      (rate 1/3 with 1..2-fold rep,
//                                final effective rate ~ 1/6).
//
//             At the budget cap both schemes reach the same Shannon
//             limit (rate ~ 1/6).  CC gets there by *energy*
//             accumulation (every bit transmitted twice), IR by
//             *coding* (a genuine rate-1/6 mother code).  In a good
//             channel IR closes the block in one 560-bit transmission
//             (rate 1/2), so its goodput peaks at ~ 0.444 vs CC's 0.296.
//
// Goodput definition (per the assignment):
//
//   goodput = (info bits successfully delivered) /
//             (total channel bits transmitted, including CRC and FEC parity)
//
// The denominator counts every bit pushed into the channel, including
// CRC parity, FEC parity and bits used by failed retransmissions.

#include "awgn_channel.hpp"
#include "crc.hpp"
#include "fec/fec_factory.hpp"
#include "qpsk.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

  // ----- Simulation parameters -----------------------------------------------

  // kInfoBits is chosen so that the number of trellis steps after tail
  // termination (k_fec + 6) is even -- this keeps every IR attempt
  // size (one or two trellis-steps worth) divisible by 2 as required
  // by QPSK.  k_info = 249 gives:
  //   k_fec = 274, trellis_steps = 280, N_base = 840, N_ir = 1400.
  constexpr std::size_t kInfoBits = 249;      // useful info bits per block
  constexpr std::size_t kBlocksPerSnr = 2000; // Monte-Carlo trials per SNR
  constexpr std::uint32_t kSeed = 53u;

  // Rate-1/3 mother code for base / arq / chase: optimum-distance
  // (d_free = 15) K=7 generators (Larsen 1973).
  const std::vector<std::uint32_t> kBaseGenerators = {0133, 0171, 0165};
  constexpr int kBaseRateNum = 1;
  constexpr int kBaseRateDen = 3;

  // Rate-1/M mother code for adaptive IR.  Generators are ordered so
  // that the first 2 form an optimum rate-1/2 K=7 code (d_free = 10),
  // the first 3 the optimum rate-1/3 K=7 code (d_free = 15), and the
  // remaining ones provide independent parity for cumulative rates
  // 1/4, 1/5 and 1/6.  Each generator corresponds to one output stream.
  const std::vector<std::uint32_t> kIrGenerators = {
      0171, 0133, 0165, 0117, 0145, 0163};
  constexpr int kIrRateNum = 1;
  constexpr int kIrRateDen = 6; // M -- number of parity streams

  const std::vector<double> kSnrValues = {
      -15, -14.5, -14, -13.5, -13, -12.5, -12, -11.5, -11, -10.5, -10.0, -9.5, -9.0, -8.5, -8.0, -7.5, -7.0, -6.5, -6.0,
      -5.5, -5.0, -4.5, -4.0, -3.5, -3.0, -2.5, -2.0, -1.5,
      -1.0, -0.5, 0, 0.5, 1.0, 1.5, 2.0};

  // CRC-24A polynomial (0x864CFB) used in the original simulation.
  harq::Crc MakeCrc()
  {
    return harq::Crc({1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                      1, 1, 0, 0, 1, 1, 0, 1, 1, 1});
  }

  // ----- Helpers --------------------------------------------------------------

  enum class Scheme
  {
    kBase = 0,
    kArq = 1,
    kChase = 2,
    kIr = 3
  };

  const char *SchemeName(Scheme s)
  {
    switch (s)
    {
    case Scheme::kBase:
      return "base";
    case Scheme::kArq:
      return "arq";
    case Scheme::kChase:
      return "chase";
    case Scheme::kIr:
      return "ir";
    }
    return "?";
  }

  // Per-scheme retransmission budget.  We now equalise the *total
  // bit budget* per block rather than the number of attempts:
  //   IR covers every parity stream of its rate-1/M mother code in
  //   M - 1 attempts (560 bits + (M-2) * 280 = N_ir total),
  //   while Chase / ARQ transmit N_base = 840 bits per attempt, so
  //   they need ceil(N_ir / N_base) attempts to reach the same
  //   cap.  For rate 1/6 this gives  ir=5  chase=arq=2  attempts
  //   with an identical 1680-bit budget.
  constexpr int kIrMaxAttempts = kIrRateDen - 1; // = 5 for rate 1/6

  int MaxAttemptsForScheme(Scheme s)
  {
    switch (s)
    {
    case Scheme::kBase:
      return 1;
    case Scheme::kArq:
    case Scheme::kChase:
      // 2 attempts of 840 bits = 1680 bits = N_ir.
      return 2;
    case Scheme::kIr:
      return kIrMaxAttempts;
    }
    return 1;
  }

  bool CrcOk(const harq::Crc &crc,
             const std::vector<std::uint8_t> &decoded_with_crc,
             std::size_t info_size)
  {
    if (decoded_with_crc.empty())
      return false;
    if (crc.HasError(decoded_with_crc))
      return false;
    return crc.Decode(decoded_with_crc).size() == info_size;
  }

  bool BitsEqual(const std::vector<std::uint8_t> &a,
                 const std::vector<std::uint8_t> &b)
  {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
  }

  void AccumulateLlrs(const std::vector<double> &src,
                      std::vector<double> &dst)
  {
    for (std::size_t i = 0; i < src.size(); ++i)
    {
      dst[i] += src[i];
    }
  }

  // --- Adaptive Incremental Redundancy rate matching -----------------------
  //
  // The rate-1/M mother codeword lays out, per trellis step, M parity
  // bits in order (one per generator polynomial).  Position i in the
  // mother codeword belongs to stream (i % M).  Adaptive IR transmits:
  //   attempt 0 -> streams {0, 1}  (rate 1/2 self-decodable)
  //   attempt k -> stream  {k + 1}, k = 1, 2, ..., M - 2
  // After attempt k the effective code rate is k_fec / ((k + 2) *
  // trellis_steps) = 1 / (k + 2).

  bool IsStreamSentOnAttempt(int stream, int attempt)
  {
    if (attempt == 0)
    {
      return stream == 0 || stream == 1;
    }
    return stream == attempt + 1;
  }

  // Pick the subset of the rate-1/M mother codeword that goes on air
  // for the given attempt (in mother-codeword order).
  std::vector<std::uint8_t> AdaptiveIrSelect(
      const std::vector<std::uint8_t> &codeword,
      int attempt,
      int rate_den)
  {
    std::vector<std::uint8_t> out;
    out.reserve(codeword.size() / static_cast<std::size_t>(rate_den) *
                (attempt == 0 ? 2u : 1u));
    for (std::size_t i = 0; i < codeword.size(); ++i)
    {
      const int stream = static_cast<int>(
          i % static_cast<std::size_t>(rate_den));
      if (IsStreamSentOnAttempt(stream, attempt))
      {
        out.push_back(codeword[i]);
      }
    }
    return out;
  }

  // Inverse rate matching: accumulate the received LLRs into the
  // mother-sized buffer at the positions occupied by the attempt's
  // streams.  Positions of streams not yet transmitted stay at 0
  // (erasure).
  void AdaptiveIrAccumulate(
      const std::vector<double> &soft,
      int attempt,
      int rate_den,
      std::vector<double> &buffer)
  {
    std::size_t src = 0;
    for (std::size_t i = 0; i < buffer.size(); ++i)
    {
      const int stream = static_cast<int>(
          i % static_cast<std::size_t>(rate_den));
      if (IsStreamSentOnAttempt(stream, attempt))
      {
        buffer[i] += soft[src++];
      }
    }
  }

  std::uint32_t ChannelSeed(std::size_t block_idx, int attempt, Scheme scheme)
  {
    return kSeed +
           static_cast<std::uint32_t>(block_idx) * 1000003u +
           static_cast<std::uint32_t>(attempt) * 31u +
           static_cast<std::uint32_t>(scheme) * 7u;
  }

  // ----- Per-scheme simulation -----------------------------------------------

  struct Metrics
  {
    double bler = 0.0;         // P(undelivered block) = no CRC pass within budget
    double undetected = 0.0;   // P(CRC pass but bits wrong)
    double goodput = 0.0;      // info bits / total channel bits
    double avg_attempts = 0.0; // mean transmissions per block
  };

  Metrics SimulateScheme(double snr_db,
                         Scheme scheme,
                         const harq::fec::IFecCodec &base_codec,
                         const harq::fec::IFecCodec &ir_codec,
                         const harq::Crc &crc,
                         std::size_t info_size,
                         std::size_t base_n,
                         std::size_t ir_n)
  {
    std::mt19937 rng(kSeed + static_cast<std::uint32_t>(scheme) * 13u);

    // Pick the codec used by this scheme.
    const harq::fec::IFecCodec &codec =
        (scheme == Scheme::kIr) ? ir_codec : base_codec;
    const std::size_t mother_n = (scheme == Scheme::kIr) ? ir_n : base_n;

    std::size_t blocks_failed = 0;     // CRC NACK after exhausting budget
    std::size_t blocks_succeeded = 0;  // CRC pass within budget
    std::size_t blocks_undetected = 0; // CRC pass but decoded != info
    std::size_t total_attempts = 0;
    std::size_t total_channel_bits = 0;

    for (std::size_t block = 0; block < kBlocksPerSnr; ++block)
    {
      std::vector<std::uint8_t> info(info_size);
      for (auto &b : info)
      {
        b = static_cast<std::uint8_t>(rng() & 1u);
      }

      const std::vector<std::uint8_t> info_with_crc = crc.Encode(info);
      const std::vector<std::uint8_t> codeword = codec.Encode(info_with_crc);

      // Buffer used by chase / IR for accumulating LLRs over attempts.
      std::vector<double> combined_llr(mother_n, 0.0);

      bool delivered = false;
      bool correct = false;
      int attempts_used = 0;

      const int max_attempts = MaxAttemptsForScheme(scheme);

      for (int attempt = 0; attempt < max_attempts; ++attempt)
      {
        ++attempts_used;

        // Build the bit stream that goes on air for this attempt.
        std::vector<std::uint8_t> tx_bits;
        switch (scheme)
        {
        case Scheme::kBase:
        case Scheme::kArq:
        case Scheme::kChase:
          tx_bits = codeword;
          break;
        case Scheme::kIr:
          tx_bits = AdaptiveIrSelect(codeword, attempt, kIrRateDen);
          break;
        }

        total_channel_bits += tx_bits.size();

        // Transmit through QPSK + AWGN.
        harq::AwgnChannel channel(snr_db, ChannelSeed(block, attempt, scheme));
        const auto symbols = harq::QpskModulate(tx_bits);
        const auto rx_symbols = channel.TransmitComplex(symbols);
        const auto soft_llrs = harq::QpskDemodulate(rx_symbols, snr_db);

        // Decode according to the scheme.
        std::vector<std::uint8_t> decoded;
        switch (scheme)
        {
        case Scheme::kBase:
        case Scheme::kArq:
          decoded = codec.DecodeSoft(soft_llrs);
          break;
        case Scheme::kChase:
          AccumulateLlrs(soft_llrs, combined_llr);
          decoded = codec.DecodeSoft(combined_llr);
          break;
        case Scheme::kIr:
        {
          AdaptiveIrAccumulate(soft_llrs, attempt, kIrRateDen, combined_llr);
          decoded = codec.DecodeSoft(combined_llr);
          break;
        }
        }

        if (CrcOk(crc, decoded, info_size))
        {
          delivered = true;
          const auto extracted = crc.Decode(decoded);
          correct = BitsEqual(extracted, info);
          break;
        }
      }

      total_attempts += static_cast<std::size_t>(attempts_used);
      if (delivered)
      {
        ++blocks_succeeded;
        if (!correct)
          ++blocks_undetected;
      }
      else
      {
        ++blocks_failed;
      }
    }

    Metrics m;
    m.bler = static_cast<double>(blocks_failed) /
             static_cast<double>(kBlocksPerSnr);
    m.undetected = static_cast<double>(blocks_undetected) /
                   static_cast<double>(kBlocksPerSnr);
    m.goodput =
        total_channel_bits == 0
            ? 0.0
            : static_cast<double>(blocks_succeeded * info_size) /
                  static_cast<double>(total_channel_bits);
    m.avg_attempts = static_cast<double>(total_attempts) /
                     static_cast<double>(kBlocksPerSnr);
    return m;
  }

  std::unique_ptr<harq::fec::IFecCodec> MakeConvCodec(
      int conv_k_in,
      int rate_num,
      int rate_den,
      const std::vector<std::uint32_t> &generators)
  {
    harq::fec::FecConfig cfg{};
    cfg.codec_type = harq::fec::CodecType::kConvolutional;
    cfg.conv_input_bits_per_frame = conv_k_in;
    cfg.conv_rate_num = rate_num;
    cfg.conv_rate_den = rate_den;
    cfg.conv_generators = generators;
    return harq::fec::CreateCodec(cfg);
  }

} // namespace

int main()
{
  std::cout << std::fixed << std::setprecision(8);

  const harq::Crc crc = MakeCrc();
  const std::size_t info_size = kInfoBits;
  const int conv_k_in = static_cast<int>(info_size) + crc.r();

  auto base_codec = MakeConvCodec(conv_k_in, kBaseRateNum, kBaseRateDen,
                                  kBaseGenerators);
  auto ir_codec = MakeConvCodec(conv_k_in, kIrRateNum, kIrRateDen,
                                kIrGenerators);

  const std::size_t base_n =
      static_cast<std::size_t>(base_codec->output_bits_per_frame());
  const std::size_t ir_n =
      static_cast<std::size_t>(ir_codec->output_bits_per_frame());

  // Adaptive IR rate matching transmits trellis-step parity in chunks
  // of 1 or 2 streams.  Each stream has trellis_steps bits, so each
  // chunk must be even (required by QPSK).  trellis_steps = N_ir / M.
  const std::size_t trellis_steps =
      ir_n / static_cast<std::size_t>(kIrRateDen);
  if (trellis_steps % 2u != 0)
  {
    std::cerr << "trellis_steps = " << trellis_steps
              << " must be even for QPSK.\n";
    return 1;
  }
  if (base_n % 2u != 0)
  {
    std::cerr << "Base mother codeword size " << base_n
              << " must be even (QPSK).\n";
    return 1;
  }

  // Cumulative IR bits after attempts 1..M-1 (= 1..max IR attempts).
  // attempt 0 sends 2 streams, every later attempt 1 stream.
  std::vector<std::size_t> ir_cum_bits;
  ir_cum_bits.reserve(MaxAttemptsForScheme(Scheme::kIr));
  std::size_t running = 0;
  for (int a = 0; a < MaxAttemptsForScheme(Scheme::kIr); ++a)
  {
    running += (a == 0 ? 2u : 1u) * trellis_steps;
    ir_cum_bits.push_back(running);
  }

  std::cerr << "Configuration:\n"
            << "  info_bits      = " << info_size << "\n"
            << "  crc_bits       = " << crc.r() << "\n"
            << "  conv_k_in      = " << conv_k_in << "\n"
            << "  base mother N  = " << base_n << "  (rate 1/3)\n"
            << "  ir   mother N  = " << ir_n
            << "  (rate 1/" << kIrRateDen << ")\n"
            << "  trellis_steps  = " << trellis_steps << "\n"
            << "  attempts:        base=" << MaxAttemptsForScheme(Scheme::kBase)
            << " arq=" << MaxAttemptsForScheme(Scheme::kArq)
            << " chase=" << MaxAttemptsForScheme(Scheme::kChase)
            << " ir=" << MaxAttemptsForScheme(Scheme::kIr) << "\n"
            << "  ir cumulative bits after k attempts:";
  for (std::size_t b : ir_cum_bits)
  {
    std::cerr << " " << b;
  }
  std::cerr << "\n"
            << "  max channel bits per block:"
            << " arq=" << MaxAttemptsForScheme(Scheme::kArq) * base_n
            << " chase=" << MaxAttemptsForScheme(Scheme::kChase) * base_n
            << " ir=" << ir_cum_bits.back()
            << "\n"
            << "  blocks_per_snr = " << kBlocksPerSnr << "\n";

  std::cout << "snr,"
            << "bler_base,und_base,gput_base,att_base,"
            << "bler_arq,und_arq,gput_arq,att_arq,"
            << "bler_chase,und_chase,gput_chase,att_chase,"
            << "bler_ir,und_ir,gput_ir,att_ir\n";

  const std::vector<Scheme> schemes = {Scheme::kBase, Scheme::kArq,
                                       Scheme::kChase, Scheme::kIr};

  for (double snr : kSnrValues)
  {
    Metrics m[4];
    for (Scheme s : schemes)
    {
      m[static_cast<int>(s)] =
          SimulateScheme(snr, s, *base_codec, *ir_codec, crc,
                         info_size, base_n, ir_n);
    }

    std::cout << snr;
    for (Scheme s : schemes)
    {
      const Metrics &mm = m[static_cast<int>(s)];
      std::cout << "," << mm.bler << "," << mm.undetected << ","
                << mm.goodput << "," << mm.avg_attempts;
    }
    std::cout << "\n";

    std::cerr << "SNR " << snr << " dB done";
    for (Scheme s : schemes)
    {
      const Metrics &mm = m[static_cast<int>(s)];
      std::cerr << "  " << SchemeName(s) << ":G=" << mm.goodput
                << "/A=" << mm.avg_attempts;
    }
    std::cerr << "\n";
  }

  return 0;
}
