/**
 * @file
 * @brief Prepare clips for the volca sample and encode them as a Syro stream.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rigkit {
namespace volca {

enum class VolcaModel { Sample = 0, Sample2 = 1 };

/** @brief 31.25 kHz — the volca sample's native rate. */
constexpr int kSampleRateHz = 31250;
/** @brief Both models: 4 MiB of sample FLASH (~65 s of 16-bit mono at 31.25 kHz). */
constexpr uint64_t kMemoryBytes = 4ull * 1024ull * 1024ull;

int slotCount(VolcaModel model);
const char* modelName(VolcaModel model);
/** @brief Seconds that `bytes` occupy at the native 16-bit mono rate. */
float secondsForBytes(uint64_t bytes);

struct PreparedSample {
	std::vector<int16_t> pcm; ///< Mono, 31.25 kHz, 16-bit.
	int sampleRate = kSampleRateHz;
	uint64_t bytes = 0; ///< `pcm.size() * 2`.
	float seconds = 0.f;
};

struct Fit {
	bool ok = false;
	uint64_t overflow = 0; ///< Bytes over budget when `ok` is false.
};

/**
 * @brief Stereo-to-mono, resample to 31.25 kHz, convert to 16-bit.
 */
bool prepareSample(const float* interleaved, uint64_t frames, int channels, int sampleRate,
				   PreparedSample& out);
bool prepareSample(const std::string& wavPath, PreparedSample& out);

class Capacity {
  public:
	explicit Capacity(VolcaModel model = VolcaModel::Sample);

	void setModel(VolcaModel model);
	VolcaModel model() const { return m_model; }
	void clear();
	void add(uint64_t bytes);
	void subtract(uint64_t bytes);

	uint64_t used() const { return m_used; }
	uint64_t budget() const { return kMemoryBytes; }
	uint64_t remaining() const;
	Fit canFit(uint64_t bytes) const;
	Fit canFit(const PreparedSample& sample) const { return canFit(sample.bytes); }

  private:
	VolcaModel m_model = VolcaModel::Sample;
	uint64_t m_used = 0;
};

struct SyroStream {
	std::vector<int16_t> pcm; ///< Interleaved stereo, 44.1 kHz.
	int sampleRate = 44100;
	int channels = 2;
	uint32_t frames = 0;
	float seconds = 0.f;
	std::string error;
};

/**
 * @brief Encode one sample into a Syro audio stream for slot `slot` (0-based).
 */
bool encodeSend(int slot, const PreparedSample& sample, SyroStream& out);
/** @brief Encode an erase of `slot`. */
bool encodeErase(int slot, SyroStream& out);

} // namespace volca
} // namespace rigkit
