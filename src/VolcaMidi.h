/**
 * @file
 * @brief volca sample / volca FM MIDI CC tables, notes-to-speed, clock, divider.
 */
#pragma once

#include "rigMidi.h"

#include <cstdint>

namespace rigkit {
namespace volca {

/// volca sample CC numbers (parts on MIDI channels 1–10).
namespace sample {
constexpr int kLevel = 7;
constexpr int kPan = 10;
constexpr int kStartPoint = 40;
constexpr int kLength = 41;
constexpr int kHiCut = 42;
constexpr int kSpeed = 43;		///< 0–127 maps to -63..+63; 64 is original pitch.
constexpr int kPitchEgInt = 44; ///< 0–127 maps to -63..+63.
constexpr int kPitchEgAttack = 45;
constexpr int kPitchEgDecay = 46;
constexpr int kAmpEgAttack = 47;
constexpr int kAmpEgDecay = 48;
} // namespace sample

/// volca FM CC numbers (when MIDI RX ShortMessage is On).
namespace fm {
constexpr int kTranspose = 40;
constexpr int kVelocity = 41;
constexpr int kModAttack = 42;
constexpr int kModDecay = 43;
constexpr int kCarrierAttack = 44;
constexpr int kCarrierDecay = 45;
constexpr int kLfoRate = 46;
constexpr int kLfoPitchDepth = 47;
constexpr int kAlgorithm = 48;
constexpr int kArpType = 49;
constexpr int kArpDiv = 50;
} // namespace fm

int clamp7(int v);
/** @brief MIDI CC 43 value for a signed semitone offset, centered on 64. */
int speedForSemitone(int semitones);
/** @brief Send CC 43 = `note`, then the note — chromatic sample playback. */
bool notesToSpeed(rigMidi& midi, int channel1, int note, int velocity);
bool sendCc(rigMidi& midi, int channel1, int cc, int value);

/**
 * @brief BPM-driven 24 ppqn MIDI clock (0xF8), with Start / Stop.
 * @details Call `pump(dt, midi)` every frame. Interval is 60 / bpm / 24 seconds.
 */
class MidiClock {
  public:
	void setBpm(float bpm);
	float bpm() const { return m_bpm; }
	bool running() const { return m_running; }

	void start(rigMidi& midi);
	void stop(rigMidi& midi);
	void pump(float dt, rigMidi& midi);

	/// Ticks emitted since start (24 per quarter).
	int ticks() const { return m_ticks; }

  private:
	float m_bpm = 120.f;
	float m_accum = 0.f;
	bool m_running = false;
	int m_ticks = 0;
};

/**
 * @brief /1 through /8 divider with reset, matching pd-volka `v-clock-divider`.
 */
class ClockDivider {
  public:
	void tick();
	void reset();
	/** @brief True on this tick for division `div` (1–8). */
	bool pulse(int div) const;
	int count() const { return m_count; }

  private:
	int m_count = 0;
	bool m_pulse[9] = {};
};

} // namespace volca
} // namespace rigkit
