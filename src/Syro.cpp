#include "Syro.h"

#include "file/AudioConvert.h"
#include "file/AudioFile.h"
#include "file/AudioWriter.h"

#include "korg_syro_volcasample.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace rigkit {
namespace volca {

int slotCount(VolcaModel model) {
	return model == VolcaModel::Sample2 ? 200 : 100;
}

const char* modelName(VolcaModel model) {
	return model == VolcaModel::Sample2 ? "volca sample2" : "volca sample";
}

float secondsForBytes(uint64_t bytes) {
	return static_cast<float>(bytes) / (2.f * static_cast<float>(kSampleRateHz));
}

namespace {

int16_t toPcm16(float s) {
	const float c = std::clamp(s, -1.f, 1.f);
	const float scaled = c * 32767.f;
	return static_cast<int16_t>(scaled < 0.f ? scaled - 0.5f : scaled + 0.5f);
}

bool encodeSyro(SyroData& data, SyroStream& out) {
	out = {};
	SyroHandle handle = nullptr;
	uint32_t frames = 0;
	const SyroStatus start = SyroVolcaSample_Start(&handle, &data, 1, 0, &frames);
	if (start != Status_Success || !handle || frames == 0) {
		out.error = "Syro start failed";
		return false;
	}
	out.pcm.resize(static_cast<size_t>(frames) * 2);
	for (uint32_t i = 0; i < frames; ++i) {
		int16_t l = 0;
		int16_t r = 0;
		const SyroStatus st = SyroVolcaSample_GetSample(handle, &l, &r);
		if (st != Status_Success) {
			SyroVolcaSample_End(handle);
			out.error = "Syro get-sample failed";
			out.pcm.clear();
			return false;
		}
		out.pcm[static_cast<size_t>(i) * 2] = l;
		out.pcm[static_cast<size_t>(i) * 2 + 1] = r;
	}
	SyroVolcaSample_End(handle);
	out.frames = frames;
	out.sampleRate = 44100;
	out.channels = 2;
	out.seconds = static_cast<float>(frames) / 44100.f;
	return true;
}

} // namespace

bool prepareSample(const float* interleaved, uint64_t frames, int channels, int sampleRate,
				   PreparedSample& out) {
	out = {};
	if (!interleaved || frames == 0 || channels <= 0 || sampleRate <= 0) {
		return false;
	}
	std::vector<float> converted;
	if (!audio::convertAudio(interleaved, frames, channels, sampleRate, converted, 1,
							 kSampleRateHz) ||
		converted.empty()) {
		return false;
	}
	out.pcm.resize(converted.size());
	for (size_t i = 0; i < converted.size(); ++i) {
		out.pcm[i] = toPcm16(converted[i]);
	}
	out.sampleRate = kSampleRateHz;
	out.bytes = static_cast<uint64_t>(out.pcm.size()) * 2ull;
	out.seconds = secondsForBytes(out.bytes);
	return true;
}

bool prepareSample(const std::string& wavPath, PreparedSample& out) {
	audio::AudioFile file;
	if (!file.load(wavPath) || file.frames() == 0) {
		return false;
	}
	return prepareSample(file.data(), file.frames(), file.channels(), file.sampleRate(), out);
}

Capacity::Capacity(VolcaModel model) : m_model(model) {}

void Capacity::setModel(VolcaModel model) {
	m_model = model;
}

void Capacity::clear() {
	m_used = 0;
}

void Capacity::add(uint64_t bytes) {
	m_used += bytes;
}

void Capacity::subtract(uint64_t bytes) {
	if (bytes >= m_used) {
		m_used = 0;
	} else {
		m_used -= bytes;
	}
}

uint64_t Capacity::remaining() const {
	return m_used >= kMemoryBytes ? 0 : kMemoryBytes - m_used;
}

Fit Capacity::canFit(uint64_t bytes) const {
	Fit f;
	if (bytes <= remaining()) {
		f.ok = true;
		return f;
	}
	f.ok = false;
	f.overflow = bytes - remaining();
	return f;
}

bool encodeSend(int slot, const PreparedSample& sample, SyroStream& out) {
	out = {};
	if (sample.pcm.empty() || slot < 0 || slot >= VOLCASAMPLE_NUM_OF_SAMPLE) {
		out.error = "bad sample or slot";
		return false;
	}
	std::vector<uint8_t> bytes(sample.pcm.size() * 2);
	std::memcpy(bytes.data(), sample.pcm.data(), bytes.size());
	SyroData data{};
	data.DataType = DataType_Sample_Compress;
	data.pData = bytes.data();
	data.Number = static_cast<uint32_t>(slot);
	data.Size = static_cast<uint32_t>(bytes.size());
	data.Quality = 16;
	data.Fs = static_cast<uint32_t>(sample.sampleRate);
	data.SampleEndian = LittleEndian;
	return encodeSyro(data, out);
}

bool encodeErase(int slot, SyroStream& out) {
	out = {};
	if (slot < 0 || slot >= VOLCASAMPLE_NUM_OF_SAMPLE) {
		out.error = "bad slot";
		return false;
	}
	SyroData data{};
	data.DataType = DataType_Sample_Erase;
	data.pData = nullptr;
	data.Number = static_cast<uint32_t>(slot);
	data.Size = 0;
	data.Quality = 0;
	data.Fs = 0;
	data.SampleEndian = LittleEndian;
	return encodeSyro(data, out);
}

} // namespace volca
} // namespace rigkit
