#pragma once

#include "core/U_core.h"
#include "packs/rigAudioUi/src/ClipPool.h"
#include "packs/rigAudioUi/src/Sampler.h"
#include "packs/rigVolca/src/Syro.h"
#include "packs/rigVolca/src/SyroPlayer.h"
#include "packs/rigVolca/src/VolcaMidi.h"

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief rigVolca example: clip pool, sampler, Syro send, volca MIDI control.
 */
class SampleManagerApp : public rigkit::IApp {
  public:
	SampleManagerApp();
	void setup() override;
	void update(float dt) override;
	void exit() override;

	rigkit::audio::ClipPool& pool() { return m_pool; }
	rigkit::audio::Sampler& sampler() { return m_sampler; }
	rigkit::volca::SyroPlayer& player() { return m_player; }
	rigkit::volca::Capacity& capacity() { return m_capacity; }
	rigkit::volca::MidiClock& clock() { return m_clock; }
	rigkit::volca::ClockDivider& divider() { return m_divider; }

	int& modelIndex() { return m_modelIndex; }
	int& deviceIndex() { return m_deviceIndex; }
	int& partChannel() { return m_partChannel; }
	bool& notesToSpeed() { return m_notesToSpeed; }
	int slotOf(const std::string& path) const;
	void setSlot(const std::string& path, int slot);
	void rebuildCapacity();
	bool queueSend(int clipIndex);
	bool queueErase(int slot);
	bool sending() const { return m_player.playing() || !m_queue.empty(); }
	const std::string& sendStatus() const { return m_sendStatus; }
	float sendProgress() const { return m_player.progress(); }

	int ccValue(int cc) const;
	void setCc(int cc, int value);

  private:
	struct Pending {
		rigkit::volca::SyroStream stream;
		std::string label;
	};
	void pumpSend();
	std::string defaultFolder() const;

	rigkit::audio::ClipPool m_pool;
	rigkit::audio::Sampler m_sampler;
	rigkit::volca::SyroPlayer m_player;
	rigkit::volca::Capacity m_capacity;
	rigkit::volca::MidiClock m_clock;
	rigkit::volca::ClockDivider m_divider;
	std::unordered_map<std::string, int> m_slots;
	std::vector<Pending> m_queue;
	int m_modelIndex = 0;
	int m_deviceIndex = 0;
	int m_partChannel = 1;
	bool m_notesToSpeed = true;
	int m_cc[128] = {};
	int m_lastClockTicks = 0;
	std::string m_sendStatus;
	int m_frames = 0;
	bool m_capture = false;
};
