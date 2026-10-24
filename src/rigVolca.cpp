#include "rigVolca.h"

#include "core/pack/PackRegistry.h"

#include <memory>
#include <spdlog/spdlog.h>

namespace rigkit {

rigVolca::rigVolca() : IPack("rigVolca") {}

bool rigVolca::init() {
	spdlog::info("[rigVolca] init");
	return true;
}

} // namespace rigkit

namespace {
struct rigVolcaRegistrar {
	rigVolcaRegistrar() {
		rigkit::PackRegistry::instance().addFactory("rigVolca", []() {
			return std::shared_ptr<rigkit::IPack>(std::make_shared<rigkit::rigVolca>());
		});
	}
};
static rigVolcaRegistrar rigVolca_auto_reg;
} // namespace
