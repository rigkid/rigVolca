#include "VolcaMidi.h"

#include <algorithm>
#include <cmath>

namespace rigkit {
namespace volca {

int clamp7(int v) {
	return std::clamp(v, 0, 127);
}

int speedForSemitone(int semitones) {
	return clamp7(64 + semitones);
}

bool sendCc(rigMidi& midi, int channel1, int cc, int value) {
	if (channel1 < 1 || channel1 > 16) {
		return false;
	}
	const uint8_t status = static_cast<uint8_t>(0xB0 | (channel1 - 1));
	return midi.send(status, static_cast<uint8_t>(clamp7(cc)), static_cast<uint8_t>(clamp7(value)));
}

bool notesToSpeed(rigMidi& midi, int channel1, int note, int velocity) {
	if (!sendCc(midi, channel1, sample::kSpeed, clamp7(note))) {
		return false;
	}
	return midi.sendNoteOn(channel1, clamp7(note), clamp7(velocity));
}

void MidiClock::setBpm(float bpm) {
	m_bpm = std::clamp(bpm, 10.f, 600.f);
}

void MidiClock::start(rigMidi& midi) {
	m_accum = 0.f;
	m_ticks = 0;
	m_running = true;
	midi.sendStart();
}

void MidiClock::stop(rigMidi& midi) {
	if (m_running) {
		midi.sendStop();
	}
	m_running = false;
	m_accum = 0.f;
}

void MidiClock::pump(float dt, rigMidi& midi) {
	if (!m_running || m_bpm <= 0.f) {
		return;
	}
	const float interval = 60.f / m_bpm / 24.f;
	m_accum += dt;
	int guard = 0;
	while (m_accum >= interval && guard++ < 48) {
		m_accum -= interval;
		midi.sendClock();
		++m_ticks;
	}
}

void ClockDivider::tick() {
	++m_count;
	for (int d = 1; d <= 8; ++d) {
		m_pulse[d] = (m_count % d) == 0;
	}
}

void ClockDivider::reset() {
	m_count = 0;
	for (int d = 1; d <= 8; ++d) {
		m_pulse[d] = false;
	}
}

bool ClockDivider::pulse(int div) const {
	if (div < 1 || div > 8) {
		return false;
	}
	return m_pulse[div];
}

} // namespace volca
} // namespace rigkit
