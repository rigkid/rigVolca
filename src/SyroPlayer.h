/**
 * @file
 * @brief Bit-clean playback of a Syro stream on a chosen output device.
 */
#pragma once

#include "Syro.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rigkit {
namespace volca {

struct SyroPlayerImpl;

/**
 * @brief Play encoded Syro PCM through miniaudio, bypassing the mixer (no FX, no master gain).
 * @details Index 0 in `deviceNames()` is the system default. The volca sample2 USB
 * audio device appears as a named playback device — pick it to send over USB.
 * The original volca sample takes the same stream through a cable into SYNC IN.
 */
class SyroPlayer {
  public:
	SyroPlayer();
	~SyroPlayer();

	SyroPlayer(const SyroPlayer&) = delete;
	SyroPlayer& operator=(const SyroPlayer&) = delete;

	void rescanDevices();
	const std::vector<std::string>& deviceNames() const { return m_names; }

	bool play(const SyroStream& stream, int deviceIndex = 0);
	void stop();
	/** @brief Uninit the device once the stream has finished. Call from the UI thread. */
	void pump();
	bool playing() const { return m_playing; }
	float progress() const;
	const std::string& lastError() const { return m_error; }

  private:
	std::unique_ptr<SyroPlayerImpl> m_impl;
	std::vector<std::string> m_names;
	bool m_playing = false;
	std::string m_error;
};

} // namespace volca
} // namespace rigkit
