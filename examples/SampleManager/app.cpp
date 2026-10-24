#include "app.h"

#include "rendering/U_gladGlfw.h"
#include "core/RigKitEngine.h"
#include "core/pack/MPack.h"
#include "core/util/AppPaths.h"
#include "packs/rigAudio/src/rigAudio.h"
#include "packs/rigAudioUi/src/rigAudioUi.h"
#include "packs/rigComponent/src/rigComponent.h"
#include "packs/rigImGui/src/IWindow.h"
#include "packs/rigImGui/src/MWindow.h"
#include "packs/rigImGui/src/Mui.h"
#include "packs/rigImGui/src/rigImGui.h"
#include "packs/rigMidi/src/rigMidi.h"
#include "packs/rigVolca/src/rigVolca.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace {

enum class Quad { TL, TR, BL, BR };

void placeQuad(Quad q) {
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	const ImVec2 pad = ImGui::GetStyle().DisplayWindowPadding;
	const float w = std::max(240.f, (vp->WorkSize.x - pad.x * 3.f) * 0.5f);
	const float h = std::max(180.f, (vp->WorkSize.y - pad.y * 3.f) * 0.5f);
	float x = vp->WorkPos.x + pad.x;
	float y = vp->WorkPos.y + pad.y;
	if (q == Quad::TR || q == Quad::BR) {
		x += w + pad.x;
	}
	if (q == Quad::BL || q == Quad::BR) {
		y += h + pad.y;
	}
	ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_FirstUseEver);
}

class ClipsWindow : public rigkit::IWindow {
  public:
	explicit ClipsWindow(SampleManagerApp& app)
		: IWindow("Audio Clips###volca.clips", ImGuiWindowFlags_NoCollapse |
												   ImGuiWindowFlags_NoDocking |
												   ImGuiWindowFlags_NoSavedSettings),
		  m_app(app) {
		setCategory("Volca");
	}
	void render() override {
		if (!isVisible()) {
			return;
		}
		placeQuad(Quad::TL);
		IWindow::render();
	}
	void renderContents() override {
		auto* packs = m_app.getEngine() ? m_app.getEngine()->getPackManager() : nullptr;
		auto audio = packs ? packs->getPack<rigkit::rigAudio>() : nullptr;
		rigkit::audio::Mixer* mixer = audio ? &audio->mixer() : nullptr;
		const int slots = rigkit::volca::slotCount(m_app.capacity().model());
		rigkit::audio::drawClipPool(m_app.pool(), mixer, [&](rigkit::audio::ClipEntry& clip, int) {
			int slot = m_app.slotOf(clip.path);
			ImGui::SetNextItemWidth(72.f);
			if (ImGui::InputInt("slot", &slot, 1, 10)) {
				m_app.setSlot(clip.path, std::clamp(slot, 0, slots - 1));
			}
		});
	}

  private:
	SampleManagerApp& m_app;
};

class SamplerWindow : public rigkit::IWindow {
  public:
	explicit SamplerWindow(SampleManagerApp& app)
		: IWindow("Sampler###volca.sampler", ImGuiWindowFlags_NoCollapse |
												 ImGuiWindowFlags_NoDocking |
												 ImGuiWindowFlags_NoSavedSettings),
		  m_app(app) {
		setCategory("Volca");
	}
	void render() override {
		if (!isVisible()) {
			return;
		}
		placeQuad(Quad::TR);
		IWindow::render();
	}
	void renderContents() override { rigkit::audio::drawSampler(m_app.sampler()); }

  private:
	SampleManagerApp& m_app;
};

class SendWindow : public rigkit::IWindow {
  public:
	explicit SendWindow(SampleManagerApp& app)
		: IWindow("Send###volca.send", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
										   ImGuiWindowFlags_NoSavedSettings),
		  m_app(app) {
		setCategory("Volca");
	}
	void render() override {
		if (!isVisible()) {
			return;
		}
		placeQuad(Quad::BL);
		IWindow::render();
	}
	void renderContents() override {
		const char* models[] = {"volca sample", "volca sample2"};
		if (ImGui::Combo("Model", &m_app.modelIndex(), models, 2)) {
			m_app.capacity().setModel(m_app.modelIndex() == 1 ? rigkit::volca::VolcaModel::Sample2
															  : rigkit::volca::VolcaModel::Sample);
			m_app.rebuildCapacity();
		}
		auto& player = m_app.player();
		const auto& names = player.deviceNames();
		std::vector<const char*> labels;
		labels.reserve(names.size());
		for (const auto& n : names) {
			labels.push_back(n.c_str());
		}
		if (!labels.empty()) {
			ImGui::Combo("Output", &m_app.deviceIndex(), labels.data(),
						 static_cast<int>(labels.size()));
		}
		ImGui::SameLine();
		if (ImGui::Button("Rescan")) {
			player.rescanDevices();
		}

		const float used = rigkit::volca::secondsForBytes(m_app.capacity().used());
		const float budget = rigkit::volca::secondsForBytes(m_app.capacity().budget());
		const float frac = budget > 0.f ? used / budget : 0.f;
		char overlay[64];
		std::snprintf(overlay, sizeof(overlay), "%.1f / %.1f s", used, budget);
		ImGui::ProgressBar(frac, ImVec2(-1.f, 0.f), overlay);

		ImGui::BeginDisabled(m_app.sending());
		if (ImGui::Button("Send selected")) {
			m_app.queueSend(m_app.pool().selected());
		}
		ImGui::SameLine();
		int eraseSlot = 0;
		ImGui::SetNextItemWidth(80.f);
		ImGui::InputInt("##erase", &eraseSlot);
		ImGui::SameLine();
		if (ImGui::Button("Erase slot")) {
			m_app.queueErase(eraseSlot);
		}
		ImGui::EndDisabled();

		if (m_app.sending()) {
			ImGui::ProgressBar(m_app.sendProgress(), ImVec2(-1.f, 0.f), "sending");
			if (ImGui::Button("Abort")) {
				m_app.player().stop();
			}
		}
		if (!m_app.sendStatus().empty()) {
			ImGui::TextWrapped("%s", m_app.sendStatus().c_str());
		}
		ImGui::Separator();
		ImGui::TextWrapped(
			"Cable: headphone out (full volume) into SYNC IN. sample2 USB: pick that device.");
	}

  private:
	SampleManagerApp& m_app;
};

class MidiWindow : public rigkit::IWindow {
  public:
	explicit MidiWindow(SampleManagerApp& app)
		: IWindow("MIDI Control###volca.midi", ImGuiWindowFlags_NoCollapse |
												   ImGuiWindowFlags_NoDocking |
												   ImGuiWindowFlags_NoSavedSettings),
		  m_app(app) {
		setCategory("Volca");
	}
	void render() override {
		if (!isVisible()) {
			return;
		}
		placeQuad(Quad::BR);
		IWindow::render();
	}
	void renderContents() override {
		auto* packs = m_app.getEngine() ? m_app.getEngine()->getPackManager() : nullptr;
		auto midi = packs ? packs->getPack<rigkit::rigMidi>() : nullptr;
		if (!midi) {
			ImGui::TextDisabled("no MIDI pack");
			return;
		}
		const auto& outs = midi->outputNames();
		int out = midi->settings().outputDevice;
		std::vector<const char*> labels;
		labels.reserve(outs.size());
		for (const auto& n : outs) {
			labels.push_back(n.c_str());
		}
		if (!labels.empty() &&
			ImGui::Combo("MIDI Out", &out, labels.data(), static_cast<int>(labels.size()))) {
			midi->setOutputDevice(out);
		}
		ImGui::SliderInt("Part channel", &m_app.partChannel(), 1, 10);
		ImGui::Checkbox("Notes to speed", &m_app.notesToSpeed());
		ImGui::SameLine();
		ImGui::TextDisabled("note 64 = original pitch");

		using namespace rigkit::volca::sample;
		const struct {
			const char* name;
			int cc;
		} knobs[] = {
			{"Level", kLevel},
			{"Pan", kPan},
			{"Start", kStartPoint},
			{"Length", kLength},
			{"Hi Cut", kHiCut},
			{"Speed", kSpeed},
			{"Pitch EG Int", kPitchEgInt},
			{"Pitch EG Atk", kPitchEgAttack},
			{"Pitch EG Dec", kPitchEgDecay},
			{"Amp EG Atk", kAmpEgAttack},
			{"Amp EG Dec", kAmpEgDecay},
		};
		for (const auto& k : knobs) {
			int v = m_app.ccValue(k.cc);
			if (ImGui::SliderInt(k.name, &v, 0, 127)) {
				m_app.setCc(k.cc, v);
				if (midi->outputOpen()) {
					rigkit::volca::sendCc(*midi, m_app.partChannel(), k.cc, v);
				}
			}
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Clock");
		float bpm = m_app.clock().bpm();
		if (ImGui::SliderFloat("BPM", &bpm, 20.f, 300.f, "%.1f")) {
			m_app.clock().setBpm(bpm);
		}
		if (m_app.clock().running()) {
			if (ImGui::Button("Stop clock")) {
				m_app.clock().stop(*midi);
				m_app.divider().reset();
			}
		} else if (ImGui::Button("Start clock")) {
			m_app.clock().start(*midi);
			m_app.divider().reset();
		}
		ImGui::SameLine();
		ImGui::Text("ticks %d", m_app.clock().ticks());
		ImGui::Text("div ");
		for (int d = 1; d <= 8; ++d) {
			ImGui::SameLine();
			ImGui::TextDisabled("/%d%s", d, m_app.divider().pulse(d) ? "*" : "");
		}
	}

  private:
	SampleManagerApp& m_app;
};

} // namespace

SampleManagerApp::SampleManagerApp() {
	window().width = 1280;
	window().height = 800;
	window().title = "SampleManager";
	settings().appName = "SampleManager";
	m_cc[rigkit::volca::sample::kLevel] = 127;
	m_cc[rigkit::volca::sample::kPan] = 64;
	m_cc[rigkit::volca::sample::kSpeed] = 64;
	m_cc[rigkit::volca::sample::kLength] = 127;
}

std::string SampleManagerApp::defaultFolder() const {
	namespace fs = std::filesystem;
	const fs::path here(AppPaths::getOsConfigDir());
	const fs::path kidvj = here.parent_path() / "KidVJ" / "samples";
	std::error_code ec;
	if (fs::exists(kidvj, ec)) {
		return kidvj.string();
	}
	return AppPaths::joinPath(AppPaths::getOsConfigDir(), "samples");
}

void SampleManagerApp::setup() {
	auto* packs = m_engine->getPackManager();
	if (!packs) {
		return;
	}
	packs->registerPack<rigkit::rigComponent>();
	packs->registerPack<rigkit::rigImGui>();
	packs->registerPack<rigkit::rigAudio>();
	packs->registerPack<rigkit::rigAudioUi>();
	packs->registerPack<rigkit::rigMidi>();
	packs->registerPack<rigkit::rigVolca>();
	packs->initAll();
	packs->setupAll();

	const std::string folder = defaultFolder();
	std::error_code ec;
	std::filesystem::create_directories(folder, ec);
	m_pool.setFolder(folder);
	m_pool.rescan();
	auto audio = packs->getPack<rigkit::rigAudio>();
	m_sampler.setAudio(audio.get());
	m_sampler.setFolder(folder);
	m_sampler.setClipPool(&m_pool);
	m_player.rescanDevices();

	if (auto* ui = m_engine->getUiManager()) {
		if (auto* wm = ui->getWindowManager()) {
			if (auto win = wm->createWindow<ClipsWindow>(*this)) {
				win->setVisible(true);
			}
			if (auto win = wm->createWindow<SamplerWindow>(*this)) {
				win->setVisible(true);
			}
			if (auto win = wm->createWindow<SendWindow>(*this)) {
				win->setVisible(true);
			}
			if (auto win = wm->createWindow<MidiWindow>(*this)) {
				win->setVisible(true);
			}
			wm->hideWindow("Log");
			wm->hideWindow("Windows");
			wm->hideWindow("Debug");
			wm->hideWindow("Theme");
			wm->hideWindow("Layers");
			wm->hideWindow("Scene");
			wm->hideWindow("Properties");
			wm->hideWindow("Viewport");
			wm->hideWindow("Shortcuts");
			wm->hideWindow("Preferences");
		}
		if (auto* mui = dynamic_cast<rigkit::Mui*>(ui)) {
			mui->setAboutIntro(
				"SampleManager — record or load clips, Syro-send them to a volca sample, "
				"and play chromatically over MIDI (notes-to-speed).");
		}
	}

	if (const char* cap = std::getenv("RIGKIT_CAPTURE_PREVIEW")) {
		m_capture = (cap[0] != '\0' && cap[0] != '0');
	}

	if (m_capture) {
		const std::string shot = AppPaths::joinPath(AppPaths::getDataDir(), "samples");
		std::filesystem::create_directories(shot, ec);
		m_pool.setFolder(shot);
		m_pool.rescan();
		m_sampler.setFolder(shot);
	}
}

void SampleManagerApp::update(float dt) {
	++m_frames;
	if (m_capture && m_engine) {
		if (m_frames == 90) {
			if (auto* mui = dynamic_cast<rigkit::Mui*>(m_engine->getUiManager())) {
				mui->requestExportPng();
			}
		} else if (m_frames == 95) {
			if (GLFWwindow* win = m_engine->getWindow()) {
				glfwSetWindowShouldClose(win, GLFW_TRUE);
			}
		}
	}
	m_sampler.pump();
	m_player.pump();
	pumpSend();

	auto* packs = m_engine ? m_engine->getPackManager() : nullptr;
	auto midi = packs ? packs->getPack<rigkit::rigMidi>() : nullptr;
	if (!midi) {
		return;
	}
	m_clock.pump(dt, *midi);
	if (m_clock.ticks() != m_lastClockTicks) {
		const int n = m_clock.ticks() - m_lastClockTicks;
		for (int i = 0; i < n; ++i) {
			m_divider.tick();
		}
		m_lastClockTicks = m_clock.ticks();
	}
	if (!m_notesToSpeed) {
		return;
	}
	for (const auto& msg : midi->messages()) {
		if (msg.isNoteOn()) {
			rigkit::volca::notesToSpeed(*midi, m_partChannel, msg.data1, msg.data2);
		} else if (msg.isNoteOff()) {
			midi->sendNoteOff(m_partChannel, msg.data1);
		}
	}
}

void SampleManagerApp::exit() {
	m_sampler.arm(false);
	m_player.stop();
	auto* packs = m_engine ? m_engine->getPackManager() : nullptr;
	if (auto midi = packs ? packs->getPack<rigkit::rigMidi>() : nullptr) {
		m_clock.stop(*midi);
	}
}

int SampleManagerApp::slotOf(const std::string& path) const {
	auto it = m_slots.find(path);
	return it == m_slots.end() ? 0 : it->second;
}

void SampleManagerApp::setSlot(const std::string& path, int slot) {
	m_slots[path] = slot;
	rebuildCapacity();
}

void SampleManagerApp::rebuildCapacity() {
	m_capacity.clear();
	for (const auto& clip : m_pool.clips()) {
		if (m_slots.find(clip.path) == m_slots.end()) {
			continue;
		}
		rigkit::volca::PreparedSample prep;
		if (rigkit::volca::prepareSample(clip.path, prep)) {
			m_capacity.add(prep.bytes);
		}
	}
}

bool SampleManagerApp::queueSend(int clipIndex) {
	if (m_player.playing()) {
		m_sendStatus = "wait for the current send to finish";
		return false;
	}
	if (clipIndex < 0 || clipIndex >= static_cast<int>(m_pool.clips().size())) {
		m_sendStatus = "no clip selected";
		return false;
	}
	const auto& clip = m_pool.clips()[static_cast<size_t>(clipIndex)];
	rigkit::volca::PreparedSample prep;
	if (!rigkit::volca::prepareSample(clip.path, prep)) {
		m_sendStatus = "could not prepare " + clip.name;
		return false;
	}
	rebuildCapacity();
	const bool already = m_slots.find(clip.path) != m_slots.end();
	rigkit::volca::Fit fit;
	if (already) {
		fit.ok = m_capacity.used() <= m_capacity.budget();
		fit.overflow = fit.ok ? 0 : m_capacity.used() - m_capacity.budget();
	} else {
		fit = m_capacity.canFit(prep);
	}
	if (!fit.ok) {
		m_sendStatus =
			"too long for remaining memory (" +
			std::to_string(static_cast<int>(rigkit::volca::secondsForBytes(fit.overflow))) +
			" s over)";
		return false;
	}
	const int slots = rigkit::volca::slotCount(m_capacity.model());
	const int slot = std::clamp(slotOf(clip.path), 0, slots - 1);
	Pending p;
	if (!rigkit::volca::encodeSend(slot, prep, p.stream)) {
		m_sendStatus = p.stream.error.empty() ? "encode failed" : p.stream.error;
		return false;
	}
	p.label = clip.name + " -> slot " + std::to_string(slot);
	m_queue.push_back(std::move(p));
	m_sendStatus = "queued " + m_queue.back().label;
	return true;
}

bool SampleManagerApp::queueErase(int slot) {
	if (m_player.playing()) {
		m_sendStatus = "wait for the current send to finish";
		return false;
	}
	Pending p;
	if (!rigkit::volca::encodeErase(slot, p.stream)) {
		m_sendStatus = p.stream.error.empty() ? "erase encode failed" : p.stream.error;
		return false;
	}
	p.label = "erase slot " + std::to_string(slot);
	m_queue.push_back(std::move(p));
	m_sendStatus = "queued " + m_queue.back().label;
	return true;
}

void SampleManagerApp::pumpSend() {
	if (m_player.playing() || m_queue.empty()) {
		return;
	}
	Pending next = std::move(m_queue.front());
	m_queue.erase(m_queue.begin());
	if (!m_player.play(next.stream, m_deviceIndex)) {
		m_sendStatus = m_player.lastError();
		return;
	}
	m_sendStatus = "sending " + next.label;
}

int SampleManagerApp::ccValue(int cc) const {
	if (cc < 0 || cc > 127) {
		return 0;
	}
	return m_cc[cc];
}

void SampleManagerApp::setCc(int cc, int value) {
	if (cc < 0 || cc > 127) {
		return;
	}
	m_cc[cc] = rigkit::volca::clamp7(value);
}
