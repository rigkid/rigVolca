#pragma once

#include "core/pack/IPack.h"

#include "Syro.h"
#include "SyroPlayer.h"
#include "VolcaMidi.h"

namespace rigkit {

/**
 * @brief volca sample Syro transfer and MIDI control.
 * @details Leaf on rigAudio + rigMidi. Hosts prepare samples, check Capacity,
 * encode a Syro stream, and play it bit-clean through SyroPlayer. MIDI helpers
 * cover notes-to-speed, CC tables, a 24 ppqn clock, and a /1–/8 divider.
 */
class rigVolca : public IPack {
  public:
	rigVolca();
	bool init() override;
};

} // namespace rigkit
