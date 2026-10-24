#include "SyroPlayer.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

#include "miniaudio.h"

namespace rigkit {
namespace volca {

struct SyroPlayerImpl {
	ma_context ctx{};
	bool ctxInit = false;
	ma_device device{};
	bool deviceInit = false;
	std::vector<int16_t> pcm;
	std::atomic<uint64_t> cursor{0};
	uint64_t frames = 0;
	std::atomic<bool> done{false};
};

static void syroPlaybackCallback(ma_device* device, void* output, const void* /*input*/,
								 ma_uint32 frameCount) {
	auto* impl = static_cast<SyroPlayerImpl*>(device->pUserData);
	auto* dst = static_cast<int16_t*>(output);
	const uint64_t frames = impl->frames;
	uint64_t cur = impl->cursor.load(std::memory_order_relaxed);
	const uint64_t remain = cur < frames ? frames - cur : 0;
	const uint64_t copy = std::min<uint64_t>(remain, frameCount);
	if (copy > 0) {
		std::memcpy(dst, impl->pcm.data() + cur * 2,
					static_cast<size_t>(copy) * 2 * sizeof(int16_t));
		cur += copy;
		impl->cursor.store(cur, std::memory_order_relaxed);
	}
	if (copy < frameCount) {
		std::memset(dst + copy * 2, 0,
					static_cast<size_t>(frameCount - copy) * 2 * sizeof(int16_t));
		impl->done.store(true, std::memory_order_relaxed);
	}
}

SyroPlayer::SyroPlayer() : m_impl(std::make_unique<SyroPlayerImpl>()) {
	rescanDevices();
}

SyroPlayer::~SyroPlayer() {
	stop();
	if (m_impl->ctxInit) {
		ma_context_uninit(&m_impl->ctx);
		m_impl->ctxInit = false;
	}
}

void SyroPlayer::rescanDevices() {
	m_names.clear();
	m_names.emplace_back("System Default");
	if (!m_impl->ctxInit) {
		if (ma_context_init(nullptr, 0, nullptr, &m_impl->ctx) != MA_SUCCESS) {
			return;
		}
		m_impl->ctxInit = true;
	}
	ma_device_info* playback = nullptr;
	ma_uint32 count = 0;
	if (ma_context_get_devices(&m_impl->ctx, &playback, &count, nullptr, nullptr) != MA_SUCCESS) {
		return;
	}
	for (ma_uint32 i = 0; i < count; ++i) {
		m_names.emplace_back(playback[i].name);
	}
}

bool SyroPlayer::play(const SyroStream& stream, int deviceIndex) {
	stop();
	m_error.clear();
	if (stream.pcm.empty() || stream.frames == 0) {
		m_error = "empty Syro stream";
		return false;
	}
	if (!m_impl->ctxInit) {
		rescanDevices();
		if (!m_impl->ctxInit) {
			m_error = "no audio context";
			return false;
		}
	}

	m_impl->pcm = stream.pcm;
	m_impl->frames = stream.frames;
	m_impl->cursor.store(0, std::memory_order_relaxed);
	m_impl->done.store(false, std::memory_order_relaxed);

	ma_device_id* id = nullptr;
	ma_device_id chosen{};
	if (deviceIndex > 0) {
		ma_device_info* playback = nullptr;
		ma_uint32 count = 0;
		if (ma_context_get_devices(&m_impl->ctx, &playback, &count, nullptr, nullptr) ==
				MA_SUCCESS &&
			static_cast<ma_uint32>(deviceIndex) <= count) {
			chosen = playback[static_cast<ma_uint32>(deviceIndex) - 1].id;
			id = &chosen;
		}
	}

	ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
	cfg.playback.format = ma_format_s16;
	cfg.playback.channels = 2;
	cfg.playback.shareMode = ma_share_mode_shared;
	cfg.playback.pDeviceID = id;
	cfg.sampleRate = static_cast<ma_uint32>(stream.sampleRate);
	cfg.dataCallback = &syroPlaybackCallback;
	cfg.pUserData = m_impl.get();
	cfg.wasapi.noHardwareOffloading = MA_TRUE;

	if (ma_device_init(&m_impl->ctx, &cfg, &m_impl->device) != MA_SUCCESS) {
		m_error = "could not open playback device";
		return false;
	}
	if (m_impl->device.pContext->backend == ma_backend_null) {
		ma_device_uninit(&m_impl->device);
		m_error = "null audio backend";
		return false;
	}
	m_impl->deviceInit = true;
	if (ma_device_start(&m_impl->device) != MA_SUCCESS) {
		ma_device_uninit(&m_impl->device);
		m_impl->deviceInit = false;
		m_error = "could not start playback";
		return false;
	}
	m_playing = true;
	return true;
}

void SyroPlayer::stop() {
	if (m_impl->deviceInit) {
		ma_device_uninit(&m_impl->device);
		m_impl->deviceInit = false;
	}
	m_playing = false;
	m_impl->done.store(true, std::memory_order_relaxed);
}

void SyroPlayer::pump() {
	if (m_playing && m_impl->done.load(std::memory_order_relaxed)) {
		stop();
	}
}

float SyroPlayer::progress() const {
	if (m_impl->frames == 0) {
		return 0.f;
	}
	if (m_impl->done.load(std::memory_order_relaxed) && !m_playing) {
		return 1.f;
	}
	const uint64_t cur = m_impl->cursor.load(std::memory_order_relaxed);
	const float p = static_cast<float>(cur) / static_cast<float>(m_impl->frames);
	return p > 1.f ? 1.f : p;
}

} // namespace volca
} // namespace rigkit
